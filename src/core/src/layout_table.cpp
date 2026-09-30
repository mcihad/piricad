// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/layout_table.hpp"

#include "piricad/core/attach.hpp"
#include "piricad/core/attribute.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/geometry.hpp"
#include "piricad/core/identity.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/text.hpp"
#include "piricad/core/text_store.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace piricad::core {
namespace {

/// Ten to the `n`, for the small `n` a fixed-point figure needs.
std::uint64_t ten_to(int n)
{
    std::uint64_t out = 1;
    for (int i = 0; i < n; ++i)
        out *= 10;
    return out;
}

/// `digits` with a separator every three from the right.
std::string grouped(const std::string& digits, char separator)
{
    std::string out;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(digits[i]);
        const std::size_t left = n - 1 - i;
        if (left > 0 && left % 3 == 0) out.push_back(separator);
    }
    return out;
}

/// The attribute column a source names, or `kNoAttr`.
AttrId attribute_of(const Document& doc, const std::string& source)
{
    const AttrTable& attrs = doc.attributes();
    for (std::size_t c = 0; c < attrs.columns(); ++c)
        if (const AttrColumn* held = attrs.column(static_cast<AttrId>(c));
            held != nullptr && turkish_key_equals(source, held->spec().id))
            return static_cast<AttrId>(c);
    return kNoAttr;
}

/// Where an object's row of a table stands: a point where it is, a closed ring
/// at its area centroid — the snap engine's `AĞIRLIK MERKEZİ`, the same
/// arithmetic — anything else at its first corner, and an object with no
/// corners at the middle of what it covers.
Point2 anchor_of(const Document& doc, EntityId e)
{
    const RingGeometry& geometry = doc.geometry();
    const RingSpan span          = geometry.rings_of(doc.entities().slot[e]);
    if (span.count == 0) {
        const Box2 box = doc.entity_extent(e);
        return Point2{box.min_x + (box.max_x - box.min_x) / 2,
                      box.min_y + (box.max_y - box.min_y) / 2};
    }
    const auto xs = geometry.ring_xs(span.first);
    const auto ys = geometry.ring_ys(span.first);
    if (xs.empty()) return Point2{};
    Point2 centre{};
    if (geometry.ring_role[span.first] != RingRole::Open && xs.size() >= 3 &&
        points_centroid(xs, ys, centre))
        return centre;
    return Point2{xs[0], ys[0]};
}

/// A cell's number, from an attribute's stored value, when the column asks for
/// a format; the stored form otherwise.
std::string attribute_text(const AttrValue& value, const LayoutColumn& column, bool comma)
{
    if (!value.present) return {};
    const bool formatted = column.decimals >= 0 || column.thousands;
    if (formatted) {
        switch (value.type) {
        case AttrType::Int64:
            return format_fixed(value.number, 0, column.decimals, column.thousands, comma);
        case AttrType::Length:
            return format_fixed(value.number, 3, column.decimals, column.thousands, comma);
        case AttrType::Decimal:
            return format_fixed(value.number, value.scale, column.decimals, column.thousands,
                                comma);
        default: break;
        }
    }
    return attr_display(value, comma ? DecimalMark::Comma : DecimalMark::Point);
}

/// Whether a table lists this object: a point, a line or a face — the things a
/// coordinate list and an attribute table are about.
///
/// NOT A CAPTION. A text in this program is an object whose slot carries words
/// (`TextTable`), a point or a short baseline standing a little off what it
/// names; a surveyed list arrives as points WITH their numbers written beside
/// them, and every number entered the coordinate list as corners of its own —
/// the points were listed twice, their labels' two ends and all (reported).
/// Nor a dimension, a leader or a hatch: their rings are measurement and fill,
/// not corners of the ground.
bool listed(const Document& doc, EntityId e)
{
    const KindId kind = doc.entities().kind[e];
    if (kind == kDimensionKind || kind == kLeaderKind || kind == kHatchKind) return false;
    return !doc.texts().has(doc.entities().slot[e]);
}

/// NATURAL ORDER, as a person counts: `2` before `10`, `K-2` before `K-10`,
/// `P-7` before `P-17`; letters by the Turkish fold, so `Ç` sits with `C`.
///
/// HAND-ROLLED, and said: the one collator in this program that orders numbers
/// this way — Qt's `QCollator` in numeric mode — lives above the core, which
/// links nothing that could (CLAUDE.md 3.2). Digits are tested by their code
/// points, never by `<cctype>` (5.6).
int natural_compare(std::string_view a, std::string_view b)
{
    const std::string x = turkish_fold_key(a);
    const std::string y = turkish_fold_key(b);
    const auto digit    = [](char c) { return c >= '0' && c <= '9'; };
    std::size_t i       = 0;
    std::size_t j       = 0;
    while (i < x.size() && j < y.size()) {
        if (digit(x[i]) && digit(y[j])) {
            std::size_t ie = i;
            std::size_t je = j;
            while (ie < x.size() && digit(x[ie]))
                ++ie;
            while (je < y.size() && digit(y[je]))
                ++je;
            std::string_view nx(x.data() + i, ie - i);
            std::string_view ny(y.data() + j, je - j);
            while (nx.size() > 1 && nx.front() == '0')
                nx.remove_prefix(1);
            while (ny.size() > 1 && ny.front() == '0')
                ny.remove_prefix(1);
            if (nx.size() != ny.size()) return nx.size() < ny.size() ? -1 : 1;
            if (const int c = nx.compare(ny); c != 0) return c < 0 ? -1 : 1;
            i = ie;
            j = je;
            continue;
        }
        if (x[i] != y[j])
            return static_cast<unsigned char>(x[i]) < static_cast<unsigned char>(y[j]) ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < x.size()) return 1;
    if (j < y.size()) return -1;
    return 0;
}

/// What a row is ordered by: a number, or words in natural order, or nothing —
/// and a row with nothing to be ordered by goes last, whichever way the table
/// is ordered.
struct SortKey
{
    bool present{false};
    bool numeric{false};
    double number{0.0};
    std::string text;
};

/// One row of a table: the object it comes from, and where it stands.
struct Row
{
    EntityId entity{kNoEntity};
    Point2 at{};
};

} // namespace

std::string format_fixed(std::int64_t scaled, int scale, int decimals, bool thousands, bool comma)
{
    scale    = std::clamp(scale, 0, 9);
    decimals = decimals < 0 ? scale : std::min(decimals, 9);

    const bool negative = scaled < 0;
    // The magnitude as unsigned, so the most negative value has one too.
    std::uint64_t magnitude = negative ? static_cast<std::uint64_t>(-(scaled + 1)) + 1u
                                       : static_cast<std::uint64_t>(scaled);

    if (decimals < scale) {
        // HALF AWAY FROM ZERO: on the magnitude, a remainder of half or more
        // carries — which, with the sign put back after, is away from zero.
        const std::uint64_t divisor = ten_to(scale - decimals);
        const std::uint64_t kept    = magnitude / divisor;
        magnitude                   = kept + (magnitude % divisor * 2 >= divisor ? 1u : 0u);
    } else {
        magnitude *= ten_to(decimals - scale);
    }

    const std::uint64_t unit = ten_to(decimals);
    std::string whole        = std::to_string(magnitude / unit);
    if (thousands) whole = grouped(whole, comma ? '.' : ',');

    std::string out = (negative && magnitude != 0 ? "-" : "") + whole;
    if (decimals > 0) {
        std::string fraction = std::to_string(magnitude % unit);
        fraction.insert(0, static_cast<std::size_t>(decimals) - fraction.size(), '0');
        out += (comma ? ',' : '.') + fraction;
    }
    return out;
}

std::vector<LayoutColumn> table_column_list(const Document& doc, const LayoutItem& item)
{
    if (!item.table_columns.empty()) return item.table_columns;

    // THE LIST A TABLE HAD BEFORE COLUMNS COULD BE SET, in the attribute
    // table's own order — the order the renderer always printed them in, which
    // is not the order `columns` names them in.
    std::vector<LayoutColumn> out;
    const AttrTable& attrs = doc.attributes();
    for (std::size_t c = 0; c < attrs.columns(); ++c) {
        const AttrColumn* held = attrs.column(static_cast<AttrId>(c));
        if (held == nullptr) continue;
        if (!item.text.empty() && !attr_applies_to(held->spec(), item.text)) continue;
        if (!item.columns.empty() &&
            std::none_of(item.columns.begin(), item.columns.end(), [&](const std::string& named) {
                return turkish_key_equals(named, held->spec().id);
            }))
            continue;
        LayoutColumn one;
        one.source = held->spec().id;
        out.push_back(std::move(one));
    }
    return out;
}

std::string table_heading(const Document& doc, const LayoutColumn& column)
{
    if (!column.heading.empty()) return column.heading;
    if (const TableSource* computed = table_source(column.source); computed != nullptr)
        return computed->heading;
    if (const AttrId id = attribute_of(doc, column.source); id != kNoAttr)
        if (const AttrColumn* held = doc.attributes().column(id); held != nullptr)
            return held->spec().name_tr.empty() ? held->spec().id : held->spec().name_tr;
    return column.source;
}

Result<TableText> table_text(const Document& doc, const LayoutItem& item)
{
    const std::string& layer_name = item.text;
    if (layer_name.empty()) return err(ErrorCode::InvalidArgument, "tablo: katman seçilmedi");
    const LayerId layer = doc.find_layer(layer_name);
    if (layer == kNoLayer)
        return err(ErrorCode::NotFound, "tablo: '" + layer_name + "' adlı katman yok");

    TableText out;
    out.columns = table_column_list(doc, item);
    if (out.columns.empty())
        return err(ErrorCode::NotFound, "tablo: '" + layer_name + "' katmanında sütun yok");

    // EVERY NAME RESOLVED BEFORE A ROW IS WRITTEN: a column the drawing has
    // lost refuses the table, it does not print as a column of blanks.
    std::vector<AttrId> attrs(out.columns.size(), kNoAttr);
    bool wants_number = turkish_key_equals(item.table.sort_by, "$no");
    for (std::size_t c = 0; c < out.columns.size(); ++c) {
        const LayoutColumn& column = out.columns[c];
        if (const TableSource* computed = table_source(column.source); computed != nullptr) {
            if (std::string_view(computed->word) == "$no") wants_number = true;
        } else {
            attrs[c] = attribute_of(doc, column.source);
            if (attrs[c] == kNoAttr)
                return err(ErrorCode::NotFound,
                           "tablo: '" + column.source + "' adlı öznitelik sütunu yok");
        }
        out.heads.push_back(table_heading(doc, column));
    }

    // ---- the rows ---------------------------------------------------------
    const EntityTable& entities = doc.entities();
    std::vector<Row> rows;
    std::set<std::pair<Mm, Mm>> seen;
    for (EntityId e = 0; e < entities.size(); ++e) {
        if (!doc.alive(e) || entities.layer[e] != layer || !listed(doc, e)) continue;
        if (item.table.rows == TableRows::Objects) {
            rows.push_back(Row{e, anchor_of(doc, e)});
            continue;
        }
        // A CORNER TWO PARCELS SHARE IS ONE POINT, listed once, where it is
        // first met.
        const RingGeometry& geometry = doc.geometry();
        const RingSpan span          = geometry.rings_of(entities.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geometry.ring_xs(r);
            const auto ys = geometry.ring_ys(r);
            for (std::size_t i = 0; i < xs.size(); ++i)
                if (seen.insert({xs[i], ys[i]}).second)
                    rows.push_back(Row{e, Point2{xs[i], ys[i]}});
        }
    }

    // THE NUMBERS THE SHEET ALREADY WRITES AT THE CORNERS. `KÖŞENUMARALA`
    // (`islem.kose_numarala`) puts each corner's number beside it as a caption
    // tied to that corner, and the coordinate table has to say the same number
    // the sheet shows there. By where the corner stands, so a corner two
    // parcels share has its number whichever of the two was numbered.
    std::map<std::pair<Mm, Mm>, std::string> labelled;
    if (wants_number) {
        const AttachTable& links = doc.attachments();
        for (const EntityId d : links.attached()) {
            const Attachment* a = links.get(d);
            if (a == nullptr || a->anchor != AttachAnchor::Vertex || !doc.alive(d)) continue;
            const std::string_view words = doc.texts().text(entities.slot[d]);
            if (words.empty()) continue;
            const EntityId from = doc.slot_of(a->source);
            if (from == kNoEntity || !doc.alive(from)) continue;
            const RingSpan span = doc.geometry().rings_of(entities.slot[from]);
            if (a->ring >= span.count) continue;
            const auto xs = doc.geometry().ring_xs(span.first + a->ring);
            const auto ys = doc.geometry().ring_ys(span.first + a->ring);
            if (a->index >= xs.size()) continue;
            labelled.emplace(std::pair{xs[a->index], ys[a->index]}, std::string(words));
        }
    }

    // AND THE NUMBERS OF THE SURVEYED POINTS, by where they stand: a corner of
    // a parcel that a numbered point sits on is that point, and it is listed
    // under its own number (`NOKTALAR` keeps it in `nokta_no`).
    std::map<std::pair<Mm, Mm>, std::string> numbered;
    const AttrId number_column = wants_number ? doc.attributes().find("nokta_no") : kNoAttr;
    if (number_column != kNoAttr)
        for (EntityId e = 0; e < entities.size(); ++e) {
            if (!doc.alive(e) || entities.kind[e] != kPointKind) continue;
            const auto held = doc.attribute(number_column, e);
            if (!held || !held.value().present || held.value().text.empty()) continue;
            const RingSpan span = doc.geometry().rings_of(entities.slot[e]);
            if (span.count == 0) continue;
            const auto xs = doc.geometry().ring_xs(span.first);
            const auto ys = doc.geometry().ring_ys(span.first);
            if (!xs.empty()) numbered.emplace(std::pair{xs[0], ys[0]}, held.value().text);
        }

    // ---- each row's own number ---------------------------------------------
    //
    // The number written at the corner, else the surveyed point's own, else —
    // for an object row — the object's own `nokta_no`. Empty when there is
    // none; the row then gets its place in the table, AFTER the order is made.
    std::vector<std::string> real(rows.size());
    if (wants_number)
        for (std::size_t n = 0; n < rows.size(); ++n) {
            const std::pair<Mm, Mm> here{rows[n].at.x, rows[n].at.y};
            if (item.table.rows == TableRows::Vertices)
                if (const auto found = labelled.find(here); found != labelled.end()) {
                    real[n] = found->second;
                    continue;
                }
            if (const auto found = numbered.find(here); found != numbered.end()) {
                real[n] = found->second;
            } else if (number_column != kNoAttr && item.table.rows == TableRows::Objects) {
                if (const auto own = doc.attribute(number_column, rows[n].entity);
                    own && own.value().present)
                    real[n] = own.value().text;
            }
        }

    // ---- the order ----------------------------------------------------------
    //
    // BY WHAT THE TABLE IS SORTED BY, in natural order and stably, so rows with
    // the same key keep the order the drawing holds them in. A row with no key
    // — a corner nobody numbered, an empty cell — goes last either way.
    std::vector<std::size_t> order(rows.size());
    for (std::size_t n = 0; n < order.size(); ++n)
        order[n] = n;
    if (const std::string& by = item.table.sort_by;
        !by.empty() && !turkish_key_equals(by, "$sira")) {
        const TableSource* computed = table_source(by);
        const AttrId by_attr        = computed == nullptr ? attribute_of(doc, by) : kNoAttr;
        if (computed == nullptr && by_attr == kNoAttr)
            return err(ErrorCode::NotFound, "tablo: sıralama sütunu '" + by + "' yok");
        const std::string_view word = computed != nullptr ? computed->word : std::string_view();
        std::vector<SortKey> keys(rows.size());
        for (std::size_t n = 0; n < rows.size(); ++n) {
            SortKey& k   = keys[n];
            const Row& r = rows[n];
            if (word == "$no") {
                k.text    = real[n];
                k.present = !k.text.empty();
            } else if (word == "$y" || word == "$x") {
                k.present = true;
                k.numeric = true;
                k.number  = static_cast<double>(word == "$y" ? r.at.x : r.at.y);
            } else if (word == "$alan") {
                k.present = true;
                k.numeric = true;
                k.number  = static_cast<double>(doc.entity_area(r.entity));
            } else if (word == "$uzunluk") {
                k.present = true;
                k.numeric = true;
                k.number  = static_cast<double>(doc.entity_perimeter(r.entity));
            } else if (word == "$katman") {
                const Layer* held = doc.layer(entities.layer[r.entity]);
                k.text            = held != nullptr ? held->name : std::string();
                k.present         = !k.text.empty();
            } else if (by_attr != kNoAttr) {
                const auto held = doc.attribute(by_attr, r.entity);
                if (!held || !held.value().present) continue;
                const AttrValue& v = held.value();
                k.present          = true;
                if (v.type == AttrType::Int64 || v.type == AttrType::Length ||
                    v.type == AttrType::Decimal) {
                    k.numeric    = true;
                    double scale = 1.0;
                    for (int d = 0; d < (v.type == AttrType::Decimal ? v.scale : 0); ++d)
                        scale *= 10.0;
                    k.number = static_cast<double>(v.number) / scale;
                } else {
                    k.text = attr_display(v, DecimalMark::Point);
                }
            }
        }
        const bool down = item.table.sort_descending;
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const SortKey& ka = keys[a];
            const SortKey& kb = keys[b];
            if (ka.present != kb.present) return ka.present;
            if (!ka.present) return false;
            int c = 0;
            if (ka.numeric && kb.numeric)
                c = ka.number < kb.number ? -1 : (kb.number < ka.number ? 1 : 0);
            else
                c = natural_compare(ka.text, kb.text);
            return down ? c > 0 : c < 0;
        });
    }

    // ---- the cells --------------------------------------------------------
    const bool comma = item.table.decimal_comma;
    out.rows.reserve(rows.size());
    for (std::size_t at = 0; at < order.size(); ++at) {
        const std::size_t n = order[at];
        const Row& row      = rows[n];
        std::vector<std::string> cells;
        cells.reserve(out.columns.size());
        for (std::size_t c = 0; c < out.columns.size(); ++c) {
            const LayoutColumn& column = out.columns[c];
            if (attrs[c] != kNoAttr) {
                const auto held = doc.attribute(attrs[c], row.entity);
                cells.push_back(held ? attribute_text(held.value(), column, comma) : std::string());
                continue;
            }
            const std::string_view word = table_source(column.source)->word;
            const auto whole            = [&](std::int64_t v) {
                return format_fixed(v, 0, column.decimals < 0 ? 0 : column.decimals,
                                    column.thousands, comma);
            };
            // THE ROW'S PLACE IS ITS PLACE IN THE TABLE, after the order: the
            // `Sıra` column counts down the page, whatever it was sorted by.
            if (word == "$sira") {
                cells.push_back(whole(static_cast<std::int64_t>(at + 1)));
            } else if (word == "$no") {
                cells.push_back(real[n].empty() ? whole(static_cast<std::int64_t>(at + 1))
                                                : real[n]);
            } else if (word == "$y") {
                // THE EASTING — `Sağa (Y)` — which the model keeps in `x`.
                cells.push_back(
                    format_fixed(row.at.x, 3, column.decimals, column.thousands, comma));
            } else if (word == "$x") {
                cells.push_back(
                    format_fixed(row.at.y, 3, column.decimals, column.thousands, comma));
            } else if (word == "$alan") {
                cells.push_back(format_fixed(doc.entity_area(row.entity), 6,
                                             column.decimals < 0 ? 2 : column.decimals,
                                             column.thousands, comma));
            } else if (word == "$uzunluk") {
                cells.push_back(format_fixed(doc.entity_perimeter(row.entity), 3,
                                             column.decimals < 0 ? 2 : column.decimals,
                                             column.thousands, comma));
            } else if (word == "$katman") {
                const Layer* held = doc.layer(entities.layer[row.entity]);
                cells.push_back(held != nullptr ? held->name : std::string());
            } else {
                cells.emplace_back();
            }
        }
        out.rows.push_back(std::move(cells));
    }
    return out;
}

} // namespace piricad::core
