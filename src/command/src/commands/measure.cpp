// SPDX-License-Identifier: GPL-3.0-or-later
// core.measure (ÖLÇ), core.measure_area (ALANÖLÇ)
//
// The two questions a surveyor asks a drawing more often than any other: how far
// is that, and how big is that. Both are READ-ONLY — they answer and change
// nothing — which is why both are `Flags::ReadOnly` and neither occupies an undo
// step. A measurement that landed in the journal as an edit would make Ctrl+Z
// undo a question.
//
// THE NUMBERS COME FROM THE DOCUMENT, not from the screen. A length is computed
// over stored millimetres and an area over rings with sign by role (model.md
// R12), so what the readout says is what an export would say — a measurement
// taken off pixel positions would disagree with the tapu by whatever the zoom
// happened to be.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// A length in metres, written the way a surveyor reads one off an instrument:
/// three decimals, which is the millimetre the document stores.
std::string metres(core::Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);

    std::string out = std::to_string(abs_mm / 1000) + "," +
                      std::string(3 - std::to_string(abs_mm % 1000).size(), '0') +
                      std::to_string(abs_mm % 1000);
    return (negative ? "-" : "") + out + " m";
}

/// An area in square metres, to two decimals — the precision a tapu carries.
std::string square_metres(core::Mm2 v)
{
    // Divided in integers so the printed figure is the stored one rounded, never
    // a double that lost a digit on the way (Article 2.4).
    const bool negative = v < 0;
    const auto abs_mm2  = static_cast<std::uint64_t>(negative ? -v : v);

    const std::uint64_t cm2 = (abs_mm2 + 5000) / 10000; // hundredths of a square metre
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;

    return (negative ? "-" : "") + std::to_string(cm2 / 100) + "," + frac + " m²";
}

/// The preview a measuring point prompt carries: the run so far, and the next
/// segment to the cursor, measured as it moves.
PointOptions measuring(const std::vector<core::Point2>& run, RubberShape shape)
{
    PointOptions o;
    if (run.empty()) return o;
    o.rubber_band   = true;
    o.rubber_origin = run.back();
    o.rubber_shape  = shape;
    o.rubber_chain  = run;
    return o;
}

Task<void> run_measure(Context& ctx)
{
    // A RUN, NOT A PAIR. ÖLÇ measured one segment and stopped, so the three
    // sides of a building, a boundary with six breaks or a pipe's route were
    // measured one ÖLÇ at a time and added up by hand. The first two points
    // still give the one-segment answer they always gave; every point after
    // that adds a segment and the running total, until Enter.
    auto a = co_await ctx.point("baslangic", "Ölçümün ilk noktası");
    if (!a) co_return;
    std::vector<core::Point2> run{*a};

    auto b = co_await ctx.point("bitis", "Ölçümün ikinci noktası",
                                measuring(run, RubberShape::MeasureRun));
    if (!b) co_return;
    run.push_back(*b);

    // The angle is written the way the session reads one: in `core.aci.birim`,
    // under `core.aci.kural` — by default the `semt açısı` a Turkish instrument
    // shows, clockwise from north in grad — and the line says which rule it
    // followed, so a figure copied into a kroki cannot be mistaken for the other
    // convention. What `@mesafe<açı` reads and what ÖLÇ writes are one setting
    // pair (TODOS-CAD P0-4), through the same core function the canvas and
    // APLİKASYON use.
    const core::AngleConvention convention = ctx.session().bus().angle_convention();
    const auto bearing                     = [&convention](core::Point2 from, core::Point2 to) {
        return core::angle_text(core::direction_turns(from, to, convention.rule), convention.unit);
    };

    const core::Mm first = core::segment_length(*a, *b);
    ctx.echo("Mesafe: " + metres(first) + "   ΔY: " + metres(b->x - a->x) +
             "   ΔX: " + metres(b->y - a->y) + "   Açı: " + bearing(*a, *b) + " (" +
             core::angle_rule_label(convention.rule) + ")");

    std::vector<core::Mm> sides{first};
    core::Mm total = first;
    for (;;) {
        auto next = co_await ctx.point("devam", "Sonraki nokta (Enter bitirir)",
                                       measuring(run, RubberShape::MeasureRun));
        if (!next) break;
        const core::Mm side = core::segment_length(run.back(), *next);
        total += side;
        ctx.echo("Kenar " + std::to_string(sides.size() + 1) + ": " + metres(side) +
                 "   Açı: " + bearing(run.back(), *next) + "   Toplam: " + metres(total));
        sides.push_back(side);
        run.push_back(*next);
    }
    if (sides.size() > 1)
        ctx.echo("Toplam uzunluk: " + metres(total) + "   (" + std::to_string(sides.size()) +
                 " kenar)");

    // THE SAME FIGURES FOR A CLIENT THAT READS DATA, and the same run for one
    // that has a canvas to leave it on.
    core::Json lengths = core::Json::array({});
    for (const core::Mm side : sides)
        lengths.push(core::Json::integer(side));
    core::Json report;
    report.set("kenarlar_mm", std::move(lengths));
    report.set("toplam_mm", core::Json::integer(total));
    ctx.report(std::move(report));

    MeasureMark mark{.shape = MeasureMark::Shape::Run, .points = run, .labels = {}};
    for (const core::Mm side : sides)
        mark.labels.push_back(metres(side));
    if (sides.size() > 1) mark.labels.push_back("toplam " + metres(total));
    ctx.mark(std::move(mark));

    ctx.record("baslangic", Value::point(*a));
    ctx.record("bitis", Value::point(*b));
}

/// The corners of the first closed ring of `slot`, when it has one.
std::vector<core::Point2> closed_ring(const core::Document& doc, core::EntityId slot)
{
    std::vector<core::Point2> out;
    if (doc.entities().kind[slot] != core::kPolylineKind) return out;
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count == 0 || doc.geometry().ring_role[span.first] == core::RingRole::Open) return out;
    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(core::Point2{xs[v], ys[v]});
    return out;
}

/// ALANÖLÇ yontem=nokta: the face the user points out, measured as it is drawn.
Task<void> measure_by_corners(Context& ctx)
{
    // A FACE THAT IS NOT IN THE DRAWING. The area of a yard between two
    // buildings, of the part of a parcel a road will take, of a field someone
    // paced out: none of them is an object, and ALANÖLÇ measured objects only.
    // The corners are pointed at, the face and its area follow the cursor, and
    // Enter answers.
    std::vector<core::Point2> ring;
    for (;;) {
        const char* asked = "Sonraki köşe (Enter bitirir)";
        if (ring.empty())
            asked = "Ölçülecek alanın ilk köşesi";
        else if (ring.size() < 3)
            asked = "Sonraki köşe";
        auto corner =
            co_await ctx.point("noktalar", asked, measuring(ring, RubberShape::MeasureRing));
        if (!corner) break;
        ring.push_back(*corner);
    }
    if (ring.size() < 3) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Alan ölçmek için en az üç köşe gerekir; " +
                                                         std::to_string(ring.size()) +
                                                         " köşe verildi.");
        co_return;
    }

    const core::Mm2 signed_area = core::ring_area(ring);
    const core::Mm2 area        = signed_area < 0 ? -signed_area : signed_area;
    core::Mm perimeter          = 0;
    for (std::size_t i = 0; i < ring.size(); ++i)
        perimeter += core::segment_length(ring[i], ring[(i + 1) % ring.size()]);

    ctx.echo("Alan: " + square_metres(area) + "   çevre: " + metres(perimeter) + "   (" +
             std::to_string(ring.size()) + " köşe)");

    core::Json report;
    report.set("alan_mm2", core::Json::integer(area));
    report.set("cevre_mm", core::Json::integer(perimeter));
    report.set("kose", core::Json::integer(static_cast<std::int64_t>(ring.size())));
    ctx.report(std::move(report));

    ctx.mark(MeasureMark{.shape  = MeasureMark::Shape::Ring,
                         .points = ring,
                         .labels = {square_metres(area) + " · çevre " + metres(perimeter)}});
    ctx.record("yontem", Value::text("nokta"));
}

Task<void> run_measure_area(Context& ctx)
{
    // BY CORNERS when asked for, or when corners were handed over: a script's
    // `noktalar` says which of the two it means without naming the method.
    const std::string method = ctx.argument("yontem").as_text();
    if (method == "nokta" || (method.empty() && !ctx.argument("noktalar").empty())) {
        co_await measure_by_corners(ctx);
        co_return;
    }
    if (!method.empty() && method != "nesne") {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Tanınmayan yöntem: '" + method + "'. Yöntemler: nesne / nokta");
        co_return;
    }

    // The argument, the selection, or ASKED FOR — the order every modify tool
    // uses (`want_objects`), so the tool-column button arms and asks instead of
    // refusing when nothing is highlighted.
    std::vector<std::int64_t> requested;
    if (!co_await want_objects(ctx, "nesneler", "Ölçülecek nesneleri seçin, sonra Enter", requested,
                               0, "ALANÖLÇ nesneler=1"))
        co_return;

    const core::Document& doc = ctx.document();
    core::Mm2 total{0};
    std::size_t counted = 0;

    for (std::int64_t raw : requested) {
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

        // THE KIND ANSWERS, so a circle reports pi*r² rather than the area of the
        // polygon it happens to be drawn with, and an arc reports nothing because
        // it encloses nothing.
        const std::uint32_t gslot = doc.entities().slot[slot];
        const core::KindId kind   = doc.entities().kind[slot];
        core::Mm2 area{0};
        core::Mm perimeter{0};

        // THE KIND ANSWERS FOR BOTH. Summing the stored run gives a CIRCLE's
        // radius, not its circumference — an eight-metre circle reported "çevre:
        // 8,000 m" — because a circle stores its centre and one point at radius
        // distance. A kind registered before `perimeter` existed has none, and
        // then the run's own length is the right answer anyway.
        perimeter = doc.geometry().perimeter_of(gslot);
        if (const core::KindSpec* spec = core::builtin_kinds().find(kind); spec != nullptr) {
            const std::uint32_t one[1]{gslot};
            spec->area(doc.geometry(), core::SlotSpan(one, 1), std::span<core::Mm2>(&area, 1));
            if (spec->perimeter != nullptr)
                spec->perimeter(doc.geometry(), core::SlotSpan(one, 1),
                                std::span<core::Mm>(&perimeter, 1));
        }

        // AN OPEN LINE HAS A LENGTH, NOT AN AREA. It used to be reported as
        // "alan: 0,00 m²", which reads as a measured empty parcel rather than as
        // the wrong question — and the figure the user did want, the length,
        // was printed under the name `çevre`.
        const core::Box2 box = doc.entities().box_of(slot);
        const core::Point2 middle{box.min_x + (box.max_x - box.min_x) / 2,
                                  box.min_y + (box.max_y - box.min_y) / 2};
        if (area == 0) {
            ctx.echo("Nesne " + std::to_string(raw) +
                     " — kapalı değil, alanı yok; uzunluk: " + metres(perimeter));
            ctx.mark(MeasureMark{.shape  = MeasureMark::Shape::Point,
                                 .points = {middle},
                                 .labels = {"uzunluk " + metres(perimeter)}});
            continue;
        }

        ctx.echo("Nesne " + std::to_string(raw) + " — alan: " + square_metres(area) +
                 "   çevre: " + metres(perimeter));
        const std::string label = square_metres(area) + " · çevre " + metres(perimeter);
        if (std::vector<core::Point2> ring = closed_ring(doc, slot); ring.size() >= 3)
            ctx.mark(MeasureMark{
                .shape = MeasureMark::Shape::Ring, .points = std::move(ring), .labels = {label}});
        else
            ctx.mark(MeasureMark{
                .shape = MeasureMark::Shape::Point, .points = {middle}, .labels = {label}});

        total += area;
        ++counted;
    }

    if (counted > 1) ctx.echo("Toplam alan: " + square_metres(total));

    ctx.record("nesneler", Value::ids(requested));
}

/// KOORDİNAT — read one point off the drawing.
///
/// The reading a surveyor takes before anything else, and the one the tool column
/// offered as a disabled button for a phase: where IS this. It is `ÖLÇ` with one
/// point instead of two, and it reports in the document's own CRS rather than in
/// whatever the view happens to be showing, because the number a user writes down
/// belongs to the drawing and not to the window.
Task<void> run_coordinate(Context& ctx)
{
    auto at = co_await ctx.point("nokta", "Okunacak nokta");
    if (!at) co_return; // ESC before anything was read

    const std::string crs = ctx.document().crs().id();

    // A LOCAL drawing's numbers are right about the site and mean nothing to
    // anyone else, so the reading says so rather than printing a bare pair a user
    // might write into a tapu.
    const bool local = crs == "YEREL" || crs == "LOCAL";
    const std::string where =
        local ? "   (YEREL — haritaya oturtulmadı)" : (crs.empty() ? "" : "   (" + crs + ")");

    // SAĞA / YUKARI, which is what a Turkish surveyor calls easting and northing,
    // and the order a TUCBS record writes them in — to the decimals the project
    // writes its coordinate tables with (`core.crs.hassasiyet`), which is what
    // the number copied off this line will be compared against.
    const int decimals =
        static_cast<int>(ctx.session().bus().setting("core.crs.hassasiyet").as_int());
    const auto reading = [decimals](core::Mm v) {
        return core::metres_fixed(v, decimals, ',') + " m";
    };
    ctx.echo("Sağa: " + reading(at->x) + "   Yukarı: " + reading(at->y) + where);

    // AND THE READING STAYS WHERE IT WAS TAKEN, in the Y/X a surveyor writes.
    ctx.mark(MeasureMark{.shape  = MeasureMark::Shape::Point,
                         .points = {*at},
                         .labels = {"Y " + reading(at->x) + "  X " + reading(at->y)}});

    ctx.record("nokta", Value::point(*at));
}

} // namespace

KENTOS_COMMAND(measure)
{
    return CommandSpec{
        .id       = "core.measure",
        .names    = {"ÖLÇ", "OLC", "MEASURE", "MS"},
        .title    = "Ölç",
        .category = Category::Query,
        .params =
            {
                Param::point("baslangic", "Ölçümün ilk noktası").en("start"),
                Param::point("bitis", "Ölçümün ikinci noktası").en("end"),
                Param::points("devam", Arity::at_least(0),
                              "Sonraki noktalar: her biri bir kenar daha ekler, toplam da yazılır")
                    .en("more"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla "
                   "nokta kenarları ve toplam uzunluğu verir.",
        .run = &run_measure,
    };
}

KENTOS_COMMAND(measure_area)
{
    return CommandSpec{
        .id       = "core.measure_area",
        .names    = {"ALANÖLÇ", "ALANOLC", "AREAOF", "AÖ"},
        .title    = "Alan Ölç",
        .category = Category::Query,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim"}
                         .en("objects"),
                     Param::choice("yontem", Arity::optional(), {"nesne", "nokta"},
                                   "nesne: seçilen nesnelerin alanı (öntanımlı); nokta: "
                                     "köşeleri gösterilen alan")
                         .en("method"),
                     Param::points("noktalar", Arity::at_least(0),
                                   "yontem=nokta için alanın köşeleri; verilirse yöntem "
                                     "kendiliğinden nokta olur")
                         .en("points")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini "
                   "yazar.",
        .run = &run_measure_area,
    };
}

KENTOS_COMMAND(coordinate)
{
    return CommandSpec{
        .id       = "core.coordinate",
        .names    = {"KOORDİNAT", "KOORDINAT", "COORDINATE", "KRD"},
        .title    = "Koordinat Oku",
        .category = Category::Query,
        .params   = {Param::point("nokta", "Okunacak nokta").en("point")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.",
        .run = &run_coordinate,
    };
}

} // namespace kentos::command
