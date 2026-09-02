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
    Draw,   ///< Çizim
    Modify, ///< Düzenleme
    View,   ///< Görünüm
    Layer,  ///< Katman
    File,   ///< Dosya
    Query,  ///< Sorgu
    Script, ///< Betik
    System, ///< Sistem
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
    std::string name; ///< Turkish, matching the script/CLI keyword
    ParamKind kind{ParamKind::Text};
    Arity arity{Arity::exactly(1)};
    std::string help; ///< one line, shown by the CLI and fed to the AI schema

    static Param points(std::string name, Arity a, std::string help = {});
    static Param point(std::string name, std::string help = {});
    static Param number(std::string name, Arity a, std::string help = {});
    static Param integer(std::string name, Arity a, std::string help = {});
    static Param text(std::string name, Arity a, std::string help = {});
    static Param boolean(std::string name, Arity a, std::string help = {});
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
struct CommandSpec
{
    std::string id;                      ///< stable, namespaced: "core.line"
    std::vector<std::string> names;      ///< Turkish primary, English equivalent, abbreviations
    Category category{Category::System}; ///< where it appears in menus and docs
    std::vector<Param> params;           ///< validated by the bus before `run`
    UndoPolicy undo{UndoPolicy::SingleTransaction}; ///< how it lands on the stack
    Flags flags{Flags::None};                       ///< which clients may reach it, and how
    std::string summary;                            ///< one line, Turkish, user-facing
    CommandFn run{nullptr};                         ///< the body; null means the spec is incomplete

    /// Machine-readable form. The AI tool schema and the generated documentation
    /// both come from here — there is no second description of a command.
    core::Json to_schema() const;
};

/// Declares the factory for one built-in command. The body returns its CommandSpec.
/// Registration happens in exactly one place (commands/builtin.cpp) from one list,
/// which keeps the static-initialisation order defined and survives static linking.
#define KENTOS_COMMAND(sym) ::kentos::command::CommandSpec kentos_command_##sym()

} // namespace kentos::command
