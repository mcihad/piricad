// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the declarative style catalogue and its rule evaluator.
//
// .claude/model.md R13–R19 settled where style lives; this is the piece that
// decides WHICH appearance an entity gets:
//
//   **A GIS renderer is a command that writes the style column.**
//
// R14 puts the decision at commit time, inside a Transaction, and materialises
// the answer as one interned `StyleId` per entity. This header carries the two
// halves of that decision — a catalogue of appearances, and an ordered list of
// declarative rules that pick one — and nothing else. It performs no I/O
// (core.md P9) and it never runs on the frame path (model.md P7).
//
// THE RULES ARE DATA, NOT A LANGUAGE. CLAUDE.md 5.11 allows the project exactly
// one grammar, `piricad/command/parser.hpp`, so a rule here is a fixed, closed
// set of tests over one named field:
//
//   equality        alan = "konut"
//   set membership  alan ∈ {"konut", "ticaret"}
//   integer range   1 ≤ alan ≤ 5
//   presence        alan is set
//
// There is no operator precedence, no nesting, no negation and no arithmetic.
// Conditions on one rule are conjunctive; rules are tried in file order and the
// first complete match wins. That is deterministic by construction: the same
// catalogue over the same document always produces the same `StyleId` sequence,
// which is what /tests/golden asserts (§7.3).
//
// NOTHING IN THIS FILE KNOWS A REGULATION. The catalogue rows, their identifiers,
// their colours, their paper widths and their draw order all arrive from
// /data/catalogs at runtime (CLAUDE.md 5.13, data.md R1). The vocabulary here is
// deliberately generic — "entry", "rule", "field" — because a C++ symbol naming a
// regulatory concept is the thing the rule forbids.
//
// WHY THIS IS HAND-WRITTEN (Article 2.7 / 5.16, and the answer is recorded in
// /NOTICE): there is no library to take. Every mature symbology engine — Mapnik,
// QGIS, MapLibre — is one half of a whole renderer and owns its own geometry
// store and draw pipeline, and QGIS is additionally GPLv2-only in parts. None of
// them resolves style once at commit into an interned id; that is the decision
// R14 makes, and it is the opposite of what a per-frame style engine is built
// for. OGC SLD/SE 1.1 is a wire format rather than an engine, and belongs at the
// import/export boundary, converted into this table on the way in. What is NOT
// hand-written here: the JSON is parsed by piricad/core/json.hpp, and the hatch
// clipping the renderer will need is Clipper2's job, not this file's.
#pragma once

#include "piricad/core/attribute.hpp"
#include "piricad/core/dash_store.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/layer.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/style.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace piricad::core {

/// The band of map scales a catalogue row applies to, in denominators (1:value).
///
/// A denominator GROWS as the view zooms out: 1:25000 is further out than 1:1000.
/// `low` is therefore the most zoomed-in edge and `high` the most zoomed-out one,
/// both inclusive, and zero means "unbounded on that side" — the same convention
/// `LayerTable::visible_at` uses, spelled unambiguously.
///
/// Scale-dependent symbology is baked into the StyleTable once per frame, never
/// per entity (model.md R16); this type is what the baking reads.
struct ScaleWindow
{
    /// Inclusive bounds on the 1:N denominator. ZERO MEANS UNBOUNDED on that side,
    /// not "1:0": a gösterim that applies at every scale declares neither, and a
    /// sentinel is cheaper to read from /data than an optional.
    ScaleDenominator low{0};
    ScaleDenominator high{0};

    /// Whether this window includes `denominator`.
    bool covers(ScaleDenominator denominator) const noexcept
    {
        if (low != 0 && denominator < low) return false;
        if (high != 0 && denominator > high) return false;
        return true;
    }

    friend bool operator==(const ScaleWindow&, const ScaleWindow&) = default;
};

/// One feature's classifying values, assembled for the duration of ONE
/// classification and then thrown away.
///
/// This is not the property bag model.md P12 bans: it is never stored on an
/// entity, never persisted, never hashed and never read by the frame path. The
/// entity's own attributes live in dense typed columns (R27); `from_row` binds
/// one such row, so the day a Document carries an `AttrTable` the evaluator needs
/// no change.
class FeatureView
{
public:
    /// Overwrites a field of the same name — a caller layering document defaults
    /// under per-entity values must be able to do so predictably.
    void set(std::string field, AttrValue value);

    void set_text(std::string field, std::string value);
    void set_number(std::string field, std::int64_t value);

    /// Null when the field was never set. An absent field matches no condition
    /// except none at all — there is no silent default (data.md R6).
    const AttrValue* find(std::string_view field) const;

    std::size_t size() const noexcept { return fields_.size(); }

    const std::vector<std::pair<std::string, AttrValue>>& fields() const noexcept
    {
        return fields_;
    }

    /// One row of a declared attribute table, as the evaluator sees it.
    static FeatureView from_row(const AttrTable& table, std::size_t row);

private:
    std::vector<std::pair<std::string, AttrValue>> fields_;
};

/// One test over one field. The closed set — extending it is an amendment, not a
/// patch, because every addition is a step towards the expression evaluator
/// CLAUDE.md 5.11 forbids.
struct StyleCondition
{
    /// The four tests, and there will not casually be a fifth: `Equals` and
    /// `OneOf` give a categorized renderer, `Range` gives a graduated one, and
    /// `Present` covers "tagged at all". Anything beyond this starts to be a
    /// language.
    enum class Test : std::uint8_t {
        Equals,  ///< text equality against `values.front()`
        OneOf,   ///< text equality against any of `values`
        Range,   ///< integer range, inclusive on each declared side
        Present, ///< the field carries a value at all
    };

    std::string field;               ///< the attribute or derived field to read
    Test test{Test::Equals};         ///< which comparison to make
    std::vector<std::string> values; ///< Equals uses the first; OneOf uses all

    /// Range bounds, each present only when its flag is set. Two flags rather
    /// than sentinels, because zero is a legal density, area and parcel number.
    std::int64_t low{0};
    std::int64_t high{0};
    bool has_low{false};
    bool has_high{false};

    /// Text comparison is exact and byte-wise. Case folding is deliberately NOT
    /// applied: a catalogue value and an attribute value both come from /data and
    /// must agree exactly, and folding Turkish text here would need the table
    /// that core is forbidden to own (core.md P6).
    bool matches(const FeatureView& feature) const;
};

const char* style_test_name(StyleCondition::Test t) noexcept;

/// One catalogue row: an appearance with an identity and a provenance.
/// One declared symbol layer, and the line type it asks for.
///
/// The PATTERN rides here rather than on `SymbolLayer`, which carries a `dash`
/// ID into a document's `DashStore`. A catalogue holds no document and cannot
/// hand out an id; whoever applies the row interns the pattern and writes the id
/// it gets. Putting the pattern on the layer instead would push a catalogue's
/// concern into the document model and into the file format, where it does not
/// belong.
struct DeclaredLayer
{
    /// Everything but the line type: what the layer draws and how.
    SymbolLayer layer{};
    DashPattern dash{}; ///< count 0 means the layer draws a solid stroke

    /// The package-relative file this layer draws, for the three `gorsel-*` layer
    /// types, and empty for every other type.
    ///
    /// A path rather than an `ImageId` because a CATALOGUE HOLDS NO DOCUMENT and
    /// cannot mint one — the same reason `dash` is interned by whoever applies
    /// the row rather than by the parser. The declaration names the picture; the
    /// `ImageResolver` handed to `StyleLibrary::add_catalog` turns the name into
    /// an id in the document that is about to carry the bytes.
    ///
    /// Without this a declared stack could not reference a picture at all, and a
    /// gösterim that is mostly numbers with one drawn glyph in it — a boundary
    /// line carrying a cogwheel, a wavy shoreline, a lightning bolt in a frame —
    /// would have to be published as a picture whole, losing the recolouring,
    /// the corners and the line-type export that the numbers exist to give.
    std::string image;
};

struct StyleEntry
{
    std::string id;         ///< stable forever; a retired id is never reused (data.md R5)
    std::string label;      ///< Turkish, what a user reads in the legend
    std::string source_ref; ///< the annex/article this row encodes, verbatim from /data

    /// Where this row sits in the package's OWN tree, outermost name first.
    ///
    /// MPYY EK-1 organises its 476 gösterim by annex and then by a section path
    /// such as SINIRLAR > İDARİ SINIRLAR, and that is the tree a planner already
    /// navigates on paper. Nothing here invents a taxonomy: the path arrives in
    /// the package (CLAUDE.md 5.13, data.md R1), and an empty one is a row the
    /// package filed nowhere.
    std::vector<std::string> group;

    /// Free labels, searched across groups. A row can be reached by a word that
    /// is in none of its names — its annex code, its plan type — without that
    /// word having to become part of the tree.
    std::vector<std::string> tags;

    /// Package-relative paths to the pictures the regulation PUBLISHED for this
    /// row, empty when it published none of that kind.
    ///
    /// MPYY's EK-1 annexes are Word documents and their symbology is pictures: a
    /// hatch for `orman`, a glyph for `cami`, a line type for `il sınırı`. The
    /// package lists them by id and maps each id to a file beside it; these are
    /// those files, resolved relative to the package.
    ///
    /// A row may list SEVERAL pictures of one kind — variants of the same
    /// gösterim. The FIRST is taken, because it is the one the annex prints first
    /// and choosing among the rest is a regulatory judgement a program does not
    /// get to make (CLAUDE.md 6.11).
    std::string image_line;   ///< `gorsel/cizgi_tipi` — repeated along the line
    std::string image_hatch;  ///< `gorsel/tarama` — tiled into the interior
    std::string image_symbol; ///< `gorsel/sembol` — placed as a glyph

    /// How big each of those is drawn, in PAPER micrometres. Zero takes the
    /// built-in starting point.
    ///
    /// The annex prints a picture and states no millimetre for it, so a default
    /// was hard-coded and every row got the same one. That is right for a scanned
    /// crop, whose size means nothing; it is wrong for a picture somebody DREW,
    /// where the size is part of the drawing. A row that names its own size is
    /// saying something the regulation's own picture could not.
    std::int32_t image_line_um{0};
    std::int32_t image_hatch_um{0};
    std::int32_t image_symbol_um{0};

    /// The spacing a tiled hatch repeats at, in paper micrometres. Zero tiles the
    /// picture edge to edge, which is what a scanned hatch wants.
    std::int32_t image_hatch_gap_um{0};

    /// The symbol this row draws, DECLARED rather than pictured.
    ///
    /// The three fields above name pictures, which is how the annex publishes its
    /// gösterimler and all a row could say until now: one appearance and up to
    /// three bitmaps. That is not enough to carry what the regulation actually
    /// prints. MPYY's İL SINIRI is a dash and a dot; its BELEDİYE SINIRI is a dash
    /// with a filled circle at each end; its PLAN ONAMA SINIRI is a row of open
    /// circles and no line at all. Each of those is a STACK of symbol layers, and
    /// a stack said in numbers turns a corner, recolours and exports as a line
    /// type — none of which a stamped picture does.
    ///
    /// Empty for a row that only has pictures, and `symbol_of_entry` then builds
    /// what it always built. A row that declares layers uses them and keeps its
    /// pictures as provenance: the annex's own drawing is what the declaration was
    /// read from and it stays beside it (CLAUDE.md 11.7).
    std::vector<DeclaredLayer> layers;

    Appearance appearance{}; ///< what the regulation says this looks like
    ScaleWindow scale{};     ///< the scales it applies at; unbounded by default
    bool deprecated{false};  ///< retained, still loadable, never silently dropped (R5)

    /// The extraction could not read this row's appearance with confidence.
    ///
    /// The MPYY package flags fourteen rows this way — the annex printed them with
    /// empty gösterim columns, or in a form the extraction could not resolve.
    /// Carrying the flag is the point: a row shown as certain when the package
    /// says otherwise is the program asserting something the regulation did not.
    /// Every surface that offers the row has to say so (data.md, CLAUDE.md 11.7).
    bool uncertain{false};

    /// Why, verbatim from the package. Empty when `uncertain` is false.
    std::vector<std::string> uncertain_reasons;
};

/// One classification rule. Conditions are conjunctive; an empty condition list
/// matches every feature and is the sanctioned way to write a catch-all last row.
struct StyleRule
{
    std::string id;                         ///< stable, for a message and a test
    std::string entry;                      ///< StyleEntry::id this rule selects
    std::vector<StyleCondition> conditions; ///< conjunctive; empty matches everything
    ScaleWindow scale{};                    ///< the scales it applies at

    /// Whether every condition holds and the scale window covers `denominator`.
    bool matches(const FeatureView& feature, ScaleDenominator denominator) const;
};

/// A loaded, self-describing style catalogue package.
///
/// The header block is mandatory and complete (data.md R2): a package that cannot
/// say which text it encodes, in which version, published when, under which
/// licence, is refused at parse time rather than believed.
class StyleCatalog
{
public:
    /// Parses one catalogue package. Every failure is an `Error` naming the field
    /// and what was expected; nothing is defaulted, and no row is skipped quietly.
    static Result<StyleCatalog> from_json(const Json& j);

    const std::string& id() const noexcept { return id_; }

    const std::string& package_version() const noexcept { return package_version_; }

    const std::string& source() const noexcept { return source_; }

    const std::string& published() const noexcept { return published_; }

    const std::string& licence() const noexcept { return licence_; }

    const std::vector<StyleEntry>& entries() const noexcept { return entries_; }

    const std::vector<StyleRule>& rules() const noexcept { return rules_; }

    /// Loud failure on an unknown id — no compiled-in fallback (data.md R6,
    /// domain.md P11). The message names the package version, because "which
    /// catalogue said that?" is a legal question (model.md R35).
    Result<const StyleEntry*> entry(std::string_view id) const;

    /// The first rule, in file order, whose scale window covers `denominator` and
    /// whose every condition holds. Returns an `Error` when nothing matches: an
    /// unclassified feature is a fact the caller must decide about, not a silent
    /// grey line.
    Result<const StyleEntry*> classify(const FeatureView& feature,
                                       ScaleDenominator denominator) const;

    /// Order-independent of nothing: the fingerprint covers the header, every row
    /// and every rule IN FILE ORDER, because the order is what decides a match.
    /// Two runs over the same package must agree bit for bit (§7.3).
    std::uint64_t content_hash() const;

private:
    std::string id_;
    std::string package_version_;
    std::string source_;
    std::string published_;
    std::string licence_;
    std::vector<StyleEntry> entries_;
    std::vector<StyleRule> rules_;
};

/// Layers a catalogue row over a base appearance.
///
/// Every property the row declares becomes `Source::Explicit`; everything it
/// leaves out keeps the base's value and the base's source, so the ByLayer /
/// ByBlock cascade of R19 survives a catalogue application intact.
Appearance apply_entry(const StyleEntry& entry, const Appearance& base);

/// `#AARRGGBB` or `#RRGGBB` (alpha then defaults to opaque) to 0xAARRGGBB.
///
/// Colours are written as hex text in the catalogue because that is how a legend
/// is read and reviewed by the engineer who signs it off; a decimal 4281236786 is
/// unreviewable. Parsing is explicit and locale-free — `<cctype>` classifiers are
/// banned on this codebase outright (CLAUDE.md 5.6).
Result<std::uint32_t> parse_rgba(std::string_view text);

} // namespace piricad::core
