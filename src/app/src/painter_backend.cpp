// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the QPainter backend.
//
// THE PHASE-0 STAND-IN (CLAUDE.md Article 8.1). It draws the same `DrawList` and
// the same `Overlay` the GPU backend will draw, through the same
// `render::Backend` interface, so replacing it changes this file and nothing
// above it.
//
// This file is the ONLY place in /src/app allowed to name QPainter (render.md R1,
// P3). Until now that rule described an intention rather than the code: the
// drawing lived in `MapCanvas::paintEvent`, `render::Backend` had no
// implementation at all, and the interface the GPU backend was supposed to slide
// in behind was an interface nothing went through. It does now.
//
// What it is NOT: a design for how the GPU path should work. Line widening here
// is `QPen`, where render.md R5 requires instanced quads expanded in a vertex
// shader; fills are `QPainterPath` with an odd-even rule, where the GPU path will
// use the stencil buffer; text is a system font, where R8 requires an msdfgen SDF
// atlas shaped with HarfBuzz. Those are the GPU backend's problems, and keeping
// them out of the interface is what makes them replaceable.
#include "piricad/app/backend_factory.hpp"

#include "piricad/render/backend.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <cmath>
#include <memory>
#include <string>

namespace piricad::app {
namespace {

QColor from_rgba(std::uint32_t rgba)
{
    return QColor::fromRgba(static_cast<QRgb>(rgba));
}

/// Brush patterns standing in for the MPYY hatch atlas.
///
/// The real hatches are a /data asset and land with the symbol atlas in Phase 1.
/// Until then a patterned fill is drawn PATTERNED rather than silently solid, so
/// nobody mistakes a hatched `gösterim` for a solid one on a plan they signed.
Qt::BrushStyle hatch_pattern(std::uint16_t hatch)
{
    static const Qt::BrushStyle kPatterns[] = {
        Qt::SolidPattern, Qt::Dense4Pattern, Qt::HorPattern,   Qt::VerPattern,
        Qt::CrossPattern, Qt::BDiagPattern,  Qt::FDiagPattern, Qt::DiagCrossPattern};
    return kPatterns[hatch % (sizeof kPatterns / sizeof kPatterns[0])];
}

class PainterBackend final : public render::Backend
{
public:
    std::string name() const override { return "QPainter (Faz 0)"; }

    bool gpu() const override { return false; }

    void render(const render::DrawList& list, const render::Overlay& overlay,
                const render::FrameContext& ctx) override
    {
        auto* device = static_cast<QWidget*>(ctx.target);
        if (device == nullptr) return;

        QPainter painter(device);
        painter.fillRect(0, 0, ctx.width_px, ctx.height_px, from_rgba(overlay.background_rgba));
        painter.setRenderHint(QPainter::Antialiasing, true);

        // Document coordinates arrive centre-relative with y UP; widget pixels run
        // from the top-left with y DOWN. One conversion, in one place.
        const double cx = ctx.width_px * 0.5;
        const double cy = ctx.height_px * 0.5;

        drawFills(painter, list, cx, cy);
        drawStrokes(painter, list, cx, cy);
        drawTexts(painter, list, cx, cy);
        drawOverlay(painter, overlay);
    }

private:
    /// Fills first, strokes on top. A boundary drawn under its own fill is a
    /// boundary the user cannot see, and in a plan the boundary is the legal edge.
    static void drawFills(QPainter& painter, const render::DrawList& list, double cx, double cy)
    {
        for (const auto& batch : list.polygons) {
            if (batch.runs.empty() || batch.rgba == 0) continue;

            // Odd-even winding is what punches the holes out: a courtyard ring
            // inside its parcel ring cancels, without this backend having to know
            // which ring was declared a hole. The flag is still carried in the
            // draw list because the GPU backend will need it explicitly.
            QPainterPath path;
            path.setFillRule(Qt::OddEvenFill);

            std::size_t offset = 0;
            for (std::uint32_t run : batch.runs) {
                path.moveTo(cx + static_cast<double>(batch.xs[offset]),
                            cy - static_cast<double>(batch.ys[offset]));
                for (std::uint32_t v = 1; v < run; ++v)
                    path.lineTo(cx + static_cast<double>(batch.xs[offset + v]),
                                cy - static_cast<double>(batch.ys[offset + v]));
                path.closeSubpath();
                offset += run;
            }

            QBrush brush(from_rgba(batch.rgba));
            if (batch.hatch != 0) brush.setStyle(hatch_pattern(batch.hatch));
            painter.fillPath(path, brush);
        }
    }

    static void drawStrokes(QPainter& painter, const render::DrawList& list, double cx, double cy)
    {
        for (const auto& batch : list.polylines) {
            if (batch.runs.empty()) continue;

            QPen pen(from_rgba(batch.rgba));
            pen.setWidthF(batch.width_px);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);

            std::size_t offset = 0;
            QPainterPath path;
            for (std::uint32_t run : batch.runs) {
                path.moveTo(cx + static_cast<double>(batch.xs[offset]),
                            cy - static_cast<double>(batch.ys[offset]));
                for (std::uint32_t v = 1; v < run; ++v)
                    path.lineTo(cx + static_cast<double>(batch.xs[offset + v]),
                                cy - static_cast<double>(batch.ys[offset + v]));
                offset += run;
            }
            painter.drawPath(path);
        }
    }

    /// Captions last, over both fills and strokes: a parcel number under its own
    /// boundary is a parcel number nobody can read.
    static void drawTexts(QPainter& painter, const render::DrawList& list, double cx, double cy)
    {
        for (const auto& item : list.texts) {
            if (item.text.empty() || item.height_px < 3.0f) continue; // unreadable, so not drawn

            const QPointF start(cx + static_cast<double>(item.x0),
                                cy - static_cast<double>(item.y0));
            const QPointF end(cx + static_cast<double>(item.x1), cy - static_cast<double>(item.y1));

            // Rotation comes from the baseline direction — the document stores no
            // angle, so there is none to disagree with the geometry.
            const double dx = end.x() - start.x();
            const double dy = end.y() - start.y();
            const double degrees =
                (dx == 0.0 && dy == 0.0) ? 0.0 : std::atan2(dy, dx) * 180.0 / M_PI;

            QFont font = painter.font();
            font.setPixelSize(std::max(3, static_cast<int>(item.height_px)));
            painter.setFont(font);
            painter.setPen(from_rgba(item.rgba));

            const QString label = QString::fromStdString(item.text);
            const QFontMetricsF metrics(font);
            const double advance = metrics.horizontalAdvance(label);

            painter.save();
            painter.translate(start);
            painter.rotate(degrees);

            // The anchor decides where the baseline sits under the glyphs.
            // Measured from the real font rather than from the advance guess the
            // command used for the bounding box.
            double shift_x = 0.0;
            double shift_y = 0.0;
            switch (item.anchor) {
            case 1: shift_x = -advance * 0.5; break; // baseline centre
            case 2: shift_x = -advance; break;       // baseline right
            case 3:                                  // middle centre
                shift_x = -advance * 0.5;
                shift_y = metrics.capHeight() * 0.5;
                break;
            default: break; // baseline left
            }
            painter.drawText(QPointF(shift_x, shift_y), label);
            painter.restore();
        }
    }

    /// The grid, the selection, the snap glyph, the crosshair, the rubber band.
    ///
    /// All of it arrives as runs of widget-space points, so this loop is the same
    /// four lines whatever it is drawing. The shapes — a square for an endpoint, a
    /// bowtie for nearest — are chosen by the canvas, because which glyph means
    /// which aid is a decision about the product and not about the renderer.
    static void drawOverlay(QPainter& painter, const render::Overlay& overlay)
    {
        for (const auto& batch : overlay.batches) {
            if (batch.runs.empty()) continue;

            QPen pen(from_rgba(batch.rgba));
            pen.setWidthF(batch.width_px);
            pen.setStyle(batch.dashed ? Qt::DashLine : Qt::SolidLine);
            painter.setPen(pen);
            painter.setBrush(batch.fill_rgba != 0 ? QBrush(from_rgba(batch.fill_rgba))
                                                  : QBrush(Qt::NoBrush));

            std::size_t offset = 0;
            for (std::size_t r = 0; r < batch.runs.size(); ++r) {
                const std::uint32_t run = batch.runs[r];
                if (run < 2) {
                    offset += run;
                    continue;
                }

                QPainterPath path;
                path.moveTo(static_cast<double>(batch.xs[offset]),
                            static_cast<double>(batch.ys[offset]));
                for (std::uint32_t v = 1; v < run; ++v)
                    path.lineTo(static_cast<double>(batch.xs[offset + v]),
                                static_cast<double>(batch.ys[offset + v]));
                if (r < batch.closed.size() && batch.closed[r] != 0) path.closeSubpath();

                painter.drawPath(path);
                offset += run;
            }
            painter.setBrush(Qt::NoBrush);
        }

        for (const auto& label : overlay.labels) {
            if (label.text.empty()) continue;

            if (label.px > 0.0f) {
                QFont font = painter.font();
                font.setPixelSize(static_cast<int>(label.px));
                painter.setFont(font);
            }
            painter.setPen(from_rgba(label.rgba));
            painter.drawText(QPointF(static_cast<double>(label.x), static_cast<double>(label.y)),
                             QString::fromStdString(label.text));
        }
    }
};

} // namespace

std::unique_ptr<render::Backend> make_canvas_backend()
{
    // One `return` today. When the QRhi backend lands it is chosen here, and the
    // canvas is not edited — which is the whole claim CLAUDE.md Article 8.1 makes
    // about its own removal.
    return std::make_unique<PainterBackend>();
}

} // namespace piricad::app
