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
// project exactly one grammar, `kentos_cad/command/parser.hpp`, and adding any of
// those things here would be a second one. Extending this is an amendment, not a
// patch — the same boundary `kentos_cad/core/style_rule.hpp` draws around its closed
// set of conditions.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"

#include <array>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The default height of a label, in ground millimetres.
///
/// Two metres on the ground, which is what an ada number is on a 1/1000 pafta.
/// A starting point the caller overrides, not a figure from a regulation: MPYY
/// prescribes text heights per plan type and those live in /data, not here.
constexpr core::Mm kDefaultHeight = 2000;

/// One value as it should read on paper — the shared formatter, with the mark
/// a Turkish plan sheet uses. It lives in `core` because the attribute table
/// needs the same arithmetic with the other mark, and two copies would drift.
std::string as_text(const core::AttrValue& value)
{
    return core::attr_display(value, core::DecimalMark::Comma);
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

/// One text slot a symbol declares: which column it reads, where it sits
/// relative to the object's centre, and how tall it is written.
///
/// Ground millimetres throughout, because that is what a text ENTITY is measured
/// in and the caller has already refused anything else.
struct Slot
{
    std::string field;
    core::Mm offset{0};
    core::Mm height{0}; ///< 0 = take the command's own height
};

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

    // THE SYMBOL'S OWN SLOTS, when it has any and no format was typed.
    //
    // This is the whole point of a parameterised gösterim: the symbol already
    // knows WHERE each figure goes — a `yapılaşma koşulu` circle carries one slot
    // above its rule and one below, at the offsets the published symbol draws its
    // fixed words at — and until now the user had to say it again, one `ETİKET`
    // per figure with a hand-measured `kaydirma`. The symbol says it once.
    //
    // Collected BEFORE the format is asked for, because having slots is what
    // makes the question unnecessary. Typing `bicim` anyway overrides them, which
    // is how a one-off label on a parameterised layer stays possible.
    std::vector<Slot> slots;
    if (ctx.argument("bicim").empty()) {
        const core::Layer* record = bus.document().layer(source);
        if (record != nullptr && bus.document().styles().contains(record->style)) {
            const core::Symbol& symbol = bus.document().styles().symbol_at(record->style);
            for (const core::SymbolLayer& sl : symbol.layers) {
                if (sl.type != core::SymbolLayerType::TextMarker || !sl.enabled) continue;

                // EVERY TEXT BINDING ON THE LAYER, because a symbol carries as
                // many parameters as its author wanted. The other kinds are
                // `STİL`'s to resolve: they land in the appearance rather than on
                // paper as an entity of their own.
                std::vector<const core::SymbolBinding*> writes;
                for (const core::SymbolBinding& b : sl.bindings)
                    if (b.what == core::SymbolProperty::Text && !b.field.empty())
                        writes.push_back(&b);
                if (writes.empty()) continue;

                // GROUND, and refused rather than guessed. A slot's offset and
                // size are what place a real text ENTITY on the drawing, and a
                // paper micrometre becomes a ground millimetre only through a
                // plot scale this command does not have. Guessing one would put
                // the figure in the wrong place at every scale but one.
                if (sl.offset.unit != core::Unit::Ground ||
                    (!sl.size.empty() && sl.size.unit != core::Unit::Ground)) {
                    ctx.session().fail(
                        core::err(core::ErrorCode::ValidationFailed,
                                  "'" + writes.front()->field +
                                      "' alanının kaydırması ve boyutu zemin biriminde olmalı: "
                                      "STİL ... birim=zemin ile verin."));
                    co_return;
                }
                for (const core::SymbolBinding* b : writes)
                    slots.push_back(Slot{b->field, static_cast<core::Mm>(sl.offset.value),
                                         static_cast<core::Mm>(sl.size.value)});
            }
        }
    }

    // The prompt NAMES what the user is being asked for, and its example uses the
    // column names a plan sheet actually has. It carries no regulatory value: the
    // TAKS and KAKS figures themselves live in /data and in the drawing.
    const char* kFormatPrompt = "Etiket biçimi, örnek: TAKS {taks} / KAKS {kaks}"; // ui-label

    std::string format_text;
    if (slots.empty()) {
        auto format = co_await ctx.text("bicim", kFormatPrompt);
        if (!format || format->empty()) co_return;
        format_text = *format;
    }

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

    // THE OFFSET, and the MPYY building-condition symbol is why it exists.
    //
    // That symbol is a circle with a horizontal rule across it, one figure above
    // the rule and the other below. Both are attributes of the same parcel and
    // both are placed from the same bounding-box centre, so without an offset the
    // two land on top of each other and on the rule between them. One `ETİKET`
    // per figure, each with its own offset, is how the published symbol is built
    // — and it is the same shape as the two `yazi-isaretci` layers that draw the
    // fixed words, which carry an offset for exactly the same reason.
    const core::Mm offset = static_cast<core::Mm>(ctx.argument("kaydirma").as_int(0));

    // Line breaks arrive already decoded: the command-line lexer turns `\n` into
    // a newline and a JSON script writes one directly. Nothing to do here, which
    // is the point — there is one lexer in this program (CLAUDE.md 5.11).
    const std::string& lines = format_text;

    // ---- pass 1: decide. Nothing below this point may fail. ----
    //
    // Every text is rendered and every position computed before the first write,
    // so a format string naming a column that turns out to be unreadable leaves
    // the drawing untouched instead of half-labelled (Article 1.6).
    std::vector<core::Point2> where;
    std::vector<std::string> texts;
    std::vector<core::Mm> heights;

    if (!slots.empty()) {
        // COLUMNS RESOLVED ONCE, not per entity. `AttrTable::find` folds a Turkish
        // string and walks a map; doing it inside the entity loop would pay for it
        // 71 820 times on the sheet this feature was written for, for an answer
        // that cannot change while the loop runs.
        std::vector<core::AttrId> columns;
        columns.reserve(slots.size());
        for (const Slot& slot : slots) {
            const core::AttrId col = bus.document().attributes().find(slot.field);
            if (col == core::kNoAttr) {
                ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                             "Sembolün istediği sütun çizimde yok: '" + slot.field +
                                                 "'. SÜTUN ile tanımlayın."));
                co_return;
            }
            columns.push_back(col);
        }

        const auto& entities = bus.document().entities();
        for (core::EntityId e = 0; e < entities.size(); ++e) {
            if (!entities.alive(e) || entities.layer[e] != source) continue;

            const core::Point2 centre = entities.box_of(e).centre();
            for (std::size_t s = 0; s < slots.size(); ++s) {
                auto value = bus.document().attribute(columns[s], e);
                if (!value) {
                    ctx.session().fail(value.error());
                    co_return;
                }
                // AN EMPTY CELL WRITES NOTHING. A parcel whose TAKS has not been
                // entered yet gets no figure rather than a `yok` printed inside
                // its circle.
                if (!value.value().present) continue;

                std::string words = as_text(value.value());
                if (words.empty()) continue;

                where.push_back(core::Point2{centre.x, centre.y + slots[s].offset});
                texts.push_back(std::move(words));
                heights.push_back(slots[s].height > 0 ? slots[s].height : height);
            }
        }
    } else {
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
            core::Point2 at = entities.box_of(e).centre();
            at.y += offset;
            where.push_back(at);
            texts.push_back(std::move(text.value()));
            heights.push_back(height);
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
        const core::Mm tall      = heights[i];
        const auto chars         = static_cast<core::Mm>(texts[i].size());
        const core::Point2 start = where[i];
        const core::Point2 end{start.x + (tall * 6 * chars) / 10, start.y};

        const std::array<core::Point2, 2> baseline{start, end};
        auto created = ctx.transaction().add_polyline(target, baseline);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }

        // Centred on the point both ways, because a label that names a face sits in
        // the middle of it — which is also what puts it inside a `merkez-isaretci`
        // circle drawn by the face's own symbol.
        if (auto st = ctx.transaction().set_text(created.value(), texts[i], tall,
                                                 core::TextAnchor::MiddleCentre);
            !st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    ctx.record("katman", Value::text(*source_name));
    if (!format_text.empty()) ctx.record("bicim", Value::text(format_text));
    ctx.record("hedef", Value::text(target_name));
    ctx.record("yukseklik", Value::integer(height));
    ctx.record("kaydirma", Value::integer(offset));

    ctx.echo(std::to_string(texts.size()) + " etiket yazıldı: '" + target_name + "' katmanına.");
}

} // namespace

KENTOS_COMMAND(label)
{
    return CommandSpec{
        .id       = "core.label",
        .names    = {"ETİKET", "ETIKET", "LABEL", "ETK"},
        .category = Category::Draw,
        .params =
            {
                Param::text("katman", Arity::exactly(1), "Etiketlenecek katmanın adı"),
                // OPTIONAL NOW, because a parameterised symbol already says what
                // to write and where. Left required, the bus refused the command
                // before its body could look at the layer's slots (Article 1.3:
                // validate, then run). The body still asks for it interactively
                // when the symbol declares none, so a bare ETİKET on an ordinary
                // layer prompts exactly as it did.
                Param::text("bicim", Arity::optional(),
                            "Etiket biçimi; {sutun} o sütunun değeriyle değişir, \\n satır kırar. "
                            "Sembol alan bildiriyorsa gerekmez"),
                Param::text("hedef", Arity::optional(),
                            "Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET'"),
                Param::integer("yukseklik", Arity::optional(),
                               "Yazı yüksekliği, zemin milimetresi"),
                Param::integer("kaydirma", Arity::optional(),
                               "Nesnenin ortasından dikey kaydırma, zemin milimetresi; "
                               "artı yukarı"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katmandaki nesneleri özniteliklerinden okuyarak etiketler.",
        .run     = &run,
    };
}

} // namespace kentos::command
