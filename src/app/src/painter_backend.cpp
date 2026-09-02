// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the QPainter backend.
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
#include "kentos_cad/app/backend_factory.hpp"
#include "kentos_cad/app/symbol_image.hpp"
#if KENTOS_HAVE_QGIS
#include "kentos_cad/app/qgis_backend.hpp"
#endif

#include "kentos_cad/core/style.hpp"
#include "kentos_cad/render/backend.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QSvgRenderer>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace kentos::app {
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

/// Puts the pass's own line type on a pen.
///
/// The lengths arrive already resolved out of the document's `DashStore` and are
/// in hundredths of the stroke's width, which is the unit `QPen::setDashPattern`
/// wants — so a pattern declared once is right at every weight the same boundary
/// is ever drawn at.
///
/// This used to be five `Qt::PenStyle` constants indexed by the dash id: a
/// stand-in, and it drew every published line type as one of five generic
/// dashes. MPYY distinguishes a province boundary from a municipal one by the
/// number of dots between the dashes, and five constants cannot say that.
void apply_dash(QPen& pen, const render::PassStyle& ps)
{
    if (ps.dash_count == 0) return;

    QList<qreal> pattern;
    pattern.reserve(ps.dash_count);
    for (std::uint8_t i = 0; i < ps.dash_count; ++i)
        pattern.push_back(static_cast<qreal>(ps.dash_lengths[i]) / 100.0);

    pen.setDashPattern(pattern);

    // FLAT CAPS, whatever the layer declared, and this is not a liberty.
    //
    // Qt puts the cap on EVERY DASH, not on the ends of the line: with a round
    // cap each mark grows by half a pen width at both ends. A pattern declaring a
    // one-width gap then has no gap left at all, and the line this program drew
    // for a published dash-dot boundary came out SOLID — indistinguishable from a
    // property boundary, which on a plan sheet is a different legal statement.
    //
    // The declared cap describes how the STROKE ends, and a dashed stroke still
    // ends the way it says at its two real ends; what it cannot also mean is
    // every mark inside it. Every CAD line type is flat-capped for this reason.
    pen.setCapStyle(Qt::FlatCap);
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
        auto* device = static_cast<QPaintDevice*>(ctx.target);
        if (device == nullptr) return;

        QPainter painter(device);

        // A FULLY TRANSPARENT background means "leave what is already there", and
        // the symbol preview relies on it: it paints its own checkerboard first
        // and a clear here would cover it. Every other caller passes an opaque
        // colour, so the canvas still starts from a known ground each frame.
        if ((overlay.background_rgba >> 24) != 0)
            painter.fillRect(0, 0, ctx.width_px, ctx.height_px, from_rgba(overlay.background_rgba));
        painter.setRenderHint(QPainter::Antialiasing, true);

        // Document coordinates arrive centre-relative with y UP; widget pixels run
        // from the top-left with y DOWN. One conversion, in one place.
        const double cx = ctx.width_px * 0.5;
        const double cy = ctx.height_px * 0.5;

        // The grid goes down BEFORE the document: it is the paper the drawing is
        // on, not a layer over it (`Overlay::beneath`).
        paint_ground(painter, overlay);

        // IN DRAW ORDER, which is not index order: MPYY prescribes a draw order
        // for plan sheets, and within one symbol the stack runs bottom layer
        // first. The scene builder sorted it; this loop obeys it.
        for (std::uint32_t index : list.order) {
            if (index >= list.passes.size()) continue;
            drawPass(painter, list.passes[index], list.polylines[index], list.polygons[index], cx,
                     cy);
        }

        paint_aids(painter, list, overlay, cx, cy);
    }

private:
    /// The brush a glyph is painted with: the layer's FILL colour, or none.
    ///
    /// A marker has two colours the way every drawn shape does — an outline and an
    /// interior — and they are the layer's stroke and fill. Painting the interior
    /// with the STROKE colour, which an earlier version did, makes an
    /// outline-only circle impossible to ask for: a `yapılaşma koşulu` ring around
    /// a KAKS value came out as a solid disc with the value hidden under it.
    static QBrush glyph_brush(const render::PassStyle& ps)
    {
        return ps.fill_rgba == 0 ? QBrush(Qt::NoBrush) : QBrush(faded(ps.fill_rgba, ps.opacity));
    }

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
        case SymbolLayerType::TextMarker:
            // Nothing here: the scene builder turned it into a TextItem, so it is
            // drawn with the captions, over every fill and stroke. A word inside a
            // gösterim that a later pass could paint over is a word nobody reads.
            break;
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
        // NO GROUND WASH HERE, and the reason is that there is only one fill
        // colour on a layer. `dolgu_renk` is the GLYPH's fill — the black of a
        // forest triangle, the red of a coral tuft — and washing the face with it
        // painted the whole parcel that colour with the glyphs invisible inside
        // it. The MPYY forest and cemetery rows came out as solid blocks.
        //
        // Washing the area is a `dolgu` layer's job, and a catalogue row that
        // wants one says so: the MPYY package puts the annex's ALAN RENK KODU in
        // a `dolgu` layer underneath the pattern, which is what draws the green
        // under the forest triangles. A pattern layer paints its pattern.

        QPen pen(faded(ps.line_rgba, ps.opacity));
        pen.setWidthF(static_cast<qreal>(ps.line_width_px));
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
        // NO GROUND WASH HERE, and the reason is that there is only one fill
        // colour on a layer. `dolgu_renk` is the GLYPH's fill — the black of a
        // forest triangle, the red of a coral tuft — and washing the face with it
        // painted the whole parcel that colour with the glyphs invisible inside
        // it. The MPYY forest and cemetery rows came out as solid blocks.
        //
        // Washing the area is a `dolgu` layer's job, and a catalogue row that
        // wants one says so: the MPYY package puts the annex's ALAN RENK KODU in
        // a `dolgu` layer underneath the pattern, which is what draws the green
        // under the forest triangles. A pattern layer paints its pattern.

        painter.setPen(QPen(faded(ps.line_rgba, ps.opacity), static_cast<qreal>(ps.line_width_px)));
        painter.setBrush(glyph_brush(ps));

        // Anchored to the world grid rather than to the bounding box, so the
        // glyphs do not crawl across the face as the user pans.
        const double x0      = std::floor(box.left() / step_x) * step_x;
        const double y0      = std::floor(box.top() / step_y) * step_y;
        const double degrees = static_cast<double>(ps.angle_udeg) / 1'000'000.0;
        for (double y = y0; y <= box.bottom() + step_y; y += step_y) {
            for (double x = x0; x <= box.right() + step_x; x += step_x) {
                painter.save();
                painter.translate(x, y);
                painter.rotate(degrees);
                painter.drawPath(markerPath(ps.shape, size));
                painter.restore();
            }
        }

        painter.setBrush(Qt::NoBrush);
        painter.restore();
    }

    /// One glyph at the centre of each face.
    static void drawCentroidFill(QPainter& painter, const render::PolygonBatch& batch,
                                 const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 6.0;
        painter.setPen(QPen(faded(ps.line_rgba, ps.opacity), static_cast<qreal>(ps.line_width_px)));
        painter.setBrush(glyph_brush(ps));

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
            // ROTATED, which it was not: the angle was read into the pass and then
            // ignored here, so a `cizik` asked to lie ACROSS a circle — the rule in
            // MPYY's `yapılaşma koşulu` gösterim — stayed upright and cut the two
            // numbers in half instead of separating them.
            painter.save();
            painter.translate((min_x + max_x) * 0.5, (min_y + max_y) * 0.5);
            painter.rotate(static_cast<double>(ps.angle_udeg) / 1'000'000.0);
            painter.drawPath(markerPath(ps.shape, size));
            painter.restore();
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
    ///
    /// The decoding itself is `symbol_image.hpp`, shared with the GPU backend —
    /// the alpha keying is a decision about what counts as paper, and a second
    /// copy of it is a drawing whose symbols look different depending on which
    /// engine drew them.
    const QImage& decoded(const render::PassStyle& ps, int wanted_px = 0)
    {
        static const QImage kNone;
        if (ps.image.empty() || ps.image_key == 0) return kNone;

        // A VECTOR picture is rasterised at the size it will be drawn at, so the
        // cache is keyed on that size too.
        const int bucket        = std::clamp(wanted_px, 8, 512);
        const std::uint64_t key = ps.image_key ^ (static_cast<std::uint64_t>(bucket) << 48);

        const auto it = images_.find(key);
        if (it != images_.end()) return it->second;

        return images_.emplace(key, decode_symbol_image(ps.image, bucket)).first->second;
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

        const double tile = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 24.0;

        const QImage& source = decoded(ps, static_cast<int>(std::lround(tile)));
        if (source.isNull()) return;

        const double ratio = source.height() > 0 ? double(source.height()) / source.width() : 1.0;

        QImage scaled = source.scaled(std::max(1, int(tile)), std::max(1, int(tile * ratio)),
                                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // SPACED, when the pass asks for it. A brush tiles its picture edge to
        // edge, which is right for a scanned hatch — the crop already contains
        // whatever spacing the annex printed. It is wrong for a symbol somebody
        // DREW: a drawn glyph fills its own box, so tiling it edge to edge puts
        // one glyph hard against the next and the pattern reads as a solid mat.
        // The picture is padded into a larger transparent cell instead, and the
        // cell is what tiles.
        const double gap = static_cast<double>(ps.interval_px);
        if (gap > scaled.width() || gap > scaled.height()) {
            const int cell_w = std::max(scaled.width(), static_cast<int>(std::lround(gap)));
            const int cell_h =
                std::max(scaled.height(), static_cast<int>(std::lround(gap * ratio)));

            QImage cell(cell_w, cell_h, QImage::Format_ARGB32_Premultiplied);
            cell.fill(Qt::transparent);

            QPainter into(&cell);
            into.drawImage(
                QPointF((cell_w - scaled.width()) * 0.5, (cell_h - scaled.height()) * 0.5), scaled);
            into.end();
            scaled = cell;
        }

        QBrush brush(scaled);
        if (ps.angle_udeg != 0) {
            QTransform rotation;
            rotation.rotate(-static_cast<double>(ps.angle_udeg) / 1'000'000.0);
            brush.setTransform(rotation);
        }

        // Composited like every other scanned picture, and it was not.
        //
        // A tarama tile is black lines on OPAQUE WHITE — the annex's paper. Poured
        // through a plain texture brush that paper covers the fill colour the same
        // row declares, so a MEVCUT KONUT ALANI came out white-on-white instead of
        // brown hatched black. The other two raster paths already multiplied; this
        // one did not, and 277 of the package's 476 rows go through it.
        painter.save();
        applyInkComposition(painter, ps);
        painter.fillPath(fillPath(batch, cx, cy), brush);
        painter.restore();
    }

    /// The picture once at the centre of each face — a MPYY `sembol` in its lekesi.
    void drawRasterCentres(QPainter& painter, const render::PolygonBatch& batch,
                           const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        const double height = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 16.0;

        const QImage& source = decoded(ps, static_cast<int>(std::lround(height)));
        if (source.isNull()) return;

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

    /// Sets a picture pass's opacity. The paper is already gone by now — see
    /// `keyed()`, which turns it into transparency at decode time rather than
    /// leaving every stamp to darken its way around it.
    static void applyInkComposition(QPainter& painter, const render::PassStyle& ps)
    {
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

        const double height = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 16.0;

        const QImage& source = decoded(ps, static_cast<int>(std::lround(height)));
        if (source.isNull()) return;

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
    /// ALONG THE WHOLE RUN, by arc length, and not edge by edge.
    ///
    /// The earlier version walked each edge on its own, kept a half-stamp margin at
    /// both of its ends, and restarted the phase at every vertex. Each of those was
    /// defensible alone and together they produced the picture a user reported: a
    /// visible HOLE at every corner of every parcel, a spacing that changed from
    /// edge to edge because each edge divided its own length, and — on a boundary
    /// whose edges are shorter than one stamp — nothing drawn at all, because
    /// `distribute_along` refuses an edge it cannot fit a stamp inside.
    ///
    /// A boundary is one line, so it is walked as one line. The pitch is constant
    /// all the way round, a stamp that lands on a vertex is drawn there, and each
    /// stamp takes the direction of the edge it lands on. Corners close.
    ///
    /// The overshoot the margin was protecting against is real — a picture is wide
    /// and a stamp near a corner leans past it — but it is a property of stamping
    /// pictures, not of walking them, and it does not survive the move to vector
    /// line types. Trading a certain hole at every corner for an occasional lean at
    /// a sharp one is the better of the two.
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

        // A raster marker on an OPEN line sits once, in the middle of the whole run:
        // that is where a plan puts a sembol on a linear feature. On a face it never
        // reaches here — `drawRasterCentres` places it in the lekesi.
        if (ps.type == core::SymbolLayerType::RasterMarker) {
            double total = 0.0;
            for (std::uint32_t v = 1; v < run; ++v)
                total += lengthOf(at(v - 1), at(v));
            if (total <= 0.0) return;

            double walked = 0.0;
            for (std::uint32_t v = 1; v < run; ++v) {
                const QPointF a  = at(v - 1);
                const QPointF b  = at(v);
                const double len = lengthOf(a, b);
                if (walked + len < total * 0.5) {
                    walked += len;
                    continue;
                }
                const double t = len > 0.0 ? (total * 0.5 - walked) / len : 0.0;
                stamp(QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t), 0.0);
                return;
            }
            return;
        }

        // The pitch is chosen ONCE for the whole run: the multiple of the requested
        // interval that divides the run's own length most evenly. The pattern then
        // closes on a ring instead of leaving whatever the last division did not
        // use as a gap beside the first stamp.
        double total = 0.0;
        for (std::uint32_t v = 1; v < run; ++v)
            total += lengthOf(at(v - 1), at(v));
        if (total <= 0.0) return;

        const auto steps   = static_cast<long long>(total / interval + 0.5);
        const double pitch = steps >= 1 ? total / static_cast<double>(steps) : total;
        if (pitch <= 0.0) return;

        // Walked, not indexed: `walked` accumulates along the polyline and the
        // next stamp is placed wherever that crosses the next multiple of `pitch`.
        // A vertex is nothing special to the walk, which is exactly why the corner
        // stops being a hole.
        double walked = 0.0;
        double next   = 0.0;

        for (std::uint32_t v = 1; v < run; ++v) {
            const QPointF a  = at(v - 1);
            const QPointF b  = at(v);
            const double len = lengthOf(a, b);
            if (len <= 0.0) continue;

            const double degrees = segmentDegrees(a, b);

            while (next <= walked + len + 1.0e-9) {
                const double t = (next - walked) / len;
                stamp(QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t), degrees);
                next += pitch;
            }
            walked += len;
        }
    }

    static void drawSimpleLine(QPainter& painter, const render::PolylineBatch& batch,
                               const render::PassStyle& ps, double cx, double cy)
    {
        if (batch.runs.empty()) return;

        QPen pen(faded(batch.rgba, ps.opacity));
        pen.setWidthF(static_cast<qreal>(batch.width_px));
        pen.setCapStyle(qt_cap(ps.cap));
        pen.setJoinStyle(qt_join(ps.join));
        apply_dash(pen, ps);
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
        painter.setPen(QPen(faded(ps.line_rgba, ps.opacity), static_cast<qreal>(ps.line_width_px)));
        // A hash tick is a stroke and has no interior to fill.
        painter.setBrush(hash ? QBrush(Qt::NoBrush) : glyph_brush(ps));

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

        // THE PHASE. Where the first marker sits, measured along the line, before
        // the interval takes over. Half an interval is the old behaviour and stays
        // the default, because a marker that starts ON the first vertex reads as
        // part of the corner rather than as one of a series.
        //
        // It is what lets two marker lines at one interval say different things:
        // MPYY's ETAPLAMA SINIRI alternates a filled circle with an open one, and
        // its ÜLKE SINIRI puts a tick at each END of a heavy bar. Both are two
        // lines at the same spacing, half a step and a whole bar apart.
        const double phase = ps.phase_px > 0.0f ? static_cast<double>(ps.phase_px) : interval * 0.5;
        double next        = ps.placement == MarkerPlacement::Centre ? target : phase;

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
    ///
    /// MULTI-LINE. A newline stacks the lines centred on the baseline, which is
    /// what MPYY's `yapılaşma koşulu` gösterim needs: a circle with TAKS over a
    /// rule and KAKS under it is two lines and a stroke, not one string with a
    /// slash in it.
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

            const QFontMetricsF metrics(font);
            const QStringList lines =
                QString::fromStdString(item.text).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (lines.isEmpty()) continue;

            // A quarter of the height between lines: tight enough that two numbers
            // read as one fraction, loose enough that they do not touch.
            const double step = metrics.height() * 1.25;

            painter.save();
            painter.translate(start);
            painter.rotate(degrees);

            for (int line = 0; line < lines.size(); ++line) {
                const double advance = metrics.horizontalAdvance(lines[line]);

                // The anchor decides where the baseline sits under the glyphs.
                // Measured from the real font rather than from the advance guess
                // the command used for the bounding box.
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

                // Stacked around the anchor, so a two-line label sits centred on
                // the point rather than hanging below it.
                shift_y += (line - (static_cast<int>(lines.size()) - 1) / 2.0) * step;

                painter.drawText(QPointF(shift_x, shift_y), lines[line]);
            }
            painter.restore();
        }
    }

    /// The grid, the selection, the snap glyph, the crosshair, the rubber band.
    ///
    /// All of it arrives as runs of widget-space points, so this loop is the same
    /// four lines whatever it is drawing. The shapes — a square for an endpoint, a
    /// bowtie for nearest — are chosen by the canvas, because which glyph means
    /// which aid is a decision about the product and not about the renderer.

public:
    /// Draws the aids over a frame someone else painted.
    ///
    /// Public so the QGIS backend can call it. The grid, the ruler, the scale bar,
    /// the snap marker and the crosshair are the PROGRAM's own furniture, not
    /// symbology, and there is no reason for a second engine to carry a second
    /// copy of them — a second copy is a second thing to keep in step, which is
    /// the objection CLAUDE.md 5.10 makes about lists.
    static void paint_ground(QPainter& painter, const render::Overlay& overlay)
    {
        drawOverlay(painter, overlay, 0, overlay.beneath);
    }

    static void paint_aids(QPainter& painter, const render::DrawList& list,
                           const render::Overlay& overlay, double cx, double cy)
    {
        drawTexts(painter, list, cx, cy);
        drawOverlay(painter, overlay, overlay.beneath, overlay.batches.size());
    }

private:
    /// Draws `[from, to)` of the overlay's batches.
    static void drawOverlay(QPainter& painter, const render::Overlay& overlay, std::size_t from,
                            std::size_t to)
    {
        for (std::size_t i = from; i < to && i < overlay.batches.size(); ++i) {
            const auto& batch = overlay.batches[i];
            if (batch.runs.empty()) continue;

            QPen pen(from_rgba(batch.rgba));
            pen.setWidthF(static_cast<qreal>(batch.width_px));
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

        const QFont uiFace = painter.font();
        for (const auto& label : overlay.labels) {
            if (label.text.empty()) continue;

            // A ruler division, a coordinate and a measurement are all NUMBERS,
            // and design.md §3 puts every number in the monospaced face — digits
            // that line up column-wise are what makes a coordinate readable at a
            // glance. The label says which face it wants; the backend obeys.
            QFont font = label.mono ? QFont(QStringLiteral("IBM Plex Mono")) : uiFace;
            if (label.mono) font.setStyleHint(QFont::Monospace);
            if (label.px > 0.0f) font.setPixelSize(static_cast<int>(label.px));
            painter.setFont(font);

            painter.setPen(from_rgba(label.rgba));
            painter.drawText(QPointF(static_cast<double>(label.x), static_cast<double>(label.y)),
                             QString::fromStdString(label.text));
        }
    }
};

} // namespace

void paint_frame_ground(QPainter& painter, const render::Overlay& overlay)
{
    PainterBackend::paint_ground(painter, overlay);
}

void paint_frame_aids(QPainter& painter, const render::DrawList& list,
                      const render::Overlay& overlay, double cx, double cy)
{
    PainterBackend::paint_aids(painter, list, overlay, cx, cy);
}

std::unique_ptr<render::Backend> make_builtin_backend()
{
    return std::make_unique<PainterBackend>();
}

std::unique_ptr<render::Backend> make_preview_backend()
{
    // The QPainter backend, always, because a preview's target is a QImage. See
    // the note on the declaration for why this is not `make_canvas_backend()`.
#if KENTOS_HAVE_QGIS
    if (qgetenv("KENTOS_BACKEND") != "dahili") return make_qgis_backend();
#endif
    return make_builtin_backend();
}

std::unique_ptr<render::Backend> make_canvas_backend()
{
    // THE ONE PLACE A BACKEND IS NAMED (render.md R1), and the canvas is not
    // edited when the choice changes — the same claim Article 8.1 makes about the
    // QRhi backend's arrival.
    //
    // QGIS when this build has it: a symbology engine is a thing Article 2.7 says
    // is used rather than reimplemented, and the built-in one is the stand-in.
    // The environment override is for looking at the two side by side while the
    // port finishes, and it names the built-in one rather than hiding it.
    // The GPU backend when this build has one, and then WITHOUT an override. The
    // canvas is a `QRhiWidget` in that build and a QPainter backend has nothing to
    // paint into there: `KENTOS_BACKEND=dahili` on a GPU build would hand the
    // painter a null device, which is a blank canvas rather than a comparison.
    // Comparing the two engines means configuring with -DKENTOS_WITH_RHI=OFF.
#if KENTOS_HAVE_RHI
    return make_rhi_backend();
#else
#if KENTOS_HAVE_QGIS
    if (qgetenv("KENTOS_BACKEND") != "dahili") return make_qgis_backend();
#endif
    return make_builtin_backend();
#endif
}

} // namespace kentos::app
