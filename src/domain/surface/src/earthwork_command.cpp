// SPDX-License-Identifier: GPL-3.0-or-later
// core.earthwork — HACİM. Cut and fill against a reference level.
//
// The two numbers a site earthwork report is built on: how much has to come off,
// and how much has to go on.
//
// THEY ARE REPORTED SEPARATELY AND NEVER NETTED. A site that is 500 m³ cut and
// 500 m³ fill is a week of machine time; a site whose net is zero because nothing
// moves is none. The machines are hired against the first pair of numbers, and a
// command that printed only their difference would be reporting the wrong thing
// with perfect arithmetic.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"
#include "kentos_cad/domain/surface/contour.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// Cubic metres to two decimals, in integers.
std::string cubic_metres(core::Mm3 v)
{
    const bool negative = v < 0;
    const auto abs_mm3  = static_cast<unsigned long long>(negative ? -v : v);

    // 1 m³ is 10^9 mm³; two decimals is a hundredth of that.
    const unsigned long long hundredths = (abs_mm3 + 5000000ULL) / 10000000ULL;
    std::string frac                    = std::to_string(hundredths % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return (negative ? "-" : "") + std::to_string(hundredths / 100) + "," + frac + " m³";
}

std::string square_metres(core::Mm2 v)
{
    const auto cm2   = static_cast<unsigned long long>((v < 0 ? -v : v) + 5000) / 10000;
    std::string frac = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return std::to_string(cm2 / 100) + "," + frac + " m²";
}

std::string metres(core::Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
}

Task<void> run(Context& ctx)
{
    if (!domain::surface::available()) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Üçgenleme bu yapıda yok; hacim hesaplanamaz. KENTOS_WITH_CDT=ON ile derleyin.");
        co_return;
    }

    // THE LEVEL, NAMED OR ASKED FOR. The Yüzey menu's entry used to answer
    // "zorunlu 'kot' parametresi eksik" and stop, which is a sentence for a
    // programmer. Asked in METRES, the unit an engineer reads a level in; a
    // named `kot` is the declared millimetres and is never reread as metres,
    // because a factor of a thousand in a level is a site that moves a kilometre.
    core::Mm level = 0;
    if (const Value given = ctx.argument("kot"); !given.empty()) {
        level = static_cast<core::Mm>(given.as_int());
    } else {
        auto typed = co_await ctx.number("kot", "Karşılaştırma kotu (metre)");
        if (!typed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Karşılaştırma kotu eksik. Örnek: HACİM kot=845000 (845 m)");
            co_return;
        }
        level = core::mm_round(*typed * static_cast<double>(core::kMmPerMetre));
    }

    const core::Document& doc    = ctx.document();
    const core::AttrTable& table = doc.attributes();
    const core::AttrId kot       = table.find("kot");
    if (kot == core::kNoAttr) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Çizimde 'kot' sütunu yok. Kotlu bir nokta listesini NOKTALAR ile okuyun.");
        co_return;
    }

    std::vector<core::EntityId> slots;
    for (core::EntityKey k : ctx.session().bus().selection().keys()) {
        const core::EntityId slot = doc.slot_of(k);
        if (slot != core::kNoEntity && doc.alive(slot)) slots.push_back(slot);
    }
    if (slots.empty())
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kPointKind) slots.push_back(e);

    std::vector<domain::surface::Level> levels;
    levels.reserve(slots.size());
    for (core::EntityId slot : slots) {
        auto height = doc.attribute(kot, slot);
        if (!height || !height.value().present) continue; // not levelled, not used

        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
        if (span.count == 0) continue;
        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        if (xs.empty()) continue;

        levels.push_back(domain::surface::Level{core::Point2{xs[0], ys[0]},
                                                static_cast<core::Mm>(height.value().number)});
    }

    if (levels.size() < 3) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kotlu nokta sayısı yetersiz: " + std::to_string(levels.size()) +
                       ". Hacim hesabı en az üç kotlu nokta ister.");
        co_return;
    }

    // LONG WORK, as the contour command's is (command/job.hpp, TODOS F-05).
    core::Result<domain::surface::Earthwork> computed =
        core::err(core::ErrorCode::Internal, "Hacim hesabı başlamadı.");
    Job job;
    job.label = "Hacim hesabı";
    job.work  = [&](const JobControl& control) {
        computed = domain::surface::earthwork(levels, level, control);
    };
    co_await run_job(ctx.session(), job);
    if (job.stop.stop_requested() ||
        (!computed && computed.error().code == core::ErrorCode::Cancelled)) {
        ctx.session().end_stopped();
        ctx.echo("Hacim hesabı durduruldu; sonuç verilmedi.");
        co_return;
    }
    if (!computed) {
        ctx.refuse(computed.error());
        co_return;
    }
    const domain::surface::Earthwork& work = computed.value();

    // SEPARATE LINES, and the net LAST and labelled. Reading the net first is how
    // a balanced site gets mistaken for a site with no work in it.
    std::string report = "Hacim — karşılaştırma kotu " + metres(level) + " m\n";
    report += "  kazı  (kotun üstünde): " + cubic_metres(work.cut) + "\n";
    report += "  dolgu (kotun altında): " + cubic_metres(work.fill) + "\n";
    report += "  fark (kazı - dolgu):   " + cubic_metres(work.cut - work.fill) + "\n";
    report += "  hesap alanı: " + square_metres(work.area) + ", " + std::to_string(levels.size()) +
              " kotlu noktadan.";
    ctx.echo(report);

    ctx.record("kot", Value::integer(level));
}

} // namespace

KENTOS_COMMAND(earthwork)
{
    return CommandSpec{
        .id       = "core.earthwork",
        .names    = {"HACİM", "HACIM", "EARTHWORK", "HCM"},
        .title    = "Hacim Hesabı",
        .category = Category::Query,
        .params   = {Param::integer("kot", Arity::exactly(1),
                                    "Karşılaştırma kotu, milimetre (845 m = 845000)")
                         .en("elevation")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly |
                 Flags::LongRunning,
        .summary = "Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar.",
        .run     = &run,
    };
}

} // namespace kentos::command
