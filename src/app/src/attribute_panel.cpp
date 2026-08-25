// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/attribute_panel.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/app/icons.hpp"
#include "piricad/app/tokens.hpp"
#include "piricad/core/document.hpp"

#include <QFileInfo>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace piricad::app {
namespace {

// `design.md` §7, in the mockup's own numbers.
constexpr int kCardPadX    = 10;
constexpr int kCardPadY    = 9;
constexpr int kCaptionPx   = 10;
constexpr int kChip        = 26;
constexpr int kChipGap     = 8;
constexpr int kTitlePx     = 13;
constexpr int kSubtitlePx  = 11;
constexpr int kGroupPadX   = 10;
constexpr int kGroupPadY   = 6;
constexpr int kGroupHeight = 26;
constexpr int kRowHeight   = 26;
constexpr int kKeyWidth    = 112; ///< the `112px | 1fr` grid §7 fixes
constexpr int kRowPadX     = 10;
constexpr int kValuePadX   = 8;
constexpr int kBadgePadX   = 5;
constexpr int kBadgeHeight = 14;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

QFont sans(int px, QFont::Weight weight = QFont::Normal, qreal tracking = 0.0)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(px);
    f.setWeight(weight);
    if (tracking != 0.0) f.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
    return f;
}

QFont mono(int px)
{
    QFont f(QStringLiteral("IBM Plex Mono"));
    f.setPixelSize(px);
    f.setStyleHint(QFont::Monospace);
    return f;
}

QString metres(core::Mm v)
{
    return QString::number(static_cast<double>(v) / core::kMmPerMetre, 'f', 3);
}

} // namespace

AttributePanel::AttributePanel(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setObjectName(QStringLiteral("attributePanel"));
    setMouseTracking(true);
    setMinimumWidth(240);
    refresh();
}

void AttributePanel::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void AttributePanel::setLayer(core::LayerId layer)
{
    layer_ = layer;
    refresh();
}

void AttributePanel::refresh()
{
    groups_.clear();

    const core::Document& doc     = controller_.document();
    const command::Selection& sel = controller_.bus().selection();

    if (sel.empty() && layer_ != core::kNoLayer) {
        if (const core::Layer* l = doc.layer(layer_)) {
            glyph_    = static_cast<int>(Glyph::Layer);
            title_    = QString::fromStdString(l->name);
            subtitle_ = tr("katman · %1 nesne").arg(doc.layer_entity_count(layer_));

            AttributeGroup group{tr("KATMAN"), {}, true};
            group.rows.push_back({tr("gorunur"), l->visible ? tr("evet") : tr("hayır"), {}, false});
            group.rows.push_back({tr("kilitli"), l->locked ? tr("evet") : tr("hayır"),
                                  l->locked ? tr("KİLİT") : QString(), false});
            group.rows.push_back(
                {tr("renk"),
                 QStringLiteral("#%1").arg(l->appearance.rgba, 8, 16, QLatin1Char('0')).toUpper(),
                 {},
                 false});
            group.rows.push_back({tr("kalinlik"),
                                  tr("%1 mm").arg(l->appearance.width_um / 1000.0, 0, 'f', 2),
                                  {},
                                  false});
            group.rows.push_back(
                {tr("grup"),
                 l->group.empty() ? QStringLiteral("—") : QString::fromStdString(l->group),
                 l->group.empty() ? tr("BOŞ") : QString(), false});
            groups_.push_back(group);

            update();
            return;
        }
    }

    if (sel.empty()) {
        // Nothing picked: the card describes the DOCUMENT. A panel that empties
        // itself is a panel that looks broken, and the document's own facts are
        // what a user checks when nothing is selected anyway.
        glyph_ = static_cast<int>(Glyph::Document);
        title_ = controller_.currentFile().isEmpty()
                     ? tr("adsız çizim")
                     : QFileInfo(controller_.currentFile()).completeBaseName();
        subtitle_ =
            tr("%1 katman · %2 nesne").arg(doc.layers().size()).arg(doc.live_entity_count());

        AttributeGroup identity{tr("BELGE"), {}, true};
        identity.rows.push_back(
            {tr("koordinat_sistemi"), QString::fromStdString(doc.crs().id()), {}, false});
        identity.rows.push_back({tr("aktif_katman"), controller_.activeLayerName(), {}, false});
        identity.rows.push_back({tr("surum"), QString::number(doc.revision()), {}, false});
        groups_.push_back(identity);

        AttributeGroup extent{tr("KAPSAM"), {}, true};
        const core::Box2 box = doc.extent();
        if (box.empty()) {
            extent.rows.push_back({tr("durum"), tr("boş çizim"), tr("BOŞ"), false});
        } else {
            extent.rows.push_back({tr("saga_min"), metres(box.min_x), tr("HESAP"), true});
            extent.rows.push_back({tr("saga_max"), metres(box.max_x), tr("HESAP"), true});
            extent.rows.push_back({tr("yukari_min"), metres(box.min_y), tr("HESAP"), true});
            extent.rows.push_back({tr("yukari_max"), metres(box.max_y), tr("HESAP"), true});
        }
        groups_.push_back(extent);

        update();
        return;
    }

    const core::EntityKey key = sel.keys().front();
    const core::EntityId slot = doc.slot_of(key);

    glyph_ = static_cast<int>(Glyph::Polygon);
    title_ = sel.size() == 1 ? tr("Nesne %1").arg(static_cast<qulonglong>(key))
                             : tr("%1 nesne seçili").arg(sel.size());
    subtitle_ =
        sel.size() == 1 ? tr("slot %1").arg(static_cast<qulonglong>(slot)) : tr("çoklu seçim");

    // Every DECLARED column, in declaration order. Nothing here knows what a
    // column means — the panel shows what the catalogue put in the document,
    // which is the only way a legislation update stays a data release (5.13).
    AttributeGroup attrs{tr("ÖZNİTELİKLER"), {}, true};
    const core::AttrTable& table = doc.attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const core::AttrColumn* column = table.column(static_cast<core::AttrId>(c));
        if (!column) continue;

        const bool present = slot < column->rows() && column->present(slot);
        attrs.rows.push_back(
            {QString::fromStdString(column->spec().name_tr.empty() ? column->spec().id
                                                                   : column->spec().name_tr),
             present ? QString::fromUtf8(column->text(slot).data(),
                                         static_cast<int>(column->text(slot).size()))
                     : QStringLiteral("—"),
             present ? QString() : tr("BOŞ"), false});
    }
    if (attrs.rows.isEmpty())
        attrs.rows.push_back({tr("sütun"), tr("tanımlı değil"), tr("BOŞ"), false});
    groups_.push_back(attrs);

    update();
}

int AttributePanel::layout(QVector<QPair<int, int>>* headerBands) const
{
    // The card first, then every group header and its rows. One walk, so the
    // painter, the hit test and the scroll range can never disagree.
    int y = kCardPadY + kCaptionPx + 4 + kChip + kCardPadY + 1;

    for (int g = 0; g < groups_.size(); ++g) {
        if (headerBands) headerBands->push_back({y, kGroupHeight});
        y += kGroupHeight;
        if (groups_[g].open) y += static_cast<int>(groups_[g].rows.size()) * kRowHeight;
    }
    return y;
}

void AttributePanel::mousePressEvent(QMouseEvent* event)
{
    QVector<QPair<int, int>> bands;
    layout(&bands);

    const int at = static_cast<int>(event->position().y()) + scroll_;
    for (int g = 0; g < bands.size() && g < groups_.size(); ++g) {
        if (at < bands[g].first || at >= bands[g].first + bands[g].second) continue;
        groups_[g].open = !groups_[g].open;
        update();
        return;
    }
}

void AttributePanel::mouseMoveEvent(QMouseEvent* event)
{
    QVector<QPair<int, int>> bands;
    layout(&bands);

    const int at  = static_cast<int>(event->position().y()) + scroll_;
    const int was = hotGroup_;
    hotGroup_     = -1;
    for (int g = 0; g < bands.size(); ++g)
        if (at >= bands[g].first && at < bands[g].first + bands[g].second) hotGroup_ = g;
    if (hotGroup_ != was) update();
}

void AttributePanel::leaveEvent(QEvent*)
{
    hotGroup_ = -1;
    update();
}

void AttributePanel::wheelEvent(QWheelEvent* event)
{
    const int total = layout(nullptr);
    const int most  = std::max(0, total - height());
    scroll_         = std::clamp(scroll_ - event->angleDelta().y() / 2, 0, most);
    update();
}

void AttributePanel::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), t.bgPanel);
    p.translate(0, -scroll_);

    // ---- the selected-object card ----
    int y = kCardPadY;
    p.setFont(sans(kCaptionPx, QFont::DemiBold, 0.8));
    p.setPen(t.textFaint);
    p.drawText(QRect(kCardPadX, y, width(), kCaptionPx + 3), Qt::AlignLeft | Qt::AlignTop,
               tr("SEÇİLİ NESNE"));
    y += kCaptionPx + 6;

    const QRectF chip(kCardPadX, y, kChip, kChip);
    p.setBrush(t.accentWash);
    p.setPen(QPen(t.accent.darker(130), 1.0));
    p.drawRoundedRect(chip.adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);
    p.drawPixmap(QRect(int(chip.left()) + 5, int(chip.top()) + 5, 16, 16),
                 glyph_pixmap(static_cast<Glyph>(glyph_), t.accent, 16, devicePixelRatioF()));

    const int textLeft = kCardPadX + kChip + kChipGap;
    p.setFont(sans(kTitlePx, QFont::DemiBold));
    p.setPen(t.text);
    p.drawText(QRect(textLeft, y - 1, width() - textLeft - kCardPadX, kTitlePx + 3),
               Qt::AlignLeft | Qt::AlignTop, title_);

    p.setFont(mono(kSubtitlePx));
    p.setPen(t.textDim);
    p.drawText(QRect(textLeft, y + kTitlePx + 2, width() - textLeft - kCardPadX, kSubtitlePx + 3),
               Qt::AlignLeft | Qt::AlignTop, subtitle_);

    y += kChip + kCardPadY;
    p.fillRect(QRect(0, y, width(), 1), t.lineSoft);
    y += 1;

    // ---- the groups ----
    for (int g = 0; g < groups_.size(); ++g) {
        const AttributeGroup& group = groups_[g];

        p.fillRect(QRect(0, y, width(), kGroupHeight), hotGroup_ == g ? t.hoverRow : t.bgHeader);
        p.drawPixmap(QRect(kGroupPadX - 2, y + (kGroupHeight - 14) / 2, 14, 14),
                     glyph_pixmap(group.open ? Glyph::ChevronDown : Glyph::ChevronRight, t.textDim,
                                  14, devicePixelRatioF()));
        p.setFont(sans(kCaptionPx + 1, QFont::DemiBold, 0.8));
        p.setPen(t.text);
        p.drawText(QRect(kGroupPadX + 16, y, width(), kGroupHeight),
                   Qt::AlignVCenter | Qt::AlignLeft, group.title);
        p.fillRect(QRect(0, y + kGroupHeight - 1, width(), 1), t.lineHard);
        y += kGroupHeight;

        if (!group.open) continue;

        for (const AttributeRow& row : group.rows) {
            p.fillRect(QRect(kKeyWidth, y, 1, kRowHeight), t.lineSoft);

            p.setFont(sans(11));
            p.setPen(t.textDim);
            p.drawText(QRect(kRowPadX, y, kKeyWidth - kRowPadX * 2, kRowHeight),
                       Qt::AlignVCenter | Qt::AlignLeft, row.key);

            int right = width() - kValuePadX;
            if (!row.badge.isEmpty()) {
                p.setFont(sans(9, QFont::DemiBold, 0.5));
                const QFontMetrics badge(p.font());
                const int w = static_cast<int>(badge.horizontalAdvance(row.badge)) + kBadgePadX * 2;
                const QRectF box(right - w, y + (kRowHeight - kBadgeHeight) / 2.0, w, kBadgeHeight);

                // The badge says WHY the cell reads as it does: a derived number
                // is `HESAP` in the warn colour because editing it is meaningless,
                // an unfilled one is `BOŞ` in the faint one because it is simply
                // not known yet. Two different facts, never the same mark.
                p.setPen(Qt::NoPen);
                p.setBrush(row.derived ? QColor(t.warn.red(), t.warn.green(), t.warn.blue(), 38)
                                       : t.hoverIcon);
                p.drawRoundedRect(box, 2.5, 2.5);
                p.setPen(row.derived ? t.warn : t.textFaint);
                p.drawText(box, Qt::AlignCenter, row.badge);
                right -= w + 6;
            }

            p.setFont(mono(11));
            p.setPen(t.text);
            p.drawText(QRect(kKeyWidth + kValuePadX, y, right - kKeyWidth - kValuePadX, kRowHeight),
                       Qt::AlignVCenter | Qt::AlignLeft, row.value);

            p.fillRect(QRect(0, y + kRowHeight - 1, width(), 1), t.lineSoft);
            y += kRowHeight;
        }
    }
}

} // namespace piricad::app
