// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/backend.hpp"

namespace kentos::render {

std::string text_backend_status()
{
#ifdef KENTOS_HAVE_TEXT
    return {};
#else
    // A BUILD THAT DRAWS NO CAPTIONS MUST SAY SO. The SDF atlas is optional, and
    // the QRhi canvas's whole text path is compiled out with it — so a plan sheet
    // with 13 112 ada and parsel numbers on it comes up correct, fast, and
    // completely silent about the numbers it is not drawing. Reported here rather
    // than discovered by a surveyor who thinks the import lost them.
    return "Yazı atlası bu yapıda yok: -DKENTOS_WITH_TEXT=ON ile yapılandırın. "
           "FreeType 2.10+ ve HarfBuzz ister (Debian/Ubuntu: libfreetype-dev "
           "libharfbuzz-dev). GPU tuvali yazıları BU YAPIDA ÇİZMEZ; çizimdeki "
           "metin nesneleri yerinde durur, yalnızca görünmezler.";
#endif
}

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
