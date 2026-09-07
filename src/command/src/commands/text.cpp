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

#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_store.hpp"

#include <array>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

core::TextAnchor anchor_from(const std::string& word)
{
    if (core::turkish_key_equals(word, "orta")) return core::TextAnchor::BaselineCentre;
    if (core::turkish_key_equals(word, "sağ") || core::turkish_key_equals(word, "sag"))
        return core::TextAnchor::BaselineRight;
    if (core::turkish_key_equals(word, "merkez")) return core::TextAnchor::MiddleCentre;
    return core::TextAnchor::BaselineLeft;
}

Task<void> run(Context& ctx)
{
    auto anchor_point = co_await ctx.point("noktalar", "Yazının başlangıç noktası");
    if (!anchor_point) co_return; // ESC before anything was drawn

    auto content = co_await ctx.text("yazi", "Yazılacak metin");
    if (!content || content->empty()) co_return;

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

    // The baseline runs to the second point when one is given, and horizontally
    // for the text's own width when it is not. Either way the entity has two real
    // vertices, so nothing downstream needs to know it is text to cull it.
    core::Point2 end = *anchor_point;
    if (const Value second = ctx.argument("bitis");
        !second.empty() && !second.as_points().empty()) {
        end = second.as_points().front();
    } else {
        // A rough advance of 0.6 em per character. This decides the entity's
        // BOUNDING BOX, not where a glyph lands — the backend measures the real
        // font — so an approximation here costs a slightly loose cull box and
        // nothing that reaches paper.
        const auto chars = static_cast<core::Mm>(content->size());
        end.x += (height * 6 * chars) / 10;
    }

    const std::array<core::Point2, 2> baseline{*anchor_point, end};
    auto created = ctx.transaction().add_polyline(ctx.active_layer(), baseline);
    if (!created) {
        ctx.echo(created.error().message);
        co_return;
    }

    if (auto st = ctx.transaction().set_text(created.value(), *content, height, anchor); !st) {
        ctx.echo(st.error().message);
        co_return; // the bus rolls the whole transaction back, baseline included
    }

    ctx.record("noktalar", Value::points({*anchor_point}));
    ctx.record("yazi", Value::text(*content));
    ctx.record("yukseklik", Value::integer(height));
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

    // NAMED, and not for tidiness: `argument` returns a `Value` by VALUE and
    // `as_ids` refers into it, so the two written together leave the loop reading
    // a destroyed vector (Article 2.2 — C++23 would extend the temporary, C++20
    // does not).
    const Value picked = ctx.argument("nesneler");
    for (std::int64_t raw : picked.as_ids()) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        targets.push_back(slot);
    }
    if (targets.empty())
        for (core::EntityKey k : ctx.session().bus().selection().keys()) {
            const core::EntityId slot = doc.slot_of(k);
            if (slot != core::kNoEntity && doc.alive(slot)) targets.push_back(slot);
        }

    if (targets.empty()) {
        ctx.echo("Düzenlenecek yazı yok. Bir yazı seçin ya da nesneler= ile verin.");
        co_return;
    }

    // ONLY WHAT IS ASKED FOR CHANGES. An unnamed field keeps the value the
    // caption already has: rewriting a parsel number must not silently reset the
    // height a planner chose for it.
    const Value content = ctx.argument("yazi");
    const Value tall    = ctx.argument("yukseklik");
    const Value align   = ctx.argument("hizalama");

    if (content.empty() && tall.empty() && align.empty()) {
        ctx.echo("Değiştirilecek bir şey verilmedi: yazi=, yukseklik= ya da hizalama=.");
        co_return;
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
            content.empty() ? std::string(doc.texts().text(slot)) : content.as_text();
        if (words.empty()) {
            ctx.echo("Boş bir yazı bir yazı değildir; silmek için SİL kullanın.");
            co_return;
        }

        core::Mm height = doc.texts().height(slot);
        if (!tall.empty()) {
            if (tall.as_int() <= 0) {
                ctx.echo("Yazı yüksekliği sıfırdan büyük olmalı.");
                co_return;
            }
            height = static_cast<core::Mm>(tall.as_int());
        }

        const core::TextAnchor anchor =
            align.empty() ? doc.texts().anchor(slot) : anchor_from(align.as_text());

        if (auto st = ctx.transaction().set_text(e, words, height, anchor); !st) {
            ctx.echo(st.error().message);
            co_return; // the bus rolls the whole transaction back
        }
        ++written;
    }

    if (written == 0) {
        ctx.echo("Seçimde yazı taşıyan nesne yok.");
        co_return;
    }

    if (!picked.empty()) ctx.record("nesneler", picked);
    if (!content.empty()) ctx.record("yazi", content);
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
        .category = Category::Draw,
        .params =
            {
                Param::point("noktalar", "Yazının başlangıç noktası"),
                Param::text("yazi", Arity::exactly(1), "Yazılacak metin"),
                Param::integer("yukseklik", Arity::optional(),
                               "Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı"),
                // A point list with optional arity, because Param::point takes no
                // Arity and is therefore always required — and a mandatory end
                // point would make every horizontal caption two clicks.
                Param::points("bitis", Arity::optional(), "Taban çizgisinin bitişi; yoksa yatay"),
                Param::text("hizalama", Arity::optional(), "sol, orta, sag veya merkez"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizime metin yazar; yükseklik ve hizalama verilebilir.",
        .run     = &run,
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
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Düzenlenecek yazılar; verilmezse seçim"},
                Param::text("yazi", Arity::optional(), "Yeni metin; verilmezse değişmez"),
                Param::integer("yukseklik", Arity::optional(),
                               "Yeni yükseklik, zeminde milimetre; verilmezse değişmez"),
                Param::text("hizalama", Arity::optional(),
                            "sol, orta, sag veya merkez; verilmezse değişmez"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir.",
        .run     = &run_edit,
    };
}

} // namespace kentos::command
