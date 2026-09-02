// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): putting a freshly read project in place of the live one.
//
// SHARED because two things open a project now — `AÇ` from a file and
// `VERİTABANI projeac` from a database — and the delicate part is identical for
// both: read into a FRESH document, resolve the CRS the file named, and swap only
// once the last byte is in. A second copy of that sequence is a second place for
// the swap to go wrong, and the failure mode is the user's drawing.
//
// This header is NOT public: it takes a `command::Bus&` and belongs to /src/io's
// own service layer.
#pragma once

#include "kentos_cad/io/project.hpp"

#include "kentos_cad/command/bus.hpp"

#include <stop_token>
#include <string>

namespace kentos::io {

/// Reads `path` and, only on success, makes it the bus's document.
///
/// ON FAILURE THE BUS IS UNTOUCHED — not partly loaded, not emptied. The read
/// happens into a local document and a local settings store, and the swap is the
/// last thing that happens (io.md P11).
///
/// Clears the undo stack, because its slots refer to a document that no longer
/// exists, and resets the active layer. That is what every CAD and GIS
/// application these users know does on File > Open.
///
/// Does NOT touch `FileService::current_path_`: where the project came FROM is
/// the caller's business, and for a database project the answer is "not a file".
///
/// `path` is taken by value for the reason `project.hpp` gives: a coroutine does
/// not copy its reference parameters into its frame.
command::Task<core::Result<ProjectReport>> adopt_project(command::Bus& bus, std::string path,
                                                         std::stop_token stop);

} // namespace kentos::io
