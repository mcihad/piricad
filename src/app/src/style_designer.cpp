// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/style_designer.hpp"

#include "piricad/core/attribute.hpp"

#include "piricad/render/scene.hpp"
#include "piricad/render/symbology.hpp"

#include "piricad/app/tokens.hpp"

#include "piricad/app/backend_factory.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/app/datagrid.hpp"
#include "piricad/app/export_dialog.hpp"
#include "piricad/app/fields.hpp"
#include "piricad/app/schema_page.hpp"
#include "piricad/app/widgets.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/style_library.hpp"
#include "piricad/core/style_rule.hpp"

#include <QAbstractTableModel>
#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStyledItemDelegate>

#include <QHeaderView>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>

#include <QVBoxLayout>
#include <QWheelEvent>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <set>
#include <utility>

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace piricad::app {
namespace {

using core::SymbolLayerType;

/// The symbol layer types offered, grouped by what they draw.
///
/// Both names are shown: the Turkish one is the legend, the machine one in
/// brackets is what `STİL tip=` takes. A user who learns the dialog has learned
/// the command line, which is the point of them being the same road.
struct TypeRow
{
    SymbolLayerType type;
    const char* label;
};

constexpr std::array<TypeRow, 12> kTypes{{
    {SymbolLayerType::SimpleLine, "Çizgi"},
    {SymbolLayerType::MarkerLine, "İşaretçi çizgi"},
    {SymbolLayerType::HashLine, "Tarak çizgi"},
    {SymbolLayerType::RasterLine, "Görsel çizgi"},
    {SymbolLayerType::SimpleFill, "Dolgu"},
    {SymbolLayerType::LinePatternFill, "İç tarama"},
    {SymbolLayerType::PointPatternFill, "Nokta desen dolgu"},
    {SymbolLayerType::RasterFill, "Görsel dolgu"},
    {SymbolLayerType::CentroidFill, "Merkez işaretçi"},
    {SymbolLayerType::SimpleMarker, "İşaretçi"},
    {SymbolLayerType::RasterMarker, "Görsel işaretçi"},
    {SymbolLayerType::TextMarker, "Yazı"},
}};

/// The layer types that mean anything on one geometry.
///
/// A point has no length to stroke and no interior to fill, so offering it
/// "Çizgi", "Dolgu" and "Tarak çizgi" is offering nine controls that cannot do
/// anything — and the default symbol of an unstyled layer IS a plain stroke, so
/// that is exactly what the Nokta tab opened on: layer type "Çizgi", with a
/// cap and a join to set on a thing that has no ends.
///
/// The current layer's own type is always added by the caller, so a symbol that
/// already carries something unusual can still be read and changed.
std::vector<SymbolLayerType> types_for(PreviewShape shape)
{
    switch (shape) {
    case PreviewShape::Point:
        return {SymbolLayerType::SimpleMarker, SymbolLayerType::RasterMarker,
                SymbolLayerType::TextMarker};
    case PreviewShape::Line:
        return {SymbolLayerType::SimpleLine, SymbolLayerType::MarkerLine, SymbolLayerType::HashLine,
                SymbolLayerType::RasterLine, SymbolLayerType::TextMarker};
    case PreviewShape::Area: break;
    }
    // An area has a boundary as well as an interior, so it keeps the line types.
    return {SymbolLayerType::SimpleFill,       SymbolLayerType::LinePatternFill,
            SymbolLayerType::PointPatternFill, SymbolLayerType::RasterFill,
            SymbolLayerType::CentroidFill,     SymbolLayerType::SimpleLine,
            SymbolLayerType::MarkerLine,       SymbolLayerType::HashLine,
            SymbolLayerType::RasterLine,       SymbolLayerType::TextMarker};
}

/// The type a NEW layer starts as on one geometry. A point starts as a marker,
/// which is the only thing a point can be.
SymbolLayerType default_type_for(PreviewShape shape)
{
    switch (shape) {
    case PreviewShape::Point: return SymbolLayerType::SimpleMarker;
    case PreviewShape::Line: return SymbolLayerType::SimpleLine;
    case PreviewShape::Area: break;
    }
    return SymbolLayerType::SimpleFill;
}

constexpr std::array<core::MarkerShape, 12> kShapes{
    {core::MarkerShape::Circle, core::MarkerShape::Square, core::MarkerShape::Triangle,
     core::MarkerShape::Diamond, core::MarkerShape::Star, core::MarkerShape::Cross,
     core::MarkerShape::XCross, core::MarkerShape::Arrow, core::MarkerShape::HalfCircle,
     core::MarkerShape::Pentagon, core::MarkerShape::Hexagon, core::MarkerShape::Tick}};

constexpr std::array<core::MarkerPlacement, 5> kPlacements{
    {core::MarkerPlacement::Interval, core::MarkerPlacement::Vertex,
     core::MarkerPlacement::FirstVertex, core::MarkerPlacement::LastVertex,
     core::MarkerPlacement::Centre}};

constexpr std::array<core::Unit, 3> kUnits{
    {core::Unit::Paper, core::Unit::Ground, core::Unit::Pixel}};

constexpr std::array<core::LineCap, 3> kCaps{
    {core::LineCap::Butt, core::LineCap::Round, core::LineCap::Square}};

constexpr std::array<core::LineJoin, 3> kJoins{
    {core::LineJoin::Miter, core::LineJoin::Round, core::LineJoin::Bevel}};

/// How many gallery thumbnails are rendered at once.
///
/// Every thumbnail is a real render through the canvas backend, so a drawer of
/// four hundred would cost a visible pause on every click. The cap is SAID OUT
/// LOUD under the grid rather than applied quietly: a list that silently stops at
/// a hundred reads as a list of a hundred.
constexpr int kGalleryCap = 120;

/// The preview swatch's side. 120 is the width at which `symbol_preview` still
/// draws the zigzag's corner (it straightens the run below that), and it is what
/// leaves the symbol stack beside it a readable column in a 352 px editor.

/// The stack index a valid row names.
std::size_t at(int row)
{
    return static_cast<std::size_t>(row);
}

/// Holds a flag true for a scope and puts back WHAT WAS THERE, not `false`.
///
/// The distinction is the whole defect it was written for. `refresh()` raised
/// `loading_` and `loadSelected()` lowered it with a bare assignment; because
/// `refresh()` calls `loadSelected()` — directly, and again through the
/// `currentRowChanged` that `clear()` emits — the guard came down while the
/// caller was still rebuilding, and a half-written row's `itemChanged` was read
/// as a user editing the symbol.
class Held
{
public:
    explicit Held(bool& flag) noexcept : flag_(flag), was_(flag) { flag = true; }

    ~Held() { flag_ = was_; }

    Held(const Held&)            = delete;
    Held& operator=(const Held&) = delete;
    Held(Held&&)                 = delete;
    Held& operator=(Held&&)      = delete;

private:
    bool& flag_;
    bool was_;
};

/// The table row a combo box names, reading an unset combo as the first row.
///
/// A `QComboBox` with no current item reports -1, and every one of these tables
/// is indexed by that number a line later.
template<class Table> typename Table::value_type pick(const Table& table, const QComboBox* box)
{
    const int last = static_cast<int>(table.size()) - 1;
    return table[at(std::clamp(box->currentIndex(), 0, last))];
}

/// A small glyph of the geometry a tab stands for.
///
/// Drawn rather than taken from `symbol_preview`: a tab says which SHAPE the
/// preview will use, which is a different statement from what a symbol looks
/// like, and a shape drawn with the symbol's own colours would go invisible
/// exactly when the symbol is white on white.
/// Draws one shelf row: the swatch, the published name, and the gösterim's id
/// at the right end in the width the name does not use.
///
/// A DELEGATE AND NOT A SECOND COLUMN, because the shelf is one list of one
/// thing. A two-column view would want headers, a splitter and a sort order for
/// what is a single line per symbol.
class ShelfRow : public QStyledItemDelegate
{
public:
    /// A heading row carries its whole group path here and nothing else.
    static constexpr int kGroupRole = Qt::UserRole + 1;

    /// A gösterim the annex has withdrawn.
    static constexpr int kRetiredRole = Qt::UserRole + 2;

    /// ONE LEFT EDGE FOR EVERY WORD IN THE SHELF.
    ///
    /// The three things in a row used to start at three different places: the
    /// group heading at 10 px, the specimen at 13, the name at 57. None of them
    /// lined up with any other, which is the kind of near-miss a reader sees as
    /// sloppiness without being able to name it. The specimen now hangs in a
    /// gutter of its own and EVERY line of text — heading and name alike —
    /// starts where the gutter ends, so the annex's sections and the gösterim
    /// under them share one edge and the hierarchy is read as indentation.
    static constexpr int kGutter = 66;

    /// The paper the specimen is printed on, inside the gutter.
    static constexpr int kCardW = 50;
    static constexpr int kCardH = 30;

    explicit ShelfRow(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setTheme(ThemeMode mode) { theme_ = mode; }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        return {base.width(), index.data(kGroupRole).toString().isEmpty() ? 38 : 30};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
        p->save();
        p->setRenderHint(QPainter::Antialiasing, false);

        // ---- a group heading ------------------------------------------------
        const QString group = index.data(kGroupRole).toString();
        if (!group.isEmpty()) {
            p->fillRect(option.rect, t.bgHeader);
            QFont small = option.font;
            small.setPixelSize(11);
            p->setFont(small);
            p->setPen(t.textDim);
            const QRect where = option.rect.adjusted(kGutter, 0, -10, -1);
            p->drawText(where, Qt::AlignLeft | Qt::AlignVCenter,
                        QFontMetrics(small).elidedText(group, Qt::ElideMiddle, where.width()));
            p->setPen(QPen(t.lineSoft, 1.0));
            p->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
            p->restore();
            return;
        }

        // ---- the row's own ground -------------------------------------------
        const bool picked = (option.state & QStyle::State_Selected) != 0;
        if (picked) {
            p->fillRect(option.rect, t.accentWash);
            p->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()),
                        t.accent);
        } else if ((option.state & QStyle::State_MouseOver) != 0) {
            p->fillRect(option.rect, t.hoverRow);
        }

        // ---- the specimen, on paper -----------------------------------------
        //
        // DRAWN HERE AND NOT BY THE VIEW, so the selection's wash stops at the
        // card's edge. Painted under the icon it tinted the paper blue, and a
        // gösterim is judged by the ink on it: a picked row must not show a
        // different colour from the eleven around it.
        const QRect card(option.rect.left() + (kGutter - kCardW) / 2,
                         option.rect.top() + (option.rect.height() - kCardH) / 2, kCardW, kCardH);
        p->fillRect(card, Qt::white);
        p->setPen(QPen(t.lineHard, 1.0));
        p->drawRect(card.adjusted(0, 0, -1, -1));
        if (const auto art = index.data(Qt::DecorationRole).value<QIcon>(); !art.isNull()) {
            const QPixmap drawn = art.pixmap(QSize(44, 26), p->device()->devicePixelRatioF());
            const QSize at      = drawn.deviceIndependentSize().toSize();
            p->drawPixmap(card.left() + (kCardW - at.width()) / 2,
                          card.top() + (kCardH - at.height()) / 2, drawn);
        }

        // ---- the published name, and the withdrawal mark ---------------------
        QFont small = option.font;
        small.setPixelSize(11);
        const QFontMetrics tiny(small);
        const QString said = tr("yürürlükte değil");
        const bool retired = index.data(kRetiredRole).toBool();
        const int reserved = retired ? tiny.horizontalAdvance(said) + 20 : 10;

        QRect words = option.rect.adjusted(kGutter, 0, -reserved, 0);
        p->setFont(option.font);
        p->setPen(t.text);
        p->drawText(words, Qt::AlignLeft | Qt::AlignVCenter,
                    option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                  Qt::ElideRight, words.width()));

        // A WITHDRAWN ROW IS MARKED WHERE IT IS SCANNED, not only after it has
        // been picked: it is still loadable, because a retired id is never
        // dropped, but it must not be chosen for a new sheet.
        if (retired) {
            p->setFont(small);
            p->setPen(t.warn);
            p->drawText(QRect(option.rect.right() - reserved, option.rect.top(), reserved - 10,
                              option.rect.height()),
                        Qt::AlignRight | Qt::AlignVCenter, said);
        }
        p->restore();
    }

private:
    ThemeMode theme_{ThemeMode::Dark};
};

QColor from_rgba(std::uint32_t rgba)
{
    return QColor::fromRgba(static_cast<QRgb>(rgba));
}

/// Draws a colour field: the value fills the field and its hex is written across
/// it, at the width of the column it sits in.
///
/// PAINTED, not styled. There is one stylesheet in this program (design.md 2)
/// and it lives in `theme.cpp`; a widget that writes its own is how two sources
/// of truth for an appearance start. The colour here is not a theme colour
/// either — it is the user's DATUM — so it cannot come from a token, and the ink
/// over it is chosen by luminance so the hex stays legible on all sixteen
/// million of them.
///
/// It used to be a 40x18 icon on a 30 px tool button, which in a column of
/// full-width spin boxes and combos read as a stray swatch rather than as a
/// value you can edit. A colour IS a field of this form and looks like one.
void show_colour(QToolButton* button, std::uint32_t rgba)
{
    constexpr int kFieldHeight = 28;

    // The button is stretched by its cell, so its own width is the column's.
    // Before the first layout it has none yet, and the floor keeps the field
    // from starting life as a chip; the next refresh corrects it.
    //
    // The floor was 200 and that was the width of the row's overflow: beside a
    // 110 px caption a 200 px icon asks for more than the editor column has,
    // the row grows past the viewport, and every row in the form — they share a
    // width — is cut at the dialog's edge. 120 is a legible hex and under the
    // column's narrowest honest width.
    const int width = std::max(button->width() - 2, 120);
    const qreal dpr = button->devicePixelRatioF();

    QPixmap face(QSize(width, kFieldHeight) * dpr);
    face.setDevicePixelRatio(dpr);
    face.fill(Qt::transparent);

    QPainter painter(&face);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box(0.5, 0.5, width - 1.0, kFieldHeight - 1.0);

    const QRectF chip(4, 5, 18, 18);
    const QColor colour = from_rgba(rgba);
    painter.setPen(QPen(button->palette().color(QPalette::Mid), 1));
    painter.setBrush(rgba == 0 ? Qt::NoBrush : QBrush(colour));
    painter.drawRoundedRect(chip, 3, 3);
    if (rgba == 0) {
        painter.setPen(QPen(button->palette().color(QPalette::WindowText), 1));
        painter.drawLine(chip.topLeft(), chip.bottomRight());
    }
    painter.setPen(button->palette().color(QPalette::WindowText));
    const QString value =
        rgba == 0
            ? QObject::tr("Dolgusuz")
            : QLocale(QLocale::Turkish)
                  .toUpper(colour.name(colour.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb));
    painter.drawText(box.adjusted(32, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, value);
    painter.end();

    button->setIcon(QIcon(face));
    button->setIconSize(QSize(width, kFieldHeight));
}

/// One row of the symbol editor: caption on the LEFT in a fixed column, editor
/// beside it — design.md §8's `110px | 1fr | 22px` grid.
///
/// §16.1 stacks a caption above its editor, and this panel did that for a
/// while; but §16.1 is the four-column record form, where a left caption eats
/// half a 312 px column, and §8 is this window. Here the reference puts the
/// caption beside the value, and it is right to: a property list is read down
/// the values and the caption is what tells one from the next. Stacking cost
/// every row a second line, doubled the column's height and put the last rows
/// under the bottom of the dialog.
///
/// The captions are kept short enough for the column — the unit goes in the
/// editor's suffix (§15.2), not in the caption — so nothing wraps.
///
/// `caption_out` hands back the label because one row renames itself: the colour
/// field says "Yazı rengi" on a text marker and "Çizgi rengi" everywhere else.
/// A combo that takes the width its column gives it rather than the width of
/// its longest entry. Left to Qt, a combo's minimum is its widest item, and
/// "Kâğıt — yakınlaştırınca boyu DEĞİŞMEZ" beside a 110 px caption is wider than
/// the whole editor column — the row ran under the dialog's edge and the value
/// was cut where the user had to read it. The popup still shows every item in
/// full; only the closed control shrinks.
class LayerVisibilityDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem small(option);
        initStyleOption(&small, index);
        small.decorationSize = QSize(16, 16);
        QStyledItemDelegate::paint(painter, small, index);
    }
};

void fit_column(QComboBox* combo)
{
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(6);
}

QWidget* form_cell(QWidget* parent, const QString& label, QWidget* editor, QWidget* unit,
                   QLabel** caption_out)
{
    auto* cell   = new QWidget(parent);
    auto* column = new QVBoxLayout(cell);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);
    auto* caption = new QLabel(label, cell);
    caption->setObjectName(QStringLiteral("formCaption"));
    cell->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    caption->setBuddy(editor);
    caption->setVisible(!label.isEmpty());
    column->addWidget(caption);
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    editor->setMinimumWidth(1);
    row->addWidget(editor, 5);
    if (unit != nullptr) {
        unit->setFixedWidth(108);
        row->addWidget(unit, 2);
    }
    column->addLayout(row);

    if (caption_out != nullptr) *caption_out = caption;
    return cell;
}

/// The symbol a layer currently draws. A configured layer default wins over an
/// entity override: this dialog edits the layer, not whichever feature happens
/// to be first in storage order.
core::Symbol symbol_of_layer(const core::Document& doc, core::LayerId layer)
{
    const core::Layer* record = doc.layer(layer);
    if (record != nullptr && record->style != core::kByLayerStyle &&
        doc.styles().contains(record->style))
        return doc.styles().symbol_at(record->style);

    const auto& entities = doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e) || entities.layer[e] != layer) continue;
        const core::StyleId sid = entities.style[e];
        if (sid != core::kByLayerStyle && doc.styles().contains(sid))
            return doc.styles().symbol_at(sid);
        break;
    }

    return core::Symbol::of(record ? record->appearance : core::Appearance{});
}

/// What the LAYER holds, read from its entities rather than from its symbol.
///
/// The geometry tab used to be chosen by `natural_shape(symbol_)` — that is, by
/// what the symbol ALREADY draws. A parcel layer whose symbol is one plain
/// stroke therefore opened on the Çizgi tab, offered the line half of the
/// gallery and put the fill layer types behind a tab nobody had a reason to
/// press. The user's own words: "bunlar alan tipleri ama sembolojide alan
/// dolgusu seçemiyorum."
///
/// It is the wrong question. This dialog edits the symbol a layer's PARCELS are
/// drawn with, so the geometry is the parcels' and not the symbol's; asking the
/// symbol means a layer can only ever be given more of what it already has.
///
/// `nullopt` when the layer is empty — there is nothing to read, and the
/// symbol's own shape is then the best guess available.
std::optional<PreviewShape> shape_of_layer(const core::Document& doc, core::LayerId layer)
{
    const auto& entities               = doc.entities();
    const core::RingGeometry& geometry = doc.geometry();

    bool any_closed = false;
    bool any_open   = false;
    bool any_point  = false;
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e) || entities.layer[e] != layer) continue;

        // A POINT IS NOT A SHORT LINE. Its ring is stored Open — a NOKTA outlines
        // to a single vertex — so a walk that asked only "open or closed" read a
        // layer of nirengi as a layer of lines, opened the designer on the Çizgi
        // tab, offered a gallery of boundary gösterims and put a stroke's cap and
        // join in the panel. Everything downstream of this answer was then about
        // the wrong geometry.
        if (entities.kind[e] == core::kPointKind) {
            any_point = true;
            continue;
        }

        const core::RingSpan span = geometry.rings_of(entities.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            if (geometry.ring_role[r] != core::RingRole::Open)
                any_closed = true;
            else if (geometry.ring_xs(r).size() >= 2)
                any_open = true;
            else
                any_point = true; // a lone vertex, whatever kind carried it
        }
        // A closed ring settles it; nothing later can make the layer less of an
        // area layer, so the walk stops rather than touching every parcel.
        if (any_closed) return PreviewShape::Area;
    }

    // Area beats line beats point: a layer that holds any parcel is an area
    // layer, and the tab has to open on the geometry the user will spend their
    // time on rather than on whatever happened to be drawn last.
    if (any_open) return PreviewShape::Line;
    if (any_point) return PreviewShape::Point;
    return std::nullopt;
}

PreviewShape shape_of(core::SymbolKind kind)
{
    switch (kind) {
    case core::SymbolKind::Area: return PreviewShape::Area;
    case core::SymbolKind::Line: return PreviewShape::Line;
    case core::SymbolKind::Point: return PreviewShape::Point;
    }
    return PreviewShape::Area;
}

/// Turns the "no symbology yet" symbol into the one that geometry can use.
///
/// An unstyled layer's symbol is a single plain stroke — the appearance every CAD
/// entity has always carried. On a POINT that describes nothing: the panel then
/// offered a line colour, a cap and a join for a thing with no ends, and the
/// layer row read "Çizgi" over a dot. Only the untouched default is converted, so
/// a symbol somebody actually built is never rewritten under them.
///
/// Returns true when it changed something.
bool adopt_geometry_default(core::Symbol& symbol, PreviewShape shape)
{
    if (shape != PreviewShape::Point) return false;
    if (symbol.layers.size() != 1) return false;

    core::SymbolLayer& only = symbol.layers.front();
    if (only.type != SymbolLayerType::SimpleLine) return false;
    if (!only.size.empty() || !only.interval.empty()) return false; // somebody set these

    only.type      = SymbolLayerType::SimpleMarker;
    only.size      = core::Measure{render::kDefaultPointSizeUm, core::Unit::Paper};
    only.shape     = core::MarkerShape::Circle;
    only.placement = core::MarkerPlacement::Vertex;
    // The stroke colour becomes the disc, which is what the canvas already draws
    // for a point with no symbology at all — so opening the designer does not
    // change how the drawing looks.
    if (only.look.fill_rgba == 0) only.look.fill_rgba = only.look.rgba;
    return true;
}

core::SymbolKind kind_of(PreviewShape shape)
{
    switch (shape) {
    case PreviewShape::Area: return core::SymbolKind::Area;
    case PreviewShape::Line: return core::SymbolKind::Line;
    case PreviewShape::Point: return core::SymbolKind::Point;
    }
    return core::SymbolKind::Area;
}

} // namespace

/// The category table's model, `design.md` §8: tick · swatch · value · legend
/// name · count. It owns nothing — the rows are the designer's — and paints
/// nothing: the grid does, from the roles this answers.
class CategoryModel : public QAbstractTableModel
{
public:
    using IconFor = std::function<QIcon(const StyleCategory&)>;

    CategoryModel(QVector<StyleCategory>& rows, IconFor icons, QObject* parent)
        : QAbstractTableModel(parent), rows_(rows), icons_(std::move(icons))
    {}

    /// The rows were rebuilt wholesale.
    void reload()
    {
        beginResetModel();
        endResetModel();
    }

    /// One row's symbol or count changed — the two columns the designer writes.
    /// Only those two, on purpose: the tick and the value are the USER's columns,
    /// and the designer recounts when they change; a notice covering them here
    /// would make that recount notify itself without end.
    void rowChanged(int row)
    {
        if (row < 0 || row >= rows_.size()) return;
        emit dataChanged(index(row, 1), index(row, 1));
        emit dataChanged(index(row, 4), index(row, 4));
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 5;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal) return {};
        if (role == Qt::TextAlignmentRole)
            return QVariant(static_cast<int>((section == 4 ? Qt::AlignRight : Qt::AlignLeft) |
                                             Qt::AlignVCenter));
        if (role != Qt::DisplayRole) return {};
        switch (section) {
        case 1: return tr("SEMBOL");
        case 2: return tr("DEĞER");
        case 3: return tr("GÖSTERİM ADI"); // ui-label
        case 4: return tr("NESNE");
        default: return QString();
        }
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= rows_.size()) return {};
        const StyleCategory& row = rows_[index.row()];
        switch (index.column()) {
        case 0:
            if (role == Qt::CheckStateRole)
                return row.other || row.enabled ? Qt::Checked : Qt::Unchecked;
            return {};
        case 1:
            if (role == Qt::DecorationRole && icons_) return icons_(row);
            return {};
        case 2:
            if (role == Qt::DisplayRole || role == Qt::EditRole) return row.value;
            if (role == Qt::TextAlignmentRole)
                return QVariant(
                    static_cast<int>((row.numeric && !row.other ? Qt::AlignRight : Qt::AlignLeft) |
                                     Qt::AlignVCenter));
            return {};
        case 3:
            if (role == Qt::DisplayRole || role == Qt::EditRole) return row.label;
            return {};
        case 4:
            if (role == Qt::DisplayRole) return QString::number(row.count);
            if (role == Qt::TextAlignmentRole)
                return QVariant(static_cast<int>(Qt::AlignRight | Qt::AlignVCenter));
            return {};
        default: return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid()) return Qt::NoItemFlags;
        Qt::ItemFlags base = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        const bool other   = rows_[index.row()].other;
        if (index.column() == 0 && !other) base |= Qt::ItemIsUserCheckable;
        if (index.column() == 3 || (index.column() == 2 && !other)) base |= Qt::ItemIsEditable;
        return base;
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        if (!index.isValid() || index.row() >= rows_.size()) return false;
        StyleCategory& row = rows_[index.row()];
        if (index.column() == 0 && role == Qt::CheckStateRole && !row.other) {
            row.enabled = static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked;
            emit dataChanged(index, index);
            return true;
        }
        if (role != Qt::EditRole) return false;
        const QString text = value.toString().trimmed();
        if (text.isEmpty()) return false;
        if (index.column() == 2 && !row.other) {
            row.value = text;
            emit dataChanged(index, index);
            return true;
        }
        if (index.column() == 3) {
            row.label = text;
            emit dataChanged(index, index);
            return true;
        }
        return false;
    }

private:
    QVector<StyleCategory>& rows_;
    IconFor icons_;
};

StyleDesigner::StyleDesigner(Controller& controller, QString layerName, QWidget* parent)
    : DialogFrame(parent), controller_(controller), layerName_(std::move(layerName))
{
    // KentOS's continuous three-column workspace, using the control and spacing
    // rules in data/design/design.md §8 and §16.
    setHeading(Glyph::Palette, tr("Stil tasarımcısı"), tr("— %1").arg(layerName_));
    setHelpVisible(true);
    setFooterHeight(48);
    setModal(true);
    // Keep the ordinary property form visible; longer types still scroll on a
    // small screen. The central preview absorbs the resize.
    setMinimumSize(1040, 680);
    resize(1360, 900);

    const core::LayerId layer = controller_.document().find_layer(layerName_.toStdString());
    symbol_ = layer == core::kNoLayer ? core::Symbol::of(core::Appearance{})
                                      : symbol_of_layer(controller_.document(), layer);
    if (symbol_.layers.empty()) symbol_ = core::Symbol::of(core::Appearance{});
    original_    = symbol_;
    draftImages_ = controller_.document().images();
    draftDashes_ = controller_.document().dashes();

    // ---- the geometry the symbol is drawn on, and the big preview ----
    //
    // A SEGMENT IN THE TOP STRIP, not a tab bar of its own above the shelf.
    //
    // It was drawn as three tabs joined to a pane, and a great deal of care went
    // into making them read as tabs — because a row of three buttons floating
    // over a dialog background reads as nothing at all. But the reason they read
    // as nothing was never the drawing: it was the PLACE. They sat alone between
    // the renderer strip and the shelf's caption, a band belonging to neither,
    // and they are the same kind of decision as the three controls above them —
    // what this symbol IS, before anything about how it looks.
    //
    // So they go where that decision is made, as the component the strip already
    // uses for a small closed choice (`SEMBOL BOYUT BİRİMİ` is the same control).
    // One band of decisions, one component for a choice of three, and the orphan
    // row is gone rather than dressed up.
    geometry_ = new Segment(this);
    geometry_->addOption(tr("Alan"), tr("Parsel, ada, yapı — kapalı alanlar"));
    geometry_->addOption(tr("Çizgi"), tr("Sınır, yol ekseni, kanal — çizgiler"));
    geometry_->addOption(tr("Nokta"), tr("Nirengi, poligon, ağaç — noktalar"));
    geometry_->setAccessibleName(tr("Sembolün çizileceği geometri"));
    const std::optional<PreviewShape> known = shape_of_layer(
        controller_.document(), controller_.document().find_layer(layerName_.toStdString()));
    geometry_->setCurrent(static_cast<int>(known.value_or(natural_shape(symbol_))));
    geometryAsked_ = !known.has_value();
    connect(geometry_, &Segment::currentChanged, this, [this](int) {
        adopt_geometry_default(symbol_, shape());
        refreshGalleryItems();
        refresh();
        updateHeaderNote();
    });

    // And once for the geometry the dialog OPENED on, which is the case a user
    // meets first: a point layer with no symbology of its own.
    adopt_geometry_default(symbol_, shape());

    // Three independent work areas: stack, live preview, selected layer form.
    // Each can grow without taking the form's vertical space away.
    auto* left       = new QWidget(this);
    auto* leftColumn = new QVBoxLayout(left);
    left->setObjectName(QStringLiteral("styleStackPanel"));
    leftColumn->setContentsMargins(16, 16, 16, 16);
    leftColumn->setSpacing(16);
    libraryDialog_ = new DialogFrame(this);
    libraryDialog_->setHeading(Glyph::Palette, tr("Stil kitaplığı"));
    libraryDialog_->resize(1000, 740);
    libraryDialog_->setMinimumSize(740, 540);
    libraryDialog_->setModal(true);
    auto* libraryBody   = new QWidget(libraryDialog_);
    auto* libraryLayout = new QVBoxLayout(libraryBody);
    libraryLayout->setContentsMargins(24, 20, 24, 20);
    libraryLayout->addWidget(buildGallery());
    libraryDialog_->setBody(libraryBody);
    auto* libraryClose =
        new Button(ButtonRole::Secondary, tr("Kapat"), std::nullopt, libraryDialog_);
    connect(libraryClose, &QPushButton::clicked, libraryDialog_, &QDialog::reject);
    libraryDialog_->footer()->addWidget(libraryClose);
    auto* browse = new Button(ButtonRole::Secondary, tr("Kitaplıktan seç…"), Glyph::Palette, left);
    connect(browse, &QPushButton::clicked, this, [this] {
        libraryDialog_->applyTheme(theme());
        libraryDialog_->exec();
    });
    leftColumn->addWidget(buildRendererRow());
    leftColumn->addWidget(buildTree(), 1);
    leftColumn->addWidget(browse);
    middle_ = new QStackedWidget(this);
    middle_->addWidget(new QWidget(this));
    middle_->addWidget(buildCategoryPage());
    middle_->hide();

    auto* previewFrame = new QFrame(this);
    previewFrame->setObjectName(QStringLiteral("stylePreview"));
    auto* previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(14, 12, 14, 14);
    previewLayout->setSpacing(12);
    auto* previewTools = new QHBoxLayout;
    sample_            = new ComboBox(previewFrame);
    sample_->addItem(tr("Standart geometri"));
    sample_->addItem(tr("Adalı alan / düz çizgi"));
    sample_->setAccessibleName(tr("Önizleme geometrisi"));
    previewTools->addWidget(sample_, 1);
    connect(sample_, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
    const auto zoomButton = [&](const QString& title, auto action) {
        auto* button = new Button(ButtonRole::Secondary, title, std::nullopt, previewFrame);
        button->setAccessibleName(title);
        connect(button, &QPushButton::clicked, this, action);
        previewTools->addWidget(button);
    };
    zoomButton(tr("−"), [this] { setPreviewScale(previewScale_->value() * 1.25); });
    zoomButton(tr("+"), [this] { setPreviewScale(previewScale_->value() / 1.25); });
    zoomButton(tr("1:1000"), [this] { setPreviewScale(1000.0); });
    previewLayout->addLayout(previewTools);
    auto* scaleRow   = new QHBoxLayout;
    auto* scaleLabel = new QLabel(tr("Çizim ölçeği 1:"), previewFrame);
    previewScale_    = new MeasureSpinBox(previewFrame);
    static_cast<MeasureSpinBox*>(previewScale_)->setDivisor(1.0);
    previewScale_->setRange(1, 1000000);
    previewScale_->setSingleStep(100);
    previewScale_->setValue(1000);
    previewScale_->setFixedWidth(120);
    previewScale_->setAccessibleName(tr("Önizleme çizim ölçeği"));
    previewScale_->setToolTip(
        tr("Zemin ölçüleri bu ölçeğe göre değişir; kâğıt ve piksel ölçüleri sabit kalır"));
    scaleLabel->setBuddy(previewScale_);
    scaleRow->addWidget(scaleLabel);
    scaleRow->addWidget(previewScale_);
    scaleRow->addStretch(1);
    previewLayout->addLayout(scaleRow);
    connect(previewScale_, &QSpinBox::valueChanged, this, [this] {
        updatePreview();
        updateHeaderNote();
    });
    preview_ = new QLabel(previewFrame);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(180, 180);
    preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    preview_->setObjectName(QStringLiteral("stylePreviewImage"));
    preview_->setAccessibleName(tr("Katman stili ön izlemesi"));
    preview_->setToolTip(
        tr("Yakınlaştırmak için tekerleği kullanın; sembol özellikleri için tıklayın"));
    preview_->installEventFilter(this);
    previewLayout->addWidget(preview_, 1);
    headerNote_ = new QLabel(previewFrame);
    headerNote_->setObjectName(QStringLiteral("quiet"));
    headerNote_->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(headerNote_);

    auto* right = new QWidget(this);
    right->setObjectName(QStringLiteral("symbolColumn"));
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(20, 18, 16, 18);
    rightLayout->setSpacing(16);
    symbolCaption_ = new QLabel(right);
    symbolCaption_->setObjectName(QStringLiteral("stylePanelTitle"));
    symbolCaption_->setWordWrap(true);
    rightLayout->addWidget(symbolCaption_);
    pages_ = new QStackedWidget(this);
    pages_->addWidget(buildGlobal());
    pages_->addWidget(buildProperties());
    pages_->setContentsMargins(0, 0, 8, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(pages_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightLayout->addWidget(scroll, 1);
    auto* panes = new QSplitter(Qt::Horizontal, this);
    panes->setChildrenCollapsible(false);
    panes->setObjectName(QStringLiteral("styleWorkspace"));
    panes->setHandleWidth(1);
    left->setMinimumWidth(240);
    right->setMinimumWidth(300);
    panes->addWidget(left);
    panes->addWidget(previewFrame);
    panes->addWidget(right);
    panes->setStretchFactor(0, 0);
    panes->setStretchFactor(1, 1);
    panes->setStretchFactor(2, 0);
    panes->setSizes({260, 700, 360});

    auto* renderer       = new QWidget(this);
    auto* rendererLayout = new QVBoxLayout(renderer);
    rendererLayout->setContentsMargins(0, 0, 0, 0);
    rendererLayout->setSpacing(16);
    rendererLayout->addWidget(middle_);
    rendererLayout->addWidget(panes, 1);
    sections_ = new Segment(this);
    sections_->addOption(tr("Stil"));
    sections_->addOption(tr("Katman bilgisi"));
    sections_->addOption(tr("Öznitelikler"));
    sections_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    pageStack_ = new QStackedWidget(this);
    pageStack_->addWidget(renderer);
    pageStack_->addWidget(buildInfoPage());
    schema_ = new SchemaPage(controller_, layerName_, this);
    pageStack_->addWidget(schema_);
    connect(sections_, &Segment::currentChanged, pageStack_, &QStackedWidget::setCurrentIndex);
    auto* body       = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    body->setObjectName(QStringLiteral("styleEditorBody"));
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    sections_->hide();
    bodyLayout->addWidget(pageStack_, 1);
    setBody(body);
    sections_->setCurrent(0);
    pageStack_->setCurrentIndex(0);

    // ---- the footer, §8 ----
    // Roles from the standard, and ONE primary: `Tamam`.

    // A REAL MENU, and it earned its arrow — which the style now draws
    // (`Button::setMenuArrow`) instead of a `▾` typed into the label. `Stil ▾`
    // was a plain button that did one thing — revert the window — while wearing
    // the mark of a button that opens a list; and beside it sat a second
    // full-width button for saving to the library, which is the rarest action in
    // the window taking the most room in the footer.
    //
    // `Stili temizle` moved in here from the layer's context menu, where it was
    // one of two style entries scattered among `Gizle` and `Gruba taşı…`. It is
    // the one style action that is not "edit the symbol", so it belongs with the
    // other things done TO a style rather than in it.
    auto* styleMenu = new Button(ButtonRole::Secondary, tr("Stil"), std::nullopt, this);
    auto* actions   = new QMenu(styleMenu);

    QAction* editPage = actions->addAction(tr("Stil tasarımına dön"));
    connect(editPage, &QAction::triggered, this, [this] { sections_->setCurrent(0); });
    QAction* infoPage = actions->addAction(tr("Katman bilgisi"));
    connect(infoPage, &QAction::triggered, this, [this] { sections_->setCurrent(1); });
    QAction* attributesPage = actions->addAction(tr("Öznitelikler"));
    connect(attributesPage, &QAction::triggered, this, [this] { sections_->setCurrent(2); });
    actions->addSeparator();

    QAction* revert = actions->addAction(tr("Katmanın çizdiğine dön"));
    revert->setToolTip(tr("Bu penceredeki değişiklikleri atar; katmana dokunmaz"));
    connect(revert, &QAction::triggered, this, &StyleDesigner::resetToLayer);

    QAction* clear = actions->addAction(tr("Stili temizle"));
    clear->setToolTip(tr("Katmanın stilini siler; nesneler katman görünümüne döner"));
    connect(clear, &QAction::triggered, this, &StyleDesigner::clearStyle);

    actions->addSeparator();

    // The road to STİLAKTAR. The manual named a `Dosya` menu entry that never
    // existed; the layer's own properties window is where its style lives, so
    // this is where it leaves from.
    QAction* toQgis = actions->addAction(tr("QGIS stiline aktar…"));
    toQgis->setToolTip(tr("Katmanın sembolojisini QGIS'in okuduğu .qml dosyası olarak yazar"));
    connect(toQgis, &QAction::triggered, this, [this] {
        ExportDialog window(controller_, ExportSubject::Style, layerName_, this);
        window.applyTheme(theme());
        window.exec();
    });

    QAction* save = actions->addAction(tr("Sembolü kütüphaneye kaydet…"));
    save->setToolTip(tr("Sembolü kendi gösterim paketiniz olarak diske yazar")); // ui-label
    connect(save, &QAction::triggered, this, &StyleDesigner::saveToLibrary);

    styleMenu->setMenuArrow(actions);

    auto* cancel = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* apply = new Button(ButtonRole::Secondary, tr("Uygula"), std::nullopt, this);
    apply->setToolTip(tr("Sembolü STİL komutlarına çevirip katmana yazar; pencere açık kalır"));
    connect(apply, &QPushButton::clicked, this, [this] { (void)applyToDocument(); });

    auto* ok = new Button(ButtonRole::Primary, tr("Tamam"), std::nullopt, this);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, [this] {
        if (applyToDocument()) accept();
    });

    QHBoxLayout* bar = footer();
    bar->insertWidget(0, styleMenu);
    bar->addWidget(cancel);
    bar->addWidget(apply);
    bar->addWidget(ok);

    // The look, in one place. Written against PALETTE ROLES rather than fixed
    // colours so a dark desktop theme gets a dark dialog: this window is opened
    // from a canvas people keep dark all day, and a sheet of hard-coded #f0f0f0
    // in the middle of it is the thing that makes an application feel bolted
    // together.
    // NO STYLESHEET HERE. This dialog used to carry its own, with its own greys
    // and its own radii, and that is exactly why it matched neither the shell nor
    // the other dialog. There is one sheet for the application (`theme.cpp`), it
    // is built from `tokens.hpp`, and a widget that needs a role asks for it by
    // object name — `sectionTitle`, `quiet`, `mono` — rather than restating the
    // colour. `ci-gate-theme.sh` keeps this true.

    refreshGalleryTree();
    refreshGalleryItems();
    refresh();
    selectTopLayer();
    updateHeaderNote();
    updateSymbolCaption();

    // A layer classified in an earlier session comes back as its classes, not as
    // one symbol: the package the designer wrote is re-read and the symbols are
    // taken from the document, which is the truth after an Apply.
    restoreClassification();
}

// ------------------------------------------------------------- the renderer ----

namespace {

/// The package's colour notation, `#AARRGGBB`.
QString hexColour(std::uint32_t rgba)
{
    return QStringLiteral("#%1").arg(rgba, 8, 16, QLatin1Char('0'));
}

/// A string as JSON writes it.
QString jsonText(const QString& text)
{
    QString out = text;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

/// A value quoted for the command line; the parser reads a quoted value whole.
QString quotedArg(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

/// Whether a column holds figures a range can compare.
bool numericType(core::AttrType type)
{
    return type == core::AttrType::Int64 || type == core::AttrType::Length ||
           type == core::AttrType::Decimal;
}

/// A class colour from the ramp. Derived from the symbol's own colour, not read
/// from a table of literals: hues spaced around the wheel, one hue from dark to
/// light, or greys — `Sınıflandır` hands the classes something to tell apart by
/// and the designer takes it from there.
QColor rampColour(int ramp, int i, int n, const QColor& base)
{
    // `float` throughout: that is what `QColor::fromHsvF` takes in Qt 6, and a
    // double here would be converted at the call with a warning on every line.
    const float t   = n <= 1 ? 0.0F : static_cast<float>(i) / static_cast<float>(n - 1);
    const float hue = base.hsvHueF() < 0.0F ? 0.58F : base.hsvHueF();
    switch (ramp) {
    case 1: return QColor::fromHsvF(hue, 0.55F, 0.35F + 0.6F * t);
    case 2: return QColor::fromHsvF(0.0F, 0.0F, 0.35F + 0.55F * t);
    default:
        return QColor::fromHsvF(
            std::fmod(hue + static_cast<float>(i) / static_cast<float>(std::max(1, n)), 1.0F), 0.5F,
            0.82F);
    }
}

/// Paints every unlocked layer of `symbol` in `colour`: fills in it, strokes and
/// glyphs a step darker so the boundary still reads against the fill.
void recolour(core::Symbol& symbol, const QColor& colour)
{
    for (core::SymbolLayer& l : symbol.layers) {
        if (l.colour_locked) continue;
        if (core::draws_fill(l.type)) {
            QColor fill = colour;
            fill.setAlpha(l.look.fill_rgba != 0 ? qAlpha(l.look.fill_rgba) : 255);
            l.look.fill_rgba = fill.rgba();
            l.look.src_fill  = core::Source::Explicit;
        }
        if (core::draws_stroke(l.type) || core::draws_marker(l.type)) {
            QColor ink = colour.darker(135);
            ink.setAlpha(qAlpha(l.look.rgba) != 0 ? qAlpha(l.look.rgba) : 255);
            l.look.rgba       = ink.rgba();
            l.look.src_colour = core::Source::Explicit;
        }
    }
}

/// A raw attribute number as the column prints it — `3 480.00` for a decimal
/// with two places, `1284` for a whole number.
QString displayNumber(std::int64_t raw, const core::AttrSpec& spec)
{
    core::AttrValue v;
    v.type    = spec.type;
    v.present = true;
    v.number  = raw;
    v.scale   = spec.scale;
    return QString::fromStdString(core::attr_display(v, core::DecimalMark::Point));
}

/// Whether one object's value falls in a class.
bool classMatches(const StyleCategory& c, const core::AttrValue& v)
{
    if (c.other || !v.present) return false;
    if (c.numeric) {
        if (!numericType(v.type) && v.type != core::AttrType::Bool &&
            v.type != core::AttrType::Date)
            return false;
        if (c.has_low && v.number < c.low) return false;
        if (c.has_high && v.number > c.high) return false;
        return true;
    }
    return QString::fromStdString(core::attr_display(v, core::DecimalMark::Point)) == c.value;
}

} // namespace

QWidget* StyleDesigner::buildCategoryPage()
{
    auto* page   = new QWidget(this);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 8, 0, 0);
    column->setSpacing(8);

    // THE ONE TABLE, at §8's widths: tick · swatch · value · legend name · count.
    categoryModel_ = new CategoryModel(
        categories_,
        [this](const StyleCategory& c) {
            return symbol_icon(c.symbol, draftImages_, draftDashes_, QSize(58, 17),
                               palette().color(QPalette::Base).rgba(), shape());
        },
        this);
    categoryGrid_ = new DataGrid(page);
    categoryGrid_->setModel(categoryModel_);
    categoryGrid_->setRowNumbers(false);
    categoryGrid_->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryGrid_->setEditors([](int) { return as_cell(field_of(FieldKind::Text)); });
    categoryGrid_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                   QAbstractItemView::EditKeyPressed);
    categoryGrid_->setColumnWidth(0, 34);
    categoryGrid_->setColumnWidth(1, 74);
    categoryGrid_->setColumnWidth(2, 200);
    categoryGrid_->setColumnWidth(4, 86);
    categoryGrid_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    categoryGrid_->setAccessibleName(tr("Sınıf tablosu"));
    connect(categoryGrid_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& now, const QModelIndex&) {
                if (now.isValid()) selectCategory(now.row());
            });
    connect(categoryModel_, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& from, const QModelIndex&, const QList<int>&) {
                // A class switched off or renamed by hand changes who gets what.
                if (from.column() == 0 || from.column() == 2) refreshCategoryCounts();
                if (from.column() == 3 && from.row() == editingCategory_) updateSymbolCaption();
            });
    column->addWidget(categoryGrid_, 1);

    // ---- the row under the table, §8: classify · add · remove · remove all ·
    //      the class count and method for a graduated renderer · advanced ----
    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(12, 0, 12, 8);
    foot->setSpacing(8);

    // Secondaries all: the window's one primary is `Tamam` in the footer.
    auto* classify = new Button(ButtonRole::Secondary, tr("Sınıflandır"), std::nullopt, page);
    classify->setToolTip(tr("Sütundaki her değer için bir sınıf kurar; renkleri skaladan verir"));
    connect(classify, &QPushButton::clicked, this, &StyleDesigner::classify);
    foot->addWidget(classify);

    auto* add = new Button(ButtonRole::Secondary, tr("Ekle"), std::nullopt, page);
    add->setToolTip(tr("Elle bir sınıf ekler; değerini tabloda yazın"));
    connect(add, &QPushButton::clicked, this, &StyleDesigner::addCategory);
    foot->addWidget(add);

    removeCategory_ = new Button(ButtonRole::Secondary, tr("Sil"), std::nullopt, page);
    connect(removeCategory_, &QPushButton::clicked, this, &StyleDesigner::removeCategory);
    foot->addWidget(removeCategory_);

    auto* clearAll = new Button(ButtonRole::Secondary, tr("Tümünü sil"), std::nullopt, page);
    connect(clearAll, &QPushButton::clicked, this, &StyleDesigner::clearCategories);
    foot->addWidget(clearAll);
    foot->addStretch(1);

    graduatedCell_ = new QWidget(page);
    auto* grad     = new QHBoxLayout(graduatedCell_);
    grad->setContentsMargins(0, 0, 0, 0);
    grad->setSpacing(8);
    auto* countCaption = new QLabel(tr("SINIF"), graduatedCell_);
    countCaption->setObjectName(QStringLiteral("groupCaption"));
    grad->addWidget(countCaption);
    classCount_ = new Field(number_of(2, 20), graduatedCell_);
    classCount_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    classCount_->setFixedWidth(64);
    classCount_->setValue(QStringLiteral("5"));
    classCount_->setAccessibleName(tr("Sınıf sayısı"));
    grad->addWidget(classCount_);
    auto* modeCaption = new QLabel(tr("YÖNTEM"), graduatedCell_);
    modeCaption->setObjectName(QStringLiteral("groupCaption"));
    grad->addWidget(modeCaption);
    classMode_ = new Segment(graduatedCell_);
    classMode_->addOption(tr("Eşit aralık"), tr("Değer aralığı eşit parçalara bölünür"));
    classMode_->addOption(tr("Eşit sayı"), tr("Her sınıfa eşit sayıda nesne düşer"));
    classMode_->setControlSize(ControlSize::Regular);
    grad->addWidget(classMode_);
    graduatedCell_->setVisible(false);
    foot->addWidget(graduatedCell_);

    // Said, not silently absent (§11.8). The `‹diğer›` row already takes every
    // unclassified object, so nothing is lost while merging waits.
    auto* merge = new CheckBox(tr("Diğer değerleri birleştir"), page);
    merge->setEnabled(false);
    merge->setToolTip(tr("Sınıfları birleştirme Faz 2'de gelecek; ‹diğer› satırı sınıfsız kalan "
                         "her nesneyi zaten alır"));
    foot->addWidget(merge);

    auto* advanced = new Button(ButtonRole::Secondary, tr("Gelişmiş"), std::nullopt, page);
    auto* more     = new QMenu(advanced);
    more->addAction(tr("Sembol seviyeleri… — Faz 2'de gelecek"))->setEnabled(false);
    more->addAction(tr("Kayıtlı sembollerle eşleştir — Faz 2'de gelecek"))->setEnabled(false);
    advanced->setMenuArrow(more);
    foot->addWidget(advanced);

    column->addLayout(foot);
    return page;
}

void StyleDesigner::fillValueColumns()
{
    if (renderValue_ == nullptr) return;
    const QString keep = renderValue_->currentText();
    const Held quiet(loading_);
    renderValue_->clear();
    const core::AttrTable& table = controller_.document().attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const core::AttrColumn* column = table.column(static_cast<core::AttrId>(c));
        if (column == nullptr) continue;
        // A graduated renderer classes FIGURES; offering it a text column would
        // offer a range over words.
        if (renderer_ == Renderer::Graduated && !numericType(column->spec().type)) continue;
        // Only the columns this layer carries: a project column or its own.
        if (!core::attr_applies_to(column->spec(), layerName_.toStdString())) continue;
        renderValue_->addItem(QString::fromStdString(column->spec().id));
    }
    const int back = renderValue_->findText(keep);
    if (back >= 0) renderValue_->setCurrentIndex(back);
}

QString StyleDesigner::rendererValueColumn() const
{
    return renderValue_ != nullptr ? renderValue_->currentText() : QString();
}

void StyleDesigner::setRenderer(Renderer kind)
{
    // Leaving the single symbol keeps it aside; coming back restores it. A class
    // being edited is stored first so nothing typed into it is lost.
    if (renderer_ == Renderer::Single && kind != Renderer::Single) single_ = symbol_;
    storeEditedCategory();

    const Renderer before = renderer_;
    renderer_             = kind;
    editingCategory_      = -1;
    if (kind == Renderer::Single || before != kind) {
        categories_.clear();
        if (!single_.layers.empty()) symbol_ = single_;
    }
    {
        const Held quiet(loading_);
        renderKind_->setCurrentIndex(static_cast<int>(kind));
    }
    fillValueColumns();
    valueCell_->setVisible(kind != Renderer::Single);
    rampCell_->setVisible(kind != Renderer::Single);
    graduatedCell_->setVisible(kind == Renderer::Graduated);
    middle_->setCurrentIndex(kind == Renderer::Single ? 0 : 1);
    middle_->setVisible(kind != Renderer::Single);
    refreshCategoryGrid();
    refresh();
    selectTopLayer();
    updateSymbolCaption();
}

void StyleDesigner::classify()
{
    const QString column = rendererValueColumn();
    if (column.isEmpty()) {
        QMessageBox::information(
            this, tr("Sınıflandır"),
            tr("Önce DEĞER için bir sütun seçin. Katmanın sütunu yoksa Öznitelikler "
               "sayfasında tanımlayın."));
        return;
    }
    const core::Document& doc  = controller_.document();
    const core::AttrId col     = doc.attributes().find(column.toStdString());
    const core::LayerId layer  = doc.find_layer(layerName_.toStdString());
    const core::AttrColumn* at = col == core::kNoAttr ? nullptr : doc.attributes().column(col);
    if (at == nullptr || layer == core::kNoLayer) return;
    const core::AttrSpec& spec = at->spec();

    storeEditedCategory();
    editingCategory_ = -1;
    if (single_.layers.empty()) single_ = symbol_;
    const core::Symbol base = single_;
    galleryCode_.clear();
    galleryPackage_.clear();

    QVector<StyleCategory> fresh;
    int leftover = 0;

    if (renderer_ == Renderer::Categorized) {
        // Distinct values in first-seen order, which is the order of the drawing
        // and therefore the order the user already knows.
        for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
            if (!doc.alive(e) || doc.entities().layer[e] != layer) continue;
            const auto cell = doc.attribute(col, e);
            if (!cell || !cell.value().present) {
                ++leftover;
                continue;
            }
            const QString text =
                QString::fromStdString(core::attr_display(cell.value(), core::DecimalMark::Point));
            bool seen = false;
            for (StyleCategory& c : fresh)
                if (c.value == text) {
                    ++c.count;
                    seen = true;
                    break;
                }
            if (seen) continue;
            StyleCategory c;
            c.value   = text;
            c.label   = text;
            c.count   = 1;
            c.numeric = numericType(spec.type) || spec.type == core::AttrType::Bool ||
                        spec.type == core::AttrType::Date;
            c.low = c.high = cell.value().number;
            c.has_low = c.has_high = c.numeric;
            fresh.push_back(c);
        }
    } else {
        // Classes over the figures: equal width, or equal count.
        std::vector<std::int64_t> numbers;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
            if (!doc.alive(e) || doc.entities().layer[e] != layer) continue;
            const auto cell = doc.attribute(col, e);
            if (!cell || !cell.value().present) {
                ++leftover;
                continue;
            }
            numbers.push_back(cell.value().number);
        }
        if (numbers.size() < 2) {
            QMessageBox::information(this, tr("Sınıflandır"),
                                     tr("Derecelendirmek için sütunda en az iki değer olmalı."));
            return;
        }
        std::sort(numbers.begin(), numbers.end());
        const int wanted      = std::clamp(classCount_->value().toInt(), 2, 20);
        const bool equalCount = classMode_->current() == 1;
        const std::int64_t lo = numbers.front();
        const std::int64_t hi = numbers.back();
        std::int64_t from     = lo;
        for (int k = 0; k < wanted && from <= hi; ++k) {
            std::int64_t to;
            if (k == wanted - 1) {
                to = hi;
            } else if (equalCount) {
                const std::size_t cut = std::min(
                    numbers.size() - 1, (numbers.size() * static_cast<std::size_t>(k + 1)) /
                                            static_cast<std::size_t>(wanted));
                to = std::max(from, numbers[cut] - 1);
            } else {
                to = std::max(from, lo + ((hi - lo) * (k + 1)) / wanted - (k + 1 < wanted ? 1 : 0));
            }
            if (to < from) to = from;
            StyleCategory c;
            c.numeric  = true;
            c.has_low  = true;
            c.has_high = true;
            c.low      = from;
            c.high     = to;
            c.value =
                QStringLiteral("%1 – %2").arg(displayNumber(from, spec), displayNumber(to, spec));
            c.label = c.value;
            for (const std::int64_t n : numbers)
                if (n >= from && n <= to) ++c.count;
            fresh.push_back(c);
            from = to + 1;
        }
    }

    // Colours from the ramp, then the row that takes whatever is left.
    const QColor baseColour =
        from_rgba(base.primary().fill_rgba != 0 ? base.primary().fill_rgba : base.primary().rgba);
    for (int i = 0; i < fresh.size(); ++i) {
        fresh[i].symbol = base;
        recolour(fresh[i].symbol,
                 rampColour(ramp_->currentIndex(), i, static_cast<int>(fresh.size()), baseColour));
    }
    StyleCategory other;
    other.other  = true;
    other.value  = tr("‹diğer›");
    other.label  = tr("diğer değerler");
    other.count  = leftover;
    other.symbol = base;
    fresh.push_back(other);

    categories_ = std::move(fresh);
    refreshCategoryGrid();
    if (!categories_.isEmpty()) {
        categoryGrid_->setCurrentIndex(categoryModel_->index(0, 2));
        selectCategory(0);
    }
}

void StyleDesigner::addCategory()
{
    if (renderer_ == Renderer::Single) return;
    storeEditedCategory();
    if (single_.layers.empty()) single_ = symbol_;

    StyleCategory c;
    c.value  = tr("değer");
    c.label  = tr("yeni sınıf");
    c.symbol = single_;
    if (categories_.isEmpty()) {
        StyleCategory other;
        other.other  = true;
        other.value  = tr("‹diğer›");
        other.label  = tr("diğer değerler");
        other.symbol = single_;
        categories_.push_back(other);
    }
    // Before `‹diğer›`, which stays last: it is the rule with no condition and a
    // rule after it would never be reached.
    const int at = std::max(0, static_cast<int>(categories_.size()) - 1);
    categories_.insert(at, c);
    if (editingCategory_ >= at) ++editingCategory_;
    refreshCategoryGrid();
    categoryGrid_->setCurrentIndex(categoryModel_->index(at, 2));
    selectCategory(at);
    categoryGrid_->edit(categoryModel_->index(at, 2));
}

void StyleDesigner::removeCategory()
{
    const QModelIndex current = categoryGrid_->currentIndex();
    if (!current.isValid() || current.row() >= categories_.size()) return;
    if (categories_[current.row()].other) {
        QMessageBox::information(
            this, tr("Sil"), tr("‹diğer› satırı silinemez: sınıfsız kalan her nesneyi o alır."));
        return;
    }
    if (editingCategory_ == current.row()) editingCategory_ = -1;
    if (editingCategory_ > current.row()) --editingCategory_;
    categories_.remove(current.row());
    refreshCategoryGrid();
    const int next = std::min(current.row(), static_cast<int>(categories_.size()) - 1);
    if (next >= 0) {
        categoryGrid_->setCurrentIndex(categoryModel_->index(next, 2));
        selectCategory(next);
    }
}

void StyleDesigner::clearCategories()
{
    categories_.clear();
    editingCategory_ = -1;
    if (!single_.layers.empty()) symbol_ = single_;
    refreshCategoryGrid();
    refresh();
    selectTopLayer();
    updateSymbolCaption();
}

void StyleDesigner::selectCategory(int row)
{
    if (row < 0 || row >= categories_.size() || row == editingCategory_) return;
    storeEditedCategory();
    editingCategory_ = row;
    symbol_          = categories_[row].symbol;
    galleryCode_.clear();
    galleryPackage_.clear();
    refresh();
    selectTopLayer();
    updateSymbolCaption();
    if (removeCategory_ != nullptr) removeCategory_->setEnabled(!categories_[row].other);
}

void StyleDesigner::storeEditedCategory()
{
    if (editingCategory_ < 0 || editingCategory_ >= categories_.size()) return;
    if (categories_[editingCategory_].symbol == symbol_) return;
    categories_[editingCategory_].symbol = symbol_;
    if (categoryModel_ != nullptr) categoryModel_->rowChanged(editingCategory_);
}

void StyleDesigner::refreshCategoryGrid()
{
    if (categoryModel_ == nullptr) return;
    refreshCategoryCounts();
    categoryModel_->reload();
    if (removeCategory_ != nullptr) removeCategory_->setEnabled(false);
}

void StyleDesigner::refreshCategoryCounts()
{
    if (categories_.isEmpty() || recounting_) return;
    const Held once(recounting_);
    const core::Document& doc = controller_.document();
    const core::AttrId col    = doc.attributes().find(rendererValueColumn().toStdString());
    const core::LayerId layer = doc.find_layer(layerName_.toStdString());
    for (StyleCategory& c : categories_)
        c.count = 0;
    if (col == core::kNoAttr || layer == core::kNoLayer) return;

    // Who gets what, in rule order: the first enabled class that matches, else
    // `‹diğer›` — exactly how `StyleCatalog::classify` will read the package.
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.entities().layer[e] != layer) continue;
        const auto cell      = doc.attribute(col, e);
        StyleCategory* taker = nullptr;
        if (cell) {
            for (StyleCategory& c : categories_) {
                if (c.other || !c.enabled) continue;
                if (classMatches(c, cell.value())) {
                    taker = &c;
                    break;
                }
            }
        }
        if (taker == nullptr)
            for (StyleCategory& c : categories_)
                if (c.other) taker = &c;
        if (taker != nullptr) ++taker->count;
    }
    if (categoryModel_ != nullptr)
        for (int r = 0; r < categories_.size(); ++r)
            categoryModel_->rowChanged(r);
}

void StyleDesigner::updateSymbolCaption()
{
    if (symbolCaption_ == nullptr) return;
    QString whose = layerName_;
    if (renderer_ != Renderer::Single)
        whose = editingCategory_ >= 0 && editingCategory_ < categories_.size()
                    ? categories_[editingCategory_].label
                    : tr("sınıf seçilmedi");
    QString title      = tr("Sembol özellikleri");
    const int selected = currentLayer();
    if (selected >= 0 && selected < static_cast<int>(symbol_.layers.size())) {
        for (const TypeRow& row : kTypes)
            if (row.type == symbol_.layers[at(selected)].type) title = tr(row.label);
    }
    symbolCaption_->setText(title);
    symbolCaption_->setToolTip(whose);
}

QString StyleDesigner::classificationPackagePath() const
{
    // The application's own settings directory, beside the user's saved symbols
    // (`saveToLibrary`): a classification belongs to the person who made it and
    // travels with them. Named by the layer, so re-opening the window finds it.
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (root.isEmpty()) return {};
    QString slug = QLocale(QLocale::Turkish).toLower(layerName_);
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    return QDir(root).filePath(
        QStringLiteral("stiller/siniflar/%1-%2.json").arg(slug).arg(qHash(layerName_), 0, 16));
}

QString StyleDesigner::layerJson(const core::SymbolLayer& sl) const
{
    QStringList fields;
    const auto text = [&](const QString& key, const QString& value) {
        fields << jsonText(key) + QStringLiteral(": ") + jsonText(value);
    };
    const auto number = [&](const QString& key, qint64 value) {
        fields << jsonText(key) + QStringLiteral(": ") + QString::number(value);
    };
    const auto measure = [&](const QString& key, core::Measure value) {
        fields << jsonText(key) +
                      QStringLiteral(": {\"deger\": %1, \"birim\": %2}")
                          .arg(value.value)
                          .arg(jsonText(QString::fromUtf8(core::unit_name(value.unit))));
    };
    text(QStringLiteral("tip"), QString::fromUtf8(core::symbol_layer_type_name(sl.type)));
    text(QStringLiteral("renk"), hexColour(sl.look.rgba));
    text(QStringLiteral("dolgu_renk"), hexColour(sl.look.fill_rgba));
    number(QStringLiteral("kalinlik"), sl.look.width_um);
    number(QStringLiteral("aci"), sl.angle_udeg);
    number(QStringLiteral("opaklik"), sl.opacity);
    text(QStringLiteral("sekil"), QString::fromUtf8(core::marker_shape_name(sl.shape)));
    text(QStringLiteral("yerlesim"), QString::fromUtf8(core::marker_placement_name(sl.placement)));
    text(QStringLiteral("uc"), sl.cap == core::LineCap::Butt    ? QStringLiteral("duz")
                               : sl.cap == core::LineCap::Round ? QStringLiteral("yuvarlak")
                                                                : QStringLiteral("kare"));
    text(QStringLiteral("birlesim"), sl.join == core::LineJoin::Miter   ? QStringLiteral("kose")
                                     : sl.join == core::LineJoin::Round ? QStringLiteral("yuvarlak")
                                                                        : QStringLiteral("pah"));
    measure(QStringLiteral("boyut"), sl.size);
    measure(QStringLiteral("aralik"), sl.interval);
    measure(QStringLiteral("aralik_y"), sl.spacing_y);
    measure(QStringLiteral("kaydirma"), sl.offset);
    measure(QStringLiteral("faz"), sl.phase);
    text(QStringLiteral("yazi"), QString::fromStdString(sl.text));
    fields << QStringLiteral("\"etkin\": %1")
                  .arg(sl.enabled ? QStringLiteral("true") : QStringLiteral("false"));
    fields << QStringLiteral("\"renk_kilidi\": %1")
                  .arg(sl.colour_locked ? QStringLiteral("true") : QStringLiteral("false"));
    if (sl.image != core::kNoImage)
        text(QStringLiteral("gorsel"),
             QStringLiteral("image-%1").arg(draftImages_.content_key(sl.image), 0, 16));
    if (sl.look.dash != core::kSolidDash) {
        const core::DashPattern& pattern = draftDashes_.at(sl.look.dash);
        QStringList lengths;
        for (std::uint8_t i = 0; i < pattern.count; ++i)
            lengths << QString::number(pattern.lengths[i] / 100.0, 'f', 2);
        fields << QStringLiteral("\"desen\": [%1]").arg(lengths.join(QLatin1Char(',')));
    }
    QStringList bindings;
    for (const core::SymbolBinding& binding : sl.bindings)
        bindings << QStringLiteral("{\"alan\": %1, \"ozellik\": %2, \"tur\": %3}")
                        .arg(jsonText(QString::fromStdString(binding.field)),
                             jsonText(QString::fromUtf8(core::symbol_property_name(binding.what))),
                             jsonText(QString::fromUtf8(core::attr_type_name(binding.type))));
    fields << QStringLiteral("\"baglar\": [%1]").arg(bindings.join(QLatin1Char(',')));
    return QStringLiteral("{%1}").arg(fields.join(QLatin1Char(',')));
}

QString StyleDesigner::imagesJson() const
{
    std::set<core::ImageId> used;
    for (const core::SymbolLayer& layer : symbol_.layers)
        if (layer.image != core::kNoImage) used.insert(layer.image);
    for (const StyleCategory& category : categories_)
        for (const core::SymbolLayer& layer : category.symbol.layers)
            if (layer.image != core::kNoImage) used.insert(layer.image);
    QStringList images;
    for (const core::ImageId image : used) {
        const QString id = QStringLiteral("image-%1").arg(draftImages_.content_key(image), 0, 16);
        images << QStringLiteral("{\"id\": %1, \"dosya\": %2}")
                      .arg(jsonText(id), jsonText(QStringLiteral("assets/%1.bin").arg(id)));
    }
    return images.join(QLatin1Char(','));
}

bool StyleDesigner::writeDraftImages(const QString& directory, QString* error) const
{
    std::set<core::ImageId> used;
    for (const core::SymbolLayer& layer : symbol_.layers)
        if (layer.image != core::kNoImage) used.insert(layer.image);
    for (const StyleCategory& category : categories_)
        for (const core::SymbolLayer& layer : category.symbol.layers)
            if (layer.image != core::kNoImage) used.insert(layer.image);
    if (used.empty()) return true;
    if (!QDir(directory).mkpath(QStringLiteral("assets"))) {
        if (error) *error = tr("Dizin oluşturulamadı: %1").arg(directory);
        return false;
    }
    for (const core::ImageId image : used) {
        const QString id   = QStringLiteral("image-%1").arg(draftImages_.content_key(image), 0, 16);
        const QString path = QDir(directory).filePath(QStringLiteral("assets/%1.bin").arg(id));
        const auto bytes   = draftImages_.bytes(image);
        QSaveFile file(path);
        const auto size = static_cast<qint64>(bytes.size());
        if (bytes.empty() || !file.open(QIODevice::WriteOnly) ||
            file.write(reinterpret_cast<const char*>(bytes.data()), size) != size ||
            !file.commit()) {
            if (error) *error = tr("Dosya yazılamadı: %1").arg(path);
            return false;
        }
    }
    return true;
}

QString StyleDesigner::symbolPackageJson(const QString& name, const QString& id) const
{
    QStringList layers;
    for (const core::SymbolLayer& layer : symbol_.layers)
        layers << layerJson(layer);
    return QStringLiteral(
               "{\"schema_version\":1,\"package_version\":\"1.0.0\",\"id\":%1,"
               "\"source\":%2,\"published\":%3,\"licence\":\"kullanıcı\","
               "\"gorseller\":[%4],\"stiller\":[{\"id\":%1,\"ad\":%5,"
               "\"bolum\":[\"KULLANICI\"],\"geometri\":%7,\"katmanlar\":[%6]}],\"kurallar\":[]}")
        .arg(jsonText(id), jsonText(tr("Kullanıcı tanımlı — %1").arg(name)),
             jsonText(QDate::currentDate().toString(Qt::ISODate)), imagesJson(), jsonText(name),
             layers.join(QLatin1Char(',')),
             jsonText(QString::fromUtf8(core::symbol_kind_name(kind_of(shape())))));
}

QString StyleDesigner::categoryPackageJson() const
{
    const QString column = rendererValueColumn();

    QStringList styles;
    QStringList rules;
    int otherIndex = -1;
    for (int i = 0; i < categories_.size(); ++i) {
        const StyleCategory& c = categories_[i];
        QStringList layers;
        for (const core::SymbolLayer& sl : c.symbol.layers) {
            const QString one = layerJson(sl);
            if (!one.isEmpty()) layers << one;
        }
        styles << QStringLiteral("    {\n      \"id\": \"s%1\",\n      \"ad\": %2,\n"
                                 "      \"bolum\": [\"KULLANICI\", \"SINIFLAR\"],\n"
                                 "      \"kaynak\": \"Stil tasarımcısı\",\n"
                                 "      \"geometri\": %4,\n"
                                 "      \"katmanlar\": [\n%3\n      ]\n    }")
                      .arg(i)
                      .arg(jsonText(c.label), layers.join(QStringLiteral(",\n")),
                           jsonText(QString::fromUtf8(core::symbol_kind_name(kind_of(shape())))));
        if (c.other) {
            otherIndex = i;
            continue;
        }
        if (!c.enabled) continue;
        QString condition;
        if (c.numeric)
            condition =
                QStringLiteral("{ \"alan\": %1, \"aralik\": { \"en_az\": %2, \"en_cok\": %3 } }")
                    .arg(jsonText(column))
                    .arg(c.low)
                    .arg(c.high);
        else
            condition = QStringLiteral("{ \"alan\": %1, \"esittir\": %2 }")
                            .arg(jsonText(column), jsonText(c.value));
        rules << QStringLiteral("    { \"id\": \"k%1\", \"stil\": \"s%1\", \"kosullar\": [ %2 ] }")
                     .arg(i)
                     .arg(condition);
    }
    // THE RULE WITH NO CONDITION, LAST: everything the classes did not take. The
    // command refuses an unclassified object rather than drawing it grey, and
    // this row is what keeps that refusal from ever being reached.
    if (otherIndex >= 0)
        rules << QStringLiteral("    { \"id\": \"k-diger\", \"stil\": \"s%1\", \"kosullar\": [] }")
                     .arg(otherIndex);

    return QStringLiteral("{\n"
                          "  \"id\": \"sinif-%1\",\n"
                          "  \"package_version\": \"1.0.0\",\n"
                          "  \"source\": %2,\n"
                          "  \"published\": \"%3\",\n"
                          "  \"licence\": \"kullanıcı\",\n"
                          "  \"aciklama\": %4,\n"
                          "  \"stiller\": [\n%5\n  ],\n"
                          "  \"gorseller\": [%7],\n"
                          "  \"kurallar\": [\n%6\n  ]\n"
                          "}\n")
        .arg(QString::number(qHash(layerName_), 16),
             jsonText(tr("Kullanıcı tanımlı sınıflandırma — %1 · %2").arg(layerName_, column)),
             QDate::currentDate().toString(Qt::ISODate),
             jsonText(QStringLiteral("simgeleyici=%1;sutun=%2")
                          .arg(renderer_ == Renderer::Graduated ? QStringLiteral("derece")
                                                                : QStringLiteral("kategori"),
                               column)),
             styles.join(QStringLiteral(",\n")), rules.join(QStringLiteral(",\n")), imagesJson());
}

bool StyleDesigner::writeClassificationPackage(QString* error) const
{
    const QString path = classificationPackagePath();
    if (path.isEmpty()) {
        if (error) *error = tr("Uygulama ayar dizini bulunamadı.");
        return false;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = tr("Dizin oluşturulamadı: %1").arg(QFileInfo(path).absolutePath());
        return false;
    }
    if (!writeDraftImages(QFileInfo(path).absolutePath(), error)) return false;
    const QByteArray bytes = categoryPackageJson().toUtf8();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = tr("Paket yazılamadı: %1").arg(path);
        return false;
    }
    return true;
}

bool StyleDesigner::applyClassification()
{
    storeEditedCategory();
    if (categories_.isEmpty()) {
        QMessageBox::information(this, tr("Simgeleyici"),
                                 tr("Önce Sınıflandır ile sınıfları kurun ya da Ekle ile bir sınıf "
                                    "yazın; uygulanacak sınıf yok."));
        return false;
    }
    QString error;
    if (!writeClassificationPackage(&error)) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"), error);
        return false;
    }

    // ONE LINE. The package carries every class and its rule; the command reads
    // each object's columns against those rules and writes the style column —
    // exactly what a script that named the same package would get (Article 1.2,
    // model.md R14).
    const auto applied = controller_.runLineResult(
        QStringLiteral("STİL katman=%1 paket=%2")
            .arg(quotedArg(layerName_), quotedArg(classificationPackagePath())),
        command::Origin::Gui);
    if (!applied) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"),
                             QString::fromStdString(applied.error().message));
        return false;
    }
    refreshCategoryCounts();
    return true;
}

bool StyleDesigner::restoreClassification()
{
    const QString path = classificationPackagePath();
    if (path.isEmpty() || !QFile::exists(path)) return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = file.readAll();
    const auto parsed      = core::Json::parse(
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    if (!parsed) return false;
    auto catalog = core::StyleCatalog::from_json(parsed.value());
    if (!catalog) return false;

    // What the package says about itself: which renderer, which column.
    QString column;
    Renderer kind = Renderer::Categorized;
    if (const core::Json* about = parsed.value().find("aciklama"); about != nullptr) {
        const QString text = QString::fromStdString(about->as_string());
        for (const QString& part : text.split(QLatin1Char(';'))) {
            if (part.startsWith(QLatin1String("sutun="))) column = part.mid(6);
            if (part == QLatin1String("simgeleyici=derece")) kind = Renderer::Graduated;
        }
    }
    const core::Document& doc  = controller_.document();
    const core::AttrId col     = doc.attributes().find(column.toStdString());
    const core::LayerId layer  = doc.find_layer(layerName_.toStdString());
    const core::AttrColumn* at = col == core::kNoAttr ? nullptr : doc.attributes().column(col);
    if (at == nullptr || layer == core::kNoLayer) return false;

    // The symbols come from the DOCUMENT where an object carries them — that is
    // the truth after an Apply — and from the package row only for a class no
    // object matches today.
    const auto fromDocument = [&](const StyleCategory& c) -> std::optional<core::Symbol> {
        for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
            if (!doc.alive(e) || doc.entities().layer[e] != layer) continue;
            const auto cell = doc.attribute(col, e);
            if (!cell || !classMatches(c, cell.value())) continue;
            const core::StyleId style = doc.entities().style[e];
            if (style == core::kByLayerStyle) continue;
            return doc.styles().symbol_at(style);
        }
        return std::nullopt;
    };

    QVector<StyleCategory> restored;
    for (const core::StyleRule& rule : catalog.value().rules()) {
        const auto entry = catalog.value().entry(rule.entry);
        if (!entry) continue;
        StyleCategory c;
        c.label = QString::fromStdString(entry.value()->label);
        if (rule.conditions.empty()) {
            c.other = true;
            c.value = tr("‹diğer›");
        } else {
            const core::StyleCondition& cond = rule.conditions.front();
            if (cond.test == core::StyleCondition::Test::Range) {
                c.numeric  = true;
                c.has_low  = cond.has_low;
                c.has_high = cond.has_high;
                c.low      = cond.low;
                c.high     = cond.high;
                c.value    = c.low == c.high
                                 ? displayNumber(c.low, at->spec())
                                 : QStringLiteral("%1 – %2").arg(displayNumber(c.low, at->spec()),
                                                                 displayNumber(c.high, at->spec()));
            } else if (!cond.values.empty()) {
                c.value = QString::fromStdString(cond.values.front());
            }
        }
        if (auto carried = fromDocument(c); carried.has_value())
            c.symbol = std::move(carried.value());
        else {
            const QDir packageDir(QFileInfo(path).absolutePath());
            c.symbol = core::symbol_of_entry(
                *entry.value(),
                [&](const std::string& relative) {
                    QFile image(packageDir.filePath(QString::fromStdString(relative)));
                    if (!image.open(QIODevice::ReadOnly)) return core::kNoImage;
                    const QByteArray bytes = image.readAll();
                    const auto interned    = draftImages_.intern(
                        std::span(reinterpret_cast<const std::byte*>(bytes.constData()),
                                  static_cast<std::size_t>(bytes.size())),
                        relative);
                    return interned ? interned.value() : core::kNoImage;
                },
                [&](const core::DashPattern& pattern, std::string_view origin) {
                    const auto interned = draftDashes_.intern(pattern, std::string(origin));
                    return interned ? interned.value() : core::kSolidDash;
                });
        }
        restored.push_back(std::move(c));
    }
    if (restored.isEmpty()) return false;

    single_          = original_;
    renderer_        = kind;
    editingCategory_ = -1;
    categories_      = std::move(restored);
    {
        const Held quiet(loading_);
        renderKind_->setCurrentIndex(static_cast<int>(kind));
    }
    fillValueColumns();
    {
        const Held quiet(loading_);
        renderValue_->setCurrentText(column);
    }
    valueCell_->setVisible(true);
    rampCell_->setVisible(true);
    graduatedCell_->setVisible(kind == Renderer::Graduated);
    middle_->setCurrentIndex(1);
    refreshCategoryGrid();
    categoryGrid_->setCurrentIndex(categoryModel_->index(0, 2));
    selectCategory(0);
    return true;
}

// ---------------------------------------------- data-defined properties ----

void StyleDesigner::bindProperty(core::SymbolProperty what, QToolButton* button)
{
    const int i = currentLayer();
    if (i < 0) return;
    core::SymbolLayer& sl = symbol_.layers[at(i)];

    QString bound;
    for (const core::SymbolBinding& b : sl.bindings)
        if (b.what == what) bound = QString::fromStdString(b.field);

    // Which columns can drive this property: a colour takes a number or a text
    // (`#RRGGBB`), a measure takes a figure, a text takes anything.
    const auto suits = [what](core::AttrType type) {
        switch (what) {
        case core::SymbolProperty::Text: return true;
        case core::SymbolProperty::Colour:
        case core::SymbolProperty::Fill:
            return type == core::AttrType::Int64 || type == core::AttrType::Text ||
                   type == core::AttrType::CodeRef;
        default: return numericType(type);
        }
    };

    QMenu menu(this);
    QAction* head = menu.addAction(tr("Sütundan al"));
    head->setEnabled(false);
    const core::AttrTable& table = controller_.document().attributes();
    QVector<QAction*> choices;
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const core::AttrColumn* column = table.column(static_cast<core::AttrId>(c));
        if (column == nullptr || !suits(column->spec().type)) continue;
        if (!core::attr_applies_to(column->spec(), layerName_.toStdString())) continue;
        const QString id = QString::fromStdString(column->spec().id);
        QAction* choice  = menu.addAction(id);
        choice->setCheckable(true);
        choice->setChecked(id == bound);
        choice->setData(static_cast<int>(c));
        choices.push_back(choice);
    }
    if (choices.isEmpty()) {
        QAction* none = menu.addAction(tr("Uygun sütun yok — Öznitelikler sayfasında tanımlayın"));
        none->setEnabled(false);
    }
    menu.addSeparator();
    QAction* clear = menu.addAction(tr("Bağı kaldır"));
    clear->setEnabled(!bound.isEmpty());

    QAction* picked = menu.exec(button->mapToGlobal(QPoint(0, button->height())));
    if (picked == nullptr || picked == head) return;

    galleryCode_.clear();
    galleryPackage_.clear();
    std::erase_if(sl.bindings, [what](const core::SymbolBinding& b) { return b.what == what; });
    if (picked != clear) {
        const core::AttrColumn* column =
            table.column(static_cast<core::AttrId>(picked->data().toInt()));
        if (column != nullptr) {
            core::SymbolBinding b;
            b.field = column->spec().id;
            b.what  = what;
            b.type  = column->spec().type;
            sl.bindings.push_back(std::move(b));
            // A text taken from a column is not also a fixed word.
            if (what == core::SymbolProperty::Text) sl.text.clear();
        }
    }
    refresh();
}

void StyleDesigner::refreshBindingMarks()
{
    if (bindingMarks_.empty()) return;
    const int i     = currentLayer();
    const Tokens& t = theme() == ThemeMode::Dark ? darkTokens() : lightTokens();

    const auto editorOf = [this](core::SymbolProperty what) -> QWidget* {
        switch (what) {
        case core::SymbolProperty::Colour: return stroke_;
        case core::SymbolProperty::Fill: return fill_;
        case core::SymbolProperty::Width: return width_;
        case core::SymbolProperty::Size: return size_;
        case core::SymbolProperty::Angle: return angle_;
        case core::SymbolProperty::Opacity: return opacity_;
        case core::SymbolProperty::Text: return text_;
        }
        return nullptr;
    };

    for (auto& [what, mark] : bindingMarks_) {
        QString field;
        if (i >= 0)
            for (const core::SymbolBinding& b : symbol_.layers[at(i)].bindings)
                if (b.what == what) field = QString::fromStdString(b.field);
        const bool bound = !field.isEmpty();
        mark->setProperty("bound", bound);
        mark->style()->unpolish(mark);
        mark->style()->polish(mark);
        mark->setIcon(icon(Glyph::DataObject, bound ? t.accentHi : t.textFaint, t.accent, 16));
        mark->setToolTip(
            bound
                ? tr("Sütundan alınıyor: %1 — değiştirmek ya da kaldırmak için tıklayın").arg(field)
                : tr("Bu özelliği bir sütundan al"));
        // A driven property is not typed: the column decides, and a box that
        // still accepted a value would accept a value nothing reads.
        if (QWidget* editor = editorOf(what); editor != nullptr) editor->setEnabled(!bound);
    }
}

QStringList StyleDesigner::probeRenderer(const QString& column)
{
    QStringList out;
    {
        // Real painter/PDF clipping regression: two opposite-direction faces in
        // one batch, an actual hole, and a third face covering part of that hole.
        // These are backend inputs, not document mutations or a separate renderer.
        render::DrawList list;
        list.order = {0};
        list.passes.resize(1);
        list.polylines.resize(1);
        list.polygons.resize(1);
        auto& face   = list.polygons.front();
        face.xs      = {-120, 30,  30,  -120, -100, -60, -60, -100,
                        -30,  -30, 120, 120,  -85,  -65, -65, -85};
        face.ys      = {-90, -90, 60, 60, -70, -70, -30, -30, -30, 90, 90, -30, -60, -60, -40, -40};
        face.runs    = {4, 4, 4, 4};
        face.is_hole = {0, 1, 0, 0};
        face.rgba    = 0xffcc3377u;
        render::Overlay overlay;
        overlay.background_rgba = 0xffffffffu;
        const std::string svg   = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"12\" "
                                  "height=\"12\"><circle cx=\"6\" cy=\"6\" r=\"3\" "
                                  "fill=\"#cc3377\"/></svg>";
        auto backend            = make_builtin_backend();
        for (const auto type :
             {core::SymbolLayerType::SimpleFill, core::SymbolLayerType::LinePatternFill,
              core::SymbolLayerType::PointPatternFill, core::SymbolLayerType::RasterFill}) {
            auto& pass         = list.passes.front();
            pass.type          = type;
            pass.wants_stroke  = false;
            pass.wants_fill    = true;
            pass.line_rgba     = face.rgba;
            pass.fill_rgba     = face.rgba;
            pass.line_width_px = 2;
            pass.interval_px   = 8;
            pass.size_px       = 12;
            pass.image         = std::as_bytes(std::span(svg.data(), svg.size()));
            pass.image_key     = 0x6f7665726c6170u;
            bool correct       = true;
            for (const bool borrowed : {false, true}) {
                QImage image(400, 300, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::white);
                QPainter painter;
                if (borrowed) painter.begin(&image);
                render::FrameContext context;
                context.width_px          = image.width();
                context.height_px         = image.height();
                context.target_is_painter = borrowed;
                context.target = borrowed ? static_cast<void*>(&painter)
                                          : static_cast<void*>(static_cast<QPaintDevice*>(&image));
                backend->render(list, overlay, context);
                if (borrowed) painter.end();
                const auto ink = [&image](int x, int y, int radius) {
                    int count = 0;
                    for (int dy = -radius; dy <= radius; ++dy)
                        for (int dx = -radius; dx <= radius; ++dx)
                            if (image.pixelColor(200 + x + dx, 150 - y + dy).green() < 200) ++count;
                    return count;
                };
                correct = correct && ink(-110, 0, 7) > 0 && ink(0, 0, 7) > 0 &&
                          ink(90, 40, 7) > 0 && ink(-95, -50, 3) == 0 && ink(-75, -50, 7) > 0 &&
                          ink(75, -70, 7) == 0;
            }
            out << QStringLiteral("dolgu örtüşmesi / delik / çıktı: %1 · %2")
                       .arg(QString::fromUtf8(core::symbol_layer_type_name(type)),
                            correct ? QStringLiteral("tamam") : QStringLiteral("BAŞARISIZ"));
        }
    }
    setRenderer(Renderer::Single);
    const QString directory = QString::fromLocal8Bit(qgetenv("PIRICAD_DESIGNER_PROBE"));
    const auto capture      = [&](QWidget* window, const QString& name) {
        if (directory.isEmpty() || directory == QLatin1String("1")) return;
        QCoreApplication::processEvents();
        if (window->grab().save(QDir(directory).filePath(name)))
            out << QStringLiteral("kare: %1").arg(name);
    };
    out << QStringLiteral("kitaplık: %1 öğe").arg(controller_.bus().style_library().size());
    geometry_->setCurrent(static_cast<int>(PreviewShape::Area));
    search_->setText(QStringLiteral("orman"));
    libraryDialog_->applyTheme(theme());
    libraryDialog_->show();
    for (int i = 0; i < gallery_->count(); ++i) {
        auto* item = gallery_->item(i);
        if (!item->data(Qt::UserRole).isValid()) continue;
        gallery_->setCurrentItem(item);
        capture(libraryDialog_, QStringLiteral("kentos-sembol-kitapligi.png"));
        applyGalleryPick();
        // Editing clears the source selection, but must keep its image bytes.
        opacity_->setValue(230);
        capture(this, QStringLiteral("kentos-sembol-tasarimci.png"));
        const bool applied = applyToDocument();
        const auto* layer  = controller_.document().layer(
            controller_.document().find_layer(layerName_.toStdString()));
        const auto& stored   = controller_.document().styles().symbol_at(layer->style);
        bool images_survived = applied && stored.layers.size() == symbol_.layers.size();
        for (std::size_t j = 0; images_survived && j < symbol_.layers.size(); ++j) {
            if (symbol_.layers[j].image == core::kNoImage) continue;
            images_survived = controller_.document().images().content_key(stored.layers[j].image) ==
                              draftImages_.content_key(symbol_.layers[j].image);
        }
        out << QStringLiteral("sembol düzenle / uygula / SVG: %1")
                   .arg(images_survived ? QStringLiteral("tamam") : QStringLiteral("BAŞARISIZ"));
        if (applied) controller_.runLine(QStringLiteral("GERİAL"), command::Origin::Gui);
        break;
    }
    {
        const core::Symbol saved             = symbol_;
        symbol_                              = core::Symbol::of(core::Appearance{});
        symbol_.layers.front().look.rgba     = 0xff233f58u;
        symbol_.layers.front().look.width_um = 600;
        symbol_.layers.front().colour_locked = true;
        previewScale_->setValue(1000);
        addLayerOfType(SymbolLayerType::SimpleFill);
        symbol_.layers[at(currentLayer())].look.fill_rgba = 0xffeaf2f9u;
        addLayerOfType(SymbolLayerType::LinePatternFill);
        const int hatchIndex                      = currentLayer();
        symbol_.layers[at(hatchIndex)].look.rgba  = 0xff518193u;
        symbol_.layers[at(hatchIndex)].offset     = {2, core::Unit::Pixel};
        symbol_.layers[at(hatchIndex)].phase      = {1500, core::Unit::Ground};
        symbol_.layers[at(hatchIndex)].angle_udeg = 45000123;
        refresh();
        const auto check = [&](const QString& name, bool success) {
            out << QStringLiteral("%1: %2").arg(name, success ? QStringLiteral("tamam")
                                                              : QStringLiteral("BAŞARISIZ"));
        };
        core::Symbol localExpected                   = symbol_;
        localExpected.layers[at(hatchIndex)].opacity = 230;
        opacity_->setValue(230);
        check(QStringLiteral("katman opaklığı hassas açıyı ve diğer ölçüleri korur"),
              symbol_ == localExpected);
        intervalUnit_->setCurrentIndex(2);
        check(QStringLiteral("kâğıt → piksel"),
              symbol_.layers[at(hatchIndex)].interval == core::Measure{11, core::Unit::Pixel});
        interval_->stepUp();
        check(QStringLiteral("piksel adımı"), symbol_.layers[at(hatchIndex)].interval.value == 12);
        intervalUnit_->setCurrentIndex(1);
        check(QStringLiteral("piksel → zemin"),
              symbol_.layers[at(hatchIndex)].interval == core::Measure{3175, core::Unit::Ground});
        tree_->setCurrentItem(tree_->topLevelItem(0));
        core::Symbol expected = symbol_;
        for (auto& layer : expected.layers)
            layer.opacity = 128;
        globalOpacity_->setValue(128);
        check(QStringLiteral("opaklık diğer ölçüleri korur"), symbol_ == expected);
        for (auto& layer : expected.layers)
            if (!layer.colour_locked) {
                layer.look.width_um  = 900;
                layer.look.src_width = core::Source::Explicit;
            }
        globalWidth_->setValue(900);
        check(QStringLiteral("kalınlık birimleri ve kilitli kenarı korur"), symbol_ == expected);
        globalUnit_->setCurrentIndex(2);
        check(QStringLiteral("genel birim fazı da dönüştürür"),
              symbol_.layers[at(hatchIndex)].phase == core::Measure{6, core::Unit::Pixel});
        // Save a mixed-unit border/fill/hatch stack through the same command as a user.
        symbol_.layers[at(hatchIndex)].interval = {3000, core::Unit::Ground};
        symbol_.layers[at(hatchIndex)].phase    = {1500, core::Unit::Ground};
        refresh();
        const bool applied = applyToDocument();
        const auto* layer  = controller_.document().layer(
            controller_.document().find_layer(layerName_.toStdString()));
        const auto& stored = controller_.document().styles().symbol_at(layer->style);
        check(QStringLiteral("kenarlık + dolgu + tarama ve birimler uygulanır"),
              applied && stored.layers.size() == 3 &&
                  stored.layers[0].type == SymbolLayerType::SimpleFill &&
                  stored.layers[1].type == SymbolLayerType::LinePatternFill &&
                  stored.layers[2].type == SymbolLayerType::SimpleLine &&
                  stored.layers[1].interval == core::Measure{3000, core::Unit::Ground} &&
                  stored.layers[1].offset == core::Measure{2, core::Unit::Pixel} &&
                  stored.layers[1].phase == core::Measure{1500, core::Unit::Ground});
        if (applied) controller_.runLine(QStringLiteral("GERİAL"), command::Origin::Gui);
        for (auto& part : symbol_.layers)
            part.opacity = 255;
        symbol_.layers[at(hatchIndex)].look.width_um = 250;
        refresh();
        auto* root = tree_->topLevelItem(0);
        tree_->setCurrentItem(root->child(1));
        for (const int denominator : {1000, 500, 5000}) {
            previewScale_->setValue(denominator);
            capture(this, QStringLiteral("stil-zemin-%1.png").arg(denominator));
        }
        symbol_.layers[at(hatchIndex)].interval = {3000, core::Unit::Paper};
        refresh();
        for (const int denominator : {1000, 500, 5000}) {
            previewScale_->setValue(denominator);
            capture(this, QStringLiteral("stil-kagit-%1.png").arg(denominator));
        }
        previewScale_->setValue(1000);
        controller_.runLine(QStringLiteral("TERCİH tema koyu"), command::Origin::Gui);
        controller_.runLine(QStringLiteral("TERCİH tema acik"), command::Origin::Gui);
        applyTheme(ThemeMode::Light);
        capture(this, QStringLiteral("stil-katmanlari-acik.png"));
        resize(1040, 680);
        capture(this, QStringLiteral("stil-katmanlari-dar.png"));
        resize(1360, 900);
        controller_.runLine(QStringLiteral("TERCİH tema koyu"), command::Origin::Gui);
        applyTheme(ThemeMode::Dark);
        capture(this, QStringLiteral("stil-katmanlari-koyu.png"));
        tree_->setCurrentItem(tree_->topLevelItem(0));
        capture(this, QStringLiteral("stil-sembol-genel.png"));
        symbol_ = saved;
        refresh();
        selectTopLayer();
    }
    search_->clear();
    setRenderer(Renderer::Categorized);
    {
        const Held quiet(loading_);
        renderValue_->setCurrentText(column);
    }
    classify();
    for (const StyleCategory& c : categories_)
        out << QStringLiteral("%1 · %2 nesne%3")
                   .arg(c.label)
                   .arg(c.count)
                   .arg(c.other ? QStringLiteral(" · diğer") : QString());
    out << QStringLiteral("paket: %1").arg(classificationPackagePath());
    out << QStringLiteral("uygula: %1")
               .arg(applyToDocument() ? QStringLiteral("tamam") : QStringLiteral("olmadı"));

    std::set<core::StyleId> styles;
    const core::Document& doc = controller_.document();
    const core::LayerId layer = doc.find_layer(layerName_.toStdString());
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.alive(e) && doc.entities().layer[e] == layer)
            styles.insert(doc.entities().style[e]);
    out << QStringLiteral("katmanda %1 farklı stil").arg(styles.size());
    return out;
}

void StyleDesigner::applyTheme(ThemeMode mode)
{
    // THE SHELF ROWS TOO. A delegate is not a widget, so the walk over the
    // children never reaches it and it keeps the tokens it was built with.
    if (shelfRows_ != nullptr) static_cast<ShelfRow*>(shelfRows_)->setTheme(mode);

    DialogFrame::applyTheme(mode);
    if (sections_) sections_->applyTheme(mode);
    if (schema_) schema_->applyTheme(mode);

    // The preview background comes from the tokens, so it has to be drawn
    // again when they change — a dark lattice under a light dialog is exactly the
    // kind of leftover a theme switch is meant not to produce.
    if (preview_) updatePreview();

    // Every drawn mark re-tints with the theme; each carries its glyph in a
    // property, which keeps this from being a second list of buttons.
    const Tokens& t = mode == ThemeMode::Dark ? darkTokens() : lightTokens();
    for (QToolButton* b : findChildren<QToolButton*>()) {
        const QVariant glyph = b->property("glyph");
        if (glyph.isValid())
            b->setIcon(icon(static_cast<Glyph>(glyph.toInt()), t.textDim, t.accent, 16));
    }
    if (categoryGrid_ != nullptr) categoryGrid_->applyTheme(mode);
    refreshBindingMarks();
    // Row icons use the palette too: a dark-theme eye must not stay white on paper.
    if (tree_ != nullptr && pages_ != nullptr) refresh();
}

void StyleDesigner::updateHeaderNote()
{
    // Named in the user's own words, because "Alan" on a tab is a noun and what
    // they need to know is what it does to this window.
    QString note;
    switch (shape()) {
    case PreviewShape::Area:
        note = tr("kapalı alan"); // ui-label
        break;
    case PreviewShape::Line:
        note = tr("kırıklı çizgi"); // ui-label
        break;
    case PreviewShape::Point:
        note = tr("tek nokta"); // ui-label
        break;
    }
    headerNote_->setText(
        tr("Kâğıtta 1 mm = %1 px · %2")
            .arg(QLocale(QLocale::Turkish).toString(render::kDefaultPixelsPerPaperMm, 'f', 1))
            .arg(note));
    headerNote_->setToolTip(tr("Ön izleme bu geometri üzerinde çiziliyor; üstteki sekme seçer"));
}

PreviewShape StyleDesigner::shape() const
{
    return static_cast<PreviewShape>(std::clamp(geometry_->current(), 0, 2));
}

// ------------------------------------------------------------- the shelf ----

QWidget* StyleDesigner::buildRendererRow()
{
    // The renderer and its driving column belong to the left sidebar. Units
    // belong to the measure fields in the right property form.
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("rendererRow"));

    auto* row = new QVBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);

    // Each control under a SMALL-CAPS caption, as §8 draws it. These were 16 px
    // headings for a while, which made a strip of two controls read as two
    // sections of the window and cost the strip a third of its height.
    const auto field = [&](const QString& caption, QWidget* editor) {
        auto* cell   = new QWidget(bar);
        auto* column = new QVBoxLayout(cell);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);

        auto* label = new QLabel(caption, cell);
        label->setObjectName(QStringLiteral("formCaption"));
        column->addWidget(label);
        column->addWidget(editor);
        row->addWidget(cell);
        return cell;
    };

    // QGIS's renderer list, cut to what the model can state (style_rule.hpp:
    // Equals, OneOf, Range, Present — and no fifth). Each is a catalogue package
    // of rules and ONE `STİL katman= paket=` line; the rule-based renderer with
    // free expressions is the evaluator CLAUDE.md 5.11 forbids, and says so.
    renderKind_ = new ComboBox(bar);
    renderKind_->addItem(tr("Tek Sembol"));
    renderKind_->addItem(tr("Kategorize Edilmiş"));
    renderKind_->addItem(tr("Derecelendirilmiş"));
    renderKind_->addItem(tr("Kural Tabanlı"));
    renderKind_->setMinimumWidth(190);
    renderKind_->setAccessibleName(tr("Simgeleyici"));
    connect(renderKind_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (loading_) return;
        if (index == 3) {
            QMessageBox::information(
                this, tr("Simgeleyici"),
                tr("Kural tabanlı simgeleyici Faz 2'de gelecek. Bir kural tek bir alan "
                   "üzerinde tek sınamadır — eşitlik, liste, aralık, var — ve serbest bir "
                   "ifade dili bu programda yoktur. Bugün değere göre Kategorize Edilmiş, "
                   "sayıya göre Derecelendirilmiş simgeleyiciler var."));
            const Held quiet(loading_);
            renderKind_->setCurrentIndex(static_cast<int>(renderer_));
            return;
        }
        setRenderer(static_cast<Renderer>(index));
    });
    field(tr("Gösterim"), renderKind_); // ui-label: the renderer selector, no regulatory value

    renderValue_ = new ComboBox(bar);
    renderValue_->setMinimumWidth(190);
    renderValue_->setAccessibleName(tr("Sınıflandırma sütunu"));
    fillValueColumns();
    connect(renderValue_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (loading_ || renderer_ == Renderer::Single) return;
        // A new column means new classes; the old ones described another column.
        if (!categories_.isEmpty()) clearCategories();
    });

    // HIDDEN, not shown disabled with an em dash in it. A control that is present
    // but does nothing is worse than one that is absent: the reader spends the
    // look working out why it will not open, and the answer — "this renderer has
    // no value column" — is already said by the renderer beside it.
    valueCell_ = field(tr("Sütun"), renderValue_);
    valueCell_->setVisible(false);

    // The colours `Sınıflandır` hands the classes, derived from the symbol's own
    // colour rather than from a table of literals: distinct hues around the
    // wheel, one hue from dark to light, or greys.
    ramp_ = new ComboBox(bar);
    ramp_->addItem(tr("Ayrık renkler"));
    ramp_->addItem(tr("Tek renk açılımı"));
    ramp_->addItem(tr("Gri tonlar"));
    // A ramp is a picture before it is a name: eight steps of each, drawn into
    // the item, so the list is read the way the reference draws it.
    ramp_->setIconSize(QSize(56, 12));
    for (int k = 0; k < ramp_->count(); ++k) {
        QPixmap strip(56, 12);
        strip.fill(Qt::transparent);
        QPainter sp(&strip);
        for (int i = 0; i < 8; ++i)
            sp.fillRect(i * 7, 0, 7, 12, rampColour(k, i, 8, palette().color(QPalette::Highlight)));
        sp.end();
        ramp_->setItemIcon(k, QIcon(strip));
    }
    ramp_->setMinimumWidth(160);
    ramp_->setAccessibleName(tr("Renk skalası"));
    rampCell_ = field(tr("Renk dağılımı"), ramp_);
    rampCell_->setVisible(false);

    // The unit as ONE segmented control rather than a combo: three choices a
    // user switches between constantly read faster side by side than in a list,
    // and the reference draws them that way. One control, not three checkable
    // buttons — the component keeps them to one lit option, clears them all when
    // the layers disagree, and rounds only its outer corners.
    // AND THE GEOMETRY, at the end of the same strip.
    //
    // SHOWN ONLY WHEN THE LAYER DOES NOT SAY. A parcel layer is areas and a road
    // axis layer is lines; asking which of the three to draw is a question the
    // drawing has already answered, and §8 has no such control. An empty layer,
    // or one holding lines and areas both, still gets asked.
    geometryCell_ = field(tr("Geometri"), geometry_);
    geometryCell_->setVisible(geometryAsked_);

    // AND NOTHING PUSHES THEM APART.
    //
    // A `addStretch` used to sit between the renderer and the unit, "so the unit
    // sits at the right end where the eye lands last". With all four controls
    // showing that reads as §8's strip; with a single-symbol renderer, where
    // DEĞER and RENK SKALASI are both hidden, it left two controls at opposite
    // ends of eleven hundred pixels with a canyon between them — one band that
    // looks like two, which is what a reader reports as a broken window. §8 says
    // four controls in a strip, and a strip is what they are.

    return bar;
}

QWidget* StyleDesigner::buildInfoPage()
{
    // Read from the document, never stored: a panel that cached would disagree
    // with a layer renamed from the command line while this window was open.
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(6);

    auto* caption = new QLabel(tr("KATMAN"), page);
    caption->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(caption);

    const core::Document& doc = controller_.document();
    const core::LayerId id    = doc.find_layer(layerName_.toStdString());
    const core::Layer* layer  = doc.layer(id);

    const auto row = [&](const QString& key, const QString& value) {
        auto* line = new QWidget(page);
        auto* box  = new QHBoxLayout(line);
        box->setContentsMargins(0, 5, 0, 5);

        auto* name = new QLabel(key, line);
        name->setObjectName(QStringLiteral("rowName"));
        name->setMinimumWidth(200);

        auto* shown = new QLabel(value, line);
        shown->setObjectName(QStringLiteral("mono"));

        box->addWidget(name);
        box->addWidget(shown, 1);
        layout->addWidget(line);
    };

    row(tr("ad"), layerName_);
    row(tr("nesne"), layer ? QString::number(doc.layer_entity_count(id)) : QStringLiteral("—"));
    row(tr("gorunur"), layer ? (layer->visible ? tr("evet") : tr("hayır")) : QStringLiteral("—"));
    row(tr("kilitli"), layer ? (layer->locked ? tr("evet") : tr("hayır")) : QStringLiteral("—"));
    row(tr("grup"), layer && !layer->group.empty() ? QString::fromStdString(layer->group)
                                                   : QStringLiteral("—"));
    row(tr("koordinat_sistemi"), QString::fromStdString(doc.crs().id()));
    row(tr("sembol_katmani"), QString::number(symbol_.layers.size()));

    layout->addStretch(1);
    return page;
}

QWidget* StyleDesigner::buildPendingPage(const QString& phase, const QString& note)
{
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);

    auto* badge = new QLabel(phase, page);
    badge->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(badge);

    auto* words = new QLabel(note, page);
    words->setObjectName(QStringLiteral("quiet"));
    words->setWordWrap(true);
    words->setMaximumWidth(520);
    layout->addWidget(words);
    layout->addStretch(1);
    return page;
}

QWidget* StyleDesigner::buildGallery()
{
    // A CAPTION OVER A LIST, NOT A BOXED GROUP — the same lesson the symbol
    // layer stack learned below, and the last box in this window to learn it. A
    // `QGroupBox` spends a 1 px border and 20 px of padding on saying where its
    // contents begin, and puts its title where the first row should be; the
    // caption says the same thing in ten pixels and leaves the rest to the list.
    auto* box    = new QWidget(this);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* galleryCaption = new QLabel(tr("HAZIR GÖSTERİMLER"), box); // ui-label
    galleryCaption->setObjectName(QStringLiteral("groupCaption"));
    layout->addWidget(galleryCaption);

    // THE DRAWER IS A LIST BESIDE THE SEARCH, not a tree band above a grid of
    // tiles. The tree took a third of the column to show five annex names, and
    // the tiles under it wrapped every published name into three or four lines
    // of capitals — a wall nobody could scan. The same shelf as rows: one
    // gösterim per 30 px line, its real swatch at the left and its whole name
    // beside it, in the table language every other window here speaks (§9).
    groups_ = new QTreeWidget(box);
    groups_->setObjectName(QStringLiteral("styleLibraryTree"));
    groups_->setHeaderHidden(true);
    groups_->setMinimumWidth(200);
    groups_->setMaximumWidth(300);
    groups_->setAccessibleName(tr("Gösterim grubu")); // ui-label: a group selector
    connect(groups_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem*, QTreeWidgetItem*) { refreshGalleryItems(); });

    search_ = new QLineEdit(box);
    search_->setObjectName(QStringLiteral("designerSearch"));
    search_->setPlaceholderText(tr("Ara — etiket, kimlik ve grup yolu"));
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this,
            [this](const QString&) { refreshGalleryItems(); });

    gallery_ = new QListWidget(box);
    gallery_->setObjectName(QStringLiteral("designerGallery"));
    gallery_->setViewMode(QListView::ListMode);
    gallery_->setIconSize(QSize(44, 26));
    // NEITHER UNIFORM NOR STRIPED. A heading row is taller than a gösterim
    // row, and the groups do the banding that the stripes used to do — both at
    // once is two grids over one list.
    gallery_->setUniformItemSizes(false);
    gallery_->setAlternatingRowColors(false);
    gallery_->setWordWrap(false);
    gallery_->setSpacing(0);
    gallery_->setMouseTracking(true);
    gallery_->setAccessibleName(tr("Hazır gösterim listesi")); // ui-label
    shelfRows_ = new ShelfRow(gallery_);
    gallery_->setItemDelegate(shelfRows_);
    connect(gallery_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { applyGalleryPick(); });

    galleryNote_ = new QLabel(box);
    galleryNote_->setWordWrap(true);
    // PIXELS, not points. The application font is set with `setPixelSize`, so
    // `pointSizeF()` returns -1 on it and `-1 - 1` asked Qt for a font of -2 pt —
    // which it refuses with a warning and then draws at some size nobody chose.
    // design.md §3 is written in pixels throughout; this follows it.
    QFont small = galleryNote_->font();
    small.setPixelSize(11);
    galleryNote_->setFont(small);

    provenance_ = new QLabel(box);
    provenance_->setWordWrap(true);
    provenance_->setFont(small);
    provenance_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // NO RESERVED BAND. It used to hold 46 px open whether or not anything had
    // been picked, and with nothing selected that was a strip of blank between the
    // thumbnails and the button under them — the gap a reader takes for a layout
    // fault. It takes the height of its own text now, and hides when it has none.
    provenance_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    provenance_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    connect(gallery_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem*, QListWidgetItem*) { showProvenance(); });

    // Secondary, though it is the page's main action: the window's one primary
    // is `Tamam` in the footer, and the standard allows a screen exactly one.
    // NO GLYPH. A tick means "done" and nothing here is done yet — it is the
    // sign a reader meets on the page AFTER pressing something. The words say
    // what the press does, the strip beside them says which row it will do it
    // to, and a third mark on the same line is one thing too many.
    use_ = new Button(ButtonRole::Secondary, tr("Seçileni kullan"), std::nullopt, box);
    use_->setToolTip(tr("Seçili gösterimi düzenlenebilir sembol yığını olarak alır")); // ui-label
    connect(use_, &QPushButton::clicked, this, &StyleDesigner::applyGalleryPick);

    // The search goes ABOVE the tree it filters. Between the tree and the
    // thumbnails it read as belonging to neither, and when the box was starved
    // for height it was the row that overlapped its neighbours.
    auto* filterRow = new QHBoxLayout;
    filterRow->setContentsMargins(0, 0, 0, 0);
    filterRow->setSpacing(8);
    filterRow->addWidget(search_, 1);
    // HOW MANY THE FILTERS LEFT, beside the filters. It had a band of its own
    // under the list, which is as far from the two controls that change it as
    // this column goes.
    filterRow->addSpacing(4);
    filterRow->addWidget(galleryNote_);
    layout->addLayout(filterRow);
    auto* shelf = new QHBoxLayout;
    shelf->setSpacing(20);
    shelf->addWidget(groups_, 1);
    shelf->addWidget(gallery_, 3);
    layout->addLayout(shelf, 1);

    // RIGHT, AND ITS OWN WIDTH. A button stretched across seven hundred pixels
    // reads as a banner rather than as something to press, and it is the one
    // action in this half of the window — §15.1's single primary.
    // THE ACTION SITS WITH WHAT IT ACTS ON.
    //
    // It was alone in the bottom-right corner of a column eleven hundred pixels
    // wide, with a short count at the far left of the same line and nothing in
    // between — the same canyon the top strip had, and a press a user has to
    // travel the width of the window to make. It is at the LEFT of the foot
    // now, under the swatches, where the eye already is when a row is picked,
    // and the row's citation runs beside it: the button and the sentence
    // saying what pressing it will apply are one line.
    //
    // DISABLED UNTIL SOMETHING IS PICKED. It used to be lit at all times and to
    // do nothing when pressed with an empty selection.
    // ---- WHAT IS PICKED, AND THE BUTTON THAT TAKES IT ----------------------
    //
    // ONE STRIP ATTACHED TO THE LIST, not two things floating under it.
    //
    // The button has now been in three places and the first two were both
    // wrong for the same reason: it was ALONE. In the bottom-right corner it
    // sat a thousand pixels from the count at the far left with nothing
    // between them; moved to the bottom-left it hung past the list's own edge,
    // centred against a two-line citation, and stayed lit and useless with
    // nothing selected.
    //
    // What it needed was not a better corner but something to belong to. The
    // citation of the picked row and the button that applies that row are one
    // object: the strip is flush against the foot of the list, it carries the
    // regulation, annex, madde and date at its left and the button at its
    // right, and it is not there at all until a row is picked. Nothing floats,
    // nothing is greyed out waiting, and the sentence saying what will be
    // applied sits on the same line as the control that applies it.
    use_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    pickFoot_    = new QWidget(box);
    auto* useRow = new QHBoxLayout(pickFoot_);
    useRow->setContentsMargins(0, 8, 0, 0);
    useRow->setSpacing(16);
    useRow->addWidget(provenance_, 1);
    useRow->addWidget(use_, 0, Qt::AlignVCenter);
    pickFoot_->setVisible(false);
    layout->addWidget(pickFoot_);
    return box;
}

void StyleDesigner::refreshGalleryTree()
{
    const core::StyleLibrary& library = controller_.bus().style_library();
    const QSignalBlocker quiet(groups_);
    groups_->clear();
    auto* all = new QTreeWidgetItem(groups_, QStringList{tr("Tümü")});
    all->setData(0, Qt::UserRole, QStringList{});
    const auto append = [&](auto&& self, QTreeWidgetItem* parent,
                            std::vector<std::string> path) -> void {
        for (const std::string& label : library.children(path)) {
            auto nested = path;
            nested.push_back(label);
            QStringList values;
            for (const std::string& part : nested)
                values << QString::fromStdString(part);
            auto* item =
                parent ? new QTreeWidgetItem(parent, QStringList{QString::fromStdString(label)})
                       : new QTreeWidgetItem(groups_, QStringList{QString::fromStdString(label)});
            item->setData(0, Qt::UserRole, values);
            self(self, item, std::move(nested));
        }
    };
    append(append, nullptr, {});
    for (int i = 0; i < groups_->topLevelItemCount(); ++i)
        groups_->topLevelItem(i)->setExpanded(true);
    groups_->setCurrentItem(all);
}

void StyleDesigner::fillTypeChoices(core::SymbolLayerType current)
{
    std::vector<SymbolLayerType> allowed = types_for(shape());
    if (std::find(allowed.begin(), allowed.end(), current) == allowed.end())
        allowed.insert(allowed.begin(), current);

    // Nothing to do when the box already offers exactly this list: rebuilding it
    // would fire `currentIndexChanged` and write the symbol back on every refresh.
    bool same = type_->count() == static_cast<int>(allowed.size());
    for (int i = 0; same && i < type_->count(); ++i)
        same = type_->itemData(i).toInt() == static_cast<int>(allowed[static_cast<std::size_t>(i)]);

    if (!same) {
        const QSignalBlocker quiet(type_);
        type_->clear();
        for (const SymbolLayerType t : allowed) {
            QString label = QString::fromUtf8(core::symbol_layer_type_name(t));
            for (const TypeRow& row : kTypes)
                if (row.type == t) label = tr(row.label);
            if (t == SymbolLayerType::SimpleLine && shape() == PreviewShape::Area)
                label = tr("Kenarlık");
            type_->addItem(label, static_cast<int>(t));
        }
    }

    const QSignalBlocker quiet(type_);
    for (int i = 0; i < type_->count(); ++i)
        if (type_->itemData(i).toInt() == static_cast<int>(current)) type_->setCurrentIndex(i);
}

void StyleDesigner::refreshGalleryItems()
{
    const core::StyleLibrary& shelf = controller_.bus().style_library();
    // INK ON PAPER, IN BOTH THEMES.
    //
    // The swatches used to take the window's own input colour as their ground,
    // which in the dark theme is #171B1E — and a published gösterim is, far more
    // often than not, a black line. A shelf of black ink on a near-black ground
    // is a shelf a user in the dark theme cannot read, and the eleven rows of
    // `SINIRLAR` are all exactly that.
    //
    // A gösterim is not a piece of this window's chrome. It is what will be
    // printed on a sheet somebody signs, and that sheet is white: the swatch is
    // drawn on paper here for the same reason the sheet in the layout designer
    // is white in both themes.
    const std::uint32_t paper   = 0xFFFFFFFFu;
    const core::SymbolKind kind = kind_of(shape());

    // The SHELF's pictures, not the document's: a gösterim on the shelf has not
    // been applied to anything yet, so its hatch lives in the library's own store.
    const core::ImageStore& images = shelf.images();

    gallery_->clear();

    // An empty shelf hides its furniture. A tree, a search box and a grid with
    // nothing in any of them read as three things that are broken; one sentence
    // reads as one thing that has not been loaded yet.
    const bool stocked = !shelf.empty();
    groups_->setVisible(stocked);
    search_->setVisible(stocked);
    gallery_->setVisible(stocked);
    // THE STRIP IS ONE THING AND IT IS SHOWN IN ONE PLACE (`showProvenance`).
    // The citation used to be hidden here too, whenever the shelf was refilled
    // while its text happened to be empty — and nothing put it back when the
    // selection returned, so the strip came up holding a button and no
    // sentence, with the button drifting to the middle of the line where the
    // sentence should have been.
    if (pickFoot_ != nullptr && !stocked) pickFoot_->setVisible(false);
    galleryNote_->setAlignment(stocked ? Qt::AlignLeft | Qt::AlignTop : Qt::AlignCenter);

    if (!stocked) {
        // The annotation rides the line it describes — the gate reads the offending
        // line, and a claim on the line above it is a claim about something else.
        const QString empty = tr("Raf boş.\n\n"
                                 "BÖHHBÜY ve MPYY gösterimlerini rafa almak için " // ui-label
                                 "komut satırına\nSEMBOL paket=<yol>\nyazın.");
        galleryNote_->setText(empty);
        return;
    }

    // Search wins over the tree: somebody typing a word wants it found wherever it
    // is, which is why the box does not narrow to the open drawer.
    std::vector<const core::LibraryEntry*> rows;
    const QString needle = search_->text().trimmed();

    if (!needle.isEmpty()) {
        rows = shelf.search(needle.toStdString());
    } else {
        const QStringList path = groups_->currentItem()
                                     ? groups_->currentItem()->data(0, Qt::UserRole).toStringList()
                                     : QStringList{};
        if (path.isEmpty()) {
            rows = shelf.of_kind(kind);
        } else {
            std::vector<std::string> where;
            for (const QString& part : path)
                where.push_back(part.toStdString());

            for (const core::LibraryEntry& entry : shelf.entries()) {
                if (entry.group.size() >= where.size() &&
                    std::equal(where.begin(), where.end(), entry.group.begin()))
                    rows.push_back(&entry);
            }
        }
    }

    // Filtered by GEOMETRY, always: the tabs are the first decision, and a drawer
    // opened under `Çizgi` must not hand back an area gösterim.
    std::vector<const core::LibraryEntry*> matching;
    for (const core::LibraryEntry* e : rows)
        if (e->kind == kind) matching.push_back(e);

    const int shown = std::min(static_cast<int>(matching.size()), kGalleryCap);

    // ---- THE SHELF IS GROUPED, NOT A HUNDRED AND THIRTEEN FLAT LINES --------
    //
    // WHAT THIS REPLACES, AND WHY IT IS THE SECOND ANSWER TO ONE QUESTION.
    // The right end of every row was blank — four hundred pixels of nothing on
    // every line — so the row was given the annex path it came from. It read
    // correctly and it was useless: consecutive rows share a group, so the
    // column repeated `EK-1a / SINIRLAR / İDARİ SINIRLAR` five times running
    // and put a second wall of capitals beside the first.
    //
    // A fact that repeats down a run of rows is the run's, not the row's. It is
    // a heading now: said once, where the run starts, and a reader scrolling a
    // hundred and thirteen published gösterim has something to count by. This
    // is what the annex itself looks like.
    QString openGroup;
    for (int i = 0; i < shown; ++i) {
        const core::LibraryEntry* e = matching[at(i)];

        QStringList where;
        for (const std::string& part : e->group)
            where << QString::fromStdString(part);
        if (const QString path = where.join(QStringLiteral(" / ")); path != openGroup) {
            openGroup  = path;
            auto* head = new QListWidgetItem(gallery_);
            head->setData(ShelfRow::kGroupRole, path.isEmpty() ? tr("Grupsuz") : path);
            // NOT SELECTABLE AND NOT A TAB STOP: a heading is not a gösterim,
            // and arrowing through the shelf must not stop on one.
            head->setFlags(Qt::NoItemFlags);
        }

        auto* item = new QListWidgetItem(gallery_);
        item->setText(QString::fromStdString(e->label.empty() ? e->id : e->label));
        item->setIcon(symbol_icon(e->symbol, images, shelf.dashes(), QSize(44, 26), paper,
                                  shape_of(e->kind)));
        item->setData(Qt::UserRole, QString::fromStdString(e->id));
        // WHERE THE ROW COMES FROM, AT THE RIGHT END OF IT.
        //
        // The longest published name here reaches about two thirds of the way
        // across a row and the rest was blank — four hundred pixels of nothing
        // on every one of a hundred and thirteen lines.
        //
        // NOT THE ID. The id is the name slugified — `ÜLKE SINIRI` is
        // `ortak-ulke-siniri` — so a column of ids is the same column of names
        // written twice. The GROUP is the thing a reader cannot get from the
        // name and had to select a row to see: which annex and which section of
        // it this gösterim was printed in. Two rows can carry the same name in
        // two annexes and mean different things.
        //
        // The full citation — regulation, annex, madde, date — stays under the
        // list for the selected row, verbatim and unshortened (CLAUDE.md 11.7).
        // WITHDRAWN ROWS ONLY. `uncertain` marks fourteen rows whose appearance
        // the package could not read with confidence, and that is a warning
        // about the DRAWING, which the note under the list gives in full once a
        // row is picked. A withdrawn gösterim is different in kind: it must not
        // be chosen at all for a new sheet, so it is marked where it is scanned.
        item->setData(ShelfRow::kRetiredRole, e->deprecated);
        // The name again, with the id and the citation: a row elides a very long
        // published name, and the id is what a script writes.
        item->setToolTip(QString::fromStdString((e->label.empty() ? e->id : e->label) + "\n" +
                                                e->id + "\n" + e->source_ref));
    }

    if (matching.empty())
        // AN EMPTY SHELF IS A PLACE TO ACT, not a statement of absence: the two
        // things that emptied it are both one click away and both named.
        galleryNote_->setText(
            search_->text().trimmed().isEmpty()
                ? tr("Bu geometride gösterim yok. Üstteki GEOMETRİ'yi değiştirin.") // ui-label
                : tr("Aramayla eşleşen gösterim yok. Aramayı temizleyin ya da "     // ui-label
                     "grubu Tümü yapın."));
    else if (shown < static_cast<int>(matching.size()))
        galleryNote_->setText(tr("%1 gösterimden ilk %2 tanesi. Aramayı daraltın.") // ui-label
                                  .arg(matching.size())
                                  .arg(shown));
    else
        galleryNote_->setText(tr("%1 gösterim.").arg(matching.size())); // ui-label
}

void StyleDesigner::showProvenance()
{
    // THE WHOLE STRIP APPEARS AND GOES WITH THE PICK. The citation and the
    // button are one thing; a button offering to apply nothing is a control
    // that lies about what it does.
    const auto nothingPicked = [this] {
        provenance_->clear();
        if (pickFoot_ != nullptr) pickFoot_->setVisible(false);
    };

    QListWidgetItem* item = gallery_->currentItem();
    if (item == nullptr) {
        nothingPicked();
        return;
    }

    const core::LibraryEntry* e =
        controller_.bus().style_library().find(item->data(Qt::UserRole).toString().toStdString());
    if (e == nullptr) {
        nothingPicked();
        return;
    }

    // The citation, verbatim from the package. Not paraphrased and not shortened:
    // a regulatory statement carries its regulation, annex, madde and publication
    // date, and a shortened one is a different statement (CLAUDE.md 11.7).
    QString text = QString::fromStdString(e->source_ref);

    // Said where the choice is made, not in a log. A row the package could not
    // read with confidence must not look like one it could.
    if (e->uncertain) {
        QStringList why;
        for (const std::string& reason : e->uncertain_reasons)
            why << QString::fromStdString(reason);
        text = tr("⚠ Bu satırın görünümü pakette kesin değil (%1).\n%2")
                   .arg(why.join(QStringLiteral(", ")), text);
    }
    if (e->deprecated) text = tr("⚠ Yürürlükten kalkmış.\n") + text;

    provenance_->setText(text);
    provenance_->setToolTip(QString::fromStdString(e->id));
    if (pickFoot_ != nullptr) pickFoot_->setVisible(true);
}

void StyleDesigner::resetToLayer()
{
    symbol_ = original_;
    single_ = original_;
    galleryCode_.clear();
    galleryPackage_.clear();
    if (renderer_ != Renderer::Single) {
        categories_.clear();
        editingCategory_ = -1;
        setRenderer(Renderer::Single);
    }
    geometry_->setCurrent(static_cast<int>(natural_shape(symbol_)));
    refresh();
    selectTopLayer();
}

void StyleDesigner::applyGalleryPick()
{
    QListWidgetItem* item = gallery_->currentItem();
    if (item == nullptr) return;

    const core::LibraryEntry* e =
        controller_.bus().style_library().find(item->data(Qt::UserRole).toString().toStdString());
    if (e == nullptr || e->symbol.layers.empty()) return;

    // REPLACES the stack rather than merging into it. Picking a published gösterim
    // means "this is what it should look like", and layering it over whatever was
    // there would produce a symbol neither the user nor the regulation asked for.
    symbol_                           = e->symbol;
    const core::StyleLibrary& library = controller_.bus().style_library();
    for (core::SymbolLayer& layer : symbol_.layers) {
        if (layer.image != core::kNoImage) {
            const auto image = draftImages_.intern(library.images().bytes(layer.image),
                                                   library.images().origin(layer.image));
            if (!image) {
                QMessageBox::warning(this, tr("Stil yüklenemedi"),
                                     QString::fromStdString(image.error().message));
                return;
            }
            layer.image = image.value();
        }
        if (layer.look.dash != core::kSolidDash) {
            const auto dash = draftDashes_.intern(library.dashes().at(layer.look.dash),
                                                  library.dashes().origin(layer.look.dash));
            if (!dash) return;
            layer.look.dash = dash.value();
        }
    }
    galleryCode_    = QString::fromStdString(e->id);
    galleryPackage_ = QString::fromStdString(e->package_path);
    geometry_->setCurrent(static_cast<int>(shape_of(e->kind)));
    refresh();
    selectTopLayer();
    libraryDialog_->accept();
}

// ------------------------------------------------------------- the stack ----

QWidget* StyleDesigner::buildTree()
{
    // A caption over a list, not a boxed group: the box spent 20 px of a 200 px
    // column on its own border and put its title where the first row should be.
    auto* box    = new QWidget(this);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* caption = new QLabel(tr("Sembol katmanları"), box); // ui-label
    caption->setObjectName(QStringLiteral("groupCaption"));
    layout->addWidget(caption);

    // The symbol root makes whole-stack settings reachable from the keyboard.
    tree_ = new QTreeWidget(box);
    tree_->setObjectName(QStringLiteral("designerList"));
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(2);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    tree_->setColumnWidth(1, 36);
    tree_->setIconSize(QSize(36, 24));
    tree_->setItemDelegateForColumn(1, new LayerVisibilityDelegate(tree_));
    tree_->setRootIsDecorated(false);
    tree_->setIndentation(12);
    tree_->setAlternatingRowColors(false);
    tree_->setAccessibleName(tr("Sembol katmanları"));
    // What the order means, where the eye already is. It used to be the box's
    // title, which is gone; a stack read the wrong way round is the one thing a
    // newcomer to this window gets wrong.
    tree_->setToolTip(tr("Üstteki katman en son çizilir — ekranda en üstte görünür"));

    // A WHOLE NUMBER OF ROWS, AND ONLY THE ROWS IT HAS. Left to grow, the tree
    // took the column's height and the property form under it took none — so it
    // was pinned at five, which is more than a published gösterim carries and
    // left a third of the box as empty ground under a two-layer symbol. It now
    // fits what is in it, floored at three so a one-layer symbol is not a slot
    // and capped at six so a deep stack scrolls rather than starving the form.
    fitStackHeight();

    connect(tree_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem*, QTreeWidgetItem*) { loadSelected(); });

    // THE EYE. A click on the second column switches the layer off or on; a
    // click anywhere else selects the row. The old check box did the same job
    // through `itemChanged`, which also fired while the rows were being built.
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        if (loading_ || item == nullptr || column != 1) return;

        // The stack index the row CARRIES, not the row it sits on.
        const QVariant carried = item->data(0, Qt::UserRole);
        if (!carried.isValid()) return;

        const int index = carried.toInt();
        if (index < 0 || index >= static_cast<int>(symbol_.layers.size())) return;

        galleryCode_.clear();
        galleryPackage_.clear();
        symbol_.layers[at(index)].enabled = !symbol_.layers[at(index)].enabled;

        // QUEUED, and this is a correctness fix rather than a nicety. Qt is still
        // inside the click delivery when this runs; `refresh()` clears the tree,
        // which deletes that very item, and the call Qt is in the middle of then
        // returns into freed memory.
        //
        // A slot that rebuilds the model it was notified about has to do it after
        // the notification has unwound. That is what a queued call is for.
        QMetaObject::invokeMethod(this, &StyleDesigner::refresh, Qt::QueuedConnection);
    });

    // §15.1's icon button: 24 px, ghost, a tooltip and no label. They were bare
    // `QToolButton`s carrying a glyph as their text, so they drew at whatever
    // width the glyph happened to be — five different sizes in one row.
    // Drawn marks, not `+` and `⧉` typed as text: a glyph typed into a button
    // sits at whatever width and baseline the font gives it, and five of them in
    // a row sat at five. The glyph rides in a property so the theme re-tints it.
    const auto button = [&](Glyph glyph, const QString& tip, auto slot) {
        auto* b = new QToolButton(box);
        b->setObjectName(QStringLiteral("rowTool"));
        b->setProperty("glyphed", true);
        b->setProperty("glyph", static_cast<int>(glyph));
        b->setIconSize(QSize(16, 16));
        b->setIcon(icon(glyph, palette().color(QPalette::WindowText),
                        palette().color(QPalette::Highlight), 16));
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setFixedSize(24, 24);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(2);
    bar->addWidget(button(Glyph::Plus, tr("Katman ekle"), &StyleDesigner::addLayer));
    bar->addWidget(button(Glyph::Duplicate, tr("Katmanı kopyala"), &StyleDesigner::duplicateLayer));
    bar->addWidget(button(Glyph::Minus, tr("Katmanı sil"), &StyleDesigner::removeLayer));
    bar->addStretch(1);
    bar->addWidget(button(Glyph::ChevronUp, tr("Yukarı"), [this] { moveLayer(+1); }));
    bar->addWidget(button(Glyph::ChevronDown, tr("Aşağı"), [this] { moveLayer(-1); }));

    // The two things a stack is used for most, on the keys a user already presses
    // for them elsewhere. Scoped to the TREE so they do not fire while a spin box
    // has focus and the user is deleting a digit.
    auto* remove = new QShortcut(QKeySequence::Delete, tree_);
    remove->setContext(Qt::WidgetShortcut);
    connect(remove, &QShortcut::activated, this, &StyleDesigner::removeLayer);

    auto* copy = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), tree_);
    copy->setContext(Qt::WidgetShortcut);
    connect(copy, &QShortcut::activated, this, &StyleDesigner::duplicateLayer);

    layout->addWidget(tree_, 1);
    layout->addLayout(bar);
    return box;
}

// ------------------------------------------------- the symbol as a whole ----

QWidget* StyleDesigner::buildGlobal()
{
    auto* box  = new QWidget(this);
    auto* form = new QVBoxLayout(box);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(10);
    addGroup(form, tr("Sembol")); // ui-label

    // Common-unit conversion is independent of opacity, colour and paper width.
    globalUnit_ = new ComboBox(box);
    fit_column(globalUnit_);
    globalUnit_->addItem(tr("Kâğıt (mm)"), // ui-label
                         static_cast<int>(core::Unit::Paper));
    globalUnit_->addItem(tr("Zemin (m)"), // ui-label
                         static_cast<int>(core::Unit::Ground));
    globalUnit_->addItem(tr("Piksel (px)"), // ui-label
                         static_cast<int>(core::Unit::Pixel));
    connect(globalUnit_, &QComboBox::currentIndexChanged, this, [this](int) {
        applyGlobalUnit(static_cast<core::Unit>(globalUnit_->currentData().toInt()));
    });

    globalUnitNote_ = new QLabel(box);
    globalUnitNote_->setObjectName(QStringLiteral("quiet"));
    globalUnitNote_->setWordWrap(true);

    globalColour_ = new QToolButton(box);
    globalColour_->setObjectName(QStringLiteral("colourField"));
    globalColour_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    globalColour_->setCursor(Qt::PointingHandCursor);
    globalColour_->installEventFilter(this); // repaint the face at the column's width
    globalColour_->setToolTip(tr("Kilitli olmayan bütün katmanların rengini birden değiştirir"));
    connect(globalColour_, &QToolButton::clicked, this, [this] {
        const QColor picked =
            QColorDialog::getColor(from_rgba(symbol_.primary().rgba), this, tr("Sembol rengi"),
                                   QColorDialog::ShowAlphaChannel);
        if (!picked.isValid()) return;

        galleryCode_.clear();
        galleryPackage_.clear();
        for (core::SymbolLayer& l : symbol_.layers) {
            if (l.colour_locked) continue; // a locked layer keeps its own
            l.look.rgba       = picked.rgba();
            l.look.src_colour = core::Source::Explicit;
        }
        refresh();
    });

    globalWidth_ = new MeasureSpinBox(box);
    globalWidth_->setRange(0, 100000);
    globalWidth_->setSingleStep(100);
    globalWidth_->setSuffix(tr(" mm"));
    connect(globalWidth_, &QSpinBox::valueChanged, this,
            [this](int) { applyGlobal(core::SymbolProperty::Width); });

    globalOpacity_ = new MeasureSpinBox(box);
    globalOpacity_->setRange(0, 255);
    globalOpacity_->setSingleStep(5);
    static_cast<MeasureSpinBox*>(globalOpacity_)->setDivisor(2.55);
    globalOpacity_->setSuffix(tr(" %"));
    connect(globalOpacity_, &QSpinBox::valueChanged, this,
            [this](int) { applyGlobal(core::SymbolProperty::Opacity); });

    form->addWidget(form_cell(box, tr("Ölçü birimi"), globalUnit_, nullptr, nullptr));
    form->addWidget(globalUnitNote_); // the note belongs to the unit above it
    form->addWidget(form_cell(box, tr("Renk"), globalColour_, nullptr, nullptr));
    form->addWidget(form_cell(box, tr("Kalınlık · kâğıt"), globalWidth_, nullptr, nullptr));
    form->addWidget(form_cell(box, tr("Opaklık"), globalOpacity_, nullptr, nullptr));
    form->addStretch(1);

    return box;
}

// -------------------------------------------------------- the properties ----

QLabel* StyleDesigner::addGroup(QVBoxLayout* form, const QString& title)
{
    auto* heading = new QLabel(title, this);
    heading->setObjectName(QStringLiteral("styleFormGroup"));
    // Air above a heading, none below: the rows under it are its, the rows
    // above are someone else's.
    heading->setContentsMargins(0, 8, 0, 0);
    if (title == tr("Dolgu") || title == tr("Çizgi") || title == tr("Ölçüler ve yerleşim"))
        heading->setProperty("compactGroup", true);
    form->addWidget(heading);
    return heading;
}

void StyleDesigner::addProperty(QVBoxLayout* form, QLabel* group, const QString& label,
                                QWidget* editor, QWidget* unit, std::vector<SymbolLayerType> types)
{
    QLabel* caption = nullptr;
    QWidget* cell   = form_cell(this, label, editor, unit, &caption);
    form->addWidget(cell);
    properties_.push_back(Property{caption, cell, unit, group, std::move(types)});
}

QWidget* StyleDesigner::buildProperties()
{
    // GROUPED, the way §8 groups them — DOLGU, KENAR, GEOMETRİ, GÖRÜNÜRLÜK — and
    // not one boxed list of fourteen rows. A flat list makes the reader work
    // out for themselves that "Aralık" and "Faz" are about the same thing and
    // "Saydamlık" is not; the headings say it. The first group is the layer's
    // identity and has no heading of its own to hide behind, so it is always
    // there.
    auto* box  = new QWidget(this);
    auto* form = new QVBoxLayout(box);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(10);

    type_ = new ComboBox(box);
    fit_column(type_);
    // FILLED PER GEOMETRY, in `fillTypeChoices`. The type rides as item DATA
    // rather than as a position, because a filtered list and a fixed table cannot
    // both be indexed by the same number.
    connect(type_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
    form->addWidget(form_cell(box, tr("Katman tipi"), type_, nullptr, nullptr));

    const auto unitCombo = [&](core::Measure core::SymbolLayer::* measure) {
        auto* c = new ComboBox(box);
        fit_column(c);
        for (const core::Unit u : kUnits)
            c->addItem(u == core::Unit::Paper    ? tr("Kâğıt mm")
                       : u == core::Unit::Ground ? tr("Zemin m")
                                                 : tr("Piksel px"));
        c->setToolTip(tr("Birim değiştirilirken önizlemedeki boyut korunur; piksel değerleri tam "
                         "sayıya yuvarlanır"));
        connect(c, &QComboBox::currentIndexChanged, this,
                [this, c, measure](int) { changeMeasureUnit(measure, pick(kUnits, c)); });
        return c;
    };

    const auto spin = [&](int max, int step) {
        auto* s = new MeasureSpinBox(box);
        s->setRange(0, max);
        s->setSingleStep(step);
        connect(s, &QSpinBox::valueChanged, this, [this](int) { applyToSelected(); });
        return s;
    };

    const auto namedCombo = [&](auto items, auto namer) {
        auto* c = new ComboBox(box);
        fit_column(c);
        for (const auto value : items)
            c->addItem(QString::fromUtf8(namer(value)));
        connect(c, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
        return c;
    };

    stroke_ = new QToolButton(box);
    stroke_->setObjectName(QStringLiteral("colourField"));
    stroke_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stroke_->setCursor(Qt::PointingHandCursor);
    stroke_->installEventFilter(this); // repaint the face at the column's width
    stroke_->setToolTip(tr("Çizgi ve simge rengi"));
    connect(stroke_, &QToolButton::clicked, this, [this] {
        const int i = currentLayer();
        if (i < 0) return;
        const QColor picked =
            QColorDialog::getColor(from_rgba(symbol_.layers[at(i)].look.rgba), this,
                                   tr("Çizgi rengi"), QColorDialog::ShowAlphaChannel);
        if (!picked.isValid()) return;
        symbol_.layers[at(i)].look.rgba       = picked.rgba();
        symbol_.layers[at(i)].look.src_colour = core::Source::Explicit;
        refresh();
    });

    fill_ = new QToolButton(box);
    fill_->setObjectName(QStringLiteral("colourField"));
    fill_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fill_->setCursor(Qt::PointingHandCursor);
    fill_->installEventFilter(this); // repaint the face at the column's width
    fill_->setToolTip(tr("Dolgu rengi — iptal edilirse dolgusuz"));
    connect(fill_, &QToolButton::clicked, this, [this] {
        const int i = currentLayer();
        if (i < 0) return;
        const QColor picked =
            QColorDialog::getColor(from_rgba(symbol_.layers[at(i)].look.fill_rgba), this,
                                   tr("Dolgu rengi"), QColorDialog::ShowAlphaChannel);
        symbol_.layers[at(i)].look.fill_rgba = picked.isValid() ? picked.rgba() : 0u;
        symbol_.layers[at(i)].look.src_fill  = core::Source::Explicit;
        refresh();
    });

    width_ = spin(100000, 100);

    // WHAT ZERO MEANS, said in the field rather than left to be guessed — and
    // ONLY in this field. A width of zero is a hairline: the thinnest line the
    // output can draw, one pixel on screen and one device dot on paper, and it is
    // what a cadastral boundary is drawn with. A reader who sees a bare `0`
    // reasonably concludes the layer draws nothing.
    //
    // Zero means something different in every other spin box here — no offset, no
    // phase, no rotation — so saying "kıl çizgi" in all of them, which the first
    // attempt did, put the word `Kaydırma: 0 — kıl çizgi` on screen.
    width_->setSpecialValueText(tr("0 — kıl çizgi"));
    // The unit in the field, not in the caption (§15.2): "Kalınlık" fits the
    // caption column and "Çizgi kalınlığı (µm)" did not.
    width_->setSuffix(tr(" mm"));
    size_     = spin(std::numeric_limits<int>::max(), 100);
    interval_ = spin(std::numeric_limits<int>::max(), 100);
    spacingY_ = spin(std::numeric_limits<int>::max(), 100);
    offset_   = spin(std::numeric_limits<int>::max(), 100);
    offset_->setMinimum(-std::numeric_limits<int>::max());
    phase_ = spin(std::numeric_limits<int>::max(), 100);
    phase_->setMinimum(-std::numeric_limits<int>::max());
    angle_ = spin(360000000, 5000000);
    angle_->setMinimum(-360000000);
    opacity_ = spin(255, 5);
    static_cast<MeasureSpinBox*>(angle_)->setDivisor(1000000);
    static_cast<MeasureSpinBox*>(opacity_)->setDivisor(2.55);
    opacity_->setSuffix(tr(" %"));
    angle_->setSuffix(tr("°"));

    size_->setSpecialValueText(tr("Otomatik"));
    interval_->setSpecialValueText(tr("Otomatik"));
    spacingY_->setSpecialValueText(tr("Aralık ile aynı"));
    text_              = new QLineEdit(box);
    const QString hint = tr("Sembolün kendi yazısı, örnek: TAKS"); // ui-label
    text_->setPlaceholderText(hint);
    connect(text_, &QLineEdit::textEdited, this, [this](const QString&) { applyToSelected(); });

    sizeUnit_     = unitCombo(&core::SymbolLayer::size);
    intervalUnit_ = unitCombo(&core::SymbolLayer::interval);
    spacingYUnit_ = unitCombo(&core::SymbolLayer::spacing_y);
    offsetUnit_   = unitCombo(&core::SymbolLayer::offset);
    phaseUnit_    = unitCombo(&core::SymbolLayer::phase);

    shape_     = namedCombo(kShapes, core::marker_shape_name);
    placement_ = namedCombo(kPlacements, core::marker_placement_name);
    cap_       = namedCombo(kCaps, [](core::LineCap c) {
        switch (c) {
        case core::LineCap::Butt: return "duz";
        case core::LineCap::Round: return "yuvarlak";
        case core::LineCap::Square: return "kare";
        }
        return "?";
    });
    join_      = namedCombo(kJoins, [](core::LineJoin j) {
        switch (j) {
        case core::LineJoin::Miter: return "kose";
        case core::LineJoin::Round: return "yuvarlak";
        case core::LineJoin::Bevel: return "pah";
        }
        return "?";
    });

    // The visibility table. Each row names the types that READ it; a type not
    // listed does not show the row at all rather than showing it greyed, because a
    // `dolgu` layer has no marker placement and a dialog that offers one is
    // offering something the renderer will ignore.
    using T = SymbolLayerType;

    const std::vector<T> strokes{T::SimpleLine,      T::MarkerLine,       T::HashLine,
                                 T::LinePatternFill, T::PointPatternFill, T::CentroidFill,
                                 T::SimpleMarker};
    const std::vector<T> fills{T::SimpleFill, T::PointPatternFill, T::SimpleMarker};
    const std::vector<T> markers{T::MarkerLine, T::PointPatternFill, T::CentroidFill,
                                 T::SimpleMarker};
    const std::vector<T> sized{T::MarkerLine,   T::HashLine,     T::PointPatternFill,
                               T::CentroidFill, T::SimpleMarker, T::RasterFill,
                               T::RasterMarker, T::RasterLine,   T::TextMarker};
    const std::vector<T> spaced{T::MarkerLine,       T::HashLine,   T::LinePatternFill,
                                T::PointPatternFill, T::RasterLine, T::RasterFill};

    // READ OFF THE BACKEND, not guessed. Every type below is one whose draw path
    // actually reads `angle_udeg`: the two marker walks rotate the glyph by it,
    // the pattern fills rotate the pattern, and `drawRasterFill` rotates its
    // brush. `gorsel-dolgu` was missing from this list and its rotation was
    // therefore unreachable from the dialog — a property the renderer reads and
    // nobody can set is worse than one that does not exist, because the drawing
    // can hold a value the user cannot see or change.
    const std::vector<T> angled{T::MarkerLine,       T::HashLine,     T::LinePatternFill,
                                T::PointPatternFill, T::SimpleMarker, T::RasterFill,
                                T::RasterLine,       T::RasterMarker, T::CentroidFill};

    // The phase is read by both marker walks — the vector one and the stamped
    // one — so every type that places something ALONG a line offers it.
    const std::vector<T> phased{T::MarkerLine, T::HashLine, T::RasterLine};

    // ONE row for the colour button, listing every type that reads it. Declaring
    // it twice under two labels put the same widget in two cells of one
    // `QFormLayout` — undefined in Qt — and left the two rows disagreeing about
    // whether it was visible, so a `yazi-isaretci` layer showed no colour at all.
    // `loadSelected()` renames the row instead.
    std::vector<T> coloured = strokes;
    coloured.push_back(T::TextMarker);
    coloured.push_back(T::RasterFill);
    coloured.push_back(T::RasterLine);
    coloured.push_back(T::RasterMarker);

    // The lock, on every layer, because every layer can be the one the regulation
    // fixes. Listed against `everything` below so it never disappears.
    lock_ = new CheckBox(tr("Katman rengini koru"), box);
    lock_->setToolTip(tr("MPYY bir lekesinin dolgusunu plancıya bırakır, sınırını ve " // ui-label
                         "glifini siyah basar. Kilitli bir katman, sembolün rengi "
                         "değiştiğinde kendi rengini korur."));
    connect(lock_, &CheckBox::toggled, this, [this](bool on) {
        if (loading_) return;
        const int i = currentLayer();
        if (i < 0) return;
        galleryCode_.clear();
        galleryPackage_.clear();
        symbol_.layers[at(i)].colour_locked = on;
        refresh();
    });

    // Every declared type, for the rows that are not about one of them. Built
    // once, here, because two of them want it and a second loop would be a second
    // answer to "which types are there".
    std::vector<T> everything;
    for (const TypeRow& row : kTypes)
        everything.push_back(row.type);

    // What the layer IS — under KATMAN with the type, no heading of their own.
    addProperty(form, nullptr, tr("Yazı"), text_, nullptr, {T::TextMarker});

    // NO PARAMETER ROW HERE, and its absence is deliberate. There was one: a
    // single line in `STİL alan=`'s own syntax, `sütun[:özellik[:tür]]`, comma
    // separated. Two things were wrong with it.
    //
    // It did not work. The row wrote `bindings` into the in-memory symbol and the
    // preview redrew, but `applyToDocument` emits one `STİL` per symbol layer and
    // never emitted `alan=` — so the parameters reached the preview and never
    // reached the document. A control that reports success and changes nothing is
    // worse than no control.
    //
    // And a colon-separated line is a programmer's answer to a plan-maker's
    // question. Which column, which property it drives, what type it is — that is
    // a small table with three choosers per row, and it belongs in this window
    // beside the layer's own attribute schema rather than as a text field the
    // user has to know a grammar for.
    //
    // The capability is untouched: `STİL katman=… alan="kod:yazi:metin"` still
    // declares them, `ETİKET` still reads the text ones, and a symbol that
    // carries bindings keeps them through this dialog. Only the row is gone,
    // pending the design that replaces it. `scripts/ci-gate-designer.sh` holds
    // the command side to that promise.

    addProperty(form, nullptr, tr("Şekil"), shape_, nullptr, markers);
    addProperty(form, nullptr, tr("Yerleşim"), placement_, nullptr,
                {T::MarkerLine, T::HashLine, T::RasterLine});

    QLabel* fillGroup = addGroup(form, tr("Dolgu")); // ui-label
    addProperty(form, fillGroup, tr("Dolgu rengi"), fill_, nullptr, fills);

    QLabel* edgeGroup = addGroup(form, tr("Çizgi")); // ui-label
    addProperty(form, edgeGroup, tr("Çizgi rengi"), stroke_, nullptr, coloured);
    strokeLabel_ = properties_.back().label;
    addProperty(form, edgeGroup, tr("Kalınlık · kâğıt"), width_, nullptr, strokes);
    addProperty(form, edgeGroup, tr("Uç biçimi"), cap_, nullptr, {T::SimpleLine});
    addProperty(form, edgeGroup, tr("Birleşim"), join_, nullptr, {T::SimpleLine});

    QLabel* geometryGroup = addGroup(form, tr("Ölçüler ve yerleşim")); // ui-label
    addProperty(form, geometryGroup, tr("Boyut"), size_, sizeUnit_, sized);
    addProperty(form, geometryGroup, tr("Aralık"), interval_, intervalUnit_, spaced);
    addProperty(form, geometryGroup, tr("İkinci eksen"), spacingY_, spacingYUnit_,
                {T::PointPatternFill});
    addProperty(form, geometryGroup, tr("Kaydırma"), offset_, offsetUnit_,
                {T::SimpleLine, T::MarkerLine, T::HashLine, T::RasterLine, T::TextMarker,
                 T::LinePatternFill});
    addProperty(form, geometryGroup, tr("Açı"), angle_, nullptr, angled);
    addProperty(form, geometryGroup, tr("Faz"), phase_, phaseUnit_, phased);

    // Opacity is read by every type, so it lists them all and is always shown.
    QLabel* visibilityGroup = addGroup(form, tr("Görünürlük")); // ui-label
    addProperty(form, visibilityGroup, tr("Opaklık"), opacity_, nullptr, everything);
    addProperty(form, visibilityGroup, QString{}, lock_, nullptr, everything);

    // THE THIRD COLUMN of §8's `110px | 1fr | 22px` row: a `{ }` at the end of
    // every property a column can drive. QGIS calls it the data-defined override;
    // the model calls it a binding (`SymbolLayer::bindings`) and `STİL alan=`
    // writes it. This is the row that used to be a colon-separated text field
    // and was removed for it; it is back as what it always was — a chooser.
    const auto attach = [this](QWidget* editor, core::SymbolProperty what) {
        for (const Property& p : properties_) {
            auto* column = p.editor->layout();
            if (column == nullptr || column->count() < 2) continue;
            auto* row = qobject_cast<QHBoxLayout*>(column->itemAt(1)->layout());
            if (row == nullptr || row->indexOf(editor) < 0) continue;
            auto* mark = new QToolButton(p.editor);
            mark->setObjectName(QStringLiteral("bindMark"));
            mark->setIconSize(QSize(16, 16));
            mark->setFixedSize(24, 30);
            mark->setCursor(Qt::PointingHandCursor);
            mark->setToolTip(tr("Bu özelliği bir sütundan al"));
            mark->setAccessibleName(tr("%1 — sütundan al").arg(p.label->text()));
            mark->setProperty("bound", false);
            connect(mark, &QToolButton::clicked, this,
                    [this, what, mark] { bindProperty(what, mark); });
            row->addWidget(mark);
            bindingMarks_.emplace_back(what, mark);
            return;
        }
    };
    attach(stroke_, core::SymbolProperty::Colour);
    attach(fill_, core::SymbolProperty::Fill);
    attach(width_, core::SymbolProperty::Width);
    attach(size_, core::SymbolProperty::Size);
    attach(angle_, core::SymbolProperty::Angle);
    attach(opacity_, core::SymbolProperty::Opacity);
    attach(text_, core::SymbolProperty::Text);
    refreshBindingMarks();
    const auto pair = [&](QWidget* first, QWidget* second) {
        QWidget* a = nullptr;
        QWidget* b = nullptr;
        for (const Property& property : properties_) {
            if (property.editor->isAncestorOf(first)) a = property.label->parentWidget();
            if (property.editor->isAncestorOf(second)) b = property.label->parentWidget();
        }
        if (!a || !b) return;
        const int position = form->indexOf(a);
        if (position < 0) return;
        form->removeWidget(a);
        form->removeWidget(b);
        auto* row = new QWidget(box);
        row->setProperty("stylePair", true);
        row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto* cells = new QHBoxLayout(row);
        cells->setContentsMargins(0, 0, 0, 0);
        cells->setSpacing(12);
        cells->addWidget(a, first == size_ ? 2 : 1);
        cells->addWidget(b, 1);
        form->insertWidget(position, row);
    };
    pair(cap_, join_);

    form->addStretch(1);

    return box;
}

// ------------------------------------------------------------- the model ----

int StyleDesigner::currentLayer() const
{
    // Through the item's stored index, NOT through the row. The tree is drawn top
    // first — the layer drawn last is the one seen on top — so a row and a stack
    // index run in opposite directions.
    const QTreeWidgetItem* item = tree_->currentItem();
    if (item == nullptr) return -1;

    const QVariant carried = item->data(0, Qt::UserRole);
    if (!carried.isValid()) return -1; // the root carries no index

    const int index = carried.toInt();
    return index >= 0 && index < static_cast<int>(symbol_.layers.size()) ? index : -1;
}

void StyleDesigner::selectTopLayer()
{
    if (tree_->topLevelItemCount() == 0) return;
    auto* root = tree_->topLevelItem(0);
    if (root->childCount() > 0) tree_->setCurrentItem(root->child(0));
}

bool StyleDesigner::rootSelected() const
{
    const QTreeWidgetItem* item = tree_->currentItem();
    // Nothing selected reads as the root too: the symbol is what this window is
    // about, and a properties pane showing nothing at all is a worse answer than
    // showing the thing being edited.
    return item == nullptr || !item->data(0, Qt::UserRole).isValid();
}

void StyleDesigner::refresh()
{
    // The class being edited is `symbol_` itself; its row in the table follows
    // every change so the swatch there is never a stroke behind the editor.
    if (renderer_ != Renderer::Single) storeEditedCategory();

    // Never from inside itself. Every editor in this dialog answers a change by
    // rebuilding the whole right-hand side, so one that is rewritten BY the
    // rebuild would otherwise ask for another one.
    if (refreshing_) return;
    const Held rebuilding(refreshing_);

    // A picked gallery row has not reached the document yet, so its image ids
    // belong to the shelf. After a document style is loaded or edited, ids belong
    // to the drawing as usual. The preview must follow that ownership boundary.
    const core::ImageStore& images = draftImages_;
    const core::DashStore& dashes  = draftDashes_;
    const std::uint32_t paper      = 0xFFFFFFFFu;

    // What was selected, as a STACK INDEX rather than a row, so it survives a
    // rebuild that reorders the tree. -1 means the root, which is a selection.
    const int keep       = currentLayer();
    const bool keep_root = rootSelected();

    {
        // The rows are built DETACHED and inserted whole, and the tree is silent
        // while that happens.
        //
        // Both halves are load bearing. A row added to the tree first and filled
        // afterwards emits `itemChanged` once per property, the first time before
        // its stack index has been written — so the handler read index 0, saw an
        // unset check box, and switched off symbol layer 0 of whatever symbol was
        // open. That is why an applied style silently came back as "katman
        // varsayılanına döndü". The handler then called `refresh()`, whose
        // `clear()` freed the very row this loop was still filling: a
        // use-after-free that ASan reports at the next `setToolTip`.
        const Held quiet(loading_);
        const QSignalBlocker silent(tree_);

        tree_->clear();

        auto* root = new QTreeWidgetItem;
        root->setText(0, tr("Sembol"));
        root->setToolTip(0, tr("Bütün katmanların ortak özellikleri"));
        root->setIcon(0, symbol_icon(symbol_, images, dashes, QSize(44, 26), paper, shape()));
        QFont font = root->font(0);
        font.setBold(true);
        root->setFont(0, font);
        tree_->addTopLevelItem(root);
        root->setExpanded(true);
        QTreeWidgetItem* chosen = root;
        const QColor eyeInk     = palette().color(QPalette::WindowText);

        for (std::size_t i = symbol_.layers.size(); i-- > 0;) {
            const core::SymbolLayer& sl = symbol_.layers[i];

            core::Symbol one;
            one.layers.push_back(sl);
            one.layers.front().enabled = true; // the row shows what it WOULD draw

            // The Turkish name on the row, the machine name in the tooltip: the
            // row is read at a glance and the token is what a script would write.
            QString label = QString::fromUtf8(core::symbol_layer_type_name(sl.type));
            for (const TypeRow& row : kTypes)
                if (row.type == sl.type) label = tr(row.label);
            if (sl.type == SymbolLayerType::SimpleLine && shape() == PreviewShape::Area)
                label = tr("Kenarlık");
            if (sl.colour_locked) label += tr("   · rengi kilitli"); // ui-label

            auto* item = new QTreeWidgetItem;
            item->setText(0, label);
            item->setToolTip(
                0, QStringLiteral("%1  ·  %2")
                       .arg(label, QString::fromUtf8(core::symbol_layer_type_name(sl.type))));
            item->setIcon(0, symbol_icon(one, images, dashes, QSize(44, 26), paper, shape()));
            item->setData(0, Qt::UserRole, static_cast<int>(i));
            // The eye says the state with a SHAPE (open or struck), not with a
            // colour alone (design.md §13); a switched-off row is also dimmed.
            item->setIcon(1, QIcon(glyph_pixmap(sl.enabled ? Glyph::Eye : Glyph::EyeOff,
                                                sl.enabled ? eyeInk : eyeInk.lighter(160), 16,
                                                devicePixelRatioF())));
            item->setToolTip(1, sl.enabled ? tr("Katmanı gizle") : tr("Katmanı göster"));
            if (!sl.enabled) item->setForeground(0, eyeInk.lighter(160));
            root->addChild(item);

            if (!keep_root && static_cast<int>(i) == keep) chosen = item;
        }

        // A new stack retains a visible selection for its whole-symbol page.
        tree_->setCurrentItem(chosen);
    }

    fitStackHeight();

    updatePreview();
    loadSelected();
}

void StyleDesigner::fitStackHeight()
{
    if (tree_ == nullptr) return;

    tree_->setMinimumHeight(120);
    tree_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void StyleDesigner::setPreviewScale(double denominator)
{
    previewScale_->setValue(static_cast<int>(std::round(std::clamp(denominator, 1.0, 1000000.0))));
}

void StyleDesigner::updatePreview()
{
    const core::ImageStore& images = draftImages_;
    const core::DashStore& dashes  = draftDashes_;

    previewWidth_ = preview_->width();
    PreviewOptions options;
    options.paper_pixels      = render::kDefaultPixelsPerPaperMm;
    options.scale_denominator = previewScale_->value();
    options.screen_ink = (theme() == ThemeMode::Dark ? darkTokens() : lightTokens()).text.rgba();
    options.hole       = sample_ && sample_->currentIndex() == 1;
    options.straight   = options.hole;
    const QImage swatch =
        symbol_preview(symbol_, images, dashes, preview_->size(),
                       (theme() == ThemeMode::Dark ? darkTokens() : lightTokens()).bgInput.rgba(),
                       shape(), PreviewGround::Flat, devicePixelRatioF(), options);

    preview_->setPixmap(QPixmap::fromImage(swatch));
}

bool StyleDesigner::eventFilter(QObject* watched, QEvent* event)
{
    // Watched on the LABEL, not on the dialog. A dialog resize arrives before the
    // layout has handed the label its share of it, so a preview redrawn there is
    // redrawn at the old width and never corrected.
    //
    // Only when the width actually moved: `setPixmap` feeds the layout a new size
    // hint, and re-rendering on every pass of that would be a loop.
    // The swatch is a fixed size now, so a resize no longer changes what is
    // drawn — only where it sits, which the label's own alignment handles.
    //
    // A COLOUR FIELD does change: it is painted at the width of its column, and
    // that width is not known until the layout has handed it out. Repainted on
    // resize, and only on resize, so the first layout does not leave a 200 px
    // chip in a 312 px field.
    // Clicking the picture selects the same root as the keyboard-accessible tree.
    if (watched == preview_ && event->type() == QEvent::MouseButtonRelease) {
        tree_->setCurrentItem(tree_->topLevelItem(0));
        loadSelected();
        return true;
    }

    if (watched == preview_ && event->type() == QEvent::Wheel) {
        const auto* wheel = static_cast<QWheelEvent*>(event);
        setPreviewScale(previewScale_->value() / std::pow(1.25, wheel->angleDelta().y() / 120.0));
        return true;
    }
    if (watched == preview_ && event->type() == QEvent::Resize) {
        updatePreview();
    }
    if (event->type() == QEvent::Resize) {
        const int i = currentLayer();
        if (watched == globalColour_) show_colour(globalColour_, symbol_.primary().rgba);
        if (i >= 0 && watched == stroke_) show_colour(stroke_, symbol_.layers[at(i)].look.rgba);
        if (i >= 0 && watched == fill_) show_colour(fill_, symbol_.layers[at(i)].look.fill_rgba);
    }

    return QDialog::eventFilter(watched, event);
}

void StyleDesigner::loadSelected()
{
    const int i     = currentLayer();
    const bool have = i >= 0;

    const Held quiet(loading_);

    // Page 0 is the symbol, page 1 is one of its layers.
    if (pages_ != nullptr) pages_->setCurrentIndex(have ? 1 : 0);
    if (!have) {
        updateSymbolCaption();
        loadGlobal();
        return;
    }

    if (have) {
        const core::SymbolLayer& sl = symbol_.layers[at(i)];

        const auto select = [](QComboBox* box, const auto& table, auto value) {
            for (int k = 0; k < static_cast<int>(table.size()); ++k)
                if (table[at(k)] == value) box->setCurrentIndex(k);
        };

        fillTypeChoices(sl.type);

        select(shape_, kShapes, sl.shape);
        select(placement_, kPlacements, sl.placement);
        select(cap_, kCaps, sl.cap);
        select(join_, kJoins, sl.join);
        select(sizeUnit_, kUnits, sl.size.unit);
        select(intervalUnit_, kUnits, sl.interval.unit);
        select(spacingYUnit_, kUnits, sl.spacing_y.unit);
        select(offsetUnit_, kUnits, sl.offset.unit);
        select(phaseUnit_, kUnits, sl.phase.unit);

        show_colour(stroke_, sl.look.rgba);
        show_colour(fill_, sl.look.fill_rgba);

        lock_->setChecked(sl.colour_locked);
        text_->setText(QString::fromStdString(sl.text));
        const auto display = [](QSpinBox* field, core::Unit unit) {
            static_cast<MeasureSpinBox*>(field)->setDivisor(unit == core::Unit::Pixel ? 1.0
                                                                                      : 1000.0);
            field->setSingleStep(unit == core::Unit::Pixel ? 1 : 100);
        };
        display(size_, sl.size.unit);
        display(interval_, sl.interval.unit);
        display(spacingY_, sl.spacing_y.unit);
        display(offset_, sl.offset.unit);
        display(phase_, sl.phase.unit);
        width_->setValue(sl.look.width_um);
        size_->setValue(sl.size.value);
        interval_->setValue(sl.interval.value);
        spacingY_->setValue(sl.spacing_y.value);
        offset_->setValue(sl.offset.value);
        phase_->setValue(sl.phase.value);
        angle_->setValue(sl.angle_udeg);
        opacity_->setValue(sl.opacity);
    }
    refreshBindingMarks();
    updateSymbolCaption();

    // Only the rows this type reads. A property nobody reads is a promise the
    // renderer does not keep.
    const SymbolLayerType type = have ? symbol_.layers[at(i)].type : SymbolLayerType::SimpleLine;

    // A `yazi-isaretci` layer's stroke IS the colour its text is written in, so
    // the one colour button is renamed rather than declared twice.
    strokeLabel_->setText(type == SymbolLayerType::TextMarker ? tr("Yazı rengi")
                                                              : tr("Çizgi rengi"));

    type_->setEnabled(have);
    // A heading is shown when one of its rows is. Two passes: the first clears,
    // the second lights, because the same heading is named by several rows and
    // the last row must not switch off what an earlier one switched on.
    for (const Property& p : properties_)
        if (p.group != nullptr) p.group->setVisible(false);
    for (const Property& p : properties_) {
        const bool shown = have && std::find(p.types.begin(), p.types.end(), type) != p.types.end();
        p.label->setVisible(shown && !p.label->text().isEmpty());
        p.editor->setVisible(shown);
        if (shown && p.group != nullptr && !p.group->property("compactGroup").toBool())
            p.group->setVisible(true);
    }
    for (QWidget* pair : pages_->widget(1)->findChildren<QWidget*>()) {
        if (!pair->property("stylePair").toBool()) continue;
        bool hasVisibleCell = false;
        for (int k = 0; k < pair->layout()->count(); ++k) {
            const QWidget* cell = pair->layout()->itemAt(k)->widget();
            if (cell != nullptr && !cell->isHidden()) hasVisibleCell = true;
        }
        pair->setVisible(hasVisibleCell);
    }
}

void StyleDesigner::loadGlobal()
{
    if (globalUnit_ == nullptr) return;

    std::optional<core::Unit> common;
    bool mixed = false;
    for (const core::SymbolLayer& layer : symbol_.layers) {
        for (const auto measure :
             {layer.size, layer.interval, layer.spacing_y, layer.offset, layer.phase}) {
            if (measure.value == 0) continue;
            if (!common)
                common = measure.unit;
            else if (*common != measure.unit)
                mixed = true;
        }
    }
    const core::Unit unit = common.value_or(core::Unit::Paper);
    globalUnit_->setCurrentIndex(mixed ? -1 : globalUnit_->findData(static_cast<int>(unit)));
    globalUnit_->setPlaceholderText(tr("Farklı birimler"));
    globalUnitNote_->setText(
        mixed ? tr("Ölçüler farklı birimlerde. Buradaki seçim hepsini dönüştürür; opaklık ve "
                   "kalınlık kendi alanlarını değiştirir.")
              : tr("Kâğıt: paftada mm. Zemin: çizimde m, ölçekle değişir. Piksel: ekranda px. "
                   "Birim dönüşümü önizlemedeki boyutu korur; çizgi kalınlığı kâğıt ölçüsüdür."));

    show_colour(globalColour_, symbol_.primary().rgba);
    globalWidth_->setValue(symbol_.primary().width_um);
    globalOpacity_->setValue(symbol_.layers.empty() ? 255 : symbol_.layers.front().opacity);
}

void StyleDesigner::applyGlobal(core::SymbolProperty property)
{
    if (loading_ || symbol_.layers.empty()) return;
    galleryCode_.clear();
    galleryPackage_.clear();
    for (core::SymbolLayer& layer : symbol_.layers) {
        if (property == core::SymbolProperty::Opacity)
            layer.opacity = static_cast<std::uint8_t>(globalOpacity_->value());
        if (property == core::SymbolProperty::Width && !layer.colour_locked) {
            layer.look.width_um  = globalWidth_->value();
            layer.look.src_width = core::Source::Explicit;
        }
    }
    refresh();
}

void StyleDesigner::applyGlobalUnit(core::Unit unit)
{
    if (loading_ || symbol_.layers.empty()) return;
    const double paperPixels  = render::kDefaultPixelsPerPaperMm;
    const double groundPixels = previewScale_->value() / paperPixels;
    galleryCode_.clear();
    galleryPackage_.clear();
    for (core::SymbolLayer& layer : symbol_.layers)
        for (auto* measure :
             {&layer.size, &layer.interval, &layer.spacing_y, &layer.offset, &layer.phase})
            *measure = render::measure_in_unit(*measure, unit, groundPixels, paperPixels);
    refresh();
}

void StyleDesigner::changeMeasureUnit(core::Measure core::SymbolLayer::* measure, core::Unit unit)
{
    if (loading_ || currentLayer() < 0) return;
    const double paperPixels = render::kDefaultPixelsPerPaperMm;
    auto& value              = symbol_.layers[at(currentLayer())].*measure;
    value = render::measure_in_unit(value, unit, previewScale_->value() / paperPixels, paperPixels);
    galleryCode_.clear();
    galleryPackage_.clear();
    refresh();
}

void StyleDesigner::syncTreeState()
{
    // Nothing to do while the tree is the only writer of these flags; kept as the
    // one place that would change if a second editor of them appeared.
}

void StyleDesigner::applyToSelected()
{
    if (loading_) return;

    const int i = currentLayer();
    if (i < 0) return;

    galleryCode_.clear();
    galleryPackage_.clear();
    core::SymbolLayer& sl = symbol_.layers[at(i)];

    const QObject* changed = sender();
    if (changed == type_) {
        sl.type = static_cast<SymbolLayerType>(type_->currentData().toInt());
        if (sl.type == SymbolLayerType::LinePatternFill && sl.interval.value == 0)
            sl.interval = {3000, core::Unit::Paper};
        if (core::draws_marker(sl.type) && sl.size.value == 0)
            sl.size = {render::kDefaultPointSizeUm, core::Unit::Paper};
        if (sl.type == SymbolLayerType::SimpleFill && sl.look.fill_rgba == 0)
            sl.look.fill_rgba = (sl.look.rgba & 0x00ffffffu) | 0x30000000u;
    }
    if (changed == shape_) sl.shape = pick(kShapes, shape_);
    if (changed == placement_) sl.placement = pick(kPlacements, placement_);
    if (changed == cap_) sl.cap = pick(kCaps, cap_);
    if (changed == join_) sl.join = pick(kJoins, join_);
    if (changed == size_) sl.size.value = size_->value();
    if (changed == interval_) sl.interval.value = interval_->value();
    if (changed == spacingY_) sl.spacing_y.value = spacingY_->value();
    if (changed == offset_) sl.offset.value = offset_->value();
    if (changed == phase_) sl.phase.value = phase_->value();
    if (changed == text_) sl.text = text_->text().toStdString();
    if (changed == width_) {
        sl.look.width_um  = width_->value();
        sl.look.src_width = core::Source::Explicit;
    }
    if (changed == angle_) sl.angle_udeg = angle_->value();
    if (changed == opacity_) sl.opacity = static_cast<std::uint8_t>(opacity_->value());

    refresh();
}

void StyleDesigner::addLayer()
{
    if (shape() != PreviewShape::Area) {
        addLayerOfType(default_type_for(shape()));
        return;
    }
    QMenu menu(this);
    menu.addAction(tr("Kenarlık"), this, [this] { addLayerOfType(SymbolLayerType::SimpleLine); });
    menu.addAction(tr("İç tarama"), this,
                   [this] { addLayerOfType(SymbolLayerType::LinePatternFill); });
    menu.addAction(tr("Dolgu"), this, [this] { addLayerOfType(SymbolLayerType::SimpleFill); });
    menu.addAction(tr("Çapraz tarama"), this, [this] {
        addLayerOfType(SymbolLayerType::LinePatternFill);
        addLayerOfType(SymbolLayerType::LinePatternFill);
        symbol_.layers[at(currentLayer())].angle_udeg = 135000000;
        refresh();
    });
    menu.exec(tree_->mapToGlobal(QPoint(0, tree_->height())));
}

void StyleDesigner::addLayerOfType(SymbolLayerType type)
{
    galleryCode_.clear();
    galleryPackage_.clear();
    core::SymbolLayer fresh;
    fresh.type           = type;
    fresh.look.rgba      = symbol_.primary().rgba;
    fresh.look.width_um  = 250;
    fresh.look.src_width = core::Source::Explicit;
    if (type == SymbolLayerType::SimpleMarker) {
        fresh.size           = {render::kDefaultPointSizeUm, core::Unit::Paper};
        fresh.look.fill_rgba = fresh.look.rgba;
    }
    if (type == SymbolLayerType::LinePatternFill) {
        fresh.interval   = {3000, core::Unit::Paper};
        fresh.angle_udeg = 45000000;
    }
    if (type == SymbolLayerType::SimpleFill)
        fresh.look.fill_rgba = (fresh.look.rgba & 0x00ffffffu) | 0x30000000u;
    auto position = symbol_.layers.end();
    if (core::draws_fill(type))
        position =
            std::find_if(symbol_.layers.begin(), symbol_.layers.end(), [](const auto& layer) {
                return layer.type == SymbolLayerType::SimpleLine;
            });
    const int index = static_cast<int>(position - symbol_.layers.begin());
    symbol_.layers.insert(position, fresh);
    refresh();
    auto* root = tree_->topLevelItem(0);
    for (int i = 0; i < root->childCount(); ++i)
        if (root->child(i)->data(0, Qt::UserRole).toInt() == index)
            tree_->setCurrentItem(root->child(i));
}

void StyleDesigner::duplicateLayer()
{
    const int i = currentLayer();
    if (i < 0) return;

    galleryCode_.clear();
    galleryPackage_.clear();
    symbol_.layers.push_back(symbol_.layers[at(i)]);
    refresh();
    selectTopLayer();
}

void StyleDesigner::removeLayer()
{
    const int i = currentLayer();
    if (i < 0 || symbol_.layers.size() <= 1) return; // a symbol with no layer draws nothing

    galleryCode_.clear();
    galleryPackage_.clear();
    symbol_.layers.erase(symbol_.layers.begin() + static_cast<std::ptrdiff_t>(at(i)));
    refresh();
}

void StyleDesigner::moveLayer(int delta)
{
    const int i = currentLayer();
    if (i < 0) return;

    const int to = i + delta;
    if (to < 0 || to >= static_cast<int>(symbol_.layers.size())) return;

    galleryCode_.clear();
    galleryPackage_.clear();
    std::swap(symbol_.layers[at(i)], symbol_.layers[at(to)]);
    refresh();

    auto* root = tree_->topLevelItem(0);
    for (int c = 0; c < root->childCount(); ++c)
        if (root->child(c)->data(0, Qt::UserRole).toInt() == to)
            tree_->setCurrentItem(root->child(c));
}

// ------------------------------------------------------------- the exits ----

bool StyleDesigner::applyToDocument()
{
    if (layerName_.isEmpty() || symbol_.layers.empty()) return false;
    if (renderer_ != Renderer::Single) return applyClassification();
    const QTemporaryDir package;
    QString error;
    if (!package.isValid() || !writeDraftImages(package.path(), &error)) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"), error);
        return false;
    }
    const QString path     = QDir(package.path()).filePath(QStringLiteral("symbol.json"));
    const QByteArray bytes = symbolPackageJson(layerName_, QStringLiteral("tasarim")).toUtf8();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"), tr("Paket yazılamadı: %1").arg(path));
        return false;
    }
    const auto applied =
        controller_.runLineResult(QStringLiteral("STİL katman=%1 paket=%2 kod=tasarim")
                                      .arg(quotedArg(layerName_), quotedArg(path)),
                                  command::Origin::Gui);
    if (!applied) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"),
                             QString::fromStdString(applied.error().message));
        return false;
    }
    original_ = symbol_;
    return true;
}

void StyleDesigner::clearStyle()
{
    if (layerName_.isEmpty()) return;

    QString quoted = layerName_;
    quoted.replace('\\', QStringLiteral("\\\\"));
    quoted.replace('"', QStringLiteral("\\\""));

    // THE COMMAND, not a reach into the style column. This window has no private
    // road to the document and this entry is no exception (Article 1.1, 5.9) —
    // it sends exactly the line the context menu used to send.
    auto cleared = controller_.runLineResult(
        QStringLiteral("STİL katman=\"%1\" sifirla=evet").arg(quoted), command::Origin::Gui);
    if (!cleared) {
        QMessageBox::warning(this, tr("Stil temizlenemedi"),
                             QString::fromStdString(cleared.error().message));
        return;
    }

    // And the window follows the document rather than keeping the symbol it was
    // showing: a dialog that still displays a style the layer no longer has is a
    // dialog that will write it back on the next Apply.
    resetToLayer();
}

void StyleDesigner::saveToLibrary()
{
    bool ok = false;

    // Hoisted so the gate annotation stays on the line it describes: clang-format
    // is free to rewrap an expression, and an annotation that drifts onto another
    // line stops being a claim about anything.
    const QString prompt = tr("Gösterim adı:"); // ui-label
    const QString name   = QInputDialog::getText(this, tr("Kütüphaneye kaydet"), prompt,
                                                 QLineEdit::Normal, layerName_, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // THE APPLICATION'S OWN SETTINGS DIRECTORY, resolved by Qt per platform:
    // ~/.config/PiriCAD on Linux, Application Support on macOS, AppData on
    // Windows. Not the project directory: a symbol a user designs belongs to the
    // user, travels with them between drawings, and must not turn up as an
    // untracked file next to somebody's pafta.
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (root.isEmpty()) {
        QMessageBox::warning(this, tr("Kaydedilemedi"), tr("Uygulama ayar dizini bulunamadı."));
        return;
    }

    QDir dir(root);
    if (!dir.mkpath(QStringLiteral("stiller"))) {
        QMessageBox::warning(this, tr("Kaydedilemedi"),
                             tr("Ayar dizini oluşturulamadı: %1").arg(root));
        return;
    }

    // Written as a gösterim PACKAGE rather than as a private blob, so `SEMBOL
    // paket=` loads it back onto the shelf beside the MPYY set. A user style and a
    // published one are the same kind of thing to everything downstream.
    const QString slug =
        QLocale(QLocale::Turkish)
            .toLower(name.trimmed())
            .replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    const QString path = dir.filePath(QStringLiteral("stiller/%1.json").arg(slug));

    const QString json = symbolPackageJson(name.trimmed(), slug);
    QString error;
    if (!writeDraftImages(QFileInfo(path).absolutePath(), &error)) {
        QMessageBox::warning(this, tr("Kaydedilemedi"), error);
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(json.toUtf8()) != json.toUtf8().size() ||
        !file.commit()) {
        QMessageBox::warning(this, tr("Kaydedilemedi"), tr("Dosya yazılamadı: %1").arg(path));
        return;
    }

    const auto loaded = controller_.bus().execute_line(
        QStringLiteral("SEMBOL paket=\"%1\"").arg(path).toStdString(), command::Origin::Gui);
    if (!loaded) {
        QMessageBox::warning(this, tr("Stil yüklenemedi"),
                             QString::fromStdString(loaded.error().message));
        return;
    }
    refreshGalleryTree();
    refreshGalleryItems();
    QMessageBox::information(this, tr("Kaydedildi"),
                             tr("Sembol kitaplığa kaydedildi:\n%1").arg(path));
}

} // namespace piricad::app
