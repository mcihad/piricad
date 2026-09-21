// SPDX-License-Identifier: GPL-3.0-or-later
// core.break — KIR, core.join — UÇUCA, core.lengthen — UZUNLUK.
//
// Three verbs a drafter uses constantly and this program had no word for.
//
// KIR REMOVES A PIECE; BÖL DOES NOT. `BÖL` cuts a line in two and keeps both —
// which is what an ifraz needs. `KIR` takes the piece BETWEEN two points out: a
// gap for a gate in a fence, a break where a pipe crosses a wall, the notch a
// symbol sits in. Given one point it splits without removing anything, which is
// AutoCAD's `break at point` and is the degenerate case of the same verb.
//
// UÇUCA IS NOT BİRLEŞTİR. `BİRLEŞTİR` is a polygon boolean — two faces become
// one face — and `UÇUCA` is a topological join: runs whose ends touch become one
// run. They are different questions with confusable names, so each page names
// the other (docs.md).
//
// UZUNLUK MOVES AN END ALONG ITS OWN DIRECTION. `UZAT` extends to a boundary,
// which needs something to extend TO; this is the form a plan gives — "make the
// kerb 2 m longer", "bring it to 48 m" — and it needs nothing but the line.
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The vertices of an OPEN run, or false with the reason echoed.
bool open_run(Context& ctx, std::int64_t id, core::EntityId& slot, std::vector<core::Point2>& pts)
{
    const core::Document& doc = ctx.document();
    const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    slot                      = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.echo("Nesne " + std::to_string(id) +
                 " bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.");
        return false;
    }
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1 || doc.geometry().ring_role[span.first] != core::RingRole::Open) {
        ctx.echo("Nesne " + std::to_string(id) + " açık bir çizgi değil.");
        return false;
    }
    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    pts.clear();
    pts.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        pts.push_back(core::Point2{xs[v], ys[v]});
    return true;
}

bool write_run(Context& ctx, core::EntityId slot, const std::vector<core::Point2>& pts)
{
    const core::RingGeometry::RingInput ring{pts, core::RingRole::Open, 0};
    auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
    if (!st) {
        ctx.echo(st.error().message);
        return false;
    }
    return true;
}

/// Where along `pts` the point `at` falls: the segment index and the fraction
/// along it, plus the foot itself. False when the run has no segments.
bool locate(const std::vector<core::Point2>& pts, core::Point2 at, std::size_t& segment,
            double& along, core::Point2& foot)
{
    if (pts.size() < 2) return false;

    double best = 0.0;
    bool found  = false;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        core::Point2 candidate{};
        double t = 0.0;
        if (!core::closest_point_on_line(pts[i], pts[i + 1], at, candidate, t)) continue;
        const double clamped = std::clamp(t, 0.0, 1.0);
        if (clamped != t) {
            const double dx = static_cast<double>(pts[i + 1].x - pts[i].x);
            const double dy = static_cast<double>(pts[i + 1].y - pts[i].y);
            candidate       = core::Point2{pts[i].x + core::mm_round(clamped * dx),
                                     pts[i].y + core::mm_round(clamped * dy)};
        }
        const double d = core::distance_squared(at, candidate);
        if (!found || d < best) {
            found   = true;
            best    = d;
            segment = i;
            along   = clamped;
            foot    = candidate;
        }
    }
    return found;
}

/// The run up to `(segment, along)`, and the run from it onwards.
void cut_at(const std::vector<core::Point2>& pts, std::size_t segment, double along,
            core::Point2 foot, std::vector<core::Point2>& head, std::vector<core::Point2>& tail)
{
    head.clear();
    tail.clear();
    for (std::size_t i = 0; i <= segment; ++i)
        head.push_back(pts[i]);
    if (along > 0.0) head.push_back(foot);

    if (along < 1.0) tail.push_back(foot);
    for (std::size_t i = segment + 1; i < pts.size(); ++i)
        tail.push_back(pts[i]);
}

// -------------------------------------------------------------------- KIR ----

Task<void> run_break(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Kırılacak çizgiyi seçin, Enter'a basın", chosen, 1,
                               "KIR nesne=1 birinci=10,0 ikinci=20,0"))
        co_return;
    if (chosen.size() != 1) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "KIR tek bir çizgiyle çalışır; " +
                                         std::to_string(chosen.size()) + " nesne seçildi."));
        co_return;
    }

    core::EntityId slot = core::kNoEntity;
    std::vector<core::Point2> pts;
    if (!open_run(ctx, chosen.front(), slot, pts)) co_return;

    auto first = co_await ctx.point("birinci", "Kırılacak parçanın ilk noktası");
    if (!first) co_return;

    // ONE POINT IS A SPLIT WITH NO GAP, which is AutoCAD's `break at point` and
    // the degenerate case of the same verb. Two points remove what is between.
    core::Point2 second = *first;
    if (const Value v = ctx.argument("ikinci"); !v.empty() && !v.as_points().empty()) {
        second = v.as_points().front();
    } else {
        auto asked = co_await ctx.point("ikinci", "Kırılacak parçanın ikinci noktası",
                                        PointOptions{.rubber_band   = true,
                                                     .rubber_origin = *first,
                                                     .rubber_shape  = RubberShape::Line});
        if (asked) second = *asked;
    }

    std::size_t seg_a = 0;
    std::size_t seg_b = 0;
    double at_a       = 0.0;
    double at_b       = 0.0;
    core::Point2 foot_a{};
    core::Point2 foot_b{};
    if (!locate(pts, *first, seg_a, at_a, foot_a) || !locate(pts, second, seg_b, at_b, foot_b)) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Kırılma noktası çizgi üzerinde bulunamadı."));
        co_return;
    }

    // ORDERED ALONG THE LINE, not in the order they were clicked: a hand clicks
    // the far end first as often as not, and a gap is a gap either way.
    if (seg_b < seg_a || (seg_b == seg_a && at_b < at_a)) {
        std::swap(seg_a, seg_b);
        std::swap(at_a, at_b);
        std::swap(foot_a, foot_b);
    }

    std::vector<core::Point2> head;
    std::vector<core::Point2> scrap;
    cut_at(pts, seg_a, at_a, foot_a, head, scrap);
    std::vector<core::Point2> middle;
    std::vector<core::Point2> tail;
    // The tail is cut from the SCRAP, whose own indices start at the first cut.
    std::size_t seg_in_scrap = seg_b - seg_a;
    double at_in_scrap       = at_b;
    if (at_a > 0.0 && seg_b == seg_a) {
        // Both cuts on one segment: the scrap's first segment is what is left of
        // it, so the second fraction has to be re-measured against that stub.
        const double left = 1.0 - at_a;
        at_in_scrap       = left > 0.0 ? (at_b - at_a) / left : 0.0;
    }
    cut_at(scrap, seg_in_scrap, at_in_scrap, foot_b, middle, tail);

    if (head.size() < 2 && tail.size() < 2) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Kırılma çizginin tamamını götürüyor; parça bırakmıyor. Silmek için SİL kullanın."));
        co_return;
    }

    if (head.size() >= 2) {
        if (!write_run(ctx, slot, head)) co_return;
        if (tail.size() >= 2) {
            auto made = ctx.transaction().add_polyline(ctx.document().entities().layer[slot], tail);
            if (!made) {
                ctx.session().fail(made.error());
                co_return;
            }
        }
    } else if (!write_run(ctx, slot, tail)) {
        co_return;
    }

    ctx.record("nesne", Value::ids({chosen.front()}));
    ctx.record("birinci", Value::point(foot_a));
    ctx.record("ikinci", Value::point(foot_b));
    ctx.echo(head.size() >= 2 && tail.size() >= 2 ? "Çizgi kırıldı; iki parça kaldı."
                                                  : "Çizginin bir ucu kırıldı.");
}

// ------------------------------------------------------------------ UÇUCA ----

Task<void> run_join(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Uç uca eklenecek çizgileri seçin, Enter'a basın",
                               chosen, 0, "UÇUCA nesneler=1,2"))
        co_return;
    if (chosen.size() < 2) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "UÇUCA en az iki çizgi ister; " + std::to_string(chosen.size()) + " geldi."));
        co_return;
    }

    // THE TOLERANCE IS A PARAMETER, IN MILLIMETRES, AND IT IS NOT THE PICK
    // TOLERANCE. How close two ends have to be to count as touching is a
    // property of the survey, not of the mouse: a 1 mm default is the one a
    // cadastral drawing wants and a metre is what a hand-digitised map needs.
    core::Mm tolerance = 1;
    if (const Value v = ctx.argument("tolerans"); !v.empty())
        tolerance = core::mm_from_metres(v.as_number());
    if (tolerance < 0) tolerance = 0;
    const double reach = static_cast<double>(tolerance) * static_cast<double>(tolerance);

    struct Run
    {
        core::EntityId slot{core::kNoEntity};
        std::int64_t id{0};
        std::vector<core::Point2> pts;
        bool used{false};
    };

    std::vector<Run> runs;
    for (const std::int64_t id : chosen) {
        Run one;
        one.id = id;
        if (!open_run(ctx, id, one.slot, one.pts)) co_return;
        runs.push_back(std::move(one));
    }

    // Greedy chaining from the first run: take whichever remaining run touches
    // either end, flipping it if it has to be flipped.
    std::vector<core::Point2> chain = runs.front().pts;
    runs.front().used               = true;
    std::size_t joined              = 1;

    bool grew = true;
    while (grew) {
        grew = false;
        for (Run& one : runs) {
            if (one.used) continue;
            const auto touches = [reach](core::Point2 a, core::Point2 b) {
                return core::distance_squared(a, b) <= reach;
            };
            if (touches(chain.back(), one.pts.front())) {
                chain.insert(chain.end(), one.pts.begin() + 1, one.pts.end());
            } else if (touches(chain.back(), one.pts.back())) {
                chain.insert(chain.end(), one.pts.rbegin() + 1, one.pts.rend());
            } else if (touches(chain.front(), one.pts.back())) {
                chain.insert(chain.begin(), one.pts.begin(), one.pts.end() - 1);
            } else if (touches(chain.front(), one.pts.front())) {
                chain.insert(chain.begin(), one.pts.rbegin(), one.pts.rend() - 1);
            } else {
                continue;
            }
            one.used = true;
            ++joined;
            grew = true;
        }
    }

    if (joined < 2) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Seçilen çizgilerin uçları birbirine değmiyor (tolerans " + std::to_string(tolerance) +
                " mm). Uçları yakalama açıkken yeniden çizin ya da tolerans= ile büyütün."));
        co_return;
    }

    if (!write_run(ctx, runs.front().slot, chain)) co_return;
    for (const Run& one : runs) {
        if (!one.used || one.slot == runs.front().slot) continue;
        auto st = ctx.transaction().erase_entity(one.slot);
        if (!st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    ctx.record("nesne", Value::ids(chosen));
    ctx.echo(std::to_string(joined) + " çizgi tek bir çizgiye eklendi (" +
             std::to_string(chain.size()) + " köşe)." +
             (joined < chosen.size() ? " " + std::to_string(chosen.size() - joined) +
                                           " çizginin ucu zincire değmedi ve olduğu gibi bırakıldı."
                                     : ""));
}

// --------------------------------------------------------------- UZUNLUK ----

Task<void> run_lengthen(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Uzunluğu değişecek çizgiyi seçin, Enter'a basın",
                               chosen, 1, "UZUNLUK nesne=1 delta=5"))
        co_return;
    if (chosen.size() != 1) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument, "UZUNLUK tek bir çizgiyle çalışır."));
        co_return;
    }

    core::EntityId slot = core::kNoEntity;
    std::vector<core::Point2> pts;
    if (!open_run(ctx, chosen.front(), slot, pts)) co_return;
    if (pts.size() < 2) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument, "Çizginin iki köşesi yok."));
        co_return;
    }

    double total = 0.0;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        total += core::mm_to_metres(core::segment_length(pts[i], pts[i + 1]));

    // ONE OF THREE, AND EXACTLY ONE. `delta` adds, `yuzde` scales, `toplam`
    // states the answer; giving two is a caller that does not know which it
    // means, and guessing is what a refusal is for.
    Value delta         = ctx.argument("delta");
    const Value percent = ctx.argument("yuzde");
    const Value target  = ctx.argument("toplam");

    // NONE GIVEN MEANS ASK, and `delta` is the one asked for.
    //
    // It read the three from arguments only, so pressing UZUNLUK in the tool
    // column asked which object and then REFUSED, telling the user to type
    // `delta=` — a command reachable by mouse that cannot be finished by one
    // (CLAUDE.md 5.15). `delta` is the question a hand asks ("lengthen this end
    // by so much"); `yuzde` and `toplam` stay the typed and scripted roads,
    // which is what their own page says.
    //
    // The emptiness of the argument decides, never `InputSource` (command.md
    // P10): a script that gave `toplam=` is not asked anything.
    if (delta.empty() && percent.empty() && target.empty()) {
        auto asked =
            co_await ctx.number("delta", "Eklenecek uzunluk (m); eksi kısaltır. Şimdiki uzunluk " +
                                             metres_text(core::mm_from_metres(total)) + " m");
        if (!asked) co_return;
        delta = Value::number(*asked);
    }

    const int given =
        (delta.empty() ? 0 : 1) + (percent.empty() ? 0 : 1) + (target.empty() ? 0 : 1);
    if (given != 1) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Tam olarak birini verin: delta= (eklenecek metre), yuzde= (oran) ya da toplam= "
            "(istenen uzunluk). Şimdiki uzunluk " +
                std::to_string(total) + " m."));
        co_return;
    }

    double wanted = total;
    if (!delta.empty())
        wanted = total + delta.as_number();
    else if (!percent.empty())
        wanted = total * percent.as_number() / 100.0;
    else
        wanted = target.as_number();

    if (wanted <= 0.0) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "İstenen uzunluk sıfır ya da eksi olamaz."));
        co_return;
    }

    // WHICH END MOVES. The default is the last, because a line is drawn towards
    // where it is going and that is the end a drafter reaches for.
    bool at_start = false;
    if (const Value v = ctx.argument("uc"); !v.empty())
        at_start = core::turkish_key_equals(v.as_text(), "bas");

    const std::size_t moving = at_start ? 0 : pts.size() - 1;
    const std::size_t anchor = at_start ? 1 : pts.size() - 2;

    // The last segment carries the change; the rest of the run is untouched,
    // which is what "make it 2 m longer" means on a multi-vertex line.
    const double others =
        total - core::mm_to_metres(core::segment_length(pts[anchor], pts[moving]));
    const double segment_wanted = wanted - others;
    if (segment_wanted <= 0.0) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "İstenen uzunluk, hareket etmeyen köşelerin toplamından küçük; son parça eksi "
            "uzunlukta olamaz."));
        co_return;
    }

    const double dx  = static_cast<double>(pts[moving].x - pts[anchor].x);
    const double dy  = static_cast<double>(pts[moving].y - pts[anchor].y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len == 0.0) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Son parçanın iki köşesi aynı; doğrultusu yok."));
        co_return;
    }
    const double want_mm = segment_wanted * static_cast<double>(core::kMmPerMetre);
    pts[moving]          = core::Point2{pts[anchor].x + core::mm_round(want_mm * dx / len),
                               pts[anchor].y + core::mm_round(want_mm * dy / len)};

    if (!write_run(ctx, slot, pts)) co_return;

    ctx.record("nesne", Value::ids({chosen.front()}));
    ctx.record("toplam", Value::number(wanted));
    ctx.echo("Çizgi uzunluğu " + std::to_string(total) + " m -> " + std::to_string(wanted) +
             " m yapıldı.");
}

} // namespace

KENTOS_COMMAND(break_line)
{
    return CommandSpec{
        .id       = "core.break",
        .names    = {"KIR", "BREAK", "KR"},
        .title    = "Kır",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Kırılacak çizgi"},
                Param::point("birinci", "Kırılacak parçanın ilk noktası"),
                Param::points("ikinci", Arity::optional(),
                              "Kırılacak parçanın ikinci noktası; verilmezse boşluk bırakmadan "
                              "böler"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta verilirse boşluk "
                   "bırakmadan böler.",
        .run    = &run_break,
        .effect = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(join_lines)
{
    return CommandSpec{
        .id       = "core.join",
        .names    = {"UÇUCA", "UCUCA", "JOIN", "UÇE"},
        .title    = "Uç Uca Ekle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Uç uca eklenecek çizgiler"},
                Param::number("tolerans", Arity::optional(),
                              "Uçların değmiş sayılması için en büyük açıklık (m); varsayılan "
                              "0,001")
                    .measured_in("m"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Uçları birbirine değen çizgileri tek bir çizgiye ekler.",
        .run     = &run_join,
        .effect  = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(lengthen)
{
    return CommandSpec{
        .id       = "core.lengthen",
        .names    = {"UZUNLUK", "LENGTHEN", "UZN"},
        .title    = "Uzunluk",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Uzunluğu değişecek çizgi"},
                Param::number("delta", Arity::optional(), "Eklenecek uzunluk (m); eksi kısaltır")
                    .measured_in("m"),
                Param::number("yuzde", Arity::optional(), "İstenen uzunluk, şimdikinin yüzdesi"),
                Param::number("toplam", Arity::optional(), "İstenen toplam uzunluk (m)")
                    .measured_in("m"),
                Param::choice("uc", Arity::optional(), {"son", "bas"},
                              "Hangi uç hareket eder; varsayılan son"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu "
                   "değiştirir.",
        .run     = &run_lengthen,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
