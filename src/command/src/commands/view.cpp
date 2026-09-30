// SPDX-License-Identifier: GPL-3.0-or-later
// core.zoom — YAKINLAŞ. A transparent command (piricad.md §3): it may interrupt
// another running command, and it changes view state, never document state.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/document.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace piricad::command {
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
    const Value::Points corners = ctx.argument("pencere").as_points();
    const Value::Points centre  = ctx.argument("merkez").as_points();
    const std::int64_t scale    = ctx.argument("olcek").as_int();
    const Value picked          = ctx.argument("nesneler");
    const std::string layer     = ctx.argument("katman").as_text();

    // THE MODE CAN BE LEFT TO THE PLACE: `YAKINLAŞ pencere=…` can only mean a
    // window and `YAKINLAŞ merkez=…` a centre, and making a hand say it twice
    // would be a rule with nothing behind it.
    if (ctx.argument("mod").empty() && !corners.empty()) mode = "PENCERE";
    if (ctx.argument("mod").empty() && corners.empty() && !centre.empty()) mode = "MERKEZ";
    if (ctx.argument("mod").empty() && corners.empty() && centre.empty() && !picked.empty())
        mode = "SECIM";
    if (ctx.argument("mod").empty() && corners.empty() && centre.empty() && picked.empty() &&
        !layer.empty())
        mode = "KATMAN";

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
    } else if (mode == "PENCERE" || mode == "WINDOW") {
        // TWO CORNERS, ASKED OF NOBODY. YAKINLAŞ is transparent, and a
        // transparent command that stopped to prompt would take the prompt of
        // the command it came through. The corners are on the line; the canvas
        // gesture (Alt+Z) is what collects them by hand.
        if (corners.size() != 2) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "PENCERE iki köşe ister: YAKINLAŞ PENCERE pencere=<köşe> <köşe>. Verilen: " +
                           std::to_string(corners.size()) + " köşe.");
            co_return;
        }
        const core::Point2 a = corners[0];
        const core::Point2 b = corners[1];
        if (a == b) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Pencerenin iki köşesi aynı nokta; bir pencere tanımlamıyor.");
            co_return;
        }
        move.kind   = ViewMove::Kind::Window;
        move.window = core::Box2{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x),
                                 std::max(a.y, b.y)};
        ctx.record("pencere", Value::points(corners));
        said = "PENCERE";
    } else if (mode == "MERKEZ" || mode == "CENTRE" || mode == "CENTER") {
        if (centre.size() != 1) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "MERKEZ bir nokta ister: YAKINLAŞ MERKEZ merkez=<nokta> [olcek=<1:N>].");
            co_return;
        }
        move.kind   = ViewMove::Kind::Centre;
        move.centre = centre.front();
        move.scale  = scale;
        ctx.record("merkez", Value::points(centre));
        if (scale > 0) ctx.record("olcek", Value::integer(scale));
        said = "MERKEZ";
    } else if (mode == "SECIM" || mode == "SELECTION") {
        // THE OBJECTS NAMED, or the ones selected when none are: an agent names
        // its objects rather than trusting whatever a hand left highlighted.
        std::vector<core::EntityKey> keys;
        if (!picked.empty()) {
            for (const std::int64_t k : picked.as_ids())
                keys.push_back(static_cast<core::EntityKey>(k));
            ctx.record("nesneler", picked);
        } else {
            keys.assign(bus.selection().keys().begin(), bus.selection().keys().end());
        }
        const core::Document& doc = bus.document();
        core::Box2 box{};
        for (const core::EntityKey k : keys) {
            const core::EntityId e = doc.slot_of(k);
            if (e == core::kNoEntity || !doc.alive(e)) continue;
            const core::Box2 each = doc.entity_extent(e);
            if (each.empty()) continue;
            box.extend(core::Point2{each.min_x, each.min_y});
            box.extend(core::Point2{each.max_x, each.max_y});
        }
        if (box.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "SEÇİM için seçili nesne yok: önce nesneleri seçin ya da nesneler= ile "
                       "verin.");
            co_return;
        }
        move.kind   = ViewMove::Kind::Fit;
        move.window = box;
        said        = "SEÇİM";
    } else if (mode == "KATMAN" || mode == "LAYER") {
        if (layer.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "KATMAN bir katman adı ister: YAKINLAŞ KATMAN katman=<ad>.");
            co_return;
        }
        const core::Document& doc = bus.document();
        const core::LayerId on    = doc.find_layer(layer);
        if (on == core::kNoLayer) {
            ctx.refuse(core::ErrorCode::NotFound, "'" + layer + "' adlı katman yok.");
            co_return;
        }
        // EVERY OBJECT ON IT, hidden ones included: asked for by name, the layer
        // is what the user wants to see, not what the screen happens to show.
        core::Box2 box{};
        const core::EntityTable& table = doc.entities();
        for (core::EntityId e = 0; e < table.size(); ++e) {
            if (!doc.alive(e) || table.layer[e] != on) continue;
            const core::Box2 each = doc.entity_extent(e);
            if (each.empty()) continue;
            box.extend(core::Point2{each.min_x, each.min_y});
            box.extend(core::Point2{each.max_x, each.max_y});
        }
        if (box.empty()) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "'" + layer + "' katmanında nesne yok; gösterilecek bir şey yok.");
            co_return;
        }
        ctx.record("katman", Value::text(layer));
        move.kind   = ViewMove::Kind::Fit;
        move.window = box;
        said        = "KATMAN";
    } else {
        ctx.refuse(
            core::ErrorCode::InvalidArgument,
            "Beklenen mod: KAPSAM | ÇARPAN | SIFIRLA | ÖNCEKİ | SONRAKİ | PENCERE | MERKEZ | "
            "SEÇİM | KATMAN. Girilen: '" +
                mode + "'");
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
    // WHAT WAS FRAMED, in millimetres, for the modes that frame something: an
    // agent that zoomed to a layer learns the layer's extent in the same answer.
    if (move.kind == ViewMove::Kind::Window || move.kind == ViewMove::Kind::Fit) {
        core::Json box = core::Json::array({});
        for (const core::Mm edge :
             {move.window.min_x, move.window.min_y, move.window.max_x, move.window.max_y})
            box.push(core::Json::integer(edge));
        report.set("kutu", std::move(box));
    }
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

PIRICAD_COMMAND(pan)
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

PIRICAD_COMMAND(zoom)
{
    return CommandSpec{
        .id       = "core.zoom",
        .names    = {"YAKINLAŞ", "YAKINLAS", "LİMİTBUL", "LIMITBUL", "ZOOM", "Z"},
        .title    = "Yakınlaş",
        .category = Category::View,
        .params =
            {
                Param::text("mod", Arity::optional(),
                            "KAPSAM | ÇARPAN | SIFIRLA | ÖNCEKİ | SONRAKİ | PENCERE | MERKEZ | "
                            "SEÇİM | KATMAN; ÖNCEKİ ve SONRAKİ görünüm geçmişinde birer adım gider "
                            "(30 adım)")
                    .en("mode"),
                Param::number("carpan", Arity::optional(), "ÇARPAN modunda ölçek katsayısı")
                    .en("factor"),
                Param::points("pencere", Arity{0, 2},
                              "PENCERE modunda pencerenin iki karşı köşesi; verilince mod "
                              "PENCERE olur")
                    .en("window"),
                Param::points("merkez", Arity::optional(),
                              "MERKEZ modunda görünümün ortasına gelecek nokta; verilince mod "
                              "MERKEZ olur")
                    .en("center"),
                Param::integer_range("olcek", Arity::optional(), 1, 100000000,
                                     "MERKEZ modunda ölçek paydası, 1:N; verilmezse ölçek kalır")
                    .en("scale"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "SEÇİM modunda çerçevelenecek nesneler; verilmezse seçim"}
                    .en("objects"),
                Param::text("katman", Arity::optional(),
                            "KATMAN modunda bütün nesneleri çerçevelenecek katmanın adı")
                    .en("layer"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::AiAccessible | Flags::Transparent | Flags::ReadOnly,
        .summary = "Görünümü çizim kapsamına ya da verilen çarpana ayarlar; ÖNCEKİ "
                   "ve SONRAKİ görünüm geçmişinde geri ve ileri gider.",
        .run     = &run,
    };
}

} // namespace piricad::command
