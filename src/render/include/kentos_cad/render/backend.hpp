// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — render: the backend interface.
//
// Target (kentoscad.md §6.3): a GPU pipeline inside QRhiWidget.
// Phase 0 deviation (CLAUDE.md Article 8.1): the QRhi backend exists behind
// `KENTOS_WITH_RHI`, which is OFF by default while it draws geometry but not yet
// text; the QPainter backend ships meanwhile. Nothing above this interface knows
// which one is live — that is the whole point of the interface.
#pragma once

#include "kentos_cad/render/drawlist.hpp"

#include <string>

namespace kentos::render {

/// What one frame needs that the draw list does not carry.
struct FrameContext
{
    int width_px{0};  ///< widget width in logical pixels
    int height_px{0}; ///< widget height in logical pixels

    /// Every pixel-space constant is scaled by this (render.md R19).
    float device_pixel_ratio{1.0f};

    /// The surface to draw into, opaque to /src/render.
    ///
    /// A `void*` because this header may not name a Qt type: `kentos_render`
    /// links no Qt (CLAUDE.md 3.4, Article 8.5) and the whole point of the
    /// interface is that nothing above it knows which backend is live. The
    /// backend that receives it is the only code that knows what it is — a paint
    /// device for the QPainter backend, a command buffer for the GPU one.
    ///
    /// WHATEVER IT IS, THE CALLER MUST HAND OVER THE ALREADY-CAST POINTER. The
    /// QPainter backend reads it as a `QPaintDevice*`, and a `QWidget*` shoved
    /// through `void*` is not that: QWidget inherits QObject and QPaintDevice
    /// both, so the two pointers have different addresses and the round trip
    /// silently produces garbage.
    ///
    /// A device rather than a widget on purpose: the symbol previews in the layer
    /// tree and the style designer render through the SAME backend into an image,
    /// so what a user sees on the shelf is what the canvas will draw. A second
    /// preview renderer is two implementations of one picture, and they diverge.
    void* target{nullptr};
};

class Backend
{
public:
    /// Virtual: a backend is owned polymorphically by the canvas.
    virtual ~Backend() = default;

    /// What this backend is, for the F12 overlay and the startup note.
    virtual std::string name() const = 0;

    /// Whether drawing happens on the GPU. False for the Phase-0 QPainter
    /// backend, which is why the transcript says so on startup (Article 8.1).
    virtual bool gpu() const = 0;

    /// Draws one frame: the document, then everything over it. Called on the
    /// render surface's thread.
    ///
    /// The overlay is a separate argument rather than a member of `DrawList`
    /// because the two have different lifetimes. The draw list is rebuilt when
    /// the document or the view changes; the overlay is rebuilt on every mouse
    /// move. Keeping them apart is what lets a later backend cache one and not
    /// the other.
    virtual void render(const DrawList& list, const Overlay& overlay, const FrameContext& ctx) = 0;
};

/// Why the GPU backend is unavailable in this build, or an empty string when it is
/// available. Reported by `make doctor` and by the application's About box, so the
/// Phase-0 deviation is visible rather than silent.
std::string gpu_backend_status();

} // namespace kentos::render
