// SPDX-License-Identifier: GPL-3.0-or-later
// core.help — YARDIM, and core.script — BETİK.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/json.hpp"

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

        // AND THE PAGE OPENS ON IT, where there is a page. The text answer is
        // written either way: a script and an agent read it, and a person who
        // asked at the prompt has both.
        if (bus.on_help_page) bus.on_help_page(spec->names.front());

        for (const auto& p : spec->params) {
            std::string arity = p.arity.max == 0xFFFFFFFFu ? "en az " + std::to_string(p.arity.min)
                                                           : std::to_string(p.arity.min) + ".." +
                                                                 std::to_string(p.arity.max);
            ctx.echo("    " + p.name + " : " + param_kind_name(p.kind) + " [" + arity + "]  " +
                     p.help);
        }
        co_return;
    }

    // ---- THE WHOLE SET, GROUPED, AND NOT NINETY-EIGHT LINES -----------------
    //
    // This used to echo one line per command: the name, every alias, and the
    // summary, for all of them. The transcript is a running conversation, so
    // ninety-eight lines pushed everything a user had done out of sight and took
    // the scroll position with it. Their own words for it were that the command
    // list "stretches away downwards" and "opens far too late" — the second half
    // being the transcript re-laying out under a hundred appends.
    //
    // A listing answers ONE question — what is there — and a category answers it
    // better than a sentence repeated ninety-eight times. Nine lines now, one per
    // category, names only. The sentence for a single command is one keystroke
    // away and always was (`YARDIM komut=ÇİZGİ`).
    //
    // The listing is still generated from the registry — there is no second
    // command list anywhere in the project (kentoscad.md §2.3).
    //
    // AND THE FULL SET STILL LEAVES, as data rather than as prose. An agent or a
    // script that wants every command with every alias and every summary reads
    // `report` and gets it in one answer instead of scraping a hundred echo
    // lines (command.md R26).
    //
    // And where a client CAN show a page, it shows one: the grouped list with
    // every command's parameters beside it (`on_help_page`, the seam
    // `on_print_request` established). The text below is written either way.
    if (bus.on_help_page) bus.on_help_page(std::string{});

    ctx.echo("Komutlar (" + std::to_string(bus.registry().size()) +
             "). Ayrıntı: YARDIM komut=<ad>; aramak için Ctrl+K.");

    core::Json all = core::Json::array({});
    for (const auto& order :
         {Category::Draw, Category::Modify, Category::View, Category::Layer, Category::File,
          Category::Query, Category::Processing, Category::Script, Category::System}) {
        std::string names;
        std::size_t count = 0;
        for (const auto& spec : bus.registry().all()) {
            if (spec.category != order || spec.names.empty()) continue;
            if (count++) names += ", ";
            names += spec.names.front();

            core::Json row;
            row.set("id", core::Json::string(spec.id));
            core::Json aliases = core::Json::array({});
            for (const std::string& alias : spec.names)
                aliases.push(core::Json::string(alias));
            row.set("adlar", std::move(aliases));
            row.set("kategori", core::Json::string(category_name(spec.category)));
            row.set("ozet", core::Json::string(spec.summary));
            all.push(std::move(row));
        }
        if (count == 0) continue;
        ctx.echo(std::string(category_name(order)) + " (" + std::to_string(count) + "): " + names);
    }

    core::Json report;
    report.set("sayi", core::Json::integer(static_cast<std::int64_t>(bus.registry().size())));
    report.set("komutlar", std::move(all));
    ctx.report(std::move(report));
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
        .title    = "Komut Listesi",
        .category = Category::System,
        .params =
            {Param::text("komut", Arity::optional(), "Ayrıntısı istenen komut adı").en("command")},
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::ReadOnly,
        .summary = "Komut listesini veya tek bir komutun ayrıntısını gösterir.",
        .run     = &run_help,
        .effect  = Effect::Query,
    };
}

KENTOS_COMMAND(script)
{
    return CommandSpec{
        .id       = "core.script",
        .names    = {"BETİK", "BETIK", "SCRIPT"},
        .title    = "Betik Çalıştır",
        .category = Category::Script,
        .params = {Param::text("dosya", Arity::exactly(1), "Çalıştırılacak betik dosyasının yolu")
                       .en("file")},
        .undo    = UndoPolicy::Custom,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Bir betik dosyasını komut veri yolu üzerinden çalıştırır.",
        .run     = &run_script,
        // A SCRIPT IS WHATEVER IT CONTAINS. Nothing here can narrow that, so the
        // worst case is every effect a command in it could have — which is also
        // why THIS COMMAND CARRIES NO `AiAccessible` BIT and never will
        // (CLAUDE.md 5.24). An agent proposes commands, which are previewable,
        // validated and journalled one at a time; a script is none of those
        // things until it has already run, and with an interpreter embedded it is
        // arbitrary code with the user's own filesystem and network.
        .effect = Effect::Query | Effect::ViewChange | Effect::DocumentEdit | Effect::FileRead |
                  Effect::FileWrite | Effect::ExternalWrite | Effect::SettingsChange,
    };
}

} // namespace kentos::command
