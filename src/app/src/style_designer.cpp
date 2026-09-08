// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/style_designer.hpp"

#include "kentos_cad/core/attribute.hpp"

#include "kentos_cad/render/symbology.hpp"

#include "kentos_cad/app/tokens.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/schema_page.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/document.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cstring>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace kentos::app {
namespace {

/// How tall the symbol layer stack is allowed to be, in rows.
constexpr int kStackRowsMin = 3;
constexpr int kStackRowsMax = 6;

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
    {SymbolLayerType::LinePatternFill, "Çizgi desen dolgu"},
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
constexpr int kSwatchSide = 120;

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
QIcon geometry_glyph(PreviewShape shape, const QColor& ink)
{
    QPixmap glyph(16, 16);
    glyph.fill(Qt::transparent);

    QPainter painter(&glyph);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(ink, 1.4));

    switch (shape) {
    case PreviewShape::Area:
        painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 60));
        painter.drawRect(2, 3, 11, 9);
        break;
    case PreviewShape::Line:
        painter.drawPolyline(
            std::array<QPointF, 4>{QPointF(2, 11), QPointF(6, 4), QPointF(10, 11), QPointF(14, 4)}
                .data(),
            4);
        break;
    case PreviewShape::Point:
        painter.setBrush(ink);
        painter.drawEllipse(QPointF(8, 8), 3.0, 3.0);
        break;
    }
    painter.end();
    return QIcon(glyph);
}

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
    constexpr int kFieldHeight = 22;

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

    if (rgba == 0) {
        // "No fill" must look like NOTHING rather than like white, which is a
        // colour a plan sheet uses and a planner must be able to choose.
        const QColor faint = button->palette().color(QPalette::Mid);
        painter.setPen(QPen(faint, 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box, 3, 3);
        painter.setPen(faint);
        painter.drawText(box, Qt::AlignCenter, QObject::tr("dolgusuz")); // ui-label
    } else {
        const QColor colour = from_rgba(rgba);
        painter.setBrush(colour);
        painter.setPen(QPen(colour.darker(140), 1));
        painter.drawRoundedRect(box, 3, 3);
        painter.setPen(colour.lightnessF() > 0.55F ? Qt::black : Qt::white);
        painter.drawText(box, Qt::AlignCenter,
                         colour.alpha() == 255 ? colour.name(QColor::HexRgb).toUpper()
                                               : colour.name(QColor::HexArgb).toUpper());
    }
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
void fit_column(QComboBox* combo)
{
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(6);
}

QWidget* form_cell(QWidget* parent, const QString& label, QWidget* editor, QWidget* unit,
                   QLabel** caption_out)
{
    constexpr int kCaptionWidth = 110; // design.md §8

    auto* cell = new QWidget(parent);
    auto* row  = new QHBoxLayout(cell);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    auto* caption = new QLabel(label, cell);
    caption->setObjectName(QStringLiteral("formCaption"));
    caption->setFixedWidth(kCaptionWidth);
    caption->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    caption->setBuddy(editor);
    row->addWidget(caption);

    // THE ROW NEVER OUTGROWS THE COLUMN. Left to Qt, an editor's minimum is
    // its widest text — a seven-digit spin box wants 115 px, a unit combo 106 —
    // and beside a 110 px caption the two together asked for 347 of the 318 the
    // column has. The scroll area answers that by widening the page past its
    // viewport, and with no horizontal bar the last 30 px of every row simply
    // disappear under the dialog's edge. An explicit minimum overrides the
    // hint, and the stretch factors then share what the column actually has:
    // about 110 px for a value and 90 for its unit, which is room for both.
    editor->setMinimumWidth(1);
    row->addWidget(editor, 5);
    if (unit != nullptr) {
        unit->setMinimumWidth(1);
        row->addWidget(unit, 4);
    }

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

StyleDesigner::StyleDesigner(Controller& controller, QString layerName, QWidget* parent)
    : DialogFrame(parent), controller_(controller), layerName_(std::move(layerName))
{
    // design.md §8 measures this window at 1280 × 756 with a 186 px left column
    // and a 48 px footer. Every one of those numbers is the reference's.
    setHeading(Glyph::Palette, tr("Katman Özellikleri"), tr("— %1").arg(layerName_));
    setHelpVisible(true);
    setFooterHeight(48);
    setModal(true);
    // TALL ENOUGH FOR THE LONGEST FORM. 756 px cut the property page in half at
    // its last row — a marker carries type, shape, fill, stroke, width, size,
    // unit, angle and opacity, and the viewport ended in the middle of the last
    // one. A row bisected by an edge reads as a broken dialog, not as a hint that
    // there is more below, whatever the scrollbar says.
    //
    // The scroll area stays: a symbol layer's property list grows with its type
    // and a small screen is still a small screen. This only stops the ordinary
    // case from needing it.
    setMinimumSize(1040, 680);
    resize(1280, 880);

    const core::LayerId layer = controller_.document().find_layer(layerName_.toStdString());
    symbol_                   = layer == core::kNoLayer ? core::Symbol::of(core::Appearance{})
                                                        : symbol_of_layer(controller_.document(), layer);
    if (symbol_.layers.empty()) symbol_ = core::Symbol::of(core::Appearance{});
    original_ = symbol_;

    // ---- geometry tabs and the big preview ----
    //
    // The tabs come FIRST because they are the first decision: what geometry is
    // this symbol for. They choose the shape the preview is drawn on AND the
    // drawer of the shelf that is open — the classification QGIS puts in a
    // separate window's tab bar.
    //
    // DRAWN AS TABS, AND SIZED TO THEIR LABELS. Stretched edge to edge over a bare
    // dialog background they read as three flat grey buttons — reported as "you
    // cannot even tell those are tabs" — which is a bad look for the control that
    // carries the first decision. Three things fix it and all three are needed: a
    // tab-shaped border, a width that comes from the text, and a PANE underneath
    // for the selected tab to join, so the pair reads as one object.
    const QColor ink = palette().color(QPalette::WindowText);

    geometry_ = new QTabBar(this);
    geometry_->setExpanding(false);
    geometry_->setDrawBase(false);
    geometry_->setUsesScrollButtons(false);
    geometry_->setIconSize(QSize(16, 16));
    geometry_->addTab(geometry_glyph(PreviewShape::Area, ink), tr("Alan"));
    geometry_->addTab(geometry_glyph(PreviewShape::Line, ink), tr("Çizgi"));
    geometry_->addTab(geometry_glyph(PreviewShape::Point, ink), tr("Nokta"));
    geometry_->setTabToolTip(0, tr("Parsel, ada, yapı — kapalı alanlar"));
    geometry_->setTabToolTip(1, tr("Sınır, yol ekseni, kanal — çizgiler"));
    geometry_->setTabToolTip(2, tr("Nirengi, poligon, ağaç — noktalar"));
    geometry_->setAccessibleName(tr("Sembolün çizileceği geometri"));
    geometry_->setCurrentIndex(
        static_cast<int>(shape_of_layer(controller_.document(),
                                        controller_.document().find_layer(layerName_.toStdString()))
                             .value_or(natural_shape(symbol_))));
    connect(geometry_, &QTabBar::currentChanged, this, [this](int) {
        adopt_geometry_default(symbol_, shape());
        refreshGalleryItems();
        refresh();
        updateHeaderNote();
    });

    // And once for the tab the dialog OPENED on, which is the case a user meets
    // first: a point layer with no symbology of its own.
    adopt_geometry_default(symbol_, shape());

    // THE SWATCH AND ITS ONE-WORD CAPTION. No title: the dialog's own title bar
    // already says which layer this is, and a 16 px heading over a 120 px
    // picture was a third of the space the picture had. The caption under it
    // still says what geometry the picture is drawn on — a user who does not
    // connect the tab to the picture reads a zigzag as a claim about their
    // parcels — but it is a phrase, not a sentence, because the column beside a
    // swatch is the stack's, not the caption's.
    auto* previewFrame = new QFrame(this);
    previewFrame->setObjectName(QStringLiteral("stylePreview"));
    auto* previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(4);

    preview_ = new QLabel(previewFrame);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setFixedSize(kSwatchSide, kSwatchSide);
    preview_->setObjectName(QStringLiteral("stylePreviewImage"));
    preview_->setAccessibleName(tr("Katman stili ön izlemesi"));
    preview_->installEventFilter(this);

    headerNote_ = new QLabel(previewFrame);
    headerNote_->setObjectName(QStringLiteral("quiet"));
    headerNote_->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    headerNote_->setFixedWidth(kSwatchSide);

    previewLayout->addWidget(preview_);
    previewLayout->addWidget(headerNote_);
    previewLayout->addStretch(1);

    // No gap between the bar and the pane: the selected tab has to touch what it
    // opens, or the two are just a row of buttons above a box.
    auto* tabRow = new QHBoxLayout;
    tabRow->setContentsMargins(1, 0, 0, 0);
    tabRow->setSpacing(0);
    tabRow->addWidget(geometry_);
    tabRow->addStretch(1);

    // ---- TWO COLUMNS, not four stacked bands -------------------------------
    //
    // This page used to be a vertical stack: the renderer row, the geometry
    // tabs, a full-width preview band, and then a splitter holding everything
    // else. Inside a 756 px dialog that left about 300 px for the shelf AND the
    // symbol stack AND the property form, and Qt does what Qt does when a layout
    // is starved — it squeezes children past their minimums until they overlap.
    // The search field sat on top of the shelf's tree, the thumbnails were a
    // 60 px band, and `Katman özellikleri` showed one row with the rest below
    // the bottom of the window and no way to reach it.
    //
    // design.md §8 puts the symbol's own controls in a 352 px column on the
    // right and gives the rest of the width to what the user is choosing FROM.
    // The preview belongs at the top of that column, not across the page: it is
    // a property of the symbol being edited, not a banner over the whole screen.
    auto* left       = new QWidget(this);
    auto* leftColumn = new QVBoxLayout(left);
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(0);
    leftColumn->addLayout(tabRow);
    leftColumn->addWidget(buildGallery(), 1);

    auto* right = new QWidget(this);
    right->setObjectName(QStringLiteral("symbolColumn"));
    right->setFixedWidth(352);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 10, 8, 10);
    rightLayout->setSpacing(10);

    // SWATCH BESIDE THE STACK, not above it — the way the reference draws them.
    //
    // Stacked, the two small things took 372 px of a 756 px window between them
    // and the property form — the thing a user is actually editing — was left a
    // strip that showed three rows and scrolled for the rest. Side by side they
    // take the height of the stack alone, and the form gets what a form needs.
    //
    // The row is sized by its content and no more (`Maximum`): the swatch is
    // fixed, the tree is a fixed number of rows, and everything under this line
    // belongs to the property form.
    QWidget* stack = buildTree();

    auto* top    = new QWidget(right);
    auto* topRow = new QHBoxLayout(top);
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(10);
    topRow->addWidget(previewFrame);
    topRow->addWidget(stack, 1);
    top->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    rightLayout->addWidget(top);

    // TWO PAGES, one selection. Selecting the symbol shows what belongs to all of
    // it; selecting a layer shows what belongs to that layer. Showing both at once
    // is what makes a symbol editor confusing — a user cannot tell which colour
    // they are about to change.
    pages_ = new QStackedWidget(this);
    pages_->addWidget(buildGlobal());
    pages_->addWidget(buildProperties());

    // INSIDE A SCROLL AREA. A symbol layer's property list grows with its type —
    // a marker line carries placement, phase, angle and offset that a plain
    // stroke does not — and without this the last rows were simply cut off at
    // the bottom of the dialog with no way to reach them.
    // Room for the scrollbar, which otherwise sits ON the editors: the widget
    // gets the viewport's width and the bar is drawn over its right edge.
    // AND ROOM UNDER THE LAST ROW. Flush against the viewport's edge, the last
    // editor is bisected by it the moment the page is one pixel too tall; a
    // row-height of air means the scroll ends on whitespace instead.
    pages_->setContentsMargins(0, 0, 14, 16);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(pages_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightLayout->addWidget(scroll, 1);

    auto* panes   = new QWidget(this);
    auto* paneRow = new QHBoxLayout(panes);
    paneRow->setContentsMargins(0, 0, 0, 0);
    paneRow->setSpacing(0);
    paneRow->addWidget(left, 1);
    paneRow->addWidget(right);

    // ---- the page this section shows ----
    auto* renderer       = new QWidget(this);
    auto* rendererLayout = new QVBoxLayout(renderer);
    rendererLayout->setContentsMargins(0, 0, 0, 0);
    rendererLayout->setSpacing(0);
    rendererLayout->addWidget(buildRendererRow());
    rendererLayout->addWidget(panes, 1);

    // ---- the left section list, design.md §8 ----
    //
    // Twelve sections, and the ones with nothing behind them yet say which phase
    // brings them rather than being hidden (§11.8). A hidden section is a
    // capability a user cannot find out about; a named one is a promise with a
    // date on it.
    sections_  = new SectionList(this);
    pageStack_ = new QStackedWidget(this);

    struct Page
    {
        Glyph glyph;
        const char* title;
        const char* phase;
        const char* note;
    };

    static const Page kPages[] = {
        {Glyph::Help, "Bilgi", "", ""},
        {Glyph::Open, "Kaynak", "Faz 2",
         "Katmanın verisinin nereden geldiği — dosya yolu, PostGIS bağlantısı, "
         "koordinat sistemi ve kodlama — buraya gelecek."},
        {Glyph::Palette, "Simgeleyici", "", ""},
        {Glyph::Table, "Öznitelikler", "", ""},
        {Glyph::Text, "Etiketler", "Faz 2",
         "Etiket yerleşimi, çakışma çözümü ve ölçek aralıkları buraya gelecek. "
         "Bugün etiketler ETİKET komutuyla yazılır; bkz. docs/komutlar/label.md."},
        {Glyph::Terrain, "3B Görünüm", "Faz 3", "Yükseklik, cephe ve çatı çizimi buraya gelecek."},
        {Glyph::EyeOff, "Şeffaflık", "Faz 2", "Katman saydamlığı ve karışım kipi buraya gelecek."},
        {Glyph::Measure, "Ölçek", "Faz 2",
         "Katmanın hangi ölçek aralığında çizileceği buraya gelecek."},
        {Glyph::Table, "Öznitelik Formu", "Faz 2",
         "Tek kaydın form görünümü ve alan denetimleri buraya gelecek. "
         "Sütunların kendisi Öznitelikler sayfasında tanımlanır."},
        {Glyph::Topology, "Geçerlilik", "Faz 2",
         "Geometri ve öznitelik geçerlilik kuralları buraya gelecek."},
        {Glyph::Script, "Eylemler", "Faz 3",
         "Nesneye bağlı eylemler — belge aç, servis çağır — buraya gelecek."},
        {Glyph::Union, "Bağlantılar", "Faz 3",
         "Başka katman ve tablolarla ilişkilendirme buraya gelecek."},
        {Glyph::History, "Sürüm", "Faz 3",
         "Katmanın sürüm geçmişi ve geri alma noktaları buraya gelecek."},
    };

    for (const Page& page : kPages) {
        const QString title = tr(page.title);
        sections_->addSection(page.glyph, title);

        if (std::strlen(page.phase) == 0) {
            if (std::strcmp(page.title, "Simgeleyici") == 0)
                pageStack_->addWidget(renderer);
            else if (std::strcmp(page.title, "Öznitelikler") == 0) {
                schema_ = new SchemaPage(controller_, this);
                pageStack_->addWidget(schema_);
            } else
                pageStack_->addWidget(buildInfoPage());
            continue;
        }
        pageStack_->addWidget(buildPendingPage(tr(page.phase), tr(page.note)));
    }

    connect(sections_, &SectionList::currentChanged, pageStack_, &QStackedWidget::setCurrentIndex);

    auto* body = new QWidget(this);
    auto* row  = new QHBoxLayout(body);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    auto* sidebar = new QWidget(body);
    sidebar->setObjectName(QStringLiteral("designerSidebar"));
    sidebar->setFixedWidth(186);
    auto* column = new QVBoxLayout(sidebar);
    column->setContentsMargins(0, 8, 1, 8);
    column->setSpacing(0);
    column->addWidget(sections_);
    column->addStretch(1);

    row->addWidget(sidebar);

    auto* pageHost   = new QWidget(body);
    auto* pageLayout = new QVBoxLayout(pageHost);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(pageStack_, 1);
    row->addWidget(pageHost, 1);

    setBody(body);
    sections_->setCurrent(2); // Simgeleyici
    pageStack_->setCurrentIndex(2);

    // ---- the footer, §8 ----
    const auto footerButton = [this](const QString& text, bool primary) {
        auto* button = new QPushButton(text, this);
        if (primary) {
            button->setObjectName(QStringLiteral("primary"));
            button->setDefault(true);
        }
        return button;
    };

    // A REAL MENU, and it earned its arrow. `Stil ▾` was a plain button that did
    // one thing — revert the window — while wearing the mark of a button that
    // opens a list; and beside it sat a second full-width button for saving to
    // the library, which is the rarest action in the window taking the most room
    // in the footer.
    //
    // `Stili temizle` moved in here from the layer's context menu, where it was
    // one of two style entries scattered among `Gizle` and `Gruba taşı…`. It is
    // the one style action that is not "edit the symbol", so it belongs with the
    // other things done TO a style rather than in it.
    auto* styleMenu = footerButton(tr("Stil ▾"), false);
    auto* actions   = new QMenu(styleMenu);

    QAction* revert = actions->addAction(tr("Katmanın çizdiğine dön"));
    revert->setToolTip(tr("Bu penceredeki değişiklikleri atar; katmana dokunmaz"));
    connect(revert, &QAction::triggered, this, &StyleDesigner::resetToLayer);

    QAction* clear = actions->addAction(tr("Stili temizle"));
    clear->setToolTip(tr("Katmanın stilini siler; nesneler katman görünümüne döner"));
    connect(clear, &QAction::triggered, this, &StyleDesigner::clearStyle);

    actions->addSeparator();

    QAction* save = actions->addAction(tr("Sembolü kütüphaneye kaydet…"));
    save->setToolTip(tr("Sembolü kendi gösterim paketiniz olarak diske yazar")); // ui-label
    connect(save, &QAction::triggered, this, &StyleDesigner::saveToLibrary);

    styleMenu->setMenu(actions);

    auto* cancel = footerButton(tr("İptal"), false);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* apply = footerButton(tr("Uygula"), false);
    apply->setToolTip(tr("Sembolü STİL komutlarına çevirip katmana yazar; pencere açık kalır"));
    connect(apply, &QPushButton::clicked, this, [this] { (void)applyToDocument(); });

    auto* ok = footerButton(tr("Tamam"), true);
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
}

void StyleDesigner::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
    if (sections_) sections_->applyTheme(mode);
    if (schema_) schema_->applyTheme(mode);

    // The swatch's checkerboard is drawn from the tokens, so it has to be drawn
    // again when they change — a dark lattice under a light dialog is exactly the
    // kind of leftover a theme switch is meant not to produce.
    if (preview_) updatePreview();
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
    headerNote_->setText(note);
    headerNote_->setToolTip(tr("Ön izleme bu geometri üzerinde çiziliyor; üstteki sekme seçer"));
}

PreviewShape StyleDesigner::shape() const
{
    return static_cast<PreviewShape>(std::clamp(geometry_->currentIndex(), 0, 2));
}

// ------------------------------------------------------------- the shelf ----

QWidget* StyleDesigner::buildRendererRow()
{
    // design.md §8's top row: what KIND of renderer, what it is driven by, and
    // what unit its sizes are in. The unit control is the one a user reaches for
    // most — "stay the same size when I zoom" versus "grow with the drawing" —
    // so it sits at the right end where the eye lands last and stays.
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("rendererRow"));

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(14, 8, 14, 8);
    row->setSpacing(18);

    // Each control under a SMALL-CAPS caption, as §8 draws it. These were 16 px
    // headings for a while, which made a strip of two controls read as two
    // sections of the window and cost the strip a third of its height.
    const auto field = [&](const QString& caption, QWidget* editor) {
        auto* cell   = new QWidget(bar);
        auto* column = new QVBoxLayout(cell);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);

        auto* label = new QLabel(caption, cell);
        label->setObjectName(QStringLiteral("groupCaption"));
        column->addWidget(label);
        column->addWidget(editor);
        row->addWidget(cell);
        return cell;
    };

    renderKind_ = new QComboBox(bar);
    renderKind_->addItem(tr("Tek Sembol"));
    renderKind_->addItem(tr("Kategorize Edilmiş"));
    renderKind_->addItem(tr("Aralıklı"));
    renderKind_->setMinimumWidth(190);
    connect(renderKind_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index == 0) return;
        // §11.8: say which phase, do not pretend. A categorised renderer needs a
        // per-value symbol column in the document, which is a model change and
        // not a widget.
        QMessageBox::information(
            this, tr("Simgeleyici"),
            tr("Kategorize ve aralıklı simgeleyiciler Faz 2'de gelecek: her değere kendi "
               "sembolünü veren bir sütun, belgede tanımlanmayı bekliyor. Bugün bir katman "
               "tek sembol çizer; değere göre ayırmak için katmanı bölün ya da ETİKET ile "
               "yazın."));
        renderKind_->setCurrentIndex(0);
    });
    field(tr("SİMGELEYİCİ"), renderKind_);

    renderValue_ = new QComboBox(bar);
    renderValue_->setMinimumWidth(190);
    for (std::size_t c = 0; c < controller_.document().attributes().columns(); ++c) {
        const core::AttrColumn* column =
            controller_.document().attributes().column(static_cast<core::AttrId>(c));
        if (column) renderValue_->addItem(QString::fromStdString(column->spec().id));
    }

    // HIDDEN, not shown disabled with an em dash in it. A control that is present
    // but does nothing is worse than one that is absent: the reader spends the
    // look working out why it will not open, and the answer — "this renderer has
    // no value column" — is already said by the renderer beside it.
    valueCell_ = field(tr("DEĞER"), renderValue_);
    valueCell_->setVisible(false);

    row->addStretch(1);

    // The unit, as three buttons rather than a combo: three choices that a user
    // switches between constantly read faster side by side than in a list, and
    // the reference draws them that way.
    auto* units       = new QWidget(bar);
    auto* unitsLayout = new QHBoxLayout(units);
    unitsLayout->setContentsMargins(0, 0, 0, 0);
    unitsLayout->setSpacing(0);

    // ONE OF THREE, ENFORCED BY QT. The buttons were checkable and ungrouped, so
    // Qt toggled each on its own: clicking the lit one turned it OFF and left the
    // control with nothing selected, and two could read as lit until something
    // else happened to re-sync them. A segmented control that can show no answer
    // and can show two answers is not a segmented control.
    unitGroup_ = new QButtonGroup(bar);
    unitGroup_->setExclusive(true);

    static const std::pair<core::Unit, const char*> kUnitButtons[] = {
        {core::Unit::Paper, "Milimetre"},
        {core::Unit::Ground, "Harita birimi"},
        {core::Unit::Pixel, "Piksel"},
    };
    for (const auto& [unit, label] : kUnitButtons) {
        auto* button = new QPushButton(tr(label), units);
        button->setObjectName(QStringLiteral("segment"));
        button->setCheckable(true);
        button->setProperty("unit", static_cast<int>(unit));
        unitButtons_.push_back(button);
        unitGroup_->addButton(button, static_cast<int>(unit));

        connect(button, &QPushButton::clicked, this, [this, unit] {
            // EVERY MEASURE ON THE LAYER, not three of the five. `spacing_y` and
            // `phase` were left behind, so switching a marker line to map units
            // converted its size, its interval and its offset and left its second
            // spacing and its phase in paper millimetres — a symbol half in one
            // unit and half in another, which the mixed check could not even see
            // because that check reads `size` alone.
            for (core::SymbolLayer& l : symbol_.layers) {
                l.size.unit      = unit;
                l.interval.unit  = unit;
                l.spacing_y.unit = unit;
                l.offset.unit    = unit;
                l.phase.unit     = unit;
            }
            refresh();
            updatePreview();
        });
        unitsLayout->addWidget(button);
    }
    field(tr("SEMBOL BOYUT BİRİMİ"), units);

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

    groups_ = new QTreeWidget(box);
    groups_->setObjectName(QStringLiteral("designerList"));
    groups_->setHeaderHidden(true);
    groups_->setUniformRowHeights(true);

    // A WHOLE NUMBER OF ROWS. The box was 132 px against a row of about 36, so it
    // ended on a half-drawn annex name — which reads as a rendering fault rather
    // than as a list that scrolls.
    const int row_px = groups_->fontMetrics().height() + 14;
    groups_->setMinimumHeight(row_px * 4 + 4);
    groups_->setMaximumHeight(row_px * 6 + 4);
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
    gallery_->setViewMode(QListView::IconMode);
    gallery_->setIconSize(QSize(56, 40));
    // WIDE ENOUGH FOR TWO LINES of a published name, TALL ENOUGH FOR FOUR.
    //
    // At 88 px the grid elided every label to its first word and a drawer of
    // water gösterims read as five rows of `İÇME VE …` — five different symbols
    // the list said were the same thing. 118 px fixed the width; the height was
    // still three lines, so the names that actually need the room were the ones
    // still losing it.
    //
    // Four lines is measured, not guessed. Over the 467 published styles the
    // median name is 20 characters, the 90th percentile 40 and the 95th 48 —
    // three lines covers 95% and four covers all but a handful, of which the
    // longest is `KATI ATIK TESİSLERİ ALANI (BOŞALTMA, BERTARAF, İŞLEME,
    // TRANSFER VE DEPOLAMA)` at 76. Sizing every cell for THAT would waste a
    // third of the grid on the median row, so the handful keep their tooltip.
    gallery_->setGridSize(QSize(118, 128));
    gallery_->setResizeMode(QListView::Adjust);
    gallery_->setMovement(QListView::Static);
    gallery_->setWordWrap(true);
    gallery_->setSpacing(4);
    gallery_->setAccessibleName(tr("Hazır gösterim galerisi")); // ui-label
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
    provenance_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    connect(gallery_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem*, QListWidgetItem*) { showProvenance(); });

    use_ = new QPushButton(tr("Seçileni kullan"), box);
    use_->setObjectName(QStringLiteral("primary"));
    use_->setToolTip(tr("Seçili gösterimi düzenlenebilir sembol yığını olarak alır")); // ui-label
    connect(use_, &QPushButton::clicked, this, &StyleDesigner::applyGalleryPick);

    // The search goes ABOVE the tree it filters. Between the tree and the
    // thumbnails it read as belonging to neither, and when the box was starved
    // for height it was the row that overlapped its neighbours.
    layout->addWidget(search_);
    layout->addWidget(groups_, 1);
    layout->addWidget(gallery_, 3);
    layout->addWidget(galleryNote_);
    layout->addWidget(provenance_);

    // RIGHT, AND ITS OWN WIDTH. A button stretched across seven hundred pixels
    // reads as a banner rather than as something to press, and it is the one
    // action in this half of the window — §15.1's single primary.
    auto* useRow = new QHBoxLayout;
    useRow->setContentsMargins(0, 0, 0, 0);
    useRow->addStretch(1);
    useRow->addWidget(use_);
    layout->addLayout(useRow);
    return box;
}

void StyleDesigner::refreshGalleryTree()
{
    const core::StyleLibrary& shelf = controller_.bus().style_library();

    groups_->clear();

    auto* all = new QTreeWidgetItem(groups_);
    all->setText(0, tr("Tümü"));
    all->setData(0, Qt::UserRole, QStringList{});

    // The regulation's own tree, two levels deep. Four hundred rows over five
    // annexes is not a package big enough to need lazy expansion, and a tree that
    // is all there is a tree you can search by eye.
    const std::vector<std::string> root;
    for (const std::string& annex : shelf.children(root)) {
        auto* node = new QTreeWidgetItem(groups_);
        node->setText(0, QString::fromStdString(annex));
        node->setData(0, Qt::UserRole, QStringList{QString::fromStdString(annex)});

        const std::vector<std::string> level{annex};
        for (const std::string& section : shelf.children(level)) {
            auto* leaf = new QTreeWidgetItem(node);
            leaf->setText(0, QString::fromStdString(section));
            leaf->setData(
                0, Qt::UserRole,
                QStringList{QString::fromStdString(annex), QString::fromStdString(section)});
        }
    }
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
            type_->addItem(QStringLiteral("%1  (%2)")
                               .arg(label, QString::fromUtf8(core::symbol_layer_type_name(t))),
                           static_cast<int>(t));
        }
    }

    const QSignalBlocker quiet(type_);
    for (int i = 0; i < type_->count(); ++i)
        if (type_->itemData(i).toInt() == static_cast<int>(current)) type_->setCurrentIndex(i);
}

void StyleDesigner::refreshGalleryItems()
{
    const core::StyleLibrary& shelf = controller_.bus().style_library();
    const std::uint32_t paper       = palette().color(QPalette::Base).rgba();
    const core::SymbolKind kind     = kind_of(shape());

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
    // Only when there is a citation to show; an empty one is a blank strip
    // between the thumbnails and the button under them.
    provenance_->setVisible(stocked && !provenance_->text().isEmpty());
    galleryNote_->setAlignment(stocked ? Qt::AlignLeft | Qt::AlignTop : Qt::AlignCenter);
    use_->setEnabled(stocked);

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
    } else if (QTreeWidgetItem* item = groups_->currentItem()) {
        const QStringList path = item->data(0, Qt::UserRole).toStringList();
        if (path.isEmpty()) {
            rows = shelf.of_kind(kind);
        } else {
            std::vector<std::string> where;
            for (const QString& part : path)
                where.push_back(part.toStdString());

            // An annex node has its sections below it and no rows of its own, so
            // opening one shows what is under it rather than nothing.
            rows = shelf.in_group(where);
            if (rows.empty()) {
                for (const std::string& child : shelf.children(where)) {
                    std::vector<std::string> deeper = where;
                    deeper.push_back(child);
                    const auto found = shelf.in_group(deeper);
                    rows.insert(rows.end(), found.begin(), found.end());
                }
            }
        }
    }

    // Filtered by GEOMETRY, always: the tabs are the first decision, and a drawer
    // opened under `Çizgi` must not hand back an area gösterim.
    std::vector<const core::LibraryEntry*> matching;
    for (const core::LibraryEntry* e : rows)
        if (e->kind == kind) matching.push_back(e);

    const int shown = std::min(static_cast<int>(matching.size()), kGalleryCap);
    for (int i = 0; i < shown; ++i) {
        const core::LibraryEntry* e = matching[at(i)];

        auto* item = new QListWidgetItem(gallery_);
        item->setText(QString::fromStdString(e->label.empty() ? e->id : e->label));
        item->setIcon(symbol_icon(e->symbol, images, shelf.dashes(), QSize(56, 40), paper,
                                  shape_of(e->kind)));
        item->setData(Qt::UserRole, QString::fromStdString(e->id));
        // The FULL name first. The cell shows two lines and a published gösterim
        // name is often longer than that, so the tooltip has to carry the name and
        // not only its provenance.
        item->setToolTip(QString::fromStdString((e->label.empty() ? e->id : e->label) + "\n" +
                                                e->id + "\n" + e->source_ref));
    }

    if (matching.empty())
        galleryNote_->setText(tr("Bu geometride eşleşen gösterim yok.")); // ui-label
    else if (shown < static_cast<int>(matching.size()))
        galleryNote_->setText(tr("%1 gösterimden ilk %2 tanesi. Aramayı daraltın.") // ui-label
                                  .arg(matching.size())
                                  .arg(shown));
    else
        galleryNote_->setText(tr("%1 gösterim.").arg(matching.size())); // ui-label
}

void StyleDesigner::showProvenance()
{
    QListWidgetItem* item = gallery_->currentItem();
    if (item == nullptr) {
        provenance_->clear();
        provenance_->setVisible(false);
        return;
    }

    const core::LibraryEntry* e =
        controller_.bus().style_library().find(item->data(Qt::UserRole).toString().toStdString());
    if (e == nullptr) {
        provenance_->clear();
        provenance_->setVisible(false);
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
    provenance_->setVisible(true);
    provenance_->setToolTip(QString::fromStdString(e->id));
}

void StyleDesigner::resetToLayer()
{
    symbol_ = original_;
    galleryCode_.clear();
    galleryPackage_.clear();
    geometry_->setCurrentIndex(static_cast<int>(natural_shape(symbol_)));
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
    symbol_         = e->symbol;
    galleryCode_    = QString::fromStdString(e->id);
    galleryPackage_ = QString::fromStdString(e->package_path);
    geometry_->setCurrentIndex(static_cast<int>(shape_of(e->kind)));
    refresh();
    selectTopLayer();
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

    auto* caption = new QLabel(tr("SEMBOL KATMANLARI"), box); // ui-label
    caption->setObjectName(QStringLiteral("groupCaption"));
    layout->addWidget(caption);

    tree_ = new QTreeWidget(box);
    tree_->setObjectName(QStringLiteral("designerList"));
    tree_->setHeaderHidden(true);
    tree_->setIconSize(QSize(44, 26));
    tree_->setRootIsDecorated(true);
    tree_->setAlternatingRowColors(true);
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

    connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) {
        if (loading_ || item == nullptr) return;

        // The stack index the row CARRIES, not the row it sits on. A row whose
        // index has not been written yet is a row this dialog is still building,
        // and answering it as a user edit is what used to switch symbol layer 0
        // off behind the user's back.
        const QVariant carried = item->data(0, Qt::UserRole);
        if (!carried.isValid()) return;

        const int index = carried.toInt();
        if (index < 0 || index >= static_cast<int>(symbol_.layers.size())) return;

        galleryCode_.clear();
        galleryPackage_.clear();
        symbol_.layers[at(index)].enabled = item->checkState(0) == Qt::Checked;

        // QUEUED, and this is a correctness fix rather than a nicety. Qt is still
        // inside `QTreeWidgetItem::setCheckState` when this runs; `refresh()`
        // clears the tree, which deletes that very item, and the call Qt is in
        // the middle of then returns into freed memory. The list version of this
        // dialog survived it by luck and the tree does not — it segfaults on the
        // first click of a check box.
        //
        // A slot that rebuilds the model it was notified about has to do it after
        // the notification has unwound. That is what a queued call is for.
        QMetaObject::invokeMethod(this, &StyleDesigner::refresh, Qt::QueuedConnection);
    });

    // §15.1's icon button: 24 px, ghost, a tooltip and no label. They were bare
    // `QToolButton`s carrying a glyph as their text, so they drew at whatever
    // width the glyph happened to be — five different sizes in one row.
    const auto button = [&](const QString& text, const QString& tip, auto slot) {
        auto* b = new QToolButton(box);
        b->setObjectName(QStringLiteral("rowTool"));
        b->setText(text);
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setFixedSize(24, 24);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(2);
    bar->addWidget(button(QStringLiteral("+"), tr("Katman ekle"), &StyleDesigner::addLayer));
    bar->addWidget(
        button(QStringLiteral("⧉"), tr("Katmanı kopyala"), &StyleDesigner::duplicateLayer));
    bar->addWidget(button(QStringLiteral("−"), tr("Katmanı sil"), &StyleDesigner::removeLayer));
    bar->addStretch(1);
    bar->addWidget(button(QStringLiteral("▲"), tr("Yukarı"), [this] { moveLayer(+1); }));
    bar->addWidget(button(QStringLiteral("▼"), tr("Aşağı"), [this] { moveLayer(-1); }));

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
    form->setSpacing(6);
    addGroup(form, tr("SEMBOL")); // ui-label

    // ---- THE UNIT, and it is the first row on purpose ----
    //
    // This is the question a CAD user actually asks about a symbol, and until now
    // this dialog answered it in a combo box beside a number with no explanation:
    // does this stay the same size when I zoom, or does it grow with the drawing?
    // Both are right and which one is right depends on what the symbol MEANS. A
    // boundary's thickness belongs to the SHEET — MPYY says 0,5 mm and it is
    // 0,5 mm at 1/1000 and at 1/5000 — so it must not move when the view does. A
    // forest hatch belongs to the GROUND: it covers an area, and letting it shrink
    // with the zoom turns a legible texture into a grey wash.
    globalUnit_ = new QComboBox(box);
    fit_column(globalUnit_);
    globalUnit_->addItem(tr("Kâğıt — yakınlaştırınca boyu DEĞİŞMEZ"), // ui-label
                         static_cast<int>(core::Unit::Paper));
    globalUnit_->addItem(tr("Zemin — çizimle birlikte BÜYÜR ve küçülür"), // ui-label
                         static_cast<int>(core::Unit::Ground));
    globalUnit_->addItem(tr("Piksel — ham ekran pikseli"), // ui-label
                         static_cast<int>(core::Unit::Pixel));
    connect(globalUnit_, &QComboBox::currentIndexChanged, this, [this](int) { applyGlobal(); });

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

    globalWidth_ = new QSpinBox(box);
    globalWidth_->setRange(0, 100000);
    globalWidth_->setSingleStep(100);
    globalWidth_->setSuffix(tr(" µm"));
    connect(globalWidth_, &QSpinBox::valueChanged, this, [this](int) { applyGlobal(); });

    globalOpacity_ = new QSpinBox(box);
    globalOpacity_->setRange(0, 255);
    globalOpacity_->setSingleStep(5);
    connect(globalOpacity_, &QSpinBox::valueChanged, this, [this](int) { applyGlobal(); });

    form->addWidget(form_cell(box, tr("Ölçü birimi"), globalUnit_, nullptr, nullptr));
    form->addWidget(globalUnitNote_); // the note belongs to the unit above it
    form->addWidget(form_cell(box, tr("Renk"), globalColour_, nullptr, nullptr));
    form->addWidget(form_cell(box, tr("Kalınlık"), globalWidth_, nullptr, nullptr));
    form->addWidget(form_cell(box, tr("Saydamlık"), globalOpacity_, nullptr, nullptr));
    form->addStretch(1);

    return box;
}

// -------------------------------------------------------- the properties ----

QLabel* StyleDesigner::addGroup(QVBoxLayout* form, const QString& title)
{
    auto* heading = new QLabel(title, this);
    heading->setObjectName(QStringLiteral("groupCaption"));
    // Air above a heading, none below: the rows under it are its, the rows
    // above are someone else's.
    heading->setContentsMargins(0, 8, 0, 0);
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
    form->setSpacing(6);

    type_ = new QComboBox(box);
    fit_column(type_);
    // FILLED PER GEOMETRY, in `fillTypeChoices`. The type rides as item DATA
    // rather than as a position, because a filtered list and a fixed table cannot
    // both be indexed by the same number.
    connect(type_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
    QLabel* identity = addGroup(form, tr("KATMAN")); // ui-label
    identity->setContentsMargins(0, 0, 0, 0);        // first heading: nothing above it
    form->addWidget(form_cell(box, tr("Katman tipi"), type_, nullptr, nullptr));

    const auto unitCombo = [&] {
        auto* c = new QComboBox(box);
        fit_column(c);
        for (const core::Unit u : kUnits)
            c->addItem(QString::fromUtf8(core::unit_name(u)));
        connect(c, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
        return c;
    };

    const auto spin = [&](int max, int step) {
        auto* s = new QSpinBox(box);
        s->setRange(0, max);
        s->setSingleStep(step);
        connect(s, &QSpinBox::valueChanged, this, [this](int) { applyToSelected(); });
        return s;
    };

    const auto namedCombo = [&](auto items, auto namer) {
        auto* c = new QComboBox(box);
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
    width_->setSuffix(tr(" µm"));
    size_     = spin(1000000, 500);
    interval_ = spin(1000000, 500);
    spacingY_ = spin(1000000, 500);
    offset_   = spin(1000000, 100);
    phase_    = spin(1000000, 100);
    angle_    = spin(359, 5);
    opacity_  = spin(255, 5);
    angle_->setSuffix(tr("°"));

    text_              = new QLineEdit(box);
    const QString hint = tr("Sembolün kendi yazısı, örnek: TAKS"); // ui-label
    text_->setPlaceholderText(hint);
    connect(text_, &QLineEdit::textEdited, this, [this](const QString&) { applyToSelected(); });

    sizeUnit_     = unitCombo();
    intervalUnit_ = unitCombo();
    spacingYUnit_ = unitCombo();
    offsetUnit_   = unitCombo();
    phaseUnit_    = unitCombo();

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
    const std::vector<T> fills{T::SimpleFill, T::LinePatternFill, T::PointPatternFill,
                               T::SimpleMarker};
    const std::vector<T> markers{T::MarkerLine, T::PointPatternFill, T::CentroidFill,
                                 T::SimpleMarker};
    const std::vector<T> sized{T::MarkerLine,   T::HashLine,     T::PointPatternFill,
                               T::CentroidFill, T::SimpleMarker, T::RasterFill,
                               T::RasterMarker, T::RasterLine,   T::TextMarker};
    const std::vector<T> spaced{T::MarkerLine, T::HashLine, T::LinePatternFill, T::PointPatternFill,
                                T::RasterLine};

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

    // The lock, on every layer, because every layer can be the one the regulation
    // fixes. Listed against `everything` below so it never disappears.
    lock_ = new QCheckBox(box);
    lock_->setText(tr("bu katmanın rengini korur"));
    lock_->setToolTip(tr("MPYY bir lekesinin dolgusunu plancıya bırakır, sınırını ve " // ui-label
                         "glifini siyah basar. Kilitli bir katman, sembolün rengi "
                         "değiştiğinde kendi rengini korur."));
    connect(lock_, &QCheckBox::toggled, this, [this](bool on) {
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
    addProperty(form, nullptr, tr("Yerleşim"), placement_, nullptr, {T::MarkerLine, T::HashLine});

    QLabel* fillGroup = addGroup(form, tr("DOLGU")); // ui-label
    addProperty(form, fillGroup, tr("Dolgu rengi"), fill_, nullptr, fills);

    QLabel* edgeGroup = addGroup(form, tr("KENAR")); // ui-label
    addProperty(form, edgeGroup, tr("Çizgi rengi"), stroke_, nullptr, coloured);
    strokeLabel_ = properties_.back().label;
    addProperty(form, edgeGroup, tr("Kalınlık"), width_, nullptr, strokes);
    addProperty(form, edgeGroup, tr("Uç biçimi"), cap_, nullptr, {T::SimpleLine});
    addProperty(form, edgeGroup, tr("Birleşim"), join_, nullptr, {T::SimpleLine});

    QLabel* geometryGroup = addGroup(form, tr("GEOMETRİ")); // ui-label
    addProperty(form, geometryGroup, tr("Boyut"), size_, sizeUnit_, sized);
    addProperty(form, geometryGroup, tr("Aralık"), interval_, intervalUnit_, spaced);
    addProperty(form, geometryGroup, tr("İkinci eksen"), spacingY_, spacingYUnit_,
                {T::PointPatternFill});
    addProperty(form, geometryGroup, tr("Kaydırma"), offset_, offsetUnit_,
                {T::SimpleLine, T::MarkerLine, T::HashLine, T::RasterLine, T::TextMarker});
    addProperty(form, geometryGroup, tr("Açı"), angle_, nullptr, angled);
    addProperty(form, geometryGroup, tr("Faz"), phase_, phaseUnit_, phased);

    // Opacity is read by every type, so it lists them all and is always shown.
    QLabel* visibilityGroup = addGroup(form, tr("GÖRÜNÜRLÜK")); // ui-label
    addProperty(form, visibilityGroup, tr("Saydamlık"), opacity_, nullptr, everything);
    addProperty(form, visibilityGroup, tr("Renk kilidi"), lock_, nullptr, everything);
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
    // The row a new or duplicated layer lands on. The tree is drawn top first, so
    // the layer drawn LAST is the first child of the root.
    if (tree_->topLevelItemCount() == 0) return;
    QTreeWidgetItem* root = tree_->topLevelItem(0);
    tree_->setCurrentItem(root->childCount() > 0 ? root->child(0) : root);
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
    // Never from inside itself. Every editor in this dialog answers a change by
    // rebuilding the whole right-hand side, so one that is rewritten BY the
    // rebuild would otherwise ask for another one.
    if (refreshing_) return;
    const Held rebuilding(refreshing_);

    // A picked gallery row has not reached the document yet, so its image ids
    // belong to the shelf. After a document style is loaded or edited, ids belong
    // to the drawing as usual. The preview must follow that ownership boundary.
    const core::ImageStore& images = galleryCode_.isEmpty()
                                         ? controller_.document().images()
                                         : controller_.bus().style_library().images();
    const core::DashStore& dashes  = galleryCode_.isEmpty()
                                         ? controller_.document().dashes()
                                         : controller_.bus().style_library().dashes();
    const std::uint32_t paper      = palette().color(QPalette::Base).rgba();

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

        // THE ROOT IS THE SYMBOL. It carries no stack index, which is how every
        // reader here tells it from a layer, and its picture is the whole symbol
        // rather than any one part of it.
        auto* root = new QTreeWidgetItem;
        root->setText(0, tr("Sembol"));
        root->setToolTip(0, tr("Bütün sembole ait özellikler: birim, renk, "
                               "kalınlık, saydamlık"));
        root->setIcon(0, symbol_icon(symbol_, images, dashes, QSize(44, 26), paper, shape()));
        tree_->addTopLevelItem(root);

        QTreeWidgetItem* chosen = keep_root ? root : nullptr;

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
            if (sl.colour_locked) label += tr("   · rengi kilitli"); // ui-label

            auto* item = new QTreeWidgetItem;
            item->setText(0, label);
            item->setToolTip(
                0, QStringLiteral("%1  ·  %2")
                       .arg(label, QString::fromUtf8(core::symbol_layer_type_name(sl.type))));
            item->setIcon(0, symbol_icon(one, images, dashes, QSize(44, 26), paper, shape()));
            item->setData(0, Qt::UserRole, static_cast<int>(i));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, sl.enabled ? Qt::Checked : Qt::Unchecked);
            root->addChild(item);

            if (!keep_root && static_cast<int>(i) == keep) chosen = item;
        }

        root->setExpanded(true);
        tree_->setCurrentItem(chosen != nullptr ? chosen : root);
    }

    fitStackHeight();

    updatePreview();
    loadSelected();
}

void StyleDesigner::fitStackHeight()
{
    if (tree_ == nullptr) return;

    // The root plus one row per symbol layer, because the root is always shown
    // expanded. Measured from the font rather than from a constant: the row
    // height follows the application font, and a hard-coded one clips at any
    // other size.
    const int rows =
        std::clamp(static_cast<int>(symbol_.layers.size()) + 1, kStackRowsMin, kStackRowsMax);
    const int row_px = tree_->fontMetrics().height() + 12;
    tree_->setFixedHeight(row_px * rows + 4);
}

void StyleDesigner::updatePreview()
{
    const core::ImageStore& images = galleryCode_.isEmpty()
                                         ? controller_.document().images()
                                         : controller_.bus().style_library().images();

    const core::DashStore& dashes = galleryCode_.isEmpty()
                                        ? controller_.document().dashes()
                                        : controller_.bus().style_library().dashes();

    // A SWATCH, not a band. This used to render across the whole width of the
    // dialog at 88 px tall, which turned an area gösterim into a stripe of flat
    // colour a thousand pixels wide — the one control whose job is to show what
    // the symbol does, showing almost nothing. The reference draws a square, and
    // a square is what a fill, a hatch and a marker interval are read on.
    //
    // Rendered at the DEVICE ratio and on a checkerboard: a translucent fill over
    // a flat ground is indistinguishable from an opaque paler one, and half of
    // what a designer is judging here is exactly that.
    // A SQUARE, at the width `symbol_preview` needs to draw a corner. Below 120
    // the swatch straightens the run, and then the caption under it — the one
    // that promises a bend — describes a corner the picture is not drawing.
    // Square rather than the 168×104 band it was: beside the stack the column is
    // the stack's, and a fill, a hatch and a marker interval are read on a
    // square anyway.
    previewWidth_ = kSwatchSide;

    const QImage swatch =
        symbol_preview(symbol_, images, dashes, QSize(kSwatchSide, kSwatchSide),
                       (theme() == ThemeMode::Dark ? darkTokens() : lightTokens()).bgInput.rgba(),
                       shape(), PreviewGround::Checker, devicePixelRatioF());

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
        width_->setValue(sl.look.width_um);
        size_->setValue(sl.size.value);
        interval_->setValue(sl.interval.value);
        spacingY_->setValue(sl.spacing_y.value);
        offset_->setValue(sl.offset.value);
        phase_->setValue(sl.phase.value);
        angle_->setValue(sl.angle_udeg / 1000000);
        opacity_->setValue(sl.opacity);
    }

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
        p.label->setVisible(shown);
        p.editor->setVisible(shown);
        if (shown && p.group != nullptr) p.group->setVisible(true);
    }
}

void StyleDesigner::loadGlobal()
{
    if (globalUnit_ == nullptr) return;

    // The symbol's unit is the one its measures agree on; when they disagree the
    // FIRST layer's is shown, and the note below says so. Inventing a fourth
    // "mixed" entry would let a user pick it, which means nothing.
    const core::Unit unit =
        symbol_.layers.empty() ? core::Unit::Paper : symbol_.layers.front().size.unit;
    bool mixed = false;
    for (const core::SymbolLayer& l : symbol_.layers)
        if (l.size.unit != unit) mixed = true;

    for (int i = 0; i < globalUnit_->count(); ++i)
        if (globalUnit_->itemData(i).toInt() == static_cast<int>(unit))
            globalUnit_->setCurrentIndex(i);

    // The renderer row's three buttons say the same thing as the combo below,
    // and they must never disagree: both read the symbol, neither remembers.
    //
    // An exclusive group refuses to have nothing checked, and MIXED is exactly the
    // state that needs it — the layers disagree, so no single button is the
    // answer. Exclusivity is relaxed for the length of the write and restored
    // afterwards, which is Qt's own way of clearing a segmented control.
    unitGroup_->setExclusive(false);
    for (QPushButton* button : unitButtons_)
        button->setChecked(!mixed && button->property("unit").toInt() == static_cast<int>(unit));
    unitGroup_->setExclusive(true);

    QString note;
    switch (unit) {
    case core::Unit::Paper:
        note = tr("Paftaya ait ölçü. MPYY bir sınırın kalınlığını paftada milimetre " // ui-label
                  "verir ve o kalınlık 1/1000'de de 1/5000'de de aynıdır — ekranda "
                  "yakınlaştırdığınızda değişmez."); // ui-label
        break;
    case core::Unit::Ground:
        note = tr("Zemine ait ölçü. Orman deseninin sıklığı alana aittir; ölçekle "
                  "küçülmesine izin vermek okunur bir dokuyu gri bir lekeye "
                  "çevirir — çizimle birlikte büyür."); // ui-label
        break;
    case core::Unit::Pixel:
        note = tr("Ham ekran pikseli. Ne paftaya ne zemine bağlıdır; ekran "
                  "yardımcıları dışında ender kullanılır."); // ui-label
        break;
    }
    if (mixed)
        note = tr("Katmanlar farklı birimler kullanıyor; ilkininki gösteriliyor. "
                  "Burada bir seçim yapmak hepsini birden değiştirir.") // ui-label
               + QStringLiteral("\n") + note;
    globalUnitNote_->setText(note);

    show_colour(globalColour_, symbol_.primary().rgba);
    globalWidth_->setValue(symbol_.primary().width_um);
    globalOpacity_->setValue(symbol_.layers.empty() ? 255 : symbol_.layers.front().opacity);
}

void StyleDesigner::applyGlobal()
{
    if (loading_ || symbol_.layers.empty()) return;

    galleryCode_.clear();
    galleryPackage_.clear();

    const auto unit = static_cast<core::Unit>(globalUnit_->currentData().toInt());

    for (core::SymbolLayer& l : symbol_.layers) {
        // EVERY measure, not just the size. A symbol whose marker is read on
        // paper and whose interval is read on the ground would come apart the
        // moment the view moved, and the whole point of the symbol's own unit is
        // that it answers the question once for all of it. A measure that wants
        // its own unit still has the box beside it on the layer's own page.
        l.size.unit      = unit;
        l.interval.unit  = unit;
        l.spacing_y.unit = unit;
        l.offset.unit    = unit;

        l.opacity = static_cast<std::uint8_t>(globalOpacity_->value());

        if (l.colour_locked) continue; // a locked layer keeps its own weight too
        l.look.width_um  = globalWidth_->value();
        l.look.src_width = core::Source::Explicit;
    }

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

    sl.type      = static_cast<SymbolLayerType>(type_->currentData().isValid()
                                                    ? type_->currentData().toInt()
                                                    : static_cast<int>(default_type_for(shape())));
    sl.shape     = pick(kShapes, shape_);
    sl.placement = pick(kPlacements, placement_);
    sl.cap       = pick(kCaps, cap_);
    sl.join      = pick(kJoins, join_);

    sl.size      = core::Measure{size_->value(), pick(kUnits, sizeUnit_)};
    sl.interval  = core::Measure{interval_->value(), pick(kUnits, intervalUnit_)};
    sl.spacing_y = core::Measure{spacingY_->value(), pick(kUnits, spacingYUnit_)};
    sl.offset    = core::Measure{offset_->value(), pick(kUnits, offsetUnit_)};
    sl.phase     = core::Measure{phase_->value(), pick(kUnits, phaseUnit_)};

    // `sl.bindings` IS DELIBERATELY NOT WRITTEN HERE. The row that edited it is
    // gone from this dialog (see the note in `buildProperties`), so what the
    // layer already carries survives a round trip through the window instead of
    // being cleared by a control that is no longer on screen.
    sl.text = text_->text().toStdString();
    for (const core::SymbolBinding& b : sl.bindings)
        if (b.what == core::SymbolProperty::Text) sl.text.clear();
    sl.look.width_um  = width_->value();
    sl.look.src_width = core::Source::Explicit;
    sl.angle_udeg     = angle_->value() * 1000000;
    sl.opacity        = static_cast<std::uint8_t>(opacity_->value());

    refresh();
}

void StyleDesigner::addLayer()
{
    // THE TYPE THE GEOMETRY CAN ACTUALLY USE. A plain stroke is what a CAD entity
    // has always had and the least surprising default on a line or an area — but
    // on a POINT it draws nothing at all, so "add a layer" produced a row that
    // could not be seen and a panel of stroke properties that could not do
    // anything.
    galleryCode_.clear();
    galleryPackage_.clear();

    core::SymbolLayer fresh;
    fresh.type = default_type_for(shape());
    if (fresh.type == SymbolLayerType::SimpleMarker) {
        // A marker with no size is an invisible marker. Same default the canvas
        // gives an unstyled point, so adding a layer changes how the point is
        // drawn without changing how big it is.
        fresh.size           = core::Measure{render::kDefaultPointSizeUm, core::Unit::Paper};
        fresh.look.fill_rgba = fresh.look.rgba;
    }
    symbol_.layers.push_back(fresh);
    refresh();
    selectTopLayer(); // the tree is top first, so the new layer is the first child
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

    // Follow the LAYER, not the row: the list runs top first, so the row the layer
    // now occupies is counted from the other end.
    // Follow the LAYER, not the row: `refresh()` already put the selection back
    // on the stack index it had, and the move changed which index that is.
    if (tree_->topLevelItemCount() > 0) {
        QTreeWidgetItem* root = tree_->topLevelItem(0);
        for (int c = 0; c < root->childCount(); ++c)
            if (root->child(c)->data(0, Qt::UserRole).toInt() == to)
                tree_->setCurrentItem(root->child(c));
    }
}

// ------------------------------------------------------------- the exits ----

bool StyleDesigner::applyToDocument()
{
    if (layerName_.isEmpty() || symbol_.layers.empty()) return false;

    command::Bus& bus = controller_.bus();
    if (auto st = bus.begin_batch(tr("Katman stilini uygula").toStdString()); !st) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"),
                             QString::fromStdString(st.error().message));
        return false;
    }

    // One `STİL` per symbol layer, the first replacing and the rest appending.
    // Exactly the lines a script would write, because they ARE the lines a script
    // would write: the designer has no private road to the style column (Article
    // 1.1, 1.2), and that is what makes it teachable to the AI.
    bool first = true;
    QString failure;
    if (!galleryCode_.isEmpty()) {
        QString quotedLayer = layerName_;
        quotedLayer.replace('\\', QStringLiteral("\\\\"));
        quotedLayer.replace('"', QStringLiteral("\\\""));

        QString quotedPackage = galleryPackage_;
        if (quotedPackage.isEmpty()) {
            const std::string_view configured =
                controller_.bus().app_settings().get("core.stil.kutuphane").as_text();
            quotedPackage =
                QString::fromUtf8(configured.data(), static_cast<int>(configured.size()));
        }
        quotedPackage.replace('\\', QStringLiteral("\\\\"));
        quotedPackage.replace('"', QStringLiteral("\\\""));

        QString quotedCode = galleryCode_;
        quotedCode.replace('\\', QStringLiteral("\\\\"));
        quotedCode.replace('"', QStringLiteral("\\\""));

        auto applied =
            controller_.runLineResult(QStringLiteral("STİL katman=\"%1\" paket=\"%2\" kod=\"%3\"")
                                          .arg(quotedLayer, quotedPackage, quotedCode),
                                      command::Origin::Gui);
        if (!applied) failure = QString::fromStdString(applied.error().message);
        first = false;
    }

    for (const core::SymbolLayer& sl : symbol_.layers) {
        if (!galleryCode_.isEmpty()) break;
        if (!sl.enabled) continue; // switched off is not applied

        QString quotedLayer = layerName_;
        quotedLayer.replace('\\', QStringLiteral("\\\\"));
        quotedLayer.replace('"', QStringLiteral("\\\""));
        QString line =
            QStringLiteral("STİL katman=\"%1\" tip=%2")
                .arg(quotedLayer, QString::fromUtf8(core::symbol_layer_type_name(sl.type)));
        if (!first) line += QStringLiteral(" ekle=evet");
        first = false;

        // Every measure carries the unit its own combo shows. Sending only `birim`
        // reinterpreted three of the four in whatever unit the fourth happened to
        // be in, so a marker sized on the ground and spaced on paper came back as
        // something the user never asked for.
        const auto named = [](core::Unit u) { return QString::fromUtf8(core::unit_name(u)); };

        line += QStringLiteral(" birim=%1").arg(named(sl.size.unit));
        line += QStringLiteral(" renk=%1").arg(sl.look.rgba);
        line += QStringLiteral(" kalinlik=%1").arg(sl.look.width_um);
        line += QStringLiteral(" dolgu=%1").arg(sl.look.fill_rgba);
        line += QStringLiteral(" boyut=%1").arg(sl.size.value);
        line += QStringLiteral(" aralik=%1 aralik_birim=%2")
                    .arg(QString::number(sl.interval.value), named(sl.interval.unit));
        line += QStringLiteral(" aralik_y=%1 aralik_y_birim=%2")
                    .arg(QString::number(sl.spacing_y.value), named(sl.spacing_y.unit));
        line += QStringLiteral(" kaydirma=%1 kaydirma_birim=%2")
                    .arg(QString::number(sl.offset.value), named(sl.offset.unit));
        line += QStringLiteral(" aci=%1").arg(sl.angle_udeg);
        line += QStringLiteral(" saydamlik=%1").arg(sl.opacity);
        line +=
            QStringLiteral(" sekil=%1").arg(QString::fromUtf8(core::marker_shape_name(sl.shape)));
        line += QStringLiteral(" yerlesim=%1")
                    .arg(QString::fromUtf8(core::marker_placement_name(sl.placement)));
        if (!sl.text.empty()) {
            QString text = QString::fromStdString(sl.text);
            text.replace('\\', QStringLiteral("\\\\"));
            text.replace('"', QStringLiteral("\\\""));
            line += QStringLiteral(" yazi=\"%1\"").arg(text);
        }

        auto applied = controller_.runLineResult(line, command::Origin::Gui);
        if (!applied) {
            failure = QString::fromStdString(applied.error().message);
            break;
        }
    }

    if (first && failure.isEmpty()) {
        QString quotedLayer = layerName_;
        quotedLayer.replace('\\', QStringLiteral("\\\\"));
        quotedLayer.replace('"', QStringLiteral("\\\""));
        auto cleared = controller_.runLineResult(
            QStringLiteral("STİL katman=\"%1\" sifirla=evet").arg(quotedLayer),
            command::Origin::Gui);
        if (!cleared) failure = QString::fromStdString(cleared.error().message);
    }

    if (!failure.isEmpty()) {
        bus.abort_batch();
        QMessageBox::warning(this, tr("Stil uygulanamadı"), failure);
        return false;
    }

    const auto closed = bus.end_batch();
    if (!closed) {
        QMessageBox::warning(this, tr("Stil uygulanamadı"),
                             QString::fromStdString(closed.error().message));
        return false;
    }
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
    // ~/.config/KentOSCad on Linux, Application Support on macOS, AppData on
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
    const QString slug = name.trimmed().toLower().replace(
        QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    const QString path = dir.filePath(QStringLiteral("stiller/%1.json").arg(slug));

    QString json;
    json += QStringLiteral("{\n");
    json += QStringLiteral("  \"id\": \"kullanici-%1\",\n").arg(slug);
    json += QStringLiteral("  \"package_version\": \"1.0.0\",\n");
    json += QStringLiteral("  \"source\": \"Kullanıcı tanımlı — %1\",\n").arg(name.trimmed());
    json += QStringLiteral("  \"published\": \"%1\",\n")
                .arg(QDate::currentDate().toString(Qt::ISODate));
    json += QStringLiteral("  \"licence\": \"kullanıcı\",\n");
    json += QStringLiteral("  \"stiller\": [\n    {\n");
    json += QStringLiteral("      \"id\": \"%1\",\n").arg(slug);
    json += QStringLiteral("      \"ad\": \"%1\",\n").arg(name.trimmed());
    json += QStringLiteral("      \"bolum\": [\"KULLANICI\"],\n");
    json += QStringLiteral("      \"kaynak\": \"Stil tasarımcısı\",\n");
    json += QStringLiteral("      \"cizgi\": { \"renk\": \"#%1\" },\n")
                .arg(symbol_.primary().rgba, 8, 16, QLatin1Char('0'));
    json += QStringLiteral("      \"dolgu\": { \"renk\": \"#%1\" }\n")
                .arg(symbol_.primary().fill_rgba, 8, 16, QLatin1Char('0'));
    json += QStringLiteral("    }\n  ]\n}\n");

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(json.toUtf8()) != json.toUtf8().size() ||
        !file.commit()) {
        QMessageBox::warning(this, tr("Kaydedilemedi"), tr("Dosya yazılamadı: %1").arg(path));
        return;
    }

    const QString done = tr("Gösterim kaydedildi:\n%1\n\nRafa almak için:"); // ui-label
    QMessageBox::information(this, tr("Kaydedildi"),
                             done.arg(path) + QStringLiteral("\nSEMBOL paket=\"%1\"").arg(path));
}

} // namespace kentos::app
