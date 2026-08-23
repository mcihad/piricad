// SPDX-License-Identifier: GPL-3.0-or-later
// QGIS QML style export — private to /src/io.
#pragma once

#include "piricad/core/layer.hpp"
#include "piricad/core/style.hpp"

#include <string>

namespace piricad::io {

/// Builds a QGIS 3 QML style document for one layer's symbology.
///
/// `area` selects the symbol type QGIS expects: a fill symbol for a layer of
/// faces, a line symbol for one of open geometry. The caller decides from the
/// layer's contents, because the document knows and this function should not
/// have to guess.
std::string build_qml(const core::Layer& layer, const core::Symbol& symbol, bool area);

} // namespace piricad::io
