// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: how a measurement is written for a person to read.
//
// ONE PLACE, because two of them disagree. These started as file-local helpers in
// `attribute_panel.cpp`, and the moment a second window had to say what an entity
// IS and how big it is — the pick chooser — the choice was to copy them or to
// share them. A parcel that reads `281.44 m²` in the property panel and
// `281.437 m2` in a dialog beside it is the same defect as two command tables
// (CLAUDE.md 5.10, in spirit): one fact, told twice, free to drift.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>

#include <QString>

namespace kentos::app::measure {

/// `12.480` — a length in metres, three decimals, no unit.
///
/// Three and not more: the document stores millimetres, so a fourth digit would
/// be a number the drawing does not have.
QString metres(core::Mm v);

/// `12.480 m`, with the unit, for a length a user reads rather than edits.
QString metresWithUnit(core::Mm v);

/// `1482.64 m²` — an area in the unit a Turkish surveyor writes it in.
///
/// Two decimals and not three: a parcel area on a tapu is stated to the square
/// centimetre and no further, and printing a digit the document does not carry
/// invites it to be copied into one that will.
QString squareMetres(core::Mm2 v);

/// The kind's Turkish name — `ALAN`, `ÇİZGİ`, `DAİRE` — or its number when the
/// document carries a kind this build does not know.
///
/// The NAME and not the id, because the id is a storage detail: `core.polyline`
/// is 1 because it declared itself 1, and a user reading a property panel is
/// owed the word, not the number (model.md R22-R26).
QString kindName(core::KindId kind);

/// What the user means by "türü", which is not always what the kind is called.
///
/// A PARCEL AND A BOUNDARY ARE THE SAME KIND. `core.polyline` stores both: a face
/// is a slot whose ring is Exterior and a line is one whose ring is Open
/// (model.md R9, R10). That is the right storage decision and the wrong ANSWER
/// for a person, where "ÇOKLUÇİZGİ" over a 281 m² parcel reads as a defect. So
/// the kind names the family and the ring's role names the thing.
QString shapeName(const core::Document& doc, core::KindId kind, std::uint32_t gslot);

/// `18904.36` -> `18 904.36`, and `1600.00 m²` -> `1 600.00 m²`. Only a MEASURE
/// is grouped.
///
/// A number with a fractional part is a measurement and a reader wants its
/// magnitude at a glance; a whole number in a table is almost always an
/// identifier — an ada, a parsel, a UAVT code — and grouping one turns `1284`
/// into `1 284`, which is not how anybody writes an ada number. The decimal point
/// is what tells the two apart.
///
/// The number may be FOLLOWED by a unit and still be grouped: the pick chooser
/// prints `1 600.00 m²` in the same column the attribute table prints a bare
/// `18 904.36` in, and a rule that only accepted a bare number would group one
/// and not the other. What it will not touch is a string whose leading run is not
/// a number at all, so a text attribute reading `A.B.C` comes through as it is.
QString spacedThousands(const QString& text);

/// The one measurement that identifies this entity among others under a cursor.
///
/// WHICH MEASUREMENT DEPENDS ON WHAT IT IS, and that is the point: a radius tells
/// two concentric circles apart and an area does not, an area tells two stacked
/// parcels apart and a length does not. A row that read `uzunluk` for all three
/// would be a column of numbers that never distinguishes anything.
QString sizeSummary(const core::Document& doc, core::EntityId entity);

} // namespace kentos::app::measure
