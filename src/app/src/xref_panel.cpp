// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/xref_panel.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/command/external_ref.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

constexpr int kRow    = 46;
constexpr int kPadX   = 10;
constexpr int kEye    = 14;
constexpr int kGap    = 9;
constexpr int kAccent = 2;

const Tokens& rowTokens(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

// The roles a row carries; the delegate paints from nothing else.
constexpr int kNameRole  = Qt::UserRole;
constexpr int kBadgeRole = Qt::UserRole + 1;
constexpr int kToneRole  = Qt::UserRole + 2;
constexpr int kCountRole = Qt::UserRole + 3;
constexpr int kWhereRole = Qt::UserRole + 4;
constexpr int kSeenRole  = Qt::UserRole + 5;
constexpr int kStateRole = Qt::UserRole + 6;

/// The badge a state reads as, in the panel's own capitals (design.md §3: never
/// `toUpper()`), and the tone that says it.
std::pair<QString, Tone> badge_of(command::ExternalListing::State state, bool changed)
{
    using State = command::ExternalListing::State;
    if (changed && state == State::Loaded)
        return {QCoreApplication::translate("XrefPanel", "DEĞİŞTİ"), Tone::Warn};
    switch (state) {
    case State::Loaded: return {QCoreApplication::translate("XrefPanel", "YÜKLÜ"), Tone::Ok};
    case State::Unloaded:
        return {QCoreApplication::translate("XrefPanel", "BOŞALTILDI"), Tone::Neutral};
    case State::Missing:
        return {QCoreApplication::translate("XrefPanel", "BULUNAMADI"), Tone::Danger};
    case State::Empty: return {QCoreApplication::translate("XrefPanel", "BOŞ"), Tone::Warn};
    }
    return {QString(), Tone::Neutral};
}

} // namespace

// ------------------------------------------------------------ the rows ----

XrefRowDelegate::XrefRowDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

bool XrefRowDelegate::onEye(int x)
{
    return x >= kPadX - 4 && x < kPadX + kEye + 4;
}

QSize XrefRowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return {option.rect.width(), kRow};
}

void XrefRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    const Tokens& t = rowTokens(theme_);
    const QRect box = option.rect;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // The shell's one selected-row pattern: an accent wash and a 2 px edge.
    if ((option.state & QStyle::State_Selected) != 0) {
        painter->fillRect(box, t.accentWash);
        painter->fillRect(QRect(box.left(), box.top(), kAccent, box.height()), t.accent);
    } else if ((option.state & QStyle::State_MouseOver) != 0) {
        painter->fillRect(box, t.hoverRow);
    }

    const bool seen      = index.data(kSeenRole).toBool();
    const QString name   = index.data(kNameRole).toString();
    const QString badge  = index.data(kBadgeRole).toString();
    const auto tone      = static_cast<Tone>(index.data(kToneRole).toInt());
    const QString counts = index.data(kCountRole).toString();
    const QString where  = index.data(kWhereRole).toString();
    const qreal dpr      = option.widget != nullptr ? option.widget->devicePixelRatioF() : 1.0;

    const int top = box.top() + 5;
    int x         = box.left() + kPadX;
    painter->drawPixmap(
        QRect(x, top + 1, kEye, kEye),
        glyph_pixmap(seen ? Glyph::Eye : Glyph::EyeOff, seen ? t.textDim : t.textFaint, kEye, dpr));
    x += kEye + kGap;

    QFont face(QStringLiteral("IBM Plex Sans"));
    face.setPixelSize(12);
    face.setWeight(QFont::DemiBold);
    painter->setFont(face);
    painter->setPen(seen ? t.text : t.textFaint);

    // THE NAME FIRST, whole where the row allows: it is what a user looks
    // for, and a row that shortened it to make room for counts showed
    // "haliha…". The state sits right after it.
    const int right   = box.right() - kPadX;
    const int badge_w = Badge::widthFor(badge);
    // Two pixels over the measured advance: the metrics round to whole pixels
    // and the eliding does not, so a name given exactly its width lost its
    // last letters to "…".
    const int fits   = painter->fontMetrics().horizontalAdvance(name) + 2;
    const int name_w = std::max(20, std::min(fits, right - badge_w - kGap - x));
    painter->drawText(QRect(x, top, name_w, 16), Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(name, Qt::ElideRight, name_w));
    Badge::paint(*painter, QRect(x + name_w + kGap, top + 1, badge_w, 14), badge, tone, theme_);

    // Under it, where its file is — what a user checks first when a reference
    // will not load — and, at the right, how much of it the drawing holds.
    QFont digits(QStringLiteral("IBM Plex Mono"));
    digits.setPixelSize(11);
    const int count_w = QFontMetrics(digits).horizontalAdvance(counts) + 2;
    painter->setFont(digits);
    painter->setPen(t.textFaint);
    painter->drawText(QRect(right - count_w, top + 19, count_w, 16),
                      Qt::AlignVCenter | Qt::AlignRight, counts);
    QFont small(QStringLiteral("IBM Plex Sans"));
    small.setPixelSize(11);
    painter->setFont(small);
    const int indent = box.left() + kPadX + kEye + kGap;
    const int room   = right - count_w - kGap - indent;
    painter->drawText(QRect(indent, top + 19, room, 16), Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(where, Qt::ElideMiddle, room));
    painter->restore();
}

// ----------------------------------------------------------- the panel ----

XrefPanel::XrefPanel(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    banner_ = new Banner(Tone::Warn, tr("Kaynak dosya değişti"), QString(), this);
    banner_->setVisible(false);
    Button* again = banner_->addButton(tr("Yenile"));
    again->setAccessibleName(tr("Değişen dış referansları yenile"));
    connect(again, &QPushButton::clicked, this, [this] {
        QStringList lines;
        for (const QString& name : changed_)
            lines << QStringLiteral("DIŞREFERANS islem=yenile ad=\"%1\"").arg(name);
        lines.sort();
        if (lines.size() == 1)
            controller_.runLine(lines.front(), command::Origin::Gui);
        else if (!lines.isEmpty())
            controller_.runLines(lines, tr("Dış referansları yenile"));
    });
    column->addWidget(banner_);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(1);
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(false);
    tree_->setIndentation(0);
    tree_->setFrameShape(QFrame::NoFrame);
    tree_->setMouseTracking(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setAccessibleName(tr("Dış referanslar"));
    delegate_ = new XrefRowDelegate(tree_);
    tree_->setItemDelegate(delegate_);
    tree_->viewport()->installEventFilter(this);
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &XrefPanel::syncButtons);
    connect(tree_, &QWidget::customContextMenuRequested, this, &XrefPanel::showMenu);
    column->addWidget(tree_, 1);

    // AN EMPTY LIST IS AN INVITATION: what a reference is and how to make one.
    empty_ = new QLabel(tr("Bu çizimde dış referans yok. Başlıktaki + ile bir proje, DXF ya "
                           "da DWG dosyasını bağlayın: kendi koordinatlarında, yerinde çizilir, "
                           "düzenlenmez ve dosyası değişince yenilenir."),
                        this);
    empty_->setWordWrap(true);
    // IT GIVES WAY: a sentence that wrapped to four lines made the whole dock
    // taller at its smallest, and the window no longer fit a 720-line screen.
    empty_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    empty_->setContentsMargins(kPadX, 12, kPadX, 12);
    empty_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    column->addWidget(empty_, 1);

    // THE STEPS, in words: a row's buttons, the same as its context menu.
    steps_     = new QWidget(this);
    auto* rows = new QVBoxLayout(steps_);
    rows->setContentsMargins(kPadX - 4, 6, kPadX - 4, 8);
    rows->setSpacing(4);
    auto* first  = new QHBoxLayout();
    auto* second = new QHBoxLayout();
    first->setSpacing(4);
    second->setSpacing(4);
    const auto step = [this](const QString& text, Glyph glyph, const QString& tip) {
        auto* b = new Button(ButtonRole::Ghost, text, glyph, this);
        b->setControlSize(ControlSize::Compact);
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        return b;
    };
    reload_ =
        step(tr("Yenile"), Glyph::XrefReload, tr("Seçili dış referansı dosyasından yeniden oku"));
    unload_ = step(tr("Boşalt"), Glyph::EyeOff,
                   tr("Seçili dış referansı çizimden çıkar; referansı yerinde, boş kalır"));
    repath_ = step(tr("Yol…"), Glyph::Open, tr("Seçili dış referansı başka bir dosyaya bağla"));
    bind_   = step(tr("Bağla"), Glyph::BlockInsert,
                   tr("Seçili dış referansı çizime kat: sıradan blok olur, dosyası değişse de "
                        "değişmez"));
    detach_ = step(tr("Kaldır"), Glyph::Erase,
                   tr("Seçili dış referansı referanslarıyla birlikte çizimden kaldır; dosyasına "
                      "dokunulmaz"));
    first->addWidget(reload_);
    first->addWidget(unload_);
    first->addWidget(repath_);
    first->addStretch(1);
    second->addWidget(bind_);
    second->addWidget(detach_);
    second->addStretch(1);
    rows->addLayout(first);
    rows->addLayout(second);
    column->addWidget(steps_);

    connect(reload_, &QPushButton::clicked, this,
            [this] { runStep(QStringLiteral("yenile"), selectedName()); });
    connect(unload_, &QPushButton::clicked, this, [this] {
        const QTreeWidgetItem* item = tree_->currentItem();
        if (item == nullptr) return;
        const bool unloaded = item->data(0, kStateRole).toInt() ==
                              static_cast<int>(command::ExternalListing::State::Unloaded);
        runStep(unloaded ? QStringLiteral("yukle") : QStringLiteral("bosalt"), selectedName());
    });
    connect(repath_, &QPushButton::clicked, this, [this] {
        const QString name = selectedName();
        if (name.isEmpty()) return;
        const QString start = QFileInfo(paths_.value(name)).absolutePath();
        const QString file  = QFileDialog::getOpenFileName(
            this, tr("'%1' için yeni dosya").arg(name), start,
            tr("Çizim dosyası (*.pcad *.dxf *.dwg);;Tüm dosyalar (*)"));
        if (!file.isEmpty()) repathTo(name, file);
    });
    connect(bind_, &QPushButton::clicked, this,
            [this] { runStep(QStringLiteral("bagla"), selectedName()); });
    connect(detach_, &QPushButton::clicked, this,
            [this] { runStep(QStringLiteral("kaldir"), selectedName()); });

    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, &XrefPanel::onFileChanged);

    refresh();
}

void XrefPanel::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    delegate_->applyTheme(mode);
    tree_->viewport()->update();
}

QString XrefPanel::selectedName() const
{
    const QTreeWidgetItem* item = tree_->currentItem();
    return item != nullptr && item->isSelected() ? item->data(0, kNameRole).toString() : QString();
}

QStringList XrefPanel::layersOf(const QString& name) const
{
    QStringList out;
    const std::string prefix = core::turkish_fold_key(name.toStdString() + "|");
    for (const core::Layer& l : controller_.document().layers())
        if (core::turkish_fold_key(l.name).starts_with(prefix))
            out << QString::fromStdString(l.name);
    return out;
}

void XrefPanel::refresh()
{
    const core::Document& doc = controller_.document();

    // ANOTHER DRAWING, another set of files: what changed under the last one
    // is not news about this one.
    if (controller_.currentFile() != project_) {
        project_ = controller_.currentFile();
        changed_.clear();
    }

    const QString keep = selectedName();
    tree_->blockSignals(true);
    tree_->clear();
    paths_.clear();
    const QDir home = project_.isEmpty() ? QDir() : QFileInfo(project_).absoluteDir();

    QStringList watch;
    for (const command::ExternalListing& row : command::list_external_references(doc)) {
        const QString name = QString::fromStdString(row.name);
        const QString path = QString::fromStdString(row.path);
        paths_.insert(name, QDir::cleanPath(path));
        const auto [badge, tone] = badge_of(row.state, changed_.contains(name));

        // Where the file is, as the project folder sees it when it is inside.
        QString where = path;
        if (!project_.isEmpty()) {
            const QString relative = home.relativeFilePath(path);
            if (!relative.startsWith(QStringLiteral(".."))) where = relative;
        }

        bool seen                = false;
        const QStringList layers = layersOf(name);
        for (const QString& l : layers) {
            const core::LayerId id = doc.find_layer(l.toStdString());
            seen                   = seen ||
                   (id != core::kNoLayer && doc.layer(id) != nullptr && doc.layer(id)->visible);
        }
        // A reference whose file brought no layer of its own draws on layer 0
        // and the reference's own: seen, for the eye has nothing to put out.
        if (layers.isEmpty()) seen = true;

        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, name);
        item->setData(0, kNameRole, name);
        item->setData(0, kBadgeRole, badge);
        item->setData(0, kToneRole, static_cast<int>(tone));
        item->setData(0, kCountRole, tr("%1 nesne · %2 ref.").arg(row.members).arg(row.references));
        item->setData(0, kWhereRole, where);
        item->setData(0, kSeenRole, seen);
        item->setData(0, kStateRole, static_cast<int>(row.state));
        item->setToolTip(0, tr("%1 — %2").arg(name, path));
        if (name == keep) {
            tree_->setCurrentItem(item);
            item->setSelected(true);
        }
        if (row.state != command::ExternalListing::State::Unloaded && QFileInfo::exists(path))
            watch << QDir::cleanPath(path);
    }
    tree_->blockSignals(false);

    // THE WATCH follows the files the drawing reads: added as they are bound,
    // dropped as they are taken off or put aside.
    const QStringList watched = watcher_->files();
    for (const QString& f : watched)
        if (!watch.contains(f)) watcher_->removePath(f);
    for (const QString& f : watch)
        if (!watched.contains(f)) watcher_->addPath(f);

    // What changed under a reference that is gone is not news either.
    for (auto it = changed_.begin(); it != changed_.end();)
        it = paths_.contains(*it) ? std::next(it) : changed_.erase(it);

    const bool any = tree_->topLevelItemCount() > 0;
    tree_->setVisible(any);
    empty_->setVisible(!any);
    // With nothing listed there is nothing to step on.
    steps_->setVisible(any);
    syncButtons();
    syncBanner();
}

void XrefPanel::syncButtons()
{
    const QTreeWidgetItem* item = tree_->currentItem();
    const bool picked           = item != nullptr && item->isSelected();
    const int state             = picked ? item->data(0, kStateRole).toInt() : -1;
    const bool unloaded = state == static_cast<int>(command::ExternalListing::State::Unloaded);
    unload_->setText(unloaded ? tr("Yükle") : tr("Boşalt"));
    unload_->setToolTip(unloaded ? tr("Boşaltılan dış referansı geri getir")
                                 : tr("Seçili dış referansı çizimden çıkar; referansı yerinde, "
                                      "boş kalır"));
    reload_->setEnabled(picked && !unloaded);
    unload_->setEnabled(picked);
    repath_->setEnabled(picked);
    bind_->setEnabled(picked && !unloaded);
    detach_->setEnabled(picked);
}

void XrefPanel::onFileChanged(const QString& path)
{
    // A save by renaming — what KentOSCad itself does — takes the file out of
    // the watch; it goes back in as soon as the new one is there.
    if (QFileInfo::exists(path) && !watcher_->files().contains(path)) watcher_->addPath(path);
    QStringList names;
    for (auto it = paths_.cbegin(); it != paths_.cend(); ++it)
        if (it.value() == QDir::cleanPath(path)) names << it.key();
    if (names.isEmpty()) return;
    for (const QString& n : names)
        changed_.insert(n);
    refresh();
    names.sort();
    emit notice(tr("Dış referans dosyası değişti: %1 — yenilemek için Dış Referanslar "
                   "panelinde Yenile.")
                    .arg(names.join(QStringLiteral(", "))));
}

void XrefPanel::syncBanner()
{
    if (changed_.isEmpty()) {
        banner_->setVisible(false);
        return;
    }
    QStringList names(changed_.cbegin(), changed_.cend());
    names.sort();
    banner_->setText(
        names.size() == 1
            ? tr("'%1' dosyası kaydedildi; çizim önceki hâlini gösteriyor.").arg(names.front())
            : tr("%1 dosyaları kaydedildi; çizim önceki hâllerini gösteriyor.")
                  .arg(names.join(QStringLiteral(", "))));
    banner_->setVisible(true);
    fitBanner();
}

void XrefPanel::fitBanner()
{
    // AS TALL AS ITS WORDS AT THIS WIDTH. Its sentence wraps, and a banner
    // given the height of one line at a wider width showed the first two
    // lines of three in a narrow dock.
    if (!banner_->isVisible()) return;
    banner_->setMinimumHeight(0);
    const int want = banner_->hasHeightForWidth() ? banner_->heightForWidth(width())
                                                  : banner_->sizeHint().height();
    banner_->setMinimumHeight(want);
}

void XrefPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitBanner();
}

void XrefPanel::onCommandFinished(const QString& id, const QString& report)
{
    if (id != QLatin1String("core.xref")) return;
    auto parsed = core::Json::parse(report.toStdString());
    if (!parsed) return;
    const core::Json* op    = parsed.value().find("islem");
    const core::Json* names = parsed.value().find("adlar");
    if (op == nullptr || names == nullptr) return;
    // READ AGAIN, OR NOT READ AT ALL ANY MORE: either way the saved change is
    // no longer something the drawing is behind on.
    if (op->as_string() != "bosalt")
        for (const core::Json& n : names->as_array())
            changed_.remove(QString::fromStdString(n.as_string()));
    refresh();
}

void XrefPanel::runStep(const QString& op, const QString& name)
{
    if (name.isEmpty()) return;
    controller_.runLine(QStringLiteral("DIŞREFERANS islem=%1 ad=\"%2\"").arg(op, name),
                        command::Origin::Gui);
}

void XrefPanel::repathTo(const QString& name, const QString& file)
{
    if (name.isEmpty() || file.isEmpty()) return;
    controller_.runLine(
        QStringLiteral("DIŞREFERANS islem=yol ad=\"%1\" dosya=\"%2\"").arg(name, file),
        command::Origin::Gui);
}

void XrefPanel::toggleSeen(const QString& name)
{
    const QStringList layers = layersOf(name);
    if (layers.isEmpty()) return;
    bool seen                 = false;
    const core::Document& doc = controller_.document();
    for (const QString& l : layers) {
        const core::LayerId id = doc.find_layer(l.toStdString());
        seen = seen || (id != core::kNoLayer && doc.layer(id) != nullptr && doc.layer(id)->visible);
    }
    const QString verb = seen ? QStringLiteral("gizle") : QStringLiteral("goster");
    QStringList lines;
    for (const QString& l : layers)
        lines << QStringLiteral("KATMANGÖRÜNÜM islem=%1 katman=\"%2\"").arg(verb, l);
    controller_.runLines(lines, seen ? tr("Dış referansı gizle") : tr("Dış referansı göster"));
}

void XrefPanel::showMenu(const QPoint& at)
{
    QTreeWidgetItem* item = tree_->itemAt(at);
    if (item == nullptr) return;
    tree_->setCurrentItem(item);
    item->setSelected(true);
    QMenu menu(this);
    for (Button* b : {reload_, unload_, repath_, bind_, detach_}) {
        QAction* a = menu.addAction(b->text());
        a->setEnabled(b->isEnabled());
        connect(a, &QAction::triggered, b, &QPushButton::click);
    }
    // THE EYE IN WORDS, so a keyboard reaches it too: the menu key opens this
    // menu on the current row (ui.md R21).
    menu.addSeparator();
    const QString name = item->data(0, kNameRole).toString();
    QAction* eye = menu.addAction(item->data(0, kSeenRole).toBool() ? tr("Katmanlarını gizle")
                                                                    : tr("Katmanlarını göster"));
    eye->setEnabled(!layersOf(name).isEmpty());
    connect(eye, &QAction::triggered, this, [this, name] { toggleSeen(name); });
    menu.exec(tree_->viewport()->mapToGlobal(at));
}

bool XrefPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != tree_->viewport() || event->type() != QEvent::MouseButtonRelease)
        return QWidget::eventFilter(watched, event);
    const auto* mouse     = static_cast<QMouseEvent*>(event);
    QTreeWidgetItem* item = tree_->itemAt(mouse->position().toPoint());
    if (item == nullptr || !XrefRowDelegate::onEye(mouse->position().toPoint().x()))
        return QWidget::eventFilter(watched, event);
    toggleSeen(item->data(0, kNameRole).toString());
    return true;
}

bool XrefPanel::pressByHand(Button* button)
{
    if (button == nullptr || !button->isEnabled() || !button->isVisible()) return false;
    const QPointF at(button->width() / 2.0, button->height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, at, button->mapToGlobal(at), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, at, button->mapToGlobal(at), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &press);
    QCoreApplication::sendEvent(button, &release);
    QCoreApplication::processEvents();
    return true;
}

QStringList XrefPanel::probeRows() const
{
    QStringList out;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = tree_->topLevelItem(i);
        out << item->data(0, kNameRole).toString() + QLatin1Char('|') +
                   item->data(0, kBadgeRole).toString() + QLatin1Char('|') +
                   item->data(0, kCountRole).toString();
    }
    return out;
}

bool XrefPanel::probePress(const QString& name, const QString& step)
{
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        if (item->data(0, kNameRole).toString() != name) continue;
        tree_->setCurrentItem(item);
        item->setSelected(true);
        syncButtons();
        for (Button* b : {reload_, unload_, repath_, bind_, detach_})
            if (b->text() == step) return pressByHand(b);
    }
    return false;
}

bool XrefPanel::probeBanner(bool press)
{
    if (!banner_->isVisible()) return false;
    if (!press) return true;
    for (Button* b : banner_->findChildren<Button*>())
        return pressByHand(b);
    return false;
}

bool XrefPanel::probeEye(const QString& name)
{
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        if (item->data(0, kNameRole).toString() != name) continue;
        const QRect rect = tree_->visualItemRect(item);
        const QPointF at(rect.left() + kPadX + kEye / 2.0, rect.top() + 12.0);
        QMouseEvent press(QEvent::MouseButtonPress, at, tree_->viewport()->mapToGlobal(at),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, at, tree_->viewport()->mapToGlobal(at),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(tree_->viewport(), &press);
        QCoreApplication::sendEvent(tree_->viewport(), &release);
        QCoreApplication::processEvents();
        return true;
    }
    return false;
}

} // namespace kentos::app
