// SPDX-License-Identifier: GPL-3.0-or-later
// core.help — YARDIM, and core.script — BETİK.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

namespace kentos::command {
namespace {

Task<void> run_help(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    if (const Value v = ctx.argument("komut"); !v.empty()) {
        const CommandSpec* spec = bus.registry().resolve(v.as_text());
        if (!spec) {
            ctx.echo("Bilinmeyen komut: '" + v.as_text() + "'");
            co_return;
        }
        ctx.record("komut", v);

        std::string line = spec->id + "  (";
        for (std::size_t i = 0; i < spec->names.size(); ++i) {
            if (i) line += ", ";
            line += spec->names[i];
        }
        line += ")  — " + spec->summary;
        ctx.echo(line);

        for (const auto& p : spec->params) {
            std::string arity = p.arity.max == 0xFFFFFFFFu ? "en az " + std::to_string(p.arity.min)
                                                           : std::to_string(p.arity.min) + ".." +
                                                                 std::to_string(p.arity.max);
            ctx.echo("    " + p.name + " : " + param_kind_name(p.kind) + " [" + arity + "]  " +
                     p.help);
        }
        co_return;
    }

    // The listing is generated from the registry — there is no second command list
    // anywhere in the project (kentoscad.md §2.3).
    ctx.echo("Komutlar (" + std::to_string(bus.registry().size()) + "):");
    for (const auto& spec : bus.registry().all()) {
        std::string names;
        for (std::size_t i = 0; i < spec.names.size(); ++i) {
            if (i) names += ", ";
            names += spec.names[i];
        }
        ctx.echo("    " + names + "  — " + spec.summary);
    }
}

Task<void> run_script(Context& ctx)
{
    auto path = co_await ctx.text("dosya", "Betik dosyası");
    if (!path || path->empty()) co_return;

    Bus& bus = ctx.session().bus();
    if (!bus.on_run_script) {
        ctx.echo("Betik motoru bağlı değil.");
        co_return;
    }

    auto st = bus.on_run_script(*path);
    if (!st)
        ctx.echo("Betik hatası: " + st.error().message);
    else
        ctx.echo("Betik tamamlandı: " + *path);
}

} // namespace

KENTOS_COMMAND(help)
{
    return CommandSpec{
        .id       = "core.help",
        .names    = {"YARDIM", "HELP", "?"},
        .category = Category::System,
        .params   = {Param::text("komut", Arity::optional(), "Ayrıntısı istenen komut adı")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::ReadOnly,
        .summary  = "Komut listesini veya tek bir komutun ayrıntısını gösterir.",
        .run      = &run_help,
    };
}

KENTOS_COMMAND(script)
{
    return CommandSpec{
        .id       = "core.script",
        .names    = {"BETİK", "BETIK", "SCRIPT"},
        .category = Category::Script,
        .params = {Param::text("dosya", Arity::exactly(1), "Çalıştırılacak betik dosyasının yolu")},
        .undo   = UndoPolicy::Custom,
        .flags  = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Bir betik dosyasını komut veri yolu üzerinden çalıştırır.",
        .run     = &run_script,
    };
}

} // namespace kentos::command
