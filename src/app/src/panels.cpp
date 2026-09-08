// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/panels.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/symbol_preview.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/render/backend.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace kentos::app {
namespace {} // namespace

/// `1 482` — thin-space thousands, the way the reference prints an entity count.
QString groupedCount(std::size_t value)
{
    QString digits = QString::number(static_cast<qulonglong>(value));
    for (qsizetype at = digits.size() - 3; at > 0; at -= 3)
        digits.insert(at, QLatin1Char(' '));
    return digits;
}

// ----------------------------------------------------------- LayerRowDelegate --

namespace {

// `design.md` §7, measured off the reference: a 30 px row, a 14 px eye 10 px in,
// an 11 px colour chip, the name, then the count and the lock at the right edge.
constexpr int kLayerRow    = 30;
constexpr int kLayerPadX   = 10;
constexpr int kLayerEye    = 14;
constexpr int kLayerChip   = 11;
constexpr int kLayerGap    = 9;
constexpr int kLayerLock   = 14;
constexpr int kLayerCountW = 52;
constexpr int kLayerAccent = 2; ///< the left edge of a selected row

const Tokens& layerTokens(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

} // namespace

LayerRowDelegate::LayerRowDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize LayerRowDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return QSize(0, kLayerRow);
}

LayerRowDelegate::Hit LayerRowDelegate::hitTest(int x, int width, int depth)
{
    // `depth` is not decoration: paint() indents the eye by it, so a hit test that
    // ignores it answers for a nested row's colour chip instead of its eye.
    // A few px of slack in each direction — a 14 px target is small for a mouse
    // and tiny for a touchpad, and the slack costs nothing because what is next
    // to the eye (the chip) and next to the lock (the count) do nothing on click.
    constexpr int kSlack = 4;

    const int eye = kLayerPadX + depth * 14;
    if (x >= eye - kSlack && x < eye + kLayerEye + kSlack) return Hit::Eye;

    const int lock = width - kLayerPadX - kLayerLock;
    if (x >= lock - kSlack && x < lock + kLayerLock + kSlack) return Hit::Lock;

    return Hit::Row;
}

void LayerRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    const Tokens& t = layerTokens(theme_);
    const QRect box = option.rect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = option.state & QStyle::State_MouseOver;

    // §2's selected-row pattern, and it is the SAME everywhere in the shell: an
    // accent wash plus a 2 px accent edge. Hover is a flat grey and never
    // resembles it, so "where the pointer is" and "what is chosen" stay distinct.
    if (selected) {
        painter->fillRect(box, t.accentWash);
        painter->fillRect(QRect(box.left(), box.top(), kLayerAccent, box.height()), t.accent);
    } else if (hovered) {
        painter->fillRect(box, t.hoverRow);
    }

    const bool visible  = index.data(Qt::UserRole + 1).toBool();
    const bool locked   = index.data(Qt::UserRole + 2).toBool();
    const bool active   = index.data(Qt::UserRole + 3).toBool();
    const QColor chip   = index.data(Qt::UserRole + 4).value<QColor>();
    const QString name  = index.data(Qt::DisplayRole).toString();
    const QString tally = index.data(Qt::UserRole + 5).toString();
    const int depth     = index.data(Qt::UserRole + 6).toInt();

    int x = box.left() + kLayerPadX + depth * 14;
    painter->drawPixmap(QRect(x, box.top() + (kLayerRow - kLayerEye) / 2, kLayerEye, kLayerEye),
                        glyph_pixmap(visible ? Glyph::Eye : Glyph::EyeOff,
                                     visible ? t.textDim : t.textFaint, kLayerEye,
                                     option.widget ? option.widget->devicePixelRatioF() : 1.0));
    x += kLayerEye + kLayerGap;

    if (chip.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(chip);
        painter->drawRoundedRect(
            QRectF(x, box.top() + (kLayerRow - kLayerChip) / 2.0, kLayerChip, kLayerChip), 2.0,
            2.0);
    }
    x += kLayerChip + kLayerGap;

    QFont face(QStringLiteral("IBM Plex Sans"));
    face.setPixelSize(12);
    face.setWeight(active ? QFont::DemiBold : QFont::Normal);
    painter->setFont(face);
    painter->setPen(visible ? t.text : t.textFaint);

    const int right = box.right() - kLayerPadX - kLayerLock - kLayerGap - kLayerCountW;
    painter->drawText(QRect(x, box.top(), right - x, kLayerRow), Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(name, Qt::ElideRight, right - x));

    QFont digits(QStringLiteral("IBM Plex Mono"));
    digits.setPixelSize(11);
    painter->setFont(digits);
    painter->setPen(t.textFaint);
    painter->drawText(QRect(right, box.top(), kLayerCountW, kLayerRow),
                      Qt::AlignVCenter | Qt::AlignRight, tally);

    // A locked layer is WARN, an open one is faint. §2 gives warn exactly one
    // meaning — "you cannot edit this yet" — and this is one of its two uses.
    painter->drawPixmap(QRect(box.right() - kLayerPadX - kLayerLock,
                              box.top() + (kLayerRow - kLayerLock) / 2, kLayerLock, kLayerLock),
                        glyph_pixmap(locked ? Glyph::Lock : Glyph::Unlock,
                                     locked ? t.warn : t.textFaint, kLayerLock,
                                     option.widget ? option.widget->devicePixelRatioF() : 1.0));

    painter->restore();
}

// ----------------------------------------------------------------- LayerPanel --

LayerPanel::LayerPanel(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("layerTree"));
    tree_->setColumnCount(1);
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(false);
    tree_->setIndentation(0);
    tree_->setFrameShape(QFrame::NoFrame);
    tree_->setMouseTracking(true);
    // Off deliberately: a Qt stylesheet's background rule outranks both the
    // alternate-background-color property and QPalette::AlternateBase, so the
    // alternating row washes out in the dark theme. The delegate draws the row.
    tree_->setAlternatingRowColors(false);
    tree_->setUniformRowHeights(true);

    rows_ = new LayerRowDelegate(this);
    tree_->setItemDelegate(rows_);
    layout->addWidget(tree_, 1);

    // §7's footer: how many layers there are and how many of them can be edited.
    // A count is what tells a user their filter is on without a second control.
    footer_ = new QLabel(this);
    footer_->setObjectName(QStringLiteral("layerFooter"));
    footer_->setFixedHeight(26);
    footer_->setContentsMargins(10, 0, 10, 0);
    layout->addWidget(footer_);

    connect(tree_, &QTreeWidget::itemSelectionChanged, this,
            [this] { emit layerSelected(selectedLayer()); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &LayerPanel::onItemActivated);

    // The eye and the lock are PAINTED by the delegate, and a delegate cannot
    // answer a click. The viewport can, so the panel watches it and asks the
    // delegate where the click landed.
    tree_->viewport()->installEventFilter(this);

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

void LayerPanel::toggleRow(QTreeWidgetItem* item, bool visibility)
{
    if (!item) return;

    // The state is read from COLUMN 0 because that is the only column there is:
    // the tree has `setColumnCount(1)` and the delegate paints eye, chip, name,
    // count and lock into that one rect. Reading column 1 or 2 — which is what
    // this did — returns an invalid QVariant, `toBool()` makes it false, and the
    // command then always says `gorunur=evet`: the eye never turned off.
    const QVariant id = item->data(0, Qt::UserRole);
    if (!id.isValid()) return; // a group row has no layer to toggle

    const QString name = item->text(0);
    const bool on      = item->data(0, visibility ? Qt::UserRole + 1 : Qt::UserRole + 2).toBool();
    const QString off  = on ? QStringLiteral("hayır") : QStringLiteral("evet");

    // Toggling is an edit, so it leaves through the command bus like everything
    // else — never a direct Document write (CLAUDE.md 5.9).
    controller_.runLine(
        QStringLiteral("KATMAN ad=\"%1\" %2=%3")
            .arg(name, visibility ? QStringLiteral("gorunur") : QStringLiteral("kilitli"), off),
        command::Origin::Gui);
}

void LayerPanel::onItemActivated(QTreeWidgetItem* item, int)
{
    if (!item) return;
    if (!item->data(0, Qt::UserRole).isValid()) return;

    controller_.runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(item->text(0)),
                        command::Origin::Gui);
}

bool LayerPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != tree_->viewport() || event->type() != QEvent::MouseButtonRelease)
        return QWidget::eventFilter(watched, event);

    auto* click = static_cast<QMouseEvent*>(event);
    if (click->button() != Qt::LeftButton) return false;

    const QPoint at       = click->position().toPoint();
    QTreeWidgetItem* item = tree_->itemAt(at);
    if (!item) return false;

    const QRect row = tree_->visualItemRect(item);
    const int depth = item->data(0, Qt::UserRole + 6).toInt();
    const auto hit  = LayerRowDelegate::hitTest(at.x() - row.left(), row.width(), depth);

    switch (hit) {
    case LayerRowDelegate::Hit::Eye: toggleRow(item, true); return true;
    case LayerRowDelegate::Hit::Lock: toggleRow(item, false); return true;
    default: return false; // the rest of the row is a normal selection click
    }
}

void LayerPanel::probeByHand()
{
    const core::Document& doc = controller_.document();

    const auto say = [](const std::string& text) {
        (void)std::fprintf(stdout, "[katman] %s\n", text.c_str());
        (void)std::fflush(stdout);
    };

    const auto hit = [this](QTreeWidgetItem* item, int x) {
        const QRect row = tree_->visualItemRect(item);
        const QPointF at(row.left() + x, row.center().y());
        QMouseEvent press(QEvent::MouseButtonPress, at, tree_->viewport()->mapToGlobal(at),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, at, tree_->viewport()->mapToGlobal(at),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(tree_->viewport(), &press);
        QCoreApplication::sendEvent(tree_->viewport(), &release);
        QCoreApplication::processEvents();
    };

    for (int i = 0; i < tree_->topLevelItemCount() && i < 3; ++i) {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        const QVariant id     = item->data(0, Qt::UserRole);
        if (!id.isValid()) continue;

        const std::string name = item->text(0).toStdString();
        const core::LayerId at = static_cast<core::LayerId>(id.toUInt());
        const core::Layer* was = doc.layer(at);
        if (was == nullptr) continue;

        // TWICE. One click proves the route exists; the second proves the handler
        // reads the CURRENT state rather than a constant — which is the half of
        // the old defect a single click would not have caught, because a handler
        // that always sends `gorunur=evet` also "works" the first time.
        const bool before = was->visible;
        hit(item, kLayerPadX + kLayerEye / 2);
        const bool once = doc.layer(at) != nullptr && doc.layer(at)->visible;

        QTreeWidgetItem* back = tree_->topLevelItem(i);
        if (back != nullptr) hit(back, kLayerPadX + kLayerEye / 2);
        const bool twice = doc.layer(at) != nullptr && doc.layer(at)->visible;

        const auto word = [](bool on) { return on ? std::string("açık") : std::string("kapalı"); };
        say("göz  '" + name + "': " + word(before) + " -> " + word(once) + " -> " + word(twice) +
            (once != before && twice == before ? "  İKİ YÖNDE ÇALIŞIYOR" : "  BOZUK"));

        // The row moved: refresh() rebuilt the tree under the click.
        QTreeWidgetItem* again = tree_->topLevelItem(i);
        if (again == nullptr) continue;

        const bool locked_before = doc.layer(at)->locked;
        hit(again, tree_->viewport()->width() - kLayerPadX - kLayerLock / 2);
        const core::Layer* after = doc.layer(at);
        say("kilit '" + name + "': " + (locked_before ? "kilitli" : "açık") + " -> " +
            (after != nullptr && after->locked ? "kilitli" : "açık") +
            (after != nullptr && after->locked != locked_before ? "  DEĞİŞTİ" : "  DEĞİŞMEDİ"));
    }

    // AND THE OTHER DIRECTION: the canvas picks, the panel follows. Driven
    // through the bus, because that is the road a click on the drawing takes.
    const core::EntityTable& entities = doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e)) continue;

        const core::LayerId on   = entities.layer[e];
        const core::Layer* named = doc.layer(on);
        if (named == nullptr) continue;

        controller_.runLine(
            QStringLiteral("SEÇ mod=NESNE nesneler=%1").arg(static_cast<qulonglong>(doc.key_of(e))),
            command::Origin::Gui);
        QCoreApplication::processEvents();

        const core::LayerId followed = selectedLayer();
        say("seçim -> panel: nesne katmanı '" + named->name + "', panelde seçili '" +
            (followed == core::kNoLayer       ? std::string("(yok)")
             : doc.layer(followed) != nullptr ? doc.layer(followed)->name
                                              : std::string("?")) +
            "'" + (followed == on ? "  EŞLEŞTİ" : "  EŞLEŞMEDİ"));
        break;
    }
}

void LayerPanel::selectLayer(core::LayerId layer)
{
    if (layer == core::kNoLayer) return;
    if (selectedLayer() == layer) return; // already there; do not fight the user's scroll

    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        const QVariant id = (*it)->data(0, Qt::UserRole);
        if (!id.isValid() || static_cast<core::LayerId>(id.toUInt()) != layer) continue;

        // Blocked because this highlight comes FROM the canvas: letting it emit
        // layerSelected would send the answer back where the question came from.
        const QSignalBlocker quiet(tree_);
        tree_->setCurrentItem(*it);
        tree_->scrollToItem(*it);
        return;
    }
}

void LayerPanel::selectAllOn(const QString& name)
{
    // Through the bus, like every other selection: SEÇ with the layer filter, so
    // the command line, a script and this menu item all take the same road
    // (CLAUDE.md 1.2).
    controller_.runLine(QStringLiteral("SEÇ mod=KATMAN katman=\"%1\"").arg(name),
                        command::Origin::Gui);
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

void LayerPanel::applyTheme(ThemeMode mode)
{
    rows_->applyTheme(mode);
    tree_->viewport()->update();
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

    QHash<QString, QTreeWidgetItem*> groups;
    const std::vector<core::StyleId> styles = layer_styles(doc);

    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& l = doc.layers()[i];

        QTreeWidgetItem* parent =
            group_node(tree_, groups, QString::fromStdString(l.group).trimmed());
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);

        // Everything the delegate paints, on the ROW rather than in columns.
        item->setText(0, QString::fromStdString(l.name));
        item->setData(0, Qt::UserRole, static_cast<uint>(i));
        item->setData(0, Qt::UserRole + 1, l.visible);
        item->setData(0, Qt::UserRole + 2, l.locked);
        item->setData(0, Qt::UserRole + 3, static_cast<core::LayerId>(i) == active);
        item->setData(0, Qt::UserRole + 4, QColor::fromRgba(l.appearance.rgba));
        item->setData(0, Qt::UserRole + 5,
                      groupedCount(doc.layer_entity_count(static_cast<core::LayerId>(i))));
        item->setData(0, Qt::UserRole + 6, parent ? 1 : 0);
        item->setToolTip(0, static_cast<core::LayerId>(i) == active
                                ? tr("Aktif katman — %1").arg(QString::fromStdString(l.name))
                                : QString::fromStdString(l.name));

        if (static_cast<core::LayerId>(i) == keep) item->setSelected(true);
    }

    tree_->expandAll();
    tree_->blockSignals(false);

    std::size_t editable = 0;
    for (const auto& l : doc.layers())
        if (l.visible && !l.locked) ++editable;
    footer_->setText(tr("%1 katman  ·  %2 düzenlenebilir").arg(doc.layers().size()).arg(editable));
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
    const core::StyleId style = layer.style != core::kByLayerStyle ? layer.style
                                : used != core::kByLayerStyle      ? used
                                                                   : core::kByLayerStyle;
    const core::Symbol symbol = doc.styles().contains(style) && style != core::kByLayerStyle
                                    ? doc.styles().symbol_at(style)
                                    : core::Symbol::of(layer.appearance);

    return symbol_icon(symbol, doc.images(), doc.dashes(), QSize(28, 18),
                       palette().color(QPalette::Base).rgba(), PreviewShape::Area);
}

void LayerPanel::showContextMenu(const QPoint& where)
{
    QMenu* menu = buildContextMenu(tree_->itemAt(where));

    // Every entry leaves through the command bus. A context menu is a client like
    // any other and gets no private road to the document (Article 1.2, 5.9) —
    // which is also what lets a script do the same things.
    menu->exec(tree_->viewport()->mapToGlobal(where));
    delete menu;
}

bool LayerPanel::triggerContextEntry(const QString& layerName, const QString& entry)
{
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if (!(*it)->data(0, Qt::UserRole).isValid()) continue;
        if ((*it)->text(0) != layerName) continue;

        QMenu* menu = buildContextMenu(*it);
        for (QAction* action : menu->actions()) {
            if (action->text() != entry) continue;
            action->trigger();
            delete menu;
            return true;
        }
        delete menu;
        return false;
    }
    return false;
}

QStringList LayerPanel::contextEntries(const QString& layerName)
{
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if (!(*it)->data(0, Qt::UserRole).isValid()) continue;
        if ((*it)->text(0) != layerName) continue;

        QMenu* menu = buildContextMenu(*it);
        QStringList texts;
        for (const QAction* action : menu->actions())
            texts << (action->isSeparator() ? QStringLiteral("—") : action->text());
        delete menu;
        return texts;
    }
    return {};
}

QMenu* LayerPanel::buildContextMenu(QTreeWidgetItem* item)
{
    // A group row carries no layer id, so the menu it gets is the one that does
    // not need one.
    const QVariant id  = item ? item->data(0, Qt::UserRole) : QVariant();
    const QString name = (item && id.isValid()) ? item->text(0) : QString();

    auto* owned = new QMenu(this);
    QMenu& menu = *owned;

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

        QAction* pick = menu.addAction(tr("Tümünü seç"));
        pick->setToolTip(tr("Bu katmandaki bütün nesneleri seçer"));
        connect(pick, &QAction::triggered, this, [this, name] { selectAllOn(name); });

        QAction* table = menu.addAction(tr("Öznitelik tablosu"));
        table->setToolTip(tr("Bu katmanın öznitelik tablosunu açar"));
        connect(table, &QAction::triggered, this,
                [this, name] { emit attributeTableRequested(name); });

        QAction* activate = menu.addAction(tr("Aktif katman yap"));
        connect(activate, &QAction::triggered, this, [this, name] {
            controller_.runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(name), command::Origin::Gui);
        });

        QAction* label = menu.addAction(tr("Özniteliklerden etiketle…"));
        connect(label, &QAction::triggered, this, [this, name] {
            bool ok = false;
            const QString format =
                QInputDialog::getText(this, tr("Özniteliklerden etiketle"),
                                      tr("Biçim — {sutun} değeriyle değişir, \\n satır kırar:"),
                                      QLineEdit::Normal, QStringLiteral("{ada}/{parsel}"), &ok);
            if (!ok || format.trimmed().isEmpty()) return;

            // The size is ASKED FOR rather than assumed: a label on a 1/1000 pafta
            // and one on a 1/25000 `çevre düzeni planı` are different heights, and
            // there is no figure this program can pick for both.
            const int height = QInputDialog::getInt(
                this, tr("Yazı yüksekliği"), tr("Zemin milimetresi:"), 2000, 1, 1000000, 100, &ok);
            if (!ok) return;

            controller_.runLine(QStringLiteral("ETİKET katman=\"%1\" bicim=\"%2\" yukseklik=%3")
                                    .arg(name, format.trimmed())
                                    .arg(height),
                                command::Origin::Gui);
        });

        menu.addSeparator();

        const bool visible = item->data(0, Qt::UserRole + 1).toBool();
        QAction* show      = menu.addAction(visible ? tr("Gizle") : tr("Göster"));
        connect(show, &QAction::triggered, this, [this, name, visible] {
            controller_.runLine(
                QStringLiteral("KATMAN ad=\"%1\" gorunur=%2")
                    .arg(name, visible ? QStringLiteral("hayır") : QStringLiteral("evet")),
                command::Origin::Gui);
        });

        const bool locked = item->data(0, Qt::UserRole + 2).toBool();
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

        // LAST, AND ON ITS OWN. Two entries used to sit in the middle of this
        // menu — `Stili düzenle…` and `Stili temizle` — and both were pieces of
        // one window shown in a list beside `Gizle` and `Gruba taşı…`. The window
        // they belong to has been called `Katman Özellikleri` all along and holds
        // a dozen pages; the symbology is one of them, and clearing the style is
        // a button on it.
        //
        // The place every desktop program puts `Properties`: the bottom of the
        // menu, after a rule, because it is the entry that opens something rather
        // than doing something.
        menu.addSeparator();

        QAction* properties = menu.addAction(tr("Katman Özellikleri…"));
        properties->setToolTip(tr("Bilgi, simgeleyici, etiketler — katmanın bütün ayarları"));
        connect(properties, &QAction::triggered, this,
                [this, name] { emit propertiesRequested(name); });
    }

    return owned;
}

} // namespace kentos::app
