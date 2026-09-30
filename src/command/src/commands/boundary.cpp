// SPDX-License-Identifier: GPL-3.0-or-later
// core.boundary (SINIR) — the boundary of the region a click is inside.
//
// A PARCEL IS OFTEN ONLY LINES. It came in from a DXF, off a total station or
// out of an old sheet as loose segments, arcs and polylines that cross and
// touch; the ground they close is obvious to the eye and nowhere in the
// document. SINIR asks the drawing which ground the click is on — the smallest
// face of the visible linework around it, every island inside as a hole — and
// writes that face as one new object, leaving the lines as they were
// (core/planar.hpp, TODOS C-09).
//
// NOTHING CLOSES SILENTLY. Ends within the project's node tolerance
// (`core.topoloji.dugum_toleransi`) are one node, as they are for ALANAÇEVİR and
// İFRAZ; anything farther apart is a GAP, and a region that does not close is
// refused with its open ends marked on the canvas and measured in the sentence.
// Bridging them is asked for by name (`bosluk=`), and every bridge is reported.
//
// ARCS STAY ARCS. A face bounded by straight lines is an area; one with arcs
// and no islands is an arc-polyline, whose arcs are the drawing's own; a whole
// circle is a circle. Only a face with arcs AND holes — which no single kind in
// the model holds — is written with its arcs as chords, and the sentence says
// how far those chords are from the arcs.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/measure_mark.hpp"
#include "piricad/command/region_input.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/arc.hpp"
#include "piricad/core/curve_path.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/planar.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace piricad::command {
namespace {

/// An area in square metres to two decimals, divided in integers (Article 2.4).
std::string square_metres(core::Mm2 v)
{
    const bool negative     = v < 0;
    const auto abs_mm2      = static_cast<std::uint64_t>(negative ? -v : v);
    const std::uint64_t cm2 = (abs_mm2 + 5000) / 10000;
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return (negative ? "-" : "") + std::to_string(cm2 / 100) + "," + frac + " m²";
}

/// The face, written as the kind that holds it (`core::face_shape`): a record,
/// or an area whose arcs are chords when it has holes too, `deviation` saying
/// how far.
Result<core::EntityId> write_face(Context& ctx, const core::NetworkFace& face, core::Mm& deviation,
                                  std::string& kind_words)
{
    const core::FaceShape shape = core::face_shape(face);
    if (shape.whole) {
        const core::RingGeometry::RingInput ring{shape.record.ring, shape.record.role, 0};
        kind_words = shape.record.kind == core::kCircleKind ? "daire" : "yaylı çoklu çizgi";
        return ctx.transaction().add_kind(ctx.active_layer(), shape.record.kind, {&ring, 1},
                                          shape.record.payload);
    }
    deviation = shape.chords;
    std::vector<core::RingGeometry::RingInput> input;
    input.reserve(shape.rings.size());
    for (std::size_t i = 0; i < shape.rings.size(); ++i)
        input.push_back(core::RingGeometry::RingInput{
            shape.rings[i], i == 0 ? core::RingRole::Exterior : core::RingRole::Interior, 0});
    kind_words = face.holes.empty() ? "alan" : "delikli alan";
    return ctx.transaction().add_area(ctx.active_layer(), input);
}

Task<void> run(Context& ctx)
{
    const core::Document& doc = ctx.document();

    // THE REGION, asked the one way every command that takes one asks it
    // (`command::ask_region`): SINIR's point, islands, bridge and boundary set.
    auto found = co_await ask_region(ctx, "Sınırı çıkarılacak bölgenin içine tıklayın", true);
    if (!found) co_return;
    const core::RegionQuery& query = found->query;
    const core::Region& region     = found->region;

    const core::NetworkFace& face = *region.face;
    core::Mm chords               = 0;
    std::string kind_words;
    auto made = write_face(ctx, face, chords, kind_words);
    if (!made) {
        ctx.refuse(made.error());
        co_return;
    }
    // The boundary's origin: every object whose linework drew it (core/lineage.hpp),
    // and how it was found, so it can be found again among them (TODOS F-04).
    Args asked;
    asked.set("nokta", Value::point(query.at));
    if (ctx.has_argument("ada")) asked.set("ada", Value::boolean(query.islands));
    if (query.bridge > 0) asked.set("bosluk", Value::integer(query.bridge));
    if (auto st = ctx.derive_result(made.value(), std::span<const core::EntityId>(region.sources),
                                    &asked);
        !st) {
        ctx.refuse(st.error());
        co_return;
    }

    record_region(ctx, *found);

    // THE SENTENCE: what was made and how big, then everything that was not the
    // drawing's own — bridges, joins, chords, curves as drawn — so nothing about
    // the boundary is a surprise later.
    std::string said = "Sınır çıkarıldı: " + square_metres(face.area) + " " + kind_words;
    if (!face.holes.empty()) {
        said += " (dış sınır " + square_metres(face.outer_area) + ", " +
                std::to_string(face.holes.size()) + " ada " +
                square_metres(face.outer_area - face.area) + ")";
    }
    said += "; " + std::to_string(region.sources.size()) + " nesnenin çizgisinden.";
    if (!region.bridges.empty()) {
        said += " " + std::to_string(region.bridges.size()) + " boşluk köprülendi:";
        for (std::size_t i = 0; i < region.bridges.size(); ++i)
            said += (i == 0 ? " " : ", ") + gap_words(region.bridges[i].width);
        said += '.';
    }
    if (region.snaps.moved > 0)
        said += " " + std::to_string(region.snaps.moved) +
                " uç düğüm toleransıyla birleştirildi (en çok " + gap_words(region.snaps.largest) +
                ").";
    if (region.approximate)
        said += " Elips ya da spline çizildiği hâliyle izlendi (sapma ≤ " +
                gap_words(region.deviation) + ").";
    if (chords > 0)
        said += " Delikli bir alan yay taşıyamadığı için yaylar kirişlerle yazıldı (sapma ≤ " +
                gap_words(chords) + ").";
    ctx.echo(said);

    core::Json report = core::Json::object({});
    report.set("nesne", core::Json::integer(static_cast<std::int64_t>(
                            core::raw(ctx.document().entities().key[made.value()]))));
    report.set("alan_mm2", core::Json::integer(face.area));
    report.set("dis_alan_mm2", core::Json::integer(face.outer_area));
    report.set("ada", core::Json::integer(static_cast<std::int64_t>(face.holes.size())));
    core::Json from = core::Json::array({});
    for (const core::EntityId e : region.sources)
        from.push(core::Json::integer(static_cast<std::int64_t>(core::raw(doc.entities().key[e]))));
    report.set("kaynaklar", std::move(from));
    report.set("kopru", core::Json::integer(static_cast<std::int64_t>(region.bridges.size())));
    report.set("birlesen", core::Json::integer(static_cast<std::int64_t>(region.snaps.moved)));
    ctx.report(std::move(report));
}

} // namespace

PIRICAD_COMMAND(boundary)
{
    return CommandSpec{
        .id       = "core.boundary",
        .names    = {"SINIR", "BOUNDARY", "SNR"},
        .title    = "Sınır Bul",
        .category = Category::Draw,
        .params =
            {
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Sınırı çıkarılacak bölgenin içindeki nokta; yoksa sorulur"}
                    .en("point"),
                Param::boolean("ada", Arity::optional(),
                               "İçerideki kapalı çizgiler delik olsun mu; varsayılan evet")
                    .en("islands"),
                Param::integer("bosluk", Arity::optional(),
                               "Bu genişliğe kadar açık uçları köprüle, milimetre; varsayılan 0: "
                               "hiçbir boşluk kendiliğinden kapanmaz")
                    .measured_in("mm")
                    .en("gap"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Sınır sayılacak nesneler; yoksa görünen her çizgi"}
                    .en("objects"),
                Param::draw_layer(),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "İçine tıklanan kapalı bölgenin sınırını yeni bir alan olarak çıkarır; "
                   "içerideki adalar delik olur, açık uçlar gösterilir.",
        .run     = &run,
    };
}

} // namespace piricad::command
