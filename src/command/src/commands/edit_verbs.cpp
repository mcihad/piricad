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
#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/break_run.hpp"
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
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.");
        return false;
    }
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1 || doc.geometry().ring_role[span.first] != core::RingRole::Open) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Nesne " + std::to_string(id) + " açık bir çizgi değil.");
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
        ctx.refuse(st.error());
        return false;
    }
    return true;
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

    // ANY PATH, NOT ONLY A LINE (TODOS C-05): the piece taken out of an arc is
    // an arc, and what stays of a circle is the arc left over. A face is not
    // broken — a parcel opened along its boundary is two lines, not a parcel —
    // and says how it is opened on purpose.
    const core::Document& doc = ctx.document();
    const auto id             = chosen.front();
    const core::EntityId slot =
        id > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(id)))
               : core::kNoEntity;
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        co_return;
    }
    const auto path = core::path_of(doc, slot, core::PathScope::Curves);
    if (!path) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " kırılamıyor; KIR çizgi, yay, daire, elips, spline ve yaylı çoklu "
                       "çizgide çalışır.");
        co_return;
    }
    if (path->closed && doc.entities().kind[slot] == core::kPolylineKind) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " bir alan; alan kırılmaz. Önce ÇİZGİDÜZENLE islem=ac ile açık çizgiye "
                       "çevirin.");
        co_return;
    }

    auto first = co_await ctx.point("birinci", "Kırılacak parçanın ilk noktası");
    if (!first) co_return;

    // ONE POINT IS A SPLIT WITH NO GAP on an open path, which is AutoCAD's
    // `break at point` and the degenerate case of the same verb. Two points
    // remove what is between — and while the second is aimed, the piece that
    // will go is drawn as going, by the function this body cuts with
    // (`core::break_path`).
    core::Point2 second = *first;
    if (const Value v = ctx.argument("ikinci"); !v.empty() && !v.as_points().empty()) {
        second = v.as_points().front();
    } else {
        auto asked = co_await ctx.point(
            "ikinci", "Kırılacak parçanın ikinci noktası",
            PointOptions{.rubber_band    = true,
                         .rubber_origin  = *first,
                         .rubber_shape   = RubberShape::Break,
                         .rubber_payload = core::encode_break_guide(core::BreakGuide{id})});
        if (asked) second = *asked;
    }

    auto cut = core::break_path(*path, *first, second);
    if (!cut) {
        ctx.session().fail(cut.error());
        co_return;
    }
    PathEdit edit;
    if (!replace_with_pieces(ctx, slot, cut.value().kept, edit)) co_return;

    ctx.record("nesne", Value::ids({id}));
    ctx.record("birinci", Value::point(cut.value().first));
    ctx.record("ikinci", Value::point(cut.value().second));
    ctx.report(edits_json({edit}));
    if (path->closed)
        ctx.echo("Kapalı şekil kırıldı; açık bir parça kaldı.");
    else if (cut.value().kept.size() >= 2)
        ctx.echo("Çizgi kırıldı; iki parça kaldı.");
    else
        ctx.echo("Çizginin bir ucu kırıldı.");
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

    // THE TOLERANCE IS A PARAMETER, IN METRES, AND IT IS NOT THE PICK
    // TOLERANCE. How close two ends have to be to count as touching is a
    // property of the survey, not of the mouse: a 1 mm default is the one a
    // cadastral drawing wants and a metre is what a hand-digitised map needs.
    core::Mm tolerance = 1;
    if (const Value v = ctx.argument("tolerans"); !v.empty())
        tolerance = core::mm_from_metres(v.as_number());
    if (tolerance < 0) tolerance = 0;

    // WHAT A CONFLICT DOES: `ilk` keeps the first object's layer, style and
    // attributes and says what differed; `reddet` refuses and says what.
    std::string on_conflict = "ilk";
    if (const Value v = ctx.argument("cakisma"); !v.empty()) on_conflict = v.as_text();
    const bool refuse_conflicts = core::turkish_key_equals(on_conflict, "reddet");

    // LINES, ARCS AND BENT POLYLINES (TODOS C-05): each walked as a path, so
    // an arc joins as an arc and is never replaced by its chord. A closed
    // shape has no ends to join.
    const core::Document& doc = ctx.document();
    std::vector<core::EntityId> slots;
    std::vector<core::CurvePath> paths;
    for (const std::int64_t id : chosen) {
        const core::EntityId slot =
            id > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(id)))
                   : core::kNoEntity;
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
            co_return;
        }
        auto path = core::path_of(doc, slot);
        if (!path) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(id) +
                           " uç uca eklenemiyor; UÇUCA çizgi, yay ve yaylı çoklu çizgide çalışır.");
            co_return;
        }
        if (path->closed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Nesne " + std::to_string(id) +
                           " kapalı; ucu olmayan bir şekil uç uca eklenmez.");
            co_return;
        }
        slots.push_back(slot);
        paths.push_back(std::move(*path));
    }

    const core::PathJoin join = core::join_paths(paths, tolerance);
    if (join.joined.size() < 2) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Seçilen çizgilerin uçları birbirine değmiyor (tolerans " + std::to_string(tolerance) +
                " mm). Uçları yakalama açıkken yeniden çizin ya da tolerans= ile büyütün."));
        co_return;
    }

    // THE CONFLICTS, measured over what joined: the layers, and every
    // attribute column whose values are not the first object's.
    const core::EntityId first = slots[join.joined.front()];
    std::size_t other_layers   = 0;
    for (const std::size_t j : join.joined)
        if (doc.entities().layer[slots[j]] != doc.entities().layer[first]) ++other_layers;
    std::vector<std::string> differing;
    const core::AttrTable& table = doc.attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto col  = static_cast<core::AttrId>(c);
        const auto mine = doc.attribute(col, first);
        for (const std::size_t j : join.joined) {
            const auto theirs     = doc.attribute(col, slots[j]);
            const bool mine_set   = mine && mine.value().present;
            const bool theirs_set = theirs && theirs.value().present;
            if (!theirs_set) continue;
            if (!mine_set || !(mine.value() == theirs.value())) {
                if (const core::AttrColumn* column = table.column(col); column != nullptr)
                    differing.push_back(column->spec().id);
                break;
            }
        }
    }
    const std::string layer_name = doc.layers()[doc.entities().layer[first]].name;
    if (refuse_conflicts && (other_layers != 0 || !differing.empty())) {
        std::string what;
        if (other_layers != 0) what += "katman";
        for (const std::string& name : differing)
            what += (what.empty() ? "" : ", ") + name;
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "UÇUCA: birleşecek nesneler farklı — " + what +
                          ". İlk nesnenin değerleriyle birleştirmek için cakisma=ilk verin."));
        co_return;
    }

    // THE RESULT IN ITS OWN KIND, drawn like the first object: in place when
    // the first can hold it — its key stays — and new when the kind changes
    // (lines joined to an arc are a bent polyline).
    std::vector<std::int64_t> result;
    const auto key_of = [&doc](core::EntityId e) {
        return static_cast<std::int64_t>(core::raw(doc.key_of(e)));
    };
    const bool in_place = core::path_record(join.chain).kind == doc.entities().kind[first];
    if (in_place) {
        if (!write_path(ctx, first, join.chain)) co_return;
        result.push_back(key_of(first));
    } else if (!add_path_like(ctx, first, join.chain, result)) {
        co_return;
    }
    for (const std::size_t j : join.joined) {
        if (in_place && slots[j] == first) continue;
        if (const auto st = ctx.transaction().erase_entity(slots[j]); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    std::vector<PathEdit> edits;
    edits.reserve(join.joined.size());
    for (const std::size_t j : join.joined)
        edits.push_back(PathEdit{.source = chosen[j], .result = result});
    ctx.record("nesne", Value::ids(chosen));
    if (const Value v = ctx.argument("tolerans"); !v.empty()) ctx.record("tolerans", v);
    if (refuse_conflicts) ctx.record("cakisma", Value::text(on_conflict));
    ctx.report(edits_json(edits));

    // WHAT WAS DONE, AND WHAT WAS ADDED OR DECIDED TO DO IT — the gaps bridged,
    // the layer kept, the values that were not the same — because a join that
    // hides its tolerance hides a geometry change.
    std::string said = std::to_string(join.joined.size()) + " çizgi tek bir çizgiye eklendi (" +
                       std::to_string(core::path_record(join.chain).ring.size()) + " köşe).";
    if (join.bridged != 0)
        said += "\n  " + std::to_string(join.bridged) +
                " boşluk doğru parçasıyla kapatıldı; en büyüğü " + std::to_string(join.widest) +
                " mm (tolerans " + std::to_string(tolerance) + " mm).";
    if (other_layers != 0)
        said += "\n  " + std::to_string(other_layers) +
                " çizgi başka katmandaydı; sonuç ilk çizginin katmanında (" + layer_name + ").";
    if (!differing.empty()) {
        said += "\n  Öznitelikleri farklıydı: ";
        for (std::size_t i = 0; i < differing.size(); ++i)
            said += (i == 0 ? "" : ", ") + differing[i];
        said += " — ilk çizginin değerleri kaldı.";
    }
    if (join.joined.size() < chosen.size())
        said += "\n  " + std::to_string(chosen.size() - join.joined.size()) +
                " çizginin ucu zincire değmedi ve olduğu gibi bırakıldı.";
    if (join.ends_meet)
        said += "\n  Zincirin iki ucu buluşuyor; kapalı alana çevirmek için ÇİZGİDÜZENLE "
                "islem=kapat.";
    ctx.echo(said);
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
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kırılacak nesne: çizgi, yay, daire ya da yaylı çoklu çizgi"}
                    .en("object"),
                Param::point("birinci", "Kırılacak parçanın ilk noktası").en("first"),
                Param::points("ikinci", Arity::optional(),
                              "Kırılacak parçanın ikinci noktası; verilmezse boşluk bırakmadan "
                              "böler")
                    .en("second"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizgiden, yaydan, daireden ya da yaylı çoklu çizgiden iki nokta arasındaki "
                   "parçayı çıkarır; tek nokta açık bir nesneyi boşluk bırakmadan böler.",
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
                      "Uç uca eklenecek çizgiler, yaylar ve yaylı çoklu çizgiler"}
                    .en("object"),
                Param::number("tolerans", Arity::optional(),
                              "Uçların değmiş sayılması için en büyük açıklık (m); varsayılan "
                              "0,001. Aradaki boşluk doğru parçasıyla kapatılır ve söylenir")
                    .measured_in("m")
                    .en("tolerance"),
                Param::choice("cakisma", Arity::optional(), {"ilk", "reddet"},
                              "ilk: katman, stil ve öznitelikler ilk nesneden, farklar söylenir "
                              "· reddet: katman ya da öznitelik farklıysa birleştirmez")
                    .en("on_conflict"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Uçları birbirine değen çizgileri, yayları ve yaylı çoklu çizgileri tek bir "
                   "nesneye ekler; yaylar yay kalır, boşluklar söylenir.",
        .run    = &run_join,
        .effect = Effect::DocumentEdit,
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
                      "Uzunluğu değişecek çizgi"}
                    .en("object"),
                Param::number("delta", Arity::optional(), "Eklenecek uzunluk (m); eksi kısaltır")
                    .measured_in("m")
                    .en("delta"),
                Param::number("yuzde", Arity::optional(), "İstenen uzunluk, şimdikinin yüzdesi")
                    .en("percent"),
                Param::number("toplam", Arity::optional(), "İstenen toplam uzunluk (m)")
                    .measured_in("m")
                    .en("total"),
                Param::choice("uc", Arity::optional(), {"son", "bas"},
                              "Hangi uç hareket eder; varsayılan son")
                    .en("which_end"),
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
