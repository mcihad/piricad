// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the serialisable command argument.
//
// kentoscad.md §2.2: a command invocation must be SERIALISABLE. That one decision
// is what makes undo/redo, macro recording, scripting, AI, regression replay,
// crash recovery, remote API and future multi-user editing fall out of the same
// machinery instead of being written five times.
#pragma once

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace kentos::command {

using core::Point2;

/// One argument of one command invocation.
///
/// A closed set of kinds, deliberately: this type has to round-trip losslessly
/// through JSON (Article 1.4), so every kind here must have an exact textual form.
/// That rules out an arbitrary variant and it rules out `std::any`.
class Value
{
public:
    /// A list of coordinates — the vertices of a line, the corners of an area.
    using Points = std::vector<Point2>;

    /// A list of integers — entity keys for a selection, ring lengths for an area.
    using Ints = std::vector<std::int64_t>;

    /// A LIST OF WORDS, for a parameter that names several things.
    ///
    /// A map frame draws a named set of layers and a table prints a named set of
    /// columns, and neither could be said from a command line: `Text` replaced
    /// rather than accumulated, so `katmanlar=parsel katmanlar=bina` kept the
    /// last one. A comma would not do — a layer name is not validated and may
    /// contain one, so splitting on it would be a guess about somebody's data.
    using Texts = std::vector<std::string>;

    /// What this value holds. `Empty` is a real state and means "the caller did
    /// not supply this argument", which is different from supplying a zero.
    enum class Kind : std::uint8_t {
        Empty,
        Bool,
        Int,
        Number,
        Text,
        Point,
        PointList,
        IdList,
        /// Added at the END: a journal reads a kind by name, not by number, but
        /// the enum's order is what a reader of this file learns first.
        TextList,
    };

    /// An absent argument. `empty()` is true and every accessor returns its
    /// default rather than throwing, because a command asking for an optional
    /// parameter it was not given is the ordinary case.
    Value() = default;

    /// Constructors, one per kind. Free functions rather than overloaded
    /// constructors so that the KIND is written at every call site: `Value(1)`
    /// could be an integer, a boolean or a number, and a journal that guessed
    /// wrong would replay a different command than the one that ran.
    static Value boolean(bool v);
    static Value integer(std::int64_t v);
    static Value number(double v);
    static Value text(std::string v);
    static Value point(Point2 v);
    static Value points(Points v);
    static Value ids(Ints v);
    static Value texts(Texts v);

    /// What this value actually holds, for a caller that must branch on it.
    Kind kind() const noexcept { return kind_; }

    /// Whether the argument was supplied at all.
    bool empty() const noexcept { return kind_ == Kind::Empty; }

    /// Readers, each returning `d` when this value is of another kind.
    ///
    /// They do NOT convert between kinds and they do not throw. A command that
    /// asks a text value for its integer gets the default, and the type mismatch
    /// was already refused by validation on the bus before the body ran (Article
    /// 1.3) — so a wrong answer here would mean validation was skipped, which is
    /// a bug to find rather than an exception to catch.
    bool as_bool(bool d = false) const;
    std::int64_t as_int(std::int64_t d = 0) const;
    double as_number(double d = 0.0) const;
    const std::string& as_text() const;
    Point2 as_point() const;
    const Points& as_points() const;
    const Ints& as_ids() const;

    /// The words, or an empty list when this holds something else. A single
    /// `Text` reads as a one-word list, because `katmanlar=parsel` means a list
    /// of one and a caller should not have to know which it wrote.
    const Texts& as_texts() const;

    /// Canonical serialisation. Coordinates go out as integer millimetres —
    /// never as a formatted double — so a journal round-trip is lossless and
    /// byte-identical across platforms (kentoscad.md §7.3).
    core::Json to_json() const;

    /// The inverse of `to_json`, and the reason a journal can be replayed. Fails
    /// rather than guessing when the shape is not one this type can hold.
    static core::Result<Value> from_json(const core::Json& j);

    /// Value equality, kind included: an integer 1 and a number 1.0 are not the
    /// same argument, because they do not serialise to the same journal line.
    friend bool operator==(const Value& a, const Value& b);

private:
    Kind kind_{Kind::Empty};
    bool b_{false};
    std::int64_t i_{0};
    double d_{0.0};
    std::string s_{};
    Points pts_{};
    Ints ids_{};
    Texts texts_{};
};

/// An ordered, named argument bundle. Order is preserved so that a journal line
/// and its replay serialise identically.
class Args
{
public:
    /// Adds or replaces `name`. A replaced argument keeps its original POSITION in
    /// the order, so re-recording a parameter mid-command cannot reorder the
    /// journal line it will be written to.
    void set(std::string name, Value v);

    /// Renames `from` to `to` IN PLACE, keeping the argument's position. Used by
    /// the bus for a `Param::was` retired name, so that an old journal line's
    /// spelling reaches the body — and the journal it writes — under the current
    /// one. Does nothing when `from` is absent.
    void rename(std::string_view from, std::string to);

    /// The argument, or null when it was not supplied. Null and an `Empty` value
    /// mean the same thing to a caller; `find` exists for one that wants to tell
    /// "absent" from "present but empty" without constructing anything.
    const Value* find(std::string_view name) const;

    /// The argument, or an empty `Value` when it was not supplied. The form a
    /// command body wants, because it treats both cases the same way.
    Value get(std::string_view name) const;

    /// Whether the argument was supplied at all.
    bool has(std::string_view name) const { return find(name) != nullptr; }

    /// How many arguments were supplied.
    std::size_t size() const noexcept { return items_.size(); }

    /// Every argument, in the order it was set. The order is what makes a
    /// journal line stable across a replay; a map would sort it by name and two
    /// runs of the same command could then produce different bytes.
    const std::vector<std::pair<std::string, Value>>& items() const noexcept { return items_; }

    /// Canonical serialisation of the whole bundle, as it appears in a journal
    /// line's `args` object.
    core::Json to_json() const;

    /// The inverse, for replaying a journal or running a script.
    static core::Result<Args> from_json(const core::Json& j);

    /// Bundle equality, order included — for the same reason `items()` preserves
    /// order. Two bundles that differ only in order serialise differently and are
    /// therefore not the same invocation.
    friend bool operator==(const Args& a, const Args& b) { return a.items_ == b.items_; }

private:
    std::vector<std::pair<std::string, Value>> items_;
};

} // namespace kentos::command
