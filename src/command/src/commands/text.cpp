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

} // namespace kentos::command
