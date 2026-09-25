// SPDX-License-Identifier: GPL-3.0-or-later
// core.move (TAŞI), core.rotate (DÖNDÜR), core.scale (ÖLÇEKLE), core.mirror (AYNALA)
//
// The four transforms every drawing program has, and the four a cadastral one
// cannot do without: a block placed off station, a plan sheet turned to grid
// north, a sketch brought to scale, a symmetrical building block built from one
// half.
//
// They share everything except one function. `Xform` says WHAT is being done and
// each kind answers for its own defining numbers, which is why a circle keeps
// being a circle when it is turned — a transform that walked raw vertices would
// move a circle's centre and its radius handle independently and leave a record
// that is no longer a circle at all.
//
// IDENTITY SURVIVES. Every one of these edits geometry through
// `Transaction::set_geometry`, so the key, the layer, the style, the attributes
// and the text stay with the object: a parsel moved onto its correct station is
// the same parsel, with the same ada/parsel numbers (model.md R4, R28).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/transform_edit.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/transform.hpp"
#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// `Xform` AND ITS APPLICATION MOVED TO `core` (core/transform.hpp), because the
/// canvas needs the same description to draw the ghost that promises what the
/// click will do. Two callers, one transform, no drift. The names stay short
/// here so the bodies below read as they did.
using Xform = core::Xform;

core::Point2 apply(const Xform& x, core::Point2 p)
{
    return core::transformed(x, p);
}

/// The ghost a verb asks for: WHICH transform the cursor is completing, so the
/// canvas draws the result rather than a slide — and of WHICH objects, the ones
/// the verb resolved, so a verb told its objects by name draws those rather
/// than whatever is highlighted.
PointOptions ghost(core::GhostKind kind, core::Point2 base, const std::vector<std::int64_t>& keys,
                   std::int64_t copies = 1, std::int64_t reference_udeg = 0,
                   core::Mm reference_length = 0)
{
    return PointOptions{.rubber_band    = true,
                        .rubber_origin  = base,
                        .rubber_shape   = RubberShape::Ghost,
                        .rubber_payload = core::encode_ghost_spec(
                            core::GhostSpec{.kind             = kind,
                                            .copies           = copies,
                                            .keys             = keys,
                                            .reference_udeg   = reference_udeg,
                                            .reference_length = reference_length})};
}

/// How a curve's radius changes. Only scaling touches it: moving, turning and
/// mirroring a circle leave a circle of exactly the same size, and recomputing
/// the radius from two transformed points would let it drift by a millimetre
/// every time the object was moved.
core::Mm apply_radius(const Xform& x, core::Mm r)
{
    // A block's placement scales by an exact ratio, the one its vertices were
    // placed by, so a member circle's rim meets the lines drawn beside it.
    if (x.kind == Xform::Kind::Place) return core::place_length(x, r);
    if (x.kind != Xform::Kind::Scale && x.kind != Xform::Kind::Align) return r;
    return core::mm_round(static_cast<double>(r) * x.factor);
}

/// The factor a transform scales every length by, 1 when it scales none —
/// a stretch that differs across and up has no one factor, and keeps
/// annotation sizes as they were.
double length_factor(const Xform& x)
{
    return x.kind == Xform::Kind::Scale || x.kind == Xform::Kind::Align ? x.factor : 1.0;
}

/// Whether the transform turns the plane inside out, so a counter-clockwise sweep
/// becomes a clockwise one. An arc's two ends are then given the other way round,
/// which is how `core.arc` spells a reversed sweep (core/arc.hpp).
bool reverses(const Xform& x)
{
    return core::xform_reverses(x);
}

/// How a stored ANGLE changes: a turn adds itself, a mirror about an axis at θ
/// takes an angle α to 2θ − α, the rest leave it. In whole micro-degrees from
/// `atan2_udeg`, never libm (§7.3).
std::int64_t apply_angle(const Xform& x, std::int64_t udeg)
{
    std::int64_t out = udeg;
    switch (x.kind) {
    case Xform::Kind::Rotate:
        out += core::atan2_udeg(static_cast<std::int64_t>(std::llround(x.turn.sin * 1e9)),
                                static_cast<std::int64_t>(std::llround(x.turn.cos * 1e9)));
        break;
    case Xform::Kind::Mirror:
        out = 2 * core::atan2_udeg(x.axis_b.y - x.base.y, x.axis_b.x - x.base.x) - udeg;
        break;
    case Xform::Kind::Align:
        out = (x.flip ? -udeg : udeg) +
              core::atan2_udeg(static_cast<std::int64_t>(std::llround(x.turn.sin * 1e9)),
                               static_cast<std::int64_t>(std::llround(x.turn.cos * 1e9)));
        break;
    case Xform::Kind::Stretch: {
        // A DIRECTION under a stretch leans toward the axis stretched more.
        const core::SinCos d = core::sin_cos_udeg(udeg);
        out = core::atan2_udeg(static_cast<std::int64_t>(std::llround(d.sin * x.factor_y * 1e9)),
                               static_cast<std::int64_t>(std::llround(d.cos * x.factor * 1e9)));
        break;
    }
    case Xform::Kind::Place: {
        // A BLOCK'S PLACEMENT: the direction through the signed scales, then
        // the turn. Uniform, it is exact — a mirror across is α → 180° − α, a
        // mirror up α → −α, both at once the half turn — and only a scale that
        // differs across and up needs the trigonometry a stretch does.
        const bool across       = x.place_sx.num < 0;
        const bool up           = x.place_sy.num < 0;
        const std::int64_t half = core::kUDegFullCircle / 2;
        if (core::place_uniform(x)) {
            out = across == up ? udeg + (across ? half : 0) : (across ? half : 0) - udeg;
        } else {
            const core::SinCos d = core::sin_cos_udeg(udeg);
            const double sx =
                static_cast<double>(x.place_sx.num) / static_cast<double>(x.place_sx.den);
            const double sy =
                static_cast<double>(x.place_sy.num) / static_cast<double>(x.place_sy.den);
            out = core::atan2_udeg(static_cast<std::int64_t>(std::llround(d.sin * sy * 1e9)),
                                   static_cast<std::int64_t>(std::llround(d.cos * sx * 1e9)));
        }
        out += x.place_udeg;
        break;
    }
    default: break;
    }
    out %= core::kUDegFullCircle;
    if (out < 0) out += core::kUDegFullCircle;
    return out;
}

/// A length under the transform: only a scale changes it.
core::Mm apply_length(const Xform& x, core::Mm v)
{
    return apply_radius(x, v);
}

/// A text's height under the transform: a scale or a scaling alignment makes
/// the letters as much bigger as the drawing; a stretch keeps them, because a
/// letter has one height and no width to stretch (TODOS C-08).
core::Mm apply_height(const Xform& x, core::Mm h)
{
    if (x.kind == Xform::Kind::Place) return std::max<core::Mm>(1, core::place_length(x, h));
    const double f = length_factor(x);
    if (f == 1.0) return h;
    return std::max<core::Mm>(1, core::mm_round(static_cast<double>(h) * f));
}

/// `a · b`, exact and reduced — or nothing when the exact terms would not fit
/// in 64 bits, which a caller answers with the six-decimal product.
std::optional<core::Ratio> ratio_times(core::Ratio a, core::Ratio b)
{
    core::Int128 num = static_cast<core::Int128>(a.num) * static_cast<core::Int128>(b.num);
    core::Int128 den = static_cast<core::Int128>(a.den) * static_cast<core::Int128>(b.den);
    if (den < 0) {
        num = -num;
        den = -den;
    }
    // Euclid over the 128-bit terms: `std::gcd` is not promised for them.
    core::Int128 p = num < 0 ? -num : num;
    core::Int128 q = den;
    while (q != 0) {
        const core::Int128 t = p % q;
        p                    = q;
        q                    = t;
    }
    if (p > 1) {
        num /= p;
        den /= p;
    }
    const auto limit = static_cast<core::Int128>(std::numeric_limits<std::int64_t>::max());
    if (den == 0 || den > limit || num > limit || -num > limit) return std::nullopt;
    return core::Ratio{static_cast<std::int64_t>(num), static_cast<std::int64_t>(den)};
}

/// The magnitude of a ratio: its sign dropped, which lives in the numerator.
core::Ratio magnitude(core::Ratio r)
{
    return core::Ratio{r.num < 0 ? -r.num : r.num, r.den};
}

/// A rational scale under the transform: multiplied by the factor, to six
/// decimals, reduced — exactly, by a block's own ratio, under its placement.
core::Ratio apply_ratio(const Xform& x, core::Ratio r)
{
    if (x.kind == Xform::Kind::Place) {
        if (!core::place_uniform(x)) return r;
        if (const auto exact = ratio_times(r, magnitude(x.place_sx))) return *exact;
        Xform approx;
        approx.kind = Xform::Kind::Scale;
        approx.factor =
            static_cast<double>(magnitude(x.place_sx).num) / static_cast<double>(x.place_sx.den);
        return apply_ratio(approx, r);
    }
    if (x.kind != Xform::Kind::Scale && x.kind != Xform::Kind::Align) return r;
    const double v       = static_cast<double>(r.num) / static_cast<double>(r.den) * x.factor;
    const auto num       = static_cast<std::int64_t>(std::llround(v * 1000000.0));
    std::int64_t den     = 1000000;
    const std::int64_t g = std::gcd(num < 0 ? -num : num, den);
    return g > 1 ? core::Ratio{num / g, den / g} : core::Ratio{num, den};
}

/// The entity's rings, transformed: every stored vertex moves. A closed ring is
/// reversed under a reflection so its winding, and the sign of its area, stay
/// what R11 requires; an open one keeps its order, because a spline's control
/// polygon, a dimension's definition points and a text baseline all MEAN their
/// order.
void transformed_rings(const core::RingGeometry& g, std::uint32_t gslot, const Xform& x,
                       std::vector<std::vector<core::Point2>>& rings,
                       std::vector<core::RingGeometry::RingInput>& input)
{
    const core::RingSpan span = g.rings_of(gslot);
    rings.clear();
    input.clear();
    rings.reserve(span.count);
    input.reserve(span.count);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = g.ring_xs(r);
        const auto ys = g.ring_ys(r);
        std::vector<core::Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            pts.push_back(apply(x, core::Point2{xs[v], ys[v]}));
        if (reverses(x) && g.ring_role[r] != core::RingRole::Open)
            std::reverse(pts.begin(), pts.end());
        rings.push_back(std::move(pts));
    }
    for (std::size_t i = 0; i < rings.size(); ++i)
        input.push_back(core::RingGeometry::RingInput{rings[i], g.ring_role[span.first + i],
                                                      g.ring_part[span.first + i]});
}

/// Applies `x` to a kind whose payload holds coordinates or angles, so rings
/// and payload change together (`Transaction::set_kind_geometry`).
bool transform_payload_kind(Context& ctx, core::EntityId slot, const Xform& x)
{
    const core::Document& doc   = ctx.document();
    const core::RingGeometry& g = doc.geometry();
    const std::uint32_t gslot   = doc.entities().slot[slot];
    const core::KindId kind     = doc.entities().kind[slot];
    std::vector<std::vector<core::Point2>> rings;
    std::vector<core::RingGeometry::RingInput> input;
    transformed_rings(g, gslot, x, rings, input);
    std::vector<std::uint8_t> payload;
    // A dimension re-laid for its new points carries its new caption here.
    bool relaid = false;
    std::string relaid_text;
    core::Mm relaid_height = 0;

    if (kind == core::kArcPolylineKind) {
        if (x.kind == Xform::Kind::Place && !core::place_uniform(x)) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Blokta yaylı bir çoklu çizgi var ve referans eşit olmayan bir ölçekle "
                       "konmuş: bu ölçek yaylarını eliptik yapar, yaylı çoklu çizgi ise yalnız "
                       "dairesel yay taşır. Referansı önce eşit ölçeğe getirin (ÖLÇEKLE).");
            return false;
        }
        if (x.kind == Xform::Kind::Stretch) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(core::raw(doc.key_of(slot))) +
                           " yaylı bir çoklu çizgi: eşit olmayan ölçek yaylarını eliptik yapar ve "
                           "yaylı çoklu çizgi yalnız dairesel yay taşır. Eşit ölçek kullanın ya "
                           "da önce PATLAT ile kenarlarına ayırın.");
            return false;
        }
        auto def = core::arc_polyline_of(g, gslot);
        if (!def) {
            ctx.refuse(def.error());
            return false;
        }
        core::ArcPolyline ap      = def.value();
        ap.constant_width         = apply_length(x, ap.constant_width);
        const core::RingSpan span = g.rings_of(gslot);
        const bool closed   = span.count > 0 && g.ring_role[span.first] != core::RingRole::Open;
        const std::size_t n = rings.empty() ? 0 : rings[0].size();
        for (core::ArcPolyline::Arc& a : ap.arcs) {
            a.centre = apply(x, a.centre);
            a.radius = apply_radius(x, a.radius);
            if (a.radius <= 0) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Ölçekleme bir yay kenarı sıfır yarıçapa indiriyor.");
                return false;
            }
            if (reverses(x)) {
                // Mirrored: the sweep flips. On a closed ring the vertices were
                // reversed too, which flips it back and renumbers the edges.
                if (closed && n > 0)
                    a.segment = a.segment + 1 >= n ? static_cast<std::uint32_t>(n - 1)
                                                   : static_cast<std::uint32_t>(n - 2 - a.segment);
                else
                    a.ccw = !a.ccw;
            }
        }
        std::sort(ap.arcs.begin(), ap.arcs.end(),
                  [](const core::ArcPolyline::Arc& a, const core::ArcPolyline::Arc& b) {
                      return a.segment < b.segment;
                  });
        payload = core::encode_arc_polyline(ap);
    } else if (kind == core::kHatchKind) {
        auto def = core::hatch_of(g, gslot);
        if (!def) {
            ctx.refuse(def.error());
            return false;
        }
        core::HatchDef h = def.value();
        h.angle_udeg     = apply_angle(x, h.angle_udeg);
        h.scale          = apply_ratio(x, h.scale);
        h.origin         = apply(x, h.origin);
        payload          = core::encode_hatch(h);
        // The pattern is drawn by the style (model.md R14), so the style follows.
        const core::StyleId st = doc.entities().style[slot];
        std::uint32_t ink      = 0xFF000000u;
        if (st != core::kByLayerStyle && st < doc.styles().size())
            ink = doc.styles().symbol_at(st).primary().rgba;
        if (auto s = ctx.transaction().set_entity_style(
                slot, ctx.transaction().intern_symbol(hatch_symbol(h, ink)));
            !s) {
            ctx.refuse(s.error());
            return false;
        }
    } else if (kind == core::kBlockReferenceKind) {
        auto def = core::block_reference_of(g, gslot);
        if (!def) {
            ctx.refuse(def.error());
            return false;
        }
        core::BlockReference ref = def.value();
        // Whether the reflection is already in the signed scales, so the
        // general rule below must not add it a second time.
        bool signed_scales = false;
        if (x.kind == Xform::Kind::Place && !core::place_uniform(x)) {
            // A BLOCK INSIDE A BLOCK, taken out by PATLAT from a placement that
            // differs across and up. The outer scales commute with the inner
            // turn only at a quarter — there they trade places — and any
            // other turn would lean the inner block, which it cannot hold.
            const std::int64_t quarter = core::kUDegFullCircle / 4;
            std::int64_t turned        = ref.rotation_udeg % core::kUDegFullCircle;
            if (turned < 0) turned += core::kUDegFullCircle;
            if (turned % quarter != 0) {
                ctx.refuse(core::ErrorCode::Unsupported,
                           "Blokta döndürülmüş bir iç blok var ve referans eşit olmayan bir "
                           "ölçekle konmuş: iç blok bu ölçekte eğilir ve blok referansı "
                           "eğikliği taşıyamaz. Referansı önce eşit ölçeğe getirin (ÖLÇEKLE).");
                return false;
            }
            const bool sideways     = (turned / quarter) % 2 != 0;
            const core::Ratio along = sideways ? x.place_sy : x.place_sx;
            const core::Ratio up    = sideways ? x.place_sx : x.place_sy;
            const auto sx           = ratio_times(ref.sx, along);
            const auto sy           = ratio_times(ref.sy, up);
            if (!sx || !sy) {
                ctx.refuse(core::ErrorCode::Unsupported,
                           "Blokta bir iç blok var ve iki ölçeğin çarpımı tam olarak yazılamıyor.");
                return false;
            }
            ref.sx             = *sx;
            ref.sy             = *sy;
            ref.column_spacing = core::mul_div_round(ref.column_spacing, along.num, along.den);
            ref.row_spacing    = core::mul_div_round(ref.row_spacing, up.num, up.den);
            ref.rotation_udeg += x.place_udeg;
            signed_scales = true;
        } else if (x.kind == Xform::Kind::Stretch) {
            // A STRETCH A BLOCK CAN HOLD: across and up its own axes, which is
            // only when it stands square — turned a quarter, the two factors
            // trade places. Turned any other way it would lean, and a
            // reference has no lean to hold.
            const std::int64_t quarter = core::kUDegFullCircle / 4;
            const std::int64_t turned  = ref.rotation_udeg % core::kUDegFullCircle;
            if (turned % quarter != 0) {
                ctx.refuse(core::ErrorCode::Unsupported,
                           "Nesne " + std::to_string(core::raw(doc.key_of(slot))) +
                               " döndürülmüş bir blok: eşit olmayan ölçek onu eğer ve blok "
                               "referansı eğikliği taşıyamaz. Eşit ölçek kullanın ya da önce "
                               "PATLAT ile açın.");
                return false;
            }
            const bool sideways = (turned / quarter) % 2 != 0;
            const double across = sideways ? x.factor_y : x.factor;
            const double up     = sideways ? x.factor : x.factor_y;
            Xform sx_only;
            sx_only.kind   = Xform::Kind::Scale;
            sx_only.factor = across;
            Xform sy_only;
            sy_only.kind       = Xform::Kind::Scale;
            sy_only.factor     = up;
            ref.sx             = apply_ratio(sx_only, ref.sx);
            ref.sy             = apply_ratio(sy_only, ref.sy);
            ref.column_spacing = apply_length(sx_only, ref.column_spacing);
            ref.row_spacing    = apply_length(sy_only, ref.row_spacing);
        } else {
            ref.rotation_udeg  = apply_angle(x, ref.rotation_udeg);
            ref.sx             = apply_ratio(x, ref.sx);
            ref.sy             = apply_ratio(x, ref.sy);
            ref.column_spacing = apply_length(x, ref.column_spacing);
            ref.row_spacing    = apply_length(x, ref.row_spacing);
        }
        // A reflection of R(ρ)·S is R(2θ−ρ)·S with the y scale negated — and
        // with the row step negated too, because the grid is stepped in the
        // same frame: a two-row reference mirrored in a horizontal line had
        // its second row drawn on the side it came from.
        if (reverses(x) && !signed_scales) {
            ref.sy.num      = -ref.sy.num;
            ref.row_spacing = -ref.row_spacing;
        }
        ref.rotation_udeg %= core::kUDegFullCircle;
        if (ref.rotation_udeg < 0) ref.rotation_udeg += core::kUDegFullCircle;
        const core::Point2 at = rings.empty() || rings[0].empty() ? core::Point2{} : rings[0][0];
        ref.bounds            = core::block_reference_bounds(doc, at, ref);
        payload               = core::encode_block_reference(ref);
    } else if (kind == core::kDimensionKind) {
        auto def = core::dimension_of(g, gslot);
        if (!def) {
            ctx.refuse(def.error());
            return false;
        }
        core::DimensionDef d = def.value();
        if (d.type == core::DimensionType::Linear)
            d.rotation_udeg = apply_angle(x, d.rotation_udeg);
        const bool angle =
            d.type == core::DimensionType::Angular || d.type == core::DimensionType::Angular3P;
        if (!angle) d.measurement = apply_length(x, d.measurement);
        d.arrow_size       = apply_length(x, d.arrow_size);
        d.extension_beyond = apply_length(x, d.extension_beyond);
        d.extension_offset = apply_length(x, d.extension_offset);
        d.text_gap         = apply_length(x, d.text_gap);

        // RE-LAID, NOT ONLY CARRIED (TODOS C-08, C-17). A dimension is
        // measured again from the points it now has and its caption set —
        // and fitted — where ÖLÇÜ would set it, above its line the right way
        // up; a caption placed by hand goes where the transform takes it,
        // reading along the dimension. A LINEAR one keeps the direction the
        // drawing was turned to (the layout's fixed rotation) and is measured
        // along it: turned half a circle, its figure is above its line again
        // rather than hanging under it. An ordinate or a four-point angle
        // from a file is re-measured and keeps its caption.
        const core::Mm height =
            doc.texts().has(gslot) ? apply_height(x, doc.texts().height(gslot)) : 0;
        const core::DrawingUnit unit = core::drawing_unit_from_setting(
            ctx.session().bus().project_settings().get("core.cizim.birim").as_enum());
        if (rings.size() >= 2 && !rings[0].empty()) {
            std::vector<core::Point2> picks;
            core::Point2 where{};
            core::DimensionLayout layout;
            if (core::dimension_picks(d.type, rings[1], rings[0][0], picks, where) &&
                core::dimension_layout(d, picks, where, height, layout,
                                       d.type == core::DimensionType::Linear)) {
                rings[1]    = layout.defs;
                relaid_text = core::dimension_text(d, unit);
                const auto base =
                    core::dimension_caption_baseline(d, layout, relaid_text, height, rings[0][0]);
                rings[0] = {base[0], base[1]};
            } else {
                if (x.kind == Xform::Kind::Stretch ||
                    (x.kind == Xform::Kind::Place && !core::place_uniform(x)))
                    d.measurement = core::dimension_measure(d.type, rings[1], d.rotation_udeg);
                relaid_text = core::dimension_text(d, unit);
                if (rings[0].size() >= 2) {
                    const core::Point2 c = rings[0][0];
                    const core::Point2 e = rings[0][1];
                    if (e.x < c.x || (e.x == c.x && e.y < c.y))
                        rings[0][1] = core::Point2{2 * c.x - e.x, 2 * c.y - e.y};
                }
            }
            relaid_height = height;
            relaid        = true;
            for (std::size_t i = 0; i < rings.size() && i < input.size(); ++i)
                input[i].points = rings[i];
        }
        payload = core::encode_dimension(d);
    } else if (kind == core::kLeaderKind) {
        auto def = core::leader_of(g, gslot);
        if (!def) {
            ctx.refuse(def.error());
            return false;
        }
        core::LeaderDef l = def.value();
        l.arrow_size      = apply_length(x, l.arrow_size);
        payload           = core::encode_leader(l);
    } else {
        return false;
    }

    auto st = ctx.transaction().set_kind_geometry(slot, input, payload);
    if (!st) {
        ctx.refuse(st.error());
        return false;
    }
    if (kind == core::kDimensionKind && doc.texts().has(doc.entities().slot[slot])) {
        const std::uint32_t now = doc.entities().slot[slot];
        const std::string text  = relaid ? relaid_text : std::string(doc.texts().text(now));
        const core::Mm height   = relaid ? relaid_height : apply_height(x, doc.texts().height(now));
        if (auto s = ctx.transaction().set_text(slot, text, height, doc.texts().anchor(now)); !s) {
            ctx.refuse(s.error());
            return false;
        }
    }
    return true;
}

/// Applies `x` to one entity, whatever kind it is. Returns false having echoed
/// why, so a caller can stop the whole command rather than half-apply it.
bool transform_one(Context& ctx, core::EntityId slot, const Xform& x)
{
    const core::Document& doc   = ctx.document();
    const core::RingGeometry& g = doc.geometry();
    const std::uint32_t gslot   = doc.entities().slot[slot];
    const core::KindId kind     = doc.entities().kind[slot];

    // AN ELLIPSE IS WRITTEN AS ITS IMAGE (`core::transformed_ellipse`): its
    // axes perpendicular again after a stretch, the right way round after a
    // reflection, its sweep re-read in the new axes. A circle or an arc
    // stretched unevenly is no longer circular, and becomes the ellipse or the
    // elliptic arc it now is — the same object, by the same key (model.md R9b,
    // TODOS C-08: "eşit olmayan ölçek daireyi uygun elipse dönüştürür").
    const auto write_ellipse = [&ctx, slot](const core::EllipseImage& img) {
        const std::array<core::Point2, 3> pts{img.centre, img.major, img.minor};
        const core::RingGeometry::RingInput ring{std::span<const core::Point2>(pts),
                                                 core::RingRole::Open, 0};
        std::vector<std::uint8_t> sweep;
        if (img.partial)
            sweep = core::encode_ellipse_arc(
                core::EllipseArc{.start_udeg = img.start_udeg, .end_udeg = img.end_udeg});
        const core::Status st =
            ctx.transaction().set_kind_geometry(slot, core::kEllipseKind, {&ring, 1}, sweep);
        if (!st) ctx.refuse(st.error());
        return static_cast<bool>(st);
    };
    const bool uneven = (x.kind == Xform::Kind::Stretch && x.factor != x.factor_y) ||
                        (x.kind == Xform::Kind::Place && !core::place_uniform(x));
    if (kind == core::kEllipseKind) {
        core::EllipseImage e{.centre = core::ellipse_centre_of(g, gslot),
                             .major  = core::ellipse_major_of(g, gslot),
                             .minor  = core::ellipse_minor_of(g, gslot)};
        if (const auto arc = core::ellipse_arc_of(g, gslot); arc.has_value()) {
            e.partial    = true;
            e.start_udeg = arc->start_udeg;
            e.end_udeg   = arc->end_udeg;
        }
        return write_ellipse(core::transformed_ellipse(x, e));
    }
    if (kind == core::kCircleKind && uneven) {
        const core::Point2 c = core::circle_centre_of(g, gslot);
        const core::Mm r     = core::circle_radius_of(g, gslot);
        return write_ellipse(core::transformed_ellipse(
            x, core::EllipseImage{.centre = c, .major = {c.x + r, c.y}, .minor = {c.x, c.y + r}}));
    }
    if (kind == core::kArcKind && uneven) {
        const core::Point2 c = core::arc_centre_of(g, gslot);
        const core::Mm r     = core::arc_radius_of(g, gslot);
        const core::Point2 a = core::arc_start_of(g, gslot);
        const core::Point2 b = core::arc_end_of(g, gslot);
        return write_ellipse(core::transformed_ellipse(
            x, core::EllipseImage{.centre     = c,
                                  .major      = {c.x + r, c.y},
                                  .minor      = {c.x, c.y + r},
                                  .partial    = true,
                                  .start_udeg = core::atan2_udeg(a.y - c.y, a.x - c.x),
                                  .end_udeg   = core::atan2_udeg(b.y - c.y, b.x - c.x)}));
    }

    if (kind == core::kCircleKind) {
        const core::Point2 centre = apply(x, core::circle_centre_of(g, gslot));
        const core::Mm radius     = apply_radius(x, core::circle_radius_of(g, gslot));
        if (radius <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Ölçekleme daireyi sıfır yarıçapa indiriyor.");
            return false;
        }
        const core::Point2 pts[2]{centre, core::Point2{centre.x + radius, centre.y}};
        const core::RingGeometry::RingInput ring{std::span<const core::Point2>(pts, 2),
                                                 core::RingRole::Open, 0};
        auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
        if (!st) {
            ctx.refuse(st.error());
            return false;
        }
        return true;
    }

    if (kind == core::kArcKind) {
        const core::Point2 centre = apply(x, core::arc_centre_of(g, gslot));
        const core::Mm radius     = apply_radius(x, core::arc_radius_of(g, gslot));
        core::Point2 start        = apply(x, core::arc_start_of(g, gslot));
        core::Point2 end          = apply(x, core::arc_end_of(g, gslot));
        if (radius <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Ölçekleme yayı sıfır yarıçapa indiriyor.");
            return false;
        }
        // A reflection turns the plane inside out, so the arc that swept
        // counter-clockwise from start to end now sweeps the other way. Swapping
        // the ends is how that is written down.
        if (reverses(x)) {
            const core::Point2 t = start;
            start                = end;
            end                  = t;
        }
        const core::Point2 pts[4]{centre, core::Point2{centre.x + radius, centre.y}, start, end};
        const core::RingGeometry::RingInput ring{std::span<const core::Point2>(pts, 4),
                                                 core::RingRole::Open, 0};
        auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
        if (!st) {
            ctx.refuse(st.error());
            return false;
        }
        return true;
    }

    // A kind whose payload holds coordinates or angles changes both at once.
    if (kind == core::kArcPolylineKind || kind == core::kHatchKind ||
        kind == core::kBlockReferenceKind || kind == core::kDimensionKind ||
        kind == core::kLeaderKind)
        return transform_payload_kind(ctx, slot, x);

    // A polyline, a face, a spline, an ellipse: every stored vertex is a real
    // corner or a definition point, so every one moves. The ring structure —
    // roles, parts, hole order — is untouched, and so is a payload that holds
    // no coordinate (a spline's knots, an ellipse's sweep), which the geometry
    // carries over.
    //
    // A REFLECTION REVERSES WINDING, so a ring walked counter-clockwise now runs
    // clockwise. A closed ring's vertex order is reversed to put it back, because
    // the sign of a ring's area is the sign of its winding and a face whose
    // exterior wound the wrong way would compute a negative alan (geometry.hpp
    // `ring_area`). An open ring keeps its order (`transformed_rings`).
    std::vector<std::vector<core::Point2>> rings;
    std::vector<core::RingGeometry::RingInput> input;
    transformed_rings(g, gslot, x, rings, input);

    // A CAPTION STAYS A CAPTION (TODOS C-08). Its letters grow with a scale —
    // a baseline made twice as long under letters of the old height was a
    // caption drawn wrong — and after a reflection it is turned about its
    // anchor to read left to right: the renderer never mirrors a glyph, so a
    // baseline reflected to run leftward read upside down.
    //
    // A BLOCK'S PLACEMENT IS THE EXCEPTION: PATLAT promises the pieces look as
    // the reference drew them, and the reference draws a member caption along
    // its placed baseline, whichever way that runs — so it stays so, and its
    // letters are as much bigger as that baseline got, which is the rule the
    // picture follows too (`render/scene.cpp`).
    const bool caption = doc.texts().has(gslot) && rings.size() == 1 && rings[0].size() == 2;
    const bool placed  = x.kind == Xform::Kind::Place;
    if (caption && reverses(x) && !placed) {
        const core::Point2 a = rings[0][0];
        const core::Point2 b = rings[0][1];
        if (b.x < a.x || (b.x == a.x && b.y < a.y)) {
            rings[0][1]     = core::Point2{2 * a.x - b.x, 2 * a.y - b.y};
            input[0].points = rings[0];
        }
    }
    const core::Mm was_height = caption ? doc.texts().height(gslot) : 0;
    const std::string words   = caption ? std::string(doc.texts().text(gslot)) : std::string();
    const core::TextAnchor anchor =
        caption ? doc.texts().anchor(gslot) : core::TextAnchor::BaselineLeft;
    core::Mm height = caption ? apply_height(x, was_height) : 0;
    if (caption && placed && !core::place_uniform(x))
        height = core::caption_height_along(g, gslot, rings[0][0], rings[0][1], was_height);

    auto st = ctx.transaction().set_geometry(slot, input);
    if (!st) {
        ctx.refuse(st.error());
        return false;
    }
    if (caption && height != was_height) {
        if (auto t = ctx.transaction().set_text(slot, words, height, anchor); !t) {
            ctx.refuse(t.error());
            return false;
        }
    }
    return true;
}

/// Makes a NEW entity that is `slot` with `x` applied, and returns its id.
///
/// Everything a user would expect to travel with a copy travels with it: the
/// kind and its payload, the layer, the style, the text, and every attribute
/// cell. What does NOT travel is the key — a copy is a new object and mints its
/// own (model.md R4), which is exactly why an ada/parsel number carried over on
/// a copy must be corrected by the surveyor rather than assumed.
///
/// A COPY IS THE SAME RECORD, THEN THE SAME TRANSFORM. The object is duplicated
/// as it is — its kind, its rings and its payload byte for byte — and the copy
/// is then transformed by `transform_one`, the call a move makes. Before this a
/// copy was rebuilt from its raw vertices as a plain polyline for every kind but
/// the circle and the arc, so a copied spline became the polygon of its control
/// points, an ellipse three stray points and a block reference, a dimension or
/// a hatch lost everything that made it one (TODOS C-08).
core::Result<core::EntityId> clone_one(Context& ctx, core::EntityId slot, const Xform& x,
                                       core::LayerId onto = core::kNoLayer)
{
    const core::Document& doc   = ctx.document();
    const core::RingGeometry& g = doc.geometry();
    const std::uint32_t gslot   = doc.entities().slot[slot];
    const core::KindId kind     = doc.entities().kind[slot];
    const core::LayerId layer   = onto != core::kNoLayer ? onto : doc.entities().layer[slot];

    std::vector<std::vector<core::Point2>> rings;
    std::vector<core::RingGeometry::RingInput> input;
    transformed_rings(g, gslot, Xform{}, rings, input); // the identity: the rings as they are
    const auto payload = g.payload_of(gslot);
    const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());

    auto made = ctx.transaction().add_kind(layer, kind, input, bytes);
    if (!made) return made;
    const core::EntityId fresh = made.value();

    // The style column, when the source did not simply inherit its layer's.
    if (const core::StyleId style = doc.entities().style[slot]; style != core::kByLayerStyle) {
        auto st = ctx.transaction().set_entity_style(fresh, style);
        if (!st) return st.error();
    }

    // The text, which is drawing content and not a label (text_store.hpp).
    if (doc.texts().has(gslot)) {
        auto st = ctx.transaction().set_text(fresh, std::string(doc.texts().text(gslot)),
                                             doc.texts().height(gslot), doc.texts().anchor(gslot),
                                             doc.texts().lines(gslot));
        if (!st) return st.error();
    }

    // And every attribute cell. A copied parsel that lost its ada number would be
    // a worse copy than one that kept it: the number is what the object IS to a
    // surveyor, and correcting one is a smaller job than retyping all of them.
    const core::AttrTable& attrs = doc.attributes();
    for (std::size_t c = 0; c < attrs.columns(); ++c) {
        const core::AttrColumn* col = attrs.column(static_cast<core::AttrId>(c));
        if (col == nullptr) continue;
        auto value = doc.attribute(static_cast<core::AttrId>(c), slot);
        if (!value) continue;
        if (!value.value().present) continue;
        auto st =
            ctx.transaction().set_attribute(static_cast<core::AttrId>(c), fresh, value.value());
        if (!st) return st.error();
    }

    // THEN THE TRANSFORM, by the one function every verb uses. It has refused
    // with its reason when it returns false, and that reason is what goes back
    // — a caller that refused again with a generic "could not transform" wrote
    // over the one sentence that said why.
    if (!transform_one(ctx, fresh, x)) return ctx.session().error();
    return made;
}

/// The entities a transform command works on: the named ones, or the selection.
Task<bool> gather(Context& ctx, std::vector<std::int64_t>& requested,
                  std::vector<core::EntityId>& slots, const char* example)
{

    // The argument, the selection, or ASKED FOR — `want_objects` in
    // `command/context.hpp` is the one place that order lives. Refusing an empty
    // selection, which is what this did, meant a tool-column button could only
    // work if the user had already highlighted something: press the tool first,
    // as every CAD trains, and nothing happened.
    if (!co_await want_objects(ctx, "nesneler", "Nesneleri seçin, sonra Enter", requested, 0,
                               example))
        co_return false;

    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Geçersiz nesne kimliği: " + std::to_string(raw) +
                           ". Kimlikler 1'den başlar.");
            co_return false;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return false;
        }
        slots.push_back(slot);
    }
    co_return true;
}

bool apply_all(Context& ctx, const std::vector<core::EntityId>& slots, const Xform& x)
{
    for (core::EntityId slot : slots)
        if (!transform_one(ctx, slot, x)) return false;
    return true;
}

/// `x` applied to every object — or, with `kopya=evet`, to a COPY of each, the
/// original left where it is: the option every CAD's DÖNDÜR, ÖLÇEKLE and AYNALA
/// offer, so a turned or mirrored duplicate is one command rather than a copy
/// and a move (TODOS C-08). Sets `copied` to whether it copied.
bool apply_or_copy(Context& ctx, const std::vector<core::EntityId>& slots, const Xform& x,
                   bool& copied)
{
    const Value wanted = ctx.argument("kopya");
    copied             = !wanted.empty() && wanted.as_bool();
    if (!copied) return apply_all(ctx, slots, x);
    for (const core::EntityId slot : slots) {
        const auto made = clone_one(ctx, slot, x);
        if (!made) {
            ctx.refuse(made.error());
            return false;
        }
    }
    ctx.record("kopya", Value::boolean(true));
    return true;
}

// ------------------------------------------------------------------ TAŞI ----

Task<void> run_move(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "TAŞI nesneler=1 baslangic=0,0 bitis=10,0"))
        co_return;

    auto from = co_await ctx.point("baslangic", "Taşımanın başlangıç noktası");
    if (!from) co_return;

    // The objects themselves ride under the cursor, offset from the base point
    // — the ghost every CAD shows — so where they will land is seen, not
    // inferred from a line.
    auto to = co_await ctx.point("bitis", "Taşımanın bitiş noktası",
                                 ghost(core::GhostKind::Translate, *from, requested));
    if (!to) co_return;

    // THE SAME CALL THE GHOST MADE. The offset was worked out here and the ghost
    // worked it out again in the canvas; one call means the two cannot differ.
    const Xform x = core::ghost_xform(core::GhostKind::Translate, *from, *to);

    if (!apply_all(ctx, slots, x)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("baslangic", Value::point(*from));
    ctx.record("bitis", Value::point(*to));
    ctx.echo(std::to_string(slots.size()) + " nesne taşındı.");
}

// -------------------------------------------------------------- KOPYALA ----

Task<void> run_copy(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "KOPYALA nesneler=1 baslangic=0,0 bitis=10,0"))
        co_return;

    auto from = co_await ctx.point("baslangic", "Kopyalamanın başlangıç noktası");
    if (!from) co_return;

    // ONE COPY PER POINT, until the right button or Esc ends the run: a row of
    // identical poles or manholes is placed in one command rather than one
    // command per pole. A script gives the same list as `bitis=`; a single
    // point is the one copy it always made.
    std::vector<core::Point2> placed;
    while (auto to = co_await ctx.point(
               "bitis", placed.empty() ? "Kopyanın geleceği nokta" : "Sonraki kopyanın yeri",
               ghost(core::GhostKind::Translate, *from, requested))) {
        const Xform x = core::ghost_xform(core::GhostKind::Translate, *from, *to);

        for (core::EntityId slot : slots) {
            auto made = clone_one(ctx, slot, x);
            if (!made) {
                ctx.refuse(made.error());
                co_return; // the bus rolls the whole transaction back
            }
        }
        placed.push_back(*to);
    }
    if (placed.empty()) co_return; // ESC before a copy was placed: nothing to record

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("baslangic", Value::point(*from));
    ctx.record("bitis", Value::points(placed));
    ctx.echo(std::to_string(slots.size() * placed.size()) + " nesne kopyalandı" +
             (placed.size() > 1 ? " (" + std::to_string(placed.size()) + " yere)." : "."));
}

// ----------------------------------------------------------------- DİZİ ----

Task<void> run_array(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots,
                         "DİZİ nesneler=1 satir=3 sutun=4 satir_aralik=10 "
                         "sutun_aralik=10"))
        co_return;

    const Value mode_arg = ctx.argument("mod");
    const bool polar =
        !mode_arg.empty() && (core::turkish_key_equals(mode_arg.as_text(), "KUTUPSAL") ||
                              core::turkish_key_equals(mode_arg.as_text(), "POLAR"));
    const bool along = !mode_arg.empty() && (core::turkish_key_equals(mode_arg.as_text(), "YOL") ||
                                             core::turkish_key_equals(mode_arg.as_text(), "PATH"));

    std::size_t made = 0;

    if (along) {
        // ALONG A PATH (TODOS C-08): copies set out along a line, an arc or a
        // road's arc polyline — poles along a kerb, trees along an avenue,
        // markers down a chainage — each turned to follow the path unless
        // asked not to. The objects' base point (the path's start unless
        // given) is carried to every station.
        const core::Document& doc = ctx.document();
        core::EntityId path_slot  = core::kNoEntity;
        std::int64_t path_key     = 0;
        if (const Value named = ctx.argument("yol"); !named.empty() && !named.as_ids().empty()) {
            path_key = named.as_ids().front();
            path_slot =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(path_key)));
        } else {
            auto on = co_await ctx.point("yol_nokta", "Dizinin izleyeceği yola tıklayın");
            if (!on) co_return;
            path_slot =
                core::pick_nearest(doc, *on, ctx.session().bus().aid_settings().pick_radius);
            if (path_slot != core::kNoEntity)
                path_key = static_cast<std::int64_t>(core::raw(doc.key_of(path_slot)));
            ctx.record("yol_nokta", Value{});
        }
        // ANY CURVE A COPY CAN WALK: an ellipse and a spline as well — the
        // copies stand at equal lengths along the curve itself (TODOS C-01).
        const auto path = path_slot != core::kNoEntity && doc.alive(path_slot)
                              ? core::path_of(doc, path_slot, core::PathScope::Curves)
                              : std::nullopt;
        if (!path || path->pieces.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Yol bulunamadı: dizi bir çizgi, yay, daire, elips, spline ya da yaylı "
                       "çoklu çizgi boyunca kurulur.");
            co_return;
        }
        const core::Mm length = core::path_length(*path);
        if (length <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument, "Yolun uzunluğu sıfır.");
            co_return;
        }
        const core::PathPlace start = core::path_start(*path);
        core::Point2 base           = core::point_at(*path, start);
        if (const Value t = ctx.argument("taban"); !t.empty()) base = t.as_point();
        bool follow = true;
        if (const Value h = ctx.argument("hizala"); !h.empty()) follow = h.as_bool();

        // THE STATIONS: a count shares the length out — both ends kept on an
        // open path, the seam once round a closed one — and a spacing walks
        // it from the start.
        std::vector<core::Mm> stations;
        if (const Value gap_arg = ctx.argument("aralik"); !gap_arg.empty()) {
            const core::Mm gap =
                core::mm_round(gap_arg.as_number() * static_cast<double>(core::kMmPerMetre));
            if (gap <= 0) {
                ctx.refuse(core::ErrorCode::InvalidArgument, "Aralık sıfırdan büyük olmalı.");
                co_return;
            }
            for (core::Mm at = 0; at <= length; at += gap)
                stations.push_back(at);
        } else {
            auto count = co_await ctx.integer("sayi", "Yol boyunca kaç nesne (özgün dahil)");
            if (!count) co_return;
            if (*count < 2) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Yol boyunca dizi en az iki nesne ister; " + std::to_string(*count) +
                               " istendi.");
                co_return;
            }
            const std::int64_t gaps = path->closed ? *count : *count - 1;
            for (std::int64_t k = 0; k < *count; ++k)
                stations.push_back(
                    core::mm_round(static_cast<double>(length) * static_cast<double>(k) /
                                   static_cast<double>(gaps)));
        }

        const std::int64_t first = core::direction_at(*path, start);
        for (const core::Mm at : stations) {
            const core::PathPlace place = core::place_at_length(*path, at);
            const core::Point2 to       = core::point_at(*path, place);
            const std::int64_t turn     = follow ? core::direction_at(*path, place) - first : 0;
            const bool square           = turn % core::kUDegFullCircle == 0;
            if (to == base && square) continue; // the original's own place
            Xform x;
            if (square) {
                x.kind = Xform::Kind::Translate;
                x.dx   = to.x - base.x;
                x.dy   = to.y - base.y;
            } else {
                x.kind   = Xform::Kind::Align;
                x.base   = base;
                x.axis_b = to;
                x.turn   = core::sin_cos_udeg(turn);
            }
            for (const core::EntityId slot : slots) {
                const auto copy = clone_one(ctx, slot, x);
                if (!copy) {
                    ctx.refuse(copy.error());
                    co_return;
                }
                ++made;
            }
        }

        ctx.record("nesneler", Value::ids(requested));
        ctx.record("mod", Value::text("YOL"));
        ctx.record("yol", Value::ids({path_key}));
        if (!follow) ctx.record("hizala", Value::boolean(false));
        ctx.echo(std::to_string(made) + " kopya yol boyunca dizildi" +
                 (follow ? ", her biri yolun doğrultusuna döndürüldü." : "."));
        co_return;
    }

    if (polar) {

        // A POLAR ARRAY: copies swung about a centre. This is what a manhole ring,
        // a roundabout's radial kerbs or a circular building's columns are.
        auto centre = co_await ctx.point("merkez", "Dizinin merkezi");
        if (!centre) co_return;

        auto count = co_await ctx.integer("sayi", "Toplam kopya sayısı (özgün dahil)");
        if (!count) co_return;

        if (*count < 2) {
            ctx.refuse(core::ErrorCode::InvalidArgument, "Kutupsal dizi en az iki nesne ister; " +
                                                             std::to_string(*count) + " istendi.");
            co_return;
        }

        // The sweep defaults to a full turn, which is what a ring of columns is.
        // Given explicitly it can be any arc — a quarter turn of kerbs, say.
        const Value sweep_arg = ctx.argument("aci");
        const double sweep    = sweep_arg.empty() ? 360.0 : sweep_arg.as_number();

        // A FULL TURN divides by the count, a partial one by the gaps: twelve
        // columns round a circle are 30 degrees apart, but twelve along a quarter
        // turn are 90/11 apart, because the first and last both stay.
        const bool full   = std::abs(std::abs(sweep) - 360.0) < 1e-9;
        const double step = sweep / static_cast<double>(full ? *count : *count - 1);

        for (std::int64_t i = 1; i < *count; ++i) {
            Xform x;
            x.kind = Xform::Kind::Rotate;
            x.base = *centre;
            x.turn = core::sin_cos_udeg(static_cast<core::UDeg>(std::llround(
                step * static_cast<double>(i) * static_cast<double>(core::kUDegPerDegree))));

            for (const core::EntityId slot : slots) {
                const auto copy = clone_one(ctx, slot, x);
                if (!copy) {
                    ctx.refuse(copy.error());
                    co_return;
                }
                ++made;
            }
        }

        ctx.record("nesneler", Value::ids(requested));
        ctx.record("mod", Value::text("KUTUPSAL"));
        ctx.record("merkez", Value::point(*centre));
        ctx.record("sayi", Value::integer(*count));
        if (!sweep_arg.empty()) ctx.record("aci", Value::number(sweep));
    } else {
        // A RECTANGULAR ARRAY: rows and columns, which is a block of identical
        // buildings, a car park's bays or a grid of survey stakes.
        auto rows = co_await ctx.integer("satir", "Satır sayısı");
        if (!rows) co_return;
        auto cols = co_await ctx.integer("sutun", "Sütun sayısı");
        if (!cols) co_return;

        if (*rows < 1 || *cols < 1) {
            ctx.refuse(core::ErrorCode::InvalidArgument, "Satır ve sütun sayısı en az bir olmalı.");
            co_return;
        }
        if (*rows == 1 && *cols == 1) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tek satır ve tek sütun bir dizi değildir; kopya üretilmedi.");
            co_return;
        }

        auto row_gap = co_await ctx.number("satir_aralik", "Satır aralığı (kuzeye, metre)");
        if (!row_gap) co_return;
        auto col_gap = co_await ctx.number("sutun_aralik", "Sütun aralığı (doğuya, metre)");
        if (!col_gap) co_return;

        const core::Mm dy = core::mm_round(*row_gap * static_cast<double>(core::kMmPerMetre));
        const core::Mm dx = core::mm_round(*col_gap * static_cast<double>(core::kMmPerMetre));

        for (std::int64_t r = 0; r < *rows; ++r) {
            for (std::int64_t c = 0; c < *cols; ++c) {
                if (r == 0 && c == 0) continue; // the original stays where it is

                Xform x;
                x.kind = Xform::Kind::Translate;
                x.dx   = dx * c;
                x.dy   = dy * r;

                for (core::EntityId slot : slots) {
                    auto copy = clone_one(ctx, slot, x);
                    if (!copy) {
                        ctx.refuse(copy.error());
                        co_return;
                    }
                    ++made;
                }
            }
        }

        ctx.record("nesneler", Value::ids(requested));
        ctx.record("satir", Value::integer(*rows));
        ctx.record("sutun", Value::integer(*cols));
        ctx.record("satir_aralik", Value::number(*row_gap));
        ctx.record("sutun_aralik", Value::number(*col_gap));
    }

    ctx.echo(std::to_string(made) + " kopya üretildi.");
}

// --------------------------------------------------------------- DÖNDÜR ----

/// Whether the run is BY REFERENCE: `yontem=referans`, or a reference given.
bool by_reference(const Context& ctx)
{
    if (ctx.has_argument("referans")) return true;
    const Value way = ctx.argument("yontem");
    return !way.empty() && core::turkish_key_equals(way.as_text(), "REFERANS");
}

Task<void> run_rotate(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "DÖNDÜR nesneler=1 merkez=0,0 aci=90")) co_return;

    // WHETHER THE ANGLE CAME WITH THE INVOCATION, read before anything is
    // awaited: a branch on whether a value was given, not on which client gave
    // it (command.md P10). A run that was handed `aci` asks nothing more, which
    // keeps every line written before the gesture existed replayable.
    const bool angle_given = ctx.has_argument("aci");
    const bool reference   = by_reference(ctx);

    auto centre = co_await ctx.point("merkez", "Döndürme merkezi");
    if (!centre) co_return;

    // BY REFERENCE (TODOS C-08): a direction on the drawing — typed as an
    // angle, or shown by two points along a building's wall — is turned onto
    // a new one, and the turn is the difference. What a surveyor does to bring
    // a sketch's wall onto the measured bearing without working the angle out.
    std::int64_t from_udeg = 0;
    if (reference) {
        if (ctx.has_argument("referans")) {
            from_udeg = std::llround(ctx.argument("referans").as_number() *
                                     static_cast<double>(core::kUDegPerDegree));
        } else {
            auto a = co_await ctx.point("referans_nokta", "Referans doğrultunun ilk noktası");
            if (!a) co_return;
            auto b = co_await ctx.point(
                "referans_nokta", "Referans doğrultunun ikinci noktası",
                PointOptions{.rubber_band = true, .rubber_origin = *a, .rubber_base = false});
            if (!b) co_return;
            if (*a == *b) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Referans doğrultu tek noktadan geçemez; iki farklı nokta verin.");
                co_return;
            }
            from_udeg = core::atan2_udeg(b->y - a->y, b->x - a->x);
            ctx.record("referans_nokta", Value{});
        }
    }

    double degrees = 0.0;
    if (angle_given) {
        degrees = ctx.argument("aci").as_number() -
                  static_cast<double>(from_udeg) / static_cast<double>(core::kUDegPerDegree);
    } else {
        // POINTED, WITH THE OBJECTS TURNING UNDER THE CURSOR. The angle used to
        // be typed and nothing else was offered, so turning a building onto a
        // measured bearing meant working the number out first and finding out
        // afterwards whether it was the one you wanted. The cursor's direction
        // from the centre IS the angle — degrees counter-clockwise from east,
        // which is what this parameter has always meant — and the ghost turns
        // with it; by reference, less the reference direction.
        auto at = co_await ctx.point(
            "aci_nokta",
            reference ? "Yeni doğrultuyu gösterin" : "Dönme açısı: yeni doğrultuyu gösterin",
            ghost(core::GhostKind::Rotate, *centre, requested, 1, from_udeg));
        if (!at) co_return;
        degrees = static_cast<double>(core::ghost_turn_udeg(*centre, *at) - from_udeg) /
                  static_cast<double>(core::kUDegPerDegree);
        // NOT PART OF THE RECORD: the gesture is HOW the angle was chosen and
        // `aci` is what the angle IS (Article 1.4).
        ctx.record("aci_nokta", Value{});
    }

    Xform x;
    x.kind = Xform::Kind::Rotate;
    x.base = *centre;
    x.turn = core::sin_cos_udeg(
        static_cast<core::UDeg>(std::llround(degrees * static_cast<double>(core::kUDegPerDegree))));

    bool copied = false;
    if (!apply_or_copy(ctx, slots, x, copied)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("merkez", Value::point(*centre));
    // THE TURN, RESOLVED: a reference is how it was found, not what it is.
    ctx.record("aci", Value::number(degrees));
    ctx.record("referans", Value{});
    ctx.record("yontem", Value{});
    ctx.echo(std::to_string(slots.size()) +
             (copied ? " nesnenin döndürülmüş kopyası çizildi." : " nesne döndürüldü."));
}

// -------------------------------------------------------------- ÖLÇEKLE ----

Task<void> run_scale(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2"))
        co_return;

    const bool factor_given = ctx.has_argument("carpan");
    const bool reference    = by_reference(ctx);

    auto centre = co_await ctx.point("merkez", "Ölçekleme merkezi");
    if (!centre) co_return;

    // BY REFERENCE (TODOS C-08): a length on the drawing — typed, or shown by
    // two points — becomes a new one, and the factor is their ratio. What a
    // scanned sketch needs when one of its sides was measured in the field.
    core::Mm from_length = 0;
    if (reference) {
        if (ctx.has_argument("referans")) {
            from_length = core::mm_round(ctx.argument("referans").as_number() *
                                         static_cast<double>(core::kMmPerMetre));
        } else {
            auto a = co_await ctx.point("referans_nokta", "Referans uzunluğun ilk noktası");
            if (!a) co_return;
            auto b = co_await ctx.point(
                "referans_nokta", "Referans uzunluğun ikinci noktası",
                PointOptions{.rubber_band = true, .rubber_origin = *a, .rubber_base = false});
            if (!b) co_return;
            from_length = core::segment_length(*a, *b);
            ctx.record("referans_nokta", Value{});
        }
        if (from_length <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Referans uzunluk sıfırdan büyük olmalı; iki farklı nokta verin.");
            co_return;
        }
    }

    std::optional<double> factor;
    if (reference) {
        if (ctx.has_argument("yeni")) {
            factor = ctx.argument("yeni").as_number() * static_cast<double>(core::kMmPerMetre) /
                     static_cast<double>(from_length);
        } else {
            // THE NEW LENGTH SHOWN: the cursor's distance from the centre, as
            // every CAD reads a pointed new length, and the ghost that size.
            auto at = co_await ctx.point(
                "carpan_nokta", "Yeni uzunluk: merkezden uzaklığı gösterin",
                ghost(core::GhostKind::Scale, *centre, requested, 1, 0, from_length));
            if (!at) co_return;
            factor = static_cast<double>(core::segment_length(*centre, *at)) /
                     static_cast<double>(from_length);
            ctx.record("carpan_nokta", Value{});
        }
    } else if (factor_given) {
        factor = ctx.argument("carpan").as_number();
    } else {
        // POINTED, WITH THE OBJECTS GROWING UNDER THE CURSOR. The factor is the
        // cursor's distance from the centre in METRES — two metres out is twice
        // the size — which is how every CAD reads a dragged scale, and the ghost
        // is the size it will be.
        auto at = co_await ctx.point("carpan_nokta", "Ölçek çarpanı: merkezden uzaklık (m)",
                                     ghost(core::GhostKind::Scale, *centre, requested));
        if (!at) co_return;
        factor = core::ghost_factor(*centre, *at);
        // As above: the gesture is not the answer, the factor is.
        ctx.record("carpan_nokta", Value{});
    }

    // A SECOND FACTOR, UP (TODOS C-08): the scale across is `carpan`, the one
    // up `carpan_y`. A circle stretched so becomes the ellipse it is; what a
    // kind cannot hold unevenly is refused by name.
    double up = *factor;
    if (const Value v = ctx.argument("carpan_y"); !v.empty()) up = v.as_number();

    if (*factor <= 0.0 || up <= 0.0) {
        // A negative factor is refused rather than quietly becoming a half turn:
        // "scale by minus one" and "mirror" are different intentions, and a user
        // who typed the wrong sign should be told rather than obeyed.
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Ölçek çarpanı sıfırdan büyük olmalı; aynalamak için AYNALA kullanın.");
        co_return;
    }

    Xform x;
    x.kind   = up == *factor ? Xform::Kind::Scale : Xform::Kind::Stretch;
    x.base   = *centre;
    x.factor = *factor;
    if (x.kind == Xform::Kind::Stretch) x.factor_y = up;

    bool copied = false;
    if (!apply_or_copy(ctx, slots, x, copied)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("merkez", Value::point(*centre));
    ctx.record("carpan", Value::number(*factor));
    if (x.kind == Xform::Kind::Stretch) ctx.record("carpan_y", Value::number(up));
    ctx.record("referans", Value{});
    ctx.record("yeni", Value{});
    ctx.record("yontem", Value{});
    ctx.echo(std::to_string(slots.size()) +
             (copied ? " nesnenin ölçeklenmiş kopyası çizildi." : " nesne ölçeklendi.") +
             (x.kind == Xform::Kind::Stretch
                  ? " Eşit olmayan ölçekte yazıların yüksekliği ve ölçülerin ok boyu korundu."
                  : ""));
}

// --------------------------------------------------------------- AYNALA ----

Task<void> run_mirror(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "AYNALA nesneler=1 baslangic=0,0 bitis=0,10"))
        co_return;

    auto a = co_await ctx.point("baslangic", "Ayna ekseninin ilk noktası");
    if (!a) co_return;

    // MIRRORED WHILE IT IS AIMED. The axis used to be drawn as a plain line, so
    // the one thing the command is about — which way round the objects end up —
    // was invisible until it had happened.
    auto b = co_await ctx.point("bitis", "Ayna ekseninin ikinci noktası",
                                ghost(core::GhostKind::Mirror, *a, requested));
    if (!b) co_return;

    if (a->x == b->x && a->y == b->y) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Ayna ekseni tek noktadan geçemez; iki farklı nokta verin.");
        co_return;
    }

    const Xform x = core::ghost_xform(core::GhostKind::Mirror, *a, *b);

    bool copied = false;
    if (!apply_or_copy(ctx, slots, x, copied)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("baslangic", Value::point(*a));
    ctx.record("bitis", Value::point(*b));
    ctx.echo(std::to_string(slots.size()) +
             (copied ? " nesnenin aynalanmış kopyası çizildi." : " nesne aynalandı."));
}

} // namespace

bool transform_entity(Context& ctx, core::EntityId slot, const core::Xform& x)
{
    return transform_one(ctx, slot, x);
}

core::Result<core::EntityId> clone_entity(Context& ctx, core::EntityId slot, const core::Xform& x,
                                          core::LayerId onto)
{
    return clone_one(ctx, slot, x, onto);
}

KENTOS_COMMAND(move)
{
    return CommandSpec{
        .id       = "core.move",
        .names    = {"TAŞI", "TASI", "MOVE", "TŞ"},
        .title    = "Taşı",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim"}
                    .en("objects"),
                Param::point("baslangic", "Taşımanın başlangıç noktası").en("start"),
                Param::point("bitis", "Taşımanın bitiş noktası").en("end"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri iki nokta arasındaki kadar taşır.",
        .run     = &run_move,
    };
}

KENTOS_COMMAND(copy_objects)
{
    return CommandSpec{
        .id       = "core.copy",
        .names    = {"KOPYALA", "COPY", "KP"},
        .title    = "Kopyala",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim"}
                    .en("objects"),
                Param::point("baslangic", "Kopyalamanın başlangıç noktası").en("start"),
                Param::points("bitis", Arity::at_least(1),
                              "Kopyaların geleceği noktalar; her nokta bir kopya")
                    .en("end"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya "
                   "kadar öteleyerek koyar.",
        .run     = &run_copy,
    };
}

KENTOS_COMMAND(array_objects)
{
    return CommandSpec{
        .id       = "core.array",
        .names    = {"DİZİ", "DIZI", "ARRAY", "DZ"},
        .title    = "Dizi",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Dizilecek nesnelerin kimlikleri; yoksa etkin seçim"}
                    .en("objects"),
                Param::text("mod", Arity::optional(),
                            "KUTUPSAL için kutupsal dizi, YOL için yol boyunca dizi; verilmezse "
                            "satır/sütun dizisi")
                    .en("mode"),
                Param::integer("satir", Arity::optional(), "Satır sayısı (dikdörtgen dizi)")
                    .en("rows"),
                Param::integer("sutun", Arity::optional(), "Sütun sayısı (dikdörtgen dizi)")
                    .en("columns"),
                Param::number("satir_aralik", Arity::optional(),
                              "Satır aralığı, metre; kuzeye artı")
                    .en("row_spacing"),
                Param::number("sutun_aralik", Arity::optional(),
                              "Sütun aralığı, metre; doğuya artı")
                    .en("column_spacing"),
                Param{"merkez", ParamKind::Point, Arity::optional(),
                      "Dizinin merkezi (kutupsal dizi)"}
                    .en("center"),
                Param::integer("sayi", Arity::optional(),
                               "Toplam kopya sayısı, özgün dahil (kutupsal ve yol boyunca dizi)")
                    .en("count"),
                Param::number("aci", Arity::optional(),
                              "Süpürülecek toplam açı, derece; verilmezse tam tur")
                    .en("angle"),
                Param{"yol", ParamKind::Selection, Arity::optional(),
                      "mod=yol için dizinin izleyeceği yol: çizgi, yay, daire ya da yaylı çoklu "
                      "çizgi"}
                    .en("path"),
                Param{"yol_nokta", ParamKind::Point, Arity::optional(),
                      "Yolu gösteren nokta; yol verilmişse sorulmaz"}
                    .en("path_point"),
                Param::number("aralik", Arity::optional(),
                              "mod=yol için kopyalar arası uzaklık, metre; verilmezse sayi")
                    .en("spacing"),
                Param::boolean("hizala", Arity::optional(),
                               "mod=yol için kopyalar yolun doğrultusuna döndürülsün mü; "
                               "varsayılan evet")
                    .en("follow"),
                Param{"taban", ParamKind::Point, Arity::optional(),
                      "mod=yol için nesnelerin yola taşınan taban noktası; varsayılan yolun "
                      "başı"}
                    .en("base_point"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri satır/sütun, bir merkez etrafında ya da bir yol boyunca "
                   "çoğaltır.",
        .run     = &run_array,
    };
}

KENTOS_COMMAND(rotate)
{
    return CommandSpec{
        .id       = "core.rotate",
        .names    = {"DÖNDÜR", "DONDUR", "ROTATE", "DÖN"},
        .title    = "Döndür",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim"}
                    .en("objects"),
                Param::point("merkez", "Döndürme merkezi").en("center"),
                Param::number("aci", Arity::optional(),
                              "Dönme açısı, derece; artı yön saat yönünün tersi. Verilmezse "
                              "yeni doğrultu gösterilir")
                    .en("angle"),
                Param::points("aci_nokta", Arity::optional(),
                              "Dönme açısının gösterildiği nokta; aci verilmişse sorulmaz")
                    .en("angle_point"),
                Param::choice("yontem", Arity::optional(), {"referans"},
                              "referans: bir doğrultu yenisine döndürülür; referans doğrultu "
                              "iki noktayla gösterilir")
                    .en("method"),
                Param::number("referans", Arity::optional(),
                              "Referans doğrultunun açısı, derece; aci onun yeni açısıdır")
                    .en("reference"),
                Param::points("referans_nokta", Arity{0, 2},
                              "Referans doğrultuyu gösteren iki nokta")
                    .en("reference_point"),
                Param::boolean("kopya", Arity::optional(),
                               "evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün "
                               "yerinde kalır")
                    .en("copy"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri bir merkez etrafında döndürür; açı verilir, gösterilir ya "
                   "da bir referans doğrultudan bulunur.",
        .run = &run_rotate,
    };
}

KENTOS_COMMAND(scale)
{
    return CommandSpec{
        .id       = "core.scale",
        .names    = {"ÖLÇEKLE", "OLCEKLE", "SCALE", "ÖLÇEK", "OLCEK"},
        .title    = "Ölçekle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim"}
                    .en("objects"),
                Param::point("merkez", "Ölçekleme merkezi; bu nokta yerinde kalır").en("center"),
                Param::number("carpan", Arity::optional(),
                              "Ölçek çarpanı; sıfırdan büyük. Verilmezse merkezden uzaklık "
                              "gösterilir")
                    .en("factor"),
                Param::points("carpan_nokta", Arity::optional(),
                              "Çarpanın gösterildiği nokta; carpan verilmişse sorulmaz")
                    .en("factor_point"),
                Param::number("carpan_y", Arity::optional(),
                              "Yukarı yöndeki çarpan; verilirse carpan yalnız sağa yöndeki "
                              "çarpandır ve daire elips olur")
                    .en("factor_y"),
                Param::choice("yontem", Arity::optional(), {"referans"},
                              "referans: bir uzunluk yenisine ölçeklenir; referans uzunluk iki "
                              "noktayla gösterilir")
                    .en("method"),
                Param::number("referans", Arity::optional(),
                              "Referans uzunluk, metre; yeni onun olacağı uzunluktur")
                    .en("reference"),
                Param::number("yeni", Arity::optional(), "Referans uzunluğun yeni değeri, metre")
                    .en("new_length"),
                Param::points("referans_nokta", Arity{0, 2}, "Referans uzunluğu gösteren iki nokta")
                    .en("reference_point"),
                Param::boolean("kopya", Arity::optional(),
                               "evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün "
                               "yerinde kalır")
                    .en("copy"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri bir merkeze göre büyütür ya da küçültür; iki çarpanla eşit "
                   "olmayan ölçek, referans uzunlukla ölçek.",
        .run = &run_scale,
    };
}

KENTOS_COMMAND(mirror)
{
    return CommandSpec{
        .id       = "core.mirror",
        .names    = {"AYNALA", "MIRROR", "AYN"},
        .title    = "Aynala",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim"}
                    .en("objects"),
                Param::point("baslangic", "Ayna ekseninin ilk noktası").en("start"),
                Param::point("bitis", "Ayna ekseninin ikinci noktası").en("end"),
                Param::boolean("kopya", Arity::optional(),
                               "evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün "
                               "yerinde kalır")
                    .en("copy"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri iki noktadan geçen eksende aynalar.",
        .run     = &run_mirror,
    };
}

} // namespace kentos::command
