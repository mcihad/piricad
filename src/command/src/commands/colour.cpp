// SPDX-License-Identifier: GPL-3.0-or-later
// core.colour — RENK. The stroke and fill colour of the objects in hand.
//
// THE TWO CHIPS AT THE FOOT OF THE TOOL COLUMN ANSWERED "RENK komutu Faz 2".
// They are the stroke and the fill every CAD palette has, and pressing one said
// that it did nothing. This is the command behind them: the named objects, the
// selection, or — with neither — the objects the user is asked to point at, get
// the colour given, or go back to their layer's (`katman`).
//
// AN OVERRIDE, NOT A NEW STYLE SYSTEM. The object's own appearance is read, the
// one property changed, and the result interned into the document's style table
// like every other appearance (model.md R13): a drawing with a thousand red
// lines holds one red record. `katman` hands the property back to the layer,
// which is how a plan sheet is meant to be coloured in the first place.
#include "kentos_cad/command/colour.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/layer.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/core/text.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::command {
namespace {

/// What one colour argument asks for.
struct Wanted
{
    enum class What : std::uint8_t { Keep, Layer, None, Colour };
    What what{What::Keep};
    std::uint32_t rgba{0}; ///< for `Colour`: 0xAARRGGBB
};

/// Reads a colour word (`parse_colour`), `katman` and — for a fill — `yok`.
/// Nothing when the word is none of them.
std::optional<Wanted> read_colour(const std::string& word, bool fill)
{
    if (word.empty()) return Wanted{};
    if (core::turkish_key_equals(word, "katman")) return Wanted{Wanted::What::Layer, 0};
    if (fill && core::turkish_key_equals(word, "yok")) return Wanted{Wanted::What::None, 0};
    if (const auto rgba = parse_colour(word)) return Wanted{Wanted::What::Colour, *rgba};
    return std::nullopt;
}

/// The colour words, comma separated, for a message and a prompt.
std::string named_list()
{
    std::string out;
    for (const NamedColour& n : named_colours()) {
        if (!out.empty()) out += ", ";
        out += n.word;
    }
    return out;
}

/// What a refused colour word is told: the forms that work, and — when the
/// command line turned the word into a number — that it did, because the user
/// typed `#112233` or `0x112233` and is looking at `1122867` in the message.
std::string refusal(const char* what, const std::string& word, bool fill)
{
    std::string out   = std::string("Tanınmayan ") + what + ": '" + word + "'. ";
    const bool number = !word.empty() && word.find_first_not_of("0123456789") == std::string::npos;
    if (number) out += "Sayı olarak okundu; rengi # ile yazın. ";
    out += fill ? "#RRGGBB (örnek #3366CC), bir renk adı, yok ya da katman yazın."
                : "#RRGGBB (örnek #C0392B), bir renk adı ya da katman yazın.";
    return out + " Renk adları: " + named_list() + ".";
}

/// How an answer is written into the journal: the one spelling a replay reads,
/// whatever was typed.
std::string written(const Wanted& w)
{
    switch (w.what) {
    case Wanted::What::Layer: return "katman";
    case Wanted::What::None: return "yok";
    case Wanted::What::Colour: return colour_hex(w.rgba);
    case Wanted::What::Keep: break;
    }
    return {};
}

/// How a colour is said back: its name too when it has one.
std::string spoken(std::uint32_t rgba)
{
    const std::string_view word = colour_word(rgba);
    return word.empty() ? colour_hex(rgba) : std::string(word) + " (" + colour_hex(rgba) + ")";
}

/// True for a symbol layer that paints the INSIDE of a face — a solid, a
/// hatch, a glyph grid, a picture — and false for one that only follows the
/// line.
bool fills(const core::SymbolLayer& l)
{
    return core::draws_fill(l.type);
}

/// True for a symbol layer that paints the line or a glyph on it.
bool strokes(const core::SymbolLayer& l)
{
    return core::draws_stroke(l.type) || core::draws_marker(l.type);
}

/// Gives the stroke of `sym` what `w` asks for. `layer` is what the object's
/// layer draws, for `katman`.
///
/// A LOCKED symbol layer keeps its own colour — the black boundary MPYY prints
/// black whatever the fill becomes — exactly as the style designer's symbol
/// colour treats it. A symbol with no line at all (a lekesi that is only a
/// fill) GAINS one: asked for a red boundary, it draws a red boundary.
void restroke(core::Symbol& sym, const Wanted& w, const core::Symbol& layer)
{
    if (w.what == Wanted::What::Keep || w.what == Wanted::What::None) return;

    const core::Appearance& theirs = layer.primary();
    bool any                       = false;
    for (core::SymbolLayer& l : sym.layers) {
        if (!strokes(l)) continue;
        any = true;
        if (l.colour_locked) continue;
        if (w.what == Wanted::What::Layer) {
            l.look.rgba       = theirs.rgba;
            l.look.src_colour = theirs.src_colour;
        } else {
            l.look.rgba       = w.rgba;
            l.look.src_colour = core::Source::Explicit;
        }
    }
    if (!any && w.what == Wanted::What::Colour) {
        core::SymbolLayer line;
        line.look            = sym.primary();
        line.look.rgba       = w.rgba;
        line.look.src_colour = core::Source::Explicit;
        sym.layers.push_back(line); // on top: a boundary is drawn over its fill
    }
}

/// Gives the fill of `sym` what `w` asks for; `layer` as above.
///
/// THE INSIDE IS PAINTED ONLY BY A FILL LAYER. The scene draws a symbol layer
/// by its type and reads no cascade (model.md R14), so the fill colour of a
/// plain one-layer appearance — the line every parcel is drawn with — is never
/// painted. A colour for an object with no fill layer therefore ADDS one, under
/// the line, which is the fill-and-outline every drawing program's filled shape
/// is. "No fill" takes the fill layers away again, so the object can go back to
/// being the plain line it was.
///
/// The colour is written into the line layers' `fill_rgba` as well: that is the
/// summary `StyleTable::at` reports for the whole stack (`Symbol::primary`), and
/// what a file format that knows one appearance per object is told.
void refill(core::Symbol& sym, const Wanted& w, const core::Symbol& layer)
{
    if (w.what == Wanted::What::Keep) return;

    // Unlocked fill layers go (every case but a colour keeps what it replaces
    // out); a locked one is the regulation's and stays.
    const auto drop_fills = [&sym] {
        core::Appearance last = sym.primary();
        std::erase_if(sym.layers, [&last](const core::SymbolLayer& l) {
            if (!fills(l) || l.colour_locked) return false;
            last = l.look;
            return true;
        });
        // A symbol that was nothing but fill keeps a line to be seen by.
        if (sym.layers.empty()) {
            core::SymbolLayer line;
            line.look           = last;
            line.look.fill_rgba = 0;
            line.look.hatch     = 0;
            sym.layers.push_back(line);
        }
    };
    const auto summarise = [&sym](std::uint32_t rgba, std::uint16_t hatch, core::Source src) {
        for (core::SymbolLayer& l : sym.layers)
            if (!fills(l) && !l.colour_locked) {
                l.look.fill_rgba = rgba;
                l.look.hatch     = hatch;
                l.look.src_fill  = src;
            }
    };

    switch (w.what) {
    case Wanted::What::Keep: return;
    case Wanted::What::None:
        drop_fills();
        summarise(0, 0, core::Source::Explicit);
        return;
    case Wanted::What::Layer: {
        // The layer's own fill layers, in their order, under everything else.
        drop_fills();
        std::vector<core::SymbolLayer> theirs;
        for (const core::SymbolLayer& l : layer.layers)
            if (fills(l)) theirs.push_back(l);
        sym.layers.insert(sym.layers.begin(), theirs.begin(), theirs.end());
        const core::Appearance& summary = layer.primary();
        summarise(summary.fill_rgba, summary.hatch, summary.src_fill);
        return;
    }
    case Wanted::What::Colour: {
        bool any = false;
        for (core::SymbolLayer& l : sym.layers) {
            if (!fills(l)) continue;
            any = true;
            if (l.colour_locked) continue;
            l.look.fill_rgba = w.rgba;
            l.look.src_fill  = core::Source::Explicit;
        }
        if (!any) {
            core::SymbolLayer inside;
            inside.type           = core::SymbolLayerType::SimpleFill;
            inside.look           = sym.primary();
            inside.look.fill_rgba = w.rgba;
            inside.look.hatch     = 0;
            inside.look.src_fill  = core::Source::Explicit;
            sym.layers.insert(sym.layers.begin(), inside); // under the line
        }
        summarise(w.rgba, sym.primary().hatch, core::Source::Explicit);
        return;
    }
    }
}

std::string said(const Wanted& w, bool fill)
{
    switch (w.what) {
    case Wanted::What::Keep: return {};
    case Wanted::What::Layer: return fill ? "dolgu katmanın" : "çizgi katmanın";
    case Wanted::What::None: return "dolgu yok";
    case Wanted::What::Colour: return (fill ? "dolgu " : "çizgi ") + spoken(w.rgba);
    }
    return {};
}

Task<void> run(Context& ctx)
{
    // THE ORDER EVERY MODIFY COMMAND KEEPS (`want_objects`): named, highlighted,
    // or asked for.
    std::vector<std::int64_t> ids;
    if (!co_await want_objects(ctx, "nesneler", "Rengi değişecek nesneleri seçin, sonra Enter", ids,
                               0, "RENK nesneler=1 renk=#C0392B"))
        co_return;

    auto stroke = read_colour(ctx.argument("renk").as_text(), false);
    auto fill   = read_colour(ctx.argument("dolgu").as_text(), true);
    if (!stroke) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   refusal("renk", ctx.argument("renk").as_text(), false));
        co_return;
    }
    if (!fill) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   refusal("dolgu", ctx.argument("dolgu").as_text(), true));
        co_return;
    }

    // NOTHING NAMED IS A QUESTION: the stroke colour, the one a hand pressing
    // RENK most often means. A script that named nothing is told what it can
    // write, because it cannot answer.
    if (stroke->what == Wanted::What::Keep && fill->what == Wanted::What::Keep) {
        // THE WORDS ARE OFFERED, so a hand can answer with a click: the shell
        // shows `Prompt::choices` beside the command line.
        std::vector<std::string> offered{"katman"};
        for (const NamedColour& n : named_colours())
            offered.emplace_back(n.word);
        auto typed =
            co_await ctx.text("renk", "Çizgi rengi (#RRGGBB, bir renk adı ya da katman)", offered);
        if (!typed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Hangi renk: renk=#RRGGBB ya da bir renk adı, dolgu=#RRGGBB|yok, ya da "
                       "katman. Örnek: RENK nesneler=1 renk=#C0392B");
            co_return;
        }
        stroke = read_colour(*typed, false);
        if (!stroke || stroke->what == Wanted::What::Keep) {
            ctx.refuse(core::ErrorCode::InvalidArgument, refusal("renk", *typed, false));
            co_return;
        }
    }

    const core::Document& doc = ctx.document();
    std::size_t changed       = 0;
    std::size_t same          = 0;
    for (const std::int64_t raw : ids) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        if (const auto st = doc.editable(e); !st) {
            ctx.refuse(st.error());
            co_return;
        }

        // WHAT IT IS DRAWN WITH, not the bare sentinel: an object that inherits
        // draws its layer's stack (the fill pattern and the glyphs of an MPYY
        // gösterim included), and that is what gets recoloured.
        const core::StyleId before = doc.entities().style[e];
        const core::Symbol layer   = core::layer_symbol(doc, doc.entities().layer[e]);
        core::Symbol sym           = core::drawn_symbol(doc, e);
        restroke(sym, *stroke, layer);
        refill(sym, *fill, layer);

        // DRAWN AS ITS LAYER DRAWS IT, IT INHERITS AGAIN. `RENK renk=katman` on
        // an object whose only override was its colour hands it back to the
        // sentinel, so it follows the layer from then on instead of holding a
        // copy of what the layer looked like today.
        const core::StyleId after =
            sym == layer ? core::kByLayerStyle : ctx.transaction().intern_symbol(sym);
        if (after == before) {
            ++same;
            continue;
        }
        if (const auto st = ctx.transaction().set_entity_style(e, after); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        ++changed;
    }

    ctx.record("nesneler", Value::ids(ids));
    if (stroke->what != Wanted::What::Keep) ctx.record("renk", Value::text(written(*stroke)));
    if (fill->what != Wanted::What::Keep) ctx.record("dolgu", Value::text(written(*fill)));

    std::string what = said(*stroke, false);
    if (const std::string f = said(*fill, true); !f.empty()) what += (what.empty() ? "" : ", ") + f;
    std::string line = std::to_string(changed) + " nesnenin rengi değişti (" + what + ").";
    if (same > 0) line += " " + std::to_string(same) + " nesne zaten böyleydi ya da rengi kilitli.";
    ctx.echo(line);
}

} // namespace

KENTOS_COMMAND(colour)
{
    return CommandSpec{
        .id       = "core.colour",
        .names    = {"RENK", "COLOR", "COLOUR", "RNK"},
        .title    = "Renk",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Rengi değişecek nesneler; verilmezse etkin seçim, o da boşsa sorulur"}
                    .en("objects"),
                Param::text("renk", Arity::optional(),
                            "Çizgi rengi: #RRGGBB (ya da saydamlıkla #AARRGGBB) veya katman")
                    .en("color"),
                Param::text("dolgu", Arity::optional(),
                            "Dolgu rengi: #RRGGBB, yok (dolgusuz) ya da katman")
                    .en("fill"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine "
                   "döndürür.",
        .run = &run,
    };
}

} // namespace kentos::command
