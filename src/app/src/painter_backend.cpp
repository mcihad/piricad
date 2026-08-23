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

#include "piricad/core/style.hpp"
#include "piricad/render/backend.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

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

/// A colour with a layer's opacity multiplied into its alpha.
///
/// Separate from the alpha the catalogue colour already carries, so a whole
/// symbol layer can be faded without rewriting a value a regulation prescribes.
QColor faded(std::uint32_t rgba, std::uint8_t opacity)
{
    QColor c = from_rgba(rgba);
    if (opacity < 255) c.setAlpha(c.alpha() * opacity / 255);
    return c;
}

Qt::PenCapStyle qt_cap(core::LineCap cap)
{
    switch (cap) {
    case core::LineCap::Butt: return Qt::FlatCap;
    case core::LineCap::Round: return Qt::RoundCap;
    case core::LineCap::Square: return Qt::SquareCap;
    }
    return Qt::RoundCap;
}

Qt::PenJoinStyle qt_join(core::LineJoin join)
{
    switch (join) {
    case core::LineJoin::Miter: return Qt::MiterJoin;
    case core::LineJoin::Round: return Qt::RoundJoin;
    case core::LineJoin::Bevel: return Qt::BevelJoin;
    }
    return Qt::RoundJoin;
}

/// Pen styles standing in for the dash table.
///
/// Like the hatch patterns above, the real dash definitions are a /data asset
/// with prescribed segment lengths. A dashed gösterim is drawn dashed rather than
/// silently solid, because a `plan onama sınırı` and a `mülkiyet sınırı` differ on
/// a plan by exactly that.
Qt::PenStyle dash_pattern(std::uint16_t dash)
{
    static const Qt::PenStyle kStyles[] = {Qt::SolidLine, Qt::DashLine, Qt::DotLine,
                                           Qt::DashDotLine, Qt::DashDotDotLine};
    return kStyles[dash % (sizeof kStyles / sizeof kStyles[0])];
}

/// One marker glyph, centred on the origin, `size` across.
///
/// Built as a path rather than drawn with primitives so every placement is one
/// `drawPath` after a translate and a rotate, and so the GPU backend can take the
/// same shape table and turn it into instanced geometry.
QPainterPath markerPath(core::MarkerShape shape, double size)
{
    const double h = size * 0.5;
    QPainterPath p;

    switch (shape) {
    case core::MarkerShape::Circle: p.addEllipse(QPointF(0, 0), h, h); break;
    case core::MarkerShape::Square: p.addRect(QRectF(-h, -h, size, size)); break;
    case core::MarkerShape::Triangle:
        p.moveTo(0, -h);
        p.lineTo(h, h);
        p.lineTo(-h, h);
        p.closeSubpath();
        break;
    case core::MarkerShape::Diamond:
        p.moveTo(0, -h);
        p.lineTo(h, 0);
        p.lineTo(0, h);
        p.lineTo(-h, 0);
        p.closeSubpath();
        break;
    case core::MarkerShape::Star:
        for (int i = 0; i < 10; ++i) {
            const double a = -M_PI / 2 + i * M_PI / 5;
            const double r = (i % 2 == 0) ? h : h * 0.42;
            const QPointF v(r * std::cos(a), r * std::sin(a));
            if (i == 0)
                p.moveTo(v);
            else
                p.lineTo(v);
        }
        p.closeSubpath();
        break;
    case core::MarkerShape::Cross:
        p.moveTo(-h, 0);
        p.lineTo(h, 0);
        p.moveTo(0, -h);
        p.lineTo(0, h);
        break;
    case core::MarkerShape::XCross:
        p.moveTo(-h, -h);
        p.lineTo(h, h);
        p.moveTo(-h, h);
        p.lineTo(h, -h);
        break;
    case core::MarkerShape::Arrow:
        p.moveTo(h, 0);
        p.lineTo(-h, h * 0.7);
        p.lineTo(-h * 0.4, 0);
        p.lineTo(-h, -h * 0.7);
        p.closeSubpath();
        break;
    case core::MarkerShape::HalfCircle:
        p.moveTo(-h, 0);
        p.arcTo(QRectF(-h, -h, size, size), 180, -180);
        p.closeSubpath();
        break;
    case core::MarkerShape::Pentagon:
    case core::MarkerShape::Hexagon: {
        const int sides = shape == core::MarkerShape::Pentagon ? 5 : 6;
        for (int i = 0; i < sides; ++i) {
            const double a = -M_PI / 2 + 2 * M_PI * i / sides;
            const QPointF v(h * std::cos(a), h * std::sin(a));
            if (i == 0)
                p.moveTo(v);
            else
                p.lineTo(v);
        }
        p.closeSubpath();
        break;
    }
    case core::MarkerShape::Tick:
        // A bare stroke. Drawn from the line outward on both sides, which is what
        // a railway's sleeper and an escarpment's tick both are.
        p.moveTo(0, -h);
        p.lineTo(0, h);
        break;
    }
    return p;
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

        // IN DRAW ORDER, which is not index order: MPYY prescribes a draw order
        // for plan sheets, and within one symbol the stack runs bottom layer
        // first. The scene builder sorted it; this loop obeys it.
        for (std::uint32_t index : list.order) {
            if (index >= list.passes.size()) continue;
            drawPass(painter, list.passes[index], list.polylines[index], list.polygons[index], cx,
                     cy);
        }

        drawTexts(painter, list, cx, cy);
        drawOverlay(painter, overlay);
    }

private:
    /// One symbol layer of one style: whatever it paints, in its own order.
    ///
    /// Not static, unlike the vector paths below: a raster pass reads the decoded
    /// picture cache, which belongs to this backend and outlives the frame.
    void drawPass(QPainter& painter, const render::PassStyle& ps,
                  const render::PolylineBatch& stroke, const render::PolygonBatch& fill, double cx,
                  double cy)
    {
        using core::SymbolLayerType;

        switch (ps.type) {
        case SymbolLayerType::SimpleFill: drawSimpleFill(painter, fill, ps, cx, cy); break;
        case SymbolLayerType::LinePatternFill:
            drawLinePatternFill(painter, fill, ps, cx, cy);
            break;
        case SymbolLayerType::PointPatternFill:
            drawPointPatternFill(painter, fill, ps, cx, cy);
            break;
        case SymbolLayerType::CentroidFill: drawCentroidFill(painter, fill, ps, cx, cy); break;
        case SymbolLayerType::SimpleLine: drawSimpleLine(painter, stroke, ps, cx, cy); break;
        case SymbolLayerType::MarkerLine:
            drawMarkerLine(painter, stroke, ps, cx, cy, /*hash=*/false);
            break;
        case SymbolLayerType::HashLine:
            drawMarkerLine(painter, stroke, ps, cx, cy, /*hash=*/true);
            break;
        case SymbolLayerType::SimpleMarker:
            drawMarkerLine(painter, stroke, ps, cx, cy, /*hash=*/false);
            break;
        case SymbolLayerType::RasterFill: drawRasterFill(painter, fill, ps, cx, cy); break;
        case SymbolLayerType::RasterMarker:
            // A published sembol sits INSIDE the lekesi it labels, which is what
            // MPYY prints. Only a run that is not a face — an open line — puts it
            // on the geometry itself.
            if (!fill.runs.empty())
                drawRasterCentres(painter, fill, ps, cx, cy);
            else
                drawRasterAlong(painter, stroke, ps, cx, cy);
            break;
        case SymbolLayerType::RasterLine: drawRasterAlong(painter, stroke, ps, cx, cy); break;
        }
    }

    /// The rings of a fill batch as one path with the odd-even rule.
    ///
    /// Odd-even winding is what punches the holes out: a courtyard ring inside its
    /// parcel ring cancels, without this backend having to know which ring was
    /// declared a hole. The flag is still carried in the draw list because the GPU
    /// backend will need it explicitly.
    static QPainterPath fillPath(const render::PolygonBatch& batch, double cx, double cy)
    {
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
        return path;
    }

    static void drawSimpleFill(QPainter& painter, const render::PolygonBatch& batch,
                               const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty() || batch.rgba == 0) return;

        QBrush brush(faded(batch.rgba, ps.opacity));
        if (batch.hatch != 0) brush.setStyle(hatch_pattern(batch.hatch));
        painter.fillPath(fillPath(batch, cx, cy), brush);
    }

    /// Parallel lines at an angle, clipped to the face.
    ///
    /// This is what a `tarım alanı` or a `jeolojik sakıncalı alan` looks like in
    /// MPYY, and it is a fill rather than a hatch brush because the spacing and
    /// the angle are prescribed numbers: Qt's brush patterns are fixed at whatever
    /// the style engine chose, and a plan whose hatch spacing is decorative is a
    /// plan that says the wrong thing.
    static void drawLinePatternFill(QPainter& painter, const render::PolygonBatch& batch,
                                    const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const QPainterPath path = fillPath(batch, cx, cy);
        const QRectF box        = path.boundingRect();
        if (box.isEmpty()) return;

        const double spacing = ps.interval_px > 0.5f ? static_cast<double>(ps.interval_px) : 6.0;

        painter.save();
        painter.setClipPath(path, Qt::IntersectClip);
        if (batch.rgba != 0) painter.fillRect(box, faded(batch.rgba, ps.opacity));

        QPen pen(faded(ps.line_rgba, ps.opacity));
        pen.setWidthF(ps.line_width_px);
        painter.setPen(pen);

        // Rotated about the box centre so the pattern is continuous across the
        // whole face rather than restarting at each ring.
        painter.translate(box.center());
        painter.rotate(-static_cast<double>(ps.angle_udeg) / 1'000'000.0);

        const double reach = std::hypot(box.width(), box.height()) * 0.5 + spacing;
        for (double y = -reach; y <= reach; y += spacing)
            painter.drawLine(QPointF(-reach, y), QPointF(reach, y));

        painter.restore();
    }

    /// A grid of glyphs, clipped to the face — `orman`, `mezarlık`, `bataklık`.
    static void drawPointPatternFill(QPainter& painter, const render::PolygonBatch& batch,
                                     const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const QPainterPath path = fillPath(batch, cx, cy);
        const QRectF box        = path.boundingRect();
        if (box.isEmpty()) return;

        const double step_x = ps.interval_px > 0.5f ? static_cast<double>(ps.interval_px) : 12.0;
        // Zero means SQUARE, not zero: a glyph grid with no second spacing is a
        // regular grid, which is what a forest symbol wants.
        const double step_y =
            ps.spacing_y_px > 0.5f ? static_cast<double>(ps.spacing_y_px) : step_x;
        const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 4.0;

        painter.save();
        painter.setClipPath(path, Qt::IntersectClip);
        if (batch.rgba != 0) painter.fillRect(box, faded(batch.rgba, ps.opacity));

        const QColor ink = faded(ps.line_rgba, ps.opacity);
        painter.setPen(QPen(ink, ps.line_width_px));
        painter.setBrush(QBrush(ink));

        // Anchored to the world grid rather than to the bounding box, so the
        // glyphs do not crawl across the face as the user pans.
        const double x0 = std::floor(box.left() / step_x) * step_x;
        const double y0 = std::floor(box.top() / step_y) * step_y;
        for (double y = y0; y <= box.bottom() + step_y; y += step_y)
            for (double x = x0; x <= box.right() + step_x; x += step_x)
                painter.drawPath(markerPath(ps.shape, size).translated(x, y));

        painter.setBrush(Qt::NoBrush);
        painter.restore();
    }

    /// One glyph at the centre of each face.
    static void drawCentroidFill(QPainter& painter, const render::PolygonBatch& batch,
                                 const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 6.0;
        const QColor ink  = faded(ps.line_rgba, ps.opacity);
        painter.setPen(QPen(ink, ps.line_width_px));
        painter.setBrush(QBrush(ink));

        std::size_t offset = 0;
        for (std::uint32_t run : batch.runs) {
            // The bounding-box centre, not the area centroid. For a plan symbol
            // placed inside a `lekesi` the difference is invisible, and the area
            // centroid of a ring with holes is a different computation that
            // belongs in core rather than in a backend.
            double min_x = 1e30, max_x = -1e30, min_y = 1e30, max_y = -1e30;
            for (std::uint32_t v = 0; v < run; ++v) {
                const double x = cx + static_cast<double>(batch.xs[offset + v]);
                const double y = cy - static_cast<double>(batch.ys[offset + v]);
                min_x          = std::min(min_x, x);
                max_x          = std::max(max_x, x);
                min_y          = std::min(min_y, y);
                max_y          = std::max(max_y, y);
            }
            painter.drawPath(markerPath(ps.shape, size)
                                 .translated((min_x + max_x) * 0.5, (min_y + max_y) * 0.5));
            offset += run;
        }
        painter.setBrush(Qt::NoBrush);
    }

    /// The decoded form of an embedded picture, or a null image.
    ///
    /// Cached by the store's CONTENT key, not by an address: a document closes,
    /// its memory is reused by the next one, and a cache keyed on the pointer
    /// would serve the old picture for the new address.
    ///
    /// A picture that fails to decode is cached as a NULL image so the failure
    /// costs one attempt rather than one attempt per frame. It draws nothing,
    /// which is the honest result: the drawing says there is a picture and this
    /// build cannot read it.
    const QImage& decoded(const render::PassStyle& ps)
    {
        static const QImage kNone;
        if (ps.image.empty() || ps.image_key == 0) return kNone;

        const auto it = images_.find(ps.image_key);
        if (it != images_.end()) return it->second;

        QImage image;
        image.loadFromData(reinterpret_cast<const uchar*>(ps.image.data()),
                           static_cast<int>(ps.image.size()));
        return images_.emplace(ps.image_key, std::move(image)).first->second;
    }

    /// The picture tiled into the face — a MPYY `tarama`.
    ///
    /// Tiled at a declared size rather than at its pixel size, because a hatch
    /// extracted from a Word annex has whatever resolution the annex had, and a
    /// plan whose hatch spacing follows the scan resolution says the wrong thing.
    void drawRasterFill(QPainter& painter, const render::PolygonBatch& batch,
                        const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const QImage& source = decoded(ps);
        if (source.isNull()) return;

        const double tile  = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 24.0;
        const double ratio = source.height() > 0 ? double(source.height()) / source.width() : 1.0;

        QImage scaled = source.scaled(std::max(1, int(tile)), std::max(1, int(tile * ratio)),
                                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QBrush brush(scaled);
        if (ps.angle_udeg != 0) {
            QTransform rotation;
            rotation.rotate(-static_cast<double>(ps.angle_udeg) / 1'000'000.0);
            brush.setTransform(rotation);
        }

        painter.save();
        if (ps.opacity < 255) painter.setOpacity(double(ps.opacity) / 255.0);
        painter.fillPath(fillPath(batch, cx, cy), brush);
        painter.restore();
    }

    /// The picture once at the centre of each face — a MPYY `sembol` in its lekesi.
    void drawRasterCentres(QPainter& painter, const render::PolygonBatch& batch,
                           const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const QImage& source = decoded(ps);
        if (source.isNull()) return;

        const double height = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 16.0;
        const double width =
            source.height() > 0 ? height * source.width() / source.height() : height;
        const QImage scaled = source.scaled(std::max(1, int(width)), std::max(1, int(height)),
                                            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        painter.save();
        applyInkComposition(painter, ps);

        std::size_t offset = 0;
        for (std::uint32_t run : batch.runs) {
            double min_x = 1e30, max_x = -1e30, min_y = 1e30, max_y = -1e30;
            for (std::uint32_t v = 0; v < run; ++v) {
                const double x = cx + static_cast<double>(batch.xs[offset + v]);
                const double y = cy - static_cast<double>(batch.ys[offset + v]);
                min_x          = std::min(min_x, x);
                max_x          = std::max(max_x, x);
                min_y          = std::min(min_y, y);
                max_y          = std::max(max_y, y);
            }
            painter.drawImage(QPointF((min_x + max_x) * 0.5 - scaled.width() * 0.5,
                                      (min_y + max_y) * 0.5 - scaled.height() * 0.5),
                              scaled);
            offset += run;
        }
        painter.restore();
    }

    /// Draws a scanned picture so its PAPER does not hide what is under it.
    ///
    /// MPYY's annex images are JPEG, which has no alpha, so every glyph and line
    /// type arrives on an opaque white rectangle. Stamped as-is, a `sembol`
    /// punches a white hole in the lekesi it is supposed to label.
    ///
    /// Multiply is the answer rather than keying white out: it darkens by the
    /// picture, so white leaves the background untouched and every grey the
    /// scanner produced still darkens by exactly as much as it is dark. Keying
    /// would need a threshold, and a threshold on a scanned regulation is a
    /// decision about which greys are ink — which nobody has authority to make
    /// here.
    static void applyInkComposition(QPainter& painter, const render::PassStyle& ps)
    {
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        if (ps.opacity < 255) painter.setOpacity(double(ps.opacity) / 255.0);
    }

    /// The picture placed along the geometry — a MPYY `sembol` or `çizgi tipi`.
    ///
    /// One code path for both, because they differ only in where the stamps go
    /// and that is already what `placement` says: a sembol is one stamp in the
    /// middle, a çizgi tipi is a stamp every interval along the run.
    void drawRasterAlong(QPainter& painter, const render::PolylineBatch& batch,
                         const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const QImage& source = decoded(ps);
        if (source.isNull()) return;

        const double height = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 16.0;
        const double width =
            source.height() > 0 ? height * source.width() / source.height() : height;

        const QImage scaled = source.scaled(std::max(1, int(width)), std::max(1, int(height)),
                                            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // A line type tiles edge to edge unless a spacing was asked for; a symbol
        // is placed once and does not tile.
        const double interval =
            ps.interval_px > 0.5f
                ? static_cast<double>(ps.interval_px)
                : (ps.type == core::SymbolLayerType::RasterLine ? width : width * 2.0);

        painter.save();
        applyInkComposition(painter, ps);

        std::size_t offset = 0;
        for (std::uint32_t run : batch.runs) {
            stampAlongRun(painter, batch, offset, run, ps, cx, cy, interval, scaled);
            offset += run;
        }
        painter.restore();
    }

    /// Walks one run and stamps the picture where the placement says.
    ///
    /// The same walk `placeAlongRun` does for a vector glyph. Kept separate rather
    /// than templated on the stamp, because a picture is drawn centred on its own
    /// rectangle and a path is drawn centred on the origin, and folding the two
    /// would put an offset in a place a reader has to hold in their head.
    static void stampAlongRun(QPainter& painter, const render::PolylineBatch& batch,
                              std::size_t offset, std::uint32_t run, const render::PassStyle& ps,
                              double cx, double cy, double interval, const QImage& picture)
    {
        if (run < 2 || interval <= 0.0) return;

        const auto at = [&](std::uint32_t v) {
            return QPointF(cx + static_cast<double>(batch.xs[offset + v]),
                           cy - static_cast<double>(batch.ys[offset + v]));
        };

        const auto stamp = [&](QPointF p, double degrees) {
            painter.save();
            painter.translate(p);
            painter.rotate(degrees + static_cast<double>(ps.angle_udeg) / 1'000'000.0);
            painter.drawImage(QPointF(-picture.width() * 0.5, -picture.height() * 0.5), picture);
            painter.restore();
        };

        double total = 0.0;
        for (std::uint32_t v = 1; v < run; ++v)
            total += lengthOf(at(v - 1), at(v));
        if (total <= 0.0) return;

        // A raster marker with no placement of its own sits in the middle of the
        // run, which is where a plan puts a `sembol` inside its lekesi.
        const bool once = ps.type == core::SymbolLayerType::RasterMarker &&
                          ps.placement == core::MarkerPlacement::Interval;

        double walked = 0.0;
        double next   = once ? total * 0.5 : interval * 0.5;

        for (std::uint32_t v = 1; v < run; ++v) {
            const QPointF a  = at(v - 1);
            const QPointF b  = at(v);
            const double len = lengthOf(a, b);
            if (len <= 0.0) continue;

            // A raster marker does NOT turn with the line: a mosque glyph lying
            // on its side is not the glyph the regulation printed.
            const double degrees =
                ps.type == core::SymbolLayerType::RasterLine ? segmentDegrees(a, b) : 0.0;

            while (next <= walked + len) {
                const double t = (next - walked) / len;
                stamp(QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t), degrees);
                if (once) return;
                next += interval;
            }
            walked += len;
        }
    }

    static void drawSimpleLine(QPainter& painter, const render::PolylineBatch& batch,
                               const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        QPen pen(faded(batch.rgba, ps.opacity));
        pen.setWidthF(batch.width_px);
        pen.setCapStyle(qt_cap(ps.cap));
        pen.setJoinStyle(qt_join(ps.join));
        if (ps.dash != 0) pen.setStyle(dash_pattern(ps.dash));
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

    /// Glyphs along a line — a marker line, or the ticks of a hash line.
    ///
    /// `hash` draws each glyph rotated ACROSS the line instead of along it, which
    /// is the difference between a railway's sleepers and a boundary's dots.
    static void drawMarkerLine(QPainter& painter, const render::PolylineBatch& batch,
                               const render::PassStyle& ps, double cx, double cy, bool hash)
    {
        if (batch.runs.empty()) return;

        const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 5.0;
        const double interval =
            ps.interval_px > 0.5f ? static_cast<double>(ps.interval_px) : size * 3.0;
        const QColor ink = faded(ps.line_rgba, ps.opacity);

        painter.setPen(QPen(ink, ps.line_width_px));
        painter.setBrush(hash ? QBrush(Qt::NoBrush) : QBrush(ink));

        const QPainterPath glyph = markerPath(hash ? core::MarkerShape::Tick : ps.shape, size);

        std::size_t offset = 0;
        for (std::uint32_t run : batch.runs) {
            placeAlongRun(painter, batch, offset, run, ps, cx, cy, interval, glyph);
            offset += run;
        }
        painter.setBrush(Qt::NoBrush);
    }

    /// Walks one run and stamps the glyph where the placement says.
    static void placeAlongRun(QPainter& painter, const render::PolylineBatch& batch,
                              std::size_t offset, std::uint32_t run, const render::PassStyle& ps,
                              double cx, double cy, double interval, const QPainterPath& glyph)
    {
        const auto at = [&](std::uint32_t v) {
            return QPointF(cx + static_cast<double>(batch.xs[offset + v]),
                           cy - static_cast<double>(batch.ys[offset + v]));
        };

        const auto stamp = [&](QPointF p, double degrees) {
            painter.save();
            painter.translate(p);
            // LOCAL +X IS ALONG THE LINE, for every glyph. That is the whole
            // convention, and the shapes are drawn to it: an arrow points at +x,
            // a tick runs along local y and therefore already crosses the line.
            // An earlier version added a quarter turn for hash ticks and laid
            // them flat on top of the line, where they were invisible.
            painter.rotate(degrees + static_cast<double>(ps.angle_udeg) / 1'000'000.0);
            painter.drawPath(glyph);
            painter.restore();
        };

        using core::MarkerPlacement;
        if (run < 2) return;

        if (ps.placement == MarkerPlacement::Vertex) {
            for (std::uint32_t v = 0; v < run; ++v)
                stamp(at(v), 0.0);
            return;
        }
        if (ps.placement == MarkerPlacement::FirstVertex) {
            stamp(at(0), segmentDegrees(at(0), at(1)));
            return;
        }
        if (ps.placement == MarkerPlacement::LastVertex) {
            stamp(at(run - 1), segmentDegrees(at(run - 2), at(run - 1)));
            return;
        }

        // Interval and Centre both need the run's length, so they share the walk.
        double total = 0.0;
        for (std::uint32_t v = 1; v < run; ++v)
            total += lengthOf(at(v - 1), at(v));
        if (total <= 0.0) return;

        const double target = ps.placement == MarkerPlacement::Centre ? total * 0.5 : interval;
        double walked       = 0.0;
        double next         = ps.placement == MarkerPlacement::Centre ? target : interval * 0.5;

        for (std::uint32_t v = 1; v < run; ++v) {
            const QPointF a  = at(v - 1);
            const QPointF b  = at(v);
            const double len = lengthOf(a, b);
            if (len <= 0.0) continue;

            const double degrees = segmentDegrees(a, b);
            while (next <= walked + len) {
                const double t = (next - walked) / len;
                stamp(QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t), degrees);
                if (ps.placement == MarkerPlacement::Centre) return;
                next += interval;
            }
            walked += len;
        }
    }

    static double lengthOf(QPointF a, QPointF b)
    {
        return std::hypot(b.x() - a.x(), b.y() - a.y());
    }

    static double segmentDegrees(QPointF a, QPointF b)
    {
        return std::atan2(b.y() - a.y(), b.x() - a.x()) * 180.0 / M_PI;
    }

    /// Decoded pictures, keyed by content. Grows with the pictures a session
    /// actually draws and is never pruned: the whole MPYY set is eight megabytes
    /// of source and a session draws a handful of them.
    std::unordered_map<std::uint64_t, QImage> images_;

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
