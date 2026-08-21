// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the serialisable command argument.
//
// piricad.md §2.2: a command invocation must be SERIALISABLE. That one decision
// is what makes undo/redo, macro recording, scripting, AI, regression replay,
// crash recovery, remote API and future multi-user editing fall out of the same
// machinery instead of being written five times.
#pragma once

#include "piricad/core/json.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace piricad::command {

using core::Point2;

class Value
{
public:
    using Points = std::vector<Point2>;
    using Ints   = std::vector<std::int64_t>;

    enum class Kind : std::uint8_t { Empty, Bool, Int, Number, Text, Point, PointList, IdList };

    Value() = default;
    static Value boolean(bool v);
    static Value integer(std::int64_t v);
    static Value number(double v);
    static Value text(std::string v);
    static Value point(Point2 v);
    static Value points(Points v);
    static Value ids(Ints v);

    Kind kind() const noexcept { return kind_; }

    bool empty() const noexcept { return kind_ == Kind::Empty; }

    bool as_bool(bool d = false) const;
    std::int64_t as_int(std::int64_t d = 0) const;
    double as_number(double d = 0.0) const;
    const std::string& as_text() const;
    Point2 as_point() const;
    const Points& as_points() const;
    const Ints& as_ids() const;

    /// Canonical serialisation. Coordinates go out as integer millimetres —
    /// never as a formatted double — so a journal round-trip is lossless and
    /// byte-identical across platforms (piricad.md §7.3).
    core::Json to_json() const;
    static core::Result<Value> from_json(const core::Json& j);

    friend bool operator==(const Value& a, const Value& b);

private:
    Kind kind_{Kind::Empty};
    bool b_{false};
    std::int64_t i_{0};
    double d_{0.0};
    std::string s_{};
    Points pts_{};
    Ints ids_{};
};

/// An ordered, named argument bundle. Order is preserved so that a journal line
/// and its replay serialise identically.
class Args
{
public:
    void set(std::string name, Value v);
    const Value* find(std::string_view name) const;
    Value get(std::string_view name) const;

    bool has(std::string_view name) const { return find(name) != nullptr; }

    std::size_t size() const noexcept { return items_.size(); }

    const std::vector<std::pair<std::string, Value>>& items() const noexcept { return items_; }

    core::Json to_json() const;
    static core::Result<Args> from_json(const core::Json& j);

    friend bool operator==(const Args& a, const Args& b) { return a.items_ == b.items_; }

private:
    std::vector<std::pair<std::string, Value>> items_;
};

} // namespace piricad::command
