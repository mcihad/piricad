// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: surveyed point lists.
//
// THE FIRST FILE A TURKISH SURVEYOR OPENS. A crew comes back from the field with
// a list of numbered points — from a total station, a GNSS receiver, or a
// colleague's Netcad export — and everything else in the job starts from it.
//
// `Y` IS THE EASTING AND `X` IS THE NORTHING, and getting that round the wrong
// way is the single most damaging mistake this file could make. Turkish surveying
// practice, every Netcad point list, every TKGM koordinat özeti and every
// aplikasyon sheet writes `nokta no, Y, X, Z` with Y across and X up — the
// opposite of the mathematical convention. A reader that took them in
// mathematical order would put every point in the wrong place, plausibly, and the
// drawing would look fine until somebody checked it against the ground.
//
// UNTRUSTED INPUT (io.md). This is a text file from somewhere else: every line is
// bounded, every field is checked, and a line that does not parse is reported
// with its number rather than skipped silently — a point list quietly one row
// short is a boundary quietly missing a corner.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <string>
#include <vector>

namespace kentos::io {

/// One surveyed point, as the file names it.
struct SurveyPoint
{
    std::string number;     ///< the point number as written: "P1", "1284", "NIR-3"
    core::Point2 at{};      ///< easting (Y) and northing (X), in millimetres
    core::Mm height{0};     ///< the Z the file carried, or 0
    bool has_height{false}; ///< false when the file carried no Z column
    std::string code;       ///< the surveyor's own code: "AGAC", "BINA", "ROPER"
};

/// Which column holds what.
enum class PointOrder : std::uint8_t {
    /// `no Y X [Z] [kod]` — the Turkish convention and this reader's default.
    NumberEastingNorthing,
    /// `no X Y [Z] [kod]` — for a file that came from a program using the
    /// mathematical order. Stated by the user, never guessed: no heuristic can
    /// tell a 485 320 easting from a 485 320 northing.
    NumberNorthingEasting,
};

/// Reads a delimited point list.
///
/// The delimiter is detected per file from the first line that carries data:
/// comma, semicolon, tab or runs of spaces. A comma-delimited file must use `.`
/// for its decimals; the others accept `,` as well, because a Turkish locale
/// exports `485320,543` and refusing that would refuse most real files.
core::Result<std::vector<SurveyPoint>> read_point_list(const std::string& path, PointOrder order);

/// Writes one back, semicolon-delimited with `.` decimals.
///
/// Semicolons rather than commas so a Turkish-locale spreadsheet opens it without
/// splitting `485320.543` in two, and dots rather than commas so the file is
/// unambiguous whoever reads it next.
core::Status write_point_list(const std::string& path, const std::vector<SurveyPoint>& points,
                              PointOrder order);

} // namespace kentos::io
