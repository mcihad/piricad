// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: settings.
//
// .claude/model.md R38–R42. A setting is declared ONCE as a SettingSpec; the
// persistence, the validation, the user interface and the documentation are all
// generated from that declaration. There is no second list of settings anywhere,
// exactly as there is no second list of commands (CLAUDE.md 5.10).
//
// The shape deliberately mirrors piricad/command/spec.hpp — stable namespaced id,
// Turkish names with ASCII-folded and English aliases, a declared type, a declared
// range, a one-line Turkish summary, and a PIRICAD_SETTING(sym) factory macro fed
// by one X-macro list. One idiom in this codebase, not two (model.md R25).
//
// Three scopes, and the boundary is the whole point (R39):
//
//   App      per user and machine. Not undoable, not hashed, not in the document.
//   Project  travels with the document. Undoable, journalled, part of content_hash().
//   Session  transient. Not persisted, not hashed, not undoable.
//
// The test for which scope a setting belongs in is R40, applied literally:
// ANYTHING THAT CAN CHANGE A BYTE OF AN EXPORTED LEGAL DOCUMENT IS Scope::Project.
// Coordinate display precision looks like a preference and is not: it decides how
// many decimals reach the koordinat özet cetveli that an engineer signs.
#pragma once

#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::core {

// ---------------------------------------------------------------- scope -----

enum class SettingScope : std::uint8_t { App, Project, Session };

/// Stable machine name. Reaches files, journals and schemas, so it never changes.
const char* setting_scope_name(SettingScope s);

/// Turkish label for the text a user reads (piricad.md §13).
const char* setting_scope_label(SettingScope s);

/// Which scopes one store accepts. The project store and the application store are
/// the same type with a different mask, so there is one code path and not two
/// implementations that drift apart.
enum class SettingScopeMask : std::uint8_t {
    None    = 0,
    App     = 0x1,
    Project = 0x2,
    Session = 0x4,
    All     = 0x7,
};

constexpr SettingScopeMask operator|(SettingScopeMask a, SettingScopeMask b) noexcept
{
    return static_cast<SettingScopeMask>(static_cast<std::uint8_t>(a) |
                                         static_cast<std::uint8_t>(b));
}

constexpr SettingScopeMask mask_of(SettingScope s) noexcept
{
    return s == SettingScope::App       ? SettingScopeMask::App
           : s == SettingScope::Project ? SettingScopeMask::Project
                                        : SettingScopeMask::Session;
}

constexpr bool accepts(SettingScopeMask m, SettingScope s) noexcept
{
    return (static_cast<std::uint8_t>(m) & static_cast<std::uint8_t>(mask_of(s))) != 0;
}

// ----------------------------------------------------------------- type -----

/// There is deliberately NO Double.
///
/// model.md R21 and P8: no stored field is floating point, and a setting IS a
/// stored field the moment its scope is Project. A setting that wants "0.5" is an
/// integer in a stated unit — a length is `Mm`, an angle is micro-degrees, a ratio
/// is per-mille — because 0.1 is not representable in binary floating point and a
/// document whose linetype scale round-trips to 0.09999999999999999 on one machine
/// and 0.1 on another cannot produce bit-identical golden output (§7.3).
enum class SettingType : std::uint8_t {
    Bool,   ///< true / false
    Int,    ///< std::int64_t in the unit named by SettingSpec::unit
    Length, ///< Mm, fixed-point millimetres
    Text,   ///< short UTF-8, fixed capacity, no allocation
    Enum,   ///< u16 index into SettingSpec::values
};

const char* setting_type_name(SettingType t);
const char* setting_type_label(SettingType t);

/// Text settings are short by construction — a CRS id, a language tag, a package
/// version. A fixed buffer keeps SettingValue trivially copyable and byte-hashable,
/// and keeps the eventual file record fixed width (model.md "New stored field").
inline constexpr std::size_t kSettingTextCapacity = 48;

/// A small POD variant. Two members rather than a union: the scalar carries Bool,
/// Int, Length and Enum, the buffer carries Text, and byte-wise equality is then
/// exact and platform-independent.
class SettingValue
{
public:
    SettingValue() = default;

    static SettingValue boolean(bool v);
    static SettingValue integer(std::int64_t v);
    static SettingValue length(Mm v);
    static SettingValue enumerated(std::uint16_t index);

    /// Fails rather than truncating: a silently shortened CRS id is a wrong CRS,
    /// and a wrong CRS is the classic field blunder (model.md R36).
    static Result<SettingValue> text(std::string_view v);

    SettingType type() const noexcept { return type_; }

    bool as_bool() const noexcept { return num_ != 0; }

    std::int64_t as_int() const noexcept { return num_; }

    Mm as_length() const noexcept { return num_; }

    std::uint16_t as_enum() const noexcept { return static_cast<std::uint16_t>(num_); }

    std::string_view as_text() const noexcept;

    /// Bool, Int, Length and Enum as one integer, for range checks and clamping.
    /// Text has no scalar and reports 0; the range of a Text setting is its capacity.
    std::int64_t scalar() const noexcept { return type_ == SettingType::Text ? 0 : num_; }

    void set_scalar(std::int64_t v) noexcept;

    std::uint64_t fold(std::uint64_t seed) const;

    friend bool operator==(const SettingValue&, const SettingValue&) = default;

private:
    SettingType type_{SettingType::Bool};
    std::int64_t num_{0};
    std::array<char, kSettingTextCapacity> text_{};
};

// ---------------------------------------------------------------- range -----

struct SettingRange
{
    std::int64_t min{std::numeric_limits<std::int64_t>::min()};
    std::int64_t max{std::numeric_limits<std::int64_t>::max()};

    static constexpr SettingRange unbounded() noexcept { return {}; }

    static constexpr SettingRange between(std::int64_t lo, std::int64_t hi) noexcept
    {
        return {lo, hi};
    }

    constexpr bool contains(std::int64_t v) const noexcept { return v >= min && v <= max; }

    constexpr std::int64_t clamp(std::int64_t v) const noexcept
    {
        return v < min ? min : (v > max ? max : v);
    }

    constexpr bool bounded() const noexcept
    {
        return min != std::numeric_limits<std::int64_t>::min() ||
               max != std::numeric_limits<std::int64_t>::max();
    }
};

// ----------------------------------------------------------------- spec -----

struct SettingSpec
{
    std::string id;                 ///< stable, lowercase, namespaced: "core.crs.id"
    std::vector<std::string> names; ///< Turkish primary, ASCII-folded, English, abbreviations
    SettingType type{SettingType::Bool};
    SettingScope scope{SettingScope::App};
    SettingValue fallback{};         ///< the declared default; "default" is a keyword
    SettingRange range{};            ///< scalar range; ignored for Text
    std::vector<std::string> values; ///< Enum value names, index order
    std::string unit;                ///< "mm", "µderece", "‰", "sn" — empty when unitless
    std::string summary;             ///< one line, Turkish, and it justifies the scope (R40)
};

/// Declares the factory for one built-in setting. The body returns its SettingSpec.
/// Registration happens in exactly one place, from one list, exactly as commands
/// are registered in commands/builtin.cpp (model.md R25).
#define PIRICAD_SETTING(sym) ::piricad::core::SettingSpec piricad_setting_##sym()

inline constexpr std::uint32_t kNoSetting = 0xFFFFFFFFu;

/// The declared settings. Immutable once built: it is a description of the product,
/// not state, so handing it out by reference costs nothing and races with nothing.
class SettingCatalog
{
public:
    /// Rejects an empty id, a duplicate id or alias, an Enum without values, a
    /// default whose type disagrees with the declaration, and a default outside the
    /// declared range — a spec that cannot be satisfied is a defect, not a warning.
    Status add(SettingSpec s);

    std::size_t size() const noexcept { return specs_.size(); }

    const std::vector<SettingSpec>& all() const noexcept { return specs_; }

    const SettingSpec& at(std::uint32_t index) const { return specs_[index]; }

    /// Resolves an id or any declared alias, Turkish-folded (CLAUDE.md 5.6).
    std::uint32_t find(std::string_view id_or_name) const;

    /// Best single suggestion for a name that did not resolve, or empty.
    std::string suggest(std::string_view typed) const;

private:
    // Three parallel alias arrays rather than a hash map: the catalogue is O(tens),
    // a linear scan beats a hash lookup at this size, and iteration order is fixed
    // instead of depending on the hash function (core.md P11).
    std::vector<SettingSpec> specs_;
    std::vector<std::string> alias_;   ///< as declared, for suggestions
    std::vector<std::string> folded_;  ///< turkish_upper(alias_), for lookup
    std::vector<std::uint32_t> owner_; ///< spec index of each alias
};

/// The built-in catalogue. Built once, never mutated, so it is not the
/// "process-wide mutable registry" core.md P8 bans — there is nothing to mutate.
const SettingCatalog& builtin_settings();

// ---------------------------------------------------- value conversion ------

/// Canonical text form: an Enum renders as its value name, a Bool as evet/hayır,
/// everything else as its integer. Used by the AYAR/TERCİH transcript and by the
/// generated documentation, so both say exactly the same thing.
std::string format_setting(const SettingSpec& spec, const SettingValue& v);

/// Text to SettingValue in the declared type. This is a scalar conversion, not a
/// grammar: the one parser (piricad/command/parser.hpp) still owns the command line
/// (CLAUDE.md 5.11). Accepts hexadecimal for masks and colours, and an all-zero
/// fraction ("3.000000") because the command line's number token renders that way.
Result<SettingValue> parse_setting(const SettingSpec& spec, std::string_view text);

// ---------------------------------------------------------------- store -----

/// A value that was accepted but not as given (R42). Recorded, never thrown away:
/// a file written by another version must open, and the user must be told what
/// changed under them.
struct SettingWarning
{
    std::string id;
    std::string message;
};

/// Everything an undo step needs to reverse one write, and everything a journal
/// line needs to replay it. `before` is the effective value that was in force;
/// `was_explicit` distinguishes "it was set to that" from "that was the default",
/// which is the difference between restoring a value and removing one.
struct SettingChange
{
    std::string id;
    SettingValue before{};
    SettingValue after{};
    bool was_explicit{false};
    bool clamped{false};
};

/// The value store. The document owns one masked to Project; the application owns
/// one masked to App; the session owns one masked to Session. Same type, same code
/// path, different mask.
class Settings
{
public:
    explicit Settings(const SettingCatalog& catalogue = builtin_settings(),
                      SettingScopeMask accepted       = SettingScopeMask::All);

    const SettingCatalog& catalogue() const noexcept { return *cat_; }

    SettingScopeMask accepted() const noexcept { return accepted_; }

    // ---- read ----

    /// Effective value: the explicit one when present, the declared default
    /// otherwise. An unknown id yields an empty value; use lookup() to be told.
    SettingValue get(std::string_view id) const;

    Result<SettingValue> lookup(std::string_view id) const;

    /// True when a value was written here — the answer to "why is this like this?".
    bool is_explicit(std::string_view id) const;

    /// True when the effective value equals the declared default. Distinct from
    /// !is_explicit(): a user may pin a value that happens to be the default.
    bool is_default(std::string_view id) const;

    std::size_t explicit_count() const noexcept { return values_.size(); }

    /// Ids that carry an explicit value, sorted by id so every caller — the UI, the
    /// writer, a diff — sees the same order (core.md P11).
    std::vector<std::string> explicit_ids() const;

    // ---- write ----

    /// Validates, clamps and stores. Out of range is clamped with a recorded
    /// warning, never silently accepted and never a hard failure that would make a
    /// file unopenable (R42). Returns the change record an undo step reverses.
    Result<SettingChange> set(std::string_view id, SettingValue v);

    /// Exact inverse of one set(): restores the previous value, or removes the
    /// entry when there was none. This is what a Project-scope Op applies.
    Status revert(const SettingChange& change);

    /// Drops the explicit value so the declared default is in force again.
    Status reset(std::string_view id);

    void clear();

    // ---- hashing ----

    /// Folds the PROJECT-scope entries and nothing else (R39, R43). App and Session
    /// values are not document content: a document must hash the same on a machine
    /// with a dark theme as on one with a light theme.
    ///
    /// Only explicit entries are folded, in id order. The fingerprint is of what the
    /// document holds, not of what the running build's defaults happen to be —
    /// otherwise shipping a new default would rewrite every stored golden hash.
    std::uint64_t fold(std::uint64_t seed) const;

    // ---- warnings ----

    const std::vector<SettingWarning>& warnings() const noexcept { return warnings_; }

    void clear_warnings() { warnings_.clear(); }

private:
    const SettingValue* find_explicit(std::uint32_t index) const;

    const SettingCatalog* cat_;
    SettingScopeMask accepted_;
    std::vector<std::pair<std::uint32_t, SettingValue>> values_; ///< sorted by index
    std::vector<SettingWarning> warnings_;
};

} // namespace piricad::core
