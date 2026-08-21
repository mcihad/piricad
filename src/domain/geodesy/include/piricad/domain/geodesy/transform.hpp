// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — geodesy: coordinate transformation.
//
// PROJ does the mathematics. What this wrapper exists for is the ONE thing PROJ
// will not decide for you, and getting it wrong produces a coordinate that is
// silently, plausibly wrong:
//
//   **Axis order.** EPSG:5254 (TUREF/TM30) declares AXIS["northing (X)"] first
//   and AXIS["easting (Y)"] second — Turkish surveying convention, the inverse of
//   the mathematical one. PiriCAD stores easting in Point2::x. Every transform is
//   therefore created through proj_normalize_for_visualization(), which forces
//   (easting, northing) regardless of what the CRS declares.
//
// .claude/model.md R37a. Measured round-trip error at Turkish coordinates is
// below one nanometre, which is nine orders under the millimetre we store.
#pragma once

#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace piricad::domain::geodesy {

/// A prepared transformation between two coordinate reference systems.
///
/// Construction is expensive and thread-unsafe; application is cheap. Build one
/// per (source, target) pair and reuse it — a per-point construction turns a
/// parcel import into a minute.
class Transform {
public:
    /// `source` and `target` are anything PROJ accepts: "EPSG:5254",
    /// "TUREF/TM30" once the catalogue maps it, a PROJ string, or WKT.
    static core::Result<Transform> between(const std::string& source, const std::string& target);

    ~Transform();
    Transform(Transform&&) noexcept;
    Transform& operator=(Transform&&) noexcept;
    Transform(const Transform&)            = delete;
    Transform& operator=(const Transform&) = delete;

    /// Forward transform, easting/northing in metres. Returns false when PROJ
    /// reports the point as unusable, which happens outside a zone's domain.
    bool forward(double& easting, double& northing) const;
    bool inverse(double& easting, double& northing) const;

    /// Transforms a run of stored millimetre points in place. Millimetres are
    /// converted to metres, transformed, and rounded back deterministically with
    /// the same `mm_from_metres` used everywhere else.
    ///
    /// REFUSES when the far side is a geographic CRS. Document geometry is
    /// projected millimetres; degrees do not fit in that unit, and rounding
    /// 29.830716° to the nearest "millimetre" moves the point about a hundred
    /// metres. Use the scalar overloads for a geographic target — display,
    /// export, a WGS84 readout — and keep degrees out of the document.
    core::Status forward(std::span<core::Point2> points) const;
    core::Status inverse(std::span<core::Point2> points) const;

    /// True when the far side of the transform is expressed in degrees.
    bool target_is_angular() const noexcept { return target_angular_; }
    bool source_is_angular() const noexcept { return source_angular_; }

    /// True when both sides are projected, i.e. the span overloads are usable.
    bool projected_both_ways() const noexcept { return !source_angular_ && !target_angular_; }

    const std::string& source() const noexcept { return source_; }
    const std::string& target() const noexcept { return target_; }

    /// True when PROJ was compiled in. When false, `between()` fails with a
    /// message naming the CMake option rather than silently returning identity —
    /// an identity transform pretending to be a datum shift is a wrong legal
    /// document (§12).
    static bool available() noexcept;

    /// PROJ's version, for the About box and the audit record.
    static std::string backend_version();

private:
    Transform() = default;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string           source_;
    std::string           target_;
    bool                  source_angular_{false};
    bool                  target_angular_{false};
};

} // namespace piricad::domain::geodesy
