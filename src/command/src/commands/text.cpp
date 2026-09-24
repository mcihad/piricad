// SPDX-License-Identifier: GPL-3.0-or-later
// core.text — METİN. A caption on the drawing.
//
// A pafta is not only geometry. It carries ada and parsel numbers, plan notes,
// legend captions and a north arrow, and each one has an exact position, an exact
// height and an exact rotation — a drafted entity, not a label the renderer
// invents. That is the CAD half of this program, and it was missing entirely.
//
// The geometry of a text entity is its BASELINE: two points, the anchor and the
// end of the line. That is a real open ring, so culling, snapping and hit testing
// all work on it with no special case, and the rotation is the direction of an
// integer segment rather than a stored angle — exact, and identical on every
// platform because no trigonometry is involved (§7.3). DXF TEXT stores an
// insertion point and an alignment point for the same reasons.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/dimension.hpp"

#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_store.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The `hizalama` words, one per anchor and in its order: the anchor's own
/// machine name (text_store.hpp), so the word a file, a message and a command
/// line use for one alignment is one word.
std::vector<std::string> anchor_words()
{
    std::vector<std::string> out;
    out.reserve(core::kTextAnchorCount);
    for (std::uint8_t a = 0; a < core::kTextAnchorCount; ++a)
        out.emplace_back(core::text_anchor_name(static_cast<core::TextAnchor>(a)));
    return out;
}

core::TextAnchor anchor_from(const std::string& word)
{
    for (std::uint8_t a = 0; a < core::kTextAnchorCount; ++a)
        if (core::turkish_key_equals(word,
                                     core::text_anchor_name(static_cast<core::TextAnchor>(a))))
            return static_cast<core::TextAnchor>(a);
    return core::TextAnchor::BaselineLeft; // unreachable: the bus holds the word list
}

/// `satir_araligi` and `genislik` onto `lines`; the width a wrapping text
/// breaks to in `width` (zero: `genislik=0`, the text stops wrapping). Only
/// what is named changes. False after refusing.
bool read_lines(Context& ctx, core::TextLines& lines, std::optional<core::Mm>& width)
{
    if (const Value v = ctx.argument("satir_araligi"); !v.empty()) {
        const double factor = v.as_number();
        if (!std::isfinite(factor) || factor < 0.25 || factor > 4.0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Satır aralığı 0,25 ile 4 arasında olmalı; 1 tek aralıktır.");
            return false;
        }
        lines.spacing = static_cast<std::uint16_t>(std::lround(factor * 1000.0));
        ctx.record("satir_araligi", v);
    }
    if (const Value v = ctx.argument("genislik"); !v.empty()) {
        const double metres = v.as_number();
        if (!(metres >= 0.0)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Genişlik eksi olamaz; satırları kırmamak için genislik=0 verin.");
            return false;
        }
        width      = core::mm_from_metres(metres);
        lines.wrap = *width > 0;
        ctx.record("genislik", v);
    }
    return true;
}

/// `\n` — the two characters — as the line break it means, wherever the words
/// came from. The command line's lexer turns it into one already; an answer
/// typed at the prompt, a panel cell and a script's string reach the command
/// as they were typed, and "ADA 101\nPARSEL 4" is two lines to all of them.
std::string with_breaks(std::string words)
{
    std::size_t at = 0;
    while ((at = words.find("\\n", at)) != std::string::npos)
        words.replace(at, 2, "\n");
    return words;
}

/// The baseline from `from` along `toward`'s direction for `length`; along the
/// page's right when `toward` is `from` itself.
std::array<core::Point2, 2> baseline_of(core::Point2 from, core::Point2 toward, core::Mm length)
{
    const auto dx    = static_cast<double>(toward.x - from.x);
    const auto dy    = static_cast<double>(toward.y - from.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (!(len > 0.0)) return {from, core::Point2{from.x + length, from.y}};
    const double k = static_cast<double>(length) / len;
    return {from, core::Point2{from.x + core::mm_round(dx * k), from.y + core::mm_round(dy * k)}};
}

Task<void> run(Context& ctx)
{
    auto anchor_point = co_await ctx.point("noktalar", "Yazının başlangıç noktası");
    if (!anchor_point) co_return; // ESC before anything was drawn

    auto content = co_await ctx.text("yazi", "Yazılacak metin");
    if (!content || content->empty()) co_return;
    *content = with_breaks(std::move(*content));

    // Height comes from the project setting unless the caller overrides it. The
    // setting is ground millimetres, and so is this: a caption drawn at 2.5 m on
    // a 1/1000 sheet is 2.5 mm on paper, which is what MPYY prescribes and what
    // the plot has to honour when the scale changes.
    core::Mm height =
        ctx.session().bus().project_settings().get("core.cizim.metin_yuksekligi").as_length();
    if (const Value given = ctx.argument("yukseklik"); !given.empty() && given.as_int() > 0)
        height = static_cast<core::Mm>(given.as_int());

    const Value align = ctx.argument("hizalama");
    const core::TextAnchor anchor =
        align.empty() ? core::TextAnchor::BaselineLeft : anchor_from(align.as_text());

    core::TextLines lines;
    std::optional<core::Mm> width;
    if (!read_lines(ctx, lines, width)) co_return;

    // The baseline runs to the second point when one is given, and along the
    // page for the text's own width when it is not. Either way the entity has
    // two real vertices, so nothing downstream needs to know it is text to cull
    // it. A text that wraps runs for its width, in the given direction.
    //
    // The width, when estimated, decides the entity's BOUNDING BOX, not where a
    // glyph lands — the backend measures the real font — so the estimate costs
    // a slightly loose cull box and nothing that reaches paper. It counts
    // LETTERS: a count of bytes made every `ş` two letters wide.
    const Value second  = ctx.argument("bitis");
    core::Point2 toward = *anchor_point;
    if (!second.empty() && !second.as_points().empty()) toward = second.as_points().front();
    std::array<core::Point2, 2> baseline{*anchor_point, toward};
    if (lines.wrap)
        baseline = baseline_of(*anchor_point, toward, width.value_or(0));
    else if (toward == *anchor_point)
        baseline = baseline_of(*anchor_point, toward, core::text_width_estimate(*content, height));
    auto created = ctx.transaction().add_polyline(ctx.active_layer(), baseline);
    if (!created) {
        ctx.refuse(created.error());
        co_return;
    }

    if (auto st = ctx.transaction().set_text(created.value(), *content, height, anchor, lines);
        !st) {
        ctx.refuse(st.error());
        co_return; // the bus rolls the whole transaction back, baseline included
    }

    ctx.record("noktalar", Value::points({*anchor_point}));
    ctx.record("yazi", Value::text(*content));
    ctx.record("yukseklik", Value::integer(height));
    if (!second.empty()) ctx.record("bitis", second);
    if (!align.empty()) ctx.record("hizalama", align);

    ctx.echo("Metin yazıldı: \"" + *content + "\"  (yükseklik " + std::to_string(height) + " mm)");
}

// -------------------------------------------------------- YAZIDÜZENLE -------

Task<void> run_edit(Context& ctx)
{
    const core::Document& doc = ctx.document();

    // WHICH CAPTIONS. The named objects, or the selection when none are named —
    // the same order every command that acts on objects reads them in, so a
    // right-click on a caption and a typed line reach the same entities.
    std::vector<core::EntityId> targets;

    // Named, highlighted, or ASKED FOR (`want_objects`). The menu entry used to
    // answer "Düzenlenecek yazı yok" and stop, so a caption could be edited only
    // by somebody who had selected it first — or who knew its key.
    std::vector<std::int64_t> picked;
    if (!co_await want_objects(ctx, "nesneler", "Düzenlenecek yazıyı seçin, sonra Enter", picked, 0,
                               "YAZIDÜZENLE nesneler=1 yazi=\"101 ada 4 parsel\""))
        co_return;
    for (std::int64_t raw : picked) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        targets.push_back(slot);
    }

    // ONLY WHAT IS ASKED FOR CHANGES. An unnamed field keeps the value the
    // caption already has: rewriting a parsel number must not silently reset the
    // height a planner chose for it.
    Value content       = ctx.argument("yazi");
    const Value tall    = ctx.argument("yukseklik");
    const Value align   = ctx.argument("hizalama");
    const Value spacing = ctx.argument("satir_araligi");
    const Value wide    = ctx.argument("genislik");

    // NOTHING NAMED IS A QUESTION: the new words. This is what a hand pressing
    // the menu entry means, and the one change a script that named nothing could
    // have meant too — it is told the same thing it was told before, because it
    // cannot answer. The first caption's own words are offered, so fixing one
    // letter is not retyping the line.
    if (content.empty() && tall.empty() && align.empty() && spacing.empty() && wide.empty()) {
        std::vector<std::string> now;
        for (const core::EntityId e : targets)
            if (const std::uint32_t slot = doc.entities().slot[e]; doc.texts().has(slot)) {
                now.emplace_back(doc.texts().text(slot));
                break;
            }
        auto typed = co_await ctx.text("yazi", "Yeni metin", std::move(now));
        if (!typed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Değiştirilecek bir şey verilmedi: yazi=, yukseklik=, hizalama=, "
                       "satir_araligi= ya da genislik=.");
            co_return;
        }
        content = Value::text(*typed);
    }

    std::size_t written = 0;
    std::size_t skipped = 0;

    for (const core::EntityId e : targets) {
        const std::uint32_t slot = doc.entities().slot[e];

        // A caption is an entity that CARRIES text; anything else in the
        // selection is passed over rather than turned into one. `METİN` draws a
        // new caption, and this command is not a second way to do that.
        if (!doc.texts().has(slot)) {
            ++skipped;
            continue;
        }

        std::string words =
            content.empty() ? std::string(doc.texts().text(slot)) : with_breaks(content.as_text());
        if (words.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Boş bir yazı bir yazı değildir; silmek için SİL kullanın.");
            co_return;
        }

        core::Mm height = doc.texts().height(slot);
        if (!tall.empty()) {
            if (tall.as_int() <= 0) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Yazı yüksekliği sıfırdan büyük olmalı.");
                co_return;
            }
            height = static_cast<core::Mm>(tall.as_int());
        }

        // A DIMENSION'S CAPTION IS ITS FIGURE (TODOS C-10). Written into the
        // caption alone, a typed number would be a measured one to every reader
        // of the model and would be put back the next time the dimension
        // followed its corner. It goes where ÖLÇÜDÜZENLE puts it: the payload's
        // typed text, with `<>` standing for the measured figure.
        if (doc.entities().kind[e] == core::kDimensionKind) {
            auto stored = core::dimension_of(doc.geometry(), slot);
            if (!stored) {
                ctx.refuse(stored.error());
                co_return;
            }
            core::DimensionDef def = stored.value();
            if (!content.empty()) def.override_text = words == "<>" ? std::string() : words;
            auto rebuilt = core::dimension_rebuild(doc, e, {.def = &def, .text_height = height},
                                                   ctx.session().bus().drawing_unit());
            if (!rebuilt) {
                ctx.refuse(rebuilt.error());
                co_return;
            }
            const core::DimensionRebuild& r = rebuilt.value();
            const std::array<core::RingGeometry::RingInput, 2> rings{
                core::RingGeometry::RingInput{r.baseline, core::RingRole::Open, 0},
                core::RingGeometry::RingInput{r.defs, core::RingRole::Open, 0}};
            if (auto st = ctx.transaction().set_kind_geometry(e, rings, r.payload); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            if (auto st = ctx.transaction().set_text(e, r.text, r.text_height,
                                                     core::TextAnchor::MiddleCentre);
                !st) {
                ctx.refuse(st.error());
                co_return;
            }
            ++written;
            continue;
        }

        const core::TextAnchor anchor =
            align.empty() ? doc.texts().anchor(slot) : anchor_from(align.as_text());
        core::TextLines lines = doc.texts().lines(slot);
        std::optional<core::Mm> width;
        if (!read_lines(ctx, lines, width)) co_return;

        // THE BASELINE FOLLOWS THE WORDS. Its length is the box the cull and the
        // pick use — the width a wrapping text breaks to, or the estimate of the
        // text's own — and a rewrite that kept the old length left a longer
        // caption unpickable past its old end. The anchor and the direction stay.
        const core::RingSpan span = doc.geometry().rings_of(slot);
        if (doc.entities().kind[e] == core::kPolylineKind && span.count == 1 &&
            doc.geometry().ring_count[span.first] == 2) {
            const core::Point2 from = doc.geometry().vertex(span.first, 0);
            const core::Point2 to   = doc.geometry().vertex(span.first, 1);
            core::Mm length         = 0;
            if (width.has_value() && lines.wrap)
                length = *width;
            else if (!lines.wrap && (!content.empty() || !tall.empty() || width.has_value()))
                length = core::text_width_estimate(words, height);
            if (length > 0) {
                const std::array<core::Point2, 2> base = baseline_of(from, to, length);
                if (base[1] != to) {
                    const core::RingGeometry::RingInput ring{base, core::RingRole::Open, 0};
                    if (auto st = ctx.transaction().set_geometry(
                            e, std::span<const core::RingGeometry::RingInput>(&ring, 1));
                        !st) {
                        ctx.refuse(st.error());
                        co_return;
                    }
                }
            }
        }

        if (auto st = ctx.transaction().set_text(e, words, height, anchor, lines); !st) {
            ctx.refuse(st.error());
            co_return; // the bus rolls the whole transaction back
        }
        ++written;
    }

    if (written == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Seçimde yazı taşıyan nesne yok.");
        co_return;
    }

    ctx.record("nesneler", Value::ids(picked));
    if (!content.empty()) ctx.record("yazi", Value::text(with_breaks(content.as_text())));
    if (!tall.empty()) ctx.record("yukseklik", tall);
    if (!align.empty()) ctx.record("hizalama", align);

    ctx.echo("Yazı güncellendi: " + std::to_string(written) + " nesne" +
             (skipped > 0 ? ", " + std::to_string(skipped) + " nesne yazı taşımıyordu" : ""));
}

} // namespace

KENTOS_COMMAND(text)
{
    return CommandSpec{
        .id       = "core.text",
        .names    = {"METİN", "METIN", "YAZI", "TEXT", "MT"},
        .title    = "Metin",
        .category = Category::Draw,
        .params =
            {
                Param::point("noktalar", "Yazının başlangıç noktası").en("points"),
                Param::text("yazi", Arity::exactly(1), "Yazılacak metin").en("text"),
                Param::integer("yukseklik", Arity::optional(),
                               "Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı")
                    .en("height"),
                // A point list with optional arity, because Param::point takes no
                // Arity and is therefore always required — and a mandatory end
                // point would make every horizontal caption two clicks.
                Param::points("bitis", Arity::optional(), "Taban çizgisinin bitişi; yoksa yatay")
                    .en("end"),
                Param::choice("hizalama", Arity::optional(), anchor_words(),
                              "Noktanın yazının neresinde durduğu: sol, orta, sag (son satırın "
                              "tabanında), orta_sol, merkez, orta_sag (ortasında), ust_sol, "
                              "ust_orta, ust_sag (ilk satırın üstünde)")
                    .en("alignment"),
                Param::number("satir_araligi", Arity::optional(),
                              "Satırlar arası, tek aralığın katı (0,25–4); tek aralık yüksekliğin "
                              "5/3'ü")
                    .en("line_spacing"),
                Param::number("genislik", Arity::optional(),
                              "Satırların kırılacağı genişlik; verilirse uzun satır kelime "
                              "sınırından alta geçer")
                    .measured_in("m")
                    .en("width"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizime tek ya da çok satırlı metin yazar; yükseklik, dokuz hizalama, satır "
                   "aralığı ve kırılma genişliği verilebilir.",
        .run = &run,
    };
}

/// YAZIDÜZENLE — rewrite a caption that is already on the drawing.
///
/// SEPARATE FROM `METİN`, and for the reason `STİLAKTAR` is separate from
/// `DIŞAAKTAR`: the two do different things to different objects. `METİN` DRAWS a
/// caption and needs a point to draw it at; this one changes the words, the
/// height or the alignment of captions that exist and needs no point at all.
/// Folding them together would mean a `METİN` that sometimes drew and sometimes
/// did not, decided by whether an argument happened to be present.
///
/// It also closes a hole the property panel named out loud: the caption's text
/// was shown read-only there with the note "a row becomes editable when a command
/// exists that changes it, and this one does not yet". Now it does.
KENTOS_COMMAND(edittext)
{
    return CommandSpec{
        .id       = "core.edittext",
        .names    = {"YAZIDÜZENLE", "YAZIDUZENLE", "EDITTEXT", "YZD"},
        .title    = "Yazıyı Düzenle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Düzenlenecek yazılar; verilmezse seçim"}
                    .en("objects"),
                Param::text("yazi", Arity::optional(), "Yeni metin; verilmezse değişmez")
                    .en("text"),
                Param::integer("yukseklik", Arity::optional(),
                               "Yeni yükseklik, zeminde milimetre; verilmezse değişmez")
                    .en("height"),
                Param::choice("hizalama", Arity::optional(), anchor_words(),
                              "Yeni hizalama (METİN'deki dokuz sözcük); verilmezse değişmez")
                    .en("alignment"),
                Param::number("satir_araligi", Arity::optional(),
                              "Yeni satır aralığı, tek aralığın katı (0,25–4); verilmezse değişmez")
                    .en("line_spacing"),
                Param::number("genislik", Arity::optional(),
                              "Satırların kırılacağı genişlik; 0 kırmayı kapatır, verilmezse "
                              "değişmez")
                    .measured_in("m")
                    .en("width"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Var olan bir yazının metnini, yüksekliğini, hizalamasını, satır aralığını ya "
                   "da kırılma genişliğini değiştirir.",
        .run = &run_edit,
    };
}

} // namespace kentos::command
