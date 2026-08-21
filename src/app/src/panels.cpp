// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/panels.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/render/backend.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPixmap>
#include <QTreeWidget>

namespace piricad::app {
namespace {

QIcon swatch(std::uint32_t rgba)
{
    QPixmap pm(28, 14);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor::fromRgba(static_cast<QRgb>(rgba)));
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawRoundedRect(QRectF(0.5, 0.5, 27.0, 13.0), 3, 3);
    return QIcon(pm);
}

QString metres(core::Mm v)
{
    return QString::number(core::mm_to_metres(v), 'f', 3);
}

} // namespace

// ----------------------------------------------------------------- LayerPanel --

LayerPanel::LayerPanel(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(4);
    tree_->setHeaderLabels({tr("Katman"), tr("Gör."), tr("Kilit"), tr("Nesne")});
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(tree_);

    connect(tree_, &QTreeWidget::itemSelectionChanged, this,
            [this] { emit layerSelected(selectedLayer()); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &LayerPanel::onItemActivated);
}

core::LayerId LayerPanel::selectedLayer() const
{
    const auto items = tree_->selectedItems();
    if (items.isEmpty()) return core::kNoLayer;
    return static_cast<core::LayerId>(items.front()->data(0, Qt::UserRole).toUInt());
}

void LayerPanel::onItemActivated(QTreeWidgetItem* item, int column)
{
    if (!item) return;

    const QString name = item->text(0);

    // Toggling a column is an edit, so it leaves through the command bus like
    // everything else — never a direct Document write.
    if (column == 1) {
        const bool visible = item->data(1, Qt::UserRole).toBool();
        controller_.runLine(
            QStringLiteral("KATMAN ad=\"%1\" gorunur=%2")
                .arg(name, visible ? QStringLiteral("hayır") : QStringLiteral("evet")),
            command::Origin::Gui);
        return;
    }
    if (column == 2) {
        const bool locked = item->data(2, Qt::UserRole).toBool();
        controller_.runLine(
            QStringLiteral("KATMAN ad=\"%1\" kilitli=%2")
                .arg(name, locked ? QStringLiteral("hayır") : QStringLiteral("evet")),
            command::Origin::Gui);
        return;
    }

    controller_.runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(name), command::Origin::Gui);
}

void LayerPanel::refresh()
{
    const core::LayerId keep = selectedLayer();

    tree_->blockSignals(true);
    tree_->clear();

    // Counts come from the document, which maintains them incrementally. Walking
    // the entity array here would put an O(n) pass on every document change.
    const auto& doc            = controller_.document();
    const core::LayerId active = controller_.bus().active_layer();

    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& l = doc.layers()[i];
        auto* item    = new QTreeWidgetItem(tree_);

        item->setText(0, QString::fromStdString(l.name));
        item->setIcon(0, swatch(l.appearance.rgba));
        item->setData(0, Qt::UserRole, static_cast<uint>(i));

        item->setText(1, l.visible ? QStringLiteral("●") : QStringLiteral("○"));
        item->setData(1, Qt::UserRole, l.visible);
        item->setTextAlignment(1, Qt::AlignCenter);

        item->setText(2, l.locked ? QStringLiteral("🔒") : QStringLiteral("–"));
        item->setData(2, Qt::UserRole, l.locked);
        item->setTextAlignment(2, Qt::AlignCenter);

        item->setText(3, QString::number(doc.layer_entity_count(static_cast<core::LayerId>(i))));
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);

        if (static_cast<core::LayerId>(i) == active) {
            QFont f = item->font(0);
            f.setBold(true);
            item->setFont(0, f);
            item->setToolTip(0, tr("Aktif katman"));
        }
        if (static_cast<core::LayerId>(i) == keep) item->setSelected(true);
    }

    tree_->blockSignals(false);
}

// -------------------------------------------------------------- PropertyPanel --

PropertyPanel::PropertyPanel(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({tr("Özellik"), tr("Değer")});
    tree_->setRootIsDecorated(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setStretchLastSection(true);
    layout->addWidget(tree_);
}

void PropertyPanel::setLayer(core::LayerId layer)
{
    layer_ = layer;
    refresh();
}

void PropertyPanel::addGroup(const QString& title)
{
    group_ = new QTreeWidgetItem(tree_);
    group_->setText(0, title);
    group_->setFirstColumnSpanned(true);

    QFont f = group_->font(0);
    f.setBold(true);
    group_->setFont(0, f);
    group_->setExpanded(true);
}

void PropertyPanel::addRow(const QString& key, const QString& value)
{
    auto* item = group_ ? new QTreeWidgetItem(group_) : new QTreeWidgetItem(tree_);
    item->setText(0, key);
    item->setText(1, value);
    item->setToolTip(1, value);
}

void PropertyPanel::refresh()
{
    tree_->clear();
    group_ = nullptr;

    const auto& doc = controller_.document();

    addGroup(tr("Doküman"));
    addRow(tr("Koordinat sistemi"), QString::fromStdString(doc.crs().id()));
    addRow(tr("Katman"), QString::number(doc.layers().size()));
    addRow(tr("Nesne"), QString::number(doc.live_entity_count()));
    addRow(tr("Aktif katman"), controller_.activeLayerName());
    addRow(tr("Sürüm"), QString::number(doc.revision()));

    const core::Box2 box = doc.extent();
    addGroup(tr("Kapsam"));
    if (box.empty()) {
        addRow(tr("Durum"), tr("boş çizim"));
    } else {
        addRow(tr("Sağa (Y) min / max"),
               QStringLiteral("%1  /  %2").arg(metres(box.min_x), metres(box.max_x)));
        addRow(tr("Yukarı (X) min / max"),
               QStringLiteral("%1  /  %2").arg(metres(box.min_y), metres(box.max_y)));
        addRow(tr("Genişlik × Yükseklik"),
               QStringLiteral("%1 × %2 m").arg(metres(box.width()), metres(box.height())));
    }

    if (const core::Layer* l = doc.layer(layer_)) {
        addGroup(tr("Katman — %1").arg(QString::fromStdString(l->name)));
        addRow(tr("Görünür"), l->visible ? tr("evet") : tr("hayır"));
        addRow(tr("Kilitli"), l->locked ? tr("evet") : tr("hayır"));
        addRow(tr("Renk"),
               QStringLiteral("#%1").arg(l->appearance.rgba, 8, 16, QLatin1Char('0')).toUpper());
        addRow(tr("Çizgi kalınlığı"),
               QStringLiteral("%1 mm").arg(l->appearance.width_um / 1000.0, 0, 'f', 2));
        addRow(tr("Nesne"), QString::number(doc.layer_entity_count(layer_)));
    }

    addGroup(tr("Oturum"));
    addRow(tr("Komut"), QString::number(controller_.registry().size()));
    addRow(tr("Günlük satırı"), QString::number(controller_.journal().size()));
    addRow(tr("Geri alma / yineleme"), QStringLiteral("%1 / %2")
                                           .arg(controller_.undoStack().undo_depth())
                                           .arg(controller_.undoStack().redo_depth()));
    addRow(tr("Render arka ucu"), render::gpu_backend_status().empty()
                                      ? QStringLiteral("QRhi (GPU)")
                                      : QStringLiteral("QPainter (Faz 0)"));
}

} // namespace piricad::app
