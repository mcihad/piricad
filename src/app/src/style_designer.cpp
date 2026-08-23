// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/style_designer.hpp"

#include "piricad/app/controller.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/core/document.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
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
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

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

constexpr std::array<TypeRow, 11> kTypes{{
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

/// The symbol a layer currently draws: what its entities carry, or its default.
core::Symbol symbol_of_layer(const core::Document& doc, core::LayerId layer)
{
    const auto& entities = doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e) || entities.layer[e] != layer) continue;
        const core::StyleId sid = entities.style[e];
        if (sid != core::kByLayerStyle && doc.styles().contains(sid))
            return doc.styles().symbol_at(sid);
        break;
    }

    const core::Layer* record = doc.layer(layer);
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
    resize(1020, 680);

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
    geometry_ = new QTabBar(this);
    geometry_->addTab(tr("Alan"));
    geometry_->addTab(tr("Çizgi"));
    geometry_->addTab(tr("Nokta"));
    geometry_->setCurrentIndex(static_cast<int>(natural_shape(symbol_)));
    connect(geometry_, &QTabBar::currentChanged, this, [this](int) {
        refreshGalleryItems();
        refresh();
    });

    preview_ = new QLabel(this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumHeight(96);
    preview_->setFrameShape(QFrame::StyledPanel);

    auto* top = new QVBoxLayout;
    top->addWidget(geometry_);
    top->addWidget(preview_);

    // ---- the two panes ----
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(buildGallery());

    auto* right       = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(buildStack(), 1);
    rightLayout->addWidget(buildProperties(), 2);
    split->addWidget(right);

    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    // ---- buttons ----
    auto* buttons      = new QDialogButtonBox(this);
    QPushButton* apply = buttons->addButton(tr("Uygula"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Vazgeç"), QDialogButtonBox::RejectRole);
    QPushButton* save = buttons->addButton(tr("Kütüphaneye kaydet…"), QDialogButtonBox::ActionRole);
    QPushButton* reset = buttons->addButton(tr("Sıfırla"), QDialogButtonBox::ResetRole);
    reset->setToolTip(tr("Katmanın şu an çizdiğine geri döner"));
    connect(reset, &QPushButton::clicked, this, &StyleDesigner::resetToLayer);
    apply->setDefault(true);

    connect(save, &QPushButton::clicked, this, &StyleDesigner::saveToLibrary);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        applyToDocument();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(top);
    root->addWidget(split, 1);
    root->addWidget(buttons);

    refreshGalleryTree();
    refreshGalleryItems();
    refresh();
    stack_->setCurrentRow(0);
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
    groups_->setMinimumHeight(150);
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

    auto* use = new QPushButton(tr("Seçileni al"), box);
    connect(use, &QPushButton::clicked, this, &StyleDesigner::applyGalleryPick);

    layout->addWidget(groups_, 1);
    layout->addWidget(search_);
    layout->addWidget(gallery_, 2);
    layout->addWidget(galleryNote_);
    layout->addWidget(provenance_);
    layout->addWidget(use);
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

    if (shelf.empty()) {
        galleryNote_->setText(
            tr("Raf boş. 'SEMBOL paket=<yol>' ile bir gösterim paketi yükleyin.")); // ui-label
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
        item->setIcon(symbol_icon(e->symbol, images, QSize(56, 40), paper, shape_of(e->kind)));
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
    provenance_->setText(QString::fromStdString(e->source_ref));
    provenance_->setToolTip(QString::fromStdString(e->id));
}

void StyleDesigner::resetToLayer()
{
    symbol_ = original_;
    geometry_->setCurrentIndex(static_cast<int>(natural_shape(symbol_)));
    refresh();
    stack_->setCurrentRow(0);
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
    symbol_ = e->symbol;
    geometry_->setCurrentIndex(static_cast<int>(shape_of(e->kind)));
    refresh();
    stack_->setCurrentRow(0);
}

// ------------------------------------------------------------- the stack ----

QWidget* StyleDesigner::buildStack()
{
    auto* box    = new QGroupBox(tr("Sembol katmanları — üstteki en son çizilir"), this);
    auto* layout = new QVBoxLayout(box);

    stack_ = new QListWidget(box);
    stack_->setIconSize(QSize(44, 26));
    connect(stack_, &QListWidget::currentRowChanged, this, [this](int) { loadSelected(); });
    connect(stack_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (loading_ || item == nullptr) return;

        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= static_cast<int>(symbol_.layers.size())) return;

        symbol_.layers[at(index)].enabled = item->checkState() == Qt::Checked;
        refresh();
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
    // for them elsewhere. Scoped to the LIST so they do not fire while a spin box
    // has focus and the user is deleting a digit.
    auto* remove = new QShortcut(QKeySequence::Delete, stack_);
    remove->setContext(Qt::WidgetShortcut);
    connect(remove, &QShortcut::activated, this, &StyleDesigner::removeLayer);

    auto* copy = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), stack_);
    copy->setContext(Qt::WidgetShortcut);
    connect(copy, &QShortcut::activated, this, &StyleDesigner::duplicateLayer);

    layout->addWidget(stack_, 1);
    layout->addLayout(bar);
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
    const std::vector<T> sized{T::MarkerLine,   T::HashLine,   T::PointPatternFill, T::CentroidFill,
                               T::SimpleMarker, T::RasterFill, T::RasterMarker,     T::RasterLine};
    const std::vector<T> spaced{T::MarkerLine, T::HashLine, T::LinePatternFill, T::PointPatternFill,
                                T::RasterLine};
    const std::vector<T> angled{T::MarkerLine,       T::HashLine,     T::LinePatternFill,
                                T::PointPatternFill, T::SimpleMarker, T::RasterFill};

    addProperty(form, tr("Çizgi rengi"), stroke_, nullptr, strokes);
    addProperty(form, tr("Çizgi kalınlığı (µm)"), width_, nullptr, strokes);
    addProperty(form, tr("Dolgu rengi"), fill_, nullptr, fills);
    addProperty(form, tr("Boyut"), size_, sizeUnit_, sized);
    addProperty(form, tr("Aralık"), interval_, intervalUnit_, spaced);
    addProperty(form, tr("İkinci eksen"), spacingY_, spacingYUnit_, {T::PointPatternFill});
    addProperty(form, tr("Kaydırma"), offset_, offsetUnit_,
                {T::SimpleLine, T::MarkerLine, T::HashLine, T::RasterLine});
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

    return box;
}

// ------------------------------------------------------------- the model ----

int StyleDesigner::currentLayer() const
{
    // Through the item's stored index, NOT through the row. The list is drawn top
    // first — the layer drawn last is the one seen on top — so a row and a stack
    // index run in opposite directions.
    const QListWidgetItem* item = stack_->currentItem();
    if (item == nullptr) return -1;

    const int index = item->data(Qt::UserRole).toInt();
    return index >= 0 && index < static_cast<int>(symbol_.layers.size()) ? index : -1;
}

void StyleDesigner::refresh()
{
    const core::ImageStore& images = controller_.document().images();
    const std::uint32_t paper      = palette().color(QPalette::Base).rgba();

    const int keep = stack_->currentRow();

    loading_ = true;
    stack_->clear();

    for (std::size_t i = symbol_.layers.size(); i-- > 0;) {
        const core::SymbolLayer& sl = symbol_.layers[i];

        core::Symbol one;
        one.layers.push_back(sl);
        one.layers.front().enabled = true; // the row shows what it WOULD draw

        // The Turkish name on the row, the machine name in the tooltip: the row is
        // read at a glance and the token is what a script would write.
        QString label = QString::fromUtf8(core::symbol_layer_type_name(sl.type));
        for (const TypeRow& row : kTypes)
            if (row.type == sl.type) label = tr(row.label);

        auto* item = new QListWidgetItem(stack_);
        item->setText(label);
        item->setToolTip(QStringLiteral("%1  ·  %2")
                             .arg(label, QString::fromUtf8(core::symbol_layer_type_name(sl.type))));
        item->setIcon(symbol_icon(one, images, QSize(44, 26), paper, shape()));
        item->setData(Qt::UserRole, static_cast<int>(i));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(sl.enabled ? Qt::Checked : Qt::Unchecked);
    }
    loading_ = false;

    if (keep >= 0 && keep < stack_->count()) stack_->setCurrentRow(keep);

    const QImage whole = symbol_preview(
        symbol_, images, QSize(std::max(160, preview_->width() - 8), 84), paper, shape());
    preview_->setPixmap(QPixmap::fromImage(whole));

    loadSelected();
}

void StyleDesigner::loadSelected()
{
    const int i     = currentLayer();
    const bool have = i >= 0;

    loading_ = true;

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

    type_->setEnabled(have);
    for (const Property& p : properties_) {
        const bool shown = have && std::find(p.types.begin(), p.types.end(), type) != p.types.end();
        p.label->setVisible(shown);
        p.editor->setVisible(shown);
    }

    loading_ = false;
}

void StyleDesigner::applyToSelected()
{
    if (loading_) return;

    const int i = currentLayer();
    if (i < 0) return;

    core::SymbolLayer& sl = symbol_.layers[at(i)];

    sl.type      = kTypes[at(type_->currentIndex())].type;
    sl.shape     = kShapes[at(shape_->currentIndex())];
    sl.placement = kPlacements[at(placement_->currentIndex())];
    sl.cap       = kCaps[at(cap_->currentIndex())];
    sl.join      = kJoins[at(join_->currentIndex())];

    sl.size      = core::Measure{size_->value(), kUnits[at(sizeUnit_->currentIndex())]};
    sl.interval  = core::Measure{interval_->value(), kUnits[at(intervalUnit_->currentIndex())]};
    sl.spacing_y = core::Measure{spacingY_->value(), kUnits[at(spacingYUnit_->currentIndex())]};
    sl.offset    = core::Measure{offset_->value(), kUnits[at(offsetUnit_->currentIndex())]};

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
    symbol_.layers.push_back(core::SymbolLayer{});
    refresh();
    stack_->setCurrentRow(0); // the list is top first, so the new layer is row 0
}

void StyleDesigner::duplicateLayer()
{
    const int i = currentLayer();
    if (i < 0) return;

    symbol_.layers.push_back(symbol_.layers[at(i)]);
    refresh();
    stack_->setCurrentRow(0);
}

void StyleDesigner::removeLayer()
{
    const int i = currentLayer();
    if (i < 0 || symbol_.layers.size() <= 1) return; // a symbol with no layer draws nothing

    symbol_.layers.erase(symbol_.layers.begin() + static_cast<std::ptrdiff_t>(at(i)));
    refresh();
}

void StyleDesigner::moveLayer(int delta)
{
    const int i = currentLayer();
    if (i < 0) return;

    const int to = i + delta;
    if (to < 0 || to >= static_cast<int>(symbol_.layers.size())) return;

    std::swap(symbol_.layers[at(i)], symbol_.layers[at(to)]);
    refresh();

    // Follow the LAYER, not the row: the list runs top first, so the row the layer
    // now occupies is counted from the other end.
    stack_->setCurrentRow(static_cast<int>(symbol_.layers.size()) - 1 - to);
}

// ------------------------------------------------------------- the exits ----

void StyleDesigner::applyToDocument()
{
    if (layerName_.isEmpty() || symbol_.layers.empty()) return;

    // One `STİL` per symbol layer, the first replacing and the rest appending.
    // Exactly the lines a script would write, because they ARE the lines a script
    // would write: the designer has no private road to the style column (Article
    // 1.1, 1.2), and that is what makes it teachable to the AI.
    bool first = true;
    for (const core::SymbolLayer& sl : symbol_.layers) {
        if (!sl.enabled) continue; // switched off is not applied

        QString line =
            QStringLiteral("STİL katman=\"%1\" tip=%2")
                .arg(layerName_, QString::fromUtf8(core::symbol_layer_type_name(sl.type)));
        if (!first) line += QStringLiteral(" ekle=evet");
        first = false;

        line += QStringLiteral(" birim=%1").arg(QString::fromUtf8(core::unit_name(sl.size.unit)));
        line += QStringLiteral(" renk=%1").arg(sl.look.rgba);
        line += QStringLiteral(" kalinlik=%1").arg(sl.look.width_um);
        line += QStringLiteral(" dolgu=%1").arg(sl.look.fill_rgba);
        line += QStringLiteral(" boyut=%1").arg(sl.size.value);
        line += QStringLiteral(" aralik=%1").arg(sl.interval.value);
        line += QStringLiteral(" aralik_y=%1").arg(sl.spacing_y.value);
        line += QStringLiteral(" kaydirma=%1").arg(sl.offset.value);
        line += QStringLiteral(" aci=%1").arg(sl.angle_udeg);
        line += QStringLiteral(" saydamlik=%1").arg(sl.opacity);
        line +=
            QStringLiteral(" sekil=%1").arg(QString::fromUtf8(core::marker_shape_name(sl.shape)));
        line += QStringLiteral(" yerlesim=%1")
                    .arg(QString::fromUtf8(core::marker_placement_name(sl.placement)));

        controller_.runLine(line, command::Origin::Gui);
    }
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
