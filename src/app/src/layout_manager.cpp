// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_manager.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/core/document.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

QString quoted(const QString& raw)
{
    QString out = raw;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(out);
}

/// `A3 420×297 mm, yatay — 5 öğe`, which is what a row has to say for a reader
/// to tell two sheets apart without opening either.
QString describe(const core::Layout& l)
{
    if (l.pages.empty()) return QObject::tr("sayfasız");
    return QStringLiteral("%1 %2×%3 mm, %4 — %5 öğe%6")
        .arg(QString::fromStdString(l.paper))
        .arg(l.pages.front().w / 1000)
        .arg(l.pages.front().h / 1000)
        .arg(l.landscape ? QObject::tr("yatay") : QObject::tr("dikey"))
        .arg(l.items.size())
        .arg(l.pages.size() > 1 ? QObject::tr(", %1 sayfa").arg(l.pages.size()) : QString());
}

} // namespace

LayoutManager::LayoutManager(Controller& controller, QWidget* parent)
    : DialogFrame(parent), controller_(controller)
{
    setHeading(Glyph::Print, tr("Çıktı Yerleşimleri"));
    setBody(buildBody());
    resize(560, 420);

    auto* close = new Button(ButtonRole::Secondary, tr("Kapat"), std::nullopt, this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer()->addWidget(close);

    auto* fresh = new Button(ButtonRole::Primary, tr("Yeni çıktı yerleşimi…"), Glyph::Plus, this);
    connect(fresh, &QPushButton::clicked, this, [this] {
        bool accepted       = false;
        const QString named = QInputDialog::getText(
            this, tr("Yeni çıktı yerleşimi"), tr("Yerleşim adı:"), QLineEdit::Normal,
            tr("Yerleşim %1").arg(controller_.document().layouts().size() + 1), &accepted);
        if (!accepted || named.trimmed().isEmpty()) return;
        controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=%1 kagit=A3 yon=yatay")
                                .arg(quoted(named.trimmed())),
                            command::Origin::Gui);
        refresh();
        // STRAIGHT INTO THE DESIGNER, because nobody makes a layout in order to
        // look at its name in a list.
        emit openRequested(named.trimmed());
        accept();
    });
    footer()->addWidget(fresh);

    connect(&controller_, &Controller::documentChanged, this, [this] { refresh(); });
    refresh();
    applyTheme(theme());
}

QWidget* LayoutManager::buildBody()
{
    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(20, 16, 20, 12);
    column->setSpacing(10);

    column->addWidget(new FormSection(tr("ÇİZİMDEKİ ÇIKTIYERLEŞİMİLAR"),
                                      tr("yerleşim çizimle birlikte kaydedilir ve geri alınabilir"),
                                      body));

    list_ = new QListWidget(body);
    list_->setObjectName(QStringLiteral("layoutList"));
    list_->setFrameShape(QFrame::NoFrame);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int) {
        const bool any = !selected().isEmpty();
        for (Button* b : {open_, rename_, copy_})
            b->setEnabled(any);
        // THE LAST SHEET MAY GO, unlike the last print profile: a drawing with
        // no layout is an ordinary drawing, and one that could not lose its last
        // sheet would be a drawing you cannot undo a mistake out of.
        remove_->setEnabled(any);
    });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this] {
        if (selected().isEmpty()) return;
        emit openRequested(selected());
        accept();
    });
    column->addWidget(list_, 1);

    auto* actions = new QWidget(body);
    auto* row     = new QHBoxLayout(actions);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    open_ = new Button(ButtonRole::Secondary, tr("Aç"), Glyph::Open, actions);
    connect(open_, &QPushButton::clicked, this, [this] {
        if (selected().isEmpty()) return;
        emit openRequested(selected());
        accept();
    });

    rename_ = new Button(ButtonRole::Secondary, tr("Yeniden adlandır"), Glyph::Pencil, actions);
    connect(rename_, &QPushButton::clicked, this, [this] {
        const QString was = selected();
        if (was.isEmpty()) return;
        bool accepted = false;
        const QString now =
            QInputDialog::getText(this, tr("Yerleşimi yeniden adlandır"), tr("Yeni ad:"),
                                  QLineEdit::Normal, was, &accepted);
        if (!accepted || now.trimmed().isEmpty() || now.trimmed() == was) return;
        controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=ad ad=%1 yeni_ad=%2")
                                .arg(quoted(was), quoted(now.trimmed())),
                            command::Origin::Gui);
        refresh();
    });

    copy_ = new Button(ButtonRole::Secondary, tr("Çoğalt"), Glyph::Duplicate, actions);
    connect(copy_, &QPushButton::clicked, this, [this] {
        const core::Layout* source =
            controller_.document().layouts().find(selected().toStdString());
        if (source == nullptr) return;

        // TWO LINES, AND NO `islem=kopyala`. A copy is a new sheet plus its
        // items, and every one of them goes through the command the hand would
        // use — so the journal shows what was actually made rather than a verb
        // that hides it. One batch, so it is still one Ctrl+Z.
        const QString fresh = tr("%1 kopyası").arg(selected());
        QStringList lines;
        lines << QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=%1 kagit=%2 yon=%3 kenar=%4")
                     .arg(quoted(fresh),
                          source->paper.empty() ? QStringLiteral("A4")
                                                : QString::fromStdString(source->paper),
                          source->landscape ? QStringLiteral("yatay") : QStringLiteral("dikey"))
                     .arg(source->margin / 1000);

        // The new sheet arrives with the four default items; the copy's own
        // items replace them, so those are removed first.
        for (const char* born : {"baslik", "harita", "olcek", "kuzey"})
            lines << QStringLiteral("ÇIKTIÖĞE islem=sil yerlesim=%1 ad=%2")
                         .arg(quoted(fresh), QString::fromUtf8(born));

        for (const core::LayoutItem& item : source->items) {
            lines << QStringLiteral("ÇIKTIÖĞE islem=ekle yerlesim=%1 tur=%2 ad=%3")
                         .arg(quoted(fresh),
                              QString::fromUtf8(core::layout_item_kind_id(item.kind)),
                              quoted(QString::fromStdString(item.id)));
            QString set = QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 "
                                         "x=%3 y=%4 genislik=%5 yukseklik=%6 sira=%7")
                              .arg(quoted(fresh), quoted(QString::fromStdString(item.id)))
                              .arg(item.frame.x / 1000.0, 0, 'f', 1)
                              .arg(item.frame.y / 1000.0, 0, 'f', 1)
                              .arg(item.frame.w / 1000.0, 0, 'f', 1)
                              .arg(item.frame.h / 1000.0, 0, 'f', 1)
                              .arg(item.z);
            if (!item.text.empty())
                set += QStringLiteral(" metin=%1").arg(quoted(QString::fromStdString(item.text)));
            if (item.scale > 0) set += QStringLiteral(" olcek=%1").arg(item.scale);
            if (item.frame_visible) set += QStringLiteral(" cerceve=evet");
            if (item.locked) set += QStringLiteral(" kilit=evet");
            lines << set;
        }
        controller_.runLines(lines, tr("Çıktı yerleşimi çoğaltıldı"));
        refresh();
    });

    remove_ = new Button(ButtonRole::Danger, tr("Sil"), Glyph::Trash, actions);
    connect(remove_, &QPushButton::clicked, this, [this] {
        const QString name = selected();
        if (name.isEmpty()) return;
        if (QMessageBox::question(this, tr("Yerleşimi sil"),
                                  tr("'%1' yerleşimi bütün öğeleriyle silinsin mi? Tek Ctrl+Z "
                                     "ile geri alınabilir.")
                                      .arg(name)) != QMessageBox::Yes)
            return;
        controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=sil ad=%1").arg(quoted(name)),
                            command::Origin::Gui);
        refresh();
    });

    for (Button* b : {open_, rename_, copy_, remove_}) {
        b->setControlSize(ControlSize::Compact);
        b->setEnabled(false);
        row->addWidget(b);
    }
    row->addStretch(1);
    column->addWidget(actions);

    auto* note = new QLabel(tr("Her düğme bir ÇIKTIYERLEŞİMİ satırı çalıştırır; komut günlüğünde "
                               "görünür ve geri alınır."),
                            body);
    note->setObjectName(QStringLiteral("formHelp"));
    note->setWordWrap(true);
    column->addWidget(note);
    return body;
}

QString LayoutManager::selected() const
{
    const QListWidgetItem* row = list_->currentItem();
    return row != nullptr ? row->data(Qt::UserRole).toString() : QString();
}

void LayoutManager::refresh()
{
    if (list_ == nullptr) return;
    const QString was = selected();
    list_->clear();

    for (const core::Layout& l : controller_.document().layouts().all()) {
        const QString name = QString::fromStdString(l.name);
        auto* row = new QListWidgetItem(QStringLiteral("%1\n%2").arg(name, describe(l)), list_);
        row->setData(Qt::UserRole, name);
        if (name == was) list_->setCurrentItem(row);
    }

    if (list_->count() == 0) {
        auto* none = new QListWidgetItem(
            tr("Çizimde çıktı yerleşimi yok. “Yeni çıktı yerleşimi…” ile başlayın."), list_);
        none->setFlags(Qt::NoItemFlags);
    }
    const bool any = !selected().isEmpty();
    for (Button* b : {open_, rename_, copy_, remove_})
        if (b != nullptr) b->setEnabled(any);
}

void LayoutManager::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
}

} // namespace kentos::app
