// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_render.hpp"

#include "kentos_cad/app/backend_factory.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/render/backend.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>

#include <algorithm>
#include <cmath>

namespace kentos::app {
namespace {

constexpr double kMmPerInch = 25.4;

/// Paper micrometres to paper millimetres.
double mm_of(core::Um um)
{
    return static_cast<double>(um) / 1000.0;
}

/// An `0xAARRGGBB` as Qt reads it.
QColor colour_of(std::uint32_t argb)
{
    return QColor::fromRgba(argb);
}

/// The item's frame inside `page_rect`, which is the page in device pixels.
QRectF frame_in(const QRectF& page_rect, const core::LayoutPage& page, const core::PaperRect& frame)
{
    const double sx = page_rect.width() / std::max(1.0, static_cast<double>(page.w));
    const double sy = page_rect.height() / std::max(1.0, static_cast<double>(page.h));
    return QRectF(page_rect.left() + frame.x * sx, page_rect.top() + frame.y * sy, frame.w * sx,
                  frame.h * sy);
}

/// A font at `height_um` cap height, in the device's pixels.
QFont font_at(core::Um height_um, double px_per_paper_mm)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    const double px = std::max(1.0, mm_of(height_um) * px_per_paper_mm);
    f.setPixelSize(static_cast<int>(std::lround(px)));
    return f;
}

Qt::Alignment alignment_of(const core::LayoutItem& item)
{
    Qt::Alignment out = Qt::AlignLeft;
    if (item.align_h == 1) out = Qt::AlignHCenter;
    if (item.align_h == 2) out = Qt::AlignRight;
    if (item.align_v == 0) out |= Qt::AlignTop;
    if (item.align_v == 1) out |= Qt::AlignVCenter;
    if (item.align_v == 2) out |= Qt::AlignBottom;
    return out;
}

/// A grid interval that gives a readable number of lines at this scale.
///
/// 1-2-5 STEPS, which is what every map grid and every chart axis has used
/// since paper: an interval of 37 m is a grid nobody can read a coordinate off.
core::Mm grid_step_for(core::Mm ground_width)
{
    if (ground_width <= 0) return 0;
    // Aim for six to ten lines across the frame.
    const double target = static_cast<double>(ground_width) / 8.0;
    double step         = 1.0;
    while (step < target) {
        if (step * 2.0 >= target) {
            step *= 2.0;
            break;
        }
        if (step * 5.0 >= target) {
            step *= 5.0;
            break;
        }
        step *= 10.0;
    }
    return static_cast<core::Mm>(step);
}

/// Ground millimetres printed the way a coordinate is read: whole metres.
QString metres_of(core::Mm value)
{
    return QString::number(static_cast<double>(value) / 1000.0, 'f', 0);
}

// ---------------------------------------------------------------- the map ---

void paint_map(QPainter& painter, const QRectF& box, const core::Document& document,
               const core::LayoutItem& item, double px_per_paper_mm)
{
    if (box.width() < 1.0 || box.height() < 1.0) return;

    const core::Box2 window = core::map_window(item);

    const int w_px = std::max(1, static_cast<int>(std::lround(box.width())));
    const int h_px = std::max(1, static_cast<int>(std::lround(box.height())));

    // INTO AN IMAGE AND THEN BLITTED, rather than painted straight onto the
    // page: the render pipeline owns its whole target — it fills a background
    // and it clips nothing — so painting it into the page's painter would wipe
    // whatever is already under the frame and spill outside it.
    QImage tile(w_px, h_px, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::white);

    if (!window.empty()) {
        render::ViewTransform view;
        view.set_viewport(w_px, h_px);
        view.fit(window, 0.0);

        render::SceneOptions options;
        options.pixels_per_paper_mm = px_per_paper_mm;
        options.cull                = true;
        options.lod                 = true;
        options.line_weights        = true;

        // THE FRAME'S OWN LAYERS, which the model has carried all along and the
        // renderer was never told about: `item.layers` reached no scene option,
        // so "this map draws these layers" was a field a user could set and a
        // sheet would ignore. Empty still means every visible layer.
        //
        // Built as a byte per layer, because the scene reads it once per entity
        // and a name compare there would sit inside the frame budget (§10.1).
        std::vector<std::uint8_t> allowed;
        if (!item.layers.empty()) {
            allowed.assign(document.layers().size(), 0);
            for (const std::string& wanted : item.layers)
                for (std::size_t i = 0; i < document.layers().size(); ++i)
                    if (core::turkish_key_equals(document.layers()[i].name, wanted)) allowed[i] = 1;
            options.layer_allowed = allowed;
        }

        render::DrawList list;
        render::build_scene(document, view, options, list);

        render::Overlay overlay;
        overlay.background_rgba = 0xFFFFFFFFu;

        render::FrameContext ctx;
        ctx.width_px           = w_px;
        ctx.height_px          = h_px;
        ctx.device_pixel_ratio = 1.0f;
        ctx.target             = static_cast<QPaintDevice*>(&tile);

        const std::unique_ptr<render::Backend> backend = make_preview_backend();
        backend->render(list, overlay, ctx);
    }

    painter.drawImage(box.topLeft(), tile);

    // ---- the coordinate grid -------------------------------------------------
    if (item.grid == core::GridStyle::None || window.empty()) return;

    const core::Mm step =
        item.grid_interval > 0 ? item.grid_interval : grid_step_for(window.width());
    if (step <= 0) return;

    painter.save();
    painter.setClipRect(box);
    const double width_px =
        std::max(0.6, mm_of(item.grid_width > 0 ? item.grid_width : 100) * px_per_paper_mm);
    painter.setPen(QPen(colour_of(item.grid_colour), width_px));

    const double sx   = box.width() / static_cast<double>(std::max<core::Mm>(1, window.width()));
    const double sy   = box.height() / static_cast<double>(std::max<core::Mm>(1, window.height()));
    const double tick = std::min(box.width(), box.height()) * 0.02;

    QFont labels = font_at(item.grid_text_height, px_per_paper_mm);
    const QFontMetricsF metrics(labels);

    // THE FIRST LINE AT OR AFTER THE WINDOW'S EDGE, on a round multiple of the
    // step: a grid whose lines fall on 483 217 m is a grid that measures nothing.
    const auto first = [step](core::Mm from) { return ((from + step - 1) / step) * step; };

    for (core::Mm gx = first(window.min_x); gx <= window.max_x; gx += step) {
        const double px = box.left() + static_cast<double>(gx - window.min_x) * sx;
        switch (item.grid) {
        case core::GridStyle::Line:
            painter.drawLine(QPointF(px, box.top()), QPointF(px, box.bottom()));
            break;
        case core::GridStyle::Tick:
            painter.drawLine(QPointF(px, box.top()), QPointF(px, box.top() + tick));
            painter.drawLine(QPointF(px, box.bottom() - tick), QPointF(px, box.bottom()));
            break;
        case core::GridStyle::Cross:
            for (core::Mm gy = first(window.min_y); gy <= window.max_y; gy += step) {
                // THE Y FLIP IS HERE, and only here: ground northing rises and
                // paper falls (core/layout.hpp on `PaperRect`).
                const double py = box.bottom() - static_cast<double>(gy - window.min_y) * sy;
                painter.drawLine(QPointF(px - tick / 2, py), QPointF(px + tick / 2, py));
                painter.drawLine(QPointF(px, py - tick / 2), QPointF(px, py + tick / 2));
            }
            break;
        case core::GridStyle::None: break;
        }
    }

    if (item.grid != core::GridStyle::Cross) {
        for (core::Mm gy = first(window.min_y); gy <= window.max_y; gy += step) {
            const double py = box.bottom() - static_cast<double>(gy - window.min_y) * sy;
            if (item.grid == core::GridStyle::Line)
                painter.drawLine(QPointF(box.left(), py), QPointF(box.right(), py));
            else {
                painter.drawLine(QPointF(box.left(), py), QPointF(box.left() + tick, py));
                painter.drawLine(QPointF(box.right() - tick, py), QPointF(box.right(), py));
            }
        }
    }

    // ---- the coordinates, in their own pass ---------------------------------
    //
    // A LABEL DOES NOT DEPEND ON THE GRID'S STYLE. It used to: the two label
    // passes were guarded by `grid != Cross`, and `default_item` gives a fresh
    // map `Cross` with `Outside` labels — so the default sheet asked for numbers
    // the renderer had decided not to draw. A cross grid with coordinates down
    // the margins is exactly what a cadastral sheet looks like; the crosses mark
    // the intersections and the numbers say which ones.
    if (item.grid_labels != core::GridLabels::None) {
        painter.save();
        painter.setClipping(false);
        painter.setFont(labels);
        painter.setPen(QPen(colour_of(item.grid_colour)));

        for (core::Mm gx = first(window.min_x); gx <= window.max_x; gx += step) {
            const double px    = box.left() + static_cast<double>(gx - window.min_x) * sx;
            const QString text = metres_of(gx);
            const double ty    = item.grid_labels == core::GridLabels::Outside
                                     ? box.top() - metrics.descent() - 1.0
                                     : box.top() + metrics.height();
            painter.drawText(QPointF(px - metrics.horizontalAdvance(text) / 2.0, ty), text);
        }
        for (core::Mm gy = first(window.min_y); gy <= window.max_y; gy += step) {
            const double py    = box.bottom() - static_cast<double>(gy - window.min_y) * sy;
            const QString text = metres_of(gy);
            const double tx    = item.grid_labels == core::GridLabels::Outside
                                     ? box.left() - metrics.horizontalAdvance(text) - 2.0
                                     : box.left() + 2.0;
            painter.drawText(QPointF(tx, py + metrics.ascent() / 2.0), text);
        }
        painter.restore();
    }
    painter.restore();
}

// ---------------------------------------------------------- the scale bar ---

void paint_scale_bar(QPainter& painter, const QRectF& box, const core::LayoutItem& item,
                     std::int64_t denominator, double px_per_paper_mm)
{
    if (denominator <= 0 || box.width() < 2.0) return;

    const int segments = std::clamp(item.style > 0 ? item.style : 4, 1, 10);

    // A ROUND GROUND LENGTH PER SEGMENT, not the box divided by four: a bar
    // whose segment is 137 m is a bar nobody can measure with. The box is the
    // most it may take; a round bar is shorter, and shorter is correct.
    const double paper_mm_available  = mm_of(item.frame.w);
    const double ground_mm_available = paper_mm_available * static_cast<double>(denominator);
    const core::Mm per_segment =
        grid_step_for(static_cast<core::Mm>(ground_mm_available / segments * 8.0));
    if (per_segment <= 0) return;

    const double px_per_ground_mm = box.width() / std::max(1.0, ground_mm_available);
    const double segment_px       = static_cast<double>(per_segment) * px_per_ground_mm;
    if (segment_px < 1.0) return;

    const double bar_h = box.height() * 0.45;
    const double top   = box.top() + box.height() * 0.25;

    painter.save();
    painter.setPen(QPen(colour_of(item.text_colour), std::max(0.6, 0.15 * px_per_paper_mm)));
    for (int i = 0; i < segments; ++i) {
        const QRectF cell(box.left() + i * segment_px, top, segment_px, bar_h);
        // ALTERNATING FILL, the convention every printed map uses: a plain
        // outlined bar is read as one length rather than as a ruler.
        painter.setBrush(i % 2 == 0 ? QBrush(colour_of(item.text_colour)) : QBrush(Qt::white));
        painter.drawRect(cell);
    }

    const QFont labels = font_at(item.text_height, px_per_paper_mm);
    const QFontMetricsF metrics(labels);
    painter.setFont(labels);
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i <= segments; ++i) {
        const QString text = metres_of(per_segment * i);
        const double x     = box.left() + i * segment_px;
        painter.drawText(QPointF(x - metrics.horizontalAdvance(text) / 2.0,
                                 top + bar_h + metrics.ascent() + 1.0),
                         text);
    }
    // AFTER THE LAST TICK LABEL, not after the bar. The tick is drawn CENTRED on
    // the bar's end, so half of "80" hangs past it — and the unit started four
    // pixels past the bar, printing "80m 1:550" on top of itself.
    const QString last = metres_of(per_segment * segments);
    const double last_end =
        box.left() + segments * segment_px + metrics.horizontalAdvance(last) / 2.0;
    const QString unit = QStringLiteral(" m   1:%1").arg(denominator);
    painter.drawText(QPointF(last_end + metrics.horizontalAdvance(QStringLiteral(" ")),
                             top + bar_h + metrics.ascent() + 1.0),
                     unit);
    painter.restore();
}

// --------------------------------------------------------- the north arrow --

void paint_north(QPainter& painter, const QRectF& box, const core::LayoutItem& item,
                 double px_per_paper_mm)
{
    const double side = std::min(box.width(), box.height());
    if (side < 2.0) return;

    const QPointF centre(box.center());
    const double r = side * 0.42;

    painter.save();
    painter.setPen(QPen(colour_of(item.text_colour), std::max(0.6, 0.2 * px_per_paper_mm)));

    // A KITE, HALF FILLED: the shape a surveyor reads as north without a legend,
    // and the half fill is what says which side is the point.
    QPainterPath left;
    left.moveTo(centre.x(), centre.y() - r);
    left.lineTo(centre.x() - r * 0.45, centre.y() + r * 0.75);
    left.lineTo(centre.x(), centre.y() + r * 0.25);
    left.closeSubpath();
    QPainterPath right;
    right.moveTo(centre.x(), centre.y() - r);
    right.lineTo(centre.x() + r * 0.45, centre.y() + r * 0.75);
    right.lineTo(centre.x(), centre.y() + r * 0.25);
    right.closeSubpath();

    painter.setBrush(colour_of(item.text_colour));
    painter.drawPath(left);
    painter.setBrush(Qt::white);
    painter.drawPath(right);

    const QFont label = font_at(item.text_height > 0 ? item.text_height : 3000, px_per_paper_mm);
    const QFontMetricsF metrics(label);
    painter.setFont(label);
    painter.setBrush(Qt::NoBrush);
    painter.drawText(QPointF(centre.x() - metrics.horizontalAdvance(QStringLiteral("K")) / 2.0,
                             centre.y() - r - 2.0),
                     QStringLiteral("K"));
    painter.restore();
}

// -------------------------------------------------------------- the legend --

void paint_legend(QPainter& painter, const QRectF& box, const core::Document& document,
                  const core::LayoutItem& item, double px_per_paper_mm)
{
    const QFont font = font_at(item.text_height, px_per_paper_mm);
    const QFontMetricsF metrics(font);
    const double row    = metrics.height() * 1.35;
    const double swatch = metrics.height() * 0.9;

    painter.save();
    painter.setClipRect(box);
    painter.setFont(font);

    double y = box.top() + metrics.ascent();
    if (!item.text.empty()) {
        painter.setPen(colour_of(item.text_colour));
        painter.drawText(QPointF(box.left(), y), QString::fromStdString(item.text));
        y += row;
    }

    // THE LAYERS THE MAP ACTUALLY DRAWS, in the drawing's own order: a legend
    // listing a layer nobody can see on the sheet is worse than no legend.
    for (const core::Layer& layer : document.layers()) {
        if (!layer.visible) continue;
        if (!item.layers.empty()) {
            bool wanted = false;
            for (const std::string& named : item.layers)
                if (core::turkish_key_equals(named, layer.name)) wanted = true;
            if (!wanted) continue;
        }
        if (y > box.bottom()) break;

        const QRectF chip(box.left(), y - metrics.ascent() * 0.85, swatch, swatch);
        painter.setPen(QPen(QColor::fromRgba(layer.appearance.rgba), 0.8));
        painter.setBrush(QColor::fromRgba(layer.appearance.rgba));
        painter.drawRect(chip);

        painter.setPen(colour_of(item.text_colour));
        painter.setBrush(Qt::NoBrush);
        painter.drawText(QPointF(box.left() + swatch * 1.6, y), QString::fromStdString(layer.name));
        y += row;
    }
    painter.restore();
}

// --------------------------------------------------------------- the table --

void paint_table(QPainter& painter, const QRectF& box, const core::Document& document,
                 const core::LayoutItem& item, double px_per_paper_mm)
{
    const QFont font = font_at(item.text_height, px_per_paper_mm);
    const QFontMetricsF metrics(font);
    const double row_h = metrics.height() * 1.4;

    painter.save();
    painter.setClipRect(box);
    painter.setFont(font);

    const core::AttrTable& table = document.attributes();
    const std::string layer      = item.text;

    // WHICH COLUMNS. The item's own list when it names one, otherwise every
    // column the layer offers — which is what `attr_applies_to` answers, and the
    // same question the attribute table asks (`attribute_table.cpp`).
    std::vector<core::AttrId> columns;
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const core::AttrColumn* held = table.column(static_cast<core::AttrId>(c));
        if (held == nullptr) continue;
        if (!layer.empty() && !core::attr_applies_to(held->spec(), layer)) continue;
        if (!item.columns.empty()) {
            bool wanted = false;
            for (const std::string& named : item.columns)
                if (core::turkish_key_equals(named, held->spec().id)) wanted = true;
            if (!wanted) continue;
        }
        columns.push_back(static_cast<core::AttrId>(c));
    }

    if (columns.empty()) {
        painter.setPen(QPen(colour_of(item.text_colour), 0.8, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box);
        painter.drawText(box, Qt::AlignCenter | Qt::TextWordWrap,
                         layer.empty() ? QObject::tr("tablo: katman seçilmedi")
                                       : QObject::tr("tablo: '%1' katmanında sütun yok")
                                             .arg(QString::fromStdString(layer)));
        painter.restore();
        return;
    }

    const double col_w = box.width() / static_cast<double>(columns.size());
    painter.setPen(colour_of(item.text_colour));

    // ---- the head ----
    double y = box.top();
    painter.save();
    QFont head = font;
    head.setWeight(QFont::DemiBold);
    painter.setFont(head);
    for (std::size_t c = 0; c < columns.size(); ++c) {
        const core::AttrColumn* held = table.column(columns[c]);
        const QRectF cell(box.left() + static_cast<double>(c) * col_w, y, col_w, row_h);
        painter.drawText(cell.adjusted(2, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         metrics.elidedText(QString::fromStdString(held->spec().name_tr),
                                            Qt::ElideRight, col_w - 4));
    }
    painter.restore();
    y += row_h;
    painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));

    // ---- the rows, in slot order, from the layer ----
    const core::LayerId only = layer.empty() ? core::kNoLayer : document.find_layer(layer);
    const std::int32_t cap =
        item.row_limit > 0 ? item.row_limit
                           : static_cast<std::int32_t>((box.bottom() - y) / std::max(1.0, row_h));

    std::int32_t written = 0;
    std::size_t skipped  = 0;
    for (core::EntityId slot = 0; slot < document.entities().size(); ++slot) {
        if (!document.alive(slot)) continue;
        if (!layer.empty() && (only == core::kNoLayer || document.entities().layer[slot] != only))
            continue;
        if (written >= cap || y + row_h > box.bottom()) {
            ++skipped;
            continue;
        }

        for (std::size_t c = 0; c < columns.size(); ++c) {
            const core::Result<core::AttrValue> cell =
                table.get(columns[c], document.entities().slot[slot]);
            const QString text =
                cell.ok() ? QString::fromStdString(core::attr_display(cell.value())) : QString();
            const QRectF at(box.left() + static_cast<double>(c) * col_w, y, col_w, row_h);
            painter.drawText(at.adjusted(2, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft,
                             metrics.elidedText(text, Qt::ElideRight, col_w - 4));
        }
        y += row_h;
        ++written;
    }

    // A TRUNCATED TABLE SAYS SO. A sheet that quietly showed the first eleven of
    // ninety parcels would be a sheet somebody files believing it is complete.
    if (skipped > 0 && y + row_h <= box.bottom() + row_h) {
        painter.save();
        QFont note = font;
        note.setItalic(true);
        painter.setFont(note);
        painter.drawText(QRectF(box.left(), y, box.width(), row_h).adjusted(2, 0, -2, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QObject::tr("… %1 satır daha sığmadı").arg(skipped));
        painter.restore();
    }
    painter.restore();
}

} // namespace

QString resolve_placeholders(const QString& text, const core::Layout& layout,
                             const core::LayoutItem* map, const LayoutFacts& facts)
{
    QString out = text;
    out.replace(QStringLiteral("<yerlesim>"), facts.sheet);
    // The retired spelling, still sitting in every title block written before
    // the rename. A document is data on disk; it does not get to be wrong
    // because the program changed its mind about a word.
    out.replace(QStringLiteral("<pafta>"), facts.sheet);
    out.replace(QStringLiteral("<proje>"), facts.project);
    out.replace(QStringLiteral("<crs>"), facts.crs);
    out.replace(QStringLiteral("<tarih>"), facts.date);
    out.replace(QStringLiteral("<kagit>"), QString::fromStdString(layout.paper));

    const std::int64_t denominator = map != nullptr ? core::map_scale(*map) : 0;
    out.replace(QStringLiteral("<olcek>"),
                denominator > 0 ? QStringLiteral("1:%1").arg(denominator) : QString());
    return out;
}

void paint_layout_page(QPainter& painter, const QRectF& target, const core::Document& document,
                       const core::Layout& layout, int page, double dpi, const LayoutFacts& facts,
                       bool margin_guide)
{
    if (layout.pages.empty() || target.width() < 1.0 || target.height() < 1.0) return;
    const std::size_t index = static_cast<std::size_t>(
        std::clamp<int>(page, 0, static_cast<int>(layout.pages.size()) - 1));
    const core::LayoutPage& sheet = layout.pages[index];

    const double px_per_paper_mm = dpi / kMmPerInch;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(target, Qt::white);

    // PAINT ORDER IS `z`, NOT ARRAY ORDER (core/layout.hpp): a user who sent the
    // map behind the title block means it to stay there whatever the file says.
    std::vector<std::size_t> ordered;
    for (std::size_t i = 0; i < layout.items.size(); ++i)
        if (layout.page_of(i) == static_cast<std::int32_t>(index)) ordered.push_back(i);
    std::stable_sort(ordered.begin(), ordered.end(), [&](std::size_t a, std::size_t b) {
        return layout.items[a].z < layout.items[b].z;
    });

    const core::LayoutItem* first_map = layout.first_map();

    for (const std::size_t at : ordered) {
        const core::LayoutItem* item = &layout.items[at];
        const QRectF box             = frame_in(target, sheet, item->frame);
        if (box.width() < 0.5 || box.height() < 0.5) continue;

        painter.save();
        if (item->rotation_udeg != 0) {
            painter.translate(box.center());
            painter.rotate(static_cast<double>(item->rotation_udeg) / 1000000.0);
            painter.translate(-box.center());
        }
        if (item->background) painter.fillRect(box, colour_of(item->background_colour));

        switch (item->kind) {
        case core::LayoutItemKind::Map:
            paint_map(painter, box, document, *item, px_per_paper_mm);
            break;

        case core::LayoutItemKind::Label: {
            painter.setPen(colour_of(item->text_colour));
            painter.setFont(font_at(item->text_height, px_per_paper_mm));
            painter.drawText(
                box, static_cast<int>(alignment_of(*item) | Qt::TextWordWrap),
                resolve_placeholders(QString::fromStdString(item->text), layout, first_map, facts));
            break;
        }

        case core::LayoutItemKind::ScaleBar:
            paint_scale_bar(painter, box, *item,
                            first_map != nullptr ? core::map_scale(*first_map) : 0,
                            px_per_paper_mm);
            break;

        case core::LayoutItemKind::NorthArrow:
            paint_north(painter, box, *item, px_per_paper_mm);
            break;

        case core::LayoutItemKind::Legend:
            paint_legend(painter, box, document, *item, px_per_paper_mm);
            break;

        case core::LayoutItemKind::Picture: {
            const QString path = QString::fromStdString(item->text);
            QImage picture(path);
            if (picture.isNull()) {
                // A MISSING PICTURE IS SAID, not skipped: an empty rectangle on
                // a signed sheet is a question nobody can answer months later.
                painter.setPen(QPen(colour_of(item->text_colour), 0.8, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(box);
                painter.setFont(font_at(item->text_height, px_per_paper_mm));
                painter.drawText(
                    box, Qt::AlignCenter | Qt::TextWordWrap,
                    path.isEmpty()
                        ? QObject::tr("resim yolu boş")
                        : QObject::tr("resim bulunamadı: %1").arg(QFileInfo(path).fileName()));
            } else {
                painter.drawImage(box, picture.scaled(box.size().toSize(), Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation));
            }
            break;
        }

        case core::LayoutItemKind::Shape: {
            painter.setPen(
                QPen(colour_of(item->frame_colour),
                     std::max(0.6, mm_of(item->frame_width > 0 ? item->frame_width : 200) *
                                       px_per_paper_mm)));
            painter.setBrush(item->background ? QBrush(colour_of(item->background_colour))
                                              : QBrush(Qt::NoBrush));
            if (item->shape == core::LayoutShape::Rectangle)
                painter.drawRect(box);
            else if (item->shape == core::LayoutShape::Ellipse)
                painter.drawEllipse(box);
            else
                painter.drawLine(box.topLeft(), box.bottomRight());
            break;
        }

        case core::LayoutItemKind::Table:
            paint_table(painter, box, document, *item, px_per_paper_mm);
            break;
        }

        if (item->frame_visible && item->kind != core::LayoutItemKind::Shape) {
            painter.setPen(
                QPen(colour_of(item->frame_colour),
                     std::max(0.6, mm_of(item->frame_width > 0 ? item->frame_width : 200) *
                                       px_per_paper_mm)));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(box);
        }
        painter.restore();
    }

    if (margin_guide && layout.margin > 0) {
        const core::PaperRect inside{layout.margin, layout.margin, sheet.w - 2 * layout.margin,
                                     sheet.h - 2 * layout.margin};
        painter.setPen(QPen(QColor(0, 0, 0, 40), 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(frame_in(target, sheet, inside));
    }
    painter.restore();
}

} // namespace kentos::app
