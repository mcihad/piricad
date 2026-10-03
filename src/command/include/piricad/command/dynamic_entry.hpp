// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the length and the angle typed beside the cursor.
//
// DYNAMIC INPUT (TODOS U-02). While a line is being dragged out from its last point, a
// person wants to say "this long" or "at this angle" without leaving the drawing, and to
// say it for ONE of the two while the hand keeps choosing the other: twelve and a half
// metres along wherever the mouse points, or exactly 45 grad at whatever length the
// mouse reaches. Every CAD calls it dynamic input; the controls are always the same —
// type a number, Tab to lock it and move to the other field, Enter to accept — and
// the mouse moving afterwards must not change what is locked.
//
// THIS CLASS HOLDS THE STATE AND NOTHING ELSE: which field is active, what is locked in
// each. It does not read a keyboard or draw a thing — the shell feeds it the text of
// the command line (the line IS the active field's text box, so there is no second
// editor to keep in step) and draws what `shown` returns.
//
// AND IT HAS NO ARITHMETIC OF ITS OWN. The point a pair of fields makes is worked out
// by writing them as the line a person could have typed — `@12.5<45`, with the
// part the mouse still decides written out exactly (the length to the millimetre, the
// angle to the micro-degree that coordinates are stored at) — and reading that line with
// `parse_point`, the one grammar (CLAUDE.md 5.11). So a locked length, a typed one and
// the same figures in a script are the same point, and the journal of a click that was
// held to a locked length is the journal of its coordinates (CLAUDE.md 1.2).
#pragma once

#include "piricad/command/input.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/core/angle.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace piricad::command {

/// The two fields, in the order Tab walks them.
enum class DynField : std::uint8_t {
    Length = 0, ///< the distance from the last point, in metres (`12.5`, `1250 cm`)
    Angle  = 1, ///< the direction, in the session's angle unit and rule (`45`, `45d`)
};

/// Whether a length and an angle can be typed for this prompt: a POINT asked for with a guide
/// drawn out from a base point, and the guide a line from it. The guides that are something else —
/// a window, a sweep, a dimension, a corner — measure a different thing from that point, and a
/// length and an angle would answer nothing they ask. The canvas's readout of the same two figures
/// asks this too, so what is shown is what can be typed.
bool takes_dynamic_entry(const Prompt& prompt) noexcept;

class DynamicEntry
{
public:
    /// What the canvas writes for one field.
    struct Shown
    {
        std::string text;    ///< `12,5 m`, `124,2238 grad` — the value this field has now
        bool locked{false};  ///< held by a Tab; the mouse does not move it
        bool active{false};  ///< the field the command line is typing into
        bool typing{false};  ///< the line holds text for it that is not locked yet
        bool invalid{false}; ///< that text is not a value this field takes
    };

    /// The field the command line is typing into.
    DynField active() const noexcept { return active_; }

    bool locked(DynField f) const noexcept { return locked_[index(f)]; }

    bool any_locked() const noexcept { return locked_[0] || locked_[1]; }

    /// Drops every lock and puts the cursor back on the length. After each point (the next
    /// segment starts afresh), on Esc, and whenever the question changes.
    void clear() noexcept;

    /// TAB: the active field takes the line's text as its locked value and the other field
    /// becomes the active one.
    ///
    ///  - Text in the line that is a value for the field: locked. The result is the text the
    ///    line should hold now — the other field's own, when it was locked before and is being
    ///    taken up again for editing (its lock released), otherwise empty.
    ///  - Nothing in the line: the active field changes. A field that was locked is released and
    ///    its text handed back to be edited, so Tab, Tab walks round and re-opens a figure.
    ///  - Text that is no value for the field: refused with the reason, nothing changed.
    core::Result<std::string> tab(std::string_view line, const ResolveContext& ctx);

    /// Whether `text` is a length on its own — one number with or without a unit, or an
    /// expression of them, and NOT a coordinate pair: a `,` makes `12,5` the point (12; 5) at a
    /// point prompt, exactly as it always has. This is what lets a bare `12.5` + Enter mean
    /// "twelve and a half metres that way" without taking a coordinate away.
    static bool is_bare_length(std::string_view text);

    /// The line a person could have typed for these figures: `@12.500<37.251934d`.
    ///
    /// `line` is the command line's current text and belongs to the active field unless that
    /// field is locked. A field with neither lock nor text is the cursor's: its length to the
    /// millimetre, its direction to the micro-degree.
    core::Result<std::string> compose(core::Point2 origin, core::Point2 cursor,
                                      std::string_view line, const ResolveContext& ctx) const;

    /// Whether the figures need the cursor at all: false when both fields have a value of their own
    /// (locked, or the active one typed), so the point is known without knowing where the hand is.
    bool needs_cursor(std::string_view line) const noexcept;

    /// The point those figures make: `compose`, read by `parse_point`.
    core::Result<core::Point2> resolve(core::Point2 origin, core::Point2 cursor,
                                       std::string_view line, const ResolveContext& ctx) const;

    /// Both fields as the canvas shows them, the cursor's figures standing in for any field that
    /// has neither a lock nor text.
    std::array<Shown, 2> shown(core::Point2 origin, core::Point2 cursor, std::string_view line,
                               const core::AngleConvention& convention) const;

private:
    static constexpr std::size_t index(DynField f) noexcept { return static_cast<std::size_t>(f); }

    /// The figure `text` stands for in `field`, as it will be written into a polar token, or the
    /// reason it is not one.
    core::Result<std::string> token_of(DynField field, std::string_view text,
                                       const ResolveContext& ctx) const;

    DynField active_{DynField::Length};
    std::array<bool, 2> locked_{false, false};
    std::array<std::string, 2> token_{}; ///< each locked field as the polar token will carry it
    std::array<std::string, 2> text_{};  ///< what each locked field was typed as, to be shown
};

} // namespace piricad::command
