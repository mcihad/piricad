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
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/corner.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The polyline behind one id, its vertices and its ring's role.
bool corner_of(Context& ctx, const Value& given, core::EntityId& slot,
               std::vector<core::Point2>& pts, std::int64_t& id, core::RingRole& role)
{
    const core::Document& doc = ctx.document();

    id = given.kind() == Value::Kind::IdList ? (given.as_ids().size() == 1 ? given.as_ids()[0] : 0)
                                             : given.as_int();

    if (id <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Geçersiz nesne kimliği: " + std::to_string(id) + ". Kimlikler 1'den başlar.");
        return false;
    }
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    slot           = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind ||
        doc.texts().has(doc.entities().slot[slot])) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda "
                       "çalışır.");
        return false;
    }

    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " çok halkalı; köşe işlemleri tek halkalı nesnelerde çalışır.");
        return false;
    }

    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    pts.clear();
    pts.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        pts.push_back(core::Point2{xs[v], ys[v]});
    role = doc.geometry().ring_role[span.first];
    return true;
}

Task<void> run_corner(Context& ctx, bool fillet)
{
    const char* verb          = fillet ? "YUVARLA" : "PAH";
    const core::Document& doc = ctx.document();
    Bus& bus                  = ctx.session().bus();

    // WHICH OBJECT AND WHICH CORNER, three ways in:
    //   1. named, `nesne`, and then the corner by `nokta`;
    //   2. one object highlighted — the same;
    //   3. neither: ONE CLICK ON THE CORNER, which names both.
    //
    // The third is the one a hand uses and it did not exist: the menu entries
    // answered "PAH için nesne belirtilmedi" and stopped, so the tool could be
    // started only by somebody who already knew the object's key. A corner is a
    // place on the drawing, and pointing at it is the whole question.
    Value given = ctx.argument("nesne");
    if (given.empty()) {
        const auto keys = bus.selection().keys();
        if (keys.size() == 1) given = Value::ids({static_cast<std::int64_t>(core::raw(keys[0]))});
    }

    std::optional<core::Point2> at_pt;
    if (given.empty()) {
        at_pt = co_await ctx.point("nokta", fillet ? "Yuvarlatılacak köşeye tıklayın"
                                                   : "Pah kırılacak köşeye tıklayın");
        if (!at_pt) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       std::string(verb) + " için nesne belirtilmedi. Örnek: " + verb +
                           " nesne=1 nokta=10,10 " + (fillet ? "yaricap=3" : "mesafe=3"));
            co_return;
        }
        // The click's own pick box — the one SEÇ uses — so what is under the
        // cursor is what is taken; a client with no screen picks what lies
        // exactly under the point, which a corner always does.
        const core::EntityId hit = core::pick_nearest(doc, *at_pt, bus.aid_settings().pick_radius);
        if (hit == core::kNoEntity) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Orada köşesi kesilecek bir çizgi ya da alan yok. Bir çizginin iki "
                       "kenarının buluştuğu köşeye tıklayın.");
            co_return;
        }
        given = Value::ids({static_cast<std::int64_t>(core::raw(doc.key_of(hit)))});
    }

    core::EntityId slot = core::kNoEntity;
    std::vector<core::Point2> pts;
    std::int64_t id     = 0;
    core::RingRole role = core::RingRole::Open;
    if (!corner_of(ctx, given, slot, pts, id, role)) co_return;
    const bool closed = role != core::RingRole::Open;

    if (!at_pt) {
        at_pt = co_await ctx.point("nokta", "İşlem yapılacak köşe");
        if (!at_pt) co_return;
    }

    const std::optional<std::size_t> at = core::nearest_corner(pts, closed, *at_pt);
    if (!at) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Burada iki kenarın buluştuğu bir köşe yok. Açık bir çizginin uçları köşe "
                   "değildir; iki kenarın buluştuğu bir noktayı gösterin.");
        co_return;
    }

    // THE SIZE, TYPED OR SHOWN, with the cut drawn at the cursor. The preview is
    // `core::cut_corner` — the function the command is about to call — at the
    // cursor's distance from the corner, and a click there hands that distance
    // back as the answer (`pick_distance`). Before this, the corner was asked for
    // and then the user typed a number blind and found out on Enter.
    PointOptions guide;
    guide.rubber_band    = true;
    guide.rubber_origin  = pts[*at];
    guide.rubber_shape   = RubberShape::Corner;
    guide.rubber_payload = core::encode_corner_preview(
        core::CornerPreview{.key = id, .at = static_cast<std::uint32_t>(*at), .fillet = fillet});
    guide.pick_distance = true;
    auto size           = co_await ctx.number(fillet ? "yaricap" : "mesafe",
                                    fillet ? "Yuvarlatma yarıçapı (metre) — yazın ya da gösterin"
                                                     : "Köşeden kesilecek mesafe (metre) — yazın ya da "
                                                       "gösterin",
                                    std::move(guide));
    if (!size) co_return;

    const core::Mm want = core::mm_round(*size * static_cast<double>(core::kMmPerMetre));
    auto cut            = core::cut_corner(pts, closed, *at, want, fillet);
    if (!cut) {
        ctx.refuse(cut.error());
        co_return;
    }

    // The FIRST piece keeps the object, so its key, layer, style and attributes
    // stay with it (model.md R4, R28), exactly as BÖL does.
    const core::RingGeometry::RingInput ring{cut.value().kept, role, 0};
    if (auto st = ctx.transaction().set_geometry(slot, {&ring, 1}); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    if (cut.value().arc) {
        auto second =
            ctx.transaction().add_polyline(doc.entities().layer[slot], cut.value().second);
        if (!second) {
            ctx.refuse(second.error());
            co_return;
        }
        if (const core::StyleId style = doc.entities().style[slot]; style != core::kByLayerStyle) {
            auto styled = ctx.transaction().set_entity_style(second.value(), style);
            if (!styled) {
                ctx.refuse(styled.error());
                co_return;
            }
        }
        const auto made =
            ctx.transaction().add_arc(doc.entities().layer[slot], cut.value().centre,
                                      cut.value().radius, cut.value().start, cut.value().end);
        if (!made) {
            ctx.refuse(made.error());
            co_return;
        }
    }

    // ONE SHAPE ON EVERY ROAD: `nesne=1` typed parses to a number, a click and
    // a script's `[1]` to a list, and the journal is to be the same line from
    // all three (Article 6.4). The declared shape is a selection, so a list.
    ctx.record("nesne", Value::ids({id}));
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

KENTOS_COMMAND(chamfer)
{
    return CommandSpec{
        .id       = "core.chamfer",
        .names    = {"PAH", "CHAMFER", "PH"},
        .title    = "Pah",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşesi kesilecek nesnenin kimliği"}
                    .en("object"),
                Param::point("nokta", "İşlem yapılacak köşe").en("point"),
                Param::number("mesafe", Arity::exactly(1),
                              "Köşeden her iki kenar boyunca kesilecek mesafe, metre")
                    .en("distance"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir köşeyi düz bir kenarla keser (pah kırar).",
        .run     = &run_chamfer,
    };
}

KENTOS_COMMAND(fillet)
{
    return CommandSpec{
        .id       = "core.fillet",
        .names    = {"YUVARLA", "FILLET", "YV"},
        .title    = "Yuvarla",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşesi yuvarlatılacak nesnenin kimliği"}
                    .en("object"),
                Param::point("nokta", "İşlem yapılacak köşe").en("point"),
                Param::number("yaricap", Arity::exactly(1), "Yuvarlatma yarıçapı, metre")
                    .en("radius"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir köşeyi verilen yarıçapta yay ile yuvarlatır.",
        .run     = &run_fillet,
    };
}

} // namespace kentos::command
