// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the input aids, and where they are applied.
//
// An "input aid" is anything that moves a point the user aimed at to the point
// they meant: object snap, `dik mod`, `kutupsal izleme` and `ızgaraya yakalama`.
// The
// engine lives in `kentos_cad/core/snap.hpp` and knows no client; this file is the
// seam that gives it a tolerance and a document.
//
// WHY IT IS HERE AND NOT IN THE CANVAS. Constitution Article 1.2 makes the GUI
// one client among equals. The aids are applied inside
// `InputAwaiter<Point2>::await_resume` (context.hpp), on the one path every
// `co_await ctx.point(...)` takes, whatever supplied the value. No client is
// asked where it came from; `InputSource` is never consulted (command.md P10).
//
// WHAT THEY ACT ON is the VALUE's own statement of what it is: a point AIMED by a
// hand (`Value::aimed_point`, made only by the canvas) is turned into the point
// meant; a point STATED — typed, scripted, proposed, replayed — is exact and
// passes untouched (TODOS F-03). The aperture is pixels, and a pixel's worth of
// ground changes with the zoom; a written coordinate does not. So one line lands
// in one place from every client, and the journal, which records the resolved
// point as a statement, replays without re-snapping.
//
// THE TOLERANCE IS PIXELS. `core.yakalama.tolerans` and `core.secim.tolerans` are
// declared in screen pixels because a user aims at what they can see: twelve
// pixels is metres at 1:5000 and centimetres at 1:50. Turning pixels into
// millimetres needs the view scale, which is view state — not document state
// (model.md R43) — so the canvas publishes it here on every zoom, and everything
// downstream reads one number.
//
// A CLIENT WITH NO VIEW gets `mm_per_pixel == 0`, which makes the object-snap
// radius zero and object snap inert. That is not a privilege and not a bypass: it
// is the same rule (radius = pixels x scale) applied where there is no screen. It
// is also what keeps journal replay honest — a recorded point must not be re-snapped
// against a document that has since grown a nearer vertex.
#pragma once

#include "kentos_cad/command/input.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/snap.hpp"

#include <cstdint>
#include <span>

namespace kentos::core {
/// Forward-declared: the aids read a document to snap against, and this header
/// must stay includable from anywhere in /src/command.
class Document;
} // namespace kentos::core

namespace kentos::command {

/// The aid settings as numbers the engine can use, resolved from the two stores
/// that own them. Assembled in one place so no caller reads a setting id twice.
struct AidSettings
{
    std::uint32_t modes{core::SnapNone}; ///< which object snaps are enabled
    core::Mm snap_radius{0};             ///< object-snap aperture in document millimetres
    core::Mm pick_radius{0};             ///< single-click pick box in document millimetres
    core::Mm grid_step{0};               ///< lattice spacing in document millimetres
    bool ortho{false};                   ///< `dik mod`: lock the cursor to the two axes

    /// `yüzey normali`: lock the run to the perpendicular of the surface it
    /// starts from, rather than to the page's axes. See `core::SnapQuery`.
    bool normal_lock{false};

    /// How far to look for that surface, in document millimetres.
    core::Mm normal_reach{0};
    std::int64_t polar_step{0}; ///< micro-degrees

    /// How far past the aperture UZANTI, PARALEL and UZATILMIŞ KESİŞİM may look
    /// for the edge they build from; see `core::SnapQuery::reach`.
    core::Mm reach{0};

    /// `core::SnapQuery::tracking_reach`: how far from a tracking trace the aim
    /// may be and still be taken. The aperture's own distance, so a trace is as
    /// easy to catch as a corner is.
    core::Mm tracking_reach{0};

    /// `core.yakalama.adim`: the multiple the distance from the previous point is
    /// rounded to, in millimetres. 0 is off.
    core::Mm step{0};
};

/// The run a prompt is in the middle of, for the snap (`SnapQuery::pending`):
/// the corners of ÇİZGİ, ÇOKLUÇİZGİ, ALAN and SPLINE fixed so far — a prompt
/// that may take its newest point back (`Prompt::can_retract`) is such a run —
/// and whether the pieces between them are drawn edges or a spline's control
/// polygon.
struct PendingRun
{
    std::span<const core::Point2> corners; ///< fixed so far, oldest first
    bool edges{false};                     ///< the pieces between them are drawn
};

/// The run `p` is in the middle of; empty for any other prompt.
PendingRun pending_run(const Prompt& p) noexcept;

class InputAids
{
public:
    /// Published by the view on every zoom and resize. Zero means "no view", and
    /// the pixel-derived radii are then zero.
    void set_view_scale(double mm_per_pixel) noexcept;

    double view_scale() const noexcept { return mm_per_pixel_; }

    bool has_view() const noexcept { return mm_per_pixel_ > 0.0; }

    /// Reads `core.yakalama.*`, `core.secim.tolerans` and `core.izgara.adim` and
    /// converts the pixel tolerances with the published view scale.
    ///
    /// `core.yakalama.izgara` is folded into the mask as `SnapGrid`, so the F9
    /// toggle and a hand-written `MOD yakalama_modları` value reach the engine
    /// through the same sixteen bits — there is one snap-mode list, not two
    /// (CLAUDE.md 5.10).
    ///
    /// MEMOISED against the two stores' revision counters and the view scale.
    /// `.claude/command.md` R22 budgets a script dispatch at 10 microseconds, and
    /// seven `Settings::get` calls per point do not fit inside it: each one folds
    /// its id for the Turkish-aware alias scan, which allocates. The cache turns a
    /// repeated dispatch into three integer comparisons; every write bumps a
    /// revision, so a stale answer is not reachable.
    const AidSettings& settings(const core::Settings& app, const core::Settings& session) const;

    /// Resolves one aim. `base` is the previous point of the running command —
    /// the rubber-band origin — which is what dik mod, kutupsal izleme and the
    /// DİK object snap measure from.
    /// `marks` are the points marked for tracking (`Bus::tracking_marks`), newest
    /// last. Passed in rather than read from anywhere: this object has no bus,
    /// and the marks are session state that belongs to one.
    /// `run` is the run the prompt is in the middle of (`pending_run`).
    core::SnapResult resolve(const core::Document& doc, const AidSettings& s, core::Point2 aim,
                             bool has_base, core::Point2 base,
                             std::span<const core::Point2> marks = {}, PendingRun run = {}) const;

    /// The last aid that fired, for the canvas marker. Session state, drawn only
    /// (model.md R43); nothing downstream may treat it as an input.
    const core::SnapResult& last() const noexcept { return last_; }

    void remember(const core::SnapResult& r) { last_ = r; }

    void forget() { last_ = core::SnapResult{}; }

private:
    double mm_per_pixel_{0.0};
    core::SnapResult last_{};

    // The memo. Mutable because resolving a setting is a read: the aids are the
    // same whether the caller holds a const bus or not.
    mutable AidSettings cache_{};
    mutable std::uint64_t cached_app_revision_{0};
    mutable std::uint64_t cached_session_revision_{0};
    mutable double cached_scale_{-1.0};
};

/// Whether `p`'s answer is aimed from its rubber origin — the base dik mod,
/// kutupsal izleme and the normal lock lay their rays through
/// (`Prompt::rubber_base`). The command's resolver and the canvas's marker both
/// ask this, so the marker cannot promise a point the command will not take.
bool aimed_from_origin(const Prompt& p) noexcept;

/// The aids that apply to `p`: `set`, with dik mod left out for the opposite
/// corner of a rectangle. Locked to an axis through the first corner, that
/// corner makes a rectangle with no width or no height — refused by every
/// command that asks for one, so a user with dik mod on could not drag a
/// window at all. Kutupsal izleme still applies there: at half a right angle it
/// is what draws a square. And with no aid at all for a pick
/// (`RubberShape::Trim`): the click names a piece, not a point.
AidSettings aids_for(const AidSettings& set, const Prompt& p);

} // namespace kentos::command
