// SPDX-License-Identifier: GPL-3.0-or-later
// core.zoom — YAKINLAŞ. A transparent command (kentoscad.md §3): it may interrupt
// another running command, and it changes view state, never document state.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    std::string mode = "KAPSAM";
    double factor    = 1.0;
    if (const Value v = ctx.argument("mod"); !v.empty()) {
        mode = core::turkish_fold_key(v.as_text());
        ctx.record("mod", Value::text(mode));
    }
    if (const Value v = ctx.argument("carpan"); !v.empty()) {
        factor = v.as_number(1.0);
        ctx.record("carpan", v);
    }

    // The word, folded, to the move and to the name the answer is given under.
    ViewMove move;
    move.factor      = factor;
    const char* said = nullptr;
    if (mode == "KAPSAM" || mode == "EXTENTS") {
        move.kind = ViewMove::Kind::Extents;
        said      = "KAPSAM";
    } else if (mode == "CARPAN" || mode == "FACTOR") {
        move.kind = ViewMove::Kind::Factor;
        said      = "ÇARPAN";
    } else if (mode == "SIFIRLA" || mode == "RESET") {
        move.kind = ViewMove::Kind::Reset;
        said      = "SIFIRLA";
    } else if (mode == "ONCEKI" || mode == "PREVIOUS") {
        move.kind = ViewMove::Kind::Previous;
        said      = "ÖNCEKİ";
    } else if (mode == "SONRAKI" || mode == "NEXT") {
        move.kind = ViewMove::Kind::Next;
        said      = "SONRAKİ";
    } else {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Beklenen mod: KAPSAM | ÇARPAN | SIFIRLA | ÖNCEKİ | SONRAKİ. Girilen: '" + mode +
                       "'");
        co_return;
    }

    if (!bus.on_view_move) {
        ctx.echo("Görünüm istemcisi bağlı değil (başsız çalışma).");
        co_return;
    }
    const ViewMoved done = bus.on_view_move(move);

    // NOWHERE TO GO IS SAID, not failed: a step back with no history behind it
    // is a question with the answer "none", and a script that walks back until
    // it stops must not be refused for asking once more.
    if (!done.moved && move.kind == ViewMove::Kind::Previous)
        ctx.echo("Geri dönülecek görünüm yok: görünüm geçmişi boş.");
    else if (!done.moved && move.kind == ViewMove::Kind::Next)
        ctx.echo("İleri gidilecek görünüm yok: ÖNCEKİ ile geri gidilmedi ya da o zamandan beri "
                 "görünüm değişti.");

    core::Json report;
    report.set("mod", core::Json::string(said));
    report.set("degisti", core::Json::boolean(done.moved));
    report.set("geri", core::Json::integer(static_cast<std::int64_t>(done.behind)));
    report.set("ileri", core::Json::integer(static_cast<std::int64_t>(done.ahead)));
    ctx.report(std::move(report));
}

/// KAYDIR — move the view without changing its scale.
///
/// Two points: the drawing slides so that the first lands on the second. That is
/// the gesture every CAD calls pan, and stating it as a pair of DOCUMENT points
/// rather than a pixel delta is what lets a script, the AI and the mouse all
/// express the same move (Article 1.2, 1.4).
Task<void> run_pan(Context& ctx)
{
    auto from = co_await ctx.point("baslangic", "Kaydırmanın tutulacağı nokta");
    if (!from) co_return; // ESC before the view moved

    auto to = co_await ctx.point("bitis", "Bu noktaya taşınacak",
                                 PointOptions{.rubber_band = true, .rubber_origin = *from});
    if (!to) co_return;

    Bus& bus = ctx.session().bus();
    if (bus.on_pan_request)
        bus.on_pan_request(*from, *to);
    else
        ctx.echo("Görünüm istemcisi bağlı değil (başsız çalışma).");

    ctx.record("baslangic", Value::point(*from));
    ctx.record("bitis", Value::point(*to));
}

} // namespace

KENTOS_COMMAND(pan)
{
    return CommandSpec{
        .id       = "core.pan",
        .names    = {"KAYDIR", "PAN", "KY"},
        .title    = "Kaydır",
        .category = Category::View,
        .params =
            {
                Param::point("baslangic", "Kaydırmanın tutulacağı nokta").en("start"),
                Param::point("bitis", "O noktanın taşınacağı yer").en("end"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::Transparent |
                 Flags::ReadOnly,
        .summary = "Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.",
        .run     = &run_pan,
    };
}

KENTOS_COMMAND(zoom)
{
    return CommandSpec{
        .id       = "core.zoom",
        .names    = {"YAKINLAŞ", "YAKINLAS", "LİMİTBUL", "LIMITBUL", "ZOOM", "Z"},
        .title    = "Yakınlaş",
        .category = Category::View,
        .params =
            {
                Param::text("mod", Arity::optional(),
                            "KAPSAM | ÇARPAN | SIFIRLA | ÖNCEKİ | SONRAKİ; ÖNCEKİ ve SONRAKİ "
                            "görünüm geçmişinde birer adım gider (30 adım)")
                    .en("mode"),
                Param::number("carpan", Arity::optional(), "ÇARPAN modunda ölçek katsayısı")
                    .en("factor"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::AiAccessible | Flags::Transparent | Flags::ReadOnly,
        .summary = "Görünümü çizim kapsamına ya da verilen çarpana ayarlar; ÖNCEKİ "
                   "ve SONRAKİ görünüm geçmişinde geri ve ileri gider.",
        .run     = &run,
    };
}

} // namespace kentos::command
