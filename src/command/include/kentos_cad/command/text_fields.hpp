// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: a caption's words filled from the object it names.
//
// ONE SUBSTITUTION, SHARED (TODOS C-12). ETİKET has always filled `{sutun}` with
// a column's value; a caption that FOLLOWS its object (core/attach.hpp,
// `AttachDerive::Fields`) fills the same format again every time that object
// changes, so the two must agree to the character — and agree they do, because
// both call this.
//
// A SUBSTITUTION, NOT A LANGUAGE (CLAUDE.md 5.11). A brace names a column, or —
// behind `#`, which no column id can begin with — one of three figures measured
// from the geometry: `{#alan}` the area in square metres, `{#cevre}` the
// perimeter, `{#uzunluk}` the length. Nothing else happens inside a brace: no
// operator, no nesting, no function. How many decimals, and in which mark, is
// the caller's to say, not the format's.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace kentos::command {

/// How a measured figure is written.
struct FieldFormat
{
    std::uint8_t precision{2};                        ///< decimals, 0–8
    char separator{','};                              ///< the decimal mark
    core::DrawingUnit unit{core::DrawingUnit::Metre}; ///< for `{#cevre}` and `{#uzunluk}`
};

/// `format` with every `{sutun}` replaced by that column's value for `e` and
/// every `{#alan}`, `{#cevre}`, `{#uzunluk}` by the figure. A brace that names
/// neither is left as it is, braces and all: deleting it would hide a typo in a
/// format a sheet is about to be printed from.
core::Result<std::string> fill_fields(const core::Document& doc, core::EntityId e,
                                      std::string_view format, const FieldFormat& how = {});

/// Whether `format` names anything `fill_fields` would fill.
bool has_fields(std::string_view format);

} // namespace kentos::command
