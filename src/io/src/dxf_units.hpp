// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: the DXF `$INSUNITS` code table.
//
// A DXF says what its numbers mean in one header variable, and a file that
// leaves it at 0 says nothing. Both readers of the format — GDAL's driver today,
// libdxfrw next — and the writer consult this one table, so the three cannot
// disagree about what code 5 means. The wire codes stay in /src/io, exactly as
// the style sentinels do (model.md R19).
//
// Not a public header: a caller outside this module asks for a
// `core::DrawingUnit`, never for a group-code value.
#pragma once

#include "kentos_cad/core/units.hpp"

#include <optional>

namespace kentos::io {

/// The unit a `$INSUNITS` value names, or nothing for 0 (unitless) and for the
/// codes this program does not scale by — miles, mils, yards, ångström,
/// nanometres, gigametres, astronomical units, light years, parsecs. What the
/// header names is REPORTED beside the project's unit, never applied in its
/// place (`ImportDiagnostics::declared_unit`).
constexpr std::optional<core::DrawingUnit> drawing_unit_from_insunits(int code) noexcept
{
    switch (code) {
    case 1: return core::DrawingUnit::Inch;
    case 2: return core::DrawingUnit::Foot;
    case 4: return core::DrawingUnit::Millimetre;
    case 5: return core::DrawingUnit::Centimetre;
    case 6: return core::DrawingUnit::Metre;
    case 7: return core::DrawingUnit::Kilometre;
    case 13: return core::DrawingUnit::Micron;
    case 14: return core::DrawingUnit::Decimetre;
    case 15: return core::DrawingUnit::Decametre;
    case 16: return core::DrawingUnit::Hectometre;
    default: return std::nullopt;
    }
}

/// Whether a `$INSUNITS` value is one the format defines at all (0..20). A code
/// outside that range is a defect in the file, not a unit this program lacks.
constexpr bool insunits_code_known(int code) noexcept
{
    return code >= 0 && code <= 20;
}

/// The `$INSUNITS` code a writer declares for a unit.
constexpr int insunits_code(core::DrawingUnit unit) noexcept
{
    switch (unit) {
    case core::DrawingUnit::Millimetre: return 4;
    case core::DrawingUnit::Centimetre: return 5;
    case core::DrawingUnit::Metre: return 6;
    case core::DrawingUnit::Inch: return 1;
    case core::DrawingUnit::Foot: return 2;
    case core::DrawingUnit::Kilometre: return 7;
    case core::DrawingUnit::Micron: return 13;
    case core::DrawingUnit::Decimetre: return 14;
    case core::DrawingUnit::Decametre: return 15;
    case core::DrawingUnit::Hectometre: return 16;
    }
    return 0;
}

/// The name GDAL's DXF driver takes for its `INSUNITS` creation option. GDAL
/// knows six; a unit outside them is written as unitless and the writer says so.
constexpr const char* gdal_insunits_name(core::DrawingUnit unit) noexcept
{
    switch (unit) {
    case core::DrawingUnit::Millimetre: return "MILLIMETERS";
    case core::DrawingUnit::Centimetre: return "CENTIMETERS";
    case core::DrawingUnit::Metre: return "METERS";
    case core::DrawingUnit::Inch: return "INCHES";
    case core::DrawingUnit::Foot: return "FEET";
    case core::DrawingUnit::Kilometre:
    case core::DrawingUnit::Micron:
    case core::DrawingUnit::Decimetre:
    case core::DrawingUnit::Decametre:
    case core::DrawingUnit::Hectometre: return "UNITLESS";
    }
    return "UNITLESS";
}

} // namespace kentos::io
