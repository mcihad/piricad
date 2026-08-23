// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the input aids, and where they are applied.
//
// An "input aid" is anything that moves a point the user aimed at to the point
// they meant: object snap, `dik mod`, `kutupsal izleme` and `ızgaraya yakalama`.
// The
// engine lives in `piricad/core/snap.hpp` and knows no client; this file is the
// seam that gives it a tolerance and a document.
//
// WHY IT IS HERE AND NOT IN THE CANVAS. Constitution Article 1.2 makes the GUI
// one client among equals. If snapping lived in the mouse handler, drawing from
// the command line, from a script or from an AI suggestion would silently land
// somewhere else than drawing with the hand — and "somewhere else" in a cadastral
// drawing is a gap in a parcel boundary. The aids are therefore applied inside
// `InputAwaiter<Point2>::await_resume` (context.hpp), on the one path every
// `co_await ctx.point(...)` takes, whatever supplied the value. No client is
// asked where it came from; `InputSource` is never consulted (command.md P10).
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

#include "piricad/core/settings.hpp"
#include "piricad/core/snap.hpp"

#include <cstdint>

namespace piricad::core {
/// Forward-declared: the aids read a document to snap against, and this header
/// must stay includable from anywhere in /src/command.
class Document;
} // namespace piricad::core

namespace piricad::command {

/// The aid settings as numbers the engine can use, resolved from the two stores
/// that own them. Assembled in one place so no caller reads a setting id twice.
struct AidSettings
{
    std::uint16_t modes{core::SnapNone}; ///< which object snaps are enabled
    core::Mm snap_radius{0};             ///< object-snap aperture in document millimetres
    core::Mm pick_radius{0};             ///< single-click pick box in document millimetres
    core::Mm grid_step{0};               ///< lattice spacing in document millimetres
    bool ortho{false};                   ///< `dik mod`: lock the cursor to the two axes
    std::int64_t polar_step{0};          ///< micro-degrees
};

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
    core::SnapResult resolve(const core::Document& doc, const AidSettings& s, core::Point2 aim,
                             bool has_base, core::Point2 base) const;

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

} // namespace piricad::command
