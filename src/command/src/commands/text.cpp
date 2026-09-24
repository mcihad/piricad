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

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/dimension.hpp"

#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_store.hpp"

#include <algorithm>
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

// ------------------------------------------------------- BULDEĞİŞTİR -------

/// One code point of a caption: where it starts, how long it is, and the form
/// a case-blind search compares.
struct Letter
{
    std::size_t at{0};
    std::size_t length{0};
    std::string folded;
};

/// The code points of `utf8`, each with its Turkish capital when `fold` — `i`
/// is `İ` and `ı` is `I` in this alphabet, which a byte-wise `toupper` gets
/// wrong (CLAUDE.md 5.6).
std::vector<Letter> letters_of(std::string_view utf8, bool fold)
{
    std::vector<Letter> out;
    out.reserve(utf8.size());
    for (std::size_t i = 0; i < utf8.size();) {
        std::size_t n = 1;
        const auto c  = static_cast<unsigned char>(utf8[i]);
        if (c >= 0xF0U)
            n = 4;
        else if (c >= 0xE0U)
            n = 3;
        else if (c >= 0xC0U)
            n = 2;
        n                            = std::min(n, utf8.size() - i);
        const std::string_view piece = utf8.substr(i, n);
        out.push_back(Letter{i, n, fold ? core::turkish_upper(piece) : std::string(piece)});
        i += n;
    }
    return out;
}

/// Whether a code point is part of a word, for `tam_kelime`.
///
/// A LETTER OR A DIGIT OF ANY ALPHABET is; a space, a punctuation mark or a
/// symbol is not — `«ADA»`, `ADA…`, `⌀120` and `m²` hold the words ADA, 120
/// and m. The layers below Qt link no Unicode library (the tree has no ICU), so
/// the rule is written out here and kept narrow, as the Turkish casing table
/// is: ASCII by class, and past ASCII everything is a letter except the Latin-1
/// signs (U+0080–U+00BF, × and ÷), the punctuation and symbol blocks
/// U+2000–U+2BFF, and CJK punctuation U+3000–U+303F. Turkish letters, and
/// every other alphabet's, fall on the letter side of it.
bool word_letter(std::string_view piece) noexcept
{
    if (piece.empty()) return false;
    const auto lead = static_cast<unsigned char>(piece.front());
    if (lead < 0x80U)
        return (lead >= '0' && lead <= '9') || (lead >= 'A' && lead <= 'Z') ||
               (lead >= 'a' && lead <= 'z') || lead == '_';
    const std::size_t n = piece.size();
    if (n == 1) return true; // a broken sequence: part of whatever word it sits in
    std::uint32_t cp = 0;
    if (n == 2)
        cp = (lead & 0x1FU) << 6U;
    else if (n == 3)
        cp = (lead & 0x0FU) << 12U;
    else
        cp = (lead & 0x07U) << 18U;
    for (std::size_t k = 1; k < n; ++k)
        cp |= (static_cast<std::uint32_t>(static_cast<unsigned char>(piece[k])) & 0x3FU)
              << (6U * static_cast<std::uint32_t>(n - 1 - k));
    if (cp <= 0xBFU || cp == 0xD7U || cp == 0xF7U) return false;
    if (cp >= 0x2000U && cp <= 0x2BFFU) return false;
    return cp < 0x3000U || cp > 0x303FU;
}

/// Every place `needle` occurs in `hay`, left to right, not overlapping, as byte
/// ranges of `hay`: case-blind in Turkish when `fold`, and only where it stands
/// as a word of its own when `whole`.
std::vector<std::pair<std::size_t, std::size_t>>
find_all(std::string_view hay, std::string_view needle, bool fold, bool whole)
{
    std::vector<std::pair<std::size_t, std::size_t>> out;
    const std::vector<Letter> h = letters_of(hay, fold);
    const std::vector<Letter> n = letters_of(needle, fold);
    if (n.empty() || n.size() > h.size()) return out;
    for (std::size_t i = 0; i + n.size() <= h.size();) {
        bool same = true;
        for (std::size_t k = 0; same && k < n.size(); ++k)
            same = h[i + k].folded == n[k].folded;
        if (same && whole) {
            const bool open_before =
                i == 0 || !word_letter(hay.substr(h[i - 1].at, h[i - 1].length));
            const std::size_t after = i + n.size();
            const bool open_after =
                after == h.size() || !word_letter(hay.substr(h[after].at, h[after].length));
            same = open_before && open_after;
        }
        if (!same) {
            ++i;
            continue;
        }
        const std::size_t from = h[i].at;
        const Letter& last     = h[i + n.size() - 1];
        out.emplace_back(from, last.at + last.length - from);
        i += n.size();
    }
    return out;
}

/// Whether a caption has no words left: nothing, or only spaces and breaks.
bool blank(std::string_view words) noexcept
{
    return std::ranges::all_of(words, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });
}

/// A caption on one line, for a preview: a line break as `\n`, long ones cut.
std::string preview_of(std::string_view words)
{
    std::string out;
    for (const char c : words) {
        if (c == '\n')
            out += "\\n";
        else
            out += c;
        if (out.size() > 60) {
            out += "…";
            break;
        }
    }
    return out;
}

Task<void> run_find_replace(Context& ctx)
{
    const core::Document& doc = ctx.document();
    auto needle               = co_await ctx.text("bul", "Aranacak yazı");
    if (!needle) co_return;
    if (needle->empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Aranacak yazı boş olamaz.");
        co_return;
    }
    const std::string wanted  = with_breaks(*needle);
    const Value replacement   = ctx.argument("degistir");
    const bool replacing      = !replacement.empty();
    const std::string instead = replacing ? with_breaks(replacement.as_text()) : std::string();
    const bool match_case     = ctx.argument("buyuk_kucuk").as_bool();
    const bool whole          = ctx.argument("tam_kelime").as_bool();
    const Value layer_arg     = ctx.argument("katman");
    const core::LayerId only =
        layer_arg.empty() ? core::kNoLayer : doc.find_layer(layer_arg.as_text());
    if (!layer_arg.empty() && only == core::kNoLayer) {
        ctx.refuse(core::ErrorCode::NotFound, "Katman bulunamadı: '" + layer_arg.as_text() + "'.");
        co_return;
    }
    // Held by name: `argument` returns a copy, and a loop over the list of a
    // temporary walks a list that is already gone (C++20 extends the life of
    // the last temporary of a range, not of the one it was read from).
    const Value named_arg = ctx.argument("nesneler");
    std::vector<core::EntityId> named;
    for (const std::int64_t raw : named_arg.as_ids()) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
        if (e != core::kNoEntity) named.push_back(e);
    }

    // ---- what matches, and what it would say ----
    //
    // The captions themselves: not a dimension's figure (ÖLÇÜDÜZENLE writes
    // that), not a caption filled from its object (its words are its format's,
    // and would come back at the next change), not a member of a block
    // definition (every reference would change at once).
    struct Hit
    {
        core::EntityId e{core::kNoEntity};
        std::string was;
        std::string now;
        std::size_t count{0};
    };

    std::vector<Hit> hits;
    std::size_t filled  = 0;
    std::size_t emptied = 0;
    std::size_t matches = 0;
    const auto& ents    = doc.entities();
    const auto consider = [&](core::EntityId e) {
        if (e >= ents.size() || !doc.alive(e) || !doc.texts().has(ents.slot[e])) return;
        if (ents.kind[e] == core::kDimensionKind || (ents.flags[e] & core::FlagInBlock) != 0)
            return;
        if (only != core::kNoLayer && ents.layer[e] != only) return;
        const std::string_view words = doc.texts().text(ents.slot[e]);
        const auto found             = find_all(words, wanted, !match_case, whole);
        if (found.empty()) return;
        if (const core::Attachment* a = doc.attachments().get(e);
            a != nullptr && a->derive != core::AttachDerive::Keep) {
            ++filled;
            return;
        }
        Hit hit{e, std::string(words), {}, found.size()};
        std::size_t from = 0;
        for (const auto& [at, length] : found) {
            hit.now.append(words.substr(from, at - from));
            hit.now.append(instead);
            from = at + length;
        }
        hit.now.append(words.substr(from));
        // A CAPTION IS NOT EMPTIED by a replacement: a caption with no words is
        // written as a bare line, which is a text turned into a line without
        // anybody asking. It keeps its words and the answer counts it; SİL is
        // how a caption goes.
        if (replacing && blank(hit.now)) {
            ++emptied;
            return;
        }
        matches += found.size();
        hits.push_back(std::move(hit));
    };
    if (named.empty())
        for (core::EntityId e = 0; e < ents.size(); ++e)
            consider(e);
    else
        for (const core::EntityId e : named)
            consider(e);

    ctx.record("bul", Value::text(wanted));
    if (replacing) ctx.record("degistir", Value::text(instead));
    if (match_case) ctx.record("buyuk_kucuk", Value::boolean(true));
    if (whole) ctx.record("tam_kelime", Value::boolean(true));
    if (!layer_arg.empty()) ctx.record("katman", layer_arg);
    if (!named.empty()) ctx.record("nesneler", named_arg);

    // ---- the preview: the same lines to every client, before anything moves ----
    core::Json rows = core::Json::array({});
    for (const Hit& h : hits) {
        core::Json row;
        row.set("nesne",
                core::Json::integer(static_cast<std::int64_t>(core::raw(doc.key_of(h.e)))));
        row.set("once", core::Json::string(h.was));
        row.set("sonra", core::Json::string(replacing ? h.now : h.was));
        row.set("adet", core::Json::integer(static_cast<std::int64_t>(h.count)));
        rows.push(std::move(row));
    }
    core::Json report;
    report.set("yazi", core::Json::integer(static_cast<std::int64_t>(hits.size())));
    report.set("eslesme", core::Json::integer(static_cast<std::int64_t>(matches)));
    report.set("atlanan", core::Json::integer(static_cast<std::int64_t>(filled)));
    report.set("bos_kalacak", core::Json::integer(static_cast<std::int64_t>(emptied)));
    report.set("satirlar", std::move(rows));
    ctx.report(std::move(report));
    std::string skipped;
    if (filled > 0)
        skipped += "; kalıptan doldurulan " + std::to_string(filled) +
                   " yazı atlandı (kalıbı BAĞLA ya da ETİKET ile değişir)";
    if (emptied > 0)
        skipped +=
            "; boş kalacak " + std::to_string(emptied) + " yazı atlandı (bir yazıyı SİL kaldırır)";
    if (hits.empty()) {
        ctx.echo("'" + preview_of(wanted) + "' hiçbir yazıda bulunmadı" + skipped + ".");
        co_return;
    }
    constexpr std::size_t kShown = 12;
    std::string listing = std::to_string(hits.size()) + " yazıda " + std::to_string(matches) +
                          " eşleşme" + skipped + ":";
    for (std::size_t i = 0; i < hits.size() && i < kShown; ++i) {
        listing += "\n  " + std::to_string(core::raw(doc.key_of(hits[i].e))) + ": «" +
                   preview_of(hits[i].was) + "»";
        if (replacing) listing += " → «" + preview_of(hits[i].now) + "»";
    }
    if (hits.size() > kShown)
        listing += "\n  … ve " + std::to_string(hits.size() - kShown) + " yazı daha";
    ctx.echo(listing);

    // FINDING SELECTS what was found, the way SEÇ would, so the next command —
    // YAZIDÜZENLE, SİL, a move — acts on exactly these.
    if (!replacing) {
        Bus& bus = ctx.session().bus();
        bus.remember_selection();
        bus.selection().clear();
        for (const Hit& h : hits)
            (void)bus.selection().add(doc.key_of(h.e));
        if (bus.on_selection_changed) bus.on_selection_changed();
        co_return;
    }

    // THE PREVIEW IS THE QUESTION: nothing is written until it is answered —
    // at the prompt by a hand, by `uygula=evet` in a script.
    auto apply = co_await ctx.boolean("uygula", "Önizlemedeki değişiklikler uygulansın mı?");
    if (!apply) {
        ctx.echo("Değiştirilmedi; uygulamak için uygula=evet verin.");
        co_return;
    }
    ctx.record("uygula", Value::boolean(*apply));
    if (!*apply) {
        ctx.echo("Değiştirilmedi.");
        co_return;
    }
    for (const Hit& h : hits) {
        const std::uint32_t slot    = ents.slot[h.e];
        const core::Mm height       = doc.texts().height(slot);
        const core::TextAnchor from = doc.texts().anchor(slot);
        const core::TextLines lines = doc.texts().lines(slot);
        // The box follows the words, as it does for YAZIDÜZENLE.
        const core::RingSpan span = doc.geometry().rings_of(slot);
        if (!lines.wrap && ents.kind[h.e] == core::kPolylineKind && span.count == 1 &&
            doc.geometry().ring_count[span.first] == 2) {
            const std::array<core::Point2, 2> base = baseline_of(
                doc.geometry().vertex(span.first, 0), doc.geometry().vertex(span.first, 1),
                core::text_width_estimate(h.now, height));
            const core::RingGeometry::RingInput ring{base, core::RingRole::Open, 0};
            if (auto st = ctx.transaction().set_geometry(
                    h.e, std::span<const core::RingGeometry::RingInput>(&ring, 1));
                !st) {
                ctx.refuse(st.error());
                co_return;
            }
        }
        if (auto st = ctx.transaction().set_text(h.e, h.now, height, from, lines); !st) {
            ctx.refuse(st.error());
            co_return; // the bus rolls the whole transaction back
        }
    }
    ctx.echo(std::to_string(hits.size()) + " yazıda " + std::to_string(matches) +
             " eşleşme değiştirildi.");
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

/// BULDEĞİŞTİR — find a word in every caption, and change it everywhere.
///
/// THE PREVIEW IS NOT A MODE. Every run lists what matched and what it would
/// become — the same lines at the prompt, in a script's transcript and in the
/// dialog's table — and a replacement waits for `uygula`: asked at the prompt,
/// named in a script. So a batch rewrite of a sheet's four hundred captions is
/// seen before it happens, by every client, and undone in one step after.
KENTOS_COMMAND(find_replace)
{
    return CommandSpec{
        .id       = "core.find_replace",
        .names    = {"BULDEĞİŞTİR", "BULDEGISTIR", "FINDREPLACE", "BUL"},
        .title    = "Bul ve Değiştir",
        .category = Category::Modify,
        .params =
            {
                Param::text("bul", Arity::exactly(1), "Aranacak yazı; \\n satır sonudur")
                    .en("find"),
                Param::text(
                    "degistir", Arity::optional(),
                    "Yerine yazılacak; boşsa bulunan silinir, verilmezse bulunanlar seçilir")
                    .en("replace"),
                Param::text("katman", Arity::optional(), "Yalnız bu katmandaki yazılar")
                    .en("layer"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Yalnız bu yazılar; verilmezse bütün çizim"}
                    .en("objects"),
                Param::boolean("buyuk_kucuk", Arity::optional(),
                               "Büyük/küçük harf ayrılsın mı; varsayılan hayır (Türkçe İ/ı ile)")
                    .en("match_case"),
                Param::boolean("tam_kelime", Arity::optional(),
                               "Yalnız kendi başına duran kelime; varsayılan hayır")
                    .en("whole_word"),
                Param::boolean("uygula", Arity::optional(),
                               "Önizlemedeki değişiklik uygulansın mı; verilmezse sorulur")
                    .en("apply"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Yazılarda bir sözcüğü bulur, önizler ve hepsinde birden değiştirir; tek geri "
                   "alma adımı.",
        .run = &run_find_replace,
    };
}

} // namespace kentos::command
