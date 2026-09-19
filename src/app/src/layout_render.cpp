// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_render.hpp"

#include "kentos_cad/app/backend_factory.hpp"
#include "kentos_cad/app/symbol_preview.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/render/backend.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <QDir>
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

/// What a paper millimetre is worth on a 96 dpi screen.
///
/// THE UNIT THE LEGEND'S CHIPS ARE SCALED AGAINST. `symbol_preview` renders at a
/// device pixel ratio; on paper the ratio that matters is how much finer the
/// sheet is than a screen, so a hatch's lines survive the trip to 1200 dpi
/// instead of arriving as a blurred square beside crisp text (L-07).
constexpr double kScreenPxPerPaperMm = 96.0 / kMmPerInch;

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

    // STRAIGHT ONTO THE PAGE, as vectors.
    //
    // It used to render into a `QImage` and blit that, because the pipeline
    // owned its whole target: it filled a background and it clipped nothing, so
    // painting into the page's painter would have wiped what was under the frame
    // and spilled outside it. The cost was that the map reached a PDF as ONE
    // PHOTOGRAPH — a 1:1000 parcel boundary arrived as pixels, unmeasurable and
    // unselectable, on a document a licensed engineer signs (TODOS L-12).
    //
    // Both reasons are answered rather than worked around: the clip is set here
    // and a zero-alpha background tells the backend to leave what is already
    // there. The white page is already under the frame.
    painter.fillRect(box, Qt::white);

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
        // ZERO ALPHA: "leave what is already there". The page's white is under
        // the frame and a clear here would cover whatever else the sheet has put
        // down.
        overlay.background_rgba = 0;

        render::FrameContext ctx;
        ctx.width_px           = w_px;
        ctx.height_px          = h_px;
        ctx.device_pixel_ratio = 1.0f;

        // THE CLIP IS THE FRAME, and the origin is its top-left: the backend
        // centres the drawing on `width_px/2, height_px/2` of whatever space it
        // is painting in, so the translate is what puts that centre in the box.
        painter.save();
        painter.setClipRect(box);
        painter.translate(box.topLeft());
        ctx.target            = static_cast<QPainter*>(&painter);
        ctx.target_is_painter = true;

        const std::unique_ptr<render::Backend> backend = make_preview_backend();
        backend->render(list, overlay, ctx);
        painter.restore();
    }

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

        // THE SYMBOL THE MAP ACTUALLY DRAWS, through the SAME backend the canvas
        // and the layer tree use (`symbol_preview`). It was a flat colour chip,
        // which is a legend that says "parcels are orange" about a layer drawn
        // with a hatch, a dashed boundary and a marker — a key that does not
        // match its own map is worse than no key, and this is a document
        // somebody signs (TODOS L-07).
        //
        // WHAT THE ENTITIES CARRY, the layer's own default otherwise — the same
        // resolution `LayerPanel::layerIcon` makes, so the shelf, the canvas and
        // the printed key cannot disagree.
        const core::StyleId style =
            layer.style != core::kByLayerStyle ? layer.style : core::kByLayerStyle;
        const core::Symbol symbol =
            document.styles().contains(style) && style != core::kByLayerStyle
                ? document.styles().symbol_at(style)
                : core::Symbol::of(layer.appearance);

        // PAINTED INTO THE SHEET'S OWN PAINTER, never blitted as an image. A
        // key drawn as a picture reaches the PDF as pixels — unmeasurable,
        // unselectable and resolution-bound — which is precisely the defect the
        // map frame was fixed for (`FrameContext::target_is_painter`, L-12).
        // The first version of this legend did blit, and the PDF grew an
        // `/Subtype /Image` for every row.
        if (!symbol.layers.empty()) {
            paint_symbol(painter, chip, symbol, document.images(), document.dashes(),
                         PreviewShape::Area, std::max(1.0, px_per_paper_mm / kScreenPxPerPaperMm));
        } else {
            // AN HONEST FALLBACK rather than a blank: a symbol that declares
            // nothing still gets its layer's colour, which is what the drawing
            // will use for it.
            painter.setPen(QPen(QColor::fromRgba(layer.appearance.rgba), 0.8));
            painter.setBrush(QColor::fromRgba(layer.appearance.rgba));
            painter.drawRect(chip);
        }

        painter.setPen(colour_of(item.text_colour));
        painter.setBrush(Qt::NoBrush);
        painter.drawText(QPointF(box.left() + swatch * 1.6, y), QString::fromStdString(layer.name));
        y += row;
    }
    painter.restore();
}

// --------------------------------------------------------------- the chart --

/// Draws a bar chart of how many objects carry each value of one column.
///
/// WHAT IT CHARTS AND WHY THAT ONE. `item.text` names the layer, `item.columns`
/// its first entry names the column. Each distinct value is a bar whose height is
/// the COUNT of objects carrying it — how many parcels are `Arsa`, how many
/// `Tarla`. That is the summary a planning sheet actually shows, and it is the
/// one that needs no surveyed area and therefore cannot be mistaken for one
/// (TODOS L-09).
///
/// AN UNUSABLE SOURCE IS SAID, NOT DRAWN AS AN EMPTY BOX. L-09 requires that an
/// unsupported source never be reported as success with an empty picture: a chart
/// with no layer, no column or no rows prints the reason on the paper and puts it
/// in `trouble`, so the preflight and the export result both carry it. An empty
/// rectangle on a signed sheet is a question nobody can answer months later.
void paint_chart(QPainter& painter, const QRectF& box, const core::Document& document,
                 std::vector<std::string>* trouble, const core::LayoutItem& item,
                 double px_per_paper_mm)
{
    const QFont font = font_at(item.text_height, px_per_paper_mm);
    const QFontMetricsF metrics(font);

    painter.save();
    painter.setClipRect(box);
    painter.setFont(font);

    const auto refuse = [&](const QString& why) {
        if (trouble != nullptr) trouble->push_back("'" + item.id + "' " + why.toStdString());
        painter.setPen(QPen(colour_of(item.text_colour), 0.8, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box);
        painter.setPen(colour_of(item.text_colour));
        painter.drawText(box, Qt::AlignCenter | Qt::TextWordWrap, why);
        painter.restore();
    };

    if (item.text.empty()) {
        refuse(QObject::tr("grafik: hangi katman olduğu söylenmemiş"));
        return;
    }
    const core::LayerId on = document.find_layer(item.text);
    if (on == core::kNoLayer) {
        refuse(QObject::tr("grafik: '%1' adlı katman yok").arg(QString::fromStdString(item.text)));
        return;
    }
    if (item.columns.empty()) {
        refuse(QObject::tr("grafik: hangi sütuna göre sayılacağı söylenmemiş"));
        return;
    }
    const core::AttrId column = document.attributes().find(item.columns.front());
    if (column == core::kNoAttr) {
        refuse(QObject::tr("grafik: '%1' adlı öznitelik sütunu yok")
                   .arg(QString::fromStdString(item.columns.front())));
        return;
    }

    // COUNTED IN THE DRAWING'S OWN ORDER, then sorted by the folded value so two
    // runs put the bars in the same places (CLAUDE.md 5.6 for the folding).
    std::vector<std::pair<std::string, std::int64_t>> bars;
    const core::EntityTable& entities = document.entities();
    for (core::EntityId slot = 0; slot < entities.size(); ++slot) {
        if (!entities.standalone(slot)) continue;
        if (entities.layer[slot] != on) continue;
        const core::Result<core::AttrValue> cell =
            document.attributes().get(column, entities.slot[slot]);
        std::string value = cell.ok() ? core::attr_display(cell.value()) : std::string();
        if (value.empty()) value = QObject::tr("(boş)").toStdString();

        bool found = false;
        for (auto& [name, count] : bars)
            if (name == value) {
                ++count;
                found = true;
            }
        if (!found) bars.emplace_back(std::move(value), 1);
    }

    if (bars.empty()) {
        refuse(QObject::tr("grafik: '%1' katmanında sayılacak nesne yok")
                   .arg(QString::fromStdString(item.text)));
        return;
    }

    std::stable_sort(bars.begin(), bars.end(), [](const auto& a, const auto& b) {
        return core::turkish_fold_key(a.first) < core::turkish_fold_key(b.first);
    });

    std::int64_t tallest = 0;
    for (const auto& [name, count] : bars)
        tallest = std::max(tallest, count);
    if (tallest <= 0) {
        refuse(QObject::tr("grafik: sayılar sıfır"));
        return;
    }

    // ---- the plot area, leaving room for the value labels under the bars ----
    const double label_h = metrics.height() * 1.4;
    const double top_pad = metrics.height() * 0.6;
    QRectF plot(box.left(), box.top() + top_pad, box.width(), box.height() - top_pad - label_h);
    if (plot.height() < 4.0 || plot.width() < 4.0) {
        refuse(QObject::tr("grafik: kutu çok küçük"));
        return;
    }

    const double slot_w = plot.width() / static_cast<double>(bars.size());
    const double bar_w  = std::max(1.0, slot_w * 0.7);

    const core::Layer* layer = document.layer(on);
    const QColor ink =
        layer != nullptr ? QColor::fromRgba(layer->appearance.rgba) : QColor(Qt::gray);

    for (std::size_t i = 0; i < bars.size(); ++i) {
        const double ratio = static_cast<double>(bars[i].second) / static_cast<double>(tallest);
        const double h     = plot.height() * ratio;
        const QRectF bar(plot.left() + slot_w * static_cast<double>(i) + (slot_w - bar_w) / 2.0,
                         plot.bottom() - h, bar_w, h);

        // THE LAYER'S OWN COLOUR, so a chart of a layer and the layer on the map
        // are read as the same thing. QGIS derives chart colours from the layer's
        // symbology and so does this; a chart in unrelated colours is a second
        // legend the reader has to learn (L-09).
        painter.setPen(QPen(ink.darker(130), 0.6));
        painter.setBrush(ink);
        painter.drawRect(bar);

        painter.setPen(colour_of(item.text_colour));
        painter.setBrush(Qt::NoBrush);
        // THE COUNT ON TOP, the value underneath: a bar whose number a reader has
        // to estimate off an axis is a bar that will be estimated wrong.
        painter.drawText(QRectF(bar.left() - slot_w * 0.15, bar.top() - metrics.height(),
                                bar_w + slot_w * 0.3, metrics.height()),
                         Qt::AlignCenter, QString::number(bars[i].second));
        painter.drawText(
            QRectF(plot.left() + slot_w * static_cast<double>(i), plot.bottom(), slot_w, label_h),
            Qt::AlignCenter, QString::fromStdString(bars[i].first));
    }
    painter.restore();
}

// --------------------------------------------------------------- the table --

void paint_table(QPainter& painter, const QRectF& box, const core::Document& document,
                 std::vector<std::string>* trouble, const core::LayoutItem& item,
                 double px_per_paper_mm)
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

    // AND IT SAYS SO TO THE CALLER, not only on the paper. A client that exported
    // the sheet gets a PDF with "… 79 more" printed on it and a result that says
    // nothing — so a script filing the output believes it is complete. The note
    // on the page is for the person holding it; this is for everyone else
    // (TODOS L-08, C-03).
    if (skipped > 0 && trouble != nullptr)
        trouble->push_back("'" + item.id + "' tablosuna " + std::to_string(skipped) +
                           " satır sığmadı; kutuyu büyütün ya da satir_siniri verin.");
    painter.restore();
}

} // namespace

/// The Qt-side facts as the Qt-free resolver reads them.
core::SheetContext sheet_context(const core::Layout& layout, const core::LayoutItem* map,
                                 const LayoutFacts& facts)
{
    core::SheetContext out;
    out.sheet   = facts.sheet.toStdString();
    out.project = facts.project.toStdString();
    out.crs     = facts.crs.toStdString();
    out.date    = facts.date.toStdString();
    out.paper   = layout.paper;
    out.pages   = static_cast<std::int32_t>(layout.pages.size());
    out.fields  = facts.fields;

    const std::int64_t denominator = map != nullptr ? core::map_scale(*map) : 0;
    if (denominator > 0) out.scale = "1:" + std::to_string(denominator);
    return out;
}

QString resolve_placeholders(const QString& text, const core::Layout& layout,
                             const core::LayoutItem* map, const LayoutFacts& facts)
{
    // DELEGATED TO `/src/core`, which is what makes the designer's preview and the
    // exported PDF resolve from the same values. Two code paths agree until they
    // do not, and the one that disagrees is always the one that got printed.
    return QString::fromStdString(
        core::resolve_fields(text.toStdString(), sheet_context(layout, map, facts)));
}

void paint_layout_page(QPainter& painter, const QRectF& target, const core::Document& document,
                       const core::Layout& layout, int page, double dpi, const LayoutFacts& facts,
                       bool margin_guide, std::vector<std::string>* trouble)
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

    for (const std::size_t at : ordered) {
        const core::LayoutItem* item = &layout.items[at];
        const QRectF box             = frame_in(target, sheet, item->frame);
        if (box.width() < 0.5 || box.height() < 0.5) continue;

        // THE MAP THIS ITEM BELONGS TO, not the first one on the sheet. A scale
        // bar states a map's scale and a `<olcek>` placeholder its denominator;
        // on a page with a 1:1000 and a 1:5000 frame, taking whichever comes
        // first prints one map's number under the other map.
        //
        // A NAME THAT NO LONGER RESOLVES DRAWS NOTHING AND IS REPORTED. Falling
        // back to the first map would put a wrong number on a document somebody
        // signs, and it would do it silently.
        const core::LayoutItem* owner = layout.map_for(*item);
        if (trouble != nullptr && layout.link_is_broken(*item))
            trouble->push_back("'" + item->id + "' öğesi '" + item->linked_map +
                               "' adlı haritaya bağlı ve o harita yok.");

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
            painter.drawText(box, static_cast<int>(alignment_of(*item) | Qt::TextWordWrap),
                             QString::fromStdString(core::resolve_fields(
                                 item->text,
                                 [&] {
                                     core::SheetContext ctx = sheet_context(layout, owner, facts);
                                     ctx.page               = static_cast<std::int32_t>(index) + 1;
                                     return ctx;
                                 }(),
                                 trouble)));
            break;
        }

        case core::LayoutItemKind::ScaleBar:
            paint_scale_bar(painter, box, *item, owner != nullptr ? core::map_scale(*owner) : 0,
                            px_per_paper_mm);
            break;

        case core::LayoutItemKind::NorthArrow:
            paint_north(painter, box, *item, px_per_paper_mm);
            break;

        case core::LayoutItemKind::Legend:
            paint_legend(painter, box, document, *item, px_per_paper_mm);
            break;

        case core::LayoutItemKind::Picture: {
            const QString stored = QString::fromStdString(item->text);

            // RESOLVED AGAINST THE PROJECT, not against the working directory.
            // A relative path is the portable way to carry a logo — the file
            // travels in the folder beside the drawing — and it is only portable
            // if this is where it is resolved (`LayoutFacts::project_dir`).
            QString path = stored;
            if (!stored.isEmpty() && QFileInfo(stored).isRelative() &&
                !facts.project_dir.isEmpty()) {
                const QString beside = QDir(facts.project_dir).filePath(stored);
                if (QFileInfo::exists(beside)) path = beside;
            }

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
                        : QObject::tr("resim bulunamadı: %1").arg(QFileInfo(stored).fileName()));
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
            paint_table(painter, box, document, trouble, *item, px_per_paper_mm);
            break;

        case core::LayoutItemKind::Chart:
            paint_chart(painter, box, document, trouble, *item, px_per_paper_mm);
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
