// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/panels.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/app/symbol_preview.hpp"
#include "piricad/render/backend.hpp"

#include <algorithm>
#include <vector>

#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QTreeWidget>

namespace piricad::app {
namespace {

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
    // Off deliberately: a Qt stylesheet's background rule outranks both the
    // alternate-background-color property and QPalette::AlternateBase, so the
    // alternating row washes out in the dark theme. The colour swatch and the bold
    // active layer already separate the rows.
    tree_->setAlternatingRowColors(false);
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

    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &LayerPanel::showContextMenu);
}

core::LayerId LayerPanel::selectedLayer() const
{
    const auto items = tree_->selectedItems();
    if (items.isEmpty()) return core::kNoLayer;

    // A group row carries no id. Returning 0 for it would silently select layer
    // zero, which every CAD document has and nobody clicked on.
    const QVariant id = items.front()->data(0, Qt::UserRole);
    if (!id.isValid()) return core::kNoLayer;
    return static_cast<core::LayerId>(id.toUInt());
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

namespace {

/// The tree node for one group path, created on the way down if it is not there.
///
/// Groups are held by their FULL path in `by_path`, not by name: two different
/// branches may each have a `SINIRLAR` under them and they are different drawers.
QTreeWidgetItem* group_node(QTreeWidget* tree, QHash<QString, QTreeWidgetItem*>& by_path,
                            const QString& path)
{
    if (path.isEmpty()) return nullptr;
    if (auto* found = by_path.value(path, nullptr)) return found;

    const auto cut            = static_cast<int>(path.lastIndexOf(QLatin1Char('>')));
    const QString parent_path = cut < 0 ? QString() : path.left(cut).trimmed();
    const QString leaf        = (cut < 0 ? path : path.mid(cut + 1)).trimmed();

    QTreeWidgetItem* parent = group_node(tree, by_path, parent_path);
    auto* node              = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);

    node->setText(0, leaf);
    node->setFirstColumnSpanned(true);

    // A group is not a layer, and the row has to say so: no id, so a click on it
    // selects nothing and the property panel does not follow.
    node->setData(0, Qt::UserRole, QVariant());
    QFont font = node->font(0);
    font.setBold(true);
    node->setFont(0, font);
    node->setExpanded(true);

    by_path.insert(path, node);
    return node;
}

/// The style the entities on each layer actually carry, where they agree.
///
/// A layer holds a DEFAULT appearance and its entities hold a style column, so
/// "what does this layer draw" has two answers and the useful one is the second:
/// after `STİL katman=OSB kod=...` the layer default is untouched and every parcel
/// on it carries the gösterim. A swatch showing the default would show grey.
///
/// ONE pass over the entities, not one per layer, and it stops as soon as every
/// layer has an answer — which on any ordinary drawing is within the first few
/// hundred entities. The cap is what keeps a five-million-parcel sheet from
/// paying for a panel refresh on every command; a layer the pass did not reach
/// keeps its own appearance, which is the truthful fallback rather than a guess.
std::vector<core::StyleId> layer_styles(const core::Document& doc)
{
    constexpr std::size_t kScanCap = 100000;

    std::vector<core::StyleId> found(doc.layers().size(), core::kByLayerStyle);
    std::size_t answered = 0;

    const auto& entities    = doc.entities();
    const std::size_t limit = std::min<std::size_t>(entities.size(), kScanCap);

    for (std::size_t e = 0; e < limit && answered < found.size(); ++e) {
        if (!entities.alive(static_cast<core::EntityId>(e))) continue;

        const core::LayerId l = entities.layer[e];
        if (l >= found.size() || found[l] != core::kByLayerStyle) continue;

        found[l] = entities.style[e];
        if (found[l] != core::kByLayerStyle) ++answered;
    }
    return found;
}

} // namespace

void LayerPanel::refresh()
{
    const core::LayerId keep = selectedLayer();

    tree_->blockSignals(true);
    tree_->clear();

    // Counts come from the document, which maintains them incrementally. Walking
    // the entity array here would put an O(n) pass on every document change.
    const auto& doc            = controller_.document();
    const core::LayerId active = controller_.bus().active_layer();

    QHash<QString, QTreeWidgetItem*> groups;
    const std::vector<core::StyleId> styles = layer_styles(doc);

    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& l = doc.layers()[i];

        QTreeWidgetItem* parent =
            group_node(tree_, groups, QString::fromStdString(l.group).trimmed());
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);

        item->setText(0, QString::fromStdString(l.name));
        item->setIcon(0, layerIcon(l, i < styles.size() ? styles[i] : core::kByLayerStyle));
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

QIcon LayerPanel::layerIcon(const core::Layer& layer, core::StyleId used) const
{
    const core::Document& doc = controller_.document();

    // What the entities carry, when they carry anything; the layer's own default
    // otherwise. Drawn by the CANVAS backend either way, so the swatch and the map
    // cannot disagree.
    //
    // Drawn as an AREA because that is what a layer of parcels, a plan lekesi and
    // a cadastral sheet mostly are; a line-only layer still reads correctly,
    // because a rectangle shows a stroke as well as a line does at this size.
    const core::Symbol symbol = used != core::kByLayerStyle && doc.styles().contains(used)
                                    ? doc.styles().symbol_at(used)
                                    : core::Symbol::of(layer.appearance);

    return symbol_icon(symbol, doc.images(), QSize(28, 18), palette().color(QPalette::Base).rgba(),
                       PreviewShape::Area);
}

void LayerPanel::showContextMenu(const QPoint& where)
{
    QTreeWidgetItem* item = tree_->itemAt(where);

    // A group row carries no layer id, so the menu it gets is the one that does
    // not need one.
    const QVariant id  = item ? item->data(0, Qt::UserRole) : QVariant();
    const QString name = (item && id.isValid()) ? item->text(0) : QString();

    QMenu menu(this);

    QAction* add = menu.addAction(tr("Yeni katman…"));
    connect(add, &QAction::triggered, this, [this] {
        bool ok             = false;
        const QString fresh = QInputDialog::getText(this, tr("Yeni katman"), tr("Katman adı:"),
                                                    QLineEdit::Normal, QString(), &ok);
        if (ok && !fresh.trimmed().isEmpty())
            controller_.runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(fresh.trimmed()),
                                command::Origin::Gui);
    });

    if (!name.isEmpty()) {
        menu.addSeparator();

        QAction* activate = menu.addAction(tr("Aktif katman yap"));
        connect(activate, &QAction::triggered, this, [this, name] {
            controller_.runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(name), command::Origin::Gui);
        });

        QAction* style = menu.addAction(tr("Stili düzenle…"));
        connect(style, &QAction::triggered, this, [this, name] { emit styleRequested(name); });

        menu.addSeparator();

        const bool visible = item->data(1, Qt::UserRole).toBool();
        QAction* show      = menu.addAction(visible ? tr("Gizle") : tr("Göster"));
        connect(show, &QAction::triggered, this, [this, name, visible] {
            controller_.runLine(
                QStringLiteral("KATMAN ad=\"%1\" gorunur=%2")
                    .arg(name, visible ? QStringLiteral("hayır") : QStringLiteral("evet")),
                command::Origin::Gui);
        });

        const bool locked = item->data(2, Qt::UserRole).toBool();
        QAction* lock     = menu.addAction(locked ? tr("Kilidi aç") : tr("Kilitle"));
        connect(lock, &QAction::triggered, this, [this, name, locked] {
            controller_.runLine(
                QStringLiteral("KATMAN ad=\"%1\" kilitli=%2")
                    .arg(name, locked ? QStringLiteral("hayır") : QStringLiteral("evet")),
                command::Origin::Gui);
        });

        menu.addSeparator();

        QAction* group = menu.addAction(tr("Gruba taşı…"));
        connect(group, &QAction::triggered, this, [this, name] {
            bool ok            = false;
            const QString path = QInputDialog::getText(
                this, tr("Gruba taşı"), tr("Grup yolu, düzeyler '>' ile ayrılır. Boş = kök:"),
                QLineEdit::Normal, QString(), &ok);
            if (ok)
                controller_.runLine(
                    QStringLiteral("KATMAN ad=\"%1\" grup=\"%2\"").arg(name, path.trimmed()),
                    command::Origin::Gui);
        });
    }

    // Every entry above leaves through the command bus. A context menu is a
    // client like any other and gets no private road to the document (Article
    // 1.2, 5.9) — which is also what lets a script do the same things.
    menu.exec(tree_->viewport()->mapToGlobal(where));
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
