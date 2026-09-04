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

#include "kentos_cad/core/entity_kind.hpp"
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

Task<void> run_measure(Context& ctx)
{
    auto a = co_await ctx.point("baslangic", "Ölçümün ilk noktası");
    if (!a) co_return;

    auto b = co_await ctx.point("bitis", "Ölçümün ikinci noktası",
                                PointOptions{.rubber_band = true, .rubber_origin = *a});
    if (!b) co_return;

    const double dx = core::mm_to_metres(b->x - a->x);
    const double dy = core::mm_to_metres(b->y - a->y);
    const core::Mm distance =
        core::mm_round(std::sqrt(dx * dx + dy * dy) * static_cast<double>(core::kMmPerMetre));

    // The bearing a surveyor reads: clockwise FROM NORTH, which is the convention
    // every Turkish instrument and every ölçü krokisi uses — not the
    // counter-clockwise-from-east of the mathematics underneath.
    double bearing = std::atan2(dx, dy) * (180.0 / 3.14159265358979323846);
    if (bearing < 0.0) bearing += 360.0;

    ctx.echo("Mesafe: " + metres(distance) + "   ΔY: " + metres(b->x - a->x) +
             "   ΔX: " + metres(b->y - a->y) + "   Açı: " + std::to_string(bearing).substr(0, 6) +
             "° (kuzeyden saat yönünde)");

    ctx.record("baslangic", Value::point(*a));
    ctx.record("bitis", Value::point(*b));
}

Task<void> run_measure_area(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    std::vector<std::int64_t> requested;
    if (const Value given = ctx.argument("nesneler"); !given.empty()) {
        requested = given.as_ids();
    } else {
        for (core::EntityKey k : bus.selection().keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

        if (requested.empty()) {
            ctx.echo("Ölçülecek nesne belirtilmedi ve seçim boş. Örnek: ALANÖLÇ nesneler=1");
            co_return;
        }
    }

    const core::Document& doc = ctx.document();
    core::Mm2 total{0};
    std::size_t counted = 0;

    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            co_return;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }

        // THE KIND ANSWERS, so a circle reports pi*r² rather than the area of the
        // polygon it happens to be drawn with, and an arc reports nothing because
        // it encloses nothing.
        const std::uint32_t gslot = doc.entities().slot[slot];
        const core::KindId kind   = doc.entities().kind[slot];
        core::Mm2 area{0};
        core::Mm perimeter{0};

        if (const core::KindSpec* spec = core::builtin_kinds().find(kind); spec != nullptr) {
            const std::uint32_t one[1]{gslot};
            spec->area(doc.geometry(), core::SlotSpan(one, 1), std::span<core::Mm2>(&area, 1));
        }
        perimeter = doc.geometry().perimeter_of(gslot);

        ctx.echo("Nesne " + std::to_string(raw) + " — alan: " + square_metres(area) +
                 "   çevre: " + metres(perimeter));

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
    // and the order a TUCBS record writes them in.
    ctx.echo("Sağa: " + metres(at->x) + "   Yukarı: " + metres(at->y) + where);

    ctx.record("nokta", Value::point(*at));
}

} // namespace

KENTOS_COMMAND(measure)
{
    return CommandSpec{
        .id       = "core.measure",
        .names    = {"ÖLÇ", "OLC", "MEASURE", "MS"},
        .category = Category::Query,
        .params =
            {
                Param::point("baslangic", "Ölçümün ilk noktası"),
                Param::point("bitis", "Ölçümün ikinci noktası"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı yazar.",
        .run     = &run_measure,
    };
}

KENTOS_COMMAND(measure_area)
{
    return CommandSpec{
        .id       = "core.measure_area",
        .names    = {"ALANÖLÇ", "ALANOLC", "AREAOF", "AÖ"},
        .category = Category::Query,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim"}},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary  = "Seçilen nesnelerin alanını ve çevresini yazar.",
        .run      = &run_measure_area,
    };
}

KENTOS_COMMAND(coordinate)
{
    return CommandSpec{
        .id       = "core.coordinate",
        .names    = {"KOORDİNAT", "KOORDINAT", "COORDINATE", "KRD"},
        .category = Category::Query,
        .params   = {Param::point("nokta", "Okunacak nokta")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.",
        .run     = &run_coordinate,
    };
}

} // namespace kentos::command
