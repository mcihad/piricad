// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: an embedded symbol picture, decoded.
//
// MPYY publishes part of its symbology as PICTURES — a hatch for `orman`, a glyph
// for `cami`, a line type for `il sınırı` — and 161 of the annex's 1 574 symbol
// layers are one of the three raster kinds. The bytes travel inside the document
// (`core::image_store.hpp`); turning them into pixels is this.
//
// SHARED BY BOTH BACKENDS, because the two must produce the SAME picture. The
// alpha keying below is a decision about what counts as paper, and a second copy
// of that decision is a drawing whose symbols look different depending on which
// engine drew it.
#pragma once

#include <QImage>

#include <cstddef>
#include <span>

namespace kentos::app {

/// Decodes an embedded picture at the size it will be drawn.
///
/// `wanted_px` is the intended HEIGHT in pixels and matters only for a vector
/// source: an SVG is rendered at that size rather than decoded once and resampled,
/// which is the whole reason for shipping symbology as SVG. Every resampling of a
/// 176 px annex crop is a softer, greyer version of a line the regulation drew
/// crisp.
///
/// Returns a null image when the bytes cannot be read. That is the honest result —
/// the drawing says there is a picture and this build cannot read it — and the
/// caller is expected to cache the null so the failure costs one attempt rather
/// than one per frame.
QImage decode_symbol_image(std::span<const std::byte> bytes, int wanted_px);

} // namespace kentos::app
