// SPDX-License-Identifier: GPL-3.0-or-later
// core.layout (ÇIKTIYERLEŞİMİ), core.layout_item (ÇIKTIÖĞE),
// core.layout_template (ÇIKTIŞABLON)
//
// THE SHEET IS A COMMAND SURFACE, not a window's private state. An output
// layout is
// composed by dragging boxes on a page, and a designer that owned that
// composition would be a capability no script, no batch job and no model could
// ever reach (CLAUDE.md 5.15, 1.2). So every edit a hand makes in the designer
// leaves as one of these two lines: the window is a client that types.
//
// TWO COMMANDS, ALONG THE SEAM A USER ALREADY FEELS. `ÇIKTIYERLEŞİMİ` is about
// SHEETS — make one, remove one, rename it, change its paper. `ÇIKTIÖĞE` is about what is
// ON one — add a map frame, move it, retype the title. Merging them would make
// `ad=` mean two things depending on another argument, which is the shape the
// parameter validator cannot check and a reader cannot remember.
//
// WHOLE-LIST WRITES. Both read `document().layouts().all()`, change their copy
// and hand it back through `Transaction::set_layouts` — one mutator, one Op kind
// and one undo entry for a subsystem that would otherwise need a dozen of each
// (`core/layout.hpp` says why an index-based inverse would be wrong).
//
// NOT `AiAccessible`, AND THE REASON IS A RULE RATHER THAN AN OVERSIGHT. A map
// item carries a ground extent, and CLAUDE.md 5.8 requires every coordinate in a
// model-generated command to trace to a recorded tool-call result. The handle
// machinery (`ai/handles.hpp`) resolves handles into `Args` for GROUND commands;
// a paper-space command that takes both paper millimetres and a ground window
// needs its own answer to which of the two a handle may fill. Until that is
// designed these two are typed by a person or a script, and a model composes a
// sheet by asking the person to run them.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

using core::Layout;
using core::LayoutItem;
using core::LayoutItemKind;
using core::Um;

/// Paper millimetres as the micrometres everything below stores.
Um um(std::int64_t mm)
{
    return core::um_from_mm(mm);
}

/// PAPER MILLIMETRES AS A PERSON WRITES THEM, which includes a decimal point.
///
/// A title block at 20 mm is one somebody can describe; a box 0.35 mm from the
/// edge is a real placement a draughtsman makes, and these parameters were whole
/// numbers — so it could not be said at all, from the command line, from a script
/// or from an agent. The model has been micrometres all along; only the door was
/// integer (TODOS L-03).
///
/// Rounded to the micrometre, the model's own resolution: 0.35 mm is 350 µm
/// exactly, and half a micrometre is a distance nobody can draw or measure.
Um um_of(double mm)
{
    return static_cast<Um>(std::llround(mm * 1000.0));
}

/// And back, for a line a person reads.
std::int64_t mm(Um value)
{
    return value / 1000;
}

/// The verb word, canonicalised to the lowercase spelling the parameter declares.
///
/// `turkish_fold_key` UPPERCASES — it is the key a NAME lookup uses — so folding
/// a verb and comparing it to a lowercase literal never matches. The words are
/// compared with `turkish_key_equals`, which is what every other `islem` in this
/// program does (`layer_visibility.cpp`), so the dotless-i spelling `tasi` and
/// the dotted `taşı` reach the same operation.
const char* canonical_verb(std::string_view typed, std::span<const char* const> words)
{
    for (const char* word : words)
        if (core::turkish_key_equals(typed, word)) return word;
    return nullptr;
}

/// The layout the `yerlesim` argument names, or the only one when there is one and
/// the argument is empty.
///
/// A DRAWING USUALLY HAS ONE SHEET, and making every `ÇIKTIÖĞE` line name it
/// would be ceremony. Two or more and the argument is required, because guessing
/// which of two layouts a command meant is how the wrong sheet gets edited.
const Layout* resolve(const core::LayoutStore& store, const std::string& named,
                      std::string& trouble)
{
    if (!named.empty()) {
        const Layout* found = store.find(named);
        if (found == nullptr) trouble = "Çıktı yerleşimi yok: '" + named + "'.";
        return found;
    }
    if (store.empty()) {
        trouble = "Çizimde hiç çıktı yerleşimi yok. Önce ÇIKTIYERLEŞİMİ islem=ekle ad=<ad> yazın.";
        return nullptr;
    }
    if (store.size() > 1) {
        trouble = "Çizimde " + std::to_string(store.size()) +
                  " çıktı yerleşimi var; hangisi olduğunu yazın: yerlesim=<ad>";
        return nullptr;
    }
    return &store.all().front();
}

/// An id nothing in `layout` answers to yet: `harita`, then `harita2`, …
std::string free_id(const Layout& layout, const std::string& stem)
{
    if (layout.find(stem) == nullptr) return stem;
    for (int n = 2; n < 1000; ++n) {
        const std::string tried = stem + std::to_string(n);
        if (layout.find(tried) == nullptr) return tried;
    }
    return stem;
}

std::string describe(const Layout& l)
{
    std::string out = l.name + " — ";
    if (!l.paper.empty()) out += l.paper + " ";
    out += std::to_string(mm(l.pages.front().w)) + "×" + std::to_string(mm(l.pages.front().h)) +
           " mm, " + (l.landscape ? "yatay" : "dikey") + ", " + std::to_string(l.items.size()) +
           " öğe";
    if (l.pages.size() > 1) out += ", " + std::to_string(l.pages.size()) + " sayfa";
    return out;
}

// ==================================================== ÇIKTIYERLEŞİMİ ========

Task<void> run_layout(Context& ctx)
{
    Bus& bus                      = ctx.session().bus();
    const core::LayoutStore& have = bus.document().layouts();

    static constexpr const char* kVerbs[] = {"listele",   "ekle",      "sil",      "ad",
                                             "sayfa",     "sayfaekle", "sayfasil", "sayfacogalt",
                                             "sayfatasi", "denetle",   "atlas",    "rapor"};
    auto verb = co_await ctx.text("islem", "İşlem: listele / ekle / sil / ad / sayfa / sayfaekle / "
                                           "sayfasil / sayfacogalt / sayfatasi / denetle / atlas");
    if (!verb) co_return;
    const char* resolved = canonical_verb(*verb, kVerbs);
    if (resolved == nullptr) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Tanınmayan işlem: '" + *verb +
                          "'. İşlemler: listele / ekle / sil / ad / sayfa / "
                          "sayfaekle / sayfasil / sayfacogalt / sayfatasi / denetle / atlas"));
        co_return;
    }
    const std::string op = resolved;
    ctx.record("islem", Value::text(op));

    if (op == "listele") {
        if (have.empty()) {
            ctx.echo("Çizimde çıktı yerleşimi yok.");
            co_return;
        }
        std::string said = std::to_string(have.size()) + " çıktı yerleşimi:";
        for (const Layout& l : have.all())
            said += "\n  " + describe(l);
        ctx.echo(said);
        co_return;
    }

    auto named = co_await ctx.text("ad", "Çıktı yerleşiminin adı");
    if (!named || named->empty()) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument, "Yerleşim adı gerekir: ad=<ad>"));
        co_return;
    }
    ctx.record("ad", Value::text(*named));

    std::vector<Layout> next = have.all();

    if (op == "sil") {
        const auto at = std::find_if(next.begin(), next.end(), [&](const Layout& l) {
            return core::turkish_key_equals(l.name, *named);
        });
        if (at == next.end()) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Çıktı yerleşimi yok: '" + *named + "'."));
            co_return;
        }
        next.erase(at);
        if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ctx.echo("Çıktı yerleşimi silindi: " + *named + ".");
        co_return;
    }

    if (op == "ad") {
        const Value fresh = ctx.argument("yeni_ad");
        if (fresh.empty()) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Yeni ad gerekir: yeni_ad=<ad>"));
            co_return;
        }
        Layout* target = nullptr;
        for (Layout& l : next)
            if (core::turkish_key_equals(l.name, *named)) target = &l;
        if (target == nullptr) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Çıktı yerleşimi yok: '" + *named + "'."));
            co_return;
        }
        ctx.record("yeni_ad", fresh);
        target->name = fresh.as_text();
        if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ctx.echo("Yerleşim adı: " + *named + " → " + fresh.as_text());
        co_return;
    }

    // ---- ekle and sayfa both decide a page size, so they read it the same way -
    const Value paper_arg = ctx.argument("kagit");
    const Value w_arg     = ctx.argument("genislik");
    const Value h_arg     = ctx.argument("yukseklik");
    const Value dir_arg   = ctx.argument("yon");

    std::string paper =
        paper_arg.empty() ? std::string("A4") : core::canonical_paper(paper_arg.as_text());
    std::int64_t width_mm  = 0;
    std::int64_t height_mm = 0;

    if (core::is_custom_paper(paper)) {
        if (w_arg.empty() || h_arg.empty() || w_arg.as_int() <= 0 || h_arg.as_int() <= 0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "ozel kâğıt için genislik ve yukseklik milimetre "
                                         "olarak verilmeli (sıfırdan büyük)."));
            co_return;
        }
        width_mm  = w_arg.as_int();
        height_mm = h_arg.as_int();
    } else {
        const auto size = core::paper_size_mm(paper);
        if (!size) {
            std::string names;
            for (const char* n : core::paper_names())
                names += (names.empty() ? "" : ", ") + std::string(n);
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Tanınmayan kâğıt: '" + paper + "'. Kâğıtlar: " + names + "."));
            co_return;
        }
        width_mm  = size->first;
        height_mm = size->second;
    }

    const bool landscape = !dir_arg.empty() && core::turkish_key_equals(dir_arg.as_text(), "yatay");
    if (landscape) std::swap(width_mm, height_mm);

    ctx.record("kagit", Value::text(paper));
    ctx.record("yon", Value::text(landscape ? "yatay" : "dikey"));
    if (core::is_custom_paper(paper)) {
        ctx.record("genislik", Value::integer(landscape ? height_mm : width_mm));
        ctx.record("yukseklik", Value::integer(landscape ? width_mm : height_mm));
    }

    const Value margin_arg  = ctx.argument("kenar");
    const std::int64_t edge = margin_arg.empty() ? 10 : margin_arg.as_int();
    if (edge < 0 || edge * 2 >= std::min(width_mm, height_mm)) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Kenar boşluğu sayfanın içinde kalmalı: 0 ile " +
                                         std::to_string(std::min(width_mm, height_mm) / 2 - 1) +
                                         " mm arası."));
        co_return;
    }
    ctx.record("kenar", Value::integer(edge));

    if (op == "ekle") {
        // A NEW SHEET IS A SHEET, not an empty page: `default_layout` puts a map
        // frame, a title, a scale bar and a north arrow on it, so the first thing
        // a user sees is something printable they can move, rather than a white
        // rectangle and a manual.
        Layout fresh    = core::default_layout(*named, um(width_mm), um(height_mm), um(edge));
        fresh.paper     = paper;
        fresh.landscape = landscape;
        if (const Value dpi_arg = ctx.argument("dpi"); !dpi_arg.empty()) {
            fresh.dpi = static_cast<std::int32_t>(dpi_arg.as_int());
            ctx.record("dpi", dpi_arg);
        }

        // AN EXISTING NAME IS REPLACED, which is what `ekle` means everywhere
        // else in this program (`YAZDIRMAPROFİLİ`, `YAPAYZEKAMODELİ`).
        const auto at = std::find_if(next.begin(), next.end(), [&](const Layout& l) {
            return core::turkish_key_equals(l.name, *named);
        });
        if (at != next.end())
            *at = std::move(fresh);
        else
            next.push_back(std::move(fresh));

        if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ctx.echo("Çıktı yerleşimi: " + describe(*bus.document().layouts().find(*named)));
        co_return;
    }

    if (op == "atlas") {
        Layout* target = nullptr;
        for (Layout& l : next)
            if (core::turkish_key_equals(l.name, *named)) target = &l;
        if (target == nullptr) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Çıktı yerleşimi yok: '" + *named + "'."));
            co_return;
        }

        if (const Value v = ctx.argument("katman"); !v.empty()) {
            if (core::turkish_key_equals(v.as_text(), "yok")) {
                target->atlas = core::Atlas{};
            } else {
                const auto& have_layers = bus.document().layers();
                const bool known        = std::any_of(
                    have_layers.begin(), have_layers.end(), [&](const core::Layer& one) {
                        return core::turkish_key_equals(one.name, v.as_text());
                    });
                if (!known) {
                    ctx.session().fail(
                        core::err(core::ErrorCode::NotFound, "Katman yok: '" + v.as_text() + "'."));
                    co_return;
                }
                target->atlas.coverage_layer = v.as_text();
            }
            ctx.record("katman", v);
        }
        if (const Value v = ctx.argument("sirala"); !v.empty()) {
            if (bus.document().attributes().find(v.as_text()) == core::kNoAttr) {
                ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                             "Öznitelik sütunu yok: '" + v.as_text() + "'."));
                co_return;
            }
            target->atlas.sort_by = v.as_text();
            ctx.record("sirala", v);
        }
        if (const Value v = ctx.argument("kenar_payi"); !v.empty()) {
            target->atlas.margin_percent = static_cast<std::uint8_t>(v.as_int());
            ctx.record("kenar_payi", v);
        }
        if (const Value v = ctx.argument("tek_dosya"); !v.empty()) {
            target->atlas.single_file = v.as_bool();
            ctx.record("tek_dosya", v);
        }

        if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
            ctx.session().fail(st.error());
            co_return;
        }

        const Layout* settled = bus.document().layouts().find(*named);
        if (settled == nullptr || settled->atlas.coverage_layer.empty()) {
            ctx.echo("'" + *named + "' bir atlas değil.");
            co_return;
        }
        // SAID BEFORE ANYTHING IS PRINTED. A run that would produce nothing has
        // to say so now, not write zero files and report success (TODOS L-10).
        const std::size_t count = core::atlas_targets(bus.document(), *settled).size();
        ctx.echo("Atlas: '" + settled->atlas.coverage_layer + "' katmanı, " +
                 std::to_string(count) + " sayfa, %" +
                 std::to_string(settled->atlas.margin_percent) + " kenar payı, " +
                 (settled->atlas.single_file ? "tek dosya" : "nesne başına dosya") + ".");
        co_return;
    }

    if (op == "rapor") {
        // A REPORT IS NOT AN ATLAS, and this verb exists because the difference
        // cannot be expressed in the other one. An atlas is a flat loop: the
        // same sheet, once per object. A report is a HIERARCHY — ada 1284 gets a
        // heading and its own totals, then each of its parcels gets a page, then
        // ada 1285 begins — and the loop has no notion of a group at all
        // (TODOS L-11).
        //
        // IT READS AND REPORTS; it does not print. What it answers is the shape
        // the run would take, so a person can see the grouping before a hundred
        // pages are written.
        const Layout* found = have.find(*named);
        if (found == nullptr) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Çıktı yerleşimi yok: '" + *named + "'."));
            co_return;
        }

        core::Report wanted;
        if (const Value v = ctx.argument("grup"); !v.empty()) {
            if (bus.document().attributes().find(v.as_text()) == core::kNoAttr) {
                ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                             "Öznitelik sütunu yok: '" + v.as_text() + "'."));
                co_return;
            }
            wanted.group_by = v.as_text();
            ctx.record("grup", v);
        }

        if (found->atlas.coverage_layer.empty()) {
            ctx.echo("'" + *named +
                     "' bir kapsama katmanına nişanlanmamış; rapor neyi bölümleyeceğini "
                     "bilemez. Önce: ÇIKTIYERLEŞİMİ islem=atlas ad=" +
                     *named + " katman=<katman>");
            co_return;
        }

        const std::vector<core::ReportGroup> groups =
            core::report_groups(bus.document(), *found, wanted);

        core::Json report;
        report.set("yerlesim", core::Json::string(found->name));
        report.set("katman", core::Json::string(found->atlas.coverage_layer));
        if (!wanted.group_by.empty()) report.set("grup", core::Json::string(wanted.group_by));

        core::Json rows   = core::Json::array({});
        std::size_t pages = 0;
        for (const core::ReportGroup& one : groups) {
            pages += one.count;
            core::Json row;
            row.set("deger", core::Json::string(one.key));
            row.set("adet", core::Json::integer(static_cast<std::int64_t>(one.count)));
            // SQUARE MILLIMETRES OF BOUNDING BOX, and the field name says which.
            // A total on a report must never be mistaken for a surveyed area.
            row.set("kutu_alani_mm2", core::Json::integer(one.bounds_area_mm2));
            rows.push(std::move(row));
        }
        report.set("bolumler", std::move(rows));
        report.set("sayfa", core::Json::integer(static_cast<std::int64_t>(pages)));
        ctx.report(std::move(report));

        if (groups.empty()) {
            ctx.echo("'" + found->atlas.coverage_layer +
                     "' katmanında raporlanacak nesne yok; hiç sayfa üretilmez.");
            co_return;
        }

        std::string said = "Rapor: " + std::to_string(groups.size()) + " bölüm, " +
                           std::to_string(pages) + " sayfa";
        if (!wanted.group_by.empty()) said += " ('" + wanted.group_by + "' ile bölümlendi)";
        said += ":";
        for (const core::ReportGroup& one : groups)
            said += "\n  " + (one.key.empty() ? std::string("(değeri yok)") : one.key) + " — " +
                    std::to_string(one.count) + " nesne";
        ctx.echo(said);
        co_return;
    }

    if (op == "denetle") {
        // READ BEFORE AN EXPORT, not after it. None of what this reports FAILS:
        // the file appears and looks finished, which is exactly why a sheet with
        // an unaimed map frame or a box hanging off the page has to be said out
        // loud (TODOS L-15).
        const Layout* found = have.find(*named);
        if (found == nullptr) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Çıktı yerleşimi yok: '" + *named + "'."));
            co_return;
        }
        const std::vector<std::string> trouble      = core::layout_trouble(*found);
        const std::vector<core::LayoutOverlap> over = core::layout_overlaps(*found);

        // WHAT COVERS WHAT, as structured data beside the prose. Most overlaps
        // are the design — a title sits ON the map frame — so they are not
        // "sorun"; but "lejant haritanın üstüne binmiş" is a thing a person
        // says, and answering it needs the program to be able to say which item
        // is over which and how much of it is hidden (TODOS A-05).
        core::Json report;
        report.set("yerlesim", core::Json::string(found->name));
        core::Json notes = core::Json::array({});
        for (const std::string& one : trouble)
            notes.push(core::Json::string(one));
        report.set("sorunlar", std::move(notes));

        core::Json covers = core::Json::array({});
        for (const core::LayoutOverlap& one : over) {
            core::Json row;
            row.set("ustte", core::Json::string(one.over));
            row.set("altta", core::Json::string(one.under));
            row.set("sayfa", core::Json::integer(one.page));
            row.set("kapanan_yuzde", core::Json::integer(one.covered_percent));
            row.set("gizliyor", core::Json::boolean(one.opaque));
            covers.push(std::move(row));
        }
        report.set("ust_uste_binen", std::move(covers));
        ctx.report(std::move(report));

        if (trouble.empty())
            ctx.echo("'" + found->name + "' yerleşiminde basmaya engel bir şey görünmüyor.");
        else {
            ctx.echo("'" + found->name + "' yerleşiminde " + std::to_string(trouble.size()) +
                     " sorun var:");
            for (const std::string& one : trouble)
                ctx.echo("  · " + one);
        }

        // SAID AFTER THE PROBLEMS AND MARKED AS NOT ONE. An overlap that hides
        // something is already in `trouble`; these are the ones a person may
        // have meant, listed so they can decide.
        if (!over.empty()) {
            ctx.echo("Üst üste binen öğeler (sorun olmayabilir):");
            for (const core::LayoutOverlap& one : over)
                ctx.echo("  · '" + one.over + "' '" + one.under + "' üzerinde, sayfa " +
                         std::to_string(one.page) + ", %" + std::to_string(one.covered_percent) +
                         (one.opaque ? " (altındakini gizliyor)" : " (saydam)"));
        }
        co_return;
    }

    // ---- the five page verbs all need the layout and most need a page index --
    if (op == "sayfa" || op == "sayfaekle" || op == "sayfasil" || op == "sayfacogalt" ||
        op == "sayfatasi") {
        Layout* target = nullptr;
        for (Layout& l : next)
            if (core::turkish_key_equals(l.name, *named)) target = &l;
        if (target == nullptr) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Çıktı yerleşimi yok: '" + *named + "'."));
            co_return;
        }

        // PAGES ARE COUNTED FROM ONE, because that is what is printed on them and
        // what a person says out loud. Only the array is zero-based.
        const Value page_arg  = ctx.argument("sayfa");
        const auto page_count = static_cast<std::int64_t>(target->pages.size());
        const auto page_index = [&](std::int64_t given) -> std::optional<std::size_t> {
            if (given < 1 || given > page_count) return std::nullopt;
            return static_cast<std::size_t>(given - 1);
        };
        const auto out_of_range = [&](std::int64_t given) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "'" + *named + "' yerleşiminde " +
                                             std::to_string(page_count) + " sayfa var; " +
                                             std::to_string(given) + ". sayfa yok."));
        };

        if (op == "sayfa") {
            // THE ITEMS ARE NOT RESCALED. A title block placed 20 mm from the top
            // of an A4 is 20 mm from the top of an A3 too; stretching the
            // composition with the paper would move every carefully placed box and
            // is not what changing paper means.
            //
            // WITHOUT `sayfa=` EVERY PAGE CHANGES, which is what "change the
            // paper" has always meant here; with it, one page does — and that is
            // how a layout comes to hold an A4 and an A3 at once.
            if (page_arg.empty()) {
                for (core::LayoutPage& page : target->pages) {
                    page.w = um(width_mm);
                    page.h = um(height_mm);
                }
                target->paper     = paper;
                target->landscape = landscape;
            } else {
                const auto at = page_index(page_arg.as_int());
                if (!at) {
                    out_of_range(page_arg.as_int());
                    co_return;
                }
                ctx.record("sayfa", page_arg);
                target->pages[*at].w = um(width_mm);
                target->pages[*at].h = um(height_mm);
                // The layout's own `paper`/`landscape` describe the sheet as a
                // whole and stop being true the moment two pages differ, so they
                // are left alone rather than made to name one page's answer.
            }
            target->margin = um(edge);

            // AND THE EXPORT RESOLUTION, which until now could only be chosen
            // when the layout was CREATED. The model carries it, `YAZDIR` reads
            // it, and no client of any kind — command line, script, designer or
            // agent — could change it afterwards; a sheet made at the default
            // 300 dpi was stuck there for good. Left out, it stays as it was.
            if (const Value dpi_arg = ctx.argument("dpi"); !dpi_arg.empty()) {
                target->dpi = static_cast<std::int32_t>(dpi_arg.as_int());
                ctx.record("dpi", dpi_arg);
            }
        } else if (op == "sayfaekle") {
            core::LayoutPage fresh{um(width_mm), um(height_mm)};
            std::size_t at = target->pages.size();
            if (!page_arg.empty()) {
                const std::int64_t given = page_arg.as_int();
                if (given < 1 || given > page_count + 1) {
                    out_of_range(given);
                    co_return;
                }
                at = static_cast<std::size_t>(given - 1);
                ctx.record("sayfa", page_arg);
            }
            target->pages.insert(target->pages.begin() + static_cast<std::ptrdiff_t>(at), fresh);
            // EVERY ITEM AFTER THE INSERTION POINT MOVES UP ONE. `item_pages`
            // holds indices, and an index that is not repaired now is an item
            // that silently changed page.
            for (std::int32_t& page : target->item_pages)
                if (page >= static_cast<std::int32_t>(at)) ++page;
        } else if (op == "sayfasil") {
            if (page_count <= 1) {
                ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                             "Son sayfa silinemez; bir yerleşimin en az bir "
                                             "sayfası olur."));
                co_return;
            }
            const auto at = page_index(page_arg.empty() ? page_count : page_arg.as_int());
            if (!at) {
                out_of_range(page_arg.as_int());
                co_return;
            }
            ctx.record("sayfa", Value::integer(static_cast<std::int64_t>(*at) + 1));

            // THE ITEMS ON IT GO WITH IT. Leaving them behind would leave boxes
            // pointing at a page that is not there, and there is no honest page
            // to move them to — the user asked for this page to stop existing.
            std::size_t removed = 0;
            for (std::size_t i = target->items.size(); i-- > 0;) {
                if (target->item_pages[i] != static_cast<std::int32_t>(*at)) continue;
                target->items.erase(target->items.begin() + static_cast<std::ptrdiff_t>(i));
                target->item_pages.erase(target->item_pages.begin() +
                                         static_cast<std::ptrdiff_t>(i));
                ++removed;
            }
            target->pages.erase(target->pages.begin() + static_cast<std::ptrdiff_t>(*at));
            for (std::int32_t& page : target->item_pages)
                if (page > static_cast<std::int32_t>(*at)) --page;
            if (removed != 0)
                ctx.echo("Sayfayla birlikte " + std::to_string(removed) + " öğe silindi.");
        } else if (op == "sayfacogalt") {
            const auto at = page_index(page_arg.empty() ? 1 : page_arg.as_int());
            if (!at) {
                out_of_range(page_arg.as_int());
                co_return;
            }
            ctx.record("sayfa", Value::integer(static_cast<std::int64_t>(*at) + 1));

            const core::LayoutPage copy = target->pages[*at];
            target->pages.insert(target->pages.begin() + static_cast<std::ptrdiff_t>(*at) + 1,
                                 copy);
            for (std::int32_t& page : target->item_pages)
                if (page > static_cast<std::int32_t>(*at)) ++page;

            // THE ITEMS ARE COPIED TOO, with fresh names: duplicating a page that
            // came back empty is not duplicating a page.
            const std::size_t had = target->items.size();
            for (std::size_t i = 0; i < had; ++i) {
                if (target->item_pages[i] != static_cast<std::int32_t>(*at)) continue;
                core::LayoutItem twin = target->items[i];
                twin.id               = free_id(*target, twin.id);
                target->items.push_back(std::move(twin));
                target->item_pages.push_back(static_cast<std::int32_t>(*at) + 1);
            }
        } else { // sayfatasi
            const Value to_arg = ctx.argument("yeni_sira");
            if (page_arg.empty() || to_arg.empty()) {
                ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                             "Sayfa taşımak için sayfa=<n> ve yeni_sira=<m> "
                                             "gerekir."));
                co_return;
            }
            const auto from = page_index(page_arg.as_int());
            const auto to   = page_index(to_arg.as_int());
            if (!from) {
                out_of_range(page_arg.as_int());
                co_return;
            }
            if (!to) {
                out_of_range(to_arg.as_int());
                co_return;
            }
            ctx.record("sayfa", page_arg);
            ctx.record("yeni_sira", to_arg);
            if (*from != *to) {
                const core::LayoutPage moved = target->pages[*from];
                target->pages.erase(target->pages.begin() + static_cast<std::ptrdiff_t>(*from));
                target->pages.insert(target->pages.begin() + static_cast<std::ptrdiff_t>(*to),
                                     moved);
                // AND EVERY ITEM FOLLOWS ITS PAGE. Reordering pages without
                // carrying the items is reordering blank paper.
                for (std::int32_t& page : target->item_pages) {
                    const auto was = static_cast<std::size_t>(page);
                    if (was == *from)
                        page = static_cast<std::int32_t>(*to);
                    else if (*from < *to && was > *from && was <= *to)
                        --page;
                    else if (*to < *from && was >= *to && was < *from)
                        ++page;
                }
            }
        }

        if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ctx.echo("Çıktı yerleşimi: " + describe(*bus.document().layouts().find(*named)));
        co_return;
    }

    // Unreachable: `canonical_verb` above accepted one of the five and each has
    // its own arm. Kept so a verb added to the table and forgotten here fails
    // loudly rather than silently doing nothing.
    ctx.session().fail(
        core::err(core::ErrorCode::Internal, "'" + op + "' işlemi tanımlı ama uygulanmamış."));
}

// =========================================================== ÇIKTIÖĞE ========

Task<void> run_item(Context& ctx)
{
    Bus& bus                      = ctx.session().bus();
    const core::LayoutStore& have = bus.document().layouts();

    static constexpr const char* kVerbs[] = {"listele", "ekle", "sil", "tasi", "ayarla", "ad"};
    auto verb = co_await ctx.text("islem", "İşlem: listele / ekle / sil / tasi / ayarla / ad");
    if (!verb) co_return;
    const char* resolved = canonical_verb(*verb, kVerbs);
    if (resolved == nullptr) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Tanınmayan işlem: '" + *verb +
                                         "'. İşlemler: listele / ekle / sil / tasi / ayarla / ad"));
        co_return;
    }
    const std::string op = resolved;
    ctx.record("islem", Value::text(op));

    std::string trouble;
    const Layout* found = resolve(have, ctx.argument("yerlesim").as_text(), trouble);
    if (found == nullptr) {
        ctx.session().fail(core::err(core::ErrorCode::NotFound, trouble));
        co_return;
    }
    const std::string sheet = found->name;
    ctx.record("yerlesim", Value::text(sheet));

    if (op == "listele") {
        if (found->items.empty()) {
            ctx.echo("'" + sheet + "' yerleşiminde öğe yok.");
            co_return;
        }
        std::string said =
            "'" + sheet + "' yerleşiminde " + std::to_string(found->items.size()) + " öğe:";
        for (const LayoutItem& item : found->items)
            said += "\n  " + item.id + " — " + core::layout_item_kind_label(item.kind) + ", " +
                    std::to_string(mm(item.frame.x)) + "," + std::to_string(mm(item.frame.y)) +
                    " " + std::to_string(mm(item.frame.w)) + "×" +
                    std::to_string(mm(item.frame.h)) + " mm";
        ctx.echo(said);
        co_return;
    }

    std::vector<Layout> next = have.all();
    Layout* target           = nullptr;
    for (Layout& l : next)
        if (core::turkish_key_equals(l.name, sheet)) target = &l;
    if (target == nullptr) {
        ctx.session().fail(
            core::err(core::ErrorCode::Internal, "Çıktı yerleşimi kayboldu: '" + sheet + "'."));
        co_return;
    }

    if (op == "ekle") {
        auto kind_text = co_await ctx.text("tur", "Öğe türü: harita / metin / olcek / kuzey / "
                                                  "lejant / resim / sekil / tablo / grafik");
        if (!kind_text) co_return;
        const std::optional<LayoutItemKind> kind = core::layout_item_kind_from_id(*kind_text);
        if (!kind) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Tanınmayan öğe türü: '" + *kind_text + "'."));
            co_return;
        }
        ctx.record("tur", Value::text(core::layout_item_kind_id(*kind)));

        // THE SAME DEFAULTS THE SEEDED SHEET GETS (`core::default_item`). Two
        // answers to "what does a fresh table look like" is how one of them ends
        // up transparent over a map.
        LayoutItem item    = core::default_item(*kind);
        const Value id_arg = ctx.argument("ad");
        item.id =
            id_arg.empty() ? free_id(*target, core::layout_item_kind_id(*kind)) : id_arg.as_text();
        if (target->find(item.id) != nullptr) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "'" + sheet + "' yerleşiminde '" + item.id +
                                             "' adlı bir öğe zaten var."));
            co_return;
        }
        ctx.record("ad", Value::text(item.id));

        // A DEFAULT BOX INSIDE THE MARGIN, so an item added without measurements
        // lands somewhere visible rather than at the corner under the title.
        const core::LayoutPage& page = target->pages.front();
        const Um edge                = target->margin;
        item.frame = core::PaperRect{edge, edge, (page.w - 2 * edge) / 3, (page.h - 2 * edge) / 6};

        target->items.push_back(std::move(item));
        target->item_pages.push_back(0);
    } else {
        auto id = co_await ctx.text("ad", "Öğe adı");
        if (!id || id->empty()) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Öğe adı gerekir: ad=<ad>"));
            co_return;
        }
        ctx.record("ad", Value::text(*id));

        if (op == "sil") {
            const auto at = std::find_if(
                target->items.begin(), target->items.end(),
                [&](const LayoutItem& one) { return core::turkish_key_equals(one.id, *id); });
            if (at == target->items.end()) {
                ctx.session().fail(
                    core::err(core::ErrorCode::NotFound,
                              "'" + sheet + "' yerleşiminde öğe yok: '" + *id + "'."));
                co_return;
            }
            const std::size_t index = static_cast<std::size_t>(at - target->items.begin());
            target->items.erase(at);
            if (index < target->item_pages.size())
                target->item_pages.erase(target->item_pages.begin() +
                                         static_cast<std::ptrdiff_t>(index));
        } else if (op == "ad") {
            // THE VERB THE KEYS EXIST FOR. Renaming an item used to be impossible,
            // which is why a name could stand in for identity; now that it is
            // possible, every reference to the item has to keep pointing at it —
            // and `Layout::rename_item` is where that is made true.
            const Value fresh = ctx.argument("yeni_ad");
            if (fresh.empty()) {
                ctx.session().fail(
                    core::err(core::ErrorCode::InvalidArgument, "Yeni ad gerekir: yeni_ad=<ad>"));
                co_return;
            }
            if (!target->rename_item(*id, fresh.as_text())) {
                ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                             "'" + sheet + "' yerleşiminde '" + *id +
                                                 "' yeniden adlandırılamadı; "
                                                 "öğe yok ya da '" +
                                                 fresh.as_text() + "' adı kullanımda."));
                co_return;
            }
            ctx.record("yeni_ad", fresh);
        } else if (op == "tasi" || op == "ayarla") {
            LayoutItem* item = target->find(*id);
            if (item == nullptr) {
                ctx.session().fail(
                    core::err(core::ErrorCode::NotFound,
                              "'" + sheet + "' yerleşiminde öğe yok: '" + *id + "'."));
                co_return;
            }
            if (item->locked && op == "tasi") {
                ctx.session().fail(
                    core::err(core::ErrorCode::InvalidArgument, "'" + *id +
                                                                    "' kilitli; önce kilidi açın: "
                                                                    "ÇIKTIÖĞE islem=ayarla ad=" +
                                                                    *id + " kilit=hayır"));
                co_return;
            }

            const auto take_um = [&ctx](const char* name, Um& into) {
                const Value v = ctx.argument(name);
                if (v.empty()) return;
                into = um_of(v.as_number());
                ctx.record(name, v);
            };
            take_um("x", item->frame.x);
            take_um("y", item->frame.y);
            take_um("genislik", item->frame.w);
            take_um("yukseklik", item->frame.h);

            // MOVING BETWEEN PAGES IS A MOVE TOO. Without this a two-page layout
            // could be built but nothing could be carried from one sheet to the
            // other, and the only way to move a title block would be to delete it
            // and make another one — which is not the same title block.
            if (const Value v = ctx.argument("sayfa"); !v.empty()) {
                const std::int64_t wanted = v.as_int();
                if (wanted < 1 || wanted > static_cast<std::int64_t>(target->pages.size())) {
                    ctx.session().fail(core::err(
                        core::ErrorCode::InvalidArgument,
                        "'" + sheet + "' yerleşiminde " + std::to_string(target->pages.size()) +
                            " sayfa var; " + std::to_string(wanted) + ". sayfa yok."));
                    co_return;
                }
                for (std::size_t i = 0; i < target->items.size(); ++i)
                    if (&target->items[i] == item)
                        target->item_pages[i] = static_cast<std::int32_t>(wanted - 1);
                ctx.record("sayfa", v);
            }

            if (const Value v = ctx.argument("metin"); !v.empty()) {
                item->text = v.as_text();
                ctx.record("metin", v);
            }
            if (const Value v = ctx.argument("yazi"); !v.empty()) {
                item->text_height = um(v.as_int());
                ctx.record("yazi", v);
            }
            if (const Value v = ctx.argument("olcek"); !v.empty()) {
                item->scale = v.as_int();
                ctx.record("olcek", v);
            }

            // HOW MANY ROWS, AND WHICH COLUMNS. Both were on the model and
            // reachable from no client — and `docs/komutlar/layout_item.md` has
            // been promising `satir_siniri` the whole time, which makes it a
            // documented feature that did not exist.
            if (const Value v = ctx.argument("satir_siniri"); !v.empty()) {
                if (item->kind != LayoutItemKind::Table) {
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "'" + *id +
                                                     "' bir tablo değil; satir_siniri yalnız "
                                                     "tabloya verilir."));
                    co_return;
                }
                item->row_limit = static_cast<std::int32_t>(v.as_int());
                ctx.record("satir_siniri", v);
            }

            if (const Value v = ctx.argument("sutunlar"); !v.empty()) {
                // TWO KINDS TAKE COLUMNS, and they take them for different
                // reasons: a table PRINTS them, a chart COUNTS BY the first one
                // (TODOS L-09). The refusal names both rather than only the one
                // this branch was written for.
                if (item->kind != LayoutItemKind::Table && item->kind != LayoutItemKind::Chart) {
                    ctx.session().fail(
                        core::err(core::ErrorCode::InvalidArgument,
                                  "'" + *id +
                                      "' bir tablo ya da grafik değil; sutunlar yalnız onlara "
                                      "verilir."));
                    co_return;
                }
                std::vector<std::string> wanted;
                for (const std::string& one : v.as_texts()) {
                    if (core::turkish_key_equals(one, "hepsi")) {
                        wanted.clear();
                        break;
                    }
                    if (bus.document().attributes().find(one) == core::kNoAttr) {
                        ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                                     "Öznitelik sütunu yok: '" + one + "'."));
                        co_return;
                    }
                    wanted.push_back(one);
                }
                item->columns = std::move(wanted);
                ctx.record("sutunlar", v);
            }

            // WHICH LAYERS THIS FRAME DRAWS. The field has been on the model all
            // along with no way to set it, which made "this map draws these
            // layers" a promise the product could not keep from any client. It
            // needed `Value::Kind::TextList` first: a comma would not do, because
            // a layer name is not validated and may contain one.
            //
            // `hepsi` empties the list rather than naming a layer called that:
            // without a word for it there would be no way back from a filter once
            // set, and "delete the item and make another one" is not an answer.
            if (const Value v = ctx.argument("katmanlar"); !v.empty()) {
                if (item->kind != LayoutItemKind::Map) {
                    ctx.session().fail(
                        core::err(core::ErrorCode::InvalidArgument,
                                  "'" + *id +
                                      "' bir harita çerçevesi değil; katmanlar yalnız haritaya "
                                      "verilir."));
                    co_return;
                }
                std::vector<std::string> wanted;
                for (const std::string& one : v.as_texts()) {
                    if (core::turkish_key_equals(one, "hepsi")) {
                        wanted.clear();
                        break;
                    }
                    const auto& drawn = bus.document().layers();
                    const bool known =
                        std::any_of(drawn.begin(), drawn.end(), [&one](const core::Layer& layer) {
                            return core::turkish_key_equals(layer.name, one);
                        });
                    if (!known) {
                        ctx.session().fail(
                            core::err(core::ErrorCode::NotFound, "Katman yok: '" + one + "'."));
                        co_return;
                    }
                    wanted.push_back(one);
                }
                item->layers = std::move(wanted);
                ctx.record("katmanlar", v);
            }

            // WHICH MAP THIS ITEM BELONGS TO. A scale bar states a map's scale
            // and a `<olcek>` placeholder its denominator; on a sheet with two
            // map frames at two scales, "the map" is not a question the program
            // may answer by taking the first one it finds.
            //
            // `ilk` clears the link rather than naming an item called that:
            // without a word for it there would be no way back once set.
            if (const Value v = ctx.argument("harita"); !v.empty()) {
                const std::string wanted = v.as_text();
                if (core::turkish_key_equals(wanted, "ilk")) {
                    item->linked_map.clear();
                } else {
                    const LayoutItem* named = target->find(wanted);
                    if (named == nullptr || named->kind != LayoutItemKind::Map) {
                        ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                                     "'" + sheet + "' yerleşiminde '" + wanted +
                                                         "' adlı bir harita çerçevesi yok."));
                        co_return;
                    }
                    if (item->kind == LayoutItemKind::Map) {
                        ctx.session().fail(
                            core::err(core::ErrorCode::InvalidArgument,
                                      "Bir harita çerçevesi başka bir haritaya bağlanmaz."));
                        co_return;
                    }
                    item->linked_map = wanted;
                }
                ctx.record("harita", v);
            }

            // WHERE THE MAP FRAME LOOKS. Two ground corners, the same shape
            // `YAZDIR pencere=` takes — which is what lets the canvas's print
            // frame aim a layout: the user drags a rectangle and the window types
            // this line (Article 1.2, and the reason the frame is not a private
            // road into the designer).
            if (const Value v = ctx.argument("pencere"); !v.empty()) {
                const std::vector<core::Point2> corners = v.as_points();
                if (corners.size() != 2) {
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "pencere iki köşe ister: pencere=x1,y1 x2,y2"));
                    co_return;
                }
                const core::Box2 box{
                    std::min(corners[0].x, corners[1].x), std::min(corners[0].y, corners[1].y),
                    std::max(corners[0].x, corners[1].x), std::max(corners[0].y, corners[1].y)};
                if (box.empty()) {
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "Pencerenin iki köşesi bir dikdörtgen "
                                                 "kurmuyor."));
                    co_return;
                }
                if (item->kind != core::LayoutItemKind::Map) {
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "'" + *id +
                                                     "' bir harita çerçevesi değil; "
                                                     "pencere yalnız haritaya verilir."));
                    co_return;
                }
                item->extent = box;
                ctx.record("pencere", v);
            }
            if (const Value v = ctx.argument("izgara"); !v.empty()) {
                static constexpr const char* kGrids[] = {"yok", "arti", "cizgi", "centik"};
                const char* word                      = canonical_verb(v.as_text(), kGrids);
                if (word == nullptr) {
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "Izgara: yok / arti / cizgi / centik"));
                    co_return;
                }
                const std::string_view grid_word{word};
                if (grid_word == "yok")
                    item->grid = core::GridStyle::None;
                else if (grid_word == "arti")
                    item->grid = core::GridStyle::Cross;
                else if (grid_word == "cizgi")
                    item->grid = core::GridStyle::Line;
                else
                    item->grid = core::GridStyle::Tick;
                ctx.record("izgara", Value::text(word));
            }
            if (const Value v = ctx.argument("izgara_aralik"); !v.empty()) {
                item->grid_interval = v.as_int();
                ctx.record("izgara_aralik", v);
            }
            if (const Value v = ctx.argument("kilit"); !v.empty()) {
                item->locked = v.as_bool();
                ctx.record("kilit", v);
            }
            if (const Value v = ctx.argument("cerceve"); !v.empty()) {
                item->frame_visible = v.as_bool();
                ctx.record("cerceve", v);
            }
            if (const Value v = ctx.argument("sira"); !v.empty()) {
                item->z = static_cast<std::int32_t>(v.as_int());
                ctx.record("sira", v);
            }
        } else {
            ctx.session().fail(core::err(core::ErrorCode::Internal,
                                         "'" + op + "' işlemi tanımlı ama uygulanmamış."));
            co_return;
        }
    }

    if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
        ctx.session().fail(st.error());
        co_return;
    }
    const Layout* after = bus.document().layouts().find(sheet);
    ctx.echo("'" + sheet + "' yerleşimi: " +
             std::to_string(after != nullptr ? after->items.size() : 0) + " öğe.");
}

// ========================================================= ÇIKTIŞABLON ======

Task<void> run_template(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    if (!bus.on_layout_template_request) {
        ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                     "Bu yapıda çıktı yerleşimi şablonu deposu yok."));
        co_return;
    }

    static constexpr const char* kVerbs[] = {"listele", "kaydet", "uygula", "sil"};
    auto verb = co_await ctx.text("islem", "İşlem: listele / kaydet / uygula / sil");
    if (!verb) co_return;
    const char* resolved = canonical_verb(*verb, kVerbs);
    if (resolved == nullptr) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Tanınmayan işlem: '" + *verb +
                                         "'. İşlemler: listele / kaydet / uygula / sil"));
        co_return;
    }
    const std::string op = resolved;
    ctx.record("islem", Value::text(op));

    LayoutTemplateRequest request;
    if (op == "listele") {
        request.verb = LayoutTemplateRequest::Verb::List;
        auto said    = co_await bus.on_layout_template_request(request);
        if (!said) {
            ctx.session().fail(said.error());
            co_return;
        }
        ctx.echo(said.value());
        co_return;
    }

    auto named = co_await ctx.text("ad", "Şablon adı");
    if (!named || named->empty()) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument, "Şablon adı gerekir: ad=<ad>"));
        co_return;
    }
    ctx.record("ad", Value::text(*named));
    request.name = *named;

    if (op == "sil") {
        request.verb = LayoutTemplateRequest::Verb::Remove;
        auto said    = co_await bus.on_layout_template_request(request);
        if (!said) {
            ctx.session().fail(said.error());
            co_return;
        }
        ctx.echo(said.value());
        co_return;
    }

    if (op == "kaydet") {
        // THE COMMAND SERIALISES, THE APPLICATION WRITES BYTES. `/src/core` owns
        // the shape and its JSON, so the store never has to know what a layout
        // is — and a test can prove the round trip with no file at all.
        std::string trouble;
        const Layout* source =
            resolve(bus.document().layouts(), ctx.argument("yerlesim").as_text(), trouble);
        if (source == nullptr) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound, trouble));
            co_return;
        }
        ctx.record("yerlesim", Value::text(source->name));
        request.verb   = LayoutTemplateRequest::Verb::Save;
        request.layout = source->name;
        request.json   = core::layout_to_json(*source, *named);

        auto said = co_await bus.on_layout_template_request(request);
        if (!said) {
            ctx.session().fail(said.error());
            co_return;
        }
        ctx.echo(said.value());
        co_return;
    }

    // ---- uygula: the template becomes a sheet of THIS drawing ---------------
    request.verb   = LayoutTemplateRequest::Verb::Apply;
    const Value as = ctx.argument("yerlesim");
    request.layout = as.empty() ? *named : as.as_text();
    if (!as.empty()) ctx.record("yerlesim", as);

    auto json = co_await bus.on_layout_template_request(request);
    if (!json) {
        ctx.session().fail(json.error());
        co_return;
    }

    auto built = core::layout_from_json(json.value(), request.layout);
    if (!built) {
        ctx.session().fail(built.error());
        co_return;
    }

    // THROUGH THE ORDINARY MUTATOR, so applying a template is one undoable edit
    // like every other layout change rather than something the application did
    // behind the command's back (Article 1.1).
    std::vector<Layout> next = bus.document().layouts().all();
    const auto at            = std::find_if(next.begin(), next.end(), [&](const Layout& l) {
        return core::turkish_key_equals(l.name, request.layout);
    });
    if (at != next.end())
        *at = std::move(built.value());
    else
        next.push_back(std::move(built.value()));

    if (auto st = ctx.transaction().set_layouts(std::move(next)); !st) {
        ctx.session().fail(st.error());
        co_return;
    }
    const Layout* made = bus.document().layouts().find(request.layout);
    ctx.echo("Şablondan çıktı yerleşimi kuruldu: " +
             (made != nullptr ? describe(*made) : request.layout));
}

} // namespace

KENTOS_COMMAND(layout)
{
    return CommandSpec{
        .id       = "core.layout",
        .names    = {"ÇIKTIYERLEŞİMİ", "CIKTIYERLESIMI", "LAYOUT", "ÇYR", "CYR"},
        .title    = "Çıktı Yerleşimi",
        .category = Category::File,
        .params =
            {
                Param::choice("islem", Arity::exactly(1),
                              {"listele", "ekle", "sil", "ad", "sayfa", "sayfaekle", "sayfasil",
                               "sayfacogalt", "sayfatasi", "denetle", "atlas", "rapor"},
                              "Ne yapılacağı"),
                Param::text("ad", Arity::optional(), "Yerleşimin adı; listele dışında gerekir"),
                Param::text("yeni_ad", Arity::optional(), "islem=ad için yeni yerleşim adı"),
                Param::text("kagit", Arity::optional(),
                            "A5, A4, A3, A2, A1, A0 ya da ozel (varsayılan A4)"),
                Param::integer_range("genislik", Arity::optional(), 1, 10000,
                                     "ozel kâğıt için sayfa genişliği")
                    .measured_in("kâğıt mm"),
                Param::integer_range("yukseklik", Arity::optional(), 1, 10000,
                                     "ozel kâğıt için sayfa yüksekliği")
                    .measured_in("kâğıt mm"),
                Param::choice("yon", Arity::optional(), {"dikey", "yatay"},
                              "Sayfa yönü (varsayılan dikey)"),
                Param::integer_range("kenar", Arity::optional(), 0, 200,
                                     "Kenar boşluğu (varsayılan 10)")
                    .measured_in("kâğıt mm"),
                Param::integer_range("dpi", Arity::optional(), 72, 4800,
                                     "Çıktı çözünürlüğü (varsayılan 300)"),
                Param::integer_range("sayfa", Arity::optional(), 1, 10000,
                                     "Hangi sayfa (1'den başlar). sayfa işleminde verilmezse "
                                     "bütün sayfalar değişir"),
                Param::integer_range("yeni_sira", Arity::optional(), 1, 10000,
                                     "sayfatasi için sayfanın gideceği sıra"),
                Param::text("katman", Arity::optional(),
                            "atlas: hangi katmanın nesneleri için bir sayfa basılacak; "
                            "'yok' atlası kapatır"),
                Param::text("sirala", Arity::optional(),
                            "atlas: sayfaların sıralanacağı ve adlandırılacağı öznitelik "
                            "sütunu; verilmezse nesne anahtarı"),
                Param::text("grup", Arity::optional(),
                            "rapor: bölümlerin oluşturulacağı öznitelik sütunu (ada_no gibi); "
                            "verilmezse tek bölüm"),
                Param::integer_range("kenar_payi", Arity::optional(), 0, 200,
                                     "atlas: nesnenin çevresinde bırakılacak pay, yüzde "
                                     "(varsayılan 10)"),
                Param::boolean("tek_dosya", Arity::optional(),
                               "atlas: tek çok sayfalı belge mi, nesne başına bir dosya mı "
                               "(varsayılan evet)"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve "
                   "kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir.",
        .run = &run_layout,
        // LISTING IS A READ. Collapsing the five words into the command's worst
        // case would make `islem=listele` ask a person for approval, which a read
        // must never do (.claude/ai.md R3).
        .effect      = Effect::Query | Effect::DocumentEdit,
        .effect_verb = "islem",
        .verb_effects =
            {
                {"listele", Effect::Query},
                {"ekle", Effect::DocumentEdit},
                {"sil", Effect::DocumentEdit},
                {"ad", Effect::DocumentEdit},
                {"sayfa", Effect::DocumentEdit},
                {"sayfaekle", Effect::DocumentEdit},
                {"sayfasil", Effect::DocumentEdit},
                {"sayfacogalt", Effect::DocumentEdit},
                {"sayfatasi", Effect::DocumentEdit},
                {"denetle", Effect::Query},
                {"atlas", Effect::DocumentEdit},
                // A REPORT READS AND REPORTS; it prints nothing and changes nothing.
                {"rapor", Effect::Query},
            },
    };
}

KENTOS_COMMAND(layout_item)
{
    return CommandSpec{
        .id       = "core.layout_item",
        .names    = {"ÇIKTIÖĞE", "CIKTIOGE", "LAYOUTITEM", "ÇÖĞ", "COG"},
        .title    = "Çıktı Öğesi",
        .category = Category::File,
        .params =
            {
                Param::choice("islem", Arity::exactly(1),
                              {"listele", "ekle", "sil", "tasi", "ayarla", "ad"}, "Ne yapılacağı"),
                Param::text("yerlesim", Arity::optional(),
                            "Hangi çıktı yerleşimi; çizimde tek yerleşim varsa gerekmez")
                    .renamed_from("pafta"),
                Param::text("ad", Arity::optional(),
                            "Öğe adı; ekle dışında gerekir, ekle'de verilmezse türetilir"),
                Param::choice("tur", Arity::optional(),
                              {"harita", "metin", "olcek", "kuzey", "lejant", "resim", "sekil",
                               "tablo", "grafik"},
                              "islem=ekle için öğe türü"),
                Param::number("x", Arity::optional(), "Sol kenardan uzaklık")
                    .measured_in("kâğıt mm"),
                Param::number("y", Arity::optional(), "ÜST kenardan uzaklık")
                    .measured_in("kâğıt mm"),
                Param::number("genislik", Arity::optional(), "Genişlik").measured_in("kâğıt mm"),
                Param::number("yukseklik", Arity::optional(), "Yükseklik").measured_in("kâğıt mm"),
                Param::text("metin", Arity::optional(),
                            "Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer "
                            "tutucuları çizim anında çözülür"),
                Param::number("yazi", Arity::optional(), "Yazı yüksekliği").measured_in("kâğıt mm"),
                Param::integer_range("olcek", Arity::optional(), 0, 100000000,
                                     "Harita öğesinin ölçeği 1:N; 0 kapsama uyar"),
                Param::points("pencere", Arity{0, 2},
                              "Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki "
                              "kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve "
                              "seçmek bu satırı yazar")
                    .measured_in("ZEMİN koordinatı — kâğıt değil"),
                Param::choice("izgara", Arity::optional(), {"yok", "arti", "cizgi", "centik"},
                              "Harita öğesinin koordinat ızgarası"),
                Param::integer_range("izgara_aralik", Arity::optional(), 0, 1000000000,
                                     "Izgara aralığı, zemin milimetresi; 0 ölçeğe göre seçilir"),
                Param::boolean("kilit", Arity::optional(), "Öğeyi taşımaya kapatır"),
                Param::boolean("cerceve", Arity::optional(), "Öğenin çevresine çerçeve çizer"),
                Param::integer_range("sayfa", Arity::optional(), 1, 10000,
                                     "Öğenin duracağı sayfa (1'den başlar); tasi ile verilir"),
                Param::text("yeni_ad", Arity::optional(), "islem=ad için öğenin yeni adı"),
                Param::integer_range("satir_siniri", Arity::optional(), 0, 100000,
                                     "Tablo öğesinin yazacağı en çok satır; 0 = kutuya kaç satır "
                                     "sığıyorsa o kadar"),
                Param::text("sutunlar", Arity{0, 64},
                            "Tablo öğesinin yazacağı öznitelik sütunları, sırasıyla; anahtar "
                            "birden çok kez yazılır. Verilmezse katmanın bütün sütunları, "
                            "'hepsi' listeyi boşaltır"),
                Param::text(
                    "katmanlar", Arity{0, 64},
                    "Harita çerçevesinin çizeceği katmanlar; anahtar birden çok kez "
                    "yazılır. Verilmezse görünür bütün katmanlar, 'hepsi' listeyi boşaltır"),
                Param::text("harita", Arity::optional(),
                            "Bu öğenin bağlı olduğu harita çerçevesinin adı. Verilmezse ilk "
                            "harita. 'ilk' bağı kaldırır"),
                Param::integer_range("sira", Arity::optional(), -1000, 1000,
                                     "Çizim sırası; büyük olan üstte"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary =
            "Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek "
            "çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar "
            "ve siler.",
        .run         = &run_item,
        .effect      = Effect::Query | Effect::DocumentEdit,
        .effect_verb = "islem",
        .verb_effects =
            {
                {"listele", Effect::Query},
                {"ekle", Effect::DocumentEdit},
                {"sil", Effect::DocumentEdit},
                {"tasi", Effect::DocumentEdit},
                {"ayarla", Effect::DocumentEdit},
                {"ad", Effect::DocumentEdit},
            },
    };
}

KENTOS_COMMAND(layout_template)
{
    return CommandSpec{
        .id       = "core.layout_template",
        .names    = {"ÇIKTIŞABLON", "CIKTISABLON", "LAYOUTTEMPLATE", "ÇŞB", "CSB"},
        .title    = "Çıktı Şablonu",
        .category = Category::File,
        .params =
            {
                Param::choice("islem", Arity::exactly(1), {"listele", "kaydet", "uygula", "sil"},
                              "Ne yapılacağı"),
                Param::text("ad", Arity::optional(), "Şablonun adı; listele dışında gerekir"),
                Param::text("yerlesim", Arity::optional(),
                            "kaydet: hangi yerleşim saklanacak (tek yerleşim varsa gerekmez). "
                            "uygula: kurulacak yerleşimin adı (verilmezse şablonun adı)")
                    .renamed_from("pafta"),
            },
        .undo = UndoPolicy::SingleTransaction,
        // NOT `AiAccessible`, for the reason `ÇIKTIYERLEŞİMİ` gives: a sheet carries a
        // ground extent and the handle machinery has no answer yet for a command
        // that takes both paper and ground (CLAUDE.md 5.8).
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary =
            "Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, "
            "kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni "
            "taşır, zemin koordinatlarını taşımaz.",
        .run = &run_template,
        // A TEMPLATE LIVES OUTSIDE THE DRAWING, in the user's profile — so
        // `kaydet` and `sil` write a FILE and leave the document alone, while
        // `uygula` reads a file and edits the document. One command, three
        // different things to the world.
        .effect      = Effect::Query | Effect::FileRead | Effect::FileWrite | Effect::DocumentEdit,
        .effect_verb = "islem",
        .verb_effects =
            {
                {"listele", Effect::Query | Effect::FileRead},
                {"kaydet", Effect::Query | Effect::FileWrite},
                {"uygula", Effect::FileRead | Effect::DocumentEdit},
                {"sil", Effect::FileWrite},
            },
    };
}

} // namespace kentos::command
