// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the canvas backend factory.
//
// render.md R1: "Only the backend factory may name a backend implementation
// type." This header is the seam that makes that possible — the canvas asks for a
// backend and gets one, and the concrete type never appears in the widget.
//
// Which backend it returns is a build-time fact today (`PIRICAD_WITH_RHI`), and
// the canvas is written so that it stays a build-time fact it does not read:
// `Backend::name()` and `Backend::gpu()` are what the F12 overlay and the About
// box ask, not a preprocessor symbol.
#pragma once

#include "piricad/render/backend.hpp"

#include <memory>

class QPainter;

namespace piricad::app {

/// Creates the canvas backend for this build.
///
/// Never null: a build with no GPU backend returns the QPainter one, which is the
/// Phase-0 deviation of CLAUDE.md Article 8.1 and is reported as such rather than
/// pretended away.
std::unique_ptr<render::Backend> make_canvas_backend();

/// The built-in QPainter backend, named so the two can be compared. Used by the
/// factory and by a test that renders one document through both.
std::unique_ptr<render::Backend> make_builtin_backend();

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

} // namespace piricad::app
