// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/style_designer.hpp"

#include "piricad/app/controller.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/core/document.hpp"

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
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
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

#include <algorithm>
#include <array>
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
    {SymbolLayerType::LinePatternFill, "Çizgi desen dolgu"},
    {SymbolLayerType::PointPatternFill, "Nokta desen dolgu"},
    {SymbolLayerType::RasterFill, "Görsel dolgu"},
    {SymbolLayerType::CentroidFill, "Merkez işaretçi"},
    {SymbolLayerType::SimpleMarker, "İşaretçi"},
    {SymbolLayerType::RasterMarker, "Görsel işaretçi"},
    {SymbolLayerType::TextMarker, "Yazı"},
}};

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

/// Paints a button's face with the colour it stands for.
void show_colour(QToolButton* button, std::uint32_t rgba)
{
    QPixmap swatch(40, 18);
    swatch.fill(rgba == 0 ? Qt::transparent : from_rgba(rgba));

    QPainter painter(&swatch);
    painter.setPen(QPen(QColor(0, 0, 0, 90)));
    painter.drawRect(0, 0, swatch.width() - 1, swatch.height() - 1);
    // "No fill" has to look like nothing rather than like white, which is a colour
    // a plan sheet uses.
    if (rgba == 0) painter.drawLine(0, swatch.height() - 1, swatch.width() - 1, 0);
    painter.end();

    button->setIcon(QIcon(swatch));
    button->setIconSize(swatch.size());
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

PreviewShape shape_of(core::SymbolKind kind)
{
    switch (kind) {
    case core::SymbolKind::Area: return PreviewShape::Area;
    case core::SymbolKind::Line: return PreviewShape::Line;
    case core::SymbolKind::Point: return PreviewShape::Point;
    }
    return PreviewShape::Area;
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
    : QDialog(parent), controller_(controller), layerName_(std::move(layerName))
{
    setWindowTitle(tr("Stil tasarımcısı — %1").arg(layerName_));
    setModal(true);
    setMinimumSize(980, 660);
    resize(1120, 740);

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
    geometry_->setCurrentIndex(static_cast<int>(natural_shape(symbol_)));
    connect(geometry_, &QTabBar::currentChanged, this, [this](int) {
        refreshGalleryItems();
        refresh();
        updateHeaderNote();
    });

    auto* previewFrame = new QFrame(this);
    previewFrame->setObjectName(QStringLiteral("stylePreview"));
    auto* previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(14, 11, 14, 12);
    previewLayout->setSpacing(7);

    auto* previewTitle = new QLabel(tr("Katman ön izlemesi — %1").arg(layerName_), previewFrame);
    previewTitle->setObjectName(QStringLiteral("sectionTitle"));

    // Said, not left to be inferred: the tab above chose the geometry this
    // preview is drawn on, and a user who does not connect the two reads the
    // picture as a claim about their parcels.
    headerNote_ = new QLabel(previewFrame);
    headerNote_->setObjectName(QStringLiteral("quiet"));

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->addWidget(previewTitle);
    titleRow->addStretch(1);
    titleRow->addWidget(headerNote_);

    preview_ = new QLabel(previewFrame);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumHeight(96);
    preview_->setObjectName(QStringLiteral("stylePreviewImage"));
    preview_->setAccessibleName(tr("Katman stili ön izlemesi"));
    preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    preview_->installEventFilter(this);
    previewLayout->addLayout(titleRow);
    previewLayout->addWidget(preview_);

    // No gap between the bar and the pane: the selected tab has to touch what it
    // opens, or the two are just a row of buttons above a box.
    auto* tabRow = new QHBoxLayout;
    tabRow->setContentsMargins(1, 0, 0, 0);
    tabRow->setSpacing(0);
    tabRow->addWidget(geometry_);
    tabRow->addStretch(1);

    auto* top = new QVBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(0);
    top->addLayout(tabRow);
    top->addWidget(previewFrame);

    // ---- the two panes ----
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(buildGallery());

    auto* right       = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    rightLayout->addWidget(buildTree(), 2);

    // TWO PAGES, one selection. Selecting the symbol shows what belongs to all of
    // it; selecting a layer shows what belongs to that layer. Showing both at once
    // is what makes a symbol editor confusing — a user cannot tell which colour
    // they are about to change.
    pages_ = new QStackedWidget(this);
    pages_->addWidget(buildGlobal());
    pages_->addWidget(buildProperties());
    rightLayout->addWidget(pages_, 3);
    split->addWidget(right);

    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    split->setSizes({430, 650});
    split->setChildrenCollapsible(false);
    split->setHandleWidth(10);

    // ---- buttons ----
    auto* buttons      = new QDialogButtonBox(this);
    QPushButton* apply = buttons->addButton(tr("Uygula"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Vazgeç"), QDialogButtonBox::RejectRole);
    QPushButton* save = buttons->addButton(tr("Kütüphaneye kaydet…"), QDialogButtonBox::ActionRole);
    QPushButton* reset = buttons->addButton(tr("Sıfırla"), QDialogButtonBox::ResetRole);
    reset->setToolTip(tr("Katmanın şu an çizdiğine geri döner"));
    connect(reset, &QPushButton::clicked, this, &StyleDesigner::resetToLayer);
    apply->setDefault(true);
    apply->setObjectName(QStringLiteral("primary"));
    apply->setToolTip(tr("Sembolü STİL komutlarına çevirip katmana yazar"));
    save->setToolTip(tr("Sembolü kendi gösterim paketiniz olarak diske yazar")); // ui-label

    connect(save, &QPushButton::clicked, this, &StyleDesigner::saveToLibrary);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (applyToDocument()) accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);
    root->addLayout(top);
    root->addWidget(split, 1);
    root->addWidget(buttons);

    // The look, in one place. Written against PALETTE ROLES rather than fixed
    // colours so a dark desktop theme gets a dark dialog: this window is opened
    // from a canvas people keep dark all day, and a sheet of hard-coded #f0f0f0
    // in the middle of it is the thing that makes an application feel bolted
    // together.
    setStyleSheet(QStringLiteral(R"(
        QTabBar::tab {
            padding: 7px 20px 8px 20px;
            margin-right: 3px;
            border: 1px solid palette(mid);
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            background: palette(window);
            color: palette(text);
        }
        QTabBar::tab:!selected  { margin-top: 4px; background: palette(alternate-base); }
        QTabBar::tab:hover:!selected { background: palette(midlight); }
        QTabBar::tab:selected   { margin-bottom: -1px; padding-bottom: 9px;
                                  background: palette(base); font-weight: 600; }
        QTabBar::tab:focus      { border-color: palette(highlight); }

        QFrame#stylePreview {
            border: 1px solid palette(mid);
            border-radius: 6px;
            border-top-left-radius: 0px;
            background: palette(base);
        }
        QLabel#stylePreviewImage {
            background: palette(window);
            border: 1px solid palette(midlight);
            border-radius: 4px;
        }
        QLabel#sectionTitle { font-weight: 600; }
        QLabel#quiet        { color: palette(dark); }

        QGroupBox {
            border: 1px solid palette(mid);
            border-radius: 6px;
            margin-top: 9px;
            padding: 10px 10px 9px 10px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 5px;
        }

        QListWidget::item { padding: 4px; border-radius: 4px; }
        QPushButton#primary { font-weight: 600; }
    )"));

    refreshGalleryTree();
    refreshGalleryItems();
    refresh();
    selectTopLayer();
    updateHeaderNote();
}

void StyleDesigner::updateHeaderNote()
{
    // Named in the user's own words, because "Alan" on a tab is a noun and what
    // they need to know is what it does to this window.
    QString note;
    switch (shape()) {
    case PreviewShape::Area:
        note = tr("Kapalı alan üzerinde çiziliyor"); // ui-label
        break;
    case PreviewShape::Line:
        note = tr("Kırıklı çizgi üzerinde çiziliyor"); // ui-label
        break;
    case PreviewShape::Point:
        note = tr("Tek nokta üzerinde çiziliyor"); // ui-label
        break;
    }
    headerNote_->setText(note);
}

PreviewShape StyleDesigner::shape() const
{
    return static_cast<PreviewShape>(std::clamp(geometry_->currentIndex(), 0, 2));
}

// ------------------------------------------------------------- the shelf ----

QWidget* StyleDesigner::buildGallery()
{
    auto* box    = new QGroupBox(tr("Hazır gösterimler"), this); // ui-label
    auto* layout = new QVBoxLayout(box);

    groups_ = new QTreeWidget(box);
    groups_->setHeaderHidden(true);
    groups_->setMinimumHeight(132);
    connect(groups_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem*, QTreeWidgetItem*) { refreshGalleryItems(); });

    search_ = new QLineEdit(box);
    search_->setPlaceholderText(tr("Ara — etiket, kimlik ve grup yolu"));
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this,
            [this](const QString&) { refreshGalleryItems(); });

    gallery_ = new QListWidget(box);
    gallery_->setViewMode(QListView::IconMode);
    gallery_->setIconSize(QSize(56, 40));
    gallery_->setGridSize(QSize(88, 82));
    gallery_->setResizeMode(QListView::Adjust);
    gallery_->setMovement(QListView::Static);
    gallery_->setWordWrap(true);
    gallery_->setSpacing(4);
    gallery_->setAccessibleName(tr("Hazır gösterim galerisi")); // ui-label
    connect(gallery_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { applyGalleryPick(); });

    galleryNote_ = new QLabel(box);
    galleryNote_->setWordWrap(true);
    QFont small = galleryNote_->font();
    small.setPointSizeF(small.pointSizeF() - 1.0);
    galleryNote_->setFont(small);

    provenance_ = new QLabel(box);
    provenance_->setWordWrap(true);
    provenance_->setFont(small);
    provenance_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    provenance_->setMinimumHeight(46);

    connect(gallery_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem*, QListWidgetItem*) { showProvenance(); });

    use_ = new QPushButton(tr("Seçileni kullan"), box);
    use_->setToolTip(tr("Seçili gösterimi düzenlenebilir sembol yığını olarak alır")); // ui-label
    connect(use_, &QPushButton::clicked, this, &StyleDesigner::applyGalleryPick);

    layout->addWidget(groups_, 1);
    layout->addWidget(search_);
    layout->addWidget(gallery_, 2);
    layout->addWidget(galleryNote_);
    layout->addWidget(provenance_);
    layout->addWidget(use_);
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
    provenance_->setVisible(stocked);
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
        item->setToolTip(QString::fromStdString(e->id + "\n" + e->source_ref));
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
        return;
    }

    const core::LibraryEntry* e =
        controller_.bus().style_library().find(item->data(Qt::UserRole).toString().toStdString());
    if (e == nullptr) {
        provenance_->clear();
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
    auto* box    = new QGroupBox(tr("Sembol — üstteki en son çizilir"), this);
    auto* layout = new QVBoxLayout(box);

    tree_ = new QTreeWidget(box);
    tree_->setHeaderHidden(true);
    tree_->setIconSize(QSize(44, 26));
    tree_->setRootIsDecorated(true);
    tree_->setMinimumHeight(190);
    tree_->setAlternatingRowColors(true);
    tree_->setAccessibleName(tr("Sembol katmanları"));

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

    const auto button = [&](const QString& text, const QString& tip, auto slot) {
        auto* b = new QToolButton(box);
        b->setText(text);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };

    auto* bar = new QHBoxLayout;
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
    auto* box  = new QGroupBox(tr("Sembol özellikleri"), this);
    auto* form = new QFormLayout(box);

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

    form->addRow(tr("Ölçü birimi"), globalUnit_);
    form->addRow(QString(), globalUnitNote_);
    form->addRow(tr("Renk"), globalColour_);
    form->addRow(tr("Çizgi kalınlığı"), globalWidth_);
    form->addRow(tr("Saydamlık (0-255)"), globalOpacity_);

    return box;
}

// -------------------------------------------------------- the properties ----

void StyleDesigner::addProperty(QFormLayout* form, const QString& label, QWidget* editor,
                                QWidget* unit, std::vector<SymbolLayerType> types)
{
    auto* text   = new QLabel(label, this);
    QWidget* row = editor;

    if (unit != nullptr) {
        row          = new QWidget(this);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(editor, 3);
        layout->addWidget(unit, 2);
    }

    form->addRow(text, row);
    properties_.push_back(Property{text, row, unit, std::move(types)});
}

QWidget* StyleDesigner::buildProperties()
{
    auto* box  = new QGroupBox(tr("Katman özellikleri"), this);
    auto* form = new QFormLayout(box);

    type_ = new QComboBox(box);
    for (const TypeRow& row : kTypes)
        type_->addItem(
            QStringLiteral("%1  (%2)")
                .arg(tr(row.label), QString::fromUtf8(core::symbol_layer_type_name(row.type))));
    connect(type_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
    form->addRow(tr("Katman tipi"), type_);

    const auto unitCombo = [&] {
        auto* c = new QComboBox(box);
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
        for (const auto value : items)
            c->addItem(QString::fromUtf8(namer(value)));
        connect(c, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
        return c;
    };

    stroke_ = new QToolButton(box);
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

    width_    = spin(100000, 100);
    size_     = spin(1000000, 500);
    interval_ = spin(1000000, 500);
    spacingY_ = spin(1000000, 500);
    offset_   = spin(1000000, 100);
    angle_    = spin(359, 5);
    opacity_  = spin(255, 5);

    text_              = new QLineEdit(box);
    const QString hint = tr("Sembolün kendi yazısı, örnek: TAKS"); // ui-label
    text_->setPlaceholderText(hint);
    connect(text_, &QLineEdit::textEdited, this, [this](const QString&) { applyToSelected(); });

    sizeUnit_     = unitCombo();
    intervalUnit_ = unitCombo();
    spacingYUnit_ = unitCombo();
    offsetUnit_   = unitCombo();

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
    const std::vector<T> angled{T::MarkerLine,       T::HashLine,     T::LinePatternFill,
                                T::PointPatternFill, T::SimpleMarker, T::RasterFill};

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
    lock_->setText(tr("Sembolün rengi bu katmanı değiştirmesin"));
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

    addProperty(form, tr("Yazı"), text_, nullptr, {T::TextMarker});
    addProperty(form, tr("Çizgi rengi"), stroke_, nullptr, coloured);
    strokeLabel_ = properties_.back().label;
    addProperty(form, tr("Çizgi kalınlığı (µm)"), width_, nullptr, strokes);
    addProperty(form, tr("Dolgu rengi"), fill_, nullptr, fills);
    addProperty(form, tr("Boyut"), size_, sizeUnit_, sized);
    addProperty(form, tr("Aralık"), interval_, intervalUnit_, spaced);
    addProperty(form, tr("İkinci eksen"), spacingY_, spacingYUnit_, {T::PointPatternFill});
    addProperty(form, tr("Kaydırma"), offset_, offsetUnit_,
                {T::SimpleLine, T::MarkerLine, T::HashLine, T::RasterLine, T::TextMarker});
    addProperty(form, tr("Açı (°)"), angle_, nullptr, angled);
    addProperty(form, tr("Şekil"), shape_, nullptr, markers);
    addProperty(form, tr("Yerleşim"), placement_, nullptr, {T::MarkerLine, T::HashLine});
    addProperty(form, tr("Uç biçimi"), cap_, nullptr, {T::SimpleLine});
    addProperty(form, tr("Birleşim"), join_, nullptr, {T::SimpleLine});

    // Opacity is read by every type, so it lists them all and is always shown.
    std::vector<T> everything;
    for (const TypeRow& row : kTypes)
        everything.push_back(row.type);
    addProperty(form, tr("Saydamlık (0-255)"), opacity_, nullptr, everything);
    addProperty(form, tr("Renk kilidi"), lock_, nullptr, everything);

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

    updatePreview();
    loadSelected();
}

void StyleDesigner::updatePreview()
{
    const core::ImageStore& images = galleryCode_.isEmpty()
                                         ? controller_.document().images()
                                         : controller_.bus().style_library().images();

    // Rendered at the width it is SHOWN at, not at whatever the label measured
    // while the dialog was still being laid out. A preview drawn once at 160 px
    // and then stretched across a 1100 px window is a picture of a symbol at the
    // wrong scale, and scale is most of what a hatch or a marker interval says.
    previewWidth_ = std::max(200, preview_->width() - 12);

    const core::DashStore& dashes = galleryCode_.isEmpty()
                                        ? controller_.document().dashes()
                                        : controller_.bus().style_library().dashes();

    const QImage whole = symbol_preview(symbol_, images, dashes, QSize(previewWidth_, 88),
                                        palette().color(QPalette::Base).rgba(), shape());
    preview_->setPixmap(QPixmap::fromImage(whole));
}

bool StyleDesigner::eventFilter(QObject* watched, QEvent* event)
{
    // Watched on the LABEL, not on the dialog. A dialog resize arrives before the
    // layout has handed the label its share of it, so a preview redrawn there is
    // redrawn at the old width and never corrected.
    //
    // Only when the width actually moved: `setPixmap` feeds the layout a new size
    // hint, and re-rendering on every pass of that would be a loop.
    if (watched == preview_ && event->type() == QEvent::Resize &&
        std::abs(preview_->width() - 12 - previewWidth_) > 2)
        updatePreview();

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

        for (int k = 0; k < static_cast<int>(kTypes.size()); ++k)
            if (kTypes[at(k)].type == sl.type) type_->setCurrentIndex(k);

        select(shape_, kShapes, sl.shape);
        select(placement_, kPlacements, sl.placement);
        select(cap_, kCaps, sl.cap);
        select(join_, kJoins, sl.join);
        select(sizeUnit_, kUnits, sl.size.unit);
        select(intervalUnit_, kUnits, sl.interval.unit);
        select(spacingYUnit_, kUnits, sl.spacing_y.unit);
        select(offsetUnit_, kUnits, sl.offset.unit);

        show_colour(stroke_, sl.look.rgba);
        show_colour(fill_, sl.look.fill_rgba);

        lock_->setChecked(sl.colour_locked);
        text_->setText(QString::fromStdString(sl.text));
        width_->setValue(sl.look.width_um);
        size_->setValue(sl.size.value);
        interval_->setValue(sl.interval.value);
        spacingY_->setValue(sl.spacing_y.value);
        offset_->setValue(sl.offset.value);
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
    for (const Property& p : properties_) {
        const bool shown = have && std::find(p.types.begin(), p.types.end(), type) != p.types.end();
        p.label->setVisible(shown);
        p.editor->setVisible(shown);
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

    sl.type      = pick(kTypes, type_).type;
    sl.shape     = pick(kShapes, shape_);
    sl.placement = pick(kPlacements, placement_);
    sl.cap       = pick(kCaps, cap_);
    sl.join      = pick(kJoins, join_);

    sl.size      = core::Measure{size_->value(), pick(kUnits, sizeUnit_)};
    sl.interval  = core::Measure{interval_->value(), pick(kUnits, intervalUnit_)};
    sl.spacing_y = core::Measure{spacingY_->value(), pick(kUnits, spacingYUnit_)};
    sl.offset    = core::Measure{offset_->value(), pick(kUnits, offsetUnit_)};

    sl.text           = text_->text().toStdString();
    sl.look.width_um  = width_->value();
    sl.look.src_width = core::Source::Explicit;
    sl.angle_udeg     = angle_->value() * 1000000;
    sl.opacity        = static_cast<std::uint8_t>(opacity_->value());

    refresh();
}

void StyleDesigner::addLayer()
{
    // A plain stroke, which is the layer a CAD entity has always had and the one a
    // user is least surprised to get.
    galleryCode_.clear();
    galleryPackage_.clear();
    symbol_.layers.push_back(core::SymbolLayer{});
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

} // namespace piricad::app
