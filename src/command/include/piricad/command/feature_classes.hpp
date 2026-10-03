// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: feature classes, the "pens" a digitiser picks (TODOS G-04).
//
// A surveyor digitising a topographic sheet does not think "polyline on layer BINA with the columns
// bina_no and kat_sayisi"; they think "a BUILDING". A feature class is that word made exact: the
// geometry it is (a point, a line, an area), the layer it lives on, the fields it carries with
// their defaults, how it is shown, and the checks it must pass. Picking the class and drawing
// leaves a record that already means something to a GIS (`KALEM`); an existing CAD object is
// brought under a class with a preview and a field mapping (`KALEMBAĞLA`); `KALEMDENETİM` asks
// every object whether it still is what its class says.
//
// THE DEFINITIONS ARE DATA. An institution's list of classes is a package under /data/catalogs
// (`kalem-katalogu.json`), validated against its schema, and a regulation that names a class
// changes by a data release (CLAUDE.md 5.13, Article 9). Nothing here knows what a "bina" is.
//
// THE DOCUMENT HOLDS ONLY WHICH CLASS A LAYER FOLLOWS (`Layer::feature_class`), not a copy of the
// rules: the rules are read from the package when the drawing is edited, so a corrected definition
// reaches every drawing that follows it and a drawing opened where the package is missing keeps all
// its objects and says it cannot check them.
//
// THE RULES ARE APPLIED IN ONE PLACE for every client alike (Article 1.2): `settle_feature_classes`
// runs after a command's own edits, inside the same transaction, so what the mouse, the command
// line, a script and an agent draw on a class layer ends the same — the defaults filled, an object
// of the wrong geometry refused and the whole command rolled back.
#pragma once

#include "piricad/core/attribute.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::command {

/// The one road to the document; see transaction.hpp.
class Transaction;

/// What a class IS, geometrically.
enum class ClassGeometry : std::uint8_t {
    Point, ///< `nokta`: a single marker
    Line,  ///< `cizgi`: an open path
    Area,  ///< `alan`: a closed face
    Other, ///< not one of the three: a caption, a dimension, a block reference
};

/// The Turkish word for `g` as the package spells it (`nokta`, `cizgi`, `alan`).
const char* class_geometry_word(ClassGeometry g) noexcept;

/// The Turkish noun for a sentence ("nokta", "açık çizgi", "kapalı alan").
const char* class_geometry_noun(ClassGeometry g) noexcept;

/// One field of a class: a column the class declares, with what a new object starts with.
struct ClassField
{
    std::string id;                            ///< the column id, lowercase ASCII
    std::string name;                          ///< what the table shows
    std::string summary;                       ///< one line, Turkish
    core::AttrType type{core::AttrType::Text}; ///< what the column holds
    bool required{false}; ///< a missing value is reported by KALEMDENETİM, never refused on write
    std::string default_text; ///< the text `attr_parse` reads for a new object; empty = none
    std::uint8_t scale{2};    ///< digits after the point of a Decimal
    std::vector<std::string> choices; ///< the only values the field may hold; empty = free
};

/// One class.
struct FeatureClass
{
    std::string id;                   ///< stable, lowercase, never reused (data.md R5)
    std::string name;                 ///< Turkish primary: `Bina`
    std::string summary;              ///< one line, Turkish
    std::vector<std::string> aliases; ///< other words that find it: `BINA`, `BUILDING`
    ClassGeometry geometry{ClassGeometry::Area};
    std::string layer;              ///< the layer name
    std::string group;              ///< the layer tree path, levels separated by `>`; empty = root
    bool has_colour{false};         ///< `colour_rgba` is set
    std::uint32_t colour_rgba{0};   ///< 0xAARRGGBB, valid when `has_colour`
    std::int32_t width_um{0};       ///< line width in PAPER micrometres; 0 = the layer's own
    bool has_fill{false};           ///< an area's fill
    std::uint32_t fill_rgba{0};     ///< 0xAARRGGBB, valid when `has_fill`
    std::string layer_description;  ///< the layer's description, shown in its panel
    std::vector<ClassField> fields; ///< the columns the class carries, in the order declared
    double min_area_m2{0.0};        ///< an area smaller than this is reported; 0 = no limit
    double min_length_m{0.0};       ///< a line shorter than this is reported; 0 = no limit
    std::string source; ///< the regulation or standard the class comes from, when it has one
};

/// A package of classes.
struct FeatureClassCatalog
{
    std::string package_id;            ///< `kalem-katalogu-ornek`
    std::string package_version;       ///< bumped on every change (data.md R4)
    std::vector<FeatureClass> classes; ///< in file order

    /// The class a word names: its id, its name or any alias, in the Turkish fold. Null when none.
    const FeatureClass* find(std::string_view word) const noexcept;

    /// The class a layer's `feature_class` reference names, matched by package and class id (the
    /// version after `@` is not part of the match). Null when the layer follows none, follows
    /// another package, or the class is no longer in this one.
    const FeatureClass* of_reference(std::string_view reference) const noexcept;

    /// The reference a layer stores for `cls`: `<package id>@<package version>/<class id>`.
    std::string reference_to(const FeatureClass& cls) const;
};

/// Where the shipped package is.
inline constexpr const char* kDefaultFeatureClassPath = "data/catalogs/cad/kalem-katalogu.json";

/// Reads and validates a package. A definition that cannot work is refused HERE, with the class and
/// the field named, not when somebody first draws with it: an unknown type, a default the type
/// cannot read, a default outside the field's own list of choices, a class with no layer, two
/// classes with one id or one layer.
core::Result<FeatureClassCatalog> load_feature_classes(const std::string& path);

/// The geometric class of a live object, as a digitiser sees it. A circle and an ellipse are areas
/// (they close), an arc and a spline are lines. A caption, a dimension and anything else is
/// `Other`.
ClassGeometry geometry_class_of(const core::Document& doc, core::EntityId e);

/// The package id of a layer reference: the part before `@`.
std::string reference_package(std::string_view reference);

/// The class id of a layer reference: the part after `/`.
std::string reference_class(std::string_view reference);

/// What the commit-time pass did.
struct ClassSettle
{
    std::size_t filled{0};  ///< cells written from a class default
    std::size_t checked{0}; ///< objects whose geometry was checked against their class
};

/// The commit-time pass (see the file header), run by the bus on the ops a command wrote since
/// `mark`: for every object CREATED on a class layer, or MOVED onto one, fills each missing field
/// from the class default and refuses an object whose geometry is not the class's. A refusal is the
/// caller's to roll back — the whole command, never a part of it (Article 1.6).
core::Result<ClassSettle> settle_feature_classes(Transaction& tx, std::size_t mark,
                                                 const FeatureClassCatalog& catalog);

/// One thing `check_feature_class` found.
struct ClassFinding
{
    enum class Rule : std::uint8_t {
        Geometry,      ///< not the geometry the class is
        TooSmall,      ///< an area below `min_area_m2`
        TooShort,      ///< a line below `min_length_m`
        MissingValue,  ///< a required field is empty
        NotAChoice,    ///< a value outside the field's list
        MissingColumn, ///< the document has no column for a field of the class
    };
    Rule rule{Rule::Geometry};
    core::EntityKey object{core::EntityKey::None}; ///< none for a column finding
    std::string field;                             ///< the field, when the rule is about one
    std::string detail;                            ///< one sentence, Turkish
};

/// Everything wrong with the objects of a layer that follows `cls`.
std::vector<ClassFinding> check_feature_class(const core::Document& doc, core::LayerId layer,
                                              const FeatureClass& cls);

/// The Turkish name of a rule, for a report.
const char* class_rule_word(ClassFinding::Rule rule) noexcept;

} // namespace piricad::command
