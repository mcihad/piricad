// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the QGIS symbology backend, when this build has one.
//
// Declared apart from `backend_factory.hpp` so the factory can choose between
// backends without either implementation's headers reaching it — QGIS pulls 2681
// headers and the factory needs none of them.
#pragma once

#include "piricad/render/backend.hpp"

#include <memory>
#include <string>

namespace piricad::app {

/// Brings the QGIS symbol, provider and SVG registries up, once per process.
/// Idempotent; every symbol this backend builds assumes it has run.
bool qgis_engine_ready();

/// The QGIS version this build draws through, for the About box and `make doctor`.
std::string qgis_engine_version();

/// Creates the QGIS-backed canvas backend.
std::unique_ptr<render::Backend> make_qgis_backend();

} // namespace piricad::app
