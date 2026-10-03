// SPDX-License-Identifier: GPL-3.0-or-later
// core.sample — ÖRNEKPROJE: a sample project in place of the drawing (TODOS U-06).
//
// A new user opens the program to an empty canvas and the question "what do I do
// with this" has no answer on it. This command is the answer that is not a
// manual: it puts one of five small, finished jobs in the window — a map from
// survey points, a parcel layout, a zoning plan, a GIS analysis, a stakeout — and
// then says what to try next, in command-line words that run as printed.
//
// IT IS A COMMAND, NOT A MENU HANDLER (Article 1.2, 5.15). The application menu,
// the command palette, the command line, a script and an agent all reach it the
// same way; the window's only addition is the question "kaydedilsin mi?" around
// it, exactly as `YENİ` and `AÇ` have it (`MainWindow::newProject`).
//
// WHAT IT DOES, in order, and why that order:
//   1. the drawing is replaced by an empty one — `YENİ` itself is dispatched — a
//      sample is a whole drawing, not something poured into the one the user is
//      working on;
//   2. the project's script runs through the same bus and the same parser as
//      `BETİK`, so it is ONE undo step and the project is built by commands like
//      any other drawing (Article 1.2);
//   3. the view frames the result.
// A failure in step 2 rolls the script back whole (Article 1.6); the empty
// drawing from step 1 stays, which is what a failed `AÇ` of a damaged file leaves
// too.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/samples.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// `a, b, c` — the ids, for a sentence that tells the user what they could have typed.
std::string joined_ids(const SampleCatalog& catalog)
{
    std::string out;
    for (const Sample& s : catalog.samples) {
        if (!out.empty()) out += ", ";
        out += s.id;
    }
    return out;
}

Task<void> run_sample(Context& ctx)
{
    auto catalog = load_samples();
    if (!catalog) {
        ctx.refuse(catalog.error());
        co_return;
    }

    // THE LIST, so the one who typed a bare ÖRNEKPROJE sees what there is before
    // being asked which; the shell offers the same ids as choices.
    if (ctx.argument("ad").empty()) {
        for (const Sample& s : catalog.value().samples)
            ctx.echo(s.id + " — " + s.title + ": " + s.summary);
    }
    std::vector<std::string> ids;
    for (const Sample& s : catalog.value().samples)
        ids.push_back(s.id);
    auto asked = co_await ctx.text("ad", "Hangi örnek proje", ids);
    if (!asked || asked->empty()) co_return;

    const Sample* sample = catalog.value().find(*asked);
    if (sample == nullptr) {
        ctx.refuse(core::ErrorCode::NotFound, "'" + *asked +
                                                  "' adında bir örnek proje yok. Olanlar: " +
                                                  joined_ids(catalog.value()) + ".");
        co_return;
    }

    Bus& bus = ctx.session().bus();
    if (!bus.on_run_script) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Betik motoru bağlı değil; bu ortamda örnek proje açılamaz.");
        co_return;
    }

    // 1. AN EMPTY DRAWING, by the command that makes one. `YENİ` is dispatched, not
    //    imitated, so it is a line of the journal like any other and a replay of
    //    this project starts from an empty drawing too; it also sends the view back
    //    to where a drawing starts.
    Invocation fresh;
    fresh.name   = "core.new";
    fresh.origin = Origin::Script;
    if (auto made = bus.dispatch(fresh); !made) {
        ctx.refuse(made.error());
        co_return;
    }

    // 2. THE PROJECT, as a script: its commands are the journal lines and, joined to
    //    the batch they run in, one undo step. This command writes no line of its
    //    own (`ReadOnly`, as `BETİK` is): the lines of `YENİ` and of the project are
    //    the whole record, and a second line saying "and then do all that" would
    //    replay the project twice.
    auto ran = bus.on_run_script(sample->script);
    if (!ran) {
        ctx.refuse(ran.error().code,
                   "'" + sample->title + "' örnek projesi kurulamadı: " + ran.error().message);
        co_return;
    }

    // 3. THE VIEW on what was built.
    if (bus.on_view_move) (void)bus.on_view_move(ViewMove{.kind = ViewMove::Kind::Extents});

    ctx.echo("Örnek proje açıldı: " + sample->title + ". " + sample->summary);
    if (!sample->layout.empty())
        ctx.echo("Pafta hazır: '" + sample->layout +
                 "' yerleşimi 1:" + std::to_string(sample->scale) + " ölçeğinde.");
    if (!sample->steps.empty()) {
        // TWO LINES PER STEP, the command and then why: the transcript is a narrow panel, and a
        // command with its sentence after it wraps into something nobody can copy from.
        ctx.echo("Deneyin — her satırı komut satırına olduğu gibi yazabilirsiniz:");
        int n = 0;
        for (const SampleStep& step : sample->steps) {
            ctx.echo("  " + std::to_string(++n) + ". " + step.command);
            ctx.echo("     " + step.description);
        }
    }
}

} // namespace

PIRICAD_COMMAND(sample)
{
    return CommandSpec{
        .id       = "core.sample",
        .names    = {"ÖRNEKPROJE", "ORNEKPROJE", "SAMPLE", "ÖRNEK", "ORNEK"},
        .title    = "Örnek Proje",
        .category = Category::File,
        .params =
            {
                Param::text("ad", Arity::optional(),
                            "Açılacak örnek projenin kimliği ya da başlığı; verilmezse liste "
                            "gösterilir ve sorulur")
                    .en("name"),
            },
        // NOT UNDOABLE AS A COMMAND, for the reason `YENİ` is not: it puts a
        // different drawing in the window, so there is no inverse over the one that
        // was there. The project's own script is the one undo step, taken after the
        // swap, and `BETİK` carries `Custom` for the same reason.
        .undo = UndoPolicy::Custom,
        // NOT AiAccessible, as `YENİ` and `AÇ` are not: it discards the drawing the
        // user is working on and cannot be taken back (`.claude/ai.md`).
        // `ReadOnly` for the reason `BETİK` carries it: the command itself writes no
        // journal line, because the commands it runs write theirs (see `run_sample`).
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Hazır bir örnek projeyi (ölçüden harita, parsel düzenleme, plan, GIS, "
                   "aplikasyon) boş bir çizim olarak açar ve ne deneyeceğinizi söyler.",
        .run     = &run_sample,
        .effect  = Effect::Query | Effect::ViewChange | Effect::DocumentEdit | Effect::FileRead,
    };
}

} // namespace piricad::command
