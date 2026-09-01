// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/backend.hpp"

namespace piricad::render {

std::string gpu_backend_status()
{
#ifdef PIRICAD_HAVE_RHI
    return {};
#else
    return "QRhi GPU canvas is not built: configure with -DPIRICAD_WITH_RHI=ON. It "
           "needs Qt 6.7+, Qt Shader Tools for `qsb` and Qt Gui's private headers "
           "for <rhi/qrhi.h>; src/app/CMakeLists.txt names the package for each. "
           "The QPainter backend is active. See CLAUDE.md Article 8.";
#endif
}

} // namespace piricad::render
