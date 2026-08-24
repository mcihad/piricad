// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — render: builds a DrawList from a Document.
#pragma once

#include "piricad/core/document.hpp"
#include "piricad/render/drawlist.hpp"
#include "piricad/render/view.hpp"

namespace piricad::render {

/// Turns one symbol layer into a pass, converting every measure to pixels.
///
/// SHARED between the scene builder and the symbol previews in the layer tree and
/// the style designer, and that is the whole reason it is a function. A preview
/// drawn by its own code is a second implementation of one picture, and the day
/// they disagree the shelf shows a symbol the canvas will not draw.
///
/// `images` supplies the bytes a raster layer needs; it is BORROWED for as long
/// as the returned pass is used, which is the frame.
PassStyle pass_of(const core::SymbolLayer& layer, const core::ImageStore& images,
                  const core::DashStore& dashes, double mm_per_pixel, double pixels_per_paper_mm);

/// The stroke width one symbol layer draws with, in this frame's pixels.
///
/// Never below one: a line the renderer rounds away is a boundary the user cannot
/// see, and on a cadastral sheet a boundary is the legal edge.
float stroke_width_px(const core::SymbolLayer& layer, double pixels_per_paper_mm);

/// Screen pixels one PAPER millimetre covers, when nobody says otherwise.
///
/// 96 dpi, which is what a desktop reports when it has no better answer and what
/// QGIS defaults its render context to. It is a resolution, not a taste: a symbol
/// the annex prints at 8 mm has to arrive on screen at the size it will be
/// printed, or the plan on the monitor is not the plan on the sheet.
constexpr double kDefaultPixelsPerPaperMm = 96.0 / 25.4;

struct SceneOptions
{
    /// Screen pixels per paper millimetre — the output resolution, from the
    /// screen the canvas is on. See `kDefaultPixelsPerPaperMm`.
    double pixels_per_paper_mm{kDefaultPixelsPerPaperMm};

    bool cull{true}; ///< frustum cull against the visible box (§10.3)
    bool lod{true};  ///< drop vertices below one pixel of separation (§10.3)
    /// A vertex closer than this to its predecessor cannot be told apart on
    /// screen and is dropped. Strokes only — a face keeps every vertex, because
    /// dropping one changes the shape being coloured.
    double lod_pixels{0.75};
};

/// Rebuilds `out` for the current view. Allocation is reused between frames:
/// the draw loop must not allocate per frame (.claude/render.md).
void build_scene(const core::Document& doc, const ViewTransform& view, const SceneOptions& options,
                 DrawList& out);

} // namespace piricad::render
