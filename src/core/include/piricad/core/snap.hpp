// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: object snap, ortho and polar tracking.
//
// A snap engine takes an AIM — wherever the user, the script or the model pointed
// — and returns the point they meant. It is the difference between a drawing that
// closes and a parcel with a 3 mm gap in its boundary, and a 3 mm gap is a failed
// TKGM kontrol (§12).
//
// THE ENGINE DOES NOT KNOW WHERE THE AIM CAME FROM. There is no mouse in this
// header, no view, no widget: an aim is a `Point2` and a tolerance is a distance.
// piricad.md §2.4 makes that the most critical detail of the architecture, and it
// is what lets the same snap apply to a mouse click, a typed coordinate, a script
// argument and an AI-produced point without one line of branching.
//
// The TOLERANCE is a world distance here, but the settings that feed it are in
// SCREEN PIXELS (core.yakalama.tolerans), because a user aims at what they can
// see: at 1:1000 a 12-pixel aperture is metres, at 1:10 it is centimetres. The
// pixels-to-millimetres conversion needs a view scale, so it happens one layer up
// (`piricad/command/aids.hpp`). A caller with no view — a headless replay, a
// batch job — passes radius 0, and object snap is then inert by construction
// rather than by a client check.
//
// ORDER, and it is deliberate:
//
//   1. object snap   — a real feature of a real entity beats every other aid
//   2. ortho / polar — a direction constraint from the previous point
//   3. grid          — the fallback lattice
//
// A hit at step 1 returns immediately: snapping to a parcel corner and then
// dragging that corner onto the grid would produce a point that is neither.
//
// IDEMPOTENCE is a requirement, not a nicety. `Bus` records the RESOLVED point in
// the journal, and a journal replay feeds that point straight back in, so
// snap(snap(p)) MUST equal snap(p) or a replay would drift. Every rule below is
// idempotent: a point already on a vertex is at distance zero from it, a point
// already on the grid rounds to itself, a point already on an axis stays on it.
#pragma once

#include "piricad/core/identity.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>

namespace piricad::core {

/// Forward-declared: the engine searches a document, and core headers avoid
/// including one another where a declaration will do.
class Document;

/// Bits of `core.yakalama.modlar`. The mask IS the engine's input, so the MOD
/// command, the F-key toggles and a script all write the same sixteen bits and
/// there is no second list of snap modes anywhere (CLAUDE.md 5.10).
enum SnapMode : std::uint16_t {
    SnapNone = 0,

    SnapEndpoint      = 1u << 0, ///< UÇ      — a ring vertex
    SnapMidpoint      = 1u << 1, ///< ORTA    — the middle of a segment
    SnapCenter        = 1u << 2, ///< MERKEZ  — the centroid of a closed ring
    SnapIntersection  = 1u << 3, ///< KESİŞİM — where two segments cross
    SnapPerpendicular = 1u << 4, ///< DİK     — the foot of a perpendicular from the last point
    SnapNearest       = 1u << 5, ///< YAKIN   — the closest point of a segment
    SnapGrid          = 1u << 6, ///< IZGARA  — the nearest grid intersection
    SnapPolar         = 1u << 7, ///< KUTUPSAL— the nearest polar ray from the last point

    /// RESULT ONLY, never requested: dik mod (core.yakalama.dik_mod) moved the
    /// point. It is a separate boolean setting, not a bit of the mask, because a
    /// surveyor turns it on and off with F8 twenty times an hour.
    SnapOrtho = 1u << 8,

    // ---- constructed points: not on the drawing, but implied by it -----------
    //
    // The three below are what a cadastral or zoning job needs when the thing to
    // snap to IS NOT DRAWN. A corner monument is gone and the boundary has to be
    // re-established from the two edges that survive; a çekme mesafesi runs
    // parallel to a road nobody has drawn an offset for yet. They are ranked
    // BELOW every real feature, so a constructed point can never take a corner
    // that actually exists away from the user.

    /// DÜĞÜM — a lone surveyed point: the control point, traverse station or
    /// benchmark a cadastral job works from.
    ///
    /// This bit was declared and left unused for exactly as long as the document
    /// could not hold such a thing, because a snap mode for something that cannot
    /// exist is a promise the program does not keep. `core.point` holds one now,
    /// and a monument is the single most important thing on a cadastral sheet to
    /// snap to — every boundary is measured from one.
    SnapNode = 1u << 9,

    SnapExtension = 1u << 10, ///< UZANTI  — the line of a segment, past its own end
    SnapParallel  = 1u << 11, ///< PARALEL — a ray from the last point, parallel to an edge
    SnapApparent  = 1u << 12, ///< UZATILMIŞ KESİŞİM — where two edges' lines would cross

    /// The modes that need geometry to snap to. Grid and polar need none.
    SnapObjectMask = SnapEndpoint | SnapMidpoint | SnapCenter | SnapIntersection |
                     SnapPerpendicular | SnapNearest | SnapNode | SnapExtension | SnapParallel |
                     SnapApparent,

    /// The modes that look BEYOND the aperture, because the point they build is
    /// not where the geometry that implies it is. They are the only reason
    /// `SnapQuery::reach` exists, and they are off unless the user asks.
    SnapConstructedMask = SnapExtension | SnapParallel | SnapApparent,

    /// Every mode a user may switch on.
    SnapAllMask = SnapObjectMask | SnapGrid | SnapPolar,
};

/// Stable machine name of ONE mode bit — "uc", "orta", "izgara". Used by the MOD
/// transcript, the generated documentation and the canvas marker table.
const char* snap_mode_id(std::uint16_t single_bit);

/// Turkish label of ONE mode bit — "uç nokta", "orta nokta" (piricad.md §13).
const char* snap_mode_label(std::uint16_t single_bit);

/// Every declared bit, low to high, terminated by SnapNone. Iterating this is how
/// callers render a mask without writing the list a second time.
const std::uint16_t* snap_mode_bits();

/// One aim, and everything the engine may use to resolve it.
struct SnapQuery
{
    Point2 aim{};                  ///< where the client pointed, in document millimetres
    Mm radius{0};                  ///< object-snap search radius; 0 disables object snap
    std::uint16_t modes{SnapNone}; ///< bit mask of the enabled object snaps
    Mm grid_step{0};               ///< lattice spacing; 0 disables the grid even if the bit is set
    bool ortho{false};             ///< dik mod
    std::int64_t polar_step{0};    ///< micro-degrees; 0 disables polar even if the bit is set
    bool has_base{false};          ///< a previous point exists (rubber-band origin)
    Point2 base{};                 ///< that previous point — ortho, polar and DİK measure from it

    /// How far past the aperture the constructed modes may look, in millimetres.
    ///
    /// UZANTI, PARALEL and UZATILMIŞ KESİŞİM build a point from an edge that is
    /// NOT under the cursor — that is the whole point of them — so the aperture
    /// alone would never find the edge. Zero switches all three off however the
    /// mask is set, which is the same "no view, no aid" contract `radius`,
    /// `grid_step` and `polar_step` already keep.
    ///
    /// It also bounds the cost: the search box grows by this much and no more, so
    /// a mode that reads more of the drawing still reads a fixed amount of it.
    Mm reach{0};
};

/// What the engine decided, and why. `mode` is SnapNone when nothing applied and
/// the aim is returned untouched — which is the answer a headless replay gets.
struct SnapResult
{
    Point2 point{};               ///< where the point ended up
    std::uint16_t mode{SnapNone}; ///< the single bit that produced the point
    EntityId entity{kNoEntity};   ///< the entity snapped to, for the canvas marker
    bool constrained{false};      ///< ortho or polar moved the point along a direction
};

/// Resolves one aim. Reads the document, never writes it (model.md R43).
SnapResult snap(const Document& doc, const SnapQuery& query);

// ---- the individual rules, exposed so each is testable on its own -----------

/// Nearest lattice intersection. `step <= 0` returns `p` unchanged.
Point2 apply_grid(Point2 p, Mm step) noexcept;

/// The lattice spacing that is ACTUALLY IN FORCE at this zoom.
///
/// ONE ANSWER FOR TWO READERS, and that is the whole reason this exists. The
/// canvas draws the grid and the snap engine snaps to it, and they used to
/// compute the spacing separately: the canvas rounded it to the 1-2-5 ladder
/// from the zoom when the mode is `uyarlanır`, and the snap read the declared
/// step regardless. With adaptive spacing on — the default — the lines a user
/// could see were 50 m apart and their clicks landed on a 10 m lattice that was
/// nowhere on screen. It looked like snapping to a grid from some earlier moment,
/// which is exactly what it was: the one the setting still named.
///
/// `declared` is `core.izgara.adim`, `adaptive` is `core.izgara.mod == uyarlanır`,
/// and `mm_per_pixel` is the view's. Returns 0 when nothing should be drawn or
/// snapped to, which both callers already treat as "no grid".
Mm grid_step_in_force(Mm declared, bool adaptive, double mm_per_pixel) noexcept;

/// Locks `p` onto the horizontal or vertical axis through `base`, whichever the
/// aim is already closer to. This is dik mod.
Point2 apply_ortho(Point2 base, Point2 p) noexcept;

/// Rotates `p` onto the nearest ray leaving `base` at a multiple of `step_udeg`,
/// keeping its distance from `base`. This is kutupsal izleme.
Point2 apply_polar(Point2 base, Point2 p, std::int64_t step_udeg) noexcept;

} // namespace piricad::core
