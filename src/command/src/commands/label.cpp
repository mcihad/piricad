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
// labels are TEXT entities too.
//
// AND A LABEL FOLLOWS WHAT IT NAMES (TODOS C-12). Each one is attached to its
// feature (core/attach.hpp: the middle of it, its words the format filled from
// it), so a corner dragged, a parcel moved or a column edited rewrites it at the
// commit of that edit — and running the command again refreshes the labels it
// wrote rather than writing a second set over them. `bagla=hayır` writes plain
// captions, as it always did.
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

#include "kentos_cad/command/text_fields.hpp"
#include "kentos_cad/command/transaction.hpp"

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/text_store.hpp"

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
    // THE LAYERS THIS DRAWING HAS. A prompt for a name is unanswerable by a
    // mouse; the shell offers what the command names (`Prompt::choices`).
    std::vector<std::string> known;
    for (const core::Layer& l : ctx.document().layers())
        known.push_back(l.name);
    auto source_name = co_await ctx.text("katman", "Etiketlenecek katman", known);
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

    // FOLLOWING, unless told not to: each label attached to its feature.
    const Value follow_arg = ctx.argument("bagla");
    const bool follow      = follow_arg.empty() || follow_arg.as_bool();

    // ---- pass 1: decide. Nothing below this point may fail. ----
    //
    // Every text is rendered and every position computed before the first write,
    // so a format string naming a column that turns out to be unreadable leaves
    // the drawing untouched instead of half-labelled (Article 1.6). A symbol's
    // slot is the format `{sutun}` at its own offset and height, so both roads
    // come out as one list.
    struct Planned
    {
        core::EntityId source{core::kNoEntity};
        std::string format;
        std::string text;
        core::Point2 at{};
        core::Mm height{0};
    };

    std::vector<Planned> planned;

    if (!slots.empty()) {
        // COLUMNS CHECKED ONCE, not per entity: a slot naming a column the
        // drawing does not have is a mistake in the symbol, said up front.
        for (const Slot& slot : slots)
            if (bus.document().attributes().find(slot.field) == core::kNoAttr) {
                ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                             "Sembolün istediği sütun çizimde yok: '" + slot.field +
                                                 "'. SÜTUN ile tanımlayın."));
                co_return;
            }
    }
    const auto& entities = bus.document().entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e) || entities.layer[e] != source) continue;
        // The bounding-box centre, which is where a plan puts a number inside
        // its lekesi; the attachment's centre anchor is the same point.
        const core::Point2 centre = entities.box_of(e).centre();
        const auto plan           = [&](std::string format, core::Mm lift, core::Mm tall) -> bool {
            auto text = fill_fields(bus.document(), e, format);
            if (!text) {
                ctx.session().fail(text.error());
                return false;
            }
            // AN EMPTY CELL WRITES NOTHING. A parcel whose TAKS has not been
            // entered yet gets no figure rather than a `yok` printed inside its
            // circle.
            if (text.value().empty()) return true;
            planned.push_back(Planned{e, std::move(format), std::move(text.value()),
                                      core::Point2{centre.x, centre.y + lift}, tall});
            return true;
        };
        if (!slots.empty()) {
            for (const Slot& slot : slots)
                if (!plan("{" + slot.field + "}", slot.offset,
                          slot.height > 0 ? slot.height : height))
                    co_return;
        } else if (!plan(lines, offset, height)) {
            co_return;
        }
    }

    if (planned.empty()) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "'" + *source_name + "' katmanında etiketlenecek bir şey bulunamadı.");
        co_return;
    }

    // ---- pass 2: write ----
    const core::LayerId target = bus.document().find_layer(target_name) != core::kNoLayer
                                     ? bus.document().find_layer(target_name)
                                     : bus.document().ensure_layer(target_name);

    std::size_t written   = 0;
    std::size_t refreshed = 0;
    std::vector<core::EntityId> dependents;
    for (const Planned& p : planned) {
        const core::Document& doc = bus.document();

        // A LABEL THIS COMMAND ALREADY WROTE is refreshed, not written again: the
        // caption that follows this feature, on this layer, with this format.
        if (follow) {
            core::EntityId found = core::kNoEntity;
            doc.attachments().dependents_of(doc.key_of(p.source), dependents);
            for (const core::EntityId d : dependents) {
                const core::Attachment* a = doc.attachments().get(d);
                if (a != nullptr && a->derive == core::AttachDerive::Fields &&
                    a->anchor == core::AttachAnchor::Centre && a->format == p.format &&
                    doc.alive(d) && doc.entities().layer[d] == target)
                    found = d;
            }
            if (found != core::kNoEntity) {
                const std::uint32_t slot = doc.entities().slot[found];
                if (doc.texts().text(slot) != p.text)
                    if (auto st = ctx.transaction().set_text(
                            found, p.text, doc.texts().height(slot), doc.texts().anchor(slot));
                        !st) {
                        ctx.session().fail(st.error());
                        co_return;
                    }
                ++refreshed;
                continue;
            }
        }

        // The baseline is an ordinary open ring, so the label is culled, snapped
        // and hit-tested by the same code every other entity uses — and as long
        // as the label is wide, by the measure both backends draw it with.
        const core::Point2 end{p.at.x + std::max<core::Mm>(1, core::text_width(p.text, p.height)),
                               p.at.y};
        const std::array<core::Point2, 2> baseline{p.at, end};
        auto created = ctx.transaction().add_polyline(target, baseline);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }

        // Centred on the point both ways, because a label that names a face sits in
        // the middle of it — which is also what puts it inside a `merkez-isaretci`
        // circle drawn by the face's own symbol.
        if (auto st = ctx.transaction().set_text(created.value(), p.text, p.height,
                                                 core::TextAnchor::MiddleCentre);
            !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ++written;
        if (!follow) continue;

        // FOLLOWING ITS FEATURE: the middle of its outer ring, its words the
        // format, the lift the hand's offset from that middle.
        core::Attachment a;
        a.source                  = doc.key_of(p.source);
        a.anchor                  = core::AttachAnchor::Centre;
        a.derive                  = core::AttachDerive::Fields;
        a.format                  = p.format;
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[p.source]);
        if (span.count == 0) continue;
        std::vector<core::Point2> outer;
        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        outer.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            outer.push_back(core::Point2{xs[v], ys[v]});
        const bool closed = doc.geometry().ring_role[span.first] != core::RingRole::Open;
        if (const auto rule = core::attach_place(outer, closed, a, p.height, false); rule)
            core::attach_measure_offset(*rule, p.at, a);
        if (auto st = ctx.transaction().set_attachment(created.value(), a); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    ctx.record("katman", Value::text(*source_name));
    if (!format_text.empty()) ctx.record("bicim", Value::text(format_text));
    ctx.record("hedef", Value::text(target_name));
    ctx.record("yukseklik", Value::integer(height));
    ctx.record("kaydirma", Value::integer(offset));
    if (!follow_arg.empty()) ctx.record("bagla", follow_arg);

    std::string said = std::to_string(written) + " etiket yazıldı: '" + target_name + "' katmanına";
    if (refreshed != 0) said += "; " + std::to_string(refreshed) + " etiket yenilendi";
    ctx.echo(said + ".");
}

} // namespace

KENTOS_COMMAND(label)
{
    return CommandSpec{
        .id       = "core.label",
        .names    = {"ETİKET", "ETIKET", "LABEL", "ETK"},
        .title    = "Etiket",
        .category = Category::Draw,
        .params =
            {
                Param::text("katman", Arity::exactly(1), "Etiketlenecek katmanın adı").en("layer"),
                // OPTIONAL NOW, because a parameterised symbol already says what
                // to write and where. Left required, the bus refused the command
                // before its body could look at the layer's slots (Article 1.3:
                // validate, then run). The body still asks for it interactively
                // when the symbol declares none, so a bare ETİKET on an ordinary
                // layer prompts exactly as it did.
                Param::text("bicim", Arity::optional(),
                            "Etiket biçimi; {sutun} o sütunun değeriyle, {#alan} alanla, {#cevre} "
                            "çevreyle değişir, \\n satır kırar. Sembol alan bildiriyorsa gerekmez")
                    .en("format"),
                Param::text("hedef", Arity::optional(),
                            "Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET'")
                    .en("target_layer"),
                Param::integer("yukseklik", Arity::optional(), "Yazı yüksekliği, zemin milimetresi")
                    .en("height"),
                Param::integer("kaydirma", Arity::optional(),
                               "Nesnenin ortasından dikey kaydırma, zemin milimetresi; "
                               "artı yukarı")
                    .en("offset"),
                Param::boolean("bagla", Arity::optional(),
                               "Etiket nesnesine bağlansın mı: bağlı etiket nesne ya da sütunu "
                               "değişince yeniden yazılır, komut yeniden çalışınca yenilenir; "
                               "varsayılan evet")
                    .en("follow"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katmandaki nesneleri özniteliklerinden ve ölçülerinden okuyarak etiketler; "
                   "etiket nesnesini izler.",
        .run     = &run,
    };
}

} // namespace kentos::command
