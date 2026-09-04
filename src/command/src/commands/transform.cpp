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

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/transform.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// What is being done to the coordinates. A description rather than a function,
/// because a curve has to ask questions a `Point2 -> Point2` map cannot answer:
/// how its radius changes, and whether its sweep is reversed.
struct Xform
{
    enum class Kind : std::uint8_t { Translate, Rotate, Scale, Mirror };

    Kind kind{Kind::Translate};
    core::Mm dx{0};
    core::Mm dy{0};
    core::Point2 base{};
    core::Point2 axis_b{};
    core::SinCos turn{};
    double factor{1.0};
};

core::Point2 apply(const Xform& x, core::Point2 p)
{
    switch (x.kind) {
    case Xform::Kind::Translate: return core::translated(p, x.dx, x.dy);
    case Xform::Kind::Rotate: return core::rotated_about(p, x.base, x.turn);
    case Xform::Kind::Scale: return core::scaled_about(p, x.base, x.factor);
    case Xform::Kind::Mirror: return core::mirrored_in_line(p, x.base, x.axis_b);
    }
    return p;
}

/// How a curve's radius changes. Only scaling touches it: moving, turning and
/// mirroring a circle leave a circle of exactly the same size, and recomputing
/// the radius from two transformed points would let it drift by a millimetre
/// every time the object was moved.
core::Mm apply_radius(const Xform& x, core::Mm r)
{
    if (x.kind != Xform::Kind::Scale) return r;
    return core::mm_round(static_cast<double>(r) * x.factor);
}

/// Whether the transform turns the plane inside out, so a counter-clockwise sweep
/// becomes a clockwise one. An arc's two ends are then given the other way round,
/// which is how `core.arc` spells a reversed sweep (core/arc.hpp).
bool reverses(const Xform& x)
{
    return x.kind == Xform::Kind::Mirror;
}

/// Applies `x` to one entity, whatever kind it is. Returns false having echoed
/// why, so a caller can stop the whole command rather than half-apply it.
bool transform_one(Context& ctx, core::EntityId slot, const Xform& x)
{
    const core::Document& doc   = ctx.document();
    const core::RingGeometry& g = doc.geometry();
    const std::uint32_t gslot   = doc.entities().slot[slot];
    const core::KindId kind     = doc.entities().kind[slot];

    if (kind == core::kCircleKind) {
        const core::Point2 centre = apply(x, core::circle_centre_of(g, gslot));
        const core::Mm radius     = apply_radius(x, core::circle_radius_of(g, gslot));
        if (radius <= 0) {
            ctx.echo("Ölçekleme daireyi sıfır yarıçapa indiriyor.");
            return false;
        }
        const core::Point2 pts[2]{centre, core::Point2{centre.x + radius, centre.y}};
        const core::RingGeometry::RingInput ring{std::span<const core::Point2>(pts, 2),
                                                 core::RingRole::Open, 0};
        auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
        if (!st) {
            ctx.echo(st.error().message);
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
            ctx.echo("Ölçekleme yayı sıfır yarıçapa indiriyor.");
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
            ctx.echo(st.error().message);
            return false;
        }
        return true;
    }

    // A polyline or a face: every stored vertex is a real corner, so every one
    // moves. The ring structure — roles, parts, hole order — is untouched.
    const core::RingSpan span = g.rings_of(gslot);
    std::vector<std::vector<core::Point2>> rings;
    std::vector<core::RingGeometry::RingInput> input;
    rings.reserve(span.count);
    input.reserve(span.count);

    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = g.ring_xs(r);
        const auto ys = g.ring_ys(r);
        std::vector<core::Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            pts.push_back(apply(x, core::Point2{xs[v], ys[v]}));
        rings.push_back(std::move(pts));
    }

    // A REFLECTION REVERSES WINDING, so a ring walked counter-clockwise now runs
    // clockwise. The vertex order is reversed to put it back, because the sign of
    // a ring's area is the sign of its winding and a face whose exterior wound the
    // wrong way would compute a negative alan (geometry.hpp `ring_area`).
    if (reverses(x))
        for (auto& pts : rings)
            std::reverse(pts.begin(), pts.end());

    for (std::size_t i = 0; i < rings.size(); ++i)
        input.push_back(core::RingGeometry::RingInput{rings[i], g.ring_role[span.first + i],
                                                      g.ring_part[span.first + i]});

    auto st = ctx.transaction().set_geometry(slot, input);
    if (!st) {
        ctx.echo(st.error().message);
        return false;
    }
    return true;
}

/// Makes a NEW entity that is `slot` with `x` applied, and returns its id.
///
/// Everything a user would expect to travel with a copy travels with it: the
/// layer, the style, the text, and every attribute cell. What does NOT travel is
/// the key — a copy is a new object and mints its own (model.md R4), which is
/// exactly why an ada/parsel number carried over on a copy must be corrected by
/// the surveyor rather than assumed.
core::Result<core::EntityId> clone_one(Context& ctx, core::EntityId slot, const Xform& x)
{
    const core::Document& doc   = ctx.document();
    const core::RingGeometry& g = doc.geometry();
    const std::uint32_t gslot   = doc.entities().slot[slot];
    const core::KindId kind     = doc.entities().kind[slot];
    const core::LayerId layer   = doc.entities().layer[slot];

    core::Result<core::EntityId> made = core::err(core::ErrorCode::Internal, "");

    if (kind == core::kCircleKind) {
        const core::Point2 centre = apply(x, core::circle_centre_of(g, gslot));
        const core::Mm radius     = apply_radius(x, core::circle_radius_of(g, gslot));
        made                      = ctx.transaction().add_circle(layer, centre, radius);
    } else if (kind == core::kArcKind) {
        const core::Point2 centre = apply(x, core::arc_centre_of(g, gslot));
        const core::Mm radius     = apply_radius(x, core::arc_radius_of(g, gslot));
        core::Point2 start        = apply(x, core::arc_start_of(g, gslot));
        core::Point2 end          = apply(x, core::arc_end_of(g, gslot));
        if (reverses(x)) {
            const core::Point2 t = start;
            start                = end;
            end                  = t;
        }
        made = ctx.transaction().add_arc(layer, centre, radius, start, end);
    } else {
        const core::RingSpan span = g.rings_of(gslot);
        std::vector<std::vector<core::Point2>> rings;
        std::vector<core::RingGeometry::RingInput> input;

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = g.ring_xs(r);
            const auto ys = g.ring_ys(r);
            std::vector<core::Point2> pts;
            pts.reserve(xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v)
                pts.push_back(apply(x, core::Point2{xs[v], ys[v]}));
            rings.push_back(std::move(pts));
        }
        if (reverses(x))
            for (auto& pts : rings)
                std::reverse(pts.begin(), pts.end());

        for (std::size_t i = 0; i < rings.size(); ++i)
            input.push_back(core::RingGeometry::RingInput{rings[i], g.ring_role[span.first + i],
                                                          g.ring_part[span.first + i]});
        made = ctx.transaction().add_area(layer, input);
    }

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
                                             doc.texts().height(gslot), doc.texts().anchor(gslot));
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
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            co_return false;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
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

// ------------------------------------------------------------------ TAŞI ----

Task<void> run_move(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "TAŞI nesneler=1 baslangic=0,0 bitis=10,0"))
        co_return;

    auto from = co_await ctx.point("baslangic", "Taşımanın başlangıç noktası");
    if (!from) co_return;

    auto to = co_await ctx.point("bitis", "Taşımanın bitiş noktası",
                                 PointOptions{.rubber_band = true, .rubber_origin = *from});
    if (!to) co_return;

    Xform x;
    x.kind = Xform::Kind::Translate;
    x.dx   = to->x - from->x;
    x.dy   = to->y - from->y;

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

    auto to = co_await ctx.point("bitis", "Kopyanın geleceği nokta",
                                 PointOptions{.rubber_band = true, .rubber_origin = *from});
    if (!to) co_return;

    Xform x;
    x.kind = Xform::Kind::Translate;
    x.dx   = to->x - from->x;
    x.dy   = to->y - from->y;

    for (core::EntityId slot : slots) {
        auto made = clone_one(ctx, slot, x);
        if (!made) {
            ctx.echo(made.error().message);
            co_return; // the bus rolls the whole transaction back
        }
    }

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("baslangic", Value::point(*from));
    ctx.record("bitis", Value::point(*to));
    ctx.echo(std::to_string(slots.size()) + " nesne kopyalandı.");
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

    std::size_t made = 0;

    if (polar) {
        // A POLAR ARRAY: copies swung about a centre. This is what a manhole ring,
        // a roundabout's radial kerbs or a circular building's columns are.
        auto centre = co_await ctx.point("merkez", "Dizinin merkezi");
        if (!centre) co_return;

        auto count = co_await ctx.integer("sayi", "Toplam kopya sayısı (özgün dahil)");
        if (!count) co_return;

        if (*count < 2) {
            ctx.echo("Kutupsal dizi en az iki nesne ister; " + std::to_string(*count) +
                     " istendi.");
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

            for (core::EntityId slot : slots) {
                auto copy = clone_one(ctx, slot, x);
                if (!copy) {
                    ctx.echo(copy.error().message);
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
            ctx.echo("Satır ve sütun sayısı en az bir olmalı.");
            co_return;
        }
        if (*rows == 1 && *cols == 1) {
            ctx.echo("Tek satır ve tek sütun bir dizi değildir; kopya üretilmedi.");
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
                        ctx.echo(copy.error().message);
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

Task<void> run_rotate(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "DÖNDÜR nesneler=1 merkez=0,0 aci=90")) co_return;

    auto centre = co_await ctx.point("merkez", "Döndürme merkezi");
    if (!centre) co_return;

    auto degrees = co_await ctx.number("aci", "Dönme açısı (derece, saat yönünün tersine)");
    if (!degrees) co_return;

    Xform x;
    x.kind = Xform::Kind::Rotate;
    x.base = *centre;
    x.turn = core::sin_cos_udeg(static_cast<core::UDeg>(
        std::llround(*degrees * static_cast<double>(core::kUDegPerDegree))));

    if (!apply_all(ctx, slots, x)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("merkez", Value::point(*centre));
    ctx.record("aci", Value::number(*degrees));
    ctx.echo(std::to_string(slots.size()) + " nesne döndürüldü.");
}

// -------------------------------------------------------------- ÖLÇEKLE ----

Task<void> run_scale(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2"))
        co_return;

    auto centre = co_await ctx.point("merkez", "Ölçekleme merkezi");
    if (!centre) co_return;

    auto factor = co_await ctx.number("carpan", "Ölçek çarpanı");
    if (!factor) co_return;

    if (*factor <= 0.0) {
        // A negative factor is refused rather than quietly becoming a half turn:
        // "scale by minus one" and "mirror" are different intentions, and a user
        // who typed the wrong sign should be told rather than obeyed.
        ctx.echo("Ölçek çarpanı sıfırdan büyük olmalı; aynalamak için AYNALA kullanın.");
        co_return;
    }

    Xform x;
    x.kind   = Xform::Kind::Scale;
    x.base   = *centre;
    x.factor = *factor;

    if (!apply_all(ctx, slots, x)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("merkez", Value::point(*centre));
    ctx.record("carpan", Value::number(*factor));
    ctx.echo(std::to_string(slots.size()) + " nesne ölçeklendi.");
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

    auto b = co_await ctx.point("bitis", "Ayna ekseninin ikinci noktası",
                                PointOptions{.rubber_band = true, .rubber_origin = *a});
    if (!b) co_return;

    if (a->x == b->x && a->y == b->y) {
        ctx.echo("Ayna ekseni tek noktadan geçemez; iki farklı nokta verin.");
        co_return;
    }

    Xform x;
    x.kind   = Xform::Kind::Mirror;
    x.base   = *a;
    x.axis_b = *b;

    if (!apply_all(ctx, slots, x)) co_return;

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("baslangic", Value::point(*a));
    ctx.record("bitis", Value::point(*b));
    ctx.echo(std::to_string(slots.size()) + " nesne aynalandı.");
}

} // namespace

KENTOS_COMMAND(move)
{
    return CommandSpec{
        .id       = "core.move",
        .names    = {"TAŞI", "TASI", "MOVE", "TŞ"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::point("baslangic", "Taşımanın başlangıç noktası"),
                Param::point("bitis", "Taşımanın bitiş noktası"),
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
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::point("baslangic", "Kopyalamanın başlangıç noktası"),
                Param::point("bitis", "Kopyanın geleceği nokta"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesnelerin kopyasını iki nokta arasındaki kadar öteye koyar.",
        .run     = &run_copy,
    };
}

KENTOS_COMMAND(array_objects)
{
    return CommandSpec{
        .id       = "core.array",
        .names    = {"DİZİ", "DIZI", "ARRAY", "DZ"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Dizilecek nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::text("mod", Arity::optional(),
                            "KUTUPSAL için kutupsal dizi; verilmezse satır/sütun dizisi"),
                Param::integer("satir", Arity::optional(), "Satır sayısı (dikdörtgen dizi)"),
                Param::integer("sutun", Arity::optional(), "Sütun sayısı (dikdörtgen dizi)"),
                Param::number("satir_aralik", Arity::optional(),
                              "Satır aralığı, metre; kuzeye artı"),
                Param::number("sutun_aralik", Arity::optional(),
                              "Sütun aralığı, metre; doğuya artı"),
                Param{"merkez", ParamKind::Point, Arity::optional(),
                      "Dizinin merkezi (kutupsal dizi)"},
                Param::integer("sayi", Arity::optional(),
                               "Toplam kopya sayısı, özgün dahil (kutupsal dizi)"),
                Param::number("aci", Arity::optional(),
                              "Süpürülecek toplam açı, derece; verilmezse tam tur"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır.",
        .run     = &run_array,
    };
}

KENTOS_COMMAND(rotate)
{
    return CommandSpec{
        .id       = "core.rotate",
        .names    = {"DÖNDÜR", "DONDUR", "ROTATE", "DÖN"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::point("merkez", "Döndürme merkezi"),
                Param::number("aci", Arity::exactly(1),
                              "Dönme açısı, derece; artı yön saat yönünün tersi"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri bir merkez etrafında döndürür.",
        .run     = &run_rotate,
    };
}

KENTOS_COMMAND(scale)
{
    return CommandSpec{
        .id       = "core.scale",
        .names    = {"ÖLÇEKLE", "OLCEKLE", "SCALE", "ÖLÇEK", "OLCEK"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::point("merkez", "Ölçekleme merkezi; bu nokta yerinde kalır"),
                Param::number("carpan", Arity::exactly(1), "Ölçek çarpanı; sıfırdan büyük"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri bir merkeze göre büyütür ya da küçültür.",
        .run     = &run_scale,
    };
}

KENTOS_COMMAND(mirror)
{
    return CommandSpec{
        .id       = "core.mirror",
        .names    = {"AYNALA", "MIRROR", "AYN"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::point("baslangic", "Ayna ekseninin ilk noktası"),
                Param::point("bitis", "Ayna ekseninin ikinci noktası"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri iki noktadan geçen eksende aynalar.",
        .run     = &run_mirror,
    };
}

} // namespace kentos::command
