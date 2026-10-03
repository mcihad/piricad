// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the command registry.
//
// The one and only catalogue of commands. CLI completion, script bindings, AI tool
// schemas and generated docs all read from here (piricad.md §2.3).
#pragma once

#include "piricad/command/spec.hpp"
#include "piricad/core/result.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace piricad::command {

class Registry
{
public:
    /// Adds a command. Fails on a duplicate id or a name already claimed by
    /// another command — silent shadowing is not permitted.
    ///
    /// A FAILURE IS ALSO KEPT (`refused`): four of the six modules that register
    /// commands drop this status on the floor, so a name that collided removed
    /// a command from the program with no trace but a green build. What was
    /// refused stays readable here for the application to report and for the
    /// test that assembles all six to fail on.
    core::Status add(CommandSpec spec);

    /// Every `add` that failed, in order: the command's id and why.
    const std::vector<std::string>& refused() const noexcept { return refused_; }

    const CommandSpec* by_id(std::string_view id) const;

    /// Resolves a user-typed name. Turkish-aware, case-insensitive, accepts every
    /// declared alias and abbreviation. Returns nullptr when unresolved.
    const CommandSpec* resolve(std::string_view typed) const;

    /// The commands another program calls `word` (`CommandSpec::known_as`):
    /// Netcad's `KUTU` is DİKDÖRTGEN. What an unknown word can still be told —
    /// which command the user meant. `resolve` never reads it, which is why a
    /// known name may be any word, another command's name included (`KAYDIR`).
    /// Empty when no command claims the word.
    std::vector<const CommandSpec*> known_as(std::string_view word) const;

    /// Names starting with the given prefix, for command-line completion.
    std::vector<std::string> complete(std::string_view prefix, std::size_t limit = 16) const;

    const std::vector<CommandSpec>& all() const noexcept { return specs_; }

    // THE AI TOOL CATALOGUE IS NOT HERE, and its absence is deliberate.
    // `ai_tool_schema()` used to project the registry into a JSON tool list from
    // this class, and `/src/ai` needed a different projection — a real JSON
    // Schema, handles instead of coordinate literals, MCP annotations. Two
    // projections of one registry are two answers to one question
    // (.claude/ai.md P7), so the survivor is `ai::build_catalog`, which is what
    // the server serves, what `llms.txt` is written from and what the generated
    // reference embeds. `Registry` keeps what belongs to it: the commands, the
    // names, and the fingerprint below.

    /// A fingerprint over EVERY declared field of every command.
    ///
    /// WHY A RUNNING PROGRAM NEEDS ONE. An agent caches the tool list it was
    /// served; the moment a command gains a parameter, that cache is a lie. The
    /// fingerprint is what `tools/list` carries and what tells a client its copy
    /// is stale, and it is what the freshness gate hashes so that "the smallest
    /// change to a parameter regenerates the surface" is a check rather than a
    /// hope (CLAUDE.md 6.14). Stable across runs and platforms: it hashes
    /// declared text and integers, never an address or an iteration order that
    /// could differ.
    std::uint64_t fingerprint() const;

    std::size_t size() const noexcept { return specs_.size(); }

private:
    core::Status admit(CommandSpec spec);

    std::vector<CommandSpec> specs_;
    std::unordered_map<std::string, std::size_t> by_id_;
    std::unordered_map<std::string, std::size_t> by_name_; ///< keyed on turkish_fold_key(name)
    std::vector<std::string> refused_;
};

/// WHAT A SEARCH WORD MATCHED IN ONE COMMAND, and how well (`search_match`).
struct SearchMatch
{
    static constexpr int kNone = -1; ///< not an answer at all

    /// 0 is the best answer. In order: the word IS one of the command's names;
    /// one of its known names; a name begins with it; a known name begins with
    /// it; a name or the id contains it; a known name contains it; its label or
    /// its summary does.
    int tier{kNone};

    /// The known name that matched, when that is what matched: the row says it,
    /// because the command's own name is not the word the user typed.
    const KnownName* known{nullptr};
};

/// How well `spec` answers the search `word` — the order the command search
/// (`Ctrl+K`) lists its answers in. Turkish-folded like every name (CLAUDE.md
/// 5.6), so `kaydir` is `KAYDIR`. An empty word matches nothing.
///
/// A NAME BEFORE A KNOWN NAME: typed at the prompt, `KAYDIR` pans the view, so
/// the search has to agree that KAYDIR is first; TAŞI, which Netcad calls
/// `Kaydır`, comes right under it and says so.
SearchMatch search_match(const CommandSpec& spec, std::string_view word);

/// THE SEARCH A PERSON TYPES A SENTENCE INTO (TODOS U-01, "doğal dil araması").
///
/// `search_match` answers a WORD, as the prompt does: the whole text has to sit inside
/// one name. A person who has not learned the names types what they want instead —
/// `çizgiyi paralel kaydır`, `alanı hesapla`, `köşeyi yuvarla` — and the whole sentence
/// is inside nothing, so the palette answered "no match" at the one moment it is the
/// only help they have.
///
/// ORDER: the whole text first, exactly as `search_match` ranks it (tiers 0–6), so
/// nothing that a word found yesterday is found lower today. Only when the text as a
/// whole matches nothing is it taken apart:
///
///  - into words, folded as every name is (CLAUDE.md 5.6), without the particles and
///    verbs that every request carries and no command is named for (`ve`, `için`,
///    `yap`, `et`, `the`, `want`…);
///  - each word is looked for among the command's names, the names other programs know
///    it by, its title and summary, and its parameters' names — a Turkish word with its
///    case and plural endings (`çizgiyi`, `alanlarını`), a word begun (`paral`), a stem
///    whose last consonant softened (`uzunluğu` is UZUNLUK), or a name inside a longer
///    one (`alan` in ALANİFRAZ);
///  - a command answers when at least half the words found something, and the more of
///    them and the nearer to its name, the better. Tiers 7 and up; `known` is set when a
///    word was found only through another program's name, so the row can say whose.
///
/// There is no synonym table to keep in step with the registry (CLAUDE.md 5.10): what a
/// command answers to is what it declares.
SearchMatch search_query(const CommandSpec& spec, std::string_view query);

/// THE LISTS THE COMMAND SEARCH OPENS ON, before a word is typed (TODOS U-01).
///
/// A palette that opens on ninety-eight commands in category order answers "what is
/// there" and makes the reader find, every time, the four they use all day. It opens
/// on those instead: the ones they starred, then the ones they ran last, and then
/// the whole reference as before. The lists are plain ids — never a name, which a
/// translation or an alias may change — and they are USER STATE, not document state:
/// like the window layout they are kept per machine by the shell, not in the
/// drawing, and no command reads them.
///
/// `recent` is most-recent-first and holds each id once. `note_use` moves `id` to
/// the front and cuts the list to `limit`.
void note_use(std::vector<std::string>& recent, std::string_view id, std::size_t limit);

/// Adds `id` to `set` when it is not there and removes it when it is; true when it is
/// a member afterwards. Order is the order of adding.
bool toggle_member(std::vector<std::string>& set, std::string_view id);

/// Moves `id` `by` places within `set` — negative towards the front — and stops at either end:
/// the order of the starred commands is the person's to arrange (TODOS U-01), and the lists stay
/// plain ids. True when the order changed; false when `id` is not a member, `by` is 0 or it was
/// at that end already.
bool move_member(std::vector<std::string>& set, std::string_view id, int by);

/// Whether a finished command is one a person would look for again. A view change
/// (ZOOM, PAN — the `Transparent` commands) is typed or pressed a hundred times a
/// session and is never what is being looked for, and undoing is not a tool; either
/// would push the commands that are out of a short list.
bool worth_remembering(const CommandSpec& spec);

/// The process-wide registry, populated once by register_builtin_commands().
Registry& registry();

/// Registers every built-in command. Idempotent.
void register_builtin_commands(Registry& r);

} // namespace piricad::command
