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
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/measure_mark.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/planar.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace kentos::command {
namespace {

/// Millimetres as metres, trimmed: "5 cm" reads better than "0,050 m" for a gap,
/// and a gap is what this prints most.
std::string length_words(core::Mm v)
{
    const core::Mm a = v < 0 ? -v : v;
    if (a < 1000) {
        if (a % 10 == 0) return std::to_string(a / 10) + " cm";
        return std::to_string(a) + " mm";
    }
    std::string frac = std::to_string((a % 1000 + 5) / 10);
    if (frac.size() < 2) frac = "0" + frac;
    if (frac == "100") return std::to_string(a / 1000 + 1) + ",00 m";
    return std::to_string(a / 1000) + "," + frac + " m";
}

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

/// The open ends, as the canvas marks them: the ones a gap away from other
/// linework first — nearest the click first, each end in one gap only, so two
/// ends that see each other are one line and a corner gets no second, wider
/// one — and bare ends, a line stopping in the open, only when no end has a gap
/// to show.
void mark_open_ends(const Context& ctx, const std::vector<core::OpenEnd>& open)
{
    // A dozen is what a person reads; the rest are counted in the sentence.
    constexpr std::size_t kShown = 12;
    std::set<core::Point2> used;
    std::size_t shown = 0;
    for (const core::OpenEnd& end : open) {
        if (shown == kShown) break;
        if (!end.has_nearest || used.contains(end.at) || used.contains(end.nearest)) continue;
        used.insert(end.at);
        used.insert(end.nearest);
        MeasureMark m;
        m.shape  = MeasureMark::Shape::Gap;
        m.points = {end.at, end.nearest};
        m.labels = {"boşluk " + length_words(end.distance)};
        ctx.mark(m);
        ++shown;
    }
    if (shown > 0) return;
    for (std::size_t i = 0; i < open.size() && i < 3; ++i) {
        MeasureMark m;
        m.shape  = MeasureMark::Shape::Gap;
        m.points = {open[i].at};
        m.labels = {"açık uç"};
        ctx.mark(m);
    }
}

Task<void> run(Context& ctx)
{
    Bus& bus                  = ctx.session().bus();
    const core::Document& doc = ctx.document();

    core::RegionQuery query;
    query.islands        = ctx.has_argument("ada") ? ctx.argument("ada").as_bool(true) : true;
    query.node_tolerance = bus.project_settings().get("core.topoloji.dugum_toleransi").as_length();
    query.bridge         = ctx.has_argument("bosluk") ? ctx.argument("bosluk").as_int() : 0;
    if (query.bridge < 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Köprülenecek boşluk eksi olamaz; milimetre olarak 0 ya da daha büyük verin.");
        co_return;
    }
    std::vector<std::int64_t> keys;
    if (ctx.has_argument("nesneler")) {
        keys = ctx.argument("nesneler").as_ids();
        for (const std::int64_t raw : keys) {
            const core::EntityId slot =
                raw > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)))
                        : core::kNoEntity;
            if (slot == core::kNoEntity || !doc.alive(slot)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
                co_return;
            }
            query.only.push_back(slot);
        }
    }

    // THE POINT: given, or shown — and while it is shown the region under the
    // cursor is drawn, found by the same call the click makes.
    if (const Value given = ctx.argument("nokta"); !given.empty() && !given.as_points().empty()) {
        query.at = given.as_points().front();
    } else {
        const core::RegionPreview preview{query.islands, query.node_tolerance, query.bridge, keys};
        auto pointed = co_await ctx.point(
            "nokta", "Sınırı çıkarılacak bölgenin içine tıklayın",
            PointOptions{.rubber_band    = true,
                         .rubber_base    = false,
                         .rubber_shape   = RubberShape::Region,
                         .rubber_payload = core::encode_region_preview(preview)});
        if (!pointed) co_return;
        query.at = *pointed;
    }

    auto found = core::region_at(doc, query);
    if (!found) {
        ctx.refuse(found.error());
        co_return;
    }
    const core::Region& region = found.value();
    if (region.on_linework) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Nokta bir çizginin üstünde; sınırı çıkarılacak bölgenin İÇİNE tıklayın.");
        co_return;
    }
    if (!region.face) {
        if (region.open.empty()) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Bu noktayı çevreleyen kapalı bir çizgi yok. Bölgenin içine tıklayın; "
                       "gizli katmanlardaki çizgiler sınır sayılmaz.");
            co_return;
        }
        mark_open_ends(ctx, region.open);
        // The nearest end to the click that is a GAP away from other linework;
        // an end in the open, with nothing near it, is counted but not measured.
        const auto gap =
            std::ranges::find_if(region.open, [](const core::OpenEnd& e) { return e.has_nearest; });
        std::string why =
            "Bu bölge kapanmıyor: " + std::to_string(region.open.size()) + " açık uç var";
        if (gap != region.open.end())
            why += "; tıkladığınız yere en yakını bir çizgiye " + length_words(gap->distance) +
                   " uzakta";
        why += ". Uçlar tuvalde işaretlendi. Boşluğu yakalamayla kapatın ya da köprülemek için "
               "bosluk=<mm> verin.";
        ctx.refuse(core::ErrorCode::InvalidArgument, why);
        co_return;
    }

    const core::NetworkFace& face = *region.face;
    core::Mm chords               = 0;
    std::string kind_words;
    auto made = write_face(ctx, face, chords, kind_words);
    if (!made) {
        ctx.refuse(made.error());
        co_return;
    }
    // The boundary's origin: every object whose linework drew it (core/lineage.hpp).
    if (auto st = ctx.derive(made.value(), std::span<const core::EntityId>(region.sources)); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    ctx.record("nokta", Value::point(query.at));
    if (ctx.has_argument("ada")) ctx.record("ada", Value::boolean(query.islands));
    if (query.bridge > 0) ctx.record("bosluk", Value::integer(query.bridge));
    if (!keys.empty()) ctx.record("nesneler", Value::ids(keys));

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
            said += (i == 0 ? " " : ", ") + length_words(region.bridges[i].width);
        said += '.';
    }
    if (region.snaps.moved > 0)
        said += " " + std::to_string(region.snaps.moved) +
                " uç düğüm toleransıyla birleştirildi (en çok " +
                length_words(region.snaps.largest) + ").";
    if (region.approximate)
        said += " Elips ya da spline çizildiği hâliyle izlendi (sapma ≤ " +
                length_words(region.deviation) + ").";
    if (chords > 0)
        said += " Delikli bir alan yay taşıyamadığı için yaylar kirişlerle yazıldı (sapma ≤ " +
                length_words(chords) + ").";
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

KENTOS_COMMAND(boundary)
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
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "İçine tıklanan kapalı bölgenin sınırını yeni bir alan olarak çıkarır; "
                   "içerideki adalar delik olur, açık uçlar gösterilir.",
        .run = &run,
    };
}

} // namespace kentos::command
