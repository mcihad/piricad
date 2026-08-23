// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/style.hpp"

#include "piricad/core/text.hpp"

namespace piricad::core {
namespace {

/// A per-record-type seed, so that folding a style into a document hash cannot
/// collide with folding a layer or an attribute record that happens to carry the
/// same integers.
constexpr std::uint64_t kAppearanceSeed = fnv1a("piricad.core.appearance");
constexpr std::uint64_t kSymbolSeed     = fnv1a("piricad.core.symbol");

} // namespace

std::uint64_t fold_appearance(const Appearance& a, std::uint64_t seed)
{
    // Field by field, never a memcpy of the struct. The padding bytes are
    // indeterminate and the byte order of a multi-byte member is not, so hashing
    // the raw object would give one answer on x86 and another somewhere else —
    // and a golden fixture that disagrees across platforms is a legal defect
    // (.claude/core.md R9, piricad.md §7.3).
    std::uint64_t h = seed;
    h               = fnv1a_int(static_cast<std::int64_t>(a.rgba), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.width_um), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.dash), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.symbol), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.fill_rgba), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.hatch), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.z_order), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_colour), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_width), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_dash), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_fill), h);
    return h;
}

std::size_t StyleTable::Hash::operator()(const Appearance& a) const noexcept
{
    // Every field, including z_order and the four Source tags: two appearances
    // that differ only in draw order are two different styles (R18).
    return static_cast<std::size_t>(fold_appearance(a, kAppearanceSeed));
}

namespace {

/// One entry of a name table: the machine name and the value it stands for.
template<class T> struct NameOf
{
    const char* name; ///< the machine name, as it appears in a file and a command
    T value;          ///< what it means
};

/// The tables the parsers and the "valid names" messages both read. One table per
/// enum, so a name added here appears in the parser and in the error message
/// without either being edited.
constexpr NameOf<Unit> kUnits[] = {
    {"kagit", Unit::Paper}, {"zemin", Unit::Ground}, {"piksel", Unit::Pixel}};

constexpr NameOf<SymbolLayerType> kLayerTypes[] = {
    {"cizgi", SymbolLayerType::SimpleLine},
    {"isaretci-cizgi", SymbolLayerType::MarkerLine},
    {"tarak-cizgi", SymbolLayerType::HashLine},
    {"dolgu", SymbolLayerType::SimpleFill},
    {"cizgi-desen-dolgu", SymbolLayerType::LinePatternFill},
    {"nokta-desen-dolgu", SymbolLayerType::PointPatternFill},
    {"merkez-isaretci", SymbolLayerType::CentroidFill},
    {"isaretci", SymbolLayerType::SimpleMarker}};

constexpr NameOf<MarkerShape> kShapes[] = {{"daire", MarkerShape::Circle},
                                           {"kare", MarkerShape::Square},
                                           {"ucgen", MarkerShape::Triangle},
                                           {"baklava", MarkerShape::Diamond},
                                           {"yildiz", MarkerShape::Star},
                                           {"arti", MarkerShape::Cross},
                                           {"carpi", MarkerShape::XCross},
                                           {"ok", MarkerShape::Arrow},
                                           {"yarim-daire", MarkerShape::HalfCircle},
                                           {"besgen", MarkerShape::Pentagon},
                                           {"altigen", MarkerShape::Hexagon},
                                           {"cizik", MarkerShape::Tick}};

constexpr NameOf<MarkerPlacement> kPlacements[] = {{"aralik", MarkerPlacement::Interval},
                                                   {"tepe", MarkerPlacement::Vertex},
                                                   {"ilk", MarkerPlacement::FirstVertex},
                                                   {"son", MarkerPlacement::LastVertex},
                                                   {"orta", MarkerPlacement::Centre}};

constexpr NameOf<LineCap> kCaps[] = {
    {"duz", LineCap::Butt}, {"yuvarlak", LineCap::Round}, {"kare", LineCap::Square}};

constexpr NameOf<LineJoin> kJoins[] = {
    {"kose", LineJoin::Miter}, {"yuvarlak", LineJoin::Round}, {"pah", LineJoin::Bevel}};

/// Byte-exact lookup. Deliberately NOT case folded: these are machine names that
/// appear in files and in golden fixtures, and folding Turkish text here would
/// need the table core is forbidden to own (core.md P6). The command layer folds
/// what a user typed before it gets here.
template<class T, std::size_t N>
std::optional<T> lookup(const NameOf<T> (&table)[N], std::string_view name) noexcept
{
    for (const auto& row : table)
        if (name == row.name) return row.value;
    return std::nullopt;
}

/// The machine name of a value. Same table as the parser, so a name can never
/// exist in one direction and not the other.
template<class T, std::size_t N> const char* name_in(const NameOf<T> (&table)[N], T value) noexcept
{
    for (const auto& row : table)
        if (row.value == value) return row.name;
    return "?";
}

template<class T, std::size_t N> std::string names_of(const NameOf<T> (&table)[N])
{
    std::string out;
    for (const auto& row : table) {
        if (!out.empty()) out += ", ";
        out += row.name;
    }
    return out;
}

/// A measure folds its unit with its value, never the value alone.
std::uint64_t fold_measure(const Measure& m, std::uint64_t seed)
{
    return fnv1a_int(static_cast<std::int64_t>(m.unit),
                     fnv1a_int(static_cast<std::int64_t>(m.value), seed));
}

} // namespace

const char* unit_name(Unit u) noexcept
{
    return name_in(kUnits, u);
}

const char* symbol_layer_type_name(SymbolLayerType t) noexcept
{
    return name_in(kLayerTypes, t);
}

const char* marker_shape_name(MarkerShape sh) noexcept
{
    return name_in(kShapes, sh);
}

const char* marker_placement_name(MarkerPlacement p) noexcept
{
    return name_in(kPlacements, p);
}

bool draws_fill(SymbolLayerType t) noexcept
{
    // A pattern fill paints the interior too: what varies is whether the paint is
    // a colour or a texture, and the caller that clips to the ring needs both.
    return t == SymbolLayerType::SimpleFill || t == SymbolLayerType::LinePatternFill ||
           t == SymbolLayerType::PointPatternFill;
}

bool draws_stroke(SymbolLayerType t) noexcept
{
    return t == SymbolLayerType::SimpleLine || t == SymbolLayerType::MarkerLine ||
           t == SymbolLayerType::HashLine || t == SymbolLayerType::LinePatternFill;
}

bool draws_marker(SymbolLayerType t) noexcept
{
    return t == SymbolLayerType::MarkerLine || t == SymbolLayerType::HashLine ||
           t == SymbolLayerType::PointPatternFill || t == SymbolLayerType::CentroidFill ||
           t == SymbolLayerType::SimpleMarker;
}

std::optional<Unit> unit_from_name(std::string_view name) noexcept
{
    return lookup(kUnits, name);
}

std::optional<SymbolLayerType> symbol_layer_type_from_name(std::string_view name) noexcept
{
    return lookup(kLayerTypes, name);
}

std::optional<MarkerShape> marker_shape_from_name(std::string_view name) noexcept
{
    return lookup(kShapes, name);
}

std::optional<MarkerPlacement> marker_placement_from_name(std::string_view name) noexcept
{
    return lookup(kPlacements, name);
}

std::optional<LineCap> line_cap_from_name(std::string_view name) noexcept
{
    return lookup(kCaps, name);
}

std::optional<LineJoin> line_join_from_name(std::string_view name) noexcept
{
    return lookup(kJoins, name);
}

std::string symbol_layer_type_names()
{
    return names_of(kLayerTypes);
}

std::string marker_shape_names()
{
    return names_of(kShapes);
}

std::string marker_placement_names()
{
    return names_of(kPlacements);
}

std::string unit_names()
{
    return names_of(kUnits);
}

std::uint64_t fold_symbol(const Symbol& sym, std::uint64_t seed)
{
    // Order matters and is folded: a fill under a stroke and a stroke under a
    // fill are two different symbols, and the second one hides the first.
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(sym.layers.size()), seed);
    for (const SymbolLayer& l : sym.layers) {
        // Every field, and each measure's UNIT beside its value: a two-millimetre
        // spacing on paper and a two-millimetre spacing on the ground are two
        // different symbols, and at 1/1000 they differ by a factor of a thousand.
        h = fnv1a_int(static_cast<std::int64_t>(l.type), h);
        h = fold_measure(l.offset, h);
        h = fold_measure(l.size, h);
        h = fold_measure(l.interval, h);
        h = fold_measure(l.spacing_y, h);
        h = fnv1a_int(static_cast<std::int64_t>(l.angle_udeg), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.shape), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.placement), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.cap), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.join), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.opacity), h);
        h = fold_appearance(l.look, h);
    }
    h = fnv1a_int(static_cast<std::int64_t>(sym.min_scale), h);
    h = fnv1a_int(static_cast<std::int64_t>(sym.max_scale), h);
    return h;
}

std::size_t StyleTable::SymbolHash::operator()(const Symbol& s) const noexcept
{
    return static_cast<std::size_t>(fold_symbol(s, kSymbolSeed));
}

StyleTable::StyleTable()
{
    // Entry 0 is the kByLayerStyle sentinel and exists before anything is
    // interned, so a default Appearance — every property ByLayer, which is what
    // an ordinary cadastral entity carries — interns back to 0 and costs nothing
    // (R13).
    const Appearance by_layer{};
    entries_.push_back(by_layer);
    symbols_.push_back(Symbol::of(by_layer));
    intern_.emplace(by_layer, kByLayerStyle);
    symbol_intern_.emplace(symbols_.front(), kByLayerStyle);
}

StyleId StyleTable::intern(const Appearance& a)
{
    if (const auto it = intern_.find(a); it != intern_.end()) return it->second;

    // Ids are handed out in first-seen order. Nothing here reads a pointer, an
    // address or the hash map's iteration order, so the same sequence of interns
    // yields the same ids on every platform and in every run — which is what the
    // golden fixtures record (.claude/core.md P11).
    const auto id = static_cast<StyleId>(entries_.size());
    entries_.push_back(a);
    symbols_.push_back(Symbol::of(a));
    intern_.emplace(a, id);
    symbol_intern_.emplace(symbols_.back(), id);
    return id;
}

StyleId StyleTable::intern(const Symbol& sym)
{
    if (const auto it = symbol_intern_.find(sym); it != symbol_intern_.end()) return it->second;

    // A one-layer stack IS its Appearance. Routing it through the same table
    // keeps a drawing that never uses a stack byte for byte the drawing it was
    // before stacks existed — the golden fixtures say so and they are right to.
    if (sym.layers.size() == 1 && sym.min_scale == 0 && sym.max_scale == 0) {
        // A DEFAULT layer carrying nothing but this appearance. Compared as a
        // whole rather than field by field, so a field added to SymbolLayer later
        // cannot quietly fall out of this test and start collapsing stacks that
        // are no longer plain.
        SymbolLayer plain;
        plain.look = sym.layers.front().look;
        if (sym.layers.front() == plain) return intern(plain.look);
    }

    const auto id = static_cast<StyleId>(entries_.size());
    entries_.push_back(sym.primary());
    symbols_.push_back(sym);
    symbol_intern_.emplace(sym, id);

    // Deliberately NOT added to intern_: the resolved appearance of a stack is a
    // summary, not the thing itself, and letting a bare Appearance intern back to
    // a stacked id would silently give an entity a symbol it never asked for.
    return id;
}

const Symbol& StyleTable::symbol_at(StyleId id) const
{
    return id < symbols_.size() ? symbols_[id] : symbols_[kByLayerStyle];
}

const Appearance& StyleTable::at(StyleId id) const
{
    // A corrupt or stale style column must not be able to read past the end and
    // take the process down mid-frame; entry 0 is always present and always a
    // legal appearance, so an unknown id degrades to "inherit from the layer".
    return id < entries_.size() ? entries_[id] : entries_[kByLayerStyle];
}

std::uint64_t StyleTable::fold(std::uint64_t seed) const
{
    // In id order, and with the id mixed in: the id is what every entity stores,
    // so two tables holding the same appearances under different ids describe two
    // different documents.
    std::uint64_t h = seed;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        h = fnv1a_int(static_cast<std::int64_t>(i), h);
        h = fold_appearance(entries_[i], h);

        // A stack folds its EXTRA layers only. A single-layer stroke stack is
        // exactly the appearance already folded above, so folding it again would
        // change the fingerprint of every drawing that has no stacks at all.
        if (i < symbols_.size() && symbols_[i].layers.size() > 1) h = fold_symbol(symbols_[i], h);
    }
    return h;
}

Appearance resolve_appearance(const Appearance& own, const Appearance& layer_default)
{
    // ByBlock has no block reference at this level. Blocks (INSERT / hücre) are a
    // Phase-2 deliverable; until the block table exists, a ByBlock property
    // resolves exactly as ByLayer does. Dropping the property instead would lose
    // data on every DWG/DXF import, which P11 forbids.
    const auto from_own = [](Source s) { return s == Source::Explicit; };

    Appearance out = own;

    if (!from_own(own.src_colour)) out.rgba = layer_default.rgba;
    if (!from_own(own.src_width)) out.width_um = layer_default.width_um;
    if (!from_own(own.src_dash)) out.dash = layer_default.dash;
    if (!from_own(own.src_fill)) {
        // hatch is the pattern half of the same fill property — Appearance carries
        // no src_hatch, and a resolved fill colour with an unresolved hatch would
        // draw an imar lekesi in the layer's colour with the entity's (empty)
        // pattern (R18).
        out.fill_rgba = layer_default.fill_rgba;
        out.hatch     = layer_default.hatch;
    }

    // symbol and z_order have no Source of their own and therefore never cascade;
    // they are carried through from `own` unchanged.

    // The result IS the resolution, so every property now speaks for itself. The
    // command that writes the style column stores this, and the renderer reads
    // one u32 and no cascade (R14).
    out.src_colour = Source::Explicit;
    out.src_width  = Source::Explicit;
    out.src_dash   = Source::Explicit;
    out.src_fill   = Source::Explicit;
    return out;
}

} // namespace piricad::core
