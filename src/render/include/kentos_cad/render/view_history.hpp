// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — render: where the view has been (`YAKINLAŞ ÖNCEKİ`, `SONRAKİ`).
//
// Netcad keeps thirty earlier windows behind Alt+C (wiki.netcad.com.tr
// 217385173), and a surveyor who zooms into a corner to read a coordinate goes
// back to the sheet the same way. The rule of what counts as one step lives
// here, Qt-free, so a unit test can hold it: the canvas only reports its moves.
#pragma once

#include "kentos_cad/core/units.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace kentos::render {

/// ONE VIEW TO GO BACK TO: where its centre was and how far in it was.
///
/// Two-dimensional today. The camera of a 3-D view (netcad_plan.md N-29) is a
/// field this gains, never a second history beside it.
struct ViewState
{
    core::Point2 centre{};  ///< the centre, document millimetres
    double mm_per_pixel{0}; ///< the scale, document millimetres per pixel

    /// Exactly equal: a move that lands where it started went nowhere.
    bool operator==(const ViewState&) const = default;
};

/// WHERE THE VIEW HAS BEEN, thirty steps back and thirty forward again.
///
/// SESSION STATE, NEVER DOCUMENT STATE (model.md R43): nothing here is hashed,
/// undone or journalled as a document line. The viewport that owns one reports
/// every move it makes (`moved`), and a step back or forward is the only move it
/// does not report.
///
/// A WHEEL BURST IS ONE STEP. Ten notches of the wheel are one look at a new
/// place, and a history that kept ten entries for them would take ten ÖNCEKİ to
/// undo one gesture. A wheel move no more than `kFoldMs` after the previous one
/// folds into the step that burst began with; any other move ends the burst. The
/// clock is the caller's, so the rule is tested without one.
///
/// A MOVE THAT WENT NOWHERE IS NOT A STEP: KAPSAM pressed on a view already
/// showing the extents leaves nothing to come back to.
class ViewHistory
{
public:
    /// How many steps each way are kept — Netcad's thirty.
    static constexpr std::size_t kDepth = 30;

    /// How close two wheel moves must be, in milliseconds, to be one step.
    static constexpr std::int64_t kFoldMs = 700;

    /// What moved the view.
    enum class Move {
        Step,  ///< one deliberate move: a command, a button, a drag
        Wheel, ///< one notch of the wheel, which folds into its burst
    };

    /// The view moved from `from` to `to` by `how`, at `at_ms` on the caller's
    /// clock. A new move forgets the steps forward, as a new page does in a
    /// browser: the branch it left is not the way forward any more.
    void moved(const ViewState& from, const ViewState& to, Move how, std::int64_t at_ms);

    /// The view to go back to, `now` becoming the first step forward; nothing
    /// when there is no earlier view.
    std::optional<ViewState> back(const ViewState& now);

    /// The view to go forward to again, `now` becoming the first step back;
    /// nothing when no step back was taken since the last move.
    std::optional<ViewState> forward(const ViewState& now);

    /// Steps `back` can still take.
    std::size_t behind() const noexcept { return back_.size(); }

    /// Steps `forward` can take.
    std::size_t ahead() const noexcept { return forward_.size(); }

    /// Forgets every step: a new drawing's view has no past in the old one.
    void clear();

private:
    std::deque<ViewState> back_;
    std::deque<ViewState> forward_;
    bool wheeling_{false};    ///< the last reported move was a wheel notch
    std::int64_t wheeled_{0}; ///< when it was
};

} // namespace kentos::render
