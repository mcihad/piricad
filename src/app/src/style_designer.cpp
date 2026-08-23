// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/style_designer.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/app/symbol_preview.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/core/document.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
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
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>

namespace piricad::app {
namespace {

/// The symbol layer types offered, in the order the property side lists them.
///
/// Machine names, not translations: they are what `STİL tip=` takes, so what the
/// dialog shows is what a script would write. Turkish labels next to them because
/// the machine name is a token and a token is not a legend.
struct TypeRow
{
    core::SymbolLayerType type;
    const char* label;
};

constexpr std::array<TypeRow, 11> kTypes{{
    {core::SymbolLayerType::SimpleLine, "Çizgi"},
    {core::SymbolLayerType::MarkerLine, "İşaretçi çizgi"},
    {core::SymbolLayerType::HashLine, "Tarak çizgi"},
    {core::SymbolLayerType::SimpleFill, "Dolgu"},
    {core::SymbolLayerType::LinePatternFill, "Çizgi desen dolgu"},
    {core::SymbolLayerType::PointPatternFill, "Nokta desen dolgu"},
    {core::SymbolLayerType::CentroidFill, "Merkez işaretçi"},
    {core::SymbolLayerType::SimpleMarker, "İşaretçi"},
    {core::SymbolLayerType::RasterFill, "Görsel dolgu"},
    {core::SymbolLayerType::RasterMarker, "Görsel işaretçi"},
    {core::SymbolLayerType::RasterLine, "Görsel çizgi"},
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

QColor from_rgba(std::uint32_t rgba)
{
    return QColor::fromRgba(static_cast<QRgb>(rgba));
}

/// Paints a button's face with the colour it stands for.
void show_colour(QToolButton* button, std::uint32_t rgba)
{
    QPixmap swatch(36, 18);
    swatch.fill(rgba == 0 ? Qt::transparent : from_rgba(rgba));

    QPainter painter(&swatch);
    painter.setPen(QPen(QColor(0, 0, 0, 90)));
    painter.drawRect(0, 0, swatch.width() - 1, swatch.height() - 1);
    if (rgba == 0) {
        // "No fill" has to look like nothing rather than like white, which is a
        // colour a plan sheet uses.
        painter.drawLine(0, swatch.height() - 1, swatch.width() - 1, 0);
    }
    painter.end();

    button->setIcon(QIcon(swatch));
    button->setIconSize(swatch.size());
}

/// The stack index a valid `currentLayer()` names.
///
/// The list widget counts in `int` and a vector in `size_t`, so the conversion has
/// to happen somewhere; here, once, guarded by the caller having already checked
/// the row is valid.
std::size_t at(int row)
{
    return static_cast<std::size_t>(row);
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

} // namespace

StyleDesigner::StyleDesigner(Controller& controller, QString layerName, QWidget* parent)
    : QDialog(parent), controller_(controller), layerName_(std::move(layerName))
{
    setWindowTitle(tr("Stil tasarımcısı — %1").arg(layerName_));
    setModal(true);
    resize(760, 520);

    const core::LayerId layer = controller_.document().find_layer(layerName_.toStdString());
    symbol_                   = layer == core::kNoLayer ? core::Symbol::of(core::Appearance{})
                                                        : symbol_of_layer(controller_.document(), layer);
    if (symbol_.layers.empty()) symbol_ = core::Symbol::of(core::Appearance{});

    // ---- the whole symbol, big, at the top ----
    preview_ = new QLabel(this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumHeight(90);
    preview_->setFrameShape(QFrame::StyledPanel);

    // ---- the stack, left ----
    stack_ = new QListWidget(this);
    stack_->setIconSize(QSize(40, 24));
    connect(stack_, &QListWidget::currentRowChanged, this, [this](int) { loadSelected(); });

    auto* add = new QToolButton(this);
    add->setText(QStringLiteral("+"));
    add->setToolTip(tr("Katman ekle"));
    connect(add, &QToolButton::clicked, this, &StyleDesigner::addLayer);

    auto* remove = new QToolButton(this);
    remove->setText(QStringLiteral("−"));
    remove->setToolTip(tr("Katmanı sil"));
    connect(remove, &QToolButton::clicked, this, &StyleDesigner::removeLayer);

    auto* up = new QToolButton(this);
    up->setText(QStringLiteral("▲"));
    up->setToolTip(tr("Yukarı taşı — üstteki en son çizilir"));
    // The list runs top first, so moving a row UP means moving the layer LATER in
    // the stack. The arrow means what it points at, not what the vector does.
    connect(up, &QToolButton::clicked, this, [this] { moveLayer(+1); });

    auto* down = new QToolButton(this);
    down->setText(QStringLiteral("▼"));
    down->setToolTip(tr("Aşağı taşı"));
    connect(down, &QToolButton::clicked, this, [this] { moveLayer(-1); });

    auto* stackButtons = new QHBoxLayout;
    stackButtons->addWidget(add);
    stackButtons->addWidget(remove);
    stackButtons->addStretch(1);
    stackButtons->addWidget(up);
    stackButtons->addWidget(down);

    auto* stackBox    = new QGroupBox(tr("Sembol katmanları"), this);
    auto* stackLayout = new QVBoxLayout(stackBox);
    stackLayout->addWidget(stack_, 1);
    stackLayout->addLayout(stackButtons);

    // ---- the selected layer's properties, right ----
    type_ = new QComboBox(this);
    for (const TypeRow& row : kTypes)
        type_->addItem(
            QStringLiteral("%1  (%2)")
                .arg(tr(row.label), QString::fromUtf8(core::symbol_layer_type_name(row.type))));

    shape_ = new QComboBox(this);
    for (const core::MarkerShape s : kShapes)
        shape_->addItem(QString::fromUtf8(core::marker_shape_name(s)));

    placement_ = new QComboBox(this);
    for (const core::MarkerPlacement p : kPlacements)
        placement_->addItem(QString::fromUtf8(core::marker_placement_name(p)));

    unit_ = new QComboBox(this);
    for (const core::Unit u : kUnits)
        unit_->addItem(QString::fromUtf8(core::unit_name(u)));

    stroke_ = new QToolButton(this);
    stroke_->setToolTip(tr("Çizgi rengi"));
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

    fill_ = new QToolButton(this);
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

    const auto spin = [this](int max, const QString& suffix) {
        auto* box = new QSpinBox(this);
        box->setRange(0, max);
        box->setSuffix(suffix);
        box->setSingleStep(100);
        connect(box, &QSpinBox::valueChanged, this, [this](int) { applyToSelected(); });
        return box;
    };

    width_    = spin(100000, tr(" µm"));
    size_     = spin(1000000, QString());
    interval_ = spin(1000000, QString());
    spacingY_ = spin(1000000, QString());

    angle_ = new QSpinBox(this);
    angle_->setRange(0, 359);
    angle_->setSuffix(QStringLiteral("°"));
    connect(angle_, &QSpinBox::valueChanged, this, [this](int) { applyToSelected(); });

    opacity_ = new QSpinBox(this);
    opacity_->setRange(0, 255);
    connect(opacity_, &QSpinBox::valueChanged, this, [this](int) { applyToSelected(); });

    connect(type_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
    connect(shape_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
    connect(placement_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });
    connect(unit_, &QComboBox::currentIndexChanged, this, [this](int) { applyToSelected(); });

    auto* propertyBox = new QGroupBox(tr("Katman özellikleri"), this);
    auto* form        = new QFormLayout(propertyBox);
    form->addRow(tr("Tip"), type_);
    form->addRow(tr("Çizgi rengi"), stroke_);
    form->addRow(tr("Çizgi kalınlığı"), width_);
    form->addRow(tr("Dolgu rengi"), fill_);
    form->addRow(tr("Ölçü birimi"), unit_);
    form->addRow(tr("Boyut"), size_);
    form->addRow(tr("Aralık"), interval_);
    form->addRow(tr("İkinci eksen"), spacingY_);
    form->addRow(tr("Açı"), angle_);
    form->addRow(tr("Şekil"), shape_);
    form->addRow(tr("Yerleşim"), placement_);
    form->addRow(tr("Saydamlık"), opacity_);

    // ---- buttons ----
    auto* buttons     = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton* save = buttons->addButton(tr("Kütüphaneye kaydet…"), QDialogButtonBox::ActionRole);
    connect(save, &QPushButton::clicked, this, &StyleDesigner::saveToLibrary);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        applyToDocument();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* columns = new QHBoxLayout;
    columns->addWidget(stackBox, 2);
    columns->addWidget(propertyBox, 3);

    auto* root = new QVBoxLayout(this);
    root->addWidget(preview_);
    root->addLayout(columns, 1);
    root->addWidget(buttons);

    refresh();
    stack_->setCurrentRow(0);
}

int StyleDesigner::currentLayer() const
{
    // Through the item's stored index, NOT through the row. The list is drawn top
    // first — the layer drawn last is the one seen on top — so a row and a stack
    // index run in opposite directions, and reading one as the other edits the
    // mirrored layer.
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
    stack_->blockSignals(true);
    stack_->clear();

    // Listed TOP FIRST, because that is the order they are seen in: the last
    // layer is drawn last and therefore sits on top. A list in storage order
    // would put the thing the user sees at the bottom of the list.
    for (std::size_t i = symbol_.layers.size(); i-- > 0;) {
        const core::SymbolLayer& sl = symbol_.layers[i];

        core::Symbol one;
        one.layers.push_back(sl);

        auto* item = new QListWidgetItem(stack_);
        item->setText(QString::fromUtf8(core::symbol_layer_type_name(sl.type)));
        item->setIcon(
            symbol_icon(one, images, QSize(40, 24), paper,
                        core::draws_fill(sl.type) ? PreviewShape::Area : PreviewShape::Line));
        item->setData(Qt::UserRole, static_cast<int>(i));
    }
    stack_->blockSignals(false);

    if (keep >= 0 && keep < stack_->count()) stack_->setCurrentRow(keep);

    const QImage whole =
        symbol_preview(symbol_, images, QSize(static_cast<int>(preview_->width()) - 8, 80), paper,
                       PreviewShape::Area);
    preview_->setPixmap(QPixmap::fromImage(whole));

    loadSelected();
}

void StyleDesigner::loadSelected()
{
    const int i     = currentLayer();
    const bool have = i >= 0;

    loading_ = true;
    const std::array<QWidget*, 12> editors{type_,     shape_,    placement_, unit_,
                                           stroke_,   fill_,     width_,     size_,
                                           interval_, spacingY_, angle_,     opacity_};
    for (QWidget* w : editors)
        w->setEnabled(have);

    if (have) {
        const core::SymbolLayer& sl = symbol_.layers[at(i)];

        for (int k = 0; k < static_cast<int>(kTypes.size()); ++k)
            if (kTypes[at(k)].type == sl.type) type_->setCurrentIndex(k);
        for (int k = 0; k < static_cast<int>(kShapes.size()); ++k)
            if (kShapes[at(k)] == sl.shape) shape_->setCurrentIndex(k);
        for (int k = 0; k < static_cast<int>(kPlacements.size()); ++k)
            if (kPlacements[at(k)] == sl.placement) placement_->setCurrentIndex(k);
        for (int k = 0; k < static_cast<int>(kUnits.size()); ++k)
            if (kUnits[at(k)] == sl.size.unit) unit_->setCurrentIndex(k);

        show_colour(stroke_, sl.look.rgba);
        show_colour(fill_, sl.look.fill_rgba);

        width_->setValue(sl.look.width_um);
        size_->setValue(sl.size.value);
        interval_->setValue(sl.interval.value);
        spacingY_->setValue(sl.spacing_y.value);
        angle_->setValue(sl.angle_udeg / 1000000);
        opacity_->setValue(sl.opacity);
    }
    loading_ = false;
}

void StyleDesigner::applyToSelected()
{
    if (loading_) return;

    const int i = currentLayer();
    if (i < 0) return;

    core::SymbolLayer& sl = symbol_.layers[at(i)];

    sl.type      = kTypes[static_cast<std::size_t>(type_->currentIndex())].type;
    sl.shape     = kShapes[static_cast<std::size_t>(shape_->currentIndex())];
    sl.placement = kPlacements[static_cast<std::size_t>(placement_->currentIndex())];

    const core::Unit unit = kUnits[static_cast<std::size_t>(unit_->currentIndex())];
    sl.size               = core::Measure{size_->value(), unit};
    sl.interval           = core::Measure{interval_->value(), unit};
    sl.spacing_y          = core::Measure{spacingY_->value(), unit};

    sl.look.width_um  = width_->value();
    sl.look.src_width = core::Source::Explicit;
    sl.angle_udeg     = angle_->value() * 1000000;
    sl.opacity        = static_cast<std::uint8_t>(opacity_->value());

    refresh();
}

void StyleDesigner::addLayer()
{
    // A plain stroke, which is the layer a CAD entity has always had and the one
    // a user is least surprised to get.
    symbol_.layers.push_back(core::SymbolLayer{});
    refresh();
    stack_->setCurrentRow(0); // the list is top first, so the new layer is row 0
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
}

void StyleDesigner::applyToDocument()
{
    if (layerName_.isEmpty() || symbol_.layers.empty()) return;

    // One `STİL` per symbol layer, the first replacing and the rest appending.
    // Exactly the lines a script would write, because they ARE the lines a script
    // would write: the designer has no private road to the style column (Article
    // 1.1, 1.2), and that is also what makes it teachable to the AI.
    for (std::size_t i = 0; i < symbol_.layers.size(); ++i) {
        const core::SymbolLayer& sl = symbol_.layers[i];

        QString line =
            QStringLiteral("STİL katman=\"%1\" tip=%2")
                .arg(layerName_, QString::fromUtf8(core::symbol_layer_type_name(sl.type)));
        if (i > 0) line += QStringLiteral(" ekle=evet");

        line += QStringLiteral(" birim=%1").arg(QString::fromUtf8(core::unit_name(sl.size.unit)));
        line += QStringLiteral(" renk=%1").arg(sl.look.rgba);
        line += QStringLiteral(" kalinlik=%1").arg(sl.look.width_um);
        line += QStringLiteral(" dolgu=%1").arg(sl.look.fill_rgba);
        line += QStringLiteral(" boyut=%1").arg(sl.size.value);
        line += QStringLiteral(" aralik=%1").arg(sl.interval.value);
        line += QStringLiteral(" aralik_y=%1").arg(sl.spacing_y.value);
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
    // paket=` loads it back onto the shelf beside the MPYY set. A user style and
    // a published one are the same kind of thing to everything downstream.
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
