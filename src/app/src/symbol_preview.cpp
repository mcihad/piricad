// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/symbol_preview.hpp"

#include "piricad/app/backend_factory.hpp"
#include "piricad/render/scene.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <span>

namespace piricad::app {
namespace {

/// Millimetres per pixel a preview pretends to be looking at the drawing with.
///
/// A preview has no view, but a symbol's ground-unit measures need one: a hatch
/// spacing declared in ground millimetres is meaningless without a scale. This is
/// the working zoom a planner edits at — about a metre across the swatch — chosen
/// so a pattern declared for a plan sheet is legible at swatch size rather than
/// collapsing into a grey wash.
constexpr double kPreviewMmPerPixel = 40.0;

/// Screen pixels per paper millimetre in a preview.
///
/// The same resolution the canvas uses, so a swatch is a picture of what the
/// drawing will show rather than a smaller drawing of it.
constexpr double kPreviewPaperPixels = render::kDefaultPixelsPerPaperMm;

/// The room a stamp has on the geometry the preview draws, in pixels.
///
/// For a line that is the SHORTEST SEGMENT, because a picture stamped along a run
/// is placed inside one segment and skipped entirely when it does not fit
/// (`render::distribute_along` refuses an edge shorter than the stamp). For a
/// face or a point it is the shorter side of the box.
double room_for(PreviewShape shape, double w, double h)
{
    if (shape != PreviewShape::Line) return 2.0 * std::min(w, h);

    // The zigzag's first leg, which is its shortest; see the geometry below.
    return std::hypot(0.5 * w, 1.2 * h);
}

/// The proportions of an embedded picture, read from its header alone.
///
/// A stamped `çizgi tipi` is as wide as its picture, not as wide as its declared
/// size — the MPYY boundary images run two to one — and it is the WIDTH that
/// decides whether a stamp fits the run. `QImageReader` answers from the header,
/// so this costs no decode.
double picture_aspect(std::span<const std::byte> bytes)
{
    if (bytes.empty()) return 1.0;

    QByteArray raw = QByteArray::fromRawData(reinterpret_cast<const char*>(bytes.data()),
                                             static_cast<qsizetype>(bytes.size()));
    QBuffer buffer(&raw);
    if (!buffer.open(QIODevice::ReadOnly)) return 1.0;

    QImageReader reader(&buffer);
    const QSize measured = reader.size();
    if (!measured.isValid() || measured.height() <= 0) return 1.0;
    return static_cast<double>(measured.width()) / measured.height();
}

/// How the preview has to look at a symbol for the symbol to fit the swatch.
struct PreviewScale
{
    double mm_per_pixel{kPreviewMmPerPixel};         ///< reads Ground measures
    double pixels_per_paper_mm{kPreviewPaperPixels}; ///< reads Paper measures
};

/// ONE zoom, applied to both unit families.
///
/// Both, because a symbol that mixes them — a marker sized on paper repeated at a
/// ground interval — would otherwise have its own proportions rewritten by the
/// act of previewing it, and a preview that changes the symbol is not a preview.
///
/// It only ever SHRINKS. A symbol that already fits is drawn at exactly the
/// canvas scale, which is what makes a swatch a promise about the drawing rather
/// than a picture of its own.
///
/// Two things made this necessary at once. The MPYY building-condition symbol
/// declares a 26 m circle, which at the working zoom is 650 px across and misses
/// a 44 px swatch entirely. And once a paper millimetre became a real one, an 8 mm
/// MPYY line type became a 30 px tall, 60 px wide stamp — wider than the whole
/// icon, so `distribute_along` refused every position and the shelf went blank.
PreviewScale fit_to_swatch(const core::Symbol& symbol, const core::ImageStore& images,
                           PreviewShape shape, QSize size, double w, double h)
{
    PreviewScale out;

    const double room = room_for(shape, w, h);
    if (room <= 1.0 || size.isEmpty()) return out;

    const auto in_pixels = [&](const core::Measure& m) {
        switch (m.unit) {
        case core::Unit::Paper:
            return std::abs(static_cast<double>(m.value)) / 1000.0 * out.pixels_per_paper_mm;
        case core::Unit::Ground: return std::abs(static_cast<double>(m.value)) / out.mm_per_pixel;
        case core::Unit::Pixel: return std::abs(static_cast<double>(m.value));
        }
        return 0.0;
    };

    double widest = 0.0;
    for (const core::SymbolLayer& l : symbol.layers) {
        if (!l.enabled) continue;

        double footprint = in_pixels(l.size) + in_pixels(l.offset);

        // A stamped picture is as wide as its own proportions make it, and width
        // is what has to fit.
        if (l.type == core::SymbolLayerType::RasterLine ||
            l.type == core::SymbolLayerType::RasterMarker)
            footprint *= picture_aspect(images.bytes(l.image));

        // A tiling fill is read from its REPEAT, so it is given room for two: one
        // tile filling the swatch is a flat colour, not a pattern.
        if (l.type == core::SymbolLayerType::RasterFill ||
            l.type == core::SymbolLayerType::LinePatternFill ||
            l.type == core::SymbolLayerType::PointPatternFill)
            footprint *= 2.0;

        widest = std::max(widest, footprint);
    }
    if (widest <= 0.0) return out;

    // Slightly inside the room rather than exactly on it, so the stamp that just
    // fits is placed rather than lost to a rounding comparison.
    const double zoom = std::min(1.0, 0.85 * room / widest);
    if (zoom >= 1.0) return out;

    out.mm_per_pixel /= zoom;
    out.pixels_per_paper_mm *= zoom;
    return out;
}

/// The pass table for one symbol, plus the geometry to draw it on.
///
/// Built by hand rather than through `build_scene`, because there is no document
/// here — but through the SAME `pass_of` the scene builder uses, so every unit
/// conversion and every type decision is the one the canvas makes.
render::DrawList build(const core::Symbol& symbol, const core::ImageStore& images,
                       const core::DashStore& dashes, QSize size, PreviewShape shape)
{
    render::DrawList list;
    if (symbol.layers.empty()) return list;

    // A layer switched off is not previewed, for the same reason it is not drawn.
    std::vector<const core::SymbolLayer*> drawn;
    drawn.reserve(symbol.layers.size());
    for (const core::SymbolLayer& l : symbol.layers)
        if (l.enabled) drawn.push_back(&l);
    if (drawn.empty()) return list;

    const auto count = drawn.size();
    list.passes.resize(count);
    list.polylines.resize(count);
    list.polygons.resize(count);
    list.order.resize(count);

    // Centre-relative with y UP, which is what a draw list holds; the backend
    // turns it into widget pixels. The inset keeps a wide stroke inside the
    // swatch instead of clipped at its edge.
    const float w = static_cast<float>(size.width()) * 0.5f - 3.0f;
    const float h = static_cast<float>(size.height()) * 0.5f - 3.0f;

    // A CORNER OR A LENGTH, whichever the swatch has room for.
    //
    // The zigzag exists to show what a corner does to a pattern, and in the big
    // preview it earns its place. In a 44 px list icon it does not: it cuts the
    // run into three stubs, each too short to hold one stamp of a published line
    // type, and the icon comes out empty. Below that width the swatch shows the
    // pattern instead of the corner, because a pattern nobody can see says less
    // about a corner than nothing at all.
    const bool corner = size.width() >= 120;

    const PreviewScale scale =
        fit_to_swatch(symbol, images, corner ? shape : PreviewShape::Point, size, w, h);
    const double mm_per_pixel = scale.mm_per_pixel;

    for (std::size_t i = 0; i < count; ++i) {
        const core::SymbolLayer& sl = *drawn[i];

        list.passes[i] =
            render::pass_of(sl, images, dashes, mm_per_pixel, scale.pixels_per_paper_mm);
        list.order[i] = static_cast<std::uint32_t>(i);

        render::PolylineBatch& stroke = list.polylines[i];
        render::PolygonBatch& fill    = list.polygons[i];
        stroke.rgba                   = sl.look.rgba;
        stroke.width_px               = render::stroke_width_px(sl, scale.pixels_per_paper_mm);
        fill.rgba                     = sl.look.fill_rgba;
        fill.hatch                    = sl.look.hatch;

        const auto push = [&](auto& batch, std::initializer_list<std::pair<float, float>> pts,
                              bool closed) {
            for (const auto& [x, y] : pts) {
                batch.xs.push_back(x);
                batch.ys.push_back(y);
            }
            batch.runs.push_back(static_cast<std::uint32_t>(pts.size()));
            (void)closed;
        };

        if (shape == PreviewShape::Area) {
            // A closed rectangle. The last vertex repeats the first because a draw
            // list holds an already-closed ring, exactly as the scene builder emits
            // one.
            if (list.passes[i].wants_stroke)
                push(stroke, {{-w, -h}, {w, -h}, {w, h}, {-w, h}, {-w, -h}}, true);
            if (list.passes[i].wants_fill) {
                push(fill, {{-w, -h}, {w, -h}, {w, h}, {-w, h}}, true);
                fill.is_hole.push_back(0);
            }
        } else if (shape == PreviewShape::Line) {
            // A zig-zag: a straight line hides what a corner does to a pattern,
            // and a corner is where the two placement defects showed up. In a
            // swatch too narrow to hold a stamp on one leg it straightens; see
            // `corner` above.
            const float q = w * 0.5f;
            if (list.passes[i].wants_stroke) {
                if (corner)
                    push(stroke, {{-w, -h * 0.6f}, {-q, h * 0.6f}, {q, -h * 0.6f}, {w, h * 0.6f}},
                         false);
                else
                    push(stroke, {{-w, 0.0f}, {w, 0.0f}}, false);
            }
        } else {
            // A point is a degenerate run of two coincident vertices, which is what
            // the marker path already walks. A single vertex would be dropped: the
            // scene builder emits nothing for a run of one.
            if (list.passes[i].wants_stroke) push(stroke, {{0.0f, 0.0f}, {0.0f, 0.0f}}, false);
        }
    }
    return list;
}

} // namespace

PreviewShape natural_shape(const core::Symbol& symbol)
{
    bool any_fill   = false;
    bool any_stroke = false;

    for (const core::SymbolLayer& l : symbol.layers) {
        if (!l.enabled) continue;
        if (core::draws_fill(l.type)) any_fill = true;
        if (core::draws_stroke(l.type)) any_stroke = true;
    }

    if (any_fill) return PreviewShape::Area;
    if (any_stroke) return PreviewShape::Line;
    return PreviewShape::Point;
}

QImage symbol_preview(const core::Symbol& symbol, const core::ImageStore& images,
                      const core::DashStore& dashes, QSize size, std::uint32_t background,
                      PreviewShape shape)
{
    if (symbol.layers.empty() || size.isEmpty()) return {};

    QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    render::DrawList list = build(symbol, images, dashes, size, shape);

    render::Overlay overlay;
    overlay.background_rgba = background;

    render::FrameContext ctx;
    ctx.width_px           = size.width();
    ctx.height_px          = size.height();
    ctx.device_pixel_ratio = 1.0f;
    ctx.target             = static_cast<QPaintDevice*>(&canvas);

    // A fresh backend per call rather than one kept alive: a preview is drawn when
    // a panel refreshes, not per frame, and the decoded-picture cache a backend
    // holds is worth keeping only for a surface that redraws.
    const std::unique_ptr<render::Backend> backend = make_canvas_backend();
    backend->render(list, overlay, ctx);
    return canvas;
}

QIcon symbol_icon(const core::Symbol& symbol, const core::ImageStore& images,
                  const core::DashStore& dashes, QSize size, std::uint32_t background,
                  PreviewShape shape)
{
    const QImage image = symbol_preview(symbol, images, dashes, size, background, shape);
    if (image.isNull()) return {};
    return QIcon(QPixmap::fromImage(image));
}

} // namespace piricad::app
