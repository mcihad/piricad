// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/backend.hpp"

namespace piricad::render {

std::string gpu_backend_status()
{
#ifdef PIRICAD_HAVE_RHI
    return {};
#else
    return "QRhi GPU canvas is not built: qt6-shadertools (qsb) is missing, so the "
           "shader packs cannot be baked. The QPainter backend is active. "
           "See CLAUDE.md Article 8.";
#endif
}

} // namespace piricad::render
