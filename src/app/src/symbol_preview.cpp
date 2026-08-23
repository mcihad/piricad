// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/symbol_preview.hpp"

#include "piricad/app/backend_factory.hpp"
#include "piricad/render/scene.hpp"

#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <memory>

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

/// The pass table for one symbol, plus the geometry to draw it on.
///
/// Built by hand rather than through `build_scene`, because there is no document
/// here — but through the SAME `pass_of` the scene builder uses, so every unit
/// conversion and every type decision is the one the canvas makes.
render::DrawList build(const core::Symbol& symbol, const core::ImageStore& images, QSize size,
                       PreviewShape shape)
{
    render::DrawList list;
    if (symbol.layers.empty()) return list;

    const auto count = symbol.layers.size();
    list.passes.resize(count);
    list.polylines.resize(count);
    list.polygons.resize(count);
    list.order.resize(count);

    // Centre-relative with y UP, which is what a draw list holds; the backend
    // turns it into widget pixels. The inset keeps a wide stroke inside the
    // swatch instead of clipped at its edge.
    const float w = static_cast<float>(size.width()) * 0.5f - 3.0f;
    const float h = static_cast<float>(size.height()) * 0.5f - 3.0f;

    for (std::size_t i = 0; i < count; ++i) {
        const core::SymbolLayer& sl = symbol.layers[i];

        list.passes[i] = render::pass_of(sl, images, kPreviewMmPerPixel);
        list.order[i]  = static_cast<std::uint32_t>(i);

        render::PolylineBatch& stroke = list.polylines[i];
        render::PolygonBatch& fill    = list.polygons[i];
        stroke.rgba                   = sl.look.rgba;
        stroke.width_px               = render::stroke_width_px(sl);
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
        } else {
            // A zig-zag: a straight line hides what a corner does to a pattern,
            // and a corner is where the two placement defects showed up.
            const float q = w * 0.5f;
            if (list.passes[i].wants_stroke)
                push(stroke, {{-w, -h * 0.6f}, {-q, h * 0.6f}, {q, -h * 0.6f}, {w, h * 0.6f}},
                     false);
        }
    }
    return list;
}

} // namespace

QImage symbol_preview(const core::Symbol& symbol, const core::ImageStore& images, QSize size,
                      std::uint32_t background, PreviewShape shape)
{
    if (symbol.layers.empty() || size.isEmpty()) return {};

    QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    render::DrawList list = build(symbol, images, size, shape);

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

QIcon symbol_icon(const core::Symbol& symbol, const core::ImageStore& images, QSize size,
                  std::uint32_t background, PreviewShape shape)
{
    const QImage image = symbol_preview(symbol, images, size, background, shape);
    if (image.isNull()) return {};
    return QIcon(QPixmap::fromImage(image));
}

} // namespace piricad::app
