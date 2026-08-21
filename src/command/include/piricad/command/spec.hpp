// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the single-source-of-truth command definition.
//
// piricad.md §2.3: a command is defined in exactly ONE place. The command-line
// help, the script binding, the AI tool schema and the documentation are all
// GENERATED from this definition. A second, hand-maintained list is a defect.
#pragma once

#include "piricad/command/task.hpp"
#include "piricad/command/value.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace piricad::command {

class Context;

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

const char* category_name(Category c);

enum class ParamKind : std::uint8_t {
    Point,
    PointList,
    Number,
    Integer,
    Text,
    Bool,
    Selection,
};

/// Stable machine name for schemas and JSON. Not user-facing.
const char* param_kind_name(ParamKind k);

/// Turkish label for messages the user reads (piricad.md §3, §13).
const char* param_kind_label(ParamKind k);

struct Arity
{
    std::uint32_t min{1};
    std::uint32_t max{1};

    static constexpr Arity exactly(std::uint32_t n) { return {n, n}; }

    static constexpr Arity at_least(std::uint32_t n) { return {n, 0xFFFFFFFFu}; }

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
/// (piricad.md §2.3, §5.1).
enum class Flags : std::uint32_t {
    None         = 0,
    Interactive  = 1u << 0, ///< can ask the user for input mid-run
    Scriptable   = 1u << 1, ///< reachable from the script engine
    AiAccessible = 1u << 2, ///< appears in the generated AI tool schema
    Transparent  = 1u << 3, ///< may interrupt another running command (ZOOM, PAN)
    ReadOnly     = 1u << 4, ///< mutates nothing; skips the transaction path
};

constexpr Flags operator|(Flags a, Flags b)
{
    return static_cast<Flags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr bool has_flag(Flags v, Flags f)
{
    return (static_cast<std::uint32_t>(v) & static_cast<std::uint32_t>(f)) != 0;
}

/// piricad.md §2.5 — one command is one undo step by default.
enum class UndoPolicy : std::uint8_t {
    SingleTransaction, ///< default
    None,              ///< read-only or view-only commands
    Custom,            ///< the command manages its own transaction boundaries
};

using CommandFn = Task<void> (*)(Context&);

struct CommandSpec
{
    std::string id;                 ///< stable, namespaced: "core.line"
    std::vector<std::string> names; ///< Turkish primary, English equivalent, abbreviations
    Category category{Category::System};
    std::vector<Param> params;
    UndoPolicy undo{UndoPolicy::SingleTransaction};
    Flags flags{Flags::None};
    std::string summary; ///< one line, Turkish, user-facing
    CommandFn run{nullptr};

    /// Machine-readable form. The AI tool schema and the generated documentation
    /// both come from here — there is no second description of a command.
    core::Json to_schema() const;
};

/// Declares the factory for one built-in command. The body returns its CommandSpec.
/// Registration happens in exactly one place (commands/builtin.cpp) from one list,
/// which keeps the static-initialisation order defined and survives static linking.
#define PIRICAD_COMMAND(sym) ::piricad::command::CommandSpec piricad_command_##sym()

} // namespace piricad::command
