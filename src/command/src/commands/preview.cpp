// SPDX-License-Identifier: GPL-3.0-or-later
// core.preview — ÖNİZLE: what command lines WOULD do, with the drawing untouched
// (TODOS F-05, command/preview.hpp).
//
// A COMMAND AND NOT ONLY A CARD. The suggestion card previews a plan; a script
// is previewed with `BETİK … onizle=evet`; this is the same preview for any
// command line, so the command line, a script and an agent can ask "what would
// this do" as the card does (Article 1.2). It changes nothing — the steps are
// run and taken back — so an agent may call it the way it calls a read tool.
#include "kentos_cad/command/preview.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The command lines a parameter holds, one or many.
std::vector<std::string> lines_of(const Value& v)
{
    if (v.empty()) return {};
    if (!v.as_texts().empty()) return v.as_texts();
    return {v.as_text()};
}

Task<void> run_preview(Context& ctx)
{
    std::vector<std::string> lines = lines_of(ctx.argument("komut"));
    if (lines.empty()) {
        auto typed = co_await ctx.text("komut", "Önizlenecek komut satırı");
        if (!typed || typed->empty()) co_return;
        lines.push_back(*typed);
    }

    // THE LINES AS STEPS, and nothing about who asked: an agent's preview runs
    // by the agent's rules because the bus knows an agent is running this
    // (`Bus::preview`), not because this body looked (command.md P10).
    Bus& bus = ctx.session().bus();
    std::vector<Invocation> steps;
    steps.reserve(lines.size());
    for (const std::string& line : lines) {
        auto inv = bus.parse_invocation(line, Origin::Script);
        if (!inv) {
            ctx.refuse(inv.error().code,
                       "Önizlenecek satır okunamadı (" + line + "): " + inv.error().message);
            co_return;
        }
        steps.push_back(std::move(inv.value()));
    }

    auto seen = bus.preview(steps);
    if (!seen) {
        ctx.refuse(seen.error());
        co_return;
    }
    answer_preview(ctx, seen.value(), ctx.argument("taslaklar").as_bool());
}

} // namespace

KENTOS_COMMAND(preview)
{
    return CommandSpec{
        .id       = "core.preview",
        .names    = {"ÖNİZLE", "ONIZLE", "PREVIEW", "ÖNZ"},
        .title    = "Önizle",
        .category = Category::Query,
        .params =
            {
                Param::text("komut", Arity{0, 256},
                            "Önizlenecek komut satırları, sırayla; çizimi değiştirmeden ne "
                            "yapacakları söylenir")
                    .en("commands"),
                Param::boolean("taslaklar", Arity::optional(),
                               "Yapılandırılmış cevaba oluşacak ve değişecek nesnelerin "
                               "taslakları (noktaları) da girsin mi; varsayılan hayır")
                    .en("outlines"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly |
                 Flags::NoEffect,
        .summary = "Komut satırlarının çizimde ne değiştireceğini, çizime dokunmadan söyler: "
                   "çalıştırır, sayar ve bütünüyle geri alır.",
        .run    = &run_preview,
        .effect = Effect::Query,
    };
}

} // namespace kentos::command
