// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the single-source-of-truth command definition.
//
// kentoscad.md §2.3: a command is defined in exactly ONE place. The command-line
// help, the script binding, the AI tool schema and the documentation are all
// GENERATED from this definition. A second, hand-maintained list is a defect.
#pragma once

#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/value.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::command {

/// The execution context handed to a command body; see context.hpp.
class Context;

/// Where a command belongs in the menus and in the generated reference.
///
/// The category is DATA the shell reads, not a place the command lives: menus,
/// toolbars and `docs/komutlar/referans.md` are all generated from the registry,
/// and a hand-maintained second list of commands is forbidden (CLAUDE.md 5.10).
enum class Category : std::uint8_t {
    Draw,       ///< Çizim
    Modify,     ///< Düzenleme
    View,       ///< Görünüm
    Layer,      ///< Katman
    File,       ///< Dosya
    Query,      ///< Sorgu
    Script,     ///< Betik
    System,     ///< Sistem
    Processing, ///< İşlem: an analysis tool applied to many objects at once (processing.md)
};

/// Turkish label for a category, for a menu title and the reference table.
const char* category_name(Category c);

/// What one parameter holds. A closed set, because every kind here must survive a
/// round trip through `Value` and through JSON (Article 1.4).
enum class ParamKind : std::uint8_t {
    Point,     ///< one coordinate
    PointList, ///< a run of coordinates: the vertices of a line or an area
    Number,    ///< a real quantity — a scale factor, a ratio
    Integer,   ///< a whole number in a declared unit
    Text,      ///< free text, or a keyword the command interprets
    Bool,      ///< evet / hayır
    Selection, ///< a list of persistent entity keys (model.md R44)
};

/// Stable machine name for schemas and JSON. Not user-facing.
const char* param_kind_name(ParamKind k);

/// Turkish label for messages the user reads (kentoscad.md §3, §13).
const char* param_kind_label(ParamKind k);

/// How many values one parameter takes.
///
/// Declared rather than checked in the body, because the bus validates before the
/// command runs (Article 1.3, 1.6): a command that discovers a missing argument
/// halfway through has already written to the transaction.
struct Arity
{
    /// Inclusive bounds. The defaults are "exactly one", the common case.
    std::uint32_t min{1};
    std::uint32_t max{1};

    /// Exactly `n` values, no more and no fewer.
    static constexpr Arity exactly(std::uint32_t n) { return {n, n}; }

    /// At least `n`, unbounded above. A polyline's vertices, an area's corners.
    static constexpr Arity at_least(std::uint32_t n) { return {n, 0xFFFFFFFFu}; }

    /// Zero or one. The parameter may be omitted entirely.
    static constexpr Arity optional() { return {0, 1}; }
};

struct Param
{
    /// A CONSTRUCTOR RATHER THAN AGGREGATE INITIALISATION, because the four
    /// declared facts about a parameter are its name, kind, arity and help, and
    /// everything after them is optional. Without it, the 46 places that write
    /// `Param{name, kind, arity, help}` would each have to name every field the
    /// struct ever gains — and `-Wmissing-field-initializers` is an error here.
    Param() = default;

    Param(std::string param_name, ParamKind param_kind, Arity param_arity,
          std::string param_help = {})
        : name(std::move(param_name)), kind(param_kind), arity(param_arity),
          help(std::move(param_help))
    {}

    std::string name; ///< Turkish, matching the script/CLI keyword

    /// THE SAME PARAMETER'S NAME IN ENGLISH, for the Python surface only.
    ///
    /// WHY A SECOND NAME AT ALL, when Article 2.6 is Turkish-first. Because the
    /// Python API is read and written by programmers in the language every Python
    /// library is written in, and `cad.line(noktalar=[...])` is the worst of both
    /// — a Turkish keyword nobody can type without a Turkish keyboard, inside an
    /// English function call. The command line, the manual and the journal keep
    /// the Turkish name; only `kentos.cad` uses this one (kentoscad.md §4.2).
    ///
    /// DECLARED HERE AND NOT IN A TABLE, and that distinction is the whole design.
    /// A table keyed by the Turkish word would have to answer `kenar` once, and
    /// `kenar` is a page MARGIN in `core.layout`, a measured DISTANCE in
    /// `geodesy.traverse` and an EDGE index in `islem.alan_duzenle`. Only the
    /// declaration site knows which. A table would also be a second list to keep
    /// in step, which is what CLAUDE.md 5.10 is about.
    ///
    /// Empty is a defect on a `Scriptable` or `AiAccessible` command and
    /// `scripts/ci-gate-python-api.sh` fails the build for it.
    std::string english;

    ParamKind kind{ParamKind::Text};
    Arity arity{Arity::exactly(1)};
    std::string help; ///< one line, shown by the CLI and fed to the AI schema

    /// For a `Text` parameter: the only words accepted, ASCII-folded lowercase.
    ///
    /// DECLARED RATHER THAN CHECKED IN THE BODY, for the reason `Arity` is: the
    /// bus validates before the coroutine starts (Article 1.3), the generated
    /// tool schema needs a real `enum` and the command line needs the list to
    /// print. A body that held the words privately made the caller guess from a
    /// help string, which is what an agent cannot do. `ToolParam` has carried
    /// this all along and `to_command_spec` dropped it on the floor.
    std::vector<std::string> choices;

    /// WHAT THE NUMBER IS MEASURED IN, in Turkish, for the reader and the schema.
    ///
    /// A layout's `x` and a map's `pencere` are both numbers and they are not the
    /// same kind of number: one is a position on PAPER, which a model may work
    /// out from a page size, and the other is a coordinate on the GROUND, which
    /// it may not invent at all (CLAUDE.md 5.8). `ParamKind` already keeps them
    /// apart structurally — a `Point` accepts only a handle — but a schema that
    /// says "integer 0..10000" and nothing else leaves an agent guessing what it
    /// is being asked for, and an agent that guesses puts a ground coordinate in
    /// a paper field.
    ///
    /// Empty when the number has no unit, which is most of them.
    std::string unit;

    /// THE NAME THIS PARAMETER USED TO CARRY, read but never written.
    ///
    /// A command id is stable because a journal resolves it by name six months
    /// later (Article 1.4, CLAUDE.md 0.5a) — and an ARGUMENT name is written into
    /// that same line, so renaming one breaks every journal and script already on
    /// disk. The bus accepts the retired name, moves it onto the current one
    /// before validation, and `Context::record` writes only the current one. The
    /// generated schema does not list it: an agent reading today's catalogue is
    /// told today's name, and nobody learns the old one from us.
    std::string was;

    /// For an `Integer` or `Number` parameter: the closed range, when `bounded`.
    std::int64_t low{0};
    std::int64_t high{0};
    bool bounded{false};

    static Param points(std::string name, Arity a, std::string help = {});
    static Param point(std::string name, std::string help = {});
    static Param number(std::string name, Arity a, std::string help = {});
    static Param integer(std::string name, Arity a, std::string help = {});
    static Param text(std::string name, Arity a, std::string help = {});
    static Param boolean(std::string name, Arity a, std::string help = {});

    /// A `Text` parameter that accepts one of a fixed set of words.
    static Param choice(std::string name, Arity a, std::vector<std::string> choices,
                        std::string help = {});

    /// An `Integer` parameter with a closed range the bus enforces.
    static Param integer_range(std::string name, Arity a, std::int64_t low, std::int64_t high,
                               std::string help = {});

    /// Names the unit the number is in. Chained onto a factory:
    /// `Param::integer_range("x", ...).measured_in("kâğıt mm")`.
    Param&& measured_in(std::string what) &&
    {
        unit = std::move(what);
        return std::move(*this);
    }

    /// Names what this parameter was called before it was renamed. Chained onto a
    /// factory: `Param::text("yerlesim", ...).renamed_from("pafta")`.
    Param&& renamed_from(std::string old_name) &&
    {
        was = std::move(old_name);
        return std::move(*this);
    }

    /// Names this parameter in English, for the Python keyword. Chained onto a
    /// factory: `Param::point("merkez", "Dairenin merkezi").en("center")`.
    Param&& en(std::string name_in_english) &&
    {
        english = std::move(name_in_english);
        return std::move(*this);
    }
};

/// Bit flags. Flags::AiAccessible is the ONLY switch that puts a command into the
/// AI tool catalogue — the catalogue is generated from it, never hand-written
/// (kentoscad.md §2.3, §5.1).
enum class Flags : std::uint32_t {
    None         = 0,
    Interactive  = 1u << 0, ///< can ask the user for input mid-run
    Scriptable   = 1u << 1, ///< reachable from the script engine
    AiAccessible = 1u << 2, ///< appears in the generated AI tool schema
    Transparent  = 1u << 3, ///< may interrupt another running command (ZOOM, PAN)
    ReadOnly     = 1u << 4, ///< mutates nothing; skips the transaction path
    NoEffect     = 1u << 5, ///< changes no document and no file: safe without approval
};

/// WHY `NoEffect` EXISTS BESIDE `ReadOnly`, and the difference is load-bearing.
/// `ReadOnly` says "skips the transaction path" — and `core.undo`, `core.redo`,
/// `core.save`, `core.saveas`, `core.export` and `core.preference` all carry it,
/// because none of them opens a transaction. None of them is harmless: they
/// reverse the drawing, write over a file, or change how the program behaves.
///
/// `NoEffect` is the narrower claim a non-human client needs: this command leaves
/// the document, the disk and the settings exactly as they were, so it may run
/// for an agent with no human approval (.claude/ai.md R3, R11). Everything else
/// becomes a suggestion. The two flags are set together on a command that is
/// both, and `ReadOnly` keeps its old meaning untouched.

/// WHAT A COMMAND DOES TO THE WORLD, as opposed to how it runs.
///
/// `Flags` answers "which clients may reach it and how". This answers the
/// different question a policy has to ask before letting a non-human run it:
/// what is left changed afterwards, and where. The two were one boolean —
/// `!NoEffect` — and that boolean cannot tell listing a layout apart from
/// printing one, nor `core.save` (which carries `ReadOnly` and writes over a
/// file) apart from `core.zoom`.
///
/// A command may carry several: `core.export` reads the document and writes a
/// file. `Effect::None` means the effect has not been stated yet, which is a
/// defect rather than a claim of harmlessness — `effect_of` never returns it.
enum class Effect : std::uint32_t {
    None           = 0,
    Query          = 1u << 0, ///< reads the document, the catalogues or the view
    ViewChange     = 1u << 1, ///< moves the view; no document, no disk, no setting
    DocumentEdit   = 1u << 2, ///< changes the drawing, inside a transaction
    FileRead       = 1u << 3, ///< reads a file the caller names
    FileWrite      = 1u << 4, ///< writes a file the caller names
    ExternalWrite  = 1u << 5, ///< a database, a printer, a network peer
    SettingsChange = 1u << 6, ///< changes a stored preference or profile
};

/// Combines effects: a command may do several things at once.
constexpr Effect operator|(Effect a, Effect b)
{
    return static_cast<Effect>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

/// Whether `value` carries `bit`.
constexpr bool has_effect(Effect value, Effect bit)
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(bit)) != 0U;
}

/// Stable machine name for one bit, for schemas, the inventory and the audit
/// record. Not user-facing.
const char* effect_name(Effect one);

/// THE EFFECT OF ONE WORD OF A VERB PARAMETER.
///
/// Half of this program's file and settings commands are `islem=listele|ekle|sil`
/// shaped, and those three words do three different things to the world: listing
/// reads, adding edits, removing edits. Collapsing them into the command's worst
/// case would make `ÇIKTIYERLEŞİMİ islem=listele` ask for approval, which
/// `.claude/ai.md` R3 says a read must never do.
struct VerbEffect
{
    std::string word; ///< the word, ASCII-folded lowercase, as `Param::choices` holds it
    Effect effect{Effect::Query}; ///< what that word does
};

/// Combines flags, so a spec can declare several in one expression.
constexpr Flags operator|(Flags a, Flags b)
{
    return static_cast<Flags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

/// Whether `v` carries `f`. The bus asks this rather than comparing, because a
/// command usually carries several flags at once.
constexpr bool has_flag(Flags v, Flags f)
{
    return (static_cast<std::uint32_t>(v) & static_cast<std::uint32_t>(f)) != 0;
}

/// kentoscad.md §2.5 — one command is one undo step by default.
enum class UndoPolicy : std::uint8_t {
    SingleTransaction, ///< default
    None,              ///< read-only or view-only commands
    Custom,            ///< the command manages its own transaction boundaries
};

/// A command body: a coroutine over a `Context`.
///
/// A plain function pointer, not a `std::function`: a command has no state to
/// capture — everything it needs arrives through the context — and a spec that
/// owned a closure could not be a constant description of the product.
using CommandFn = Task<void> (*)(Context&);

/// Everything the product knows about one command.
///
/// This declaration is the SINGLE source: the command line's help, the script
/// bindings, the AI tool schema, the menu and toolbar actions and the generated
/// reference are all produced from it (Article 1.7, 5.10). Nothing about a command
/// is written down twice.
/// What this command is called in `kentos.cad`.
///
/// `core.line` is `line`; another namespace keeps its own with an underscore, so
/// `geodesy.traverse` is `geodesy_traverse`; and a spec that declares
/// `CommandSpec::python` gets that instead.
///
/// IT LIVES IN `/src/command` AND NOT IN THE PYTHON HOST, because two callers
/// need it and one of them is built when Python is not: `kentos_docgen` writes
/// `docs/python/referans.md` in every configuration, or the freshness gate would
/// pass or fail depending on a build option (Article 6.14).
std::string python_callable_name(const struct CommandSpec& spec);

struct CommandSpec
{
    std::string id; ///< stable, namespaced: "core.line"

    /// THE NAME OF THIS COMMAND'S PYTHON CALLABLE, when the id does not give one.
    ///
    /// The `kentos.cad` surface is projected from `Registry`, and the function
    /// name is derived from the id: `core.line` becomes `cad.line`, and a
    /// namespace other than `core` is kept with an underscore, so
    /// `geodesy.traverse` would become `cad.geodesy_traverse`. That rule is
    /// complete and collision-free, and for 111 of the 117 commands it also
    /// produces an ENGLISH name, because their ids are English.
    ///
    /// The exceptions are the commands whose id is Turkish — `islem.uzunluk_yaz`,
    /// `islem.kose_numarala` — and no rule of grammar turns those into English.
    /// The Python API is English throughout (kentoscad.md §4.2), so they name
    /// their callable here: `label_length`, `number_vertices`.
    ///
    /// NOT A SECOND IDENTITY. The command id stays what it always was and is what
    /// the journal records; this is one more projection of it, beside the menu
    /// label and the tool name, and it is never resolved against.
    std::string python;
    std::vector<std::string> names; ///< Turkish primary, English equivalent, abbreviations
    /// The human Turkish label: `Blok Ekle`, `Yazdırma Profili`, `Eşyükselti`.
    ///
    /// A NAME IS NOT A LABEL. `names.front()` is the word typed at the prompt and
    /// it is one word by design — `ÇIKTIYERLEŞİMİ`, `ÖZNİTELİKŞEMASI` — so a menu
    /// built from it reads `Çıktıyerleşimi`, which is not Turkish. There is no
    /// rule that recovers the spaces, because the name never had any.
    ///
    /// `ToolSpec` has carried a `title` since the processing tools shipped and
    /// its menu reads `Kenar uzunluklarını yaz`; this is the same field for the
    /// same reason. Empty falls back to the Turkish title-casing of the primary
    /// name, which is right for a one-word command and the only honest guess for
    /// any other.
    std::string title;

    Category category{Category::System}; ///< where it appears in menus and docs

    std::vector<Param> params;                      ///< validated by the bus before `run`
    UndoPolicy undo{UndoPolicy::SingleTransaction}; ///< how it lands on the stack
    Flags flags{Flags::None};                       ///< which clients may reach it, and how
    std::string summary;                            ///< one line, Turkish, user-facing
    CommandFn run{nullptr};                         ///< the body; null means the spec is incomplete

    /// What running it leaves changed. `Effect::None` means "not stated", and
    /// `effect_of` fills it in from the flags rather than treating it as a claim
    /// that the command is harmless.
    Effect effect{Effect::None};

    /// The parameter that holds the verb, when one word decides the effect.
    /// Empty when the command does one thing.
    std::string effect_verb;

    /// The effect of each word of `effect_verb`. A word missing from this list
    /// falls back to `effect`, which is the command's worst case — so forgetting
    /// a word is over-cautious rather than unsafe.
    std::vector<VerbEffect> verb_effects;

    // NO `to_schema()` HERE EITHER. It emitted this program's own vocabulary —
    // `"type": "point"`, a min/max/required triple — which reads well in a
    // Turkish reference table and is not a JSON Schema. `ai::build_catalog`
    // (/src/ai) does the projection now, once, for the server, the documents and
    // the chat alike.
};

/// WHAT THIS CALL LEAVES CHANGED, for these arguments.
///
/// Never `Effect::None`. A spec that states nothing is read from its flags —
/// `NoEffect` means it touched nothing, `ReadOnly` alone means it skipped the
/// transaction path and says NOTHING about whether it wrote a file, so it falls
/// to the command's category — and a verb parameter narrows the answer to the
/// word that was actually given.
///
/// This is the one place the question is answered. A policy, an audit record and
/// the inventory all ask it here, so they cannot disagree (CLAUDE.md 5.10).
Effect effect_of(const CommandSpec& spec, const Args& args);

/// The same question with no arguments in hand: the command's WORST CASE over
/// every word its verb can take. What a catalogue tells a client before it has
/// decided what to send.
Effect effect_of(const CommandSpec& spec);

/// Declares the factory for one built-in command. The body returns its CommandSpec.
/// Registration happens in exactly one place (commands/builtin.cpp) from one list,
/// which keeps the static-initialisation order defined and survives static linking.
#define KENTOS_COMMAND(sym) ::kentos::command::CommandSpec kentos_command_##sym()

} // namespace kentos::command
