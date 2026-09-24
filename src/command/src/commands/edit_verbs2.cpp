// SPDX-License-Identifier: GPL-3.0-or-later
// core.explode — PATLAT, core.align — HİZALA,
// core.divide — BÖLÜMLE, core.pedit — ÇİZGİDÜZENLE.
//
// PATLAT TAKES A THING APART into the pieces it is drawn from: a run of edges
// into single edges, a face into its boundary, a block reference into its
// members placed where they stand. It is what a drafter reaches for when the
// grouping is in the way — one corner of a parcel has to move and the parcel is
// a face, one leg of a fence has to go and the fence is one run.
//
// WHAT IT REFUSES AND WHY. A definition holding a circle, an arc or a caption is
// named rather than half-placed: those kinds carry a payload whose transform
// under a mirrored or non-uniform scale is not a circle, an arc or a caption,
// and placing their drawn outline instead would turn a circle into a 128-gon
// with an area that is not πr² — which is exactly the number a tapu reads
// (§12). Phase 2's `BLOKDÜZENLE` is where that gets a real answer.
//
// HİZALA IS NOT OTURT. `OTURT` is a least-squares Helmert fit over many common
// points and belongs to geodesy; this is the drafting verb — one or two point
// pairs, move and turn (and scale when asked), no adjustment. Each page names
// the other, because the two are easy to confuse and picking the wrong one
// silently changes what a drawing claims.
//
// BÖLÜMLE PUTS MARKS ALONG SOMETHING THAT EXISTS. `ARANOKTA` divides a line
// given by two points; this divides an OBJECT — a surveyed kerb, a road centre
// line — which is the form a station peg list is actually asked for.
//
// ÇİZGİDÜZENLE is the small edits a run needs and nothing else does: close it,
// open it, reverse it, thin it out.
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/command/transform_edit.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/transform.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One ring of an entity, as points.
std::vector<core::Point2> ring_points(const core::Document& doc, std::uint32_t ring)
{
    const auto xs = doc.geometry().ring_xs(ring);
    const auto ys = doc.geometry().ring_ys(ring);
    std::vector<core::Point2> out;
    out.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(core::Point2{xs[v], ys[v]});
    return out;
}

/// The name of a kind, for a refusal that says what it found.
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

// ----------------------------------------------------------------- PATLAT ----

Task<void> run_explode(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Patlatılacak nesneleri seçin, Enter'a basın", chosen,
                               0, "PATLAT nesneler=1"))
        co_return;
    if (chosen.empty()) co_return;

    const core::Document& doc = ctx.document();
    std::size_t made          = 0;
    std::size_t gone          = 0;

    for (const std::int64_t id : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id)));
            co_return;
        }
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }

        const core::KindId kind   = doc.entities().kind[slot];
        const core::LayerId home  = doc.entities().layer[slot];
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);

        if (kind == core::kBlockReferenceKind) {
            auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[slot]);
            if (!ref) {
                ctx.session().fail(ref.error());
                co_return;
            }
            if (ref.value().block >= doc.blocks().all().size()) {
                ctx.session().fail(core::err(core::ErrorCode::NotFound, "Blok tanımı bulunamadı."));
                co_return;
            }
            const core::BlockDef* def = &doc.blocks().at(ref.value().block);
            const core::Point2 insertion =
                core::block_reference_insertion(doc.geometry(), doc.entities().slot[slot]);

            // EVERY COPY OF THE GRID, because a grid reference is that many
            // placements and exploding one of them would leave the rest as a
            // reference nobody can tell from the pieces.
            for (int row = 0; row < static_cast<int>(ref.value().rows); ++row)
                for (int column = 0; column < static_cast<int>(ref.value().columns); ++column)
                    for (const core::EntityKey member_key : def->members) {
                        const core::EntityId member = doc.slot_of(member_key);
                        if (member == core::kNoEntity) continue;
                        const core::KindId member_kind = doc.entities().kind[member];
                        if (member_kind != core::kPolylineKind) {
                            ctx.session().fail(core::err(
                                core::ErrorCode::Unsupported,
                                std::string("Blok '") + def->name + "' içinde bir " +
                                    kind_word(member_kind) +
                                    " var ve bu sürüm onu yerine koyamıyor: bir daire ya da "
                                    "yay, aynalı veya eşit olmayan bir ölçekte artık daire "
                                    "ya da yay değildir ve çizilmiş dış çizgisini koymak "
                                    "alanını bozar. Bileşenleri tek tek düzenlemek için "
                                    "Faz 2'nin BLOKDÜZENLE komutu gelecek."));
                            co_return;
                        }
                        const core::RingSpan member_span =
                            doc.geometry().rings_of(doc.entities().slot[member]);
                        for (std::uint32_t r = member_span.first;
                             r < member_span.first + member_span.count; ++r) {
                            std::vector<core::Point2> pts = ring_points(doc, r);
                            for (core::Point2& p : pts)
                                p = core::place_block_point(ref.value(), insertion, def->base, p,
                                                            column, row);
                            auto one = ctx.transaction().add_polyline(home, pts);
                            if (!one) {
                                ctx.session().fail(one.error());
                                co_return;
                            }
                            ++made;
                        }
                    }
        } else if (kind == core::kPolylineKind) {
            // A RUN OF EDGES INTO SINGLE EDGES. A face's boundary becomes an
            // open run per ring, which is what "take the face apart" means: a
            // face is a closed thing and its pieces are not.
            for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                std::vector<core::Point2> pts = ring_points(doc, r);
                const bool closed             = doc.geometry().ring_role[r] != core::RingRole::Open;
                if (closed && pts.size() >= 3) pts.push_back(pts.front());
                for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                    const core::Point2 pair[2]{pts[i], pts[i + 1]};
                    auto one = ctx.transaction().add_polyline(home, pair);
                    if (!one) {
                        ctx.session().fail(one.error());
                        co_return;
                    }
                    ++made;
                }
            }
        } else {
            ctx.session().fail(
                core::err(core::ErrorCode::Unsupported,
                          std::string("Nesne ") + std::to_string(id) + " bir " + kind_word(kind) +
                              "; PATLAT bu sürümde çizgileri, alanları ve blok referanslarını "
                              "patlatır."));
            co_return;
        }

        auto st = ctx.transaction().erase_entity(slot);
        if (!st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ++gone;
    }

    ctx.record("nesne", Value::ids(chosen));
    ctx.echo(std::to_string(gone) + " nesne patlatıldı, " + std::to_string(made) + " parça çıktı.");
}

// ----------------------------------------------------------------- HİZALA ----

Task<void> run_align(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Hizalanacak nesneleri seçin, Enter'a basın", chosen,
                               0, "HİZALA nesneler=1 kaynak=0,0 hedef=10,10"))
        co_return;
    if (chosen.empty()) co_return;

    // THE OBJECTS RIDE UNDER THE CURSOR at every step, drawn by the transform
    // this body applies (`core::ghost_xform`, `core::align_xform`): first carried
    // by the move of the first pair, then turned — and, asked to, scaled — by
    // the second. Before this the targets were aimed along a bare line and the
    // turn was seen only after it had happened.
    auto from1 = co_await ctx.point("kaynak", "Birinci kaynak nokta");
    if (!from1) co_return;
    auto to1 = co_await ctx.point(
        "hedef", "Birinci kaynağın gideceği yer",
        PointOptions{.rubber_band    = true,
                     .rubber_origin  = *from1,
                     .rubber_shape   = RubberShape::Ghost,
                     .rubber_payload = core::encode_ghost_spec(core::GhostSpec{.keys = chosen})});
    if (!to1) co_return;

    bool scaling = false;
    if (const Value v = ctx.argument("olcekle"); !v.empty()) scaling = v.as_bool();

    // THE SECOND PAIR IS OPTIONAL AND IT IS WHAT ADDS THE TURN. One pair is a
    // move; two are a move and a rotation, and a rotation needs two. ASKED, so
    // the turn is not a script's alone: Enter at the second source keeps the
    // move and nothing else, the way every CAD's ALIGN reads it.
    std::optional<core::Point2> from2;
    std::optional<core::Point2> to2;
    if (const Value v = ctx.argument("kaynak2"); !v.empty() && !v.as_points().empty())
        from2 = v.as_points().front();
    else
        from2 = co_await ctx.point("kaynak2", "İkinci kaynak nokta — Enter: yalnız taşı",
                                   PointOptions{.rubber_band   = true,
                                                .rubber_origin = *to1,
                                                .rubber_base   = false,
                                                .rubber_shape  = RubberShape::Fixed,
                                                .rubber_chain  = {*from1, *to1}});
    if (from2) {
        if (const Value v = ctx.argument("hedef2"); !v.empty() && !v.as_points().empty())
            to2 = v.as_points().front();
        else
            to2 =
                co_await ctx.point("hedef2", "İkinci kaynağın gideceği doğrultu",
                                   PointOptions{.rubber_band    = true,
                                                .rubber_origin  = *to1,
                                                .rubber_shape   = RubberShape::Ghost,
                                                .rubber_payload = core::encode_ghost_spec(
                                                    core::GhostSpec{.kind  = core::GhostKind::Align,
                                                                    .keys  = chosen,
                                                                    .from1 = *from1,
                                                                    .to1   = *to1,
                                                                    .from2 = *from2,
                                                                    .scale = scaling})});
    }
    const bool turning = from2 && to2;

    // A THIRD PAIR SAYS WHICH SIDE (TODOS C-08). Two pairs fix a move, a turn
    // and a scale; in the plane a third decides whether the objects are also
    // turned over — when the source triangle runs the other way round from the
    // target one, they are reflected, so the third point lands on the side of
    // the line its target is on. Only given, never asked: two pairs are how
    // every CAD aligns in the plane.
    bool flip = false;
    std::optional<core::Point2> from3;
    std::optional<core::Point2> to3;
    if (const Value v = ctx.argument("kaynak3"); turning && !v.empty() && !v.as_points().empty())
        from3 = v.as_points().front();
    if (const Value v = ctx.argument("hedef3"); from3 && !v.empty() && !v.as_points().empty())
        to3 = v.as_points().front();
    if (from3 && !to3) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Üçüncü kaynağın gideceği yer verilmedi: hedef3=<nokta>."));
        co_return;
    }
    if (from2 && to2 && from3 && to3) {
        const auto side = [](core::Point2 a, core::Point2 b, core::Point2 c) {
            const double t = static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y) -
                             static_cast<double>(b.y - a.y) * static_cast<double>(c.x - a.x);
            return (t > 0.0) - (t < 0.0);
        };
        const int was     = side(*from1, *from2, *from3);
        const int becomes = side(*to1, *to2, *to3);
        if (was == 0 || becomes == 0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Üçüncü nokta ilk ikisiyle aynı doğru üzerinde; hangi "
                                         "yana düştüğü okunamıyor."));
            co_return;
        }
        flip = was != becomes;
    }

    // ONE TRANSFORM, the one the ghost drew: `p -> to1 + factor · turn(p - from1)`.
    core::Xform x = core::ghost_xform(core::GhostKind::Translate, *from1, *to1);
    if (turning) {
        auto aligned = core::align_xform(*from1, *to1, *from2, *to2, scaling, flip);
        if (!aligned) {
            ctx.session().fail(core::err(
                core::ErrorCode::InvalidArgument,
                "İki kaynak ya da iki hedef nokta aynı; doğrultu ve ölçek hesaplanamaz."));
            co_return;
        }
        x = *aligned;
    }
    const double factor = turning ? x.factor : 1.0;

    const core::Document& doc = ctx.document();
    std::size_t moved         = 0;
    for (const std::int64_t id : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id)));
            co_return;
        }
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        // EVERY KIND, BY THE ONE TRANSFORM EVERY VERB USES: a circle stays a
        // circle of the scaled radius, a caption turns and grows, a block
        // keeps its symbol, a dimension says its new figure. It aligned lines
        // and faces only (TODOS C-08).
        if (!transform_entity(ctx, slot, x)) co_return;
        ++moved;
    }

    ctx.record("nesne", Value::ids(chosen));
    if (turning) {
        ctx.record("kaynak2", Value::point(*from2));
        ctx.record("hedef2", Value::point(*to2));
    }
    if (from3 && to3) {
        ctx.record("kaynak3", Value::point(*from3));
        ctx.record("hedef3", Value::point(*to3));
    }
    std::string how = " (taşındı).";
    if (turning)
        how = factor == 1.0 ? " (taşındı ve döndürüldü" : " (taşındı, döndürüldü ve ölçeklendi";
    if (turning) how += flip ? "; üçüncü nokta öbür yana düştüğü için ters çevrildi)." : ").";
    ctx.echo(std::to_string(moved) + " nesne hizalandı" + how);
}

// --------------------------------------------------- BÖLÜMLE / İŞARETLE ----

Task<void> run_divide(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Bölünecek nesneyi seçin, Enter'a basın", chosen, 1,
                               "BÖLÜMLE nesne=1 sayi=4"))
        co_return;
    if (chosen.size() != 1) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument, "BÖLÜMLE tek bir nesneyle çalışır."));
        co_return;
    }

    const core::Document& doc = ctx.document();
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(chosen.front()));
    const core::EntityId slot = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.session().fail(core::err(core::ErrorCode::NotFound, "Nesne bulunamadı."));
        co_return;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.session().fail(
            core::err(core::ErrorCode::Unsupported,
                      std::string("Nesne bir ") + kind_word(doc.entities().kind[slot]) +
                          "; BÖLÜMLE bu sürümde çizgileri ve alan sınırlarını böler."));
        co_return;
    }

    const core::RingSpan span     = doc.geometry().rings_of(doc.entities().slot[slot]);
    std::vector<core::Point2> pts = ring_points(doc, span.first);
    if (doc.geometry().ring_role[span.first] != core::RingRole::Open && pts.size() >= 3)
        pts.push_back(pts.front());
    if (pts.size() < 2) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument, "Nesnenin iki köşesi yok."));
        co_return;
    }

    // The cumulative length at each vertex, so a station can be found by reading
    // along rather than by walking the run again per mark.
    std::vector<double> at;
    at.push_back(0.0);
    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        at.push_back(at.back() + core::mm_to_metres(core::segment_length(pts[i], pts[i + 1])));
    const double total = at.back();
    if (total <= 0.0) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument, "Nesnenin uzunluğu sıfır."));
        co_return;
    }

    // EITHER A COUNT OR A SPACING, AND EXACTLY ONE. `sayi` cuts into that many
    // equal parts and marks the joins; `aralik` walks a fixed distance from the
    // start, which is what a chainage list is.
    Value count         = ctx.argument("sayi");
    const Value spacing = ctx.argument("aralik");

    // NEITHER GIVEN MEANS ASK, and `sayi` is the one asked for.
    //
    // It read both from arguments only, so pressing BÖLÜMLE in the tool column
    // asked which object and then REFUSED, telling the user to type `sayi=` — a
    // command reachable by mouse that cannot be finished by one (CLAUDE.md 5.15).
    // `sayi` is this command's own meaning ("into how many equal parts"); the
    // fixed interval is what the plan's İŞARETLE would have been and stays the
    // typed and scripted road, because one number cannot say which of the two it
    // is. The emptiness of the argument decides, never `InputSource`.
    if (count.empty() && spacing.empty()) {
        auto asked = co_await ctx.integer(
            "sayi", "Kaç eşit parçaya bölünecek (sabit aralık için aralik=<m> yazın). Uzunluk " +
                        metres_text(core::mm_from_metres(total)) + " m");
        if (!asked) co_return;
        count = Value::integer(*asked);
    }

    if (count.empty() == spacing.empty()) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Tam olarak birini verin: sayi= (kaç eşit parça) ya da aralik= (sabit aralık, m). "
            "Nesnenin uzunluğu " +
                std::to_string(total) + " m."));
        co_return;
    }

    std::vector<double> stations;
    if (!count.empty()) {
        const std::int64_t parts = count.as_int();
        for (std::int64_t i = 1; i < parts; ++i)
            stations.push_back(total * static_cast<double>(i) / static_cast<double>(parts));
    } else {
        const double step = spacing.as_number();
        if (step <= 0.0) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Aralık sıfır ya da eksi olamaz."));
            co_return;
        }
        for (double s = step; s < total; s += step)
            stations.push_back(s);
    }

    // A POINT, OR A BLOCK AT EVERY STATION. A row of manholes, poles, trees or
    // kerb markers is what a chainage list is FOR, and placing them one INSERT at
    // a time is the work this command exists to remove. The definition has to
    // exist already — minting one here would be `BLOK`'s job done twice (5.10).
    core::BlockId definition = core::kNoBlock;
    if (const Value named = ctx.argument("blok"); !named.empty()) {
        definition = doc.blocks().find(named.as_text());
        if (definition == core::kNoBlock) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "'" + named.as_text() +
                                             "' adlı blok yok. BLOK ile tanımlayın, sonra "
                                             "BÖLÜMLE blok=" +
                                             named.as_text() + " ile dizin."));
            co_return;
        }
    }

    // ALIGNED MEANS TURNED TO THE SEGMENT IT SITS ON, which is what a kerb
    // marker or an arrow wants and what a manhole cover does not care about.
    // Default off, because a block drawn upright stays upright unless asked.
    bool aligned = false;
    if (const Value v = ctx.argument("hizala"); !v.empty()) aligned = v.as_bool();

    std::size_t placed = 0;
    for (const double station : stations) {
        // Which segment the station falls on, and where along it.
        std::size_t i = 0;
        while (i + 2 < at.size() && at[i + 1] < station)
            ++i;
        const double span_m = at[i + 1] - at[i];
        const double along  = span_m > 0.0 ? (station - at[i]) / span_m : 0.0;
        const double dx     = static_cast<double>(pts[i + 1].x - pts[i].x);
        const double dy     = static_cast<double>(pts[i + 1].y - pts[i].y);
        const core::Point2 mark{pts[i].x + core::mm_round(along * dx),
                                pts[i].y + core::mm_round(along * dy)};

        if (definition == core::kNoBlock) {
            auto one = ctx.transaction().add_point(ctx.active_layer(), mark);
            if (!one) {
                ctx.session().fail(one.error());
                co_return;
            }
            ++placed;
            continue;
        }

        core::BlockReference ref;
        ref.block = definition;
        // THE SEGMENT'S OWN DIRECTION, from `atan2_udeg` and not from libm: a
        // reference's rotation is stored in micro-degrees counter-clockwise, which
        // is exactly what that function answers (§7.3).
        if (aligned)
            ref.rotation_udeg = core::atan2_udeg(pts[i + 1].y - pts[i].y, pts[i + 1].x - pts[i].x);
        ref.bounds = core::block_reference_bounds(doc, mark, ref);

        const core::Point2 one_point[1]{mark};
        const core::RingGeometry::RingInput ring{std::span<const core::Point2>(one_point, 1),
                                                 core::RingRole::Open, 0};
        const std::vector<std::uint8_t> payload = core::encode_block_reference(ref);
        auto made                               = ctx.transaction().add_kind(
            ctx.active_layer(), core::kBlockReferenceKind,
            std::span<const core::RingGeometry::RingInput>(&ring, 1), payload);
        if (!made) {
            ctx.session().fail(made.error());
            co_return;
        }
        ++placed;
    }

    ctx.record("nesne", Value::ids({chosen.front()}));
    ctx.echo(std::to_string(placed) + (definition == core::kNoBlock ? " işaret" : " blok") +
             " yerleştirildi (uzunluk " + std::to_string(total) + " m).");
}

// --------------------------------------------------------- ÇİZGİDÜZENLE ----

Task<void> run_pedit(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Düzenlenecek çizgileri seçin, Enter'a basın", chosen,
                               0, "ÇİZGİDÜZENLE nesneler=1 islem=kapat"))
        co_return;
    if (chosen.empty()) co_return;

    auto verb = co_await ctx.text("islem", "İşlem: kapat / ac / ters / sadelestir",
                                  {"kapat", "ac", "ters", "sadelestir"});
    if (!verb) co_return;
    const auto is = [&verb](const char* word) { return core::turkish_key_equals(*verb, word); };

    // THE WORD IS CHECKED BEFORE ANYTHING IS EDITED. An unknown or empty one fell
    // through to the last branch — simplifying at a tolerance of nothing — and
    // the command reported "2 çizgi düzenlendi ()" before the bus's own check of
    // the resolved word refused it and rolled the edit back: a success sentence
    // and a refusal for one press, and the success was false.
    if (!is("kapat") && !is("ac") && !is("ters") && !is("sadelestir")) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Tanınmayan işlem: '" + *verb + "'. İşlemler: kapat / ac / ters / sadelestir");
        co_return;
    }

    core::Mm tolerance = 0;
    if (is("sadelestir")) {
        double given = 0.0;
        if (const Value v = ctx.argument("tolerans"); !v.empty())
            given = v.as_number();
        else {
            auto asked =
                co_await ctx.number("tolerans", "Sadeleştirme toleransı (m); bundan yakın köşeler "
                                                "atılır");
            if (!asked) co_return;
            given = *asked;
        }
        tolerance = core::mm_from_metres(given);
        if (tolerance <= 0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Sadeleştirme toleransı sıfırdan büyük olmalı."));
            co_return;
        }
    }

    const core::Document& doc = ctx.document();
    std::size_t touched       = 0;
    std::size_t dropped       = 0;

    for (const std::int64_t id : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id)));
            co_return;
        }
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        // A POLYLINE WHOSE EDGES BEND (TODOS C-05) is closed, opened and turned
        // round as the path it is, so every arc stays its arc: closing adds a
        // straight closing edge, opening removes the closing edge, and turning
        // it round walks each arc the other way. Thinning its vertices would
        // take the ends out from under its arcs, so that is refused by name.
        if (doc.entities().kind[slot] == core::kArcPolylineKind) {
            auto path = core::path_of(doc, slot);
            if (!path) {
                ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                             "Nesne " + std::to_string(id) +
                                                 " okunamadı; yaylı çoklu çizginin yayları "
                                                 "köşelerine uymuyor."));
                co_return;
            }
            core::CurvePath edited = *path;
            if (is("ters")) {
                edited = core::reversed(*path);
            } else if (is("kapat")) {
                if (!edited.closed) {
                    const core::Point2 end   = edited.pieces.back().to;
                    const core::Point2 start = edited.pieces.front().from;
                    if (end != start)
                        edited.pieces.push_back(core::PathPiece{.from = end, .to = start});
                    edited.closed = true;
                }
            } else if (is("ac")) {
                if (edited.closed) {
                    edited.pieces.pop_back(); ///< the closing edge, bent or straight
                    edited.closed = false;
                }
            } else {
                ctx.session().fail(core::err(
                    core::ErrorCode::Unsupported,
                    "Nesne " + std::to_string(id) +
                        " yaylı bir çoklu çizgi; sadeleştirmek yaylarının uçlarını atardı. "
                        "Yayları korumak için sadeleştirmeyin; gerekirse önce PATLAT ile ayırın."));
                co_return;
            }
            if (edited.pieces.empty() || !write_path(ctx, slot, edited)) {
                if (edited.pieces.empty())
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "Açılan şekilde kenar kalmıyor."));
                co_return;
            }
            ++touched;
            continue;
        }
        if (doc.entities().kind[slot] != core::kPolylineKind) {
            ctx.session().fail(
                core::err(core::ErrorCode::Unsupported,
                          std::string("Nesne ") + std::to_string(id) + " bir " +
                              kind_word(doc.entities().kind[slot]) +
                              "; ÇİZGİDÜZENLE çizgilerle, yaylı çoklu çizgilerle ve alanlarla "
                              "çalışır."));
            co_return;
        }

        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
        std::vector<std::vector<core::Point2>> store;
        std::vector<core::RingGeometry::RingInput> rings;

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            std::vector<core::Point2> pts = ring_points(doc, r);
            core::RingRole role           = doc.geometry().ring_role[r];

            if (is("ters")) {
                std::reverse(pts.begin(), pts.end());
            } else if (is("kapat")) {
                role = r == span.first ? core::RingRole::Exterior : core::RingRole::Interior;
            } else if (is("ac")) {
                role = core::RingRole::Open;
            } else {
                // SADELEŞTİR: a vertex within the tolerance of the simplified
                // line carries no information about the shape.
                //
                // CLIPPER2 DOES IT, not a loop here. This used to walk the run
                // comparing each vertex against the line from the last KEPT one
                // to the next — which is a perpendicular-distance filter and not
                // what simplifying means: whether a vertex survives then depends
                // on which of its neighbours happened to survive before it, so
                // the same shape thinned differently depending on where the walk
                // started. Clipper2 has solved this and every degenerate case
                // around it, and CLAUDE.md 5.16 says a solved problem is not
                // re-solved here. The plan named `SimplifyPath` by name.
                const std::size_t was = pts.size();
                pts = core::simplify_ring(pts, tolerance, role != core::RingRole::Open);
                dropped += was - pts.size();
            }

            store.push_back(std::move(pts));
            rings.push_back(core::RingGeometry::RingInput{{}, role, doc.geometry().ring_part[r]});
        }
        for (std::size_t i = 0; i < rings.size(); ++i)
            rings[i].points = store[i];

        auto st = ctx.transaction().set_geometry(slot, rings);
        if (!st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ++touched;
    }

    ctx.record("nesne", Value::ids(chosen));
    ctx.record("islem", Value::text(*verb));
    ctx.echo(std::to_string(touched) + " çizgi düzenlendi (" + *verb + ")" +
             (is("sadelestir") ? ", " + std::to_string(dropped) + " köşe atıldı." : "."));
}

} // namespace

KENTOS_COMMAND(explode)
{
    return CommandSpec{
        .id       = "core.explode",
        .names    = {"PATLAT", "EXPLODE", "PTL"},
        .title    = "Patlat",
        .category = Category::Modify,
        .params   = {Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Patlatılacak nesneler"}
                         .en("object")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine "
                   "ayırır.",
        .run    = &run_explode,
        .effect = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(align)
{
    return CommandSpec{
        .id       = "core.align",
        .names    = {"HİZALA", "HIZALA", "ALIGN", "HZL"},
        .title    = "Hizala",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Hizalanacak nesneler"}
                    .en("object"),
                Param::point("kaynak", "Birinci kaynak nokta").en("source"),
                Param::point("hedef", "Birinci kaynağın gideceği yer").en("target"),
                Param::points("kaynak2", Arity::optional(),
                              "İkinci kaynak nokta; verilirse döndürme de yapılır")
                    .en("source2"),
                Param::points("hedef2", Arity::optional(), "İkinci kaynağın gideceği yer")
                    .en("target2"),
                Param::boolean("olcekle", Arity::optional(),
                               "İki çiftin uzunluk oranıyla ölçekler de")
                    .en("scale"),
                Param::points("kaynak3", Arity::optional(),
                              "Üçüncü kaynak nokta: hedefi ilk iki hedefin öbür yanındaysa "
                              "nesneler ters çevrilir")
                    .en("source3"),
                Param::points("hedef3", Arity::optional(), "Üçüncü kaynağın gideceği yan")
                    .en("target3"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse "
                   "ölçekler.",
        .run     = &run_align,
        .effect  = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(divide)
{
    return CommandSpec{
        .id       = "core.divide",
        .names    = {"BÖLÜMLE", "BOLUMLE", "DIVIDE", "BLM"},
        .title    = "Bölümle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Bölünecek nesne"}.en(
                    "object"),
                Param::integer_range("sayi", Arity::optional(), 2, 10000,
                                     "Kaç eşit parçaya bölünecek")
                    .en("count"),
                Param::number("aralik", Arity::optional(),
                              "Sabit aralık (m); başlangıçtan itibaren yürür")
                    .measured_in("m")
                    .en("spacing"),
                Param::text("blok", Arity::optional(),
                            "Nokta yerine bu bloğu koyar; blok önceden tanımlı olmalı")
                    .en("block"),
                Param::boolean("hizala", Arity::optional(),
                               "Bloğu üzerinde durduğu kenarın doğrultusuna çevirir")
                    .en("align"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya "
                   "blok yerleştirir.",
        .run     = &run_divide,
        .effect  = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(pedit)
{
    return CommandSpec{
        .id       = "core.pedit",
        .names    = {"ÇİZGİDÜZENLE", "CIZGIDUZENLE", "PEDIT", "ÇZD", "CZD"},
        .title    = "Çizgi Düzenle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Düzenlenecek çizgiler"}
                    .en("object"),
                Param::choice("islem", Arity::optional(), {"kapat", "ac", "ters", "sadelestir"},
                              "kapat: kapalı alana çevir · ac: aç · ters: yönünü çevir · "
                              "sadelestir: yakın köşeleri at")
                    .en("action"),
                Param::number("tolerans", Arity::optional(),
                              "sadelestir: bu uzaklıktan yakın köşeler atılır (m)")
                    .measured_in("m")
                    .en("tolerance"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak "
                   "sadeleştirir.",
        .run     = &run_pedit,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
