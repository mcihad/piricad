// SPDX-License-Identifier: GPL-3.0-or-later
// core.chamfer (PAH), core.fillet (YUVARLA)
//
// The two ways a corner stops being a corner. A kerb return, a building's cut
// corner at a junction, a road curve where two alignments meet: all of them are
// one of these two operations on a vertex.
//
// BOTH ARE EXACT WHERE THEY CAN BE. The tangent points are computed from unit
// vectors built with `sqrt` — correctly rounded by IEEE-754 — and never from
// `atan2` and a trigonometric call, which are not required to agree between
// platforms (§7.3). The half-angle identities below exist for that reason and not
// to save a call.
//
// A FILLET'S ARC IS A SEPARATE OBJECT, because a polyline in this model holds
// vertices and not bulges (model.md R9-R12). That is honest rather than
// convenient: the arc is a `core.arc` with a real centre and a real radius, so its
// length and its geometry are exact — a bulge flattened into the polyline would
// be a curve stored as its own approximation.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/geometry.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

struct Unit
{
    double x{0.0};
    double y{0.0};
};

Unit unit_from(core::Point2 from, core::Point2 to)
{
    // Metres first: the square of a TM3 coordinate difference in millimetres
    // leaves the 53-bit mantissa long before it leaves int64 (core.md R3).
    const double dx  = core::mm_to_metres(to.x - from.x);
    const double dy  = core::mm_to_metres(to.y - from.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) return Unit{};
    return Unit{dx / len, dy / len};
}

/// The polyline behind one id, and the vertex the user pointed at.
bool corner_of(Context& ctx, const Value& given, core::EntityId& slot,
               std::vector<core::Point2>& pts, std::int64_t& id)
{
    const core::Document& doc = ctx.document();

    id = given.kind() == Value::Kind::IdList
             ? (given.as_ids().size() == 1 ? given.as_ids()[0] : 0)
             : given.as_int();

    if (id <= 0) {
        ctx.echo("Geçersiz nesne kimliği: " + std::to_string(id) + ". Kimlikler 1'den başlar.");
        return false;
    }
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    slot           = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.echo("Nesne " + std::to_string(id) +
                 " bir eğri ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.");
        return false;
    }

    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1) {
        ctx.echo("Nesne " + std::to_string(id) +
                 " çok halkalı; köşe işlemleri tek halkalı nesnelerde çalışır.");
        return false;
    }

    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    pts.clear();
    pts.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v) pts.push_back(core::Point2{xs[v], ys[v]});
    return true;
}

/// The index of the vertex nearest `probe`, and whether it HAS two edges — a
/// corner needs one on each side, which the ends of an open line do not have.
bool pick_corner(const std::vector<core::Point2>& pts, bool closed, core::Point2 probe,
                 std::size_t& at)
{
    if (pts.size() < 3) return false;

    double best  = -1.0;
    std::size_t k = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const double d = core::distance_squared(pts[i], probe);
        if (best < 0.0 || d < best) {
            best = d;
            k    = i;
        }
    }

    // An open line's first and last vertices are ENDS, not corners: there is only
    // one edge at them and nothing to cut across.
    if (!closed && (k == 0 || k + 1 == pts.size())) return false;

    at = k;
    return true;
}

core::RingRole role_of(Context& ctx, core::EntityId slot)
{
    const core::RingSpan span = ctx.document().geometry().rings_of(
        ctx.document().entities().slot[slot]);
    return ctx.document().geometry().ring_role[span.first];
}

/// The two tangent lengths a corner cut needs, plus the geometry around it.
struct Corner
{
    core::Point2 v{};   ///< the vertex itself
    Unit d1{};          ///< unit direction from the vertex toward the previous point
    Unit d2{};          ///< unit direction from the vertex toward the next point
    double cos_theta{}; ///< cosine of the angle at the vertex
    double sin_theta{}; ///< its sine, always positive: a magnitude
    double cross{};     ///< signed, so the turn direction is known
    core::Mm edge1{};   ///< how long the two edges are, in millimetres
    core::Mm edge2{};
};

bool measure_corner(const std::vector<core::Point2>& pts, std::size_t at, Corner& c)
{
    const std::size_t prev = at == 0 ? pts.size() - 1 : at - 1;
    const std::size_t next = at + 1 == pts.size() ? 0 : at + 1;

    c.v  = pts[at];
    c.d1 = unit_from(c.v, pts[prev]);
    c.d2 = unit_from(c.v, pts[next]);

    c.cos_theta = c.d1.x * c.d2.x + c.d1.y * c.d2.y;
    c.cross     = c.d1.x * c.d2.y - c.d1.y * c.d2.x;
    c.sin_theta = std::abs(c.cross);

    // Collinear edges have no corner to cut, and a doubled-back edge has no
    // inside. Both are refused rather than divided by zero.
    if (c.sin_theta < 1e-12) return false;

    const auto len = [](core::Point2 a, core::Point2 b) {
        const double dx = core::mm_to_metres(b.x - a.x);
        const double dy = core::mm_to_metres(b.y - a.y);
        return core::mm_round(std::sqrt(dx * dx + dy * dy) *
                              static_cast<double>(core::kMmPerMetre));
    };
    c.edge1 = len(c.v, pts[prev]);
    c.edge2 = len(c.v, pts[next]);
    return true;
}

core::Point2 along(core::Point2 v, Unit d, core::Mm distance)
{
    const double m = core::mm_to_metres(distance);
    return core::Point2{v.x + core::mm_round(d.x * m * static_cast<double>(core::kMmPerMetre)),
                        v.y + core::mm_round(d.y * m * static_cast<double>(core::kMmPerMetre))};
}

Task<void> run_corner(Context& ctx, bool fillet)
{
    const char* verb = fillet ? "YUVARLA" : "PAH";

    const Value given = ctx.argument("nesne");
    if (given.empty()) {
        ctx.echo(std::string(verb) + " için nesne belirtilmedi. Örnek: " + verb +
                 " nesne=1 nokta=10,10 " + (fillet ? "yaricap=3" : "mesafe=3"));
        co_return;
    }

    core::EntityId slot = core::kNoEntity;
    std::vector<core::Point2> pts;
    std::int64_t id = 0;
    if (!corner_of(ctx, given, slot, pts, id)) co_return;

    const bool closed = role_of(ctx, slot) != core::RingRole::Open;

    auto at_pt = co_await ctx.point("nokta", "İşlem yapılacak köşe");
    if (!at_pt) co_return;

    std::size_t at = 0;
    if (!pick_corner(pts, closed, *at_pt, at)) {
        ctx.echo("Burada iki kenarın buluştuğu bir köşe yok. Açık bir çizginin uçları köşe "
                 "değildir; iki kenarın buluştuğu bir noktayı gösterin.");
        co_return;
    }

    Corner c;
    if (!measure_corner(pts, at, c)) {
        ctx.echo("Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.");
        co_return;
    }

    auto size = co_await ctx.number(fillet ? "yaricap" : "mesafe",
                                    fillet ? "Yuvarlatma yarıçapı (metre)"
                                           : "Köşeden kesilecek mesafe (metre)");
    if (!size) co_return;

    if (*size <= 0.0) {
        ctx.echo(std::string(fillet ? "Yarıçap" : "Mesafe") + " sıfırdan büyük olmalı.");
        co_return;
    }

    const core::Mm want = core::mm_round(*size * static_cast<double>(core::kMmPerMetre));

    // HOW FAR ALONG EACH EDGE the cut lands. For a chamfer it is the distance the
    // user gave; for a fillet it is r / tan(theta/2), written with the half-angle
    // identity tan(t/2) = sin t / (1 + cos t) so nothing but +, *, / and sqrt is
    // used.
    core::Mm tangent = want;
    if (fillet) {
        const double t = static_cast<double>(want) * (1.0 + c.cos_theta) / c.sin_theta;
        tangent        = core::mm_round(t);
    }

    if (tangent <= 0) {
        ctx.echo("Bu köşe için hesaplanan kesim sıfır ya da negatif çıkıyor.");
        co_return;
    }
    if (tangent >= c.edge1 || tangent >= c.edge2) {
        ctx.echo("Kesim komşu kenardan uzun: kenarlar " + std::to_string(c.edge1 / 1000) + " m ve " +
                 std::to_string(c.edge2 / 1000) + " m, gereken " + std::to_string(tangent / 1000) +
                 " m. Daha küçük bir değer verin.");
        co_return;
    }

    const core::Point2 p1 = along(c.v, c.d1, tangent);
    const core::Point2 p2 = along(c.v, c.d2, tangent);

    if (!fillet) {
        // A CHAMFER stays one object: the corner vertex is replaced by its two
        // tangent points and the straight edge between them IS the chamfer. The
        // ring's own order decides which comes first — p1 lies toward the
        // PREVIOUS vertex.
        std::vector<core::Point2> out;
        out.reserve(pts.size() + 1);
        for (std::size_t i = 0; i < pts.size(); ++i) {
            if (i != at) {
                out.push_back(pts[i]);
                continue;
            }
            out.push_back(p1);
            out.push_back(p2);
        }

        const core::RingGeometry::RingInput ring{out, role_of(ctx, slot), 0};
        auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
    } else {
        // A FILLET BREAKS THE LINE IN TWO, and it has to.
        //
        // Leaving both tangent points in one run would draw a straight chord
        // between them AND the arc over it — a lens where a rounded corner should
        // be. The arc replaces that chord, so the two legs become two objects with
        // the arc between them.
        //
        // A CLOSED ring therefore cannot be filleted: the result is a boundary
        // made partly of a curve, and this model's ring holds vertices rather than
        // curve segments (model.md R9-R12). Refusing says so; producing an open
        // line where a parcel used to be would quietly destroy the face.
        if (closed) {
            ctx.echo("Kapalı bir alanın köşesi yuvarlatılamaz: sonuç bir kısmı yay olan bir "
                     "sınır olurdu ve bu belge modelinde halka köşe noktalarından oluşur. "
                     "Düz kenarla kesmek için PAH kullanın.");
            co_return;
        }

        std::vector<core::Point2> leg1(pts.begin(), pts.begin() + static_cast<std::ptrdiff_t>(at));
        leg1.push_back(p1);

        std::vector<core::Point2> leg2;
        leg2.push_back(p2);
        leg2.insert(leg2.end(), pts.begin() + static_cast<std::ptrdiff_t>(at) + 1, pts.end());

        if (leg1.size() < 2 || leg2.size() < 2) {
            ctx.echo("Bu köşe yuvarlatılınca kenarlardan biri tek noktaya iniyor.");
            co_return;
        }

        // The FIRST leg keeps the object, so its key, layer, style and attributes
        // stay with it (model.md R4, R28), exactly as BÖL does.
        const core::RingGeometry::RingInput ring{leg1, core::RingRole::Open, 0};
        auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }

        auto second = ctx.transaction().add_polyline(ctx.document().entities().layer[slot], leg2);
        if (!second) {
            ctx.echo(second.error().message);
            co_return;
        }
        if (const core::StyleId style = ctx.document().entities().style[slot];
            style != core::kByLayerStyle) {
            auto styled = ctx.transaction().set_entity_style(second.value(), style);
            if (!styled) {
                ctx.echo(styled.error().message);
                co_return;
            }
        }
    }

    if (fillet) {
        // The arc's centre sits on the bisector, at r / sin(theta/2) from the
        // vertex. The half-angle sine comes from cos theta by identity, so there
        // is still no trigonometric call anywhere in this file.
        const double half_sin = std::sqrt((1.0 - c.cos_theta) * 0.5);
        if (half_sin <= 0.0) {
            ctx.echo("Bu köşe yuvarlatılamıyor: kenarlar üst üste geliyor.");
            co_return;
        }

        const Unit bis = [&] {
            const double bx  = c.d1.x + c.d2.x;
            const double by  = c.d1.y + c.d2.y;
            const double len = std::sqrt(bx * bx + by * by);
            return len > 0.0 ? Unit{bx / len, by / len} : Unit{};
        }();

        const core::Mm to_centre = core::mm_round(static_cast<double>(want) / half_sin);
        const core::Point2 centre = along(c.v, bis, to_centre);

        // THE SWEEP IS COUNTER-CLOCKWISE from the first end to the second
        // (core/arc.hpp), so which tangent point starts the arc depends on which
        // way the corner turns.
        const core::Point2 start = c.cross > 0.0 ? p2 : p1;
        const core::Point2 end   = c.cross > 0.0 ? p1 : p2;

        auto made = ctx.transaction().add_arc(ctx.document().entities().layer[slot], centre, want,
                                              start, end);
        if (!made) {
            ctx.echo(made.error().message);
            co_return;
        }
    }

    ctx.record("nesne", given);
    ctx.record("nokta", Value::point(*at_pt));
    ctx.record(fillet ? "yaricap" : "mesafe", Value::number(*size));
    ctx.echo(fillet ? "Köşe yuvarlatıldı." : "Köşeye pah kırıldı.");
}

Task<void> run_chamfer(Context& ctx)
{
    co_await run_corner(ctx, false);
}

Task<void> run_fillet(Context& ctx)
{
    co_await run_corner(ctx, true);
}

} // namespace

PIRICAD_COMMAND(chamfer)
{
    return CommandSpec{
        .id       = "core.chamfer",
        .names    = {"PAH", "CHAMFER", "PH"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşesi kesilecek nesnenin kimliği"},
                Param::point("nokta", "İşlem yapılacak köşe"),
                Param::number("mesafe", Arity::exactly(1),
                              "Köşeden her iki kenar boyunca kesilecek mesafe, metre"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir köşeyi düz bir kenarla keser (pah kırar).",
        .run     = &run_chamfer,
    };
}

PIRICAD_COMMAND(fillet)
{
    return CommandSpec{
        .id       = "core.fillet",
        .names    = {"YUVARLA", "FILLET", "YV"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşesi yuvarlatılacak nesnenin kimliği"},
                Param::point("nokta", "İşlem yapılacak köşe"),
                Param::number("yaricap", Arity::exactly(1), "Yuvarlatma yarıçapı, metre"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir köşeyi verilen yarıçapta yay ile yuvarlatır.",
        .run     = &run_fillet,
    };
}

} // namespace piricad::command
