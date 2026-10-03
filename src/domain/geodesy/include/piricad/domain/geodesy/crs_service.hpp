// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — geodesy: the CRS resolver, installed on the command bus.
//
// model.md R36 says a bare id string is not a CRS: a document has to carry its
// epoch, its zone central meridian and its datum, because a TM30/TM33 mix-up is
// the classic field blunder and an unlabelled epoch makes a cadastral coordinate
// uncheckable against a later survey.
//
// Knowing that `TUREF/TM30` means EPSG:5254 requires the zone catalogue under
// /data/crs, and that catalogue is read HERE — /src/core may not read data files
// and /src/io may not reach this module (Article 3.2, 3.3). So this service
// installs `Bus::on_crs_resolve`, exactly as `io::FileService` installs
// `on_file_request`, and everything that needs a resolved CRS asks the bus.
//
// Without this service installed nothing breaks: a CRS keeps its id and reports
// `resolved() == false`, which is the honest state for a build that genuinely
// does not know what TM30 is.
#pragma once

#include "piricad/domain/geodesy/crs_catalog.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/core/crs.hpp"

#include <memory>
#include <string>

namespace piricad::domain::geodesy {

/// Whether `id` names a local site grid — `YEREL` or `LOCAL`: metres from wherever the surveyor put
/// the origin, the one system no registry can place on the map.
bool is_local_grid(std::string_view id);

/// Why a local grid cannot be carried into a mapped system, and the one way out: an explicit
/// placement from common points (`OTURT`). The same sentence for `DÖNÜŞTÜR`, `İÇEAKTAR cevir=evet`
/// and `KOORDİNAT sistem=`, because it is one answer to one question.
std::string local_grid_refusal(std::string_view id);

/// Installs a CRS resolver on a bus for as long as it lives.
///
/// Scoped rather than global for the reason `core.md` P8 gives about process-wide
/// state: two documents open at once must be able to hold different catalogue
/// packages without one silently answering for the other.
class CrsService
{
public:
    /// Installs the hook. The catalogue is copied, so the service owns what it
    /// answers from and a later reload cannot change an answer mid-command.
    CrsService(command::Bus& bus, CrsCatalog catalogue);

    /// Clears the hook. A bus outliving its service must not call into a
    /// destroyed one.
    ~CrsService();

    /// Non-copyable: it owns a hook on a bus, and two services would fight over
    /// which one answers.
    CrsService(const CrsService&)            = delete;
    CrsService& operator=(const CrsService&) = delete;

    /// Resolves an id into a populated `core::Crs`.
    ///
    /// Accepts what a user or a file might actually write: `TUREF/TM30`, `TM30`,
    /// `EPSG:5254` and a bare `5254`. An id it cannot place comes back carrying
    /// only that id — never a guess, because a CRS guessed wrong moves every
    /// coordinate in the document by kilometres.
    core::Crs resolve(std::string_view id) const;

    /// A mapping of coordinates from `request.from` into `request.to`, PROJ's own work
    /// (`Bus::on_crs_mapping`); or why none can be built — an unknown system, a local grid, a pair
    /// PROJ can only join with a ballpark shift while `allow_rough` is off.
    core::Result<std::shared_ptr<command::CrsMapping>>
    mapping(const command::CrsMappingRequest& request) const;

private:
    /// The text PROJ is given for `id`: its EPSG code when the catalogue places it, `id` as written
    /// otherwise.
    std::string proj_text(std::string_view id) const;

    command::Bus& bus_;
    CrsCatalog catalogue_;
};

} // namespace piricad::domain::geodesy
