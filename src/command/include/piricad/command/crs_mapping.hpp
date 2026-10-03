// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: carrying a coordinate from one system into another.
//
// Three clients want the same thing and none of them may know PROJ: the importer (a layer in
// another zone, in degrees, in feet), `KOORDİNAT` (the readout of a point in a system other than
// the drawing's) and anything that has to say what a datum shift cost. /src/io and /src/command may
// not reach /src/domain/geodesy (Article 3.2), so the geodesy module installs `Bus::on_crs_mapping`
// — the seam `on_crs_resolve` is — and this is the shape of its answer.
//
// EVERYTHING IN IT IS PROJ'S ANSWER. The axis units, the datum shift, the accuracy and the grids
// are asked of PROJ (TODOS G-01); nothing here is a table of ours, and a unit conversion is never
// done by hand because PROJ already does it, feet and degrees included.
#pragma once

#include "piricad/core/json.hpp"
#include "piricad/core/result.hpp"

#include <functional>
#include <memory>
#include <string>

namespace piricad::command {

/// What a mapping is asked for.
struct CrsMappingRequest
{
    std::string from; ///< the system the coordinates are in now (id, `EPSG:n` or a definition)
    std::string to;   ///< the system they are wanted in
    bool allow_rough{
        false}; ///< accept a ballpark shift, or a lower-accuracy operation (`kaba=evet`)
};

/// A prepared mapping. Not thread-safe (it owns PROJ handles): one thread at a time, which is how
/// an import uses it — built and used by the worker that reads the file.
struct CrsMapping
{
    /// Carries one position from `from` to `to`, in place. Both sides are in the order and unit the
    /// system's PEOPLE use — easting then northing in metres for a projected system, longitude then
    /// latitude in degrees for a geographic one; the axis order a CRS declares never reaches a
    /// caller. False when the place is outside the systems' area of validity.
    std::function<bool(double& x, double& y)> apply;

    bool from_degrees{false}; ///< the source counts degrees
    bool to_degrees{false};   ///< the target counts degrees

    /// A sentence about the operation PROJ uses at (`x`, `y`) of the SOURCE system: its name, the
    /// accuracy it states and the grids it reads. Empty when PROJ names no operation.
    std::function<std::string(double x, double y)> describe_at;

    /// The same facts as an object (`islem`, `dogruluk_m`, `kaba`, `gridler`) for a client that
    /// reads structure; null when PROJ names no operation.
    std::function<core::Json(double x, double y)> report_at;
};

/// The hook's type: a mapping, or why none can be built (an unknown system, a ballpark-only pair).
using CrsMappingHook =
    std::function<core::Result<std::shared_ptr<CrsMapping>>(const CrsMappingRequest&)>;

} // namespace piricad::command
