// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/about_dialog.hpp"

#include "piricad/app/tokens.hpp"
#include "piricad/app/widgets.hpp"

#include <QClipboard>
#include <QFile>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace piricad::app {
namespace {

constexpr int kWindowWidth  = 640;
constexpr int kWindowHeight = 600;

/// The logo's band at the top of the window. Tall enough that the word under
/// the emblem is read as a word, not as a caption.
constexpr int kBrandHeight = 168;

/// Alpha below this is the antialiasing haze round the mark, not the mark.
constexpr int kInk = 30;

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

/// The logo cut to its ink. The artwork carries a wide transparent margin, and
/// a margin inside the band would centre the mark somewhere it is not.
QImage inked(const QImage& logo)
{
    const QImage argb = logo.convertToFormat(QImage::Format_ARGB32);
    int left = argb.width(), top = argb.height(), right = -1, bottom = -1;
    for (int y = 0; y < argb.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x)
            if (qAlpha(line[x]) > kInk) {
                left   = std::min(left, x);
                right  = std::max(right, x);
                top    = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    }
    if (right < left || bottom < top) return argb;
    return argb.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
}

/// The mark for a dark window: the navy redrawn in `ink`, the water's blue kept.
///
/// The logo is two colours — a deep navy and a light blue — and the navy is
/// the one a dark ground swallows. Each pixel is placed between the two by its
/// lightness, so an edge where navy meets blue blends the way the artwork
/// blended it, and the alpha of every pixel is left exactly as it was drawn.
QImage onDark(const QImage& logo, const QColor& ink)
{
    constexpr double kNavy = 0.16; ///< the navy's lightness, near enough
    constexpr double kBlue = 0.52; ///< the water's
    QImage out             = logo.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const QColor was = QColor::fromRgba(line[x]);
            if (was.alpha() == 0) continue;
            const double keep = std::clamp(
                (static_cast<double>(was.lightnessF()) - kNavy) / (kBlue - kNavy), 0.0, 1.0);
            const auto mix = [keep](int from, int to) {
                return static_cast<int>(std::lround(from + (to - from) * keep));
            };
            line[x] = qRgba(mix(ink.red(), was.red()), mix(ink.green(), was.green()),
                            mix(ink.blue(), was.blue()), was.alpha());
        }
    }
    return out;
}

/// THE PROGRAM'S MARK: the logo as it was drawn, the compass emblem over the
/// word, centred in a band at the top of the window.
///
/// Drawn as it is on the light theme. On the dark one its navy would be a
/// shape nobody can see, so the navy is redrawn in the theme's own text ink
/// (`onDark`) — the same mark in both themes, rather than a white box pasted
/// onto a dark window.
class Brand : public QWidget, public Themed
{
public:
    explicit Brand(QWidget* parent) : QWidget(parent)
    {
        setFixedHeight(kBrandHeight);
        setAccessibleName(QObject::tr("PiriCAD logosu"));
        logo_ = inked(QImage(QStringLiteral(":/brand/data/images/piricad_logo.png")));
    }

    void applyTheme(ThemeMode mode) override
    {
        theme_ = mode;
        shown_ = QImage();
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        if (logo_.isNull()) return;
        // SCALED ONCE PER SIZE AND THEME, smoothly: a 1080 px artwork drawn into
        // a 160 px band by the painter's own filter comes out jagged.
        const qreal dpr  = devicePixelRatioF();
        const QSize room = (QSizeF(size()) * dpr).toSize();
        if (shown_.isNull() || shownFor_ != room) {
            const QImage source =
                theme_ == ThemeMode::Dark ? onDark(logo_, tokensOf(theme_).text) : logo_;
            shown_ = source.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            shown_.setDevicePixelRatio(dpr);
            shownFor_ = room;
        }
        QPainter p(this);
        const QSizeF drawn = QSizeF(shown_.size()) / dpr;
        p.drawImage(QPointF((width() - drawn.width()) / 2.0, (height() - drawn.height()) / 2.0),
                    shown_);
    }

private:
    QImage logo_;
    QImage shown_;
    QSize shownFor_;
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
    // THE NAME ON THIS WINDOW IS PiriCAD, the logo's own word; the rest of the
    // program keeps its name until the maintainer renames it (CLAUDE.md 0.5a).
    setHeading(Glyph::Info, tr("PiriCAD Hakkında"));
    resize(kWindowWidth, kWindowHeight);
    setMinimumSize(560, 520);

    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(24, 18, 24, 14);
    column->setSpacing(14);

    // ---- the mark, what it is and which build -------------------------------
    //
    // CENTRED, as a title page is: the logo stacks the emblem over the word,
    // and a stacked mark set against the left edge reads as a picture that
    // slipped. The tagline and the build line sit under it on the same axis.
    auto* mark = new Brand(body);
    mark_      = mark;
    column->addWidget(mark);
    auto* tagline = new QLabel(tr("Türkiye odaklı CBS + CAD — kadastro, imar ve ölçme için"), body);
    tagline->setObjectName(QStringLiteral("aboutTagline"));
    tagline->setAlignment(Qt::AlignHCenter);
    auto* build = new QLabel(tr("Sürüm %1 · GPLv3 veya sonrası").arg(facts_.version), body);
    build->setObjectName(QStringLiteral("aboutBuild"));
    build->setAlignment(Qt::AlignHCenter);
    auto* names = new QVBoxLayout();
    names->setSpacing(4);
    names->addWidget(tagline);
    names->addWidget(build);
    column->addLayout(names);
    column->addSpacing(4);

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
    return tr("PiriCAD %1\nQt %2\nÇizim motoru: %3\nPlatform: %4\nKomutlar: %5 (%6 işlem "
              "aracı)")
        .arg(facts_.version, facts_.qt, facts_.backend, facts_.platform)
        .arg(facts_.commands)
        .arg(facts_.tools);
}

} // namespace piricad::app
