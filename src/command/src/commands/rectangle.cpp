// SPDX-License-Identifier: GPL-3.0-or-later
// core.rectangle — DİKDÖRTGEN. A four-cornered face from two opposite corners.
//
// WHY A COMMAND OF ITS OWN, when ALAN already draws any face.
//
// Because a rectangle is what a plan is mostly made of — a building footprint, a
// setback boundary, a plan sheet's own frame — and asking for four corners when
// two determine the shape is asking the user to place two points that the
// program can compute and can guarantee are square. Clicking them by hand gives
// four corners that are nearly right, and "nearly right" in a cadastral drawing
// is a defect that survives into the signed sheet.
//
// The second corner is the one OPPOSITE the first, not the next one along, which
// is what every drawing program means by a rectangle drag and what makes the
// diagonal lock produce a square: lock the aim to 45° from the first corner and
// |dx| equals |dy| (see `core.yakalama.kosegen`).
//
// Degenerate input is refused rather than drawn: two corners sharing an x or a y
// enclose nothing, and the geometry layer would reject the ring anyway — saying
// so here names the corner the user has to move.
//
// TWO MORE WAYS TO SAY A RECTANGLE, both from a field sketch (netcad_plan.md
// N-13). `yontem=derinlik`: the two corners of a wall and how deep the building
// goes off it, RIGHT of the first→second direction positive and left negative —
// the one sign every offset from a baseline uses (the building tool Netcad calls
// `Bina Oluştur`). `yontem=olcu`: one corner, a width and a length, turned by an
// angle — and the size may be a sheet of paper, whose millimetres the plan scale
// brings down to ground metres (Netcad's `Kutu`). The arithmetic of both is
// `core`'s (`depth_rectangle_corners`, `box_corners`), for the reason
// `edge_rectangle_corners` is: one answer for the command and for anything that
// previews it.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::command {
namespace {

/// A length as a surveyor reads one: metres, three decimals, the Turkish comma.
std::string metres(core::Mm v)
{
    return core::metres_fixed(v, 3, ',') + " m";
}

/// The method that reads a parameter only some methods take.
///
/// A PARAMETER THE CHOSEN METHOD DOES NOT READ IS REFUSED, not ignored: `aci=30`
/// beside `yontem=3n` would otherwise be a turn the user asked for and never got,
/// which is exactly the silent surplus `.claude/command.md` P15 forbids.
struct Owned
{
    const char* param;  ///< the parameter
    const char* method; ///< the one method that reads it
};

constexpr Owned kOwned[] = {{"derinlik", "derinlik"}, {"en", "olcu"},  {"boy", "olcu"},
                            {"kagit", "olcu"},        {"yon", "olcu"}, {"olcek", "olcu"},
                            {"aci", "olcu"}};

/// Refuses a parameter that belongs to another method than `method`; true when
/// none does. `method` is the method's own spelling: `2n`, `3n`, `derinlik`, `olcu`.
bool only_own_parameters(Context& ctx, std::string_view method)
{
    for (const Owned& owned : kOwned) {
        if (method == owned.method || !ctx.has_argument(owned.param)) continue;
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string("`") + owned.param + "` yalnız yontem=" + owned.method +
                       " ile verilir; yontem=" + std::string(method) + " onu kullanmaz.");
        return false;
    }
    return true;
}

/// A RUN THAT CANNOT ASK IS REFUSED FOR WHAT IT WAS NOT GIVEN, before anything is
/// drawn, as the bus refuses a missing argument before a script's command runs.
/// The spec cannot say it here: how many corners `noktalar` holds, and whether
/// a depth or a size is wanted, depends on the method. A run a person drives
/// (`Session::client_driven`) is asked instead, and Esc there is a cancel, as
/// it always was; a script's values ending short was a success that drew
/// nothing and said nothing.
bool given_to_a_run_that_cannot_ask(Context& ctx, std::size_t corners, std::string_view method,
                                    std::string_view what)
{
    if (ctx.session().client_driven()) return true;
    if (ctx.argument("noktalar").as_points().size() < corners) {
        ctx.refuse(core::ErrorCode::ValidationFailed,
                   "DİKDÖRTGEN yontem=" + std::string(method) + " " + std::string(what) + "; " +
                       std::to_string(ctx.argument("noktalar").as_points().size()) +
                       " nokta verildi.");
        return false;
    }
    return true;
}

/// The same for a number the method needs: `derinlik`, `en`, `boy`.
bool number_given_to_a_run_that_cannot_ask(Context& ctx, const char* param, std::string_view method,
                                           std::string_view what)
{
    if (ctx.session().client_driven() || ctx.has_argument(param)) return true;
    ctx.refuse(core::ErrorCode::ValidationFailed,
               "DİKDÖRTGEN yontem=" + std::string(method) + " " + std::string(what) + ".");
    return false;
}

/// Writes the four corners as one closed face on the active layer. False when the
/// document refused it, with the command already failed.
bool draw_face(Context& ctx, std::span<const core::Point2> corners)
{
    std::vector<core::RingGeometry::RingInput> rings{
        core::RingGeometry::RingInput{corners, core::RingRole::Exterior, 0}};
    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.refuse(created.error());
        return false; // the bus rolls the transaction back
    }
    return true;
}

/// DİKDÖRTGEN `yontem=derinlik`: the edge's two corners, then how deep.
Task<void> run_depth(Context& ctx)
{
    if (!only_own_parameters(ctx, "derinlik")) co_return;
    if (const std::size_t given = ctx.argument("noktalar").as_points().size(); given > 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "yontem=derinlik iki köşe ister: kenarın iki ucu; " + std::to_string(given) +
                       " nokta verildi.");
        co_return;
    }
    if (!given_to_a_run_that_cannot_ask(ctx, 2, "derinlik", "kenarın iki köşesini ister") ||
        !number_given_to_a_run_that_cannot_ask(
            ctx, "derinlik", "derinlik",
            "derinlik=<metre> ister: ilk köşeden ikinciye bakarken sağ artı, sol eksi"))
        co_return;

    auto first = co_await ctx.point("noktalar", "Kenarın ilk köşesi");
    if (!first) co_return;
    auto second = co_await ctx.point("noktalar", "Kenarın ikinci köşesi",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *first,
                                                  .rubber_shape  = RubberShape::Line});
    if (!second) co_return;
    if (*first == *second) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kenarın iki köşesi aynı nokta; bir kenar tanımlamıyor.");
        co_return;
    }

    // THE EDGE STAYS ON SCREEN while the depth is typed, as DİKAYAK keeps its
    // baseline: nothing follows the cursor, because a depth is a number and a
    // number is not answered by pointing.
    auto depth = co_await ctx.number(
        "derinlik", "Derinlik (m): ilk köşeden ikinciye bakarken sağ artı, sol eksi",
        PointOptions{.rubber_band   = true,
                     .rubber_origin = *second,
                     .rubber_shape  = RubberShape::Fixed,
                     .rubber_chain  = {*first, *second}});
    if (!depth) co_return;

    std::array<core::Point2, 4> four{};
    if (!core::depth_rectangle_corners(*first, *second, core::mm_from_metres(*depth), four)) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Derinlik sıfır olamaz (en az 1 mm): sağa artı, sola eksi verin.");
        co_return;
    }
    if (!draw_face(ctx, four)) co_return;

    // THE TWO CORNERS AND THE DEPTH ARE RECORDED, not the four that came out.
    ctx.record("yontem", Value::text("derinlik"));
    ctx.record("noktalar", Value::points(Value::Points{*first, *second}));
    ctx.record("derinlik", Value::number(*depth));

    ctx.echo("Dikdörtgen çizildi: " + metres(core::segment_length(four[0], four[1])) + " × " +
             metres(core::segment_length(four[1], four[2])) + " (derinlik " +
             (*depth < 0 ? "solda" : "sağda") + ").");
}

/// DİKDÖRTGEN `yontem=olcu`: one corner, a width and a length — or a sheet of
/// paper — and a turn.
Task<void> run_size(Context& ctx)
{
    if (!only_own_parameters(ctx, "olcu")) co_return;
    if (const std::size_t given = ctx.argument("noktalar").as_points().size(); given > 1) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "yontem=olcu tek nokta ister: kutunun ilk köşesi; " + std::to_string(given) +
                       " nokta verildi.");
        co_return;
    }

    const bool paper_given = ctx.has_argument("kagit");
    if (paper_given && (ctx.has_argument("en") || ctx.has_argument("boy"))) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "`kagit` verilince en ve boy kâğıttan gelir; `kagit` ile `en` ya da `boy` "
                   "birlikte verilmez.");
        co_return;
    }
    if (!paper_given && (ctx.has_argument("yon") || ctx.has_argument("olcek"))) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "`yon` ve `olcek` yalnız `kagit` ile verilir: kâğıdın yönü ve ölçeği kâğıt "
                   "boyunu zemine indirir.");
        co_return;
    }
    if (!given_to_a_run_that_cannot_ask(ctx, 1, "olcu", "kutunun ilk köşesini ister") ||
        (!paper_given && (!number_given_to_a_run_that_cannot_ask(
                              ctx, "en", "olcu", "en= ve boy= ya da kagit= ister") ||
                          !number_given_to_a_run_that_cannot_ask(
                              ctx, "boy", "olcu", "en= ve boy= ya da kagit= ister"))))
        co_return;

    // THE SIZE, in ground millimetres. Either typed as the two metres a plan
    // gives, or a sheet of paper brought down by the plan scale: the table of
    // sheet sizes is `core::paper_size_mm`, the one the print profiles and the
    // layouts read (CLAUDE.md 5.10), and the scale is the drawing's own
    // (`core.plan.olcek`) unless `olcek` says otherwise.
    core::Mm width_mm  = 0;
    core::Mm length_mm = 0;
    double typed_width = 0.0;
    double typed_long  = 0.0;
    std::string paper;
    std::string orientation;
    std::int64_t scale = 0;
    if (paper_given) {
        paper           = core::canonical_paper(ctx.argument("kagit").as_text());
        const auto size = core::is_custom_paper(paper) ? std::nullopt : core::paper_size_mm(paper);
        if (!size) {
            std::string known;
            for (const char* name : core::paper_names())
                if (!core::is_custom_paper(name))
                    known += (known.empty() ? "" : ", ") + std::string(name);
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tanınmayan kâğıt: '" + ctx.argument("kagit").as_text() +
                           "'. Kâğıtlar: " + known + "; başka bir ölçü için en= ve boy= verin.");
            co_return;
        }

        // A SHEET IS LANDSCAPE BY DEFAULT: a plan is drawn across the page, and
        // A3 at 1:1000 is 420 m east–west by 297 m north–south. `yon=dikey`
        // stands the sheet upright.
        orientation = "yatay";
        if (const Value v = ctx.argument("yon"); !v.empty()) {
            if (core::turkish_key_equals(v.as_text(), "dikey")) {
                orientation = "dikey";
            } else if (!core::turkish_key_equals(v.as_text(), "yatay")) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Beklenen yön: yatay | dikey. Girilen: '" + v.as_text() + "'");
                co_return;
            }
        }

        // THE SCALE IS RECORDED RESOLVED, as YAZDIR records it: written down, it
        // is what lets a replay draw the same box after somebody changes the plan
        // scale. A scale that was given and is no scale — a zero — is NOT read as
        // "not given" and quietly replaced by the plan's own: the range is the
        // bus's to check, and this refuses whatever slips past it.
        scale = ctx.has_argument("olcek")
                    ? ctx.argument("olcek").as_int()
                    : ctx.session().bus().project_settings().get("core.plan.olcek").as_int();
        if (scale <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Ölçek bilinmiyor: olcek=<N> verin (1:N) ya da projenin plan ölçeğini "
                       "ayarlayın.");
            co_return;
        }

        const std::int64_t wide   = std::max(size->first, size->second);
        const std::int64_t narrow = std::min(size->first, size->second);
        const bool landscape      = orientation == "yatay";
        width_mm                  = (landscape ? wide : narrow) * scale;
        length_mm                 = (landscape ? narrow : wide) * scale;
    } else {
        auto width = co_await ctx.number("en", "Kutunun eni (m): doğu–batı boyutu");
        if (!width) co_return;
        auto length = co_await ctx.number("boy", "Kutunun boyu (m): kuzey–güney boyutu");
        if (!length) co_return;
        typed_width = *width;
        typed_long  = *length;
        width_mm    = core::mm_from_metres(typed_width);
        length_mm   = core::mm_from_metres(typed_long);
    }
    if (width_mm <= 0 || length_mm <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kutunun eni ve boyu sıfırdan büyük olmalı; en " + metres(width_mm) + ", boy " +
                       metres(length_mm) + " geldi.");
        co_return;
    }

    // THE TURN, read under the session's convention like ÇOKGEN's `aci`: the box
    // is built square to the axes and swung about its first corner the way the
    // session's angles grow — clockwise under semt, the default.
    const core::AngleConvention convention = ctx.session().bus().angle_convention();
    const Value angle                      = ctx.argument("aci");
    double turns                           = 0.0;
    if (!angle.empty())
        turns = core::turns_from_udeg(core::udeg_from_angle(angle.as_number(), convention.unit));

    auto at = co_await ctx.point("noktalar", "Kutunun ilk köşesi (yerleştirme noktası)");
    if (!at) co_return;

    std::array<core::Point2, 4> four{};
    if (!core::box_corners(*at, width_mm, length_mm, turns, convention.rule, four)) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kutu kurulamadı: en ve boy sıfırdan büyük olmalı.");
        co_return;
    }
    if (!draw_face(ctx, four)) co_return;

    // WHAT WAS ASKED, in the form a replay reads: the corner, the size (the sheet
    // and its resolved scale, or the two metres) and the turn if there was one.
    ctx.record("yontem", Value::text("olcu"));
    ctx.record("noktalar", Value::points(Value::Points{*at}));
    if (paper_given) {
        ctx.record("kagit", Value::text(paper));
        ctx.record("yon", Value::text(orientation));
        ctx.record("olcek", Value::integer(scale));
    } else {
        ctx.record("en", Value::number(typed_width));
        ctx.record("boy", Value::number(typed_long));
    }
    if (!angle.empty()) ctx.record("aci", Value::number(angle.as_number()));

    std::string said = "Kutu çizildi: " + metres(width_mm) + " × " + metres(length_mm);
    if (paper_given)
        said += " (" + paper + " " + orientation + ", 1:" + std::to_string(scale) + ")";
    if (!angle.empty()) said += "; " + core::angle_text(turns, convention.unit) + " döndürüldü";
    ctx.echo(said + ".");
}

Task<void> run(Context& ctx)
{
    // THREE POINTS INSTEAD OF TWO, when the building is not square to the grid.
    // A cadastral sheet is full of them: a block along a road that does not run
    // east–west, a wall on a skewed boundary. `yontem=3n` gives one EDGE and
    // then a height, which is how such a shape is actually measured — two
    // corners of the wall and the depth off it (TODOS-CAD P2-4).
    std::string how = "2n";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const bool by_edge = core::turkish_key_equals(how, "3n");

    // THE TWO METHODS OF A FIELD SKETCH have prompts and records of their own
    // and leave the two-corner flow below exactly as it was.
    if (core::turkish_key_equals(how, "derinlik")) {
        co_await run_depth(ctx);
        co_return;
    }
    if (core::turkish_key_equals(how, "olcu")) {
        co_await run_size(ctx);
        co_return;
    }

    // A WORD THAT NAMES NO METHOD is the bus's to refuse: it is in `yontem`
    // as it was typed, so the declared word list catches it before this body runs
    // for a line or a script and after it for a session, which rolls the face
    // back.
    if (!only_own_parameters(ctx, by_edge ? "3n" : "2n")) co_return;
    if (!given_to_a_run_that_cannot_ask(
            ctx, by_edge ? 3 : 2, by_edge ? "3n" : "2n",
            by_edge ? "üç nokta ister: kenarın iki köşesi ve karşı kenarın geçtiği nokta"
                    : "iki karşı köşe ister"))
        co_return;

    auto first = co_await ctx.point("noktalar",
                                    by_edge ? "Bir kenarın ilk köşesi" : "Dikdörtgenin ilk köşesi");
    if (!first) co_return; // ESC before anything was drawn

    // The rubber band starts at the first corner, so the diagonal lock — polar,
    // which at half a right angle draws a square — and every object snap measure
    // from it exactly as they do for a line. Dik mod does NOT: locked to an axis
    // the opposite corner makes a rectangle with no width or no height, so the
    // aids leave it out of every rectangle's corner (`command::aids_for`).
    // Nothing here is a private input path (kentoscad.md §2.4).
    auto second = co_await ctx.point(
        "noktalar", by_edge ? "Aynı kenarın öteki köşesi" : "Karşı köşe",
        PointOptions{.rubber_band   = true,
                     .rubber_origin = *first,
                     .rubber_shape  = by_edge ? RubberShape::Line : RubberShape::Rectangle});
    if (!second) co_return;

    std::vector<core::Point2> corners;

    if (by_edge) {
        if (*first == *second) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Kenarın iki köşesi aynı nokta; bir kenar tanımlamıyor."));
            co_return;
        }
        // THE GUIDE SHOWS THE RECTANGLE, and this is a fix a user's report
        // forced: it used to ask for this point under a `Ring` preview with no
        // chain, which is an origin and a cursor — two points, and two points
        // draw nothing. Pressing "rotated rectangle" therefore gave a tool that
        // drew correctly and showed nothing on the way, which from the chair is
        // a tool that does not work. `EdgeRectangle` is handed the edge and
        // draws the four corners this command is about to make, from the very
        // function that makes them (core/polygon.hpp).
        auto across = co_await ctx.point("noktalar", "Karşı kenarın geçtiği nokta",
                                         PointOptions{.rubber_band   = true,
                                                      .rubber_origin = *second,
                                                      .rubber_shape  = RubberShape::EdgeRectangle,
                                                      .rubber_chain  = {*first, *second}});
        if (!across) co_return;

        // THE THIRD POINT GIVES THE DEPTH, not a corner: it is projected onto
        // the edge's own normal, so a hand that is a few millimetres off still
        // gets a rectangle rather than a parallelogram.
        std::array<core::Point2, 4> four{};
        if (!core::edge_rectangle_corners(*first, *second, *across, four)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Üçüncü nokta kenarın üzerinde; dikdörtgenin yüksekliği sıfır olamaz."));
            co_return;
        }

        corners.assign(four.begin(), four.end());
        ctx.record("yontem", Value::text(how));
    } else {
        if (first->x == second->x || first->y == second->y) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Bu iki köşe bir alan kapatmaz: karşı köşenin hem doğusu hem kuzeyi "
                       "ilkinden farklı olmalı.");
            co_return;
        }

        // Counter-clockwise from the first corner. The ring geometry closes it,
        // so the fourth vertex is the last one written (R11).
        corners = {
            core::Point2{first->x, first->y},
            core::Point2{second->x, first->y},
            core::Point2{second->x, second->y},
            core::Point2{first->x, second->y},
        };
    }

    if (!draw_face(ctx, corners)) co_return;

    // THE TWO CORNERS ARE RECORDED, not the four that were derived from them.
    // A journal line has to replay to the same document, and replaying the two
    // corners through this command derives the same four — while recording four
    // would let a later edit move one of them and leave a "rectangle" that is
    // not one (Article 1.4).
    if (by_edge)
        ctx.record("noktalar", Value::points(Value::Points{
                                   *first, *second, core::Point2{corners[3].x, corners[3].y}}));
    else
        ctx.record("noktalar", Value::points(Value::Points{*first, *second}));
}

/// The sheet sizes a box may be cut from, in the order the table lists them —
/// every size `core::paper_size_mm` knows and none of the `ozel` slot, which is a
/// size the user gives, not one the table has. Read from the table rather than
/// listed again (CLAUDE.md 5.10).
std::vector<std::string> sheet_papers()
{
    std::vector<std::string> names;
    for (const char* name : core::paper_names())
        if (!core::is_custom_paper(name)) names.emplace_back(name);
    return names;
}

/// The same names as one sentence's list: `A5, A4, A3, A2, A1, A0`.
std::string sheet_paper_list()
{
    std::string list;
    for (const std::string& name : sheet_papers())
        list += (list.empty() ? "" : ", ") + name;
    return list;
}

} // namespace

KENTOS_COMMAND(rectangle)
{
    return CommandSpec{
        .id       = "core.rectangle",
        .names    = {"DİKDÖRTGEN", "DIKDORTGEN", "RECTANGLE", "DKD", "REC"},
        .known_as = {{"KUTU", "Netcad"}}, // a caption, a layer, a SEÇ mode: never a name
        .title    = "Dikdörtgen",
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity{1, 3},
                              "2n: karşılıklı iki köşe · 3n: bir kenarın iki köşesi ve karşı "
                              "kenarın geçtiği nokta · derinlik: bir kenarın iki köşesi · olcu: "
                              "kutunun ilk köşesi")
                    .en("points"),
                Param::choice("yontem", Arity::optional(), {"2n", "3n", "derinlik", "olcu"},
                              "2n: karşılıklı iki köşe, eksenlere paralel · 3n: bir kenar ve "
                              "yükseklik, döndürülmüş · derinlik: bir kenar ve derinlik, sağ artı, "
                              "sol eksi · olcu: bir köşe, en ve boy ya da kâğıt boyu")
                    .en("method"),
                Param::number("derinlik", Arity::optional(),
                              "yontem=derinlik için kenardan karşı kenara uzaklık (m); ilk "
                              "köşeden ikinciye bakarken SAĞ pozitif, sol negatiftir")
                    .measured_in("m")
                    .en("depth"),
                Param::number("en", Arity::optional(),
                              "yontem=olcu için kutunun eni (m); aci verilmemişken doğu–batı "
                              "boyutu")
                    .measured_in("m")
                    .en("width"),
                Param::number("boy", Arity::optional(),
                              "yontem=olcu için kutunun boyu (m); aci verilmemişken kuzey–güney "
                              "boyutu")
                    .measured_in("m")
                    .en("length"),
                Param::choice("kagit", Arity::optional(), sheet_papers(),
                              "yontem=olcu için kâğıt boyu (" + sheet_paper_list() +
                                  "): en ve boy kâğıdın ölçüsü çarpı ölçek paydasıdır; en ve boy "
                                  "ile birlikte verilmez")
                    .en("paper"),
                Param::choice("yon", Arity::optional(), {"yatay", "dikey"},
                              "kagit ile: yatay (varsayılan) kâğıdın uzun kenarı doğu–batı, dikey "
                              "kuzey–güney")
                    .en("orientation"),
                Param::integer_range("olcek", Arity::optional(), 1, 1000000,
                                     "kagit ile: ölçek paydası (1:N); verilmezse projenin plan "
                                     "ölçeği (AYAR plan_ölçeği)")
                    .en("scale"),
                Param::number("aci", Arity::optional(),
                              "yontem=olcu için kutunun dönüklüğü: oturumun açı biriminde "
                              "(öntanımlı grad), açıların arttığı yönde (öntanımlı kuzeyden saat "
                              "yönünde) döner; varsayılan 0")
                    .en("angle"),
                Param::draw_layer(),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Karşılıklı iki köşeden, bir kenar ve yükseklikten ya da derinlikten, ya da "
                   "bir köşe ve ölçüden (en, boy, kâğıt boyu) dört köşeli kapalı bir alan çizer.",
        .run = &run,
    };
}

} // namespace kentos::command
