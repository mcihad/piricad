// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: what a symbol looks like, drawn small.
//
// The picture that appears beside a layer in the tree, beside a row on the symbol
// shelf, and above the editor in the style designer.
//
// IT GOES THROUGH THE CANVAS BACKEND. Not a simplified painter that "looks about
// right", not a colour swatch: the same `render::Backend` that draws the map,
// handed a small `DrawList` and an image to draw into. A preview written its own
// way is a second implementation of one picture, and the day the two disagree the
// shelf shows a symbol the drawing will not have.
//
// That is also why the geometry is FIXED and representative rather than sampled
// from the document: a preview has to say what the symbol does, not what one
// parcel happens to look like.
#pragma once

#include "piricad/core/dash_store.hpp"
#include "piricad/core/image_store.hpp"
#include "piricad/core/style.hpp"
#include "piricad/render/backend.hpp"

#include <QIcon>
#include <QImage>
#include <QRectF>
#include <QSize>

class QPainter;

#include <cstdint>

namespace piricad::app {

/// What shape the preview draws the symbol on.
enum class PreviewShape {
    /// A closed rectangle inset in the box. What a fill, a hatch and an area
    /// gösterim are read on, and what a layer of parcels wants beside its name.
    Area,
    /// A zig-zag across the box. What a line type, a marker line and a hash line
    /// are read on: a straight line hides what a corner does to the pattern.
    Line,
    /// A single point in the middle. What a marker is read on, and the only shape
    /// on which a marker's size means what it says.
    Point,
};

/// The shape a symbol should be previewed on, from what its layers draw.
///
/// A guess, and an honest one: a symbol that fills is shown on an area, one that
/// only places glyphs on a point, everything else on a line. The user overrides it
/// with the geometry tabs, because a symbol built for parcels can perfectly well
/// be applied to a boundary and they are the ones who know which.
PreviewShape natural_shape(const core::Symbol& symbol);

/// Renders `symbol` into an image of `size`, on `background`.
///
/// `images` supplies the bytes a raster layer needs and is borrowed for the call.
/// An empty symbol produces an empty image rather than a blank one, so a caller
/// can tell "nothing declared" from "declared and invisible".
/// What the symbol is drawn ON.
///
/// A flat ground is right for a list icon, where the row's own colour is the
/// context. It is wrong for the DESIGNER's preview: a translucent fill over a
/// flat ground looks exactly like an opaque paler fill, so the one control that
/// is supposed to show what the symbol does hides the thing a user most needs to
/// see. A checkerboard shows through, which is why every image editor has one.
enum class PreviewGround {
    Flat,   ///< a single colour; list icons and small swatches
    Checker ///< a fine two-tone lattice; the designer's own preview
};

struct PreviewOptions
{
    std::uint32_t screen_ink{0};      ///< ink contrast in the editor, never in saved symbols
    double paper_pixels{0.0};         ///< zero: fit thumbnails; positive: explicit workspace zoom
    double scale_denominator{1000.0}; ///< ground millimetres per paper millimetre
    bool hole{false};                 ///< include an island to inspect fill clipping
    bool straight{false};             ///< use a straight sample instead of a bent boundary
};

/// Draws the symbol on `shape`, on `ground`.
///
/// `dpr` is the device pixel ratio to render at. A preview drawn at 1.0 and shown
/// on a 2x display is a blurred picture of a symbol whose whole job is to be
/// looked at closely; this renders at the ratio and tags the image with it, so
/// Qt maps it one device pixel per rendered pixel.
QImage symbol_preview(const core::Symbol& symbol, const core::ImageStore& images,
                      const core::DashStore& dashes, QSize size, std::uint32_t background,
                      PreviewShape shape, PreviewGround ground = PreviewGround::Flat,
                      qreal dpr = 1.0, PreviewOptions options = {});

/// Draws the symbol into an ALREADY-ACTIVE painter, inside `box`.
///
/// WHY THIS EXISTS BESIDE THE IMAGE FORM. A legend's key has to reach a PDF as
/// GEOMETRY. The image form is right for a list icon — a tree row is pixels
/// anyway — but blitting it onto a sheet puts a photograph of a symbol on a
/// document somebody signs: unmeasurable, unselectable and resolution-bound, and
/// it is the same defect the map frame had before it was made to paint through
/// the caller's painter (TODOS L-12, L-07).
///
/// The painter's state is left as it was found, and its clip and transform are
/// honoured. `box` is in the painter's own coordinates.
void paint_symbol(QPainter& painter, const QRectF& box, const core::Symbol& symbol,
                  const core::ImageStore& images, const core::DashStore& dashes, PreviewShape shape,
                  double device_pixel_ratio = 1.0);

/// The same picture as an icon, for a tree row or a list item.
QIcon symbol_icon(const core::Symbol& symbol, const core::ImageStore& images,
                  const core::DashStore& dashes, QSize size, std::uint32_t background,
                  PreviewShape shape);

} // namespace piricad::app
