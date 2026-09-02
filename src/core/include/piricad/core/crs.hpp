// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the coordinate reference system a document's numbers are in.
//
// model.md R36 is explicit that a bare id string is NOT a CRS, and this type used
// to be one anyway: an opaque `std::string` and nothing else. The consequences
// were not theoretical. `DIŞAAKTAR` could not resolve the default `TUREF/TM30`
// and refused every fresh drawing until the user typed an EPSG code by hand,
// because the only place that knows TM30 is EPSG:5254 is the zone catalogue under
// /data — and /src/io may not reach the geodesy module that reads it (Article
// 3.2, 3.3).
//
// So the FIELDS live here, in core, where every module may read them, and the
// INTERPRETATION stays in the geodesy module, which fills them in through
// `Bus::on_crs_resolve` — the same seam `io::FileService` uses for file work. Core
// still knows nothing about a zone, a datum or a projection; it only carries what
// the geodesy module worked out.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace piricad::core {

/// The coordinate reference system a document's numbers are expressed in.
///
/// Document-level, never per layer (R37): a drawing whose layers disagreed about
/// what a coordinate means is not a drawing, it is two drawings in one file.
class Crs
{
public:
    /// An unset CRS. A document must declare one before anything is exported:
    /// unlabelled coordinates mean nothing to whoever receives them (io.md R20).
    Crs() = default;

    /// A CRS named by its id — `TUREF/TM30`, `EPSG:5254`. Explicit so a bare
    /// string cannot become a coordinate system by accident.
    ///
    /// The metadata below is EMPTY until something resolves it. That is the
    /// honest state: naming a CRS and knowing what it means are different acts,
    /// and core is not the layer that knows.
    explicit Crs(std::string id) : id_(std::move(id)) {}

    /// The id as the user or the file wrote it.
    const std::string& id() const noexcept { return id_; }

    /// Whether this document has declared a CRS at all.
    bool empty() const noexcept { return id_.empty(); }

    /// The EPSG code, or 0 when unresolved.
    ///
    /// This is what an exporter hands to GDAL. Zero does not mean "no CRS" — it
    /// means "nobody has looked it up yet", and the difference matters because the
    /// first is an error and the second is a missing step.
    int epsg() const noexcept { return epsg_; }

    /// The REALISATION EPOCH, exactly as the data package writes it.
    ///
    /// Not decoration: a reference frame is realised at an epoch, the Anatolian
    /// plate moves centimetres a year, and the same point measured in two epochs
    /// is two coordinates. A cadastral document that does not say which epoch its
    /// numbers belong to cannot be checked against a later survey.
    ///
    /// EMPTY today. The value is a regulatory fact and /data/crs does not carry it
    /// yet; filling it in from memory would put an unsourced regulatory value into
    /// the product, which CLAUDE.md 5.13 forbids and 6.11 makes a domain expert's
    /// call. The field exists — which is what R36 asked for — and the value
    /// arrives with the data package that cites it.
    const std::string& epoch() const noexcept { return epoch_; }

    /// Central meridian of the TM zone, in whole degrees; 0 when not a TM zone.
    /// A TM30/TM33 mix-up is the classic field blunder and this is the field that
    /// makes it detectable rather than invisible (R36).
    int central_meridian_deg() const noexcept { return meridian_; }

    /// The geoid model orthometric heights are referred to, empty when unknown.
    const std::string& geoid_model() const noexcept { return geoid_; }

    /// The vertical datum, empty when the document is purely planar.
    const std::string& vertical_datum() const noexcept { return vertical_; }

    /// Whether anything has filled in the metadata. A resolved CRS can be
    /// exported; an unresolved one names something nobody has looked up.
    bool resolved() const noexcept { return epsg_ != 0; }

    /// Fills in what the geodesy module worked out. Called through
    /// `Bus::on_crs_resolve`; core never calls it itself, because core does not
    /// know what a zone is.
    void resolve(int epsg, std::string epoch, int central_meridian_deg, std::string geoid_model,
                 std::string vertical_datum)
    {
        epsg_     = epsg;
        epoch_    = std::move(epoch);
        meridian_ = central_meridian_deg;
        geoid_    = std::move(geoid_model);
        vertical_ = std::move(vertical_datum);
    }

    /// Equality on the ID ALONE, deliberately.
    ///
    /// Two documents that name the same CRS are in the same CRS whether or not a
    /// build happened to resolve it — otherwise a document would stop equalling
    /// itself the moment the geodesy module was enabled, and `content_hash()`
    /// would depend on which optional dependency was compiled in.
    friend bool operator==(const Crs& a, const Crs& b) { return a.id_ == b.id_; }

private:
    std::string id_{"TUREF/TM36"};
    int epsg_{0};
    std::string epoch_;
    int meridian_{0};
    std::string geoid_;
    std::string vertical_;
};

// The Turkish TM 3° zone list is a BÖHHBÜY table, so it lives in
// /data/crs/tm3-dilimleri.json and is loaded, never compiled in
// (CLAUDE.md 5.13, .claude/data.md). The geodesy module owns the loader.

} // namespace piricad::core
