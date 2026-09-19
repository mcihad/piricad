// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/layout.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>

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
    if (layout.name.empty()) return err(ErrorCode::InvalidArgument, "Pafta adı boş olamaz.");
    if (layout.pages.empty())
        return err(ErrorCode::InvalidArgument, "'" + layout.name + "' paftasının hiç sayfası yok.");
    for (const LayoutPage& page : layout.pages)
        if (page.w <= 0 || page.h <= 0)
            return err(ErrorCode::InvalidArgument,
                       "'" + layout.name + "' paftasının bir sayfasının boyu sıfır ya da negatif.");

    // ITEM IDS ARE UNIQUE INSIDE A LAYOUT, because a command names an item by id
    // and two boxes answering to one name is a command whose meaning depends on
    // the order the file happened to be written in.
    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        if (layout.items[i].id.empty())
            return err(ErrorCode::InvalidArgument,
                       "'" + layout.name + "' paftasında adsız bir öğe var.");
        for (std::size_t j = i + 1; j < layout.items.size(); ++j)
            if (turkish_key_equals(layout.items[i].id, layout.items[j].id))
                return err(ErrorCode::InvalidArgument,
                           "'" + layout.name + "' paftasında iki öğe aynı adı taşıyor: '" +
                               layout.items[i].id + "'.");
    }

    // The page column is filled out rather than refused: a short one is what an
    // older file and a caller that never used a second page both produce.
    layout.item_pages.resize(layout.items.size(), 0);
    for (std::int32_t& page : layout.item_pages)
        if (page < 0 || page >= static_cast<std::int32_t>(layout.pages.size())) page = 0;

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
            h = fnv1a_int(l.page_of(i), h);
        }
    }
    return h;
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
    return static_cast<std::int64_t>(ground_mm / paper_mm + 0.5);
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

Layout default_layout(std::string name, Um width, Um height, Um margin)
{
    Layout out;
    out.name   = std::move(name);
    out.margin = margin;
    out.pages  = {LayoutPage{width, height}};

    // THE SHEET A NEW PAFTA STARTS AS, and every number below is a proportion of
    // the page rather than a constant: the same call has to produce a sensible
    // A4 kroki and a sensible A0 pafta, and a 20 mm title block is a banner on
    // one and a whisker on the other.
    const Um title_h = std::max(um_from_mm(8), height / 28);
    const Um bar_h   = std::max(um_from_mm(6), height / 40);
    const Um gap     = std::max(um_from_mm(3), height / 90);

    LayoutItem title;
    title.id          = "baslik";
    title.kind        = LayoutItemKind::Label;
    title.frame       = PaperRect{margin, margin, width - 2 * margin, title_h};
    title.z           = 10;
    title.text        = "<pafta>";
    title.text_height = std::max(um_from_mm(4), title_h / 2);
    title.align_h     = 1; // centred
    title.align_v     = 1;
    out.items.push_back(std::move(title));

    LayoutItem map;
    map.id            = "harita";
    map.kind          = LayoutItemKind::Map;
    map.frame         = PaperRect{margin, margin + title_h + gap, width - 2 * margin,
                          height - 2 * margin - title_h - bar_h - 2 * gap};
    map.z             = 0;
    map.frame_visible = true;
    map.grid          = GridStyle::Cross;
    map.grid_labels   = GridLabels::Outside;
    out.items.push_back(std::move(map));

    LayoutItem bar;
    bar.id          = "olcek";
    bar.kind        = LayoutItemKind::ScaleBar;
    bar.frame       = PaperRect{margin, height - margin - bar_h, (width - 2 * margin) / 3, bar_h};
    bar.z           = 10;
    bar.style       = 4; // four segments
    bar.text_height = std::max(um_from_mm(2), bar_h / 3);
    out.items.push_back(std::move(bar));

    LayoutItem north;
    north.id    = "kuzey";
    north.kind  = LayoutItemKind::NorthArrow;
    const Um nw = std::max(um_from_mm(10), width / 22);
    north.frame = PaperRect{width - margin - nw, height - margin - bar_h - gap - nw, nw, nw};
    north.z     = 10;
    out.items.push_back(std::move(north));

    out.item_pages.assign(out.items.size(), 0);
    return out;
}

} // namespace kentos::core
