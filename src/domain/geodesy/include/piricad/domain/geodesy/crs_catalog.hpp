// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — geodesy: the CRS catalogue.
//
// piricad.md §12 opens with TUREF/ITRF96, the TM 3° zones and ED50. Those
// parameters are a BÖHHBÜY table, so they live in /data/crs as DATA and are
// loaded, never compiled in (CLAUDE.md 5.13). A legislation change is a data
// release, not a rebuild.
#pragma once

#include "piricad/core/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace piricad::domain::geodesy {

/// One TM 3° zone as declared in data/crs/tm3-dilimleri.json.
struct Tm3Zone
{
    int central_meridian{0}; ///< degrees east
    std::string name;        ///< "TM30"
    int epsg{0};             ///< 5253..5259
};

/// Projection parameters shared by every Turkish TM 3° zone.
struct Tm3Parameters
{
    double scale_factor{1.0};     ///< 1.0 for TM3; TM6 and UTM differ
    long false_easting_m{500000}; ///< the offset that keeps eastings positive
    long false_northing_m{0};     ///< zero in the northern hemisphere
    std::string datum;            ///< "TUREF", "ED50" — what the coordinates mean
};

/// The loaded catalogue, with the provenance a regulatory statement needs
/// (.claude/data.md, domain.md R23).
class CrsCatalog
{
public:
    /// Loads from a data package directory, e.g. "<repo>/data/crs".
    static core::Result<CrsCatalog> load(const std::string& crs_dir);

    const std::vector<Tm3Zone>& zones() const noexcept { return zones_; }

    const Tm3Parameters& parameters() const noexcept { return params_; }

    const std::string& source() const noexcept { return source_; }

    const std::string& published() const noexcept { return published_; }

    const std::string& package_version() const noexcept { return package_version_; }

    /// Zone whose central meridian is nearest to `longitude_deg`, or nullptr when
    /// the longitude falls outside Turkey's zones by more than 1.5 degrees.
    const Tm3Zone* zone_for_longitude(double longitude_deg) const;

    const Tm3Zone* zone_by_epsg(int epsg) const;
    const Tm3Zone* zone_by_name(std::string_view name) const;

private:
    std::vector<Tm3Zone> zones_;
    Tm3Parameters params_;
    std::string source_;
    std::string published_;
    std::string package_version_;
};

} // namespace piricad::domain::geodesy
