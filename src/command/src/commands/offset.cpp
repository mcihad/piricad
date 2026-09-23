// SPDX-License-Identifier: GPL-3.0-or-later
// core.offset — OFSET. The parallel of what is selected, at a given distance, on
// the side the user shows.
//
// The construction a cadastral or zoning job leans on hardest after the line
// itself: a road right-of-way from a centre line, a çekme mesafesi from a parcel
// boundary, a protection band along a watercourse. Netcad users reach for it by
// reflex.
//
// A PARALLEL, NOT A BAND (TODOS C-03). This used to offset every ring with
// Clipper2 and add the result as a FACE, so the 2 m parallel of an open line was
// a closed 200 m² band around it — a buffer, which is a GIS operation with a
// name of its own (TAMPON). The geometry is now `core::entity_parallel`: an open
// line's parallel is an open line on one side, a face's is a face with its holes
// still holes, a circle's a circle with another radius, an arc's a concentric
// arc. A curve with no parallel of its own kind (an ellipse, a spline) gets the
// exact parallel of the curve as drawn, and the command says how far that is from
// the true one.
//
// THE SIDE IS SHOWN. After the distance the command asks for a point, and the
// canvas draws the parallel on whichever side of each object the cursor is on —
// from the same `core::entity_parallel` this body calls, so the preview is the
// result. A script names the side (`taraf=`), or shows it (`nokta=`), or gives a
// signed distance, which is what it always meant for a closed shape: plus grows
// it, minus shrinks it. An open run given neither gets both sides.
//
// THE PARALLEL IS A NEW OBJECT, not a change to the old one: a boundary is the
// measured thing and its parallel a derived one. It takes the source's layer and
// style unless told otherwise (`ozellik=aktif`), the source's attribute values
// unless told otherwise (`oznitelik=aktarma`), and the source stays unless told
// otherwise (`kaynak=sil`).
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

core::JoinStyle join_from(const std::string& word)
{
    if (core::turkish_key_equals(word, "yuvarlak")) return core::JoinStyle::Round;
    if (core::turkish_key_equals(word, "pah")) return core::JoinStyle::Bevel;
    return core::JoinStyle::Miter;
}

/// Millimetres as the metres a user reads, three decimals, Turkish comma.
std::string metres(core::Mm v)
{
    const bool negative = v < 0;
    const auto whole    = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(whole % 1000);
    while (frac.size() < 3)
        frac.insert(frac.begin(), '0');
    return (negative ? "-" : "") + std::to_string(whole / 1000) + "," + frac + " m";
}

/// Adds one piece of a parallel to the transaction, on `layer`.
core::Result<core::EntityId> add_piece(Context& ctx, core::LayerId layer,
                                       const core::ParallelPiece& piece)
{
    switch (piece.shape) {
    case core::ParallelPiece::Shape::Circle:
        return ctx.transaction().add_circle(layer, piece.centre, piece.radius);
    case core::ParallelPiece::Shape::Arc:
        return ctx.transaction().add_arc(layer, piece.centre, piece.radius, piece.start, piece.end);
    case core::ParallelPiece::Shape::Face: {
        std::vector<core::RingGeometry::RingInput> rings;
        for (const core::Polygon& face : piece.faces) {
            rings.push_back(
                core::RingGeometry::RingInput{face.exterior, core::RingRole::Exterior, 0});
            for (const std::vector<core::Point2>& hole : face.holes)
                rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});
        }
        return ctx.transaction().add_area(layer, rings);
    }
    case core::ParallelPiece::Shape::Run:
        if (piece.closed) {
            const core::RingGeometry::RingInput ring{piece.run, core::RingRole::Exterior, 0};
            return ctx.transaction().add_area(layer, {&ring, 1});
        }
        return ctx.transaction().add_polyline(layer, piece.run);
    }
    return core::err(core::ErrorCode::Internal, "Tanınmayan paralel parçası.");
}

Task<void> run(Context& ctx)
{
    // The argument, the selection, or ASKED FOR — see `want_objects`.
    Value::Ints requested;
    if (!co_await want_objects(ctx, "nesneler", "Paraleli çizilecek nesneleri seçin, Enter'a basın",
                               requested))
        co_return;

    // EVERY OBJECT IS CHECKED BEFORE ANYTHING ELSE IS ASKED: a text or a point
    // has no parallel, and saying so after the user had typed a distance and
    // aimed a side is saying it too late.
    const core::Document& doc = ctx.document();
    std::vector<core::EntityId> slots;
    for (const std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Geçersiz nesne kimliği: " + std::to_string(raw) +
                           ". Kimlikler 1'den başlar.");
            co_return;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        if (auto why = core::parallel_refusal(doc, slot)) {
            ctx.refuse(core::ErrorCode::Unsupported, "Nesne " + std::to_string(raw) + ": " + *why);
            co_return;
        }
        slots.push_back(slot);
    }

    // The distance is a LENGTH the user states: asked in metres, recorded in
    // millimetres like every other measure.
    //
    // A DISTANCE THE CALL STATED CARRIES ITS SIGN'S MEANING, and one typed at
    // the prompt does not. `OFSET nesneler=1 mesafe=-2000` is a complete call
    // and means what it always meant — minus shrinks a closed shape — so it is
    // not asked anything more. A distance typed at the prompt is always followed
    // by the side, shown with the cursor; an Esc or an empty Enter there is then
    // a cancel and draws nothing, because nothing was decided.
    const bool stated = ctx.has_argument("mesafe");
    core::Mm distance = 0;
    if (stated) {
        distance = static_cast<core::Mm>(ctx.argument("mesafe").as_int());
    } else {
        auto typed = co_await ctx.number("mesafe", "Paralel mesafesi (metre)");
        if (!typed) co_return; // ESC before anything was drawn
        distance = core::mm_round(*typed * static_cast<double>(core::kMmPerMetre));
    }
    if (distance == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Paralel mesafesi sıfır olamaz. Kaç metre paralel istediğinizi yazın.");
        co_return;
    }
    const core::Mm reach       = distance < 0 ? -distance : distance;
    const core::JoinStyle join = join_from(ctx.argument("kose").as_text());

    // THE SIDE: named, shown, or — for a complete call that gave neither — the
    // sign's meaning.
    std::optional<core::ParallelSide> named;
    if (const std::string word = ctx.argument("taraf").as_text(); !word.empty()) {
        named = core::parallel_side_from_name(word);
        if (!named) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tanınmayan taraf: '" + word + "'. Taraflar: sol / sag / dis / ic / iki");
            co_return;
        }
    }
    std::optional<core::Point2> pointed;
    if (!named && (!stated || ctx.has_argument("nokta"))) {
        PointOptions guide;
        guide.rubber_band    = true;
        guide.rubber_shape   = RubberShape::Parallel;
        const core::Box2 box = doc.entities().box_of(slots.front());
        guide.rubber_origin  = core::Point2{box.min_x + (box.max_x - box.min_x) / 2,
                                           box.min_y + (box.max_y - box.min_y) / 2};
        core::ParallelPreview preview{.keys = {}, .distance = reach, .join = join};
        for (const core::EntityId e : slots)
            preview.keys.push_back(static_cast<std::int64_t>(core::raw(doc.key_of(e))));
        guide.rubber_payload = core::encode_parallel_preview(preview);
        pointed = co_await ctx.point("nokta", "Paralelin tarafını gösterin", std::move(guide));
        if (!pointed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Paralelin tarafı gösterilmedi. Tarafı tıklayın ya da taraf=sol|sag|dis|"
                       "ic|iki ile yazın.");
            co_return;
        }
    }

    const std::string keep  = ctx.argument("kaynak").as_text();
    const bool drop_source  = core::turkish_key_equals(keep, "sil");
    const std::string props = ctx.argument("ozellik").as_text();
    const bool on_active    = core::turkish_key_equals(props, "aktif");
    // THE SOURCE'S DATA TRAVELS WITH IT, as it does with KOPYALA and with both
    // halves of a BÖL: the kerb lines of a road axis are that road's, and a
    // parallel that forgot the road's name would be retyped by hand. A setback
    // line drawn inside a parcel is the case for `aktarma` — it is not the
    // parcel, and two rows carrying one ada/parsel number is the one duplicate
    // a cadastral table must not grow (TODOS C-03).
    const std::string data = ctx.argument("oznitelik").as_text();
    const bool carry_data  = !core::turkish_key_equals(data, "aktarma");

    std::size_t made      = 0;
    std::size_t collapsed = 0;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const core::EntityId e = slots[i];
        const std::string who  = "Nesne " + std::to_string(requested[i]);

        core::ParallelSide side = core::ParallelSide::Both;
        if (named) {
            side = *named;
        } else if (pointed) {
            auto at = core::parallel_side_at(doc, e, *pointed);
            if (!at) {
                ctx.refuse(at.error());
                co_return;
            }
            side = at.value();
        } else if (core::parallel_encloses(doc, e)) {
            side = distance > 0 ? core::ParallelSide::Outside : core::ParallelSide::Inside;
        }

        auto parallel = core::entity_parallel(doc, e, reach, side, join);
        if (!parallel) {
            ctx.refuse(core::ErrorCode::InvalidArgument, who + ": " + parallel.error().message);
            co_return;
        }

        // NONE IS AN ANSWER, and it is said: shrinking a parcel past half its
        // own width leaves nothing, and a circle shrunk past its radius is no
        // circle. The other objects still get theirs.
        if (parallel.value().pieces.empty()) {
            ++collapsed;
            ctx.echo(who + ": bu mesafede paralel kalmıyor — şekil " + metres(reach) +
                     " içeri alınınca kendi içinde kapanıyor.");
            continue;
        }
        if (parallel.value().approximate)
            ctx.echo(who +
                     " kendi türünde paraleli olmayan bir eğri; paraleli çizildiği hâliyle (" +
                     std::to_string(parallel.value().drawn_vertices) +
                     " köşe) alındı ve çoklu çizgi oldu. Çizim gerçek eğriden en çok " +
                     metres(parallel.value().deviation) + " sapar.");

        const core::LayerId layer = on_active ? ctx.active_layer() : doc.entities().layer[e];
        const core::StyleId style = doc.entities().style[e];
        for (const core::ParallelPiece& piece : parallel.value().pieces) {
            auto added = add_piece(ctx, layer, piece);
            if (!added) {
                ctx.refuse(added.error());
                co_return;
            }
            // THE SOURCE'S LOOK TRAVELS WITH ITS LAYER: a road edge drawn in the
            // road style has a parallel in the road style.
            if (!on_active && style != core::kByLayerStyle) {
                if (auto st = ctx.transaction().set_entity_style(added.value(), style); !st) {
                    ctx.refuse(st.error());
                    co_return;
                }
            }
            if (carry_data) {
                const core::AttrTable& table = doc.attributes();
                for (std::size_t c = 0; c < table.columns(); ++c) {
                    const auto col = static_cast<core::AttrId>(c);
                    auto had       = doc.attribute(col, e);
                    if (!had || !had.value().present) continue;
                    if (const auto st =
                            ctx.transaction().set_attribute(col, added.value(), had.value());
                        !st) {
                        ctx.refuse(st.error());
                        co_return;
                    }
                }
            }
            ++made;
        }

        if (drop_source) {
            if (const auto st = doc.editable(e); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            if (auto st = ctx.transaction().erase_entity(e); !st) {
                ctx.refuse(st.error());
                co_return;
            }
        }
    }

    if (made == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu mesafede hiçbir nesnenin paraleli kalmıyor: " + metres(reach) +
                       " içeri alınınca şekiller kendi içinde kapanıyor. Daha küçük bir mesafe "
                       "verin ya da tarafı dışarı gösterin.");
        co_return;
    }

    // WHAT WAS DECIDED, in the words a replay reads: the named side, or the
    // point that showed it — each object's own side follows from the point, so
    // the point is the one fact that reproduces all of them. The awaiter has
    // recorded the point already.
    ctx.record("nesneler", Value::ids(requested));
    ctx.record("mesafe", Value::integer(distance));
    if (!ctx.argument("kose").empty()) ctx.record("kose", ctx.argument("kose"));
    if (named) ctx.record("taraf", Value::text(std::string(core::parallel_side_name(*named))));
    if (!keep.empty()) ctx.record("kaynak", Value::text(keep));
    if (!props.empty()) ctx.record("ozellik", Value::text(props));
    if (!data.empty()) ctx.record("oznitelik", Value::text(data));

    ctx.echo(
        std::to_string(made) + " paralel çizildi (" + metres(reach) + ")" +
        (collapsed > 0 ? "; " + std::to_string(collapsed) + " nesnenin paraleli kalmadı." : "."));
}

} // namespace

KENTOS_COMMAND(offset)
{
    return CommandSpec{
        .id       = "core.offset",
        .names    = {"OFSET", "OFFSET", "OF"},
        .title    = "Ofset",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ofseti alınacak nesneler; yoksa etkin seçim"}
                    .en("objects"),
                Param::integer("mesafe", Arity::optional(),
                               "Paralel mesafesi, milimetre. Taraf verilmez ve gösterilmezse "
                               "işaret anlam taşır: kapalı şekilde artı dışarı, eksi içeri")
                    .en("distance"),
                Param::text("kose", Arity::optional(), "KÖŞE | YUVARLAK | PAH — dış köşenin biçimi")
                    .en("corner"),
                Param::choice("taraf", Arity::optional(), {"sol", "sag", "dis", "ic", "iki"},
                              "Paralelin tarafı: açık çizgide sol ya da sag (çizim yönüne göre), "
                              "kapalı şekilde dis ya da ic, iki her iki yan")
                    .en("side"),
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Tarafı gösteren nokta: her nesnenin paraleli bu noktanın olduğu yana "
                      "düşer"}
                    .en("through"),
                Param::choice("kaynak", Arity::optional(), {"koru", "sil"},
                              "Kaynak nesne: koru (öntanımlı) ya da paralel çizilince sil")
                    .en("source"),
                Param::choice("ozellik", Arity::optional(), {"kaynak", "aktif"},
                              "Paralelin katmanı ve stili: kaynak nesneninki (öntanımlı) ya da "
                              "etkin katman")
                    .en("properties"),
                Param::choice("oznitelik", Arity::optional(), {"aktar", "aktarma"},
                              "Kaynağın öznitelik değerleri: paralele aktar (öntanımlı) ya da "
                              "aktarma — parselin içine çizilen çekme hattı gibi kaynağın kendisi "
                              "olmayan bir çizgi için")
                    .en("attributes"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: "
                   "açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire.",
        .run     = &run,
    };
}

} // namespace kentos::command
