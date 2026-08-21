// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: coordinate reference system identity.
//
// Phase 0 holds identity and metadata only. Real transformation is PROJ,
// gated behind PIRICAD_WITH_PROJ. See .claude/domain.md and piricad.md §12.
#pragma once

#include <string>
#include <string_view>

namespace piricad::core {

/// A CRS identity such as "TUREF/TM30" or "EPSG:5254".
/// Stored as an opaque string; the geodesy module owns interpretation.
class Crs
{
public:
    Crs() = default;

    explicit Crs(std::string id) : id_(std::move(id)) {}

    const std::string& id() const noexcept { return id_; }

    bool empty() const noexcept { return id_.empty(); }

    friend bool operator==(const Crs& a, const Crs& b) { return a.id_ == b.id_; }

private:
    std::string id_{"TUREF/TM30"};
};

// The Turkish TM 3° zone list is a BÖHHBÜY table, so it lives in
// /data/crs/tm3-dilimleri.json and is loaded, never compiled in
// (CLAUDE.md 5.13, .claude/data.md). The geodesy module owns the loader.

} // namespace piricad::core
