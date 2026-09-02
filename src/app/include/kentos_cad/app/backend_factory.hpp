// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the canvas backend factory.
//
// render.md R1: "Only the backend factory may name a backend implementation
// type." This header is the seam that makes that possible — the canvas asks for a
// backend and gets one, and the concrete type never appears in the widget.
//
// Which backend it returns is a build-time fact today (`KENTOS_WITH_RHI`), and
// the canvas is written so that it stays a build-time fact it does not read:
// `Backend::name()` and `Backend::gpu()` are what the F12 overlay and the About
// box ask, not a preprocessor symbol.
#pragma once

#include "kentos_cad/render/backend.hpp"

#include <memory>

class QPainter;
class QRhi;
class QRhiCommandBuffer;
class QRhiRenderTarget;

namespace kentos::app {

/// The QRhi handles one frame needs, behind `FrameContext::target`.
///
/// `FrameContext::target` is a `void*` because `/src/render` may not name a Qt
/// type (CLAUDE.md 3.4). The QPainter backend reads it as a `QPaintDevice*`; the
/// GPU backend reads it as this. Three handles rather than the widget, because a
/// backend that reached back into the widget would be a backend the symbol
/// previews could not use.
struct RhiFrameTarget
{
    QRhi* rhi{nullptr};             ///< the device, for buffers and pipelines
    QRhiCommandBuffer* cb{nullptr}; ///< this frame's command buffer, already open
    QRhiRenderTarget* rt{nullptr};  ///< where the frame goes, with its pass descriptor
};

/// Packs this frame's handles into the opaque pointer the canvas hands over.
///
/// The canvas calls this instead of naming `RhiFrameTarget` itself: render.md R1
/// keeps backend knowledge inside this seam, and a widget that filled the struct
/// would be a widget that knows what the live backend expects.
void* rhi_frame_target(QRhi* rhi, QRhiCommandBuffer* cb, QRhiRenderTarget* rt);

/// Creates the canvas backend for this build.
///
/// Never null: a build with no GPU backend returns the QPainter one, which is the
/// Phase-0 deviation of CLAUDE.md Article 8.1 and is reported as such rather than
/// pretended away.
std::unique_ptr<render::Backend> make_canvas_backend();

/// The built-in QPainter backend, named so the two can be compared. Used by the
/// factory and by a test that renders one document through both.
std::unique_ptr<render::Backend> make_builtin_backend();

/// The GPU backend. Only built when `KENTOS_WITH_RHI=ON`; declared here because
/// this header is the one place a backend implementation may be named (R1).
std::unique_ptr<render::Backend> make_rhi_backend();

/// The backend for a frame that goes into a `QPaintDevice` rather than onto the
/// canvas — the symbol previews on the layer shelf and in the style designer.
///
/// NOT `make_canvas_backend()`, and the difference is the surface. A preview
/// renders into a `QImage`, and the GPU backend has nowhere to put one: a QRhi
/// frame needs a command buffer and a render target, and rendering to an image
/// means an offscreen texture target this slice does not build yet (CLAUDE.md
/// 8.1). Handing it a `QPaintDevice*` through `FrameContext::target` is not a
/// degraded picture, it is a wild pointer — which is exactly what it was, until
/// the style designer opened on a GPU build and segfaulted on the first preview.
///
/// So the two callers ask different questions. When the GPU backend can render
/// to a texture, this returns it and the previews go back to being drawn by
/// whatever draws the canvas, which is what `render.md` R1 wants.
std::unique_ptr<render::Backend> make_preview_backend();

/// Draws the OVERLAY — grid, ruler, scale bar, north arrow, snap marker,
/// crosshair, selection box — and the document's own captions, over a frame that
/// something else has already painted.
///
/// Every backend needs these and none of them is symbology: they are the
/// program's own furniture. A backend that carried its own copy would be a second
/// list to keep in step, and the day the two disagree the user sees one grid on
/// one engine and another on the other. The QGIS backend draws symbols and calls
/// this; the built-in one calls it too, at the same point in the frame.
///
/// `painter` must be active on the frame's device, and `cx`/`cy` are the widget
/// centre the draw list's coordinates are relative to.
/// The part of the overlay that goes UNDER the document: the grid.
///
/// Every backend owes the user this too, and in this order — the grid is the
/// paper the drawing sits on. Drawn after the passes it covers the map with a
/// lattice of grey lines, which is what a user sees first and reports first.
void paint_frame_ground(QPainter& painter, const render::Overlay& overlay);

void paint_frame_aids(QPainter& painter, const render::DrawList& list,
                      const render::Overlay& overlay, double cx, double cy);

} // namespace kentos::app
