// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/backend.hpp"

namespace kentos::render {

std::string gpu_backend_status()
{
#ifdef KENTOS_HAVE_RHI
    return {};
#else
    return "QRhi GPU canvas is not built: configure with -DKENTOS_WITH_RHI=ON. It "
           "needs Qt 6.7+, Qt Shader Tools for `qsb` and Qt Gui's private headers "
           "for <rhi/qrhi.h>; src/app/CMakeLists.txt names the package for each. "
           "The QPainter backend is active. See CLAUDE.md Article 8.";
#endif
}

} // namespace kentos::render
