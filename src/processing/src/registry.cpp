// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — processing: the tool list, and the one command body every tool runs through.
//
// THE RUNNER IS THE WHOLE CONTRACT. A tool declares what it takes and computes
// from a snapshot; everything that touches the bus — reading the scope, asking
// for objects when the selection is empty, defaults and validation, the worker
// thread, the stop, the transaction, the journal — happens here once, so a new
// tool cannot get any of it wrong and a fix here fixes every tool.
//
// Three phases, on two threads (command.md R8a):
//   1. on the bus thread: resolve the scope, snapshot the objects (`ToolInput`);
//   2. on the worker, when the session can host one: `tool.run` into a `ToolOutput`
//      that shares nothing with the document;
//   3. on the bus thread again, inside the command's one transaction: the output
//      becomes objects on the output layer, and the resolved arguments are recorded
//      so a replay acts on the same objects with the same parameters.
// Durdur requests the job's stop; the tool returns `Cancelled`; phase 3 never
// runs, and the transaction commits nothing.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <utility>
#include <vector>

namespace kentos::processing {

KENTOS_PROCESSING_TOOL(label_length);
KENTOS_PROCESSING_TOOL(number_vertices);
KENTOS_PROCESSING_TOOL(area_edit);

namespace {

using command::Context;
using command::Task;
using command::Value;

/// THE LIST. Tree order: by group, then by title, as the panel shows them.
const std::vector<const ProcessingTool*>& all_tools()
{
    static const std::vector<const ProcessingTool*> tools = [] {
        std::vector<const ProcessingTool*> out{
            &kentos_tool_label_length(),
            &kentos_tool_number_vertices(),
            &kentos_tool_area_edit(),
        };
        std::stable_sort(out.begin(), out.end(),
                         [](const ProcessingTool* a, const ProcessingTool* b) {
                             if (a->spec().group != b->spec().group)
                                 return a->spec().group < b->spec().group;
                             return a->spec().title < b->spec().title;
                         });
        return out;
    }();
    return tools;
}

/// A fallback written as text, as the Value its kind wants.
Value value_from_text(const ToolParam& p, const std::string& text)
{
    switch (p.kind) {
    case command::ParamKind::Integer: {
        std::int64_t n = 0;
        (void)std::from_chars(text.data(), text.data() + text.size(), n);
        return Value::integer(n);
    }
    case command::ParamKind::Number: {
        double d = 0.0;
        (void)std::from_chars(text.data(), text.data() + text.size(), d);
        return Value::number(d);
    }
    case command::ParamKind::Bool:
        return Value::boolean(core::turkish_key_equals(text, "evet") ||
                              core::turkish_key_equals(text, "true") || text == "1");
    default: return Value::text(text);
    }
}

/// Applies defaults and validates what the bus could not: a choice word, a range.
/// Returns the message to echo, or empty when every parameter is acceptable.
std::string resolve_params(Context& ctx, const ToolSpec& spec, command::Args& out)
{
    for (const ToolParam& p : spec.params) {
        Value v = ctx.argument(p.name);
        if (v.empty()) {
            if (p.fallback.empty()) continue;
            v = value_from_text(p, p.fallback);
        }
        if (!p.choices.empty()) {
            const std::string& word = v.as_text();
            bool known              = false;
            for (const std::string& c : p.choices)
                if (core::turkish_key_equals(word, c)) {
                    v     = Value::text(c);
                    known = true;
                }
            if (!known) {
                std::string list;
                for (const std::string& c : p.choices)
                    list += (list.empty() ? "" : " / ") + c;
                return "'" + p.name + "' için tanınmayan değer: '" + word +
                       "'. Seçenekler: " + list + ".";
            }
        }
        if (p.bounded && p.kind == command::ParamKind::Integer) {
            const std::int64_t n = v.as_int();
            if (n < p.low || n > p.high)
                return "'" + p.name + "' " + std::to_string(p.low) + " ile " +
                       std::to_string(p.high) + " arasında olmalı; verilen " + std::to_string(n) +
                       ".";
        }
        out.set(p.name, std::move(v));
    }
    return {};
}

/// Copies one entity out of the document into the worker's snapshot.
InputEntity snapshot(const core::Document& doc, core::EntityId e, Applies cls)
{
    InputEntity out;
    const core::EntityTable& ents = doc.entities();
    out.slot                      = e;
    out.key                       = static_cast<std::int64_t>(core::raw(ents.key[e]));
    out.kind                      = ents.kind[e];
    out.cls                       = cls;
    out.layer                     = ents.layer[e];
    if (out.layer < doc.layers().size()) out.layer_name = doc.layers()[out.layer].name;
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan rs        = geom.rings_of(ents.slot[e]);
    for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
        InputEntity::Ring ring;
        ring.role     = geom.ring_role[r];
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        ring.points.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.points.push_back(core::Point2{xs[v], ys[v]});
        out.rings.push_back(std::move(ring));
    }
    if (doc.texts().has(ents.slot[e])) {
        out.text        = std::string(doc.texts().text(ents.slot[e]));
        out.text_height = doc.texts().height(ents.slot[e]);
    }
    return out;
}

bool touches(const core::Box2& a, const core::Box2& b) noexcept
{
    return a.max_x >= b.min_x && a.min_x <= b.max_x && a.max_y >= b.min_y && a.min_y <= b.max_y;
}

/// THE ONE BODY. Which tool it is comes from the session's spec, so the same
/// function serves every generated command.
Task<void> run_tool(Context& ctx)
{
    const ProcessingTool* tool = find_tool(ctx.session().spec().id);
    if (tool == nullptr) {
        ctx.echo("Bu komuta bağlı bir işlem aracı yok: " + ctx.session().spec().id);
        co_return;
    }
    const ToolSpec& spec      = tool->spec();
    const core::Document& doc = ctx.document();

    // ---- parameters first, so a typo fails before anyone is asked to point ----
    ToolInput input;
    if (const std::string why = resolve_params(ctx, spec, input.args); !why.empty()) {
        ctx.echo(why);
        co_return;
    }
    command::Bus& bus = ctx.session().bus();
    input.unit        = core::drawing_unit_from_setting(
        static_cast<std::uint16_t>(bus.project_settings().get("core.cizim.birim").as_enum()));
    input.plan_scale = bus.project_settings().get("core.plan.olcek").as_int();
    if (input.plan_scale <= 0) input.plan_scale = 1000;

    // ---- the scope: ids given, the selection (asked for when empty), the
    //      viewport, or the whole project ----
    std::vector<std::int64_t> ids;
    std::string scope = "secili";
    if (const Value k = ctx.argument("kapsam"); !k.empty()) {
        const std::string& w = k.as_text();
        if (core::turkish_key_equals(w, "secili") || core::turkish_key_equals(w, "selected"))
            scope = "secili";
        else if (core::turkish_key_equals(w, "gorunum") || core::turkish_key_equals(w, "view"))
            scope = "gorunum";
        else if (core::turkish_key_equals(w, "proje") || core::turkish_key_equals(w, "project"))
            scope = "proje";
        else {
            ctx.echo("Tanınmayan kapsam: '" + w + "'. Kapsamlar: secili, gorunum, proje.");
            co_return;
        }
    }

    std::vector<core::EntityId> slots;
    const std::string example = spec.names.front() + " kapsam=proje";
    if (ctx.has_argument("nesneler") || scope == "secili") {
        if (!co_await command::want_objects(
                ctx, "nesneler", spec.title + ": nesneleri seçin, sonra sağ tık", ids, 0, example))
            co_return;
        for (const std::int64_t raw : ids) {
            if (raw <= 0) {
                ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                         ". Kimlikler 1'den başlar.");
                co_return;
            }
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
            if (e == core::kNoEntity || !doc.alive(e)) {
                ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
                co_return;
            }
            slots.push_back(e);
        }
    } else if (scope == "gorunum") {
        const Value window = ctx.argument("pencere");
        if (window.as_points().size() < 2) {
            ctx.echo("gorunum kapsamı görünümün iki köşesini ister: pencere=<x1,y1> <x2,y2>. "
                     "Arayüzden çalıştırıldığında pencere görünümden alınır.");
            co_return;
        }
        const core::Point2 a = window.as_points()[0];
        const core::Point2 b = window.as_points()[1];
        const core::Box2 box{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x),
                             std::max(a.y, b.y)};
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && touches(doc.entities().box_of(e), box)) slots.push_back(e);
    } else {
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e)) slots.push_back(e);
    }

    // ---- what the tool applies to; the rest is counted, never silently dropped ----
    std::size_t skipped = 0;
    std::vector<std::int64_t> used;
    for (const core::EntityId e : slots) {
        const Applies cls = classify(doc, e);
        if (cls == Applies::None || !applies_to(spec.applies, cls)) {
            ++skipped;
            continue;
        }
        input.entities.push_back(snapshot(doc, e, cls));
        used.push_back(input.entities.back().key);
    }
    if (input.entities.empty()) {
        std::string takes;
        for (const Applies one :
             {Applies::Points, Applies::Lines, Applies::Faces, Applies::Curves, Applies::Texts})
            if (applies_to(spec.applies, one))
                takes += (takes.empty() ? "" : ", ") + std::string(applies_name(one));
        ctx.echo("Kapsamda bu araca uygun nesne yok (" + std::to_string(slots.size()) +
                 " nesne bakıldı). Araç şunlara uygulanır: " + takes + ".");
        co_return;
    }

    // ---- the tool's own questions, on the bus thread, before the worker ----
    if (auto asked = co_await tool->interact(ctx, input); !asked) {
        if (asked.error().code != core::ErrorCode::Cancelled) ctx.echo(asked.error().message);
        co_return;
    }

    // ---- the work, off the bus thread when the session can host it ----
    ToolOutput output;
    core::Status status = core::ok();
    command::Job job;
    job.label = spec.title;
    job.work  = [&](const command::JobControl& control) {
        const Progress progress{control.stop, &job.permille};
        status = tool->run(input, output, progress);
    };
    co_await command::run_job(ctx.session(), job);

    if (job.stop.stop_requested() ||
        (!status && status.error().code == core::ErrorCode::Cancelled)) {
        ctx.echo("İşlem durduruldu; çizim değişmedi.");
        co_return;
    }
    if (!status) {
        ctx.echo(status.error().message);
        co_return;
    }

    // ---- the result, through the command's one transaction ----
    core::LayerId layer = ctx.active_layer();
    std::string layer_name;
    if (const Value k = ctx.argument("katman");
        !k.empty() && !k.as_text().empty() && spec.output != OutputShape::InPlace) {
        layer      = ctx.transaction().ensure_layer(k.as_text());
        layer_name = k.as_text();
    } else if (layer < doc.layers().size()) {
        layer_name = doc.layers()[layer].name;
    }

    std::size_t made = 0;
    for (const ToolOutput::Caption& c : output.captions) {
        if (c.text.empty() || c.height <= 0) continue;
        const std::array<core::Point2, 2> base =
            core::dimension_baseline(c.centre, c.dir_x, c.dir_y, c.height, c.text);
        auto created = ctx.transaction().add_polyline(layer, base);
        if (!created) {
            ctx.echo(created.error().message);
            co_return;
        }
        if (auto st = ctx.transaction().set_text(created.value(), c.text, c.height,
                                                 core::TextAnchor::MiddleCentre);
            !st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++made;
    }
    for (const ToolOutput::Polyline& p : output.polylines) {
        if (p.points.size() < 2) continue;
        core::Result<core::EntityId> created = core::err(core::ErrorCode::Internal, "");
        if (p.closed) {
            const core::RingGeometry::RingInput ring{p.points, core::RingRole::Exterior, 0};
            created = ctx.transaction().add_kind(
                layer, core::kPolylineKind,
                std::span<const core::RingGeometry::RingInput>(&ring, 1), {});
        } else {
            created = ctx.transaction().add_polyline(layer, p.points);
        }
        if (!created) {
            ctx.echo(created.error().message);
            co_return;
        }
        ++made;
    }

    for (const ToolOutput::Replacement& r : output.replacements) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(r.key)));
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        std::vector<core::RingGeometry::RingInput> rings;
        rings.reserve(r.rings.size());
        for (const InputEntity::Ring& ring : r.rings)
            rings.push_back(core::RingGeometry::RingInput{ring.points, ring.role, 0});
        if (auto st = ctx.transaction().set_geometry(e, rings); !st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++made;
    }

    // ---- the record: the objects it acted on and every parameter it used ----
    ctx.record("nesneler", Value::ids(used));
    if (!layer_name.empty()) ctx.record("katman", Value::text(layer_name));
    for (const ToolParam& p : spec.params)
        if (const Value* v = input.args.find(p.name); v != nullptr && !v->empty())
            ctx.record(p.name, *v);

    std::string said =
        spec.title + ": " + std::to_string(output.touched) + " nesneye uygulandı, " +
        std::to_string(made) +
        (spec.output == OutputShape::InPlace ? " nesne değiştirildi" : " nesne üretildi");
    if (!layer_name.empty()) said += " (katman: " + layer_name + ")";
    said += ".";
    if (skipped != 0)
        said += " Kapsamdaki " + std::to_string(skipped) +
                " nesne bu araca uygun olmadığı için atlandı.";
    for (const std::string& n : output.notes)
        said += "\n  not: " + n;
    ctx.echo(said);
}

} // namespace

std::span<const ProcessingTool* const> processing_tools()
{
    return all_tools();
}

const ProcessingTool* find_tool(std::string_view id_or_name)
{
    for (const ProcessingTool* t : all_tools()) {
        if (t->spec().id == id_or_name) return t;
        for (const std::string& n : t->spec().names)
            if (core::turkish_key_equals(n, id_or_name)) return t;
    }
    return nullptr;
}

void register_processing_commands(command::Registry& registry)
{
    for (const ProcessingTool* t : all_tools()) {
        command::CommandSpec spec = t->spec().to_command_spec();
        spec.run                  = &run_tool;
        (void)registry.add(std::move(spec));
    }
}

} // namespace kentos::processing
