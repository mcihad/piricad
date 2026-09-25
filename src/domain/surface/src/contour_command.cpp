// SPDX-License-Identifier: GPL-3.0-or-later
// core.contour — EŞYÜKSELTİ. The contour lines a plan sheet carries.
//
// A crew levels a site and comes back with a few hundred numbered points, each
// with a Z. This turns them into the lines a designer reads the ground from.
//
// THE HEIGHTS COME FROM THE `kot` COLUMN, which is what `NOKTALAR` fills when it
// reads a field list. A point with no `kot` is not levelled and is left out
// rather than treated as zero — a sea-level point in the middle of a hillside
// would drag every contour around it.
//
// THE TRIANGULATION IS NOT STORED. It is an intermediate; keeping it would add an
// entity kind the whole program has to learn about for a thing nobody draws.
//
// THE CONTOURS ARE A RESULT (TODOS F-04, core/lineage.hpp): every line records
// the points it was traced from and what each one said, once for the run. A
// point moved, a height re-read, a point erased — and every contour of the run
// says it is out of date, because a surface is one thing and any of its points
// can bend any of its lines.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/units.hpp"
#include "kentos_cad/domain/surface/contour.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

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
                   "Üçgenleme bu yapıda yok; eş yükselti eğrisi çizilemez. "
                   "KENTOS_WITH_CDT=ON ile derleyin.");
        co_return;
    }

    const Value given = ctx.argument("aralik");
    core::Mm interval = 1000; // one metre, the interval a 1/1000 sheet usually carries
    if (!given.empty()) interval = static_cast<core::Mm>(given.as_int());
    if (interval <= 0) {
        ctx.refuse(
            core::ErrorCode::InvalidArgument,
            "Eş yükselti aralığı sıfırdan büyük olmalı. Örnek: EŞYÜKSELTİ aralik=500 (0,5 m)");
        co_return;
    }

    const core::Document& doc    = ctx.document();
    const core::AttrTable& table = doc.attributes();
    const core::AttrId kot       = table.find("kot");
    if (kot == core::kNoAttr) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Çizimde 'kot' sütunu yok. Kotlu bir nokta listesini NOKTALAR ile okuyun.");
        co_return;
    }

    // What to level from: the points named, else the selection when there is
    // one, otherwise every point in the drawing that carries a height.
    std::vector<core::EntityId> slots;
    bool from_selection = false;
    if (const Value named = ctx.argument("nesneler"); !named.empty()) {
        for (const std::int64_t id : named.as_ids()) {
            const core::EntityId slot =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(id)));
            if (slot == core::kNoEntity || !doc.alive(slot)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
                co_return;
            }
            slots.push_back(slot);
        }
    } else {
        for (core::EntityKey k : ctx.session().bus().selection().keys()) {
            const core::EntityId slot = doc.slot_of(k);
            if (slot != core::kNoEntity && doc.alive(slot)) slots.push_back(slot);
        }
        from_selection = !slots.empty();
    }
    if (slots.empty())
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kPointKind) slots.push_back(e);

    std::vector<domain::surface::Level> levels;
    levels.reserve(slots.size());
    for (core::EntityId slot : slots) {
        auto height = doc.attribute(kot, slot);
        // NOT LEVELLED, NOT USED. Treating a missing height as zero would drag
        // every contour near it down to sea level.
        if (!height || !height.value().present) continue;

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
                       ". Yüzey en az üç kotlu nokta ister.");
        co_return;
    }

    // LONG WORK, OFF THE UI THREAD when the session can be resumed
    // (command/job.hpp, TODOS F-05): the triangulation and the tracing read
    // only the levels gathered above, and the lines are drawn below, here, in
    // the command's one transaction.
    core::Result<std::vector<domain::surface::Contour>> traced =
        core::err(core::ErrorCode::Internal, "Eş yükselti hesabı başlamadı.");
    Job job;
    job.label = "Eş yükselti eğrileri";
    job.work  = [&](const JobControl& control) {
        traced = domain::surface::trace_contours(levels, interval, control);
    };
    co_await run_job(ctx.session(), job);
    if (job.stop.stop_requested() ||
        (!traced && traced.error().code == core::ErrorCode::Cancelled)) {
        ctx.session().end_stopped();
        ctx.echo("Eş yükselti çizimi durduruldu; çizim değişmedi.");
        co_return;
    }
    if (!traced) {
        ctx.refuse(traced.error());
        co_return;
    }
    if (traced.value().empty()) {
        ctx.echo("Bu aralıkta eş yükselti eğrisi yok: arazinin kot farkı aralıktan küçük.");
        co_return;
    }

    // The lines land on their own layer so they can be styled and switched off as
    // a set — which is what a plan sheet does with them.
    const Value layer_arg = ctx.argument("katman");
    const std::string layer_name =
        layer_arg.empty() ? std::string("ESYUKSELTI") : layer_arg.as_text();

    const command::LayerId layer = ctx.transaction().ensure_layer(layer_name);

    // The height goes on every line as an attribute, because a contour without its
    // level is a line nobody can label.
    core::AttrId height_column = table.find("kot");

    std::size_t drawn = 0;
    std::vector<core::EntityId> lines;
    lines.reserve(traced.value().size());
    for (const domain::surface::Contour& c : traced.value()) {
        // CLOSED IS NOT THE SAME AS ENCLOSING. A run whose two ends meet but whose
        // area is zero is a line that went out and came back along itself — which
        // happens where a level lies exactly along a row of levelled points, and a
        // grid of them on a planned slope does that at every whole metre. Handing
        // it to `add_area` gets it refused, correctly, as a zero-area face; it is
        // a polyline, and drawing it as one is the honest answer.
        const core::Mm2 enclosed = core::ring_area(c.path);
        const bool is_face       = c.closed && (enclosed > 0 || enclosed < 0);

        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{c.path, core::RingRole::Exterior, 0});

        auto created = is_face ? ctx.transaction().add_area(layer, rings)
                               : ctx.transaction().add_polyline(layer, c.path);
        if (!created) {
            ctx.refuse(created.error());
            co_return; // the bus rolls the whole set back
        }

        if (auto st = ctx.transaction().set_attribute(height_column, created.value(),
                                                      core::attr_mm(c.height));
            !st) {
            ctx.refuse(st.error());
            co_return;
        }
        lines.push_back(created.value());
        ++drawn;
    }

    // WHAT THEY WERE TRACED FROM: every point the run looked at, a point with
    // no height included — reading one in later changes the surface as surely
    // as moving one does.
    std::vector<core::EntityKey> read;
    read.reserve(slots.size());
    for (const core::EntityId slot : slots)
        read.push_back(doc.key_of(slot));
    // And how they were traced, so the run can be traced again (TODOS F-04).
    Args traced_with;
    traced_with.set("aralik", Value::integer(interval));
    if (!layer_arg.empty()) traced_with.set("katman", layer_arg);
    if (auto st = ctx.derive_results(lines, read, &traced_with); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    ctx.record("aralik", Value::integer(interval));
    if (!layer_arg.empty()) ctx.record("katman", layer_arg);
    // THE SELECTION IT READ, named: a replay runs with no selection, and would
    // otherwise trace every point in the drawing.
    if (from_selection) {
        Value::Ints keys;
        for (const core::EntityId slot : slots)
            keys.push_back(static_cast<std::int64_t>(core::raw(doc.key_of(slot))));
        ctx.record("nesneler", Value::ids(std::move(keys)));
    }

    ctx.echo(std::to_string(drawn) + " eş yükselti eğrisi çizildi (" + metres(interval) +
             " m aralıkla, " + std::to_string(levels.size()) + " kotlu noktadan), '" + layer_name +
             "' katmanına.");
}

} // namespace

KENTOS_COMMAND(contour)
{
    return CommandSpec{
        .id       = "core.contour",
        .names    = {"EŞYÜKSELTİ", "ESYUKSELTI", "CONTOUR", "EŞY"},
        .title    = "Eşyükselti Eğrileri",
        .category = Category::Draw,
        .params =
            {
                Param::integer("aralik", Arity::optional(),
                               "Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m)")
                    .en("interval"),
                Param::text("katman", Arity::optional(),
                            "Eğrilerin çizileceği katman; varsayılan ESYUKSELTI")
                    .en("layer"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kotlu noktalar; verilmezse seçim, o da boşsa çizimdeki bütün noktalar"}
                    .en("objects"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible | Flags::LongRunning,
        .summary = "Kotlu noktalardan eş yükselti eğrileri çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
