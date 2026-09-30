// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: what a table item on a sheet says.
//
// THE WORDS OF A TABLE, NOT ITS PICTURE. Which rows, which columns, and what
// every cell reads — worked out here, where it can be tested without a painter,
// and drawn by `layout_render.cpp`, which only decides where the words go. A
// coordinate list is a legal document's content; the figure a cell prints is
// the program's answer, and it must not depend on how a screen happens to draw.
#pragma once

#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::core {

class Document; ///< the drawing a table reads

/// The columns a table prints.
///
/// Its own list when it has one (`LayoutItem::table_columns`). A table written
/// before columns could be set says it with `columns` — attribute ids, empty
/// meaning every column the layer offers — and is read exactly as it was.
std::vector<LayoutColumn> table_column_list(const Document& doc, const LayoutItem& item);

/// What a column's head says: its own heading, or the name of what it shows.
std::string table_heading(const Document& doc, const LayoutColumn& column);

/// A table, as text.
struct TableText
{
    std::vector<LayoutColumn> columns;          ///< what `table_column_list` gave
    std::vector<std::string> heads;             ///< one per column
    std::vector<std::vector<std::string>> rows; ///< every row the layer gives, in order
};

/// Every row and every cell of a table item.
///
/// ROWS: in `TableRows::Objects`, the live objects on the item's layer in slot
/// order; in `TableRows::Vertices`, every corner of those objects in the same
/// order, a corner met twice — the one two parcels share — listed once. Only
/// points, lines and faces are listed: a caption, a dimension, a leader or a
/// hatch on the same layer is not a row.
///
/// CELLS: an attribute column reads the object's cell (a corner, its object's);
/// a computed one (`table_sources`) works the value out. A corner's `$no` is
/// the number the sheet writes beside it — the caption `KÖŞENUMARALA` tied to
/// it — else the number of a surveyed point standing on it (`nokta_no`), else
/// its row's. A number is written
/// with the column's decimals and grouping and the table's decimal mark, in
/// whole-number arithmetic, so the same drawing prints the same figures on every
/// platform (§7.3) — a half is rounded away from zero, as a surveyor rounds.
///
/// Fails, naming it, when the layer or an attribute column is not in the
/// drawing: a table over nothing must say so rather than print a head over no
/// rows (model.md R46h).
Result<TableText> table_text(const Document& doc, const LayoutItem& item);

/// `scaled` over ten to the `scale`, written with `decimals` digits after the
/// mark — rounded half away from zero, or padded with zeros — its thousands
/// grouped by a space when asked, the mark a comma or a point.
///
/// WHOLE NUMBERS THROUGHOUT. A coordinate of 485 320,155 m held as a `double`
/// is a hair either side of the half, and which side decides whether the sheet
/// says ,15 or ,16.
std::string format_fixed(std::int64_t scaled, int scale, int decimals, bool thousands, bool comma);

} // namespace kentos::core
