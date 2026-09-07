// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: attribute columns.
//
// .claude/model.md R27–R29. The schema belongs to the COLLECTION, not to the
// object: this is OGRFeatureDefn and Blender's CustomData, and it is never a
// per-entity property bag, a variant map or an XDATA blob (P12). A property bag
// costs a heap allocation and a hash lookup per entity per field; five million
// parcels with an ada/parsel/detay triple is fifteen million allocations for
// data that is three dense arrays.
//
// The schema is DECLARED FROM /data, not in C++ (R27, CLAUDE.md 5.13). Every
// type here is runtime-constructible from parsed catalogue text; nothing in this
// header knows a BÖHHBÜY code, a TUCBS theme or a plan legend row, and nothing
// under /src ever may.
//
// R28 is the load-bearing rule: one attribute write is one generic operation —
// column, row, previous value — so a new attribute adds ZERO Op variants.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// Dense column index inside one AttrTable. Not persisted: the stable identity
/// of an attribute is its `AttrSpec::id` string, which comes from /data.
using AttrId = std::uint16_t;

inline constexpr AttrId kNoAttr = 0xFFFFu;

/// The closed set of column types. Adding one is an amendment, not a patch:
/// every type here must have a fixed-width, floating-point-free storage form
/// (R21, P8).
enum class AttrType : std::uint8_t {
    Int64 = 0, ///< a plain integer: ada no, parsel no, a count
    Length,    ///< a length, stored as Mm. Not spelled `Mm`: an enumerator by
               ///< that name shadows the units.hpp alias and -Wshadow is on.
    Bool,
    Text,    ///< free text, dictionary-encoded
    CodeRef, ///< a code drawn from a named /data catalogue
};

/// Stable machine name for schemas, files and messages.
const char* attr_type_name(AttrType t) noexcept;

/// The type a user's word names, or nothing when it names none.
///
/// THE INVERSE OF `attr_type_name`, AND THE ONLY ONE. `SÜTUN` parsed these words
/// inline; the moment a second command had to read the same word — a symbol
/// declaring the column it needs — the choice was to copy the chain or to share
/// it, and a copy is how `metin` comes to mean one thing in one command and
/// another somewhere else. Turkish-folded, so `TAM_SAYI` and `tam_sayı` are the
/// same word (CLAUDE.md 5.6).
std::optional<AttrType> attr_type_from_name(std::string_view word);

/// One column's declaration, as read from /data. All-runtime by construction:
/// there is no constexpr table of specs anywhere, because that table would be
/// the regulation baked into the binary.
struct AttrSpec
{
    std::string id;                 ///< stable, lowercase, from the catalogue package
    std::string name_tr;            ///< what the user sees in the attribute panel
    std::string summary_tr;         ///< one line, Turkish
    AttrType type{AttrType::Int64}; ///< what the cells of this column hold

    /// Whether every row must carry a value. Checked by `validate_required`, not
    /// on write: a parcel is often drawn before its ada number is known, and
    /// refusing the geometry until the paperwork arrives would be the wrong order.
    bool required{false};

    /// Catalogue this column's codes are drawn from. Meaningful only when
    /// `type == CodeRef`, and then it must name a catalogue the document holds
    /// (R34: three cardinalities, one authority).
    std::string catalog;

    friend bool operator==(const AttrSpec&, const AttrSpec&) = default;
};

/// One cell, in the ONE shape a generic attribute Op carries (R28). An Op needs
/// a column, a row and this — never a type-specific payload, or every new
/// attribute would add an Op variant and a journal case.
struct AttrValue
{
    AttrType type{AttrType::Int64}; ///< must match the column's declared type
    bool present{false};            ///< false = the cell is empty, OGR's IsFieldSet
    std::int64_t number{0};         ///< Int64, Length, Bool
    std::string text;               ///< Text, CodeRef

    /// Exact equality, `present` included: an empty cell and a cell holding zero
    /// are different facts about a parcel, and an undo record has to tell them
    /// apart to restore the right one.
    friend bool operator==(const AttrValue&, const AttrValue&) = default;
};

/// Constructors, one per type, plus the empty case. Free functions so the TYPE is
/// written at the call site: an `Int64` and a `Length` are both integers and
/// storing one as the other silently changes what the number means.
AttrValue attr_absent(AttrType t);
AttrValue attr_int64(std::int64_t v);
AttrValue attr_mm(Mm v);
AttrValue attr_bool(bool v);
AttrValue attr_text(std::string v);

/// Which mark separates the whole part of a number from its fraction.
///
/// TWO CONVENTIONS, ONE FORMATTER. A plan sheet prints `1,5` because that is
/// what Turkish typography does; an attribute table prints `1.5` because the
/// filter grammar, the sort and every export read a point. They are the same
/// number and the same millimetre-to-metre arithmetic, so they are one function
/// with a parameter rather than two functions that will drift.
enum class DecimalMark : std::uint8_t {
    Comma, ///< paper: `1,5`
    Point, ///< data: `1.5`
};

/// One cell as text.
///
/// A `Length` is stored in millimetres and printed in metres, because that is
/// the unit both a plan sheet and an attribute table write. An absent cell
/// prints as nothing rather than as a zero: an unmeasured frontage and a zero
/// frontage are different facts about a parcel.
std::string attr_display(const AttrValue& value, DecimalMark mark = DecimalMark::Comma);
AttrValue attr_code(std::string code);

/// The part of a /data catalogue package that validating a CodeRef needs.
///
/// Built at runtime from a catalogue file by /src/io — a code list in C++ is
/// banned outright (CLAUDE.md 5.13, data.md P1), so this class is deliberately
/// empty until something loads it.
class Catalogue
{
public:
    /// An empty catalogue with no id. Exists so a document can hold a slot for
    /// one before /src/io has loaded the package.
    Catalogue() = default;

    /// A catalogue identified by its package id and version, both required: R35
    /// makes the regulatory basis of a document a fact it must carry, never a
    /// default somebody supplied later.
    Catalogue(std::string id, std::string package_version);

    /// R35: a document whose regulatory basis is unknown must not open silently,
    /// so the package version travels with the codes and is never defaulted.
    const std::string& id() const noexcept { return id_; }

    const std::string& package_version() const noexcept { return package_version_; }

    /// Ignores a duplicate. Retired codes are still loaded (data.md R5): a file
    /// referencing a code that has since been withdrawn must still validate, or
    /// the withdrawal would retroactively invalidate a signed document.
    void add_code(std::string code);

    bool contains(std::string_view code) const;

    std::size_t size() const noexcept { return codes_.size(); }

    /// Sorted, so iteration order is content order and not insertion accident.
    const std::vector<std::string>& codes() const noexcept { return codes_; }

private:
    std::string id_;
    std::string package_version_;
    std::vector<std::string> codes_; ///< sorted; binary search, no hash order (P11)
};

/// The catalogues one document validates against.
class CatalogueSet
{
public:
    /// Replaces a catalogue with the same id — reloading a package is a command
    /// (R15), and it must not leave two versions of the same catalogue in play.
    void add(Catalogue c);

    const Catalogue* find(std::string_view id) const;

    std::size_t size() const noexcept { return items_.size(); }

    const std::vector<Catalogue>& all() const noexcept { return items_; }

private:
    std::vector<Catalogue> items_; ///< sorted by id
};

/// One dense typed column, indexed by entity SLOT.
///
/// R29 / P29: **an attribute column is never read by the frame path.** The
/// renderer reads one StyleId per entity, resolved at commit time by the command
/// that changed the data (R14). If a draw call ever needs a value from here, the
/// answer is a new style class, not a lookup — the hot/cold split (core.md R7)
/// is the reason a five-million-parcel pan stays inside 16 ms.
///
/// Text and CodeRef are dictionary-encoded: an interned pool plus a u32 index
/// per row. A parcel layer has five million rows and perhaps two hundred
/// distinct mahalle names, so the pool is the difference between 200 strings and
/// 5,000,000.
class AttrColumn
{
public:
    /// Builds an empty column from its declaration. The spec is copied and then
    /// fixed: a column's type cannot change under the rows already written to it.
    explicit AttrColumn(AttrSpec spec);

    /// The declaration this column was built from.
    const AttrSpec& spec() const noexcept { return spec_; }

    /// What the cells hold. A shorthand for `spec().type`, which the read and
    /// write paths both need on every call.
    AttrType type() const noexcept { return spec_.type; }

    /// How many cells this column has. Always equal to the document's slot count:
    /// the tables grow with the geometry, so `content_hash()` cannot depend on the
    /// ORDER in which a session wrote its attributes.
    std::size_t rows() const noexcept { return rows_; }

    /// Grows with every new cell absent; shrinking drops the tail. The pool is
    /// never compacted here: an id handed out stays valid for the document's
    /// lifetime, and rebuilding it would renumber a journal's previous values.
    void resize(std::size_t rows);

    /// The one generic write (R28). Returns the value that was there, which is
    /// exactly what the undo record needs and all it needs.
    Result<AttrValue> set(std::size_t row, const AttrValue& v);

    Result<AttrValue> get(std::size_t row) const;

    bool present(std::size_t row) const noexcept;

    /// Empty when the cell is absent or the column is not text-shaped.
    std::string_view text(std::size_t row) const;

    /// Distinct interned strings. Zero for a numeric column.
    std::size_t pool_size() const noexcept { return pool_.size(); }

    /// Folds the column into a content hash. Absent cells fold distinctly from
    /// a present zero, because "no ada number recorded" and "ada number 0" are
    /// different facts about a parcel.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    Result<std::uint32_t> intern(const std::string& s);

    AttrSpec spec_;
    std::size_t rows_{0};
    std::vector<std::int64_t> numbers_;                     ///< Int64, Length, Bool
    std::vector<std::uint32_t> codes_;                      ///< index into pool_
    std::vector<std::string> pool_;                         ///< insertion-ordered
    std::unordered_map<std::string, std::uint32_t> intern_; ///< looked up, never iterated
    std::vector<std::uint64_t> present_;                    ///< validity bitmap, 64 rows/word
};

/// The columns of one collection, plus the schema they were declared from.
class AttrTable
{
public:
    /// Fails on a duplicate id, an empty id, or a CodeRef column that names no
    /// catalogue — an unvalidatable regulatory reference is worse than none.
    Result<AttrId> add(AttrSpec spec);

    AttrId find(std::string_view id) const;

    const AttrColumn* column(AttrId col) const;
    AttrColumn* column(AttrId col);

    std::size_t columns() const noexcept { return columns_.size(); }

    std::size_t rows() const noexcept { return rows_; }

    void resize(std::size_t rows);

    /// R28, addressed the way an Op addresses it: column, row, value in; the
    /// previous value out. Nothing here knows what the attribute means.
    Result<AttrValue> set(AttrId col, std::size_t row, const AttrValue& v);

    Result<AttrValue> get(AttrId col, std::size_t row) const;

    /// Every `required` column has a value on this row.
    Status validate_required(std::size_t row) const;

    /// Every CodeRef value on this row exists in its catalogue. An unknown code
    /// fails loudly with no compiled-in fallback (data.md R6).
    Status validate_codes(std::size_t row, const CatalogueSet& catalogues) const;

    /// Both of the above, in that order.
    Status validate_row(std::size_t row, const CatalogueSet& catalogues) const;

    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::vector<AttrColumn> columns_;
    std::size_t rows_{0};
};

} // namespace kentos::core
