// SPDX-License-Identifier: GPL-3.0-or-later
// ETİKET — writes what a layer's attributes say onto the drawing.
//
// The case this exists for is a plan sheet: MPYY prints TAKS and KAKS inside a
// circle on a `yapılaşma koşulu` island, a frontage width beside a building line,
// an ada/parsel number in a parcel. All of those are ATTRIBUTES of the feature and
// TEXT on the paper, and something has to carry one to the other.
//
// WHY A COMMAND AND NOT A SYMBOL LAYER. `.claude/model.md` P29 forbids the frame
// path from reading attribute columns at all, and it is right to: a renderer that
// looked up a column per entity per frame would put a hash lookup inside the 16 ms
// budget and make the cull test depend on the attribute table's layout. The same
// rule is why a GIS renderer here is "a command that writes the style column"
// (R14). A labeller is the same shape:
//
//     **A labeller is a command that writes TEXT ENTITIES.**
//
// So a label is an ordinary drawing object. It can be moved, restyled, put on its
// own layer and switched off, it round-trips through `.pcad` and DXF without
// anything new, and it is exactly what a CAD label has always been — AutoCAD's
// labels are TEXT entities too. The cost is that a label does not follow a later
// attribute edit; re-running the command refreshes them, and that is the same
// bargain every CAD annotation makes.
//
// THE FORMAT STRING IS A SUBSTITUTION, NOT A LANGUAGE. `{sutun}` is replaced by
// that column's value and nothing else happens: there is no operator, no nesting,
// no function, no conditional, no number formatting. CLAUDE.md 5.11 allows this
// project exactly one grammar, `piricad/command/parser.hpp`, and adding any of
// those things here would be a second one. Extending this is an amendment, not a
// patch — the same boundary `piricad/core/style_rule.hpp` draws around its closed
// set of conditions.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/attribute.hpp"

#include <array>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// The default height of a label, in ground millimetres.
///
/// Two metres on the ground, which is what an ada number is on a 1/1000 pafta.
/// A starting point the caller overrides, not a figure from a regulation: MPYY
/// prescribes text heights per plan type and those live in /data, not here.
constexpr core::Mm kDefaultHeight = 2000;

/// One value as it should read on paper.
///
/// A `Length` column is stored in millimetres and printed in metres, because that
/// is the unit a plan sheet writes; an absent cell prints as nothing rather than
/// as a zero, since an unmeasured frontage and a zero frontage are different
/// facts about a parcel.
std::string as_text(const core::AttrValue& value)
{
    if (!value.present) return {};

    switch (value.type) {
    case core::AttrType::Text:
    case core::AttrType::CodeRef: return value.text;
    case core::AttrType::Bool: return value.number != 0 ? "evet" : "hayır";
    case core::AttrType::Length: {
        // Millimetres to metres with three decimals, built from integers so no
        // locale can put a comma where a golden fixture expects a point
        // (core.md R9).
        const bool negative          = value.number < 0;
        const std::int64_t magnitude = negative ? -value.number : value.number;

        std::string out             = std::to_string(magnitude / 1000);
        const std::int64_t fraction = magnitude % 1000;
        if (fraction != 0) {
            std::string digits = std::to_string(fraction);
            digits.insert(0, static_cast<std::size_t>(3 - digits.size()), '0');
            while (!digits.empty() && digits.back() == '0')
                digits.pop_back();
            out += "," + digits;
        }
        return negative ? "-" + out : out;
    }
    case core::AttrType::Int64: return std::to_string(value.number);
    }
    return {};
}

/// Replaces every `{sutun}` in `format` with that column's value for `e`.
///
/// A brace that names no declared column is left ALONE, braces and all. Silently
/// deleting it would hide a typo in a format string that a plan sheet is about to
/// be printed from; leaving it visible is how the user sees the mistake.
core::Result<std::string> render(const core::Document& doc, const std::string& format,
                                 core::EntityId e)
{
    std::string out;
    out.reserve(format.size());

    for (std::size_t i = 0; i < format.size();) {
        if (format[i] != '{') {
            out += format[i++];
            continue;
        }

        const std::size_t close = format.find('}', i + 1);
        if (close == std::string::npos) {
            out += format[i++];
            continue;
        }

        const std::string name = format.substr(i + 1, close - i - 1);
        const core::AttrId col = doc.attributes().find(name);

        if (col == core::kNoAttr) {
            out += format.substr(i, close - i + 1);
        } else {
            auto value = doc.attribute(col, e);
            if (!value) return value.error();
            out += as_text(value.value());
        }
        i = close + 1;
    }
    return out;
}

Task<void> run(Context& ctx)
{
    auto source_name = co_await ctx.text("katman", "Etiketlenecek katman");
    if (!source_name || source_name->empty()) co_return;

    Bus& bus                   = ctx.session().bus();
    const core::LayerId source = bus.document().find_layer(*source_name);
    if (source == core::kNoLayer) {
        ctx.session().fail(
            core::err(core::ErrorCode::NotFound, "Katman bulunamadı: '" + *source_name +
                                                     "'. Önce KATMAN komutuyla oluşturun."));
        co_return;
    }

    // The prompt NAMES what the user is being asked for, and its example uses the
    // column names a plan sheet actually has. It carries no regulatory value: the
    // TAKS and KAKS figures themselves live in /data and in the drawing.
    const char* kFormatPrompt = "Etiket biçimi, örnek: TAKS {taks} / KAKS {kaks}"; // ui-label
    auto format               = co_await ctx.text("bicim", kFormatPrompt);
    if (!format || format->empty()) co_return;

    // Labels go on their OWN layer by default, so a sheet can be plotted with and
    // without them and so restyling them does not touch the parcels. The name is
    // built from the source layer's, which is a file name rather than a regulatory
    // value.
    const Value target_arg = ctx.argument("hedef");
    const std::string target_name =
        target_arg.empty() ? *source_name + " ETİKET" : target_arg.as_text();

    const core::Mm height =
        ctx.argument("yukseklik").empty()
            ? kDefaultHeight
            : static_cast<core::Mm>(ctx.argument("yukseklik").as_int(kDefaultHeight));

    if (height <= 0) {
        ctx.session().fail(core::err(core::ErrorCode::ValidationFailed,
                                     "Etiket yüksekliği sıfırdan büyük olmalı."));
        co_return;
    }

    // Line breaks arrive already decoded: the command-line lexer turns `\n` into
    // a newline and a JSON script writes one directly. Nothing to do here, which
    // is the point — there is one lexer in this program (CLAUDE.md 5.11).
    const std::string& lines = *format;

    // ---- pass 1: decide. Nothing below this point may fail. ----
    //
    // Every text is rendered and every position computed before the first write,
    // so a format string naming a column that turns out to be unreadable leaves
    // the drawing untouched instead of half-labelled (Article 1.6).
    std::vector<core::Point2> where;
    std::vector<std::string> texts;

    {
        const auto& entities = bus.document().entities();
        for (core::EntityId e = 0; e < entities.size(); ++e) {
            if (!entities.alive(e) || entities.layer[e] != source) continue;

            auto text = render(bus.document(), lines, e);
            if (!text) {
                ctx.session().fail(text.error());
                co_return;
            }
            if (text.value().empty()) continue; // nothing to say about this one

            // The bounding-box centre, which is where a plan puts a number inside
            // its lekesi. The area centroid of a ring with holes is a different
            // computation and belongs in core rather than in a command.
            where.push_back(entities.box_of(e).centre());
            texts.push_back(std::move(text.value()));
        }
    }

    if (texts.empty()) {
        ctx.echo("'" + *source_name + "' katmanında etiketlenecek bir şey bulunamadı.");
        co_return;
    }

    // ---- pass 2: write ----
    const core::LayerId target = bus.document().find_layer(target_name) != core::kNoLayer
                                     ? bus.document().find_layer(target_name)
                                     : bus.document().ensure_layer(target_name);

    for (std::size_t i = 0; i < texts.size(); ++i) {
        // The baseline is an ordinary open ring, so the label is culled, snapped
        // and hit-tested by the same code every other entity uses. Its advance is
        // approximate on purpose: it sets the cull box, and the backend measures
        // the real font when it draws.
        const auto chars         = static_cast<core::Mm>(texts[i].size());
        const core::Point2 start = where[i];
        const core::Point2 end{start.x + (height * 6 * chars) / 10, start.y};

        const std::array<core::Point2, 2> baseline{start, end};
        auto created = ctx.transaction().add_polyline(target, baseline);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }

        // Centred on the point both ways, because a label that names a face sits in
        // the middle of it — which is also what puts it inside a `merkez-isaretci`
        // circle drawn by the face's own symbol.
        if (auto st = ctx.transaction().set_text(created.value(), texts[i], height,
                                                 core::TextAnchor::MiddleCentre);
            !st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    ctx.record("katman", Value::text(*source_name));
    ctx.record("bicim", Value::text(*format));
    ctx.record("hedef", Value::text(target_name));
    ctx.record("yukseklik", Value::integer(height));

    ctx.echo(std::to_string(texts.size()) + " etiket yazıldı: '" + target_name + "' katmanına.");
}

} // namespace

PIRICAD_COMMAND(label)
{
    return CommandSpec{
        .id       = "core.label",
        .names    = {"ETİKET", "ETIKET", "LABEL", "ETK"},
        .category = Category::Draw,
        .params =
            {
                Param::text("katman", Arity::exactly(1), "Etiketlenecek katmanın adı"),
                Param::text("bicim", Arity::exactly(1),
                            "Etiket biçimi; {sutun} o sütunun değeriyle değişir, \\n satır kırar"),
                Param::text("hedef", Arity::optional(),
                            "Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET'"),
                Param::integer("yukseklik", Arity::optional(),
                               "Yazı yüksekliği, zemin milimetresi"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katmandaki nesneleri özniteliklerinden okuyarak etiketler.",
        .run     = &run,
    };
}

} // namespace piricad::command
