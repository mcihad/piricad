// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/preview.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/pick.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace kentos::command {
namespace {

using core::ErrorCode;

/// Why a step is not run in a preview, or empty when it is.
///
/// WHAT A PREVIEW MAY NOT DO is anything the batch abort cannot take back: the
/// abort rolls the document back and cuts it back, and puts the project
/// settings back — and nothing else.
std::string_view not_run_because(const CommandSpec& spec, const Args& args, Origin origin)
{
    // A PREVIEW NEVER PREVIEWS: one batch at a time.
    if (spec.id == "core.preview") return "önizleme önizlenmez";
    // ANOTHER DRAWING: an opened file is not un-opened.
    if (spec.id == "core.open" || spec.id == "core.new") return "çizimin yerine başkasını koyar";
    // FOR AN AGENT, only what an agent may call: a preview runs the command's
    // own code, and a file an agent may not import is not read for it here.
    if (origin == Origin::Ai && !has_flag(spec.flags, Flags::AiAccessible))
        return "yapay zekâya kapalı bir komut";

    const Effect e = effect_of(spec, args);
    if (has_effect(e, Effect::FileWrite)) return "dosya yazar";
    if (has_effect(e, Effect::ExternalWrite)) return "makinenin dışına yazar";
    if (has_effect(e, Effect::SettingsChange) && spec.id != "core.setting")
        return "uygulama ya da oturum ayarını değiştirir";
    // READ-ONLY YET EDITING: undo, redo, applying a suggestion — they change the
    // drawing OUTSIDE the transaction, where no rollback reaches.
    if (has_flag(spec.flags, Flags::ReadOnly) && has_effect(e, Effect::DocumentEdit))
        return "çizimi işlemin dışında değiştirir";
    if (has_effect(e, Effect::ViewChange) && !has_effect(e, Effect::DocumentEdit))
        return "görünümü değiştirir";
    return {};
}

/// Appends `e`'s outline, as the document draws it, to `shape`; the vertices it
/// added.
std::size_t outline_into(const core::Document& doc, core::EntityId e, PreviewShape& shape)
{
    std::size_t added = 0;
    // A CAPTION AS THE BOX ITS LETTERS FILL — what the selection ghost draws for
    // one (`core::text_quad`) — rather than the baseline it stands on.
    if (std::array<core::Point2, 4> quad; core::text_quad(doc, e, quad)) {
        shape.runs.push_back(GhostRun{std::vector<core::Point2>(quad.begin(), quad.end()), true});
        added += quad.size();
        if (doc.entities().kind[e] != core::kPolylineKind) return added;
    }
    core::EmitBuffer drawn;
    if (core::entity_outline(doc, e, drawn)) {
        for (std::size_t r = 0; r < drawn.run_start.size(); ++r) {
            GhostRun run;
            run.closed = drawn.run_closed[r] != 0;
            for (std::uint32_t v = 0; v < drawn.run_count[r]; ++v) {
                const std::uint32_t at = drawn.run_start[r] + v;
                run.points.push_back(core::Point2{drawn.xs[at], drawn.ys[at]});
            }
            added += run.points.size();
            shape.runs.push_back(std::move(run));
        }
        return added;
    }
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[e]);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        GhostRun run;
        run.closed    = geom.ring_role[r] != core::RingRole::Open;
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        run.points.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            run.points.push_back(core::Point2{xs[v], ys[v]});
        added += run.points.size();
        shape.runs.push_back(std::move(run));
    }
    return added;
}

/// The objects the batch's record touched, sorted, once each, with what they
/// were before it: alive or not.
std::vector<std::pair<core::EntityId, bool>> touched(std::span<const core::Op> step,
                                                     const core::Document& doc)
{
    std::vector<std::pair<core::EntityId, bool>> out; // (entity, alive before)
    for (const core::Op& op : step) {
        if (op.entity == core::kNoEntity) continue;
        switch (op.kind) {
        case core::Op::Kind::SetEntityAlive: out.emplace_back(op.entity, op.bool_arg); break;
        case core::Op::Kind::SetGeometry:
        case core::Op::Kind::SetKindGeometry:
        case core::Op::Kind::SetText: out.emplace_back(op.entity, true); break;
        default: break;
        }
    }
    // BY OBJECT, THE FIRST RECORD FIRST: an object's first alive op holds what it
    // was before the batch (see `summarize_changes`); stable, so ties keep order.
    std::ranges::stable_sort(out, [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::pair<core::EntityId, bool>> once;
    for (std::size_t i = 0; i < out.size();) {
        const core::EntityId e = out[i].first;
        bool before            = e < doc.entities().size() && doc.alive(e);
        bool known             = false;
        for (; i < out.size() && out[i].first == e; ++i)
            if (!known) {
                before = out[i].second;
                known  = true;
            }
        once.emplace_back(e, before);
    }
    return once;
}

} // namespace

core::Result<Preview> Bus::preview(std::span<const Invocation> steps)
{
    if (previewing_)
        return core::err(ErrorCode::InvalidArgument,
                         "Önizleme başka bir önizlemenin içinde yapılmaz.");

    Preview out;
    out.steps = steps.size();

    // WHAT A PREVIEW MUST GIVE BACK THAT THE ABORT DOES NOT: the selection and
    // the tracking marks, which a step may change as it runs.
    const Selection selection_before = selection_;
    const Selection previous_before  = previous_selection_;
    const auto tracking_before       = tracking_;

    // INSIDE A BATCH — a script, a Python console line — THE PREVIEW IS A
    // SAVEPOINT in it rather than a batch of its own: the steps borrow the
    // batch's transaction, and everything after the mark goes back — the
    // edits, what they appended, the journal lines they queued, the project
    // settings and the batch's command count — leaving the batch as it was.
    const bool inside                    = batch_ != nullptr;
    const std::size_t mark               = inside ? batch_->size() : 0;
    const core::Document::Tail tail      = doc_.tail();
    const LayerId active_before          = active_layer_;
    const std::size_t queued             = batch_journal_.size();
    const std::size_t commands           = batch_commands_;
    const core::Settings settings_before = project_settings_;
    if (!inside)
        if (const auto st = begin_batch("Önizleme"); !st) return st.error();
    previewing_          = true;
    const bool was_muted = echo_muted_;
    echo_muted_          = true;
    {
        const EchoCapture capture(*this, out.lines);
        // ASKED BY AN AGENT — directly, or from inside a command an agent ran —
        // every step runs by the agent's rules: only what an agent may call.
        const bool agent_asked = for_agent_ > 0;
        for (std::size_t i = 0; i < steps.size(); ++i) {
            Invocation inv = steps[i];
            if (agent_asked) inv.origin = Origin::Ai;
            const CommandSpec* spec = reg_.by_id(inv.name);
            if (spec == nullptr) spec = reg_.resolve(inv.name);
            if (spec == nullptr) {
                out.failed_at = i;
                out.error = core::err(ErrorCode::NotFound, "Bilinmeyen komut: '" + inv.name + "'.");
                break;
            }
            if (const std::string_view why = not_run_because(*spec, inv.args, inv.origin);
                !why.empty()) {
                out.skipped.push_back(PreviewSkip{i, spec->id, std::string(why)});
                continue;
            }
            auto ran = dispatch(inv);
            if (!ran) {
                out.failed_at = i;
                out.error     = ran.error();
                break;
            }
            ++out.ran;
            out.warnings.insert(out.warnings.end(), ran.value().warnings.begin(),
                                ran.value().warnings.end());
        }
    }

    // WHAT THEY WOULD LEAVE, read before the batch is taken back: the counts
    // from the record, the outlines from the drawing as it now stands.
    const std::span<const core::Op> record = batch_->ops_since(mark);
    out.changes                            = summarize_changes(doc_, record);
    std::size_t vertices                   = 0;
    for (const auto& [e, before] : touched(record, doc_)) {
        const bool now = e < doc_.entities().size() && doc_.alive(e);
        if ((doc_.entities().flags[e] & core::FlagInBlock) != 0) continue;
        if (before && !now) {
            out.erased.push_back(doc_.key_of(e));
            continue;
        }
        if (!now) continue; // born and gone inside the steps
        if (out.shapes.size() >= kPreviewShapeLimit || vertices >= kPreviewVertexLimit) {
            ++out.shapes_left_out;
            continue;
        }
        PreviewShape shape;
        shape.created = !before;
        if (before) shape.key = doc_.key_of(e);
        if (const core::Layer* l = doc_.layer(doc_.entities().layer[e])) shape.layer = l->name;
        const std::uint32_t slot = doc_.entities().slot[e];
        if (doc_.texts().has(slot)) shape.text = std::string(doc_.texts().text(slot));
        vertices += outline_into(doc_, e, shape);
        out.shapes.push_back(std::move(shape));
    }

    if (inside) {
        batch_->rollback_to(mark);
        cut_back(tail, active_before);
        batch_journal_.resize(queued);
        batch_commands_ = commands;
        project_settings_.restore(settings_before);
    } else {
        abort_batch();
    }
    echo_muted_         = was_muted;
    previewing_         = false;
    selection_          = selection_before;
    previous_selection_ = previous_before;
    tracking_           = tracking_before;
    return out;
}

std::string describe_preview(const Preview& p)
{
    std::string out;
    if (p.failed_at) {
        out = "Önizleme: " + std::to_string(*p.failed_at + 1) + ". adımda duracak — " +
              p.error.message;
        if (!out.empty() && out.back() != '.') out += '.';
        out += " Uygulanırsa bütünüyle geri alınacak; çizim değişmedi.";
    } else {
        const std::string would = describe_changes(p.changes, ChangeTense::Would);
        out                     = "Önizleme: " + std::to_string(p.steps) + " adım" +
              (would.empty() ? std::string(" — uygulanırsa çizimde bir şey değişmeyecek")
                             : " — uygulanırsa " + would);
        out += ". Çizim değişmedi.";
    }
    for (const PreviewSkip& s : p.skipped)
        out += "\n  " + std::to_string(s.step + 1) + ". adım (" + s.command_id +
               ") önizlemede çalıştırılmadı: " + s.reason + ".";
    if (p.shapes_left_out != 0)
        out += "\n  " + std::to_string(p.shapes_left_out) +
               " nesnenin taslağı çizilmedi (önizleme sınırı); sayılara dahil.";
    return out;
}

core::Json preview_json(const Preview& p, bool with_shapes)
{
    const auto n = [](std::size_t v) { return core::Json::integer(static_cast<std::int64_t>(v)); };
    core::Json out;
    out.set("adim", n(p.steps));
    out.set("calisan", n(p.ran));
    out.set("degisiklik", changes_json(p.changes));
    if (const std::string would = describe_changes(p.changes, ChangeTense::Would); !would.empty())
        out.set("degisiklik_ozeti", core::Json::string(would));
    if (p.failed_at) {
        out.set("duracagi_adim", n(*p.failed_at + 1));
        out.set("hata", core::Json::string(p.error.message));
    }
    core::Json skipped = core::Json::array({});
    for (const PreviewSkip& s : p.skipped) {
        core::Json one;
        one.set("adim", n(s.step + 1));
        one.set("komut", core::Json::string(s.command_id));
        one.set("neden", core::Json::string(s.reason));
        skipped.push(std::move(one));
    }
    out.set("calistirilmayan", std::move(skipped));
    core::Json erased = core::Json::array({});
    for (const core::EntityKey k : p.erased)
        erased.push(core::Json::integer(static_cast<std::int64_t>(core::raw(k))));
    out.set("silinecek", std::move(erased));
    if (with_shapes) {
        core::Json shapes = core::Json::array({});
        for (const PreviewShape& s : p.shapes) {
            core::Json one;
            one.set("yeni", core::Json::boolean(s.created));
            if (!s.created)
                one.set("nesne", core::Json::integer(static_cast<std::int64_t>(core::raw(s.key))));
            one.set("katman", core::Json::string(s.layer));
            if (!s.text.empty()) one.set("yazi", core::Json::string(s.text));
            core::Json runs = core::Json::array({});
            for (const GhostRun& r : s.runs) {
                core::Json pts = core::Json::array({});
                for (const core::Point2 v : r.points)
                    pts.push(
                        core::Json::array({core::Json::integer(v.x), core::Json::integer(v.y)}));
                core::Json run;
                run.set("kapali", core::Json::boolean(r.closed));
                run.set("noktalar", std::move(pts));
                runs.push(std::move(run));
            }
            one.set("halkalar", std::move(runs));
            shapes.push(std::move(one));
        }
        out.set("taslaklar", std::move(shapes));
        if (p.shapes_left_out != 0) out.set("cizilmeyen_taslak", n(p.shapes_left_out));
    }
    return out;
}

void answer_preview(Context& ctx, const Preview& p, bool with_shapes)
{
    const std::string said = describe_preview(p);
    for (std::size_t from = 0; from <= said.size();) {
        const std::size_t to = said.find('\n', from);
        ctx.echo(said.substr(from, to == std::string::npos ? std::string::npos : to - from));
        if (to == std::string::npos) break;
        from = to + 1;
    }
    ctx.report(preview_json(p, with_shapes));
}

} // namespace kentos::command
