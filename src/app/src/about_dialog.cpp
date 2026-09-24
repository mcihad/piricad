// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/about_dialog.hpp"

#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QClipboard>
#include <QFile>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

constexpr int kWindowWidth  = 620;
constexpr int kWindowHeight = 520;
constexpr int kMarkSize     = 64;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// A text the program carries in its resources, as it was written.
QString resourceText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll())
                                                            : QString();
}

/// THE PROGRAM'S MARK: a parcel and the north arrow of the sheet it is drawn on,
/// white on the accent — the two things every drawing this program makes has.
class Mark : public QWidget, public Themed
{
public:
    explicit Mark(QWidget* parent) : QWidget(parent)
    {
        setFixedSize(kMarkSize, kMarkSize);
        setAccessibleName(QObject::tr("KentOS CAD işareti"));
    }

    void applyTheme(ThemeMode mode) override
    {
        theme_ = mode;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        const Tokens& t = tokensOf(theme_);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(t.accent);
        p.drawRoundedRect(QRectF(rect()), 15.0, 15.0);

        const QColor ink = t.onAccent;
        // The parcel, with its corners.
        const QPolygonF parcel(
            {QPointF(14.0, 24.0), QPointF(38.0, 17.0), QPointF(47.0, 44.0), QPointF(19.0, 49.0)});
        QColor wash = ink;
        wash.setAlphaF(0.18F);
        p.setBrush(wash);
        p.setPen(QPen(ink, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolygon(parcel);
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        for (const QPointF& at : parcel)
            p.drawRect(QRectF(at.x() - 2.6, at.y() - 2.6, 5.2, 5.2));
        // And the north arrow over it.
        QPainterPath arrow;
        arrow.moveTo(50.0, 8.0);
        arrow.lineTo(54.5, 20.0);
        arrow.lineTo(50.0, 17.0);
        arrow.lineTo(45.5, 20.0);
        arrow.closeSubpath();
        p.drawPath(arrow);
    }

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// A page of plain text the user reads and may select: NOTICE, the licence.
QPlainTextEdit* textPage(const QString& text, const QString& name, QWidget* parent)
{
    auto* page = new QPlainTextEdit(parent);
    page->setObjectName(QStringLiteral("aboutText"));
    page->setReadOnly(true);
    page->setLineWrapMode(QPlainTextEdit::NoWrap);
    page->setPlainText(text);
    page->setAccessibleName(name);
    return page;
}

} // namespace

AboutDialog::AboutDialog(const AboutFacts& facts, QWidget* parent)
    : DialogFrame(parent), facts_(facts)
{
    setHeading(Glyph::Info, tr("KentOS CAD Hakkında"));
    resize(kWindowWidth, kWindowHeight);
    setMinimumSize(520, 420);

    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(24, 22, 24, 14);
    column->setSpacing(16);

    // ---- the name and the build ---------------------------------------------
    auto* hero = new QHBoxLayout();
    hero->setSpacing(16);
    auto* mark = new Mark(body);
    mark_      = mark;
    hero->addWidget(mark, 0, Qt::AlignTop);
    auto* names = new QVBoxLayout();
    names->setSpacing(2);
    auto* title = new QLabel(tr("KentOS CAD"), body);
    title->setObjectName(QStringLiteral("aboutTitle"));
    auto* tagline = new QLabel(tr("Türkiye odaklı CBS + CAD — kadastro, imar ve ölçme için"), body);
    tagline->setObjectName(QStringLiteral("aboutTagline"));
    auto* build = new QLabel(tr("Sürüm %1 · GPLv3 veya sonrası").arg(facts_.version), body);
    build->setObjectName(QStringLiteral("aboutBuild"));
    names->addWidget(title);
    names->addWidget(tagline);
    names->addSpacing(4);
    names->addWidget(build);
    names->addStretch(1);
    hero->addLayout(names, 1);
    column->addLayout(hero);

    // ---- the three pages ----------------------------------------------------
    pages_ = new Segment(body);
    pages_->addOption(tr("Genel"), tr("Bu derlemenin bilgileri"));
    pages_->addOption(tr("Bileşenler"), tr("Programın üzerine kurulduğu açık kaynak bileşenler"));
    pages_->addOption(tr("Lisans"), tr("GNU Genel Kamu Lisansı, sürüm 3"));
    pages_->setAccessibleName(tr("Hakkında sayfaları"));
    column->addWidget(pages_, 0, Qt::AlignLeft);

    stack_ = new QStackedWidget(body);
    stack_->setObjectName(QStringLiteral("aboutPages"));

    auto* general = new QWidget(stack_);
    general->setObjectName(QStringLiteral("aboutGeneral"));
    auto* grid = new QGridLayout(general);
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(8);
    const std::pair<QString, QString> rows[] = {
        {tr("Sürüm"), facts_.version},
        {tr("Qt"), facts_.qt},
        {tr("Çizim motoru"), facts_.backend},
        {tr("Platform"), facts_.platform},
        {tr("Komutlar"), tr("%1 komut, %2 işlem aracı").arg(facts_.commands).arg(facts_.tools)},
        {tr("Veri dizini"), facts_.dataRoot},
    };
    int at = 0;
    for (const auto& [key, value] : rows) {
        auto* k = new QLabel(key, general);
        k->setObjectName(QStringLiteral("aboutKey"));
        auto* v = new QLabel(value, general);
        v->setObjectName(QStringLiteral("aboutValue"));
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setWordWrap(true);
        grid->addWidget(k, at, 0, Qt::AlignLeft | Qt::AlignTop);
        grid->addWidget(v, at, 1);
        ++at;
    }
    // WHAT THE PROGRAM IS, in the one sentence its whole design follows.
    auto* idea = new QLabel(tr("Çizimin durumunu değiştiren her şey bir komuttur: şerit, komut "
                               "satırı, Python betiği ve yapay zekâ aynı komut yolunu kullanır "
                               "ve her komut aynı günlüğe yazılır. Resmî bir belge olan çıktı "
                               "her platformda bit bit aynıdır."),
                            general);
    idea->setObjectName(QStringLiteral("aboutIdea"));
    idea->setWordWrap(true);
    grid->addWidget(idea, at + 1, 0, 1, 2);
    grid->setRowMinimumHeight(at, 10);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(at + 2, 1);
    stack_->addWidget(general);

    QString notice = resourceText(QStringLiteral(":/about/NOTICE"));
    if (notice.isEmpty()) notice = tr("NOTICE dosyası bu derlemeye gömülmemiş.");
    stack_->addWidget(textPage(notice, tr("Bileşenler ve lisansları"), stack_));
    QString licence = resourceText(QStringLiteral(":/about/LICENSE"));
    if (licence.isEmpty()) licence = tr("LICENSE dosyası bu derlemeye gömülmemiş.");
    stack_->addWidget(textPage(licence, tr("Lisans metni"), stack_));
    column->addWidget(stack_, 1);
    connect(pages_, &Segment::currentChanged, stack_, &QStackedWidget::setCurrentIndex);

    setBody(body);

    // ---- the foot -----------------------------------------------------------
    auto* copy = new Button(ButtonRole::Secondary, tr("Bilgileri Kopyala"), Glyph::Duplicate, this);
    copy->setToolTip(tr("Sürüm, Qt, çizim motoru ve platform bilgisini bir hata bildirimine "
                        "yapıştırmak için panoya kopyalar"));
    connect(copy, &QAbstractButton::clicked, this,
            [this] { QGuiApplication::clipboard()->setText(factsText()); });
    footer()->insertWidget(0, copy);
    auto* close = new Button(ButtonRole::Primary, tr("Kapat"), std::nullopt, this);
    close->setDefault(true);
    connect(close, &QAbstractButton::clicked, this, &QDialog::accept);
    footer()->addWidget(close);
}

void AboutDialog::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
    applyThemeToChildren(this, mode);
}

QString AboutDialog::factsText() const
{
    return tr("KentOS CAD %1\nQt %2\nÇizim motoru: %3\nPlatform: %4\nKomutlar: %5 (%6 işlem "
              "aracı)")
        .arg(facts_.version, facts_.qt, facts_.backend, facts_.platform)
        .arg(facts_.commands)
        .arg(facts_.tools);
}

} // namespace kentos::app
