// SPDX-License-Identifier: GPL-3.0-or-later
// core.block_clip — BLOKKIRP: a block reference or an external reference
// shows only what lies inside a boundary (TODOS C-14, model.md R45).
//
// A base map attached whole is usually far larger than the sheet it serves,
// and a detail block is often wanted in part; AutoCAD's XCLIP is how a user
// keeps the piece that matters. The boundary is stored ON THE REFERENCE, in
// its definition's own coordinates (`core::BlockReference::clip`), so it
// turns, scales and moves with the reference, crops every copy of a grid
// alike, and changes nothing in the definition: another reference to the same
// block still shows the whole, and taking the clip off shows everything again.
//
// What a clip hides is not drawn, not snapped to and not picked — the three
// read the same cropped expansion (`core::expand_block_definition`).
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/outline.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// How many corners a boundary may have: the payload's own bound.
constexpr std::size_t kMostCorners = 65'535;

/// The name of a kind, for a refusal that says what it found — PATLAT's
/// words, so the two refuse alike.
const char* kind_word(core::KindId k)
{
    switch (k) {
    case core::kPolylineKind: return "çizgi";
    case core::kCircleKind: return "daire";
    case core::kArcKind: return "yay";
    case core::kPointKind: return "nokta";
    case core::kEllipseKind: return "elips";
    case core::kArcPolylineKind: return "yaylı çizgi";
    case core::kSplineKind: return "spline";
    case core::kHatchKind: return "tarama";
    case core::kBlockReferenceKind: return "blok referansı";
    case core::kDimensionKind: return "ölçü";
    case core::kLeaderKind: return "kılavuz çizgi";
    default: return "bilinmeyen tür";
    }
}

/// What a client is told when no boundary came: a script that forgot one and
/// a user who ended the question read the same sentence; a user who pressed
/// Esc is told "İptal edildi" by the bus instead.
constexpr const char* kNoBoundary =
    "Kırpma sınırı verilmedi: noktalar= ile köşelerini ya da cizgi= ile kapalı bir nesneyi "
    "verin.\n  Örnek: BLOKKIRP nesne=1 noktalar=0,0 100,50";

/// Twice the signed area of `ring`, in square millimetres, carried in 128 bits.
core::Int128 twice_area(const std::vector<core::Point2>& ring)
{
    core::Int128 sum = 0;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        sum += static_cast<core::Int128>(ring[j].x) * ring[i].y -
               static_cast<core::Int128>(ring[i].x) * ring[j].y;
    return sum;
}

/// `pts` without a corner that repeats the one before it, nor a last corner
/// that repeats the first — a ring's closing edge is implied.
std::vector<core::Point2> distinct_corners(const std::vector<core::Point2>& pts)
{
    std::vector<core::Point2> out;
    out.reserve(pts.size());
    for (const core::Point2 p : pts)
        if (out.empty() || out.back() != p) out.push_back(p);
    while (out.size() > 1 && out.back() == out.front())
        out.pop_back();
    return out;
}

/// The four corners of the rectangle `a` and `b` span, counter-clockwise.
std::vector<core::Point2> rectangle(core::Point2 a, core::Point2 b)
{
    const core::Mm x0 = std::min(a.x, b.x);
    const core::Mm x1 = std::max(a.x, b.x);
    const core::Mm y0 = std::min(a.y, b.y);
    const core::Mm y1 = std::max(a.y, b.y);
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

/// The boundary's corners in the drawing, from a closed object `id`: a closed
/// polyline's ring, or the drawn form of a closed curve — a circle, an
/// ellipse, a closed polyline whose edges bend. Empty, with the refusal
/// already made, when the object cannot bound anything.
std::vector<core::Point2> corners_of_object(Context& ctx, std::int64_t id)
{
    const core::Document& doc = ctx.document();
    const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    const core::EntityId e    = doc.slot_of(key);
    if (e == core::kNoEntity || !doc.alive(e)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Sınır olacak nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return {};
    }
    const core::KindId kind = doc.entities().kind[e];
    std::vector<core::Point2> ring;
    if (kind == core::kPolylineKind) {
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
        if (span.count > 0) {
            const auto xs    = doc.geometry().ring_xs(span.first);
            const auto ys    = doc.geometry().ring_ys(span.first);
            const bool shut  = doc.geometry().ring_role[span.first] != core::RingRole::Open;
            const bool meets = !xs.empty() && xs.front() == xs.back() && ys.front() == ys.back();
            if (shut || meets)
                for (std::size_t v = 0; v < xs.size(); ++v)
                    ring.push_back(core::Point2{xs[v], ys[v]});
        }
    } else if (kind != core::kBlockReferenceKind) {
        core::EmitBuffer drawn;
        if (core::entity_outline(doc, e, drawn) && drawn.run_total() > 0 &&
            drawn.run_closed[0] != 0) {
            const auto xs = drawn.run_xs(0);
            const auto ys = drawn.run_ys(0);
            for (std::size_t v = 0; v < xs.size(); ++v)
                ring.push_back(core::Point2{xs[v], ys[v]});
        }
    }
    ring = distinct_corners(ring);
    if (ring.size() < 3) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Nesne " + std::to_string(id) + " bir " + kind_word(kind) +
                       " ve bir alanı çevirmiyor; sınır kapalı bir çizgi, alan, daire ya da "
                       "elips olmalı.");
        return {};
    }
    return ring;
}

/// The boundary in the drawing: `noktalar` when given — two corners are a
/// rectangle, three or more a polygon — else asked for as `tur` says. Nothing
/// when the user stopped, or when a refusal has already been made.
///
/// WHAT IS RECORDED is `noktalar` in the form a replay reads back to the same
/// boundary: the two corners of a rectangle, the corners of a polygon, and
/// the corners of the object a `cizgi` named — so a replay does not depend on
/// that object still being there.
Task<std::optional<std::vector<core::Point2>>> boundary_in_drawing(Context& ctx)
{
    const auto as_given = [&ctx](std::vector<core::Point2> pts) {
        ctx.record("noktalar", Value::points(pts));
        return pts.size() == 2 ? rectangle(pts[0], pts[1]) : pts;
    };
    if (const Value given = ctx.argument("noktalar"); !given.empty())
        co_return as_given(given.as_points());

    std::string shape = "dikdortgen";
    if (const Value v = ctx.argument("tur"); !v.empty()) shape = v.as_text();
    if (shape == "dikdortgen" && !ctx.argument("cizgi").empty()) shape = "cizgi";

    if (shape == "cizgi") {
        std::vector<std::int64_t> line;
        if (!co_await want_objects(ctx, "cizgi",
                                   "Sınır olacak kapalı çizgiyi, alanı ya da daireyi seçin", line,
                                   1, "BLOKKIRP nesne=1 cizgi=2"))
            co_return std::nullopt;
        if (line.empty()) co_return std::nullopt;
        std::vector<core::Point2> ring = corners_of_object(ctx, line.front());
        if (ring.empty()) co_return std::nullopt;
        co_return as_given(std::move(ring));
    }

    if (shape == "cokgen") {
        // Corner by corner, as ÇOKLUÇİZGİ takes a run: the newest can be taken
        // back, and taking back the first asks for it again.
        std::vector<core::Point2> corners;
        while (corners.empty()) {
            auto first = co_await ctx.point("noktalar", "Kırpma sınırının ilk köşesi");
            if (!first) {
                ctx.refuse(core::ErrorCode::InvalidArgument, kNoBoundary);
                co_return std::nullopt;
            }
            corners.push_back(*first);
            for (;;) {
                auto next = co_await ctx.point(
                    "noktalar", "Sonraki köşe — Enter: sınırı kapat, ⌫: son köşeyi geri al",
                    PointOptions{.rubber_band   = true,
                                 .rubber_origin = corners.back(),
                                 .rubber_shape  = RubberShape::Ring,
                                 .rubber_chain  = corners,
                                 .can_retract   = true});
                if (next) {
                    corners.push_back(*next);
                    continue;
                }
                if (!ctx.took_back()) break;
                corners.pop_back();
                if (corners.empty()) break;
            }
        }
        co_return as_given(std::move(corners));
    }

    auto a = co_await ctx.point("noktalar", "Kırpma dikdörtgeninin ilk köşesi");
    if (!a) {
        ctx.refuse(core::ErrorCode::InvalidArgument, kNoBoundary);
        co_return std::nullopt;
    }
    auto b = co_await ctx.point("noktalar", "Karşı köşe",
                                PointOptions{.rubber_band   = true,
                                             .rubber_origin = *a,
                                             .rubber_shape  = RubberShape::Rectangle});
    if (!b) {
        ctx.refuse(core::ErrorCode::InvalidArgument, kNoBoundary);
        co_return std::nullopt;
    }
    co_return as_given({*a, *b});
}

/// The structured answer every step gives.
void report_clip(Context& ctx, const char* op, std::int64_t id, const std::string& block,
                 std::size_t corners)
{
    core::Json report = core::Json::object({});
    report.set("islem", core::Json::string(op));
    report.set("nesne", core::Json::integer(id));
    report.set("blok", core::Json::string(block));
    report.set("kose", core::Json::integer(static_cast<std::int64_t>(corners)));
    ctx.report(std::move(report));
}

Task<void> run_block_clip(Context& ctx)
{
    std::string op = "yeni";
    if (const Value v = ctx.argument("islem"); !v.empty()) op = v.as_text();
    if (op != "yeni") ctx.record("islem", Value::text(op));

    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Kırpılacak blok ya da dış referansı seçin", chosen, 1,
                               "BLOKKIRP nesne=1 noktalar=0,0 100,50", core::kBlockReferenceKind))
        co_return;
    if (chosen.empty()) co_return;

    const core::Document& doc = ctx.document();
    const std::int64_t id     = chosen.front();
    const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    const core::EntityId e    = doc.slot_of(key);
    if (e == core::kNoEntity || !doc.alive(e)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        co_return;
    }
    const core::KindId kind = doc.entities().kind[e];
    if (kind != core::kBlockReferenceKind) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) + " bir " + kind_word(kind) +
                       "; BLOKKIRP blok referanslarını ve dış referansları kırpar.");
        co_return;
    }
    if (auto st = doc.editable(e); !st) {
        ctx.refuse(st.error());
        co_return;
    }
    const std::uint32_t slot = doc.entities().slot[e];
    auto decoded             = core::block_reference_of(doc.geometry(), slot);
    if (!decoded) {
        ctx.refuse(decoded.error());
        co_return;
    }
    core::BlockReference ref = decoded.value();
    if (ref.block >= doc.blocks().size()) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne " + std::to_string(id) + " bilinmeyen bir blok tanımını çiziyor.");
        co_return;
    }
    const core::BlockDef& def = doc.blocks().at(ref.block);
    const std::string name    = def.name;
    const core::Point2 at     = core::block_reference_insertion(doc.geometry(), slot);
    ctx.record("nesne", Value::ids({id}));

    const auto write = [&ctx, e, at](core::BlockReference& next) {
        next.bounds = core::block_reference_bounds(ctx.document(), at, next);
        auto st     = ctx.transaction().set_kind_payload(e, core::encode_block_reference(next));
        if (!st) ctx.refuse(st.error());
        return static_cast<bool>(st);
    };

    if (op == "kaldir") {
        if (ref.clip.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Nesne " + std::to_string(id) + " ('" + name +
                           "') kırpılmamış; kaldırılacak sınır yok.");
            co_return;
        }
        ref.clip.clear();
        if (!write(ref)) co_return;
        report_clip(ctx, "kaldir", id, name, 0);
        ctx.echo("Nesne " + std::to_string(id) + " ('" + name +
                 "') yeniden bütün çiziliyor; kırpma sınırı kaldırıldı.");
        co_return;
    }

    if (op == "sinir") {
        if (ref.clip.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Nesne " + std::to_string(id) + " ('" + name +
                           "') kırpılmamış; çizilecek sınır yok.");
            co_return;
        }
        // Where the sheet shows it: through copy (0, 0), the copy the
        // boundary was drawn over.
        std::vector<core::Point2> world;
        world.reserve(ref.clip.size());
        for (const core::Point2 p : ref.clip)
            world.push_back(core::place_block_point(ref, at, def.base, p, 0, 0));
        const core::RingGeometry::RingInput ring{world, core::RingRole::Exterior, 0};
        auto made = ctx.transaction().add_area(
            ctx.active_layer(), std::span<const core::RingGeometry::RingInput>(&ring, 1));
        if (!made) {
            ctx.refuse(made.error());
            co_return;
        }
        report_clip(ctx, "sinir", id, name, world.size());
        ctx.echo("Nesne " + std::to_string(id) + " ('" + name + "') kırpma sınırı " +
                 std::to_string(world.size()) +
                 " köşeli kapalı çizgi olarak etkin katmana çizildi.");
        co_return;
    }

    auto asked = co_await boundary_in_drawing(ctx);
    if (!asked) co_return;
    const std::vector<core::Point2> drawn = distinct_corners(*asked);
    if (drawn.size() < 3 || twice_area(drawn) == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kırpma sınırı bir alan çevirmiyor (" + std::to_string(drawn.size()) +
                       " ayrı köşe). İki köşe bir dikdörtgen, üç ya da daha çok köşe bir çokgen "
                       "verir; köşeler bir doğru üzerinde olmamalı.");
        co_return;
    }
    if (drawn.size() > kMostCorners) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kırpma sınırı en çok " + std::to_string(kMostCorners) + " köşe alır; " +
                       std::to_string(drawn.size()) + " verildi.");
        co_return;
    }

    // KEPT WHERE THE DEFINITION IS: each corner carried back through the
    // reference, so the clip moves, turns and scales with it.
    std::vector<core::Point2> inside;
    inside.reserve(drawn.size());
    for (const core::Point2 p : drawn)
        inside.push_back(core::unplace_block_point(ref, at, def.base, p));
    inside = distinct_corners(inside);
    if (inside.size() < 3 || twice_area(inside) == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kırpma sınırı referansın ölçeğinde bir milimetreden küçük kalıyor; daha "
                   "büyük bir sınır çizin.");
        co_return;
    }

    const bool was_drawing = !ref.bounds.empty();
    const bool replaced    = !ref.clip.empty();
    ref.clip               = std::move(inside);
    // A REFERENCE THAT WOULD DRAW NOTHING is refused: it would stay on the
    // drawing with nothing to see, point at or pick, and no way back but a
    // command line that knew its number.
    if (was_drawing && core::block_reference_bounds(doc, at, ref).empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kırpma sınırı, nesne " + std::to_string(id) + " ('" + name +
                       "') referansının çizdiği hiçbir şeyi içine almıyor; referans görünmez "
                       "olurdu. Sınırı referansın üzerine çizin.");
        co_return;
    }
    if (!write(ref)) co_return;
    report_clip(ctx, "yeni", id, name, ref.clip.size());
    ctx.echo("Nesne " + std::to_string(id) + " ('" + name + "') " +
             std::to_string(ref.clip.size()) + " köşeli sınırla kırpıldı" +
             (replaced ? " (önceki sınırın yerine)" : "") +
             "; dışında kalan çizilmiyor, yakalanmıyor. Kaldırmak için: BLOKKIRP islem=kaldir "
             "nesne=" +
             std::to_string(id));
}

} // namespace

KENTOS_COMMAND(block_clip)
{
    return CommandSpec{
        .id       = "core.block_clip",
        .names    = {"BLOKKIRP", "BLOKKIRP", "XCLIP", "BKR"},
        .title    = "Blok Kırp",
        .category = Category::Modify,
        .params =
            {
                Param::choice("islem", Arity::optional(), {"yeni", "kaldir", "sinir"},
                              "yeni: sınırı koyar, varsa eskisinin yerine (varsayılan); kaldir: "
                              "sınırı kaldırır, referans bütün çizilir; sinir: sınırı etkin "
                              "katmana kapalı çizgi olarak çizer")
                    .en("action"),
                Param{"nesne", ParamKind::Selection, Arity{0, 1},
                      "Kırpılacak blok referansı ya da dış referans, bir tane; yoksa etkin "
                      "seçim"}
                    .en("reference"),
                Param::choice("tur", Arity::optional(), {"dikdortgen", "cokgen", "cizgi"},
                              "noktalar verilmediğinde sınırın nasıl gösterileceği: dikdortgen "
                              "iki köşe (varsayılan), cokgen köşe köşe, cizgi var olan kapalı bir "
                              "çizgi, alan, daire ya da elips")
                    .en("shape"),
                Param::points("noktalar", Arity{0, 0xFFFFFFFFu},
                              "Sınırın köşeleri, çizimde: iki köşe dikdörtgen, üç ya da daha "
                              "çok köşe çokgen")
                    .en("points"),
                Param{"cizgi", ParamKind::Selection, Arity{0, 1},
                      "Sınır olacak kapalı nesne: kapalı çizgi, alan, daire ya da elips"}
                    .en("boundary"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir blok referansını ya da dış referansı bir sınırla kırpar: içi çizilir, "
                   "dışı çizilmez ve yakalanmaz; tanım değişmez, kırpma kaldırılınca hepsi "
                   "görünür.",
        .run = &run_block_clip,
    };
}

} // namespace kentos::command
