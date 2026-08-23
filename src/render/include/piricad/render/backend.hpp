// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — render: the backend interface.
//
// Target (piricad.md §6.3): a GPU pipeline inside QRhiWidget.
// Phase 0 deviation (CLAUDE.md Article 8): `qsb` from qt6-shadertools is not
// available on this machine, so shader packs cannot be baked and the QRhi backend
// cannot be built. The application ships the QPainter backend meanwhile. Nothing
// above this interface knows which backend is live.
#pragma once

#include "piricad/render/drawlist.hpp"

#include <string>

namespace piricad::render {

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

    /// Draws one frame. Called on the render surface's thread.
    virtual void render(const DrawList& list) = 0;
};

/// Why the GPU backend is unavailable in this build, or an empty string when it is
/// available. Reported by `make doctor` and by the application's About box, so the
/// Phase-0 deviation is visible rather than silent.
std::string gpu_backend_status();

} // namespace piricad::render
