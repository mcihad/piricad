// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: drawing an output layout.
//
// ONE PAINTER FOR THE THREE PLACES A SHEET APPEARS: the designer's page, the
// print preview and the exported PDF. They differ only in the paint device and
// the resolution handed in — which is the same bargain `PrintService`'s
// `paint_window` already makes for a plain print, and for the same reason: three
// painters would be three answers to "what does this sheet look like", and the
// one that matters is the one that comes out of the printer.
//
// IT NEEDS Qt, so it lives here. `core::Layout` is the model and knows nothing
// about painting; `render::build_scene` draws the DRAWING; this file draws the
// paper, places the items and asks the render pipeline for the map inside the
// map frame. Nothing below `/src/app` learns that a layout can be painted.
#pragma once

#include "kentos_cad/core/layout.hpp"

#include <QString>

#include <string>
#include <vector>

class QPainter;
class QRectF;

namespace kentos::core {
class Document; ///< read to draw the map frames; never written here
} // namespace kentos::core

namespace kentos::app {

/// What the `<...>` placeholders in a label resolve to.
///
/// RESOLVED AT DRAWING TIME, NEVER STORED RESOLVED (core/layout.hpp): a sheet
/// re-exported after the scale changed must print the new scale, and a title
/// that had been flattened to text would print the old one for ever.
struct LayoutFacts
{
    QString sheet;   ///< `<yerlesim>` — the layout's own name
    QString project; ///< `<proje>` — the drawing's file name, without the path
    QString crs;     ///< `<crs>`   — the coordinate system's id
    QString date;    ///< `<tarih>` — today, as the user's locale writes it

    /// The current object's attributes, for an atlas sheet: `<ada>`, `<parsel>`.
    /// Empty on an ordinary sheet.
    std::vector<std::pair<std::string, std::string>> fields;

    /// THE FOLDER THE PROJECT LIVES IN, so a picture stored beside it is found.
    ///
    /// A picture item holds a path. An ABSOLUTE one names a place on the machine
    /// it was typed on and breaks the moment the project is copied to a colleague
    /// or to a server — the sheet then prints a dashed box where the kurum logo
    /// was, and prints it without complaint. A RELATIVE one is the portable
    /// answer, and it is only portable if it is resolved against the project
    /// rather than against whatever directory the program happens to be running
    /// in (TODOS L-07: "taşınan projede logo kaybolmaz").
    ///
    /// Empty for an unsaved drawing, which genuinely has no folder; a relative
    /// path is then left to resolve however it can, and a picture that cannot be
    /// found still says so on the page.
    QString project_dir;
};

/// Draws one page of `layout` into `target`, which is in DEVICE PIXELS.
///
/// `dpi` is what a paper millimetre is worth in those pixels; it decides line
/// weights and text sizes through the same `SceneOptions::pixels_per_paper_mm`
/// the canvas uses, so a hairline is a hairline at 96 dpi and at 1200.
///
/// The page's white ground is painted first. `margin_guide` draws the hairline
/// the designer shows and the printer does not.
///
/// `trouble` collects what the page could not honour — today, an item linked to a
/// map frame that is not there. It is an OUT-PARAMETER rather than a refusal
/// because a sheet with one broken link still has to print the rest of itself;
/// this is the seed of the preflight report (TODOS L-15).
void paint_layout_page(QPainter& painter, const QRectF& target, const core::Document& document,
                       const core::Layout& layout, int page, double dpi, const LayoutFacts& facts,
                       bool margin_guide = false, std::vector<std::string>* trouble = nullptr);

/// The text a label prints, with its placeholders resolved.
QString resolve_placeholders(const QString& text, const core::Layout& layout,
                             const core::LayoutItem* map, const LayoutFacts& facts);

} // namespace kentos::app
