// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/layout.hpp"

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace kentos::core {
namespace {

/// One entry of the A-series table.
struct Paper
{
    const char* name;
    std::int64_t width_mm;
    std::int64_t height_mm;
};

/// ISO 216 A-series, portrait. A geometric fact, not a regulation.
constexpr Paper kPapers[] = {
    {"A5", 148, 210}, {"A4", 210, 297}, {"A3", 297, 420},
    {"A2", 420, 594}, {"A1", 594, 841}, {"A0", 841, 1189},
};

constexpr std::array<const char*, 7> kPaperNames = {"A5", "A4", "A3", "A2", "A1", "A0", "ozel"};

/// Folds one string into a running hash, length first.
///
/// THE LENGTH IS PART OF IT, so that two adjacent fields cannot be confused for
/// one: a layout named `AB` with an item named `C` must not hash like `A` with
/// `BC`, and without the length it would.
std::uint64_t fold_text(std::uint64_t h, std::string_view s)
{
    h = fnv1a_int(static_cast<std::int64_t>(s.size()), h);
    return fnv1a(s, h);
}

} // namespace

std::optional<std::pair<std::int64_t, std::int64_t>> paper_size_mm(std::string_view paper)
{
    for (const Paper& p : kPapers)
        if (turkish_key_equals(paper, p.name)) return std::make_pair(p.width_mm, p.height_mm);
    return std::nullopt;
}

std::span<const char* const> paper_names()
{
    return kPaperNames;
}

bool is_custom_paper(std::string_view paper)
{
    return turkish_key_equals(paper, "ozel") || turkish_key_equals(paper, "custom");
}

std::string canonical_paper(std::string_view paper)
{
    if (is_custom_paper(paper)) return "ozel";
    for (const Paper& p : kPapers)
        if (turkish_key_equals(paper, p.name)) return p.name;
    return std::string(paper);
}

const char* layout_item_kind_id(LayoutItemKind kind) noexcept
{
    switch (kind) {
    case LayoutItemKind::Map: return "harita";
    case LayoutItemKind::Label: return "metin";
    case LayoutItemKind::ScaleBar: return "olcek";
    case LayoutItemKind::NorthArrow: return "kuzey";
    case LayoutItemKind::Legend: return "lejant";
    case LayoutItemKind::Picture: return "resim";
    case LayoutItemKind::Shape: return "sekil";
    case LayoutItemKind::Table: return "tablo";
    }
    return "metin";
}

const char* layout_item_kind_label(LayoutItemKind kind) noexcept
{
    switch (kind) {
    case LayoutItemKind::Map: return "Harita";
    case LayoutItemKind::Label: return "Metin";
    case LayoutItemKind::ScaleBar: return "Ölçek çubuğu";
    case LayoutItemKind::NorthArrow: return "Kuzey oku";
    case LayoutItemKind::Legend: return "Lejant";
    case LayoutItemKind::Picture: return "Resim";
    case LayoutItemKind::Shape: return "Şekil";
    case LayoutItemKind::Table: return "Tablo";
    }
    return "Metin";
}

std::optional<LayoutItemKind> layout_item_kind_from_id(std::string_view id) noexcept
{
    constexpr std::array<LayoutItemKind, 8> kKinds = {
        LayoutItemKind::Map,        LayoutItemKind::Label,  LayoutItemKind::ScaleBar,
        LayoutItemKind::NorthArrow, LayoutItemKind::Legend, LayoutItemKind::Picture,
        LayoutItemKind::Shape,      LayoutItemKind::Table,
    };
    for (LayoutItemKind kind : kKinds)
        if (id == layout_item_kind_id(kind)) return kind;
    return std::nullopt;
}

// ------------------------------------------------------------------ Layout ---

const LayoutItem* Layout::find(std::string_view id) const
{
    for (const LayoutItem& item : items)
        if (turkish_key_equals(item.id, id)) return &item;
    return nullptr;
}

LayoutItem* Layout::find(std::string_view id)
{
    return const_cast<LayoutItem*>(std::as_const(*this).find(id));
}

const LayoutItem* Layout::first_map() const
{
    // THE FIRST IN PAINT ORDER, not the first in the array: a user who sent a
    // second map behind the first means the one in front to be the main one, and
    // the print frame aims the main one.
    const LayoutItem* best = nullptr;
    for (const LayoutItem& item : items) {
        if (item.kind != LayoutItemKind::Map) continue;
        if (best == nullptr || item.z > best->z) best = &item;
    }
    return best;
}

// -------------------------------------------------------------- LayoutStore --

const Layout* LayoutStore::find(std::string_view name) const
{
    for (const Layout& l : layouts_)
        if (turkish_key_equals(l.name, name)) return &l;
    return nullptr;
}

Layout* LayoutStore::find(std::string_view name)
{
    return const_cast<Layout*>(std::as_const(*this).find(name));
}

Status LayoutStore::upsert(Layout layout)
{
    if (layout.name.empty()) return err(ErrorCode::InvalidArgument, "Yerleşim adı boş olamaz.");
    if (layout.pages.empty())
        return err(ErrorCode::InvalidArgument,
                   "'" + layout.name + "' yerleşiminin hiç sayfası yok.");
    for (const LayoutPage& page : layout.pages)
        if (page.w <= 0 || page.h <= 0)
            return err(ErrorCode::InvalidArgument,
                       "'" + layout.name +
                           "' yerleşiminin bir sayfasının boyu sıfır ya da negatif.");

    // ITEM IDS ARE UNIQUE INSIDE A LAYOUT, because a command names an item by id
    // and two boxes answering to one name is a command whose meaning depends on
    // the order the file happened to be written in.
    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        if (layout.items[i].id.empty())
            return err(ErrorCode::InvalidArgument,
                       "'" + layout.name + "' yerleşiminde adsız bir öğe var.");
        for (std::size_t j = i + 1; j < layout.items.size(); ++j)
            if (turkish_key_equals(layout.items[i].id, layout.items[j].id))
                return err(ErrorCode::InvalidArgument,
                           "'" + layout.name + "' yerleşiminde iki öğe aynı adı taşıyor: '" +
                               layout.items[i].id + "'.");
    }

    // The page column is filled out rather than refused: a short one is what an
    // older file and a caller that never used a second page both produce.
    layout.item_pages.resize(layout.items.size(), 0);
    for (std::int32_t& page : layout.item_pages)
        if (page < 0 || page >= static_cast<std::int32_t>(layout.pages.size())) page = 0;

    // EVERY LAYOUT GOES THROUGH HERE, so this is where identity is settled: a
    // layout arriving from a file, a template or a JSON script has names and no
    // keys, and one arriving from an edit has keys on most of it. `relink` mints
    // what is missing and makes the name and the key of every link agree.
    layout.relink();

    if (Layout* existing = find(layout.name); existing != nullptr) {
        *existing = std::move(layout);
        return ok();
    }
    layouts_.push_back(std::move(layout));
    return ok();
}

bool LayoutStore::remove(std::string_view name)
{
    const auto at = std::find_if(layouts_.begin(), layouts_.end(), [name](const Layout& l) {
        return turkish_key_equals(l.name, name);
    });
    if (at == layouts_.end()) return false;
    layouts_.erase(at);
    return true;
}

std::uint64_t LayoutStore::fold(std::uint64_t seed) const
{
    std::uint64_t h = seed;
    for (const Layout& l : layouts_) {
        h = fold_text(h, l.name);
        h = fnv1a_int(l.dpi, h);
        h = fnv1a_int(l.margin, h);
        h = fold_text(h, l.paper);
        h = fnv1a_int(l.landscape ? 1 : 0, h);

        for (const LayoutPage& page : l.pages) {
            h = fnv1a_int(page.w, h);
            h = fnv1a_int(page.h, h);
        }

        for (std::size_t i = 0; i < l.items.size(); ++i) {
            const LayoutItem& item = l.items[i];
            h                      = fold_text(h, item.id);
            h                      = fnv1a_int(static_cast<std::int64_t>(item.kind), h);
            h                      = fnv1a_int(item.frame.x, h);
            h                      = fnv1a_int(item.frame.y, h);
            h                      = fnv1a_int(item.frame.w, h);
            h                      = fnv1a_int(item.frame.h, h);
            h                      = fnv1a_int(item.z, h);
            h                      = fnv1a_int(item.locked ? 1 : 0, h);
            h                      = fnv1a_int(item.rotation_udeg, h);
            h                      = fnv1a_int(item.frame_visible ? 1 : 0, h);
            h                      = fnv1a_int(item.frame_width, h);
            h                      = fnv1a_int(item.frame_colour, h);
            h                      = fnv1a_int(item.background ? 1 : 0, h);
            h                      = fnv1a_int(item.background_colour, h);
            h                      = fold_text(h, item.text);
            h                      = fnv1a_int(item.text_height, h);
            h                      = fnv1a_int(item.text_colour, h);
            h                      = fnv1a_int(item.align_h, h);
            h                      = fnv1a_int(item.align_v, h);
            h                      = fnv1a_int(item.extent.min_x, h);
            h                      = fnv1a_int(item.extent.min_y, h);
            h                      = fnv1a_int(item.extent.max_x, h);
            h                      = fnv1a_int(item.extent.max_y, h);
            h                      = fnv1a_int(item.scale, h);
            h                      = fnv1a_int(static_cast<std::int64_t>(item.grid), h);
            h                      = fnv1a_int(static_cast<std::int64_t>(item.grid_labels), h);
            h                      = fnv1a_int(item.grid_interval, h);
            h                      = fnv1a_int(item.grid_width, h);
            h                      = fnv1a_int(item.grid_colour, h);
            h                      = fnv1a_int(item.grid_text_height, h);
            h                      = fnv1a_int(static_cast<std::int64_t>(item.style), h);
            h                      = fnv1a_int(static_cast<std::int64_t>(item.shape), h);
            h                      = fnv1a_int(item.row_limit, h);
            for (const std::string& layer : item.layers)
                h = fold_text(h, layer);
            for (const std::string& column : item.columns)
                h = fold_text(h, column);
            // THE MAP LINK IS CONTENT. Which frame a scale bar states the scale
            // of decides the number that prints; two drawings that print
            // different numbers are not the same drawing, so they may not share a
            // fingerprint.
            h = fold_text(h, item.linked_map);
            h = fnv1a_int(l.page_of(i), h);
        }
    }
    return h;
}

// ------------------------------------------------------------------- JSON ----

namespace {

/// The template format's version. `.claude/io.md` P5: never write a format
/// without one, and that binds every format this program writes rather than only
/// the ones under `/src/io`.
constexpr std::int64_t kTemplateVersion = 1;

Json int_json(std::int64_t v)
{
    return Json::integer(v);
}

} // namespace

std::string layout_to_json(const Layout& layout, std::string_view name)
{
    Json root = Json::object({});
    root.set("surum", int_json(kTemplateVersion));
    root.set("ad", Json::string(std::string(name)));
    root.set("kagit", Json::string(layout.paper));
    root.set("yatay", Json::boolean(layout.landscape));
    root.set("dpi", int_json(layout.dpi));
    root.set("kenar", int_json(layout.margin));

    JsonArray pages;
    for (const LayoutPage& page : layout.pages) {
        Json one = Json::object({});
        one.set("genislik", int_json(page.w));
        one.set("yukseklik", int_json(page.h));
        pages.push_back(std::move(one));
    }
    root.set("sayfalar", Json::array(std::move(pages)));

    JsonArray items;
    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        const LayoutItem& item = layout.items[i];
        Json one               = Json::object({});
        one.set("ad", Json::string(item.id));
        one.set("tur", Json::string(layout_item_kind_id(item.kind)));
        one.set("sayfa", int_json(layout.page_of(i)));
        one.set("x", int_json(item.frame.x));
        one.set("y", int_json(item.frame.y));
        one.set("genislik", int_json(item.frame.w));
        one.set("yukseklik", int_json(item.frame.h));
        one.set("sira", int_json(item.z));
        if (!item.linked_map.empty()) one.set("harita", Json::string(item.linked_map));
        if (item.locked) one.set("kilit", Json::boolean(true));
        if (item.rotation_udeg != 0) one.set("donme", int_json(item.rotation_udeg));
        if (item.frame_visible) one.set("cerceve", Json::boolean(true));
        if (item.frame_width != 0) one.set("cerceve_kalinlik", int_json(item.frame_width));
        one.set("cerceve_renk", int_json(item.frame_colour));
        if (item.background) one.set("dolgu", Json::boolean(true));
        one.set("dolgu_renk", int_json(item.background_colour));
        if (!item.text.empty()) one.set("metin", Json::string(item.text));
        one.set("yazi", int_json(item.text_height));
        one.set("yazi_renk", int_json(item.text_colour));
        one.set("hiza_x", int_json(item.align_h));
        one.set("hiza_y", int_json(item.align_v));

        // NO `kapsam`: a template carries the arrangement, never the ground.
        if (item.scale > 0) one.set("olcek", int_json(item.scale));
        one.set("izgara", int_json(static_cast<std::int64_t>(item.grid)));
        one.set("izgara_etiket", int_json(static_cast<std::int64_t>(item.grid_labels)));
        if (item.grid_interval > 0) one.set("izgara_aralik", int_json(item.grid_interval));
        if (item.grid_width > 0) one.set("izgara_kalinlik", int_json(item.grid_width));
        one.set("izgara_renk", int_json(item.grid_colour));
        one.set("izgara_yazi", int_json(item.grid_text_height));
        if (item.style != 0) one.set("bicim", int_json(item.style));
        one.set("sekil", int_json(static_cast<std::int64_t>(item.shape)));
        if (item.row_limit != 0) one.set("satir_siniri", int_json(item.row_limit));

        if (!item.layers.empty()) {
            JsonArray names;
            for (const std::string& one_layer : item.layers)
                names.push_back(Json::string(one_layer));
            one.set("katmanlar", Json::array(std::move(names)));
        }
        if (!item.columns.empty()) {
            JsonArray names;
            for (const std::string& column : item.columns)
                names.push_back(Json::string(column));
            one.set("sutunlar", Json::array(std::move(names)));
        }
        items.push_back(std::move(one));
    }
    root.set("ogeler", Json::array(std::move(items)));
    return root.dump_pretty();
}

Result<Layout> layout_from_json(std::string_view text, std::string name)
{
    auto parsed = Json::parse(text);
    if (!parsed)
        return err(ErrorCode::ParseError, "Çıktı şablonu okunamadı: " + parsed.error().message);
    const Json& root = parsed.value();
    if (!root.is_object())
        return err(ErrorCode::ParseError, "Çıktı şablonu bir JSON nesnesi değil.");

    const Json* version         = root.find("surum");
    const std::int64_t declared = version != nullptr ? version->as_int() : 0;
    if (declared > kTemplateVersion)
        return err(ErrorCode::Unsupported, "Çıktı şablonu bu sürümden yeni (dosya " +
                                               std::to_string(declared) + ", bu sürüm " +
                                               std::to_string(kTemplateVersion) +
                                               "). Programı güncelleyin.");

    const auto text_of = [](const Json& parent, const char* key) {
        const Json* v = parent.find(key);
        return v != nullptr && v->is_string() ? v->as_string() : std::string();
    };
    const auto int_of = [](const Json& parent, const char* key, std::int64_t fallback = 0) {
        const Json* v = parent.find(key);
        return v != nullptr && v->is_number() ? v->as_int() : fallback;
    };
    const auto bool_of = [](const Json& parent, const char* key) {
        const Json* v = parent.find(key);
        return v != nullptr && v->is_bool() && v->as_bool();
    };

    Layout out;
    out.name      = std::move(name);
    out.paper     = text_of(root, "kagit");
    out.landscape = bool_of(root, "yatay");
    out.dpi       = static_cast<std::int32_t>(int_of(root, "dpi", 300));
    out.margin    = static_cast<Um>(int_of(root, "kenar", um_from_mm(10)));

    out.pages.clear();
    if (const Json* pages = root.find("sayfalar"); pages != nullptr && pages->is_array())
        for (const Json& page : pages->as_array())
            out.pages.push_back(LayoutPage{static_cast<Um>(int_of(page, "genislik")),
                                           static_cast<Um>(int_of(page, "yukseklik"))});
    if (out.pages.empty()) out.pages.push_back(LayoutPage{});

    if (const Json* items = root.find("ogeler"); items != nullptr && items->is_array())
        for (const Json& one : items->as_array()) {
            if (!one.is_object()) continue;
            LayoutItem item;
            item.id = text_of(one, "ad");
            if (item.id.empty())
                return err(ErrorCode::ParseError, "Çıktı şablonunda adsız bir öğe var.");

            const std::optional<LayoutItemKind> kind =
                layout_item_kind_from_id(text_of(one, "tur"));
            if (!kind)
                return err(ErrorCode::Unsupported, "'" + item.id +
                                                       "' öğesinin türü bu sürümde yok: '" +
                                                       text_of(one, "tur") + "'.");
            item.kind = *kind;
            item.frame =
                PaperRect{static_cast<Um>(int_of(one, "x")), static_cast<Um>(int_of(one, "y")),
                          static_cast<Um>(int_of(one, "genislik")),
                          static_cast<Um>(int_of(one, "yukseklik"))};
            item.z             = static_cast<std::int32_t>(int_of(one, "sira"));
            item.linked_map    = text_of(one, "harita");
            item.locked        = bool_of(one, "kilit");
            item.rotation_udeg = static_cast<std::int32_t>(int_of(one, "donme"));
            item.frame_visible = bool_of(one, "cerceve");
            item.frame_width   = static_cast<Um>(int_of(one, "cerceve_kalinlik"));
            item.frame_colour = static_cast<std::uint32_t>(int_of(one, "cerceve_renk", 0xFF000000));
            item.background   = bool_of(one, "dolgu");
            item.background_colour =
                static_cast<std::uint32_t>(int_of(one, "dolgu_renk", 0xFFFFFFFF));
            item.text          = text_of(one, "metin");
            item.text_height   = static_cast<Um>(int_of(one, "yazi", um_from_mm(3)));
            item.text_colour   = static_cast<std::uint32_t>(int_of(one, "yazi_renk", 0xFF000000));
            item.align_h       = static_cast<std::uint8_t>(int_of(one, "hiza_x"));
            item.align_v       = static_cast<std::uint8_t>(int_of(one, "hiza_y"));
            item.scale         = int_of(one, "olcek");
            item.grid          = static_cast<GridStyle>(std::min<std::int64_t>(
                int_of(one, "izgara"), static_cast<std::int64_t>(GridStyle::Tick)));
            item.grid_labels   = static_cast<GridLabels>(std::min<std::int64_t>(
                int_of(one, "izgara_etiket"), static_cast<std::int64_t>(GridLabels::Inside)));
            item.grid_interval = int_of(one, "izgara_aralik");
            item.grid_width    = static_cast<Um>(int_of(one, "izgara_kalinlik"));
            item.grid_colour   = static_cast<std::uint32_t>(int_of(one, "izgara_renk", 0xFF000000));
            item.grid_text_height = static_cast<Um>(int_of(one, "izgara_yazi", um_from_mm(2)));
            item.style            = static_cast<std::int32_t>(int_of(one, "bicim"));
            item.shape            = static_cast<LayoutShape>(std::min<std::int64_t>(
                int_of(one, "sekil"), static_cast<std::int64_t>(LayoutShape::Line)));
            item.row_limit        = static_cast<std::int32_t>(int_of(one, "satir_siniri"));

            if (const Json* names = one.find("katmanlar"); names != nullptr && names->is_array())
                for (const Json& named : names->as_array())
                    if (named.is_string()) item.layers.push_back(named.as_string());
            if (const Json* names = one.find("sutunlar"); names != nullptr && names->is_array())
                for (const Json& named : names->as_array())
                    if (named.is_string()) item.columns.push_back(named.as_string());

            out.items.push_back(std::move(item));
            out.item_pages.push_back(static_cast<std::int32_t>(int_of(one, "sayfa")));
        }

    return out;
}

// ------------------------------------------------------------- map geometry --

std::int64_t map_scale(const LayoutItem& item)
{
    if (item.scale > 0) return item.scale;
    if (item.frame.w <= 0 || item.extent.empty()) return 0;

    // Ground millimetres across the window over PAPER millimetres across the
    // frame. The frame is micrometres, so the paper term is `w / 1000`.
    const double paper_mm  = static_cast<double>(item.frame.w) / 1000.0;
    const double ground_mm = static_cast<double>(item.extent.width());
    if (paper_mm <= 0.0) return 0;
    return static_cast<std::int64_t>(std::llround(ground_mm / paper_mm));
}

Box2 map_window(const LayoutItem& item)
{
    if (item.frame.w <= 0 || item.frame.h <= 0) return item.extent;

    const Point2 centre = item.extent.empty() ? Point2{0, 0} : item.extent.centre();

    if (item.scale > 0) {
        // THE PAPER DECIDES THE GROUND. A frame 180 mm wide at 1:1000 shows
        // 180 000 mm of ground, whatever was stored in `extent` — the extent
        // only says WHERE.
        const auto ground_w = static_cast<Mm>((static_cast<double>(item.frame.w) / 1000.0) *
                                              static_cast<double>(item.scale));
        const auto ground_h = static_cast<Mm>((static_cast<double>(item.frame.h) / 1000.0) *
                                              static_cast<double>(item.scale));
        return Box2{centre.x - ground_w / 2, centre.y - ground_h / 2, centre.x + ground_w / 2,
                    centre.y + ground_h / 2};
    }

    if (item.extent.empty()) return item.extent;

    // NO DECLARED SCALE: keep the ground, widen to the frame's aspect on the
    // shorter side, so the drawing is never stretched to fit a box.
    const double frame_aspect =
        static_cast<double>(item.frame.w) / static_cast<double>(item.frame.h);
    const double window_aspect = static_cast<double>(item.extent.width()) /
                                 static_cast<double>(std::max<Mm>(1, item.extent.height()));

    Mm w = item.extent.width();
    Mm h = item.extent.height();
    if (window_aspect < frame_aspect)
        w = static_cast<Mm>(static_cast<double>(h) * frame_aspect);
    else
        h = static_cast<Mm>(static_cast<double>(w) / frame_aspect);

    return Box2{centre.x - w / 2, centre.y - h / 2, centre.x + w / 2, centre.y + h / 2};
}

// ----------------------------------------------------------- default_layout --

LayoutItem default_item(LayoutItemKind kind)
{
    LayoutItem out;
    out.kind = kind;
    // THE MAP GOES UNDER EVERYTHING ELSE. It is the content; the rest is the
    // apparatus that explains it, and apparatus that hides the content is worse
    // than no apparatus.
    out.z = kind == LayoutItemKind::Map ? 0 : 10;

    switch (kind) {
    case LayoutItemKind::Map:
        out.frame_visible = true;
        out.grid          = GridStyle::Cross;
        out.grid_labels   = GridLabels::Outside;
        break;
    case LayoutItemKind::Legend:
    case LayoutItemKind::Table:
        // OPAQUE, AND FRAMED. Both of these sit over the map by design, and a
        // transparent one is a table whose rows have a parcel boundary drawn
        // through them.
        out.background    = true;
        out.frame_visible = true;
        break;
    case LayoutItemKind::ScaleBar:
        out.style = 4; // four segments
        break;
    case LayoutItemKind::Label:
        out.align_h = 1; // centred
        out.align_v = 1;
        break;
    case LayoutItemKind::NorthArrow:
    case LayoutItemKind::Picture:
    case LayoutItemKind::Shape: break;
    }
    return out;
}

const LayoutItem* Layout::map_for(const LayoutItem& item) const
{
    // THE KEY FIRST. It is the reference that survives a rename; the name is what
    // the file carries and what a person types.
    if (item.linked != LayoutItemKey::None) {
        const LayoutItem* held = find_key(item.linked);
        if (held != nullptr && held->kind == LayoutItemKind::Map) return held;
        return nullptr;
    }
    if (item.linked_map.empty()) return first_map();
    const LayoutItem* named = find(item.linked_map);
    // NAMED BUT NOT THERE IS NOT "the first one". A scale bar quietly restating
    // a different map's scale is a wrong number on a document somebody signs.
    if (named == nullptr || named->kind != LayoutItemKind::Map) return nullptr;
    return named;
}

bool Layout::link_is_broken(const LayoutItem& item) const
{
    return !item.linked_map.empty() && map_for(item) == nullptr;
}

LayoutItem* Layout::find_key(LayoutItemKey wanted)
{
    if (wanted == LayoutItemKey::None) return nullptr;
    for (LayoutItem& item : items)
        if (item.key == wanted) return &item;
    return nullptr;
}

const LayoutItem* Layout::find_key(LayoutItemKey wanted) const
{
    if (wanted == LayoutItemKey::None) return nullptr;
    for (const LayoutItem& item : items)
        if (item.key == wanted) return &item;
    return nullptr;
}

void Layout::relink()
{
    if (next_key == 0) next_key = 1;

    // ---- 1. everything that came in without a key gets one ------------------
    //
    // A layout arrives from a file, a template or a JSON script with names and
    // no keys, and from an edit with keys already on most of it. Both are
    // ordinary; minting only what is missing is what makes them the same case.
    for (LayoutPage& page : pages)
        if (page.key == LayoutPageKey::None) page.key = static_cast<LayoutPageKey>(next_key++);
    for (LayoutItem& item : items)
        if (item.key == LayoutItemKey::None) item.key = static_cast<LayoutItemKey>(next_key++);

    // ---- 2. the two halves of a link are made to agree ----------------------
    //
    // THE KEY WINS when both are set and they disagree, because the key is the
    // one that survived a rename: the name is what the file said when it was
    // written, and the key is what the item is.
    for (LayoutItem& item : items) {
        if (item.linked != LayoutItemKey::None) {
            const LayoutItem* target = find_key(item.linked);
            if (target != nullptr && target->kind == LayoutItemKind::Map) {
                item.linked_map = target->id;
                continue;
            }
            // The key points at something that is gone. Fall back to the name so
            // `link_is_broken` can say so rather than the link vanishing.
            item.linked = LayoutItemKey::None;
        }
        if (item.linked_map.empty()) continue;
        const LayoutItem* named = find(item.linked_map);
        item.linked             = (named != nullptr && named->kind == LayoutItemKind::Map)
                                      ? named->key
                                      : LayoutItemKey::None;
    }
}

bool Layout::rename_item(std::string_view from, std::string to)
{
    LayoutItem* item = find(from);
    if (item == nullptr || to.empty()) return false;
    // A NAME IS UNIQUE INSIDE A LAYOUT, and renaming onto a taken one would make
    // two items answer to one word — which is how the wrong box gets edited.
    if (const LayoutItem* taken = find(to); taken != nullptr && taken != item) return false;

    item->id = std::move(to);
    // AND EVERY LINK STILL POINTS AT IT. This is what the keys are for: the
    // references are by key, and `relink` writes the new name back into the
    // `linked_map` the file will carry.
    relink();
    return true;
}

std::vector<std::string> layout_trouble(const Layout& layout)
{
    std::vector<std::string> out;
    const auto mm_of = [](Um v) { return std::to_string(v / 1000); };

    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        const LayoutItem& item = layout.items[i];
        const std::string who  = "'" + item.id + "'";

        // ---- the box is on the page it claims ------------------------------
        const std::int32_t page = layout.page_of(i);
        if (page < 0 || static_cast<std::size_t>(page) >= layout.pages.size()) {
            out.push_back(who + " var olmayan bir sayfada duruyor.");
            continue;
        }
        const LayoutPage& sheet = layout.pages[static_cast<std::size_t>(page)];

        if (item.frame.w <= 0 || item.frame.h <= 0) {
            out.push_back(who + " kutusunun eni ya da boyu sıfır; hiçbir şey çizilmeyecek.");
        } else if (item.frame.x < 0 || item.frame.y < 0 || item.frame.x + item.frame.w > sheet.w ||
                   item.frame.y + item.frame.h > sheet.h) {
            // A BOX THAT HANGS OFF THE PAGE PRINTS CUT OFF, and the file still
            // appears and still looks finished.
            out.push_back(who + " sayfanın dışına taşıyor (sayfa " + mm_of(sheet.w) + "×" +
                          mm_of(sheet.h) + " mm).");
        }

        // ---- a map frame that was never aimed -------------------------------
        if (item.kind == LayoutItemKind::Map && item.extent.empty())
            out.push_back(who + " bir harita çerçevesi ama nereye bakacağı söylenmemiş; boş "
                                "çıkacak.");

        // ---- a link that no longer resolves ---------------------------------
        if (layout.link_is_broken(item))
            out.push_back(who + " '" + item.linked_map +
                          "' adlı haritaya bağlı ve o harita yok; ölçeği söyleyemez.");

        // ---- a table with no layer to read ----------------------------------
        if (item.kind == LayoutItemKind::Table && item.text.empty())
            out.push_back(who + " bir tablo ama hangi katmanı yazacağı söylenmemiş.");

        // ---- a picture with no file -----------------------------------------
        if (item.kind == LayoutItemKind::Picture && item.text.empty())
            out.push_back(who + " bir resim ama dosya yolu verilmemiş.");
    }

    // ---- a sheet with nothing to show -------------------------------------
    if (layout.first_map() == nullptr)
        out.push_back("'" + layout.name +
                      "' yerleşiminde harita çerçevesi yok; ölçek çubuğu ve "
                      "kuzey oku neyi anlatacağını bilemez.");

    return out;
}

Layout default_layout(std::string name, Um width, Um height, Um margin)
{
    Layout out;
    out.name   = std::move(name);
    out.margin = margin;
    out.pages  = {LayoutPage{width, height}};

    // THE SHEET A NEW LAYOUT STARTS AS, and every number below is a proportion of
    // the page rather than a constant: the same call has to produce a sensible
    // A4 kroki and a sensible A0 sheet, and a 20 mm title block is a banner on
    // one and a whisker on the other.
    const Um title_h = std::max(um_from_mm(8), height / 28);
    const Um bar_h   = std::max(um_from_mm(6), height / 40);
    const Um gap     = std::max(um_from_mm(3), height / 90);

    LayoutItem title  = default_item(LayoutItemKind::Label);
    title.id          = "baslik";
    title.frame       = PaperRect{margin, margin, width - 2 * margin, title_h};
    title.text        = "<yerlesim>";
    title.text_height = std::max(um_from_mm(4), title_h / 2);
    out.items.push_back(std::move(title));

    LayoutItem map = default_item(LayoutItemKind::Map);
    map.id         = "harita";
    map.frame      = PaperRect{margin, margin + title_h + gap, width - 2 * margin,
                          height - 2 * margin - title_h - bar_h - 2 * gap};
    out.items.push_back(std::move(map));

    LayoutItem bar  = default_item(LayoutItemKind::ScaleBar);
    bar.id          = "olcek";
    bar.frame       = PaperRect{margin, height - margin - bar_h, (width - 2 * margin) / 3, bar_h};
    bar.text_height = std::max(um_from_mm(2), bar_h / 3);
    out.items.push_back(std::move(bar));

    LayoutItem north = default_item(LayoutItemKind::NorthArrow);
    north.id         = "kuzey";
    const Um nw      = std::max(um_from_mm(10), width / 22);
    north.frame      = PaperRect{width - margin - nw, height - margin - bar_h - gap - nw, nw, nw};
    out.items.push_back(std::move(north));

    out.item_pages.assign(out.items.size(), 0);
    return out;
}

} // namespace kentos::core
