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

} // namespace piricad::app
