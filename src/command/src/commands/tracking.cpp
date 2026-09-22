// SPDX-License-Identifier: GPL-3.0-or-later
// core.tracking — İZ. Geçici izleme: the marks a trace runs from.
//
// THE COMMONEST SETTING-OUT QUESTION A DRAWING CANNOT ANSWER BY ITSELF: put a
// point level with THAT corner and in line with THIS one. There is no geometry
// at the answer and none at either corner pointing to it; the point exists only
// because two others do. AutoCAD calls it object-snap tracking; a hand calls it
// holding a straightedge against two marks.
//
// TRANSPARENT, and that is the whole design. A mark is made in the MIDDLE of
// another command — `ÇİZGİ`, then `İZ 10,20` at the "next point" prompt, then
// point at the crossing — so this command must run beside a waiting one rather
// than cancel it (command.md R18). The marks live on the `Bus` for the same
// reason: they outlive the command that made them.
//
// THE WRITTEN FORM ALREADY EXISTED as `xy(P,Q)` in the one grammar (P1a), and it
// still does. This is its HAND version, and the two answer the same point by
// construction: `xy` computes the crossing from two points given at once, `İZ`
// marks them one at a time and the snap engine computes the same crossing. One
// rule, two roads (Article 1.2).
//
// SESSION STATE, never the document's (model.md R43): not hashed, not journalled,
// not undoable. A mark is scaffolding.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>

namespace kentos::command {
namespace {

/// A coordinate pair in metres, the way the ruler reads one.
std::string metres_pair(core::Point2 p)
{
    const auto one = [](core::Mm v) {
        const bool negative = v < 0;
        const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
        std::string frac    = std::to_string(abs_mm % 1000);
        frac                = std::string(3 - frac.size(), '0') + frac;
        return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
    };
    return one(p.x) + ", " + one(p.y);
}

Task<void> run(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    if (const Value clear = ctx.argument("sil"); !clear.empty() && clear.as_bool()) {
        const std::size_t had = bus.tracking_marks().size();
        bus.clear_tracking();
        ctx.record("sil", Value::boolean(true));
        ctx.echo(had == 0 ? "İşaretli nokta yoktu."
                          : std::to_string(had) + " işaret silindi. İzleme kapandı.");
        co_return;
    }

    const Value given = ctx.argument("nokta");
    if (given.empty() || given.as_points().empty()) {
        // NO ARGUMENT LISTS, it does not ask. A command that asked for a point
        // while the command it interrupted was asking for one would put two
        // prompts on one line, and the transparent road exists precisely so the
        // other command keeps its prompt.
        const auto& marks = bus.tracking_marks();
        if (marks.empty()) {
            ctx.echo("İşaretli nokta yok. Bir köşeyi işaretlemek için İZ <nokta> yazın ya da "
                     "nokta isteminde Shift + sağ tık yapın. İki işaret, birinin sağası ile "
                     "öbürünün yukarısının kesiştiği noktayı verir.");
            co_return;
        }
        std::string out = std::to_string(marks.size()) + " işaret:";
        for (const core::Point2 at : marks)
            out += "\n  " + metres_pair(at);
        if (marks.size() >= 2) {
            out += "\nKesişimler:  " + metres_pair(core::Point2{marks.front().x, marks.back().y}) +
                   "   ve   " + metres_pair(core::Point2{marks.back().x, marks.front().y});
        }
        ctx.echo(out);
        co_return;
    }

    for (const core::Point2 at : given.as_points())
        bus.mark_tracking(at);

    ctx.record("nokta", Value::points(given.as_points()));

    const auto& marks = bus.tracking_marks();
    std::string said  = "İşaretlendi: " + metres_pair(given.as_points().back()) + ".  " +
                       std::to_string(marks.size()) + " işaret";
    if (marks.size() >= 2)
        said += "; kesişim " + metres_pair(core::Point2{marks.front().x, marks.back().y}) +
                " ya da " + metres_pair(core::Point2{marks.back().x, marks.front().y});
    else
        said += "; yatay ve düşey izi açık";
    ctx.echo(said + ".");
}

} // namespace

KENTOS_COMMAND(tracking)
{
    return CommandSpec{
        .id       = "core.tracking",
        .names    = {"İZ", "IZ", "TRACK", "TRK"},
        .title    = "Geçici İzleme",
        .category = Category::Query,
        .params =
            {
                Param::points("nokta", Arity::optional(),
                              "İşaretlenecek nokta; yoksa işaretler listelenir")
                    .en("point"),
                Param::boolean("sil", Arity::optional(), "Bütün işaretleri siler").en("delete"),
            },
        // NOT UNDOABLE and not journalled as a mutation: a mark is a session aid,
        // exactly as a snap mode is (`MOD`). `ReadOnly` is what says so to the
        // bus; `Transparent` is what lets it run beside a waiting command.
        .undo  = UndoPolicy::None,
        .flags = Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly | Flags::NoEffect |
                 Flags::Transparent,
        .summary = "Geçici izleme için nokta işaretler; iki işaretin izleri kesişir.",
        .run     = &run,
    };
}

} // namespace kentos::command
