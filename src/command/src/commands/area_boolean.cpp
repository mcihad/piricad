// SPDX-License-Identifier: GPL-3.0-or-later
// Generic face booleans, separate from legal parcel operations and line joins.
// All geometry goes through OCCT, including straight boundaries. Inputs enter
// as exact paths; only the final result is rounded and written through the bus.
#include "piricad/command/area_face.hpp"
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/job.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"
#include "piricad/command/transaction.hpp"

#include "piricad/core/curve_path.hpp"
#include "piricad/core/kernel.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

struct Operation
{
    core::BooleanOp op;
    const char* id;
    std::vector<std::string> names;
    const char* title;
    const char* help;
};

Operation words(core::BooleanOp op)
{
    switch (op) {
    case core::BooleanOp::Union:
        return {op,
                "core.area_union",
                {"BİRLEŞİM", "BIRLESIM", "UNION", "ABR"},
                "Birleşim",
                "Kapalı alanların bütününü OpenCASCADE ile birleştirir; ayrı parçaları korur."};
    case core::BooleanOp::Intersection:
        return {op,
                "core.area_intersection",
                {"KESİŞİM", "KESISIM", "INTERSECTION", "AKS"},
                "Kesişim",
                "İki kapalı alanın ortak bölgesini OpenCASCADE ile oluşturur."};
    case core::BooleanOp::Difference:
        return {op,
                "core.area_difference",
                {"FARK", "DIFFERENCE", "SUBTRACT", "AFR"},
                "Fark",
                "İlk seçilen kapalı alandan diğerlerini OpenCASCADE ile çıkarır; sıra önemlidir."};
    case core::BooleanOp::SymmetricDifference:
        return {op,
                "core.area_symdifference",
                {"SİMETRİKFARK", "SIMETRIKFARK", "SYMDIFFERENCE", "XOR", "ASF"},
                "Simetrik Fark",
                "İki kapalı alanın yalnız birine ait bölgeleri oluşturur; "
                "ortak bölgeyi OpenCASCADE ile çıkarır."};
    }
    return words(core::BooleanOp::Union);
}

bool copy_attributes(Context& ctx, std::span<const core::EntityId> sources, core::EntityId result,
                     std::set<std::string>& differing)
{
    const auto& table = ctx.document().attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto id      = static_cast<core::AttrId>(c);
        const auto* column = table.column(id);
        if (column == nullptr) continue;
        auto value = ctx.document().attribute(id, sources.front());
        if (!value) {
            ctx.refuse(value.error());
            return false;
        }
        bool equal = true;
        for (const auto source : sources.subspan(1)) {
            auto next = ctx.document().attribute(id, source);
            if (!next) {
                ctx.refuse(next.error());
                return false;
            }
            equal = equal && next.value() == value.value();
        }
        if (!equal) {
            differing.insert(column->spec().id);
            continue;
        }
        if (value.value().present)
            if (auto written = ctx.transaction().set_attribute(id, result, value.value());
                !written) {
                ctx.refuse(written.error());
                return false;
            }
    }
    return true;
}

Task<void> run_boolean(Context& ctx, core::BooleanOp op)
{
    const auto operation = words(op);
    const bool pair =
        op == core::BooleanOp::Intersection || op == core::BooleanOp::SymmetricDifference;
    const bool supplied = ctx.has_argument("nesneler");
    std::vector<std::int64_t> requested;
    const std::string prompt =
        op == core::BooleanOp::Difference
            ? "Önce tutulacak alanı, sonra çıkarılacak alanları seçin; Enter'a basın"
            : std::string(operation.title) + " için kapalı alanları seçin; Enter'a basın";
    if (!co_await want_objects(ctx, "nesneler", prompt, requested, pair ? 2 : 0)) co_return;
    // A selected parcel can start the contextual tool on its own. Ask for
    // the remaining input rather than making that button fail immediately.
    const bool singlePick = !supplied && requested.size() == 1;
    if (singlePick) {
        auto more =
            co_await ctx.objects("nesneler",
                                 op == core::BooleanOp::Difference
                                     ? "Seçili alandan çıkarılacak alanları seçin; Enter'a basın"
                                     : "İşleme katılacak diğer alanı seçin; Enter'a basın",
                                 core::kNoKind, pair ? 1 : 0);
        if (!more) co_return;
        requested.insert(requested.end(), more->begin(), more->end());
    }
    if (requested.size() < 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string(operation.title) + " en az iki kapalı alan ister.");
        co_return;
    }
    std::set<std::int64_t> seen;
    if (op == core::BooleanOp::Difference &&
        (ctx.has_argument("tutulan") || (!supplied && !singlePick))) {
        // The active selection is sorted by key; it has no picking order.
        // Never silently subtract from the oldest object in that selection.
        auto base = co_await ctx.objects(
            "tutulan", "Tutulacak alanı seçin; diğer alanlar bundan çıkarılacak", core::kNoKind, 1);
        if (!base) co_return;
        if (base->size() != 1 || std::ranges::find(requested, base->front()) == requested.end()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tutulacak alan işlemdeki alanlardan biri olmalı.");
            co_return;
        }
        const auto chosen = std::ranges::find(requested, base->front());
        std::rotate(requested.begin(), chosen, chosen + 1);
        ctx.record("tutulan", Value::ids(*base));
    }
    std::vector<core::EntityId> sources;
    std::vector<core::KernelFace> input;
    std::vector<std::int64_t> owners;
    std::size_t first_count = 0;
    for (const auto raw : requested) {
        if (raw <= 0 || !seen.insert(raw).second) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Nesne kimlikleri pozitif ve birbirinden farklı olmalı.");
            co_return;
        }
        const auto slot = ctx.document().slot_of(static_cast<core::EntityKey>(raw));
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound, "Nesne bulunamadı: " + std::to_string(raw));
            co_return;
        }
        auto faces      = area_faces(ctx.document(), slot);
        const auto kind = ctx.document().entities().kind[slot];
        if (!faces && (kind == core::kCircleKind || kind == core::kEllipseKind ||
                       kind == core::kSplineKind)) {
            auto path = core::path_of(ctx.document(), slot, core::PathScope::Curves);
            if (path && path->closed)
                faces =
                    std::vector<AreaFace>{AreaFace{core::KernelFace{std::move(*path), {}}, true}};
        }
        if (!faces) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(raw) +
                           " kapalı bir alan değil. Alan, kapalı yaylı çizgi veya daire seçin.");
            co_return;
        }
        sources.push_back(slot);
        for (auto& face : *faces) {
            input.push_back(std::move(face.face));
            owners.push_back(raw);
        }
        if (sources.size() == 1) first_count = input.size();
    }
    const bool keep = ctx.argument("kaynaklari_koru").as_bool();
    ctx.record("nesneler", Value::ids(requested));
    ctx.record("kaynaklari_koru", Value::boolean(keep));
    core::Result<std::vector<core::KernelFace>> result =
        core::err(core::ErrorCode::Internal, "Alan işlemi başlamadı.");
    Job job;
    job.label = operation.title;
    job.work  = [&](const JobControl& control) {
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (control.cancelled()) {
                result = core::err(core::ErrorCode::Cancelled, "Alan işlemi durduruldu.");
                return;
            }
            control.at(i, input.size(), 0, 100);
            const auto valid = core::kernel_face_issue(input[i]);
            if (!valid) {
                result = valid.error();
                return;
            }
            if (valid.value() != core::KernelFaceIssue::None) {
                result =
                    core::err(core::ErrorCode::InvalidArgument,
                              "Nesne " + std::to_string(owners[i]) +
                                  " geçersiz bir alan sınırı içeriyor; kaynaklar değiştirilmedi.");
                return;
            }
        }
        control.at(0, 1, 100, 950);
        result = core::kernel_boolean(std::span(input).first(first_count),
                                      std::span(input).subspan(first_count), op, control.stop);
        control.at(1, 1);
    };
    co_await run_job(ctx.session(), job);
    if (job.stop.stop_requested() ||
        (!result && result.error().code == core::ErrorCode::Cancelled)) {
        ctx.session().end_stopped();
        ctx.echo(std::string(operation.title) + " durduruldu; kaynaklar değiştirilmedi.");
        co_return;
    }
    if (!result) {
        ctx.refuse(result.error());
        co_return;
    }

    const auto layer = ctx.document().entities().layer[sources.front()];
    auto style       = ctx.document().entities().style[sources.front()];
    if (op != core::BooleanOp::Difference)
        for (const auto source : sources)
            if (ctx.document().entities().style[source] != style) style = core::kByLayerStyle;
    std::set<std::string> differing;
    core::Json created = core::Json::array({});
    core::Int128 total = 0;
    for (const auto& face : result.value()) {
        auto made = add_face(ctx, layer, face);
        if (!made) {
            ctx.refuse(made.error());
            co_return;
        }
        if (style != core::kByLayerStyle)
            if (auto written = ctx.transaction().set_entity_style(made.value(), style); !written) {
                ctx.refuse(written.error());
                co_return;
            }
        if (auto provenance = ctx.derive(made.value(), std::span<const core::EntityId>(sources));
            !provenance) {
            ctx.refuse(provenance.error());
            co_return;
        }
        const auto attributes =
            op == core::BooleanOp::Difference ? std::span(sources).first(1) : std::span(sources);
        if (!copy_attributes(ctx, attributes, made.value(), differing)) co_return;
        created.push(core::Json::integer(
            static_cast<std::int64_t>(core::raw(ctx.document().entities().key[made.value()]))));
        total += face_area(face);
    }
    if (total > std::numeric_limits<core::Mm2>::max()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Sonuç alanı raporun sayısal sınırını aşıyor; kaynaklar değiştirilmedi.");
        co_return;
    }
    // A mathematically empty answer is successful; it consumes no measured
    // input and cannot erase a drawing merely because the faces did not meet.
    if (!keep && !result.value().empty())
        for (const auto source : sources)
            if (auto erased = ctx.transaction().erase_entity(source); !erased) {
                ctx.refuse(erased.error());
                co_return;
            }
    core::Json report;
    report.set("islem", core::Json::string(operation.id));
    if (op == core::BooleanOp::Difference)
        report.set("tutulan", core::Json::integer(requested.front()));
    report.set("sonuclar", std::move(created));
    report.set("parca", core::Json::integer(static_cast<std::int64_t>(result.value().size())));
    report.set("alan_mm2", core::Json::integer(static_cast<core::Mm2>(total)));
    report.set("kaynaklar_korundu", core::Json::boolean(keep || result.value().empty()));
    report.set("geometri_cekirdegi", core::Json::string(core::kernel_version()));
    core::Json cleared = core::Json::array({});
    for (const auto& field : differing)
        cleared.push(core::Json::string(field));
    report.set("ayrisan_sutunlar", std::move(cleared));
    ctx.report(std::move(report));
    if (result.value().empty())
        ctx.echo(std::string(operation.title) + ": sonuç boş; kaynaklar korundu.");
    else {
        std::string said = std::string(operation.title) + ": " +
                           std::to_string(result.value().size()) + " alan oluşturuldu.";
        if (op == core::BooleanOp::Difference)
            said += " Tutulan kaynak: " + std::to_string(requested.front()) + ".";
        if (!differing.empty()) {
            said += " Ayrışan sütunlar boş bırakıldı:";
            for (const auto& field : differing)
                said += " " + field;
        }
        ctx.echo(std::move(said));
    }
}

Task<void> run_union(Context& ctx)
{
    return run_boolean(ctx, core::BooleanOp::Union);
}

Task<void> run_intersection(Context& ctx)
{
    return run_boolean(ctx, core::BooleanOp::Intersection);
}

Task<void> run_difference(Context& ctx)
{
    return run_boolean(ctx, core::BooleanOp::Difference);
}

Task<void> run_symdifference(Context& ctx)
{
    return run_boolean(ctx, core::BooleanOp::SymmetricDifference);
}

CommandSpec specification(core::BooleanOp op, Task<void> (*run)(Context&))
{
    auto operation = words(op);
    const bool pair =
        op == core::BooleanOp::Intersection || op == core::BooleanOp::SymmetricDifference;
    std::vector<Param> params{
        Param{"nesneler", ParamKind::Selection, Arity{0, pair ? 2u : 0xFFFFFFFFu},
              "Kapalı alanlar; yoksa etkin seçim veya tıklayarak seçim. Farkta ilk alan tutulur"}
            .en("objects"),
        Param::boolean("kaynaklari_koru", Arity::optional(),
                       "evet: sonuç oluşturulurken kaynaklar saklanır; varsayılan hayır. Boş "
                       "sonuçta daima korunur")
            .en("keep_sources")};
    if (op == core::BooleanOp::Difference)
        params.push_back(Param{"tutulan", ParamKind::Selection, Arity{0, 1},
                               "Nesneler içinden tutulacak alan; verilmezse açık listedeki ilk "
                               "alan, çoklu etkin seçimde ayrıca sorulur"}
                             .en("base"));
    return CommandSpec{
        .id       = operation.id,
        .names    = std::move(operation.names),
        .title    = operation.title,
        .category = Category::Modify,
        .params   = std::move(params),
        .undo     = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::LongRunning,
        .summary = std::string(operation.help) +
                   " Doğru/yay sınırları korunur; sonuç ilk kaynağın katmanında oluşturulur. "
                   "Kaydedilemeyen eğri/delik birleşimleri çizimi değiştirmeden reddedilir.",
        .run     = run,
        .targets = Targets::Faces | Targets::Curves,
    };
}
} // namespace

PIRICAD_COMMAND(area_union)
{
    return specification(core::BooleanOp::Union, &run_union);
}

PIRICAD_COMMAND(area_intersection)
{
    return specification(core::BooleanOp::Intersection, &run_intersection);
}

PIRICAD_COMMAND(area_difference)
{
    return specification(core::BooleanOp::Difference, &run_difference);
}

PIRICAD_COMMAND(area_symdifference)
{
    return specification(core::BooleanOp::SymmetricDifference, &run_symdifference);
}
} // namespace piricad::command
