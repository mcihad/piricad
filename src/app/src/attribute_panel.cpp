// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/attribute_panel.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/core/document.hpp"

#include <QColorDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QWheelEvent>

#include <algorithm>

namespace kentos::app {
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
    setFocusPolicy(Qt::StrongFocus); // ui.md R21: the panel is a keyboard surface
    setAccessibleName(tr("Öznitelikler"));
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

            // EVERY ROW THAT HAS A COMMAND BEHIND IT IS EDITABLE, and the command
            // is the whole mechanism: the panel never touches the document.
            const QString name = QString::fromStdString(l->name);

            AttributeGroup group{tr("KATMAN"), {}, true};
            group.rows.push_back({tr("gorunur"),
                                  l->visible ? tr("evet") : tr("hayır"),
                                  {},
                                  false,
                                  tr("KATMAN ad=\"%1\" gorunur=%2").arg(name),
                                  EditKind::Bool,
                                  {}});
            group.rows.push_back({tr("kilitli"),
                                  l->locked ? tr("evet") : tr("hayır"),
                                  l->locked ? tr("KİLİT") : QString(),
                                  false,
                                  tr("KATMAN ad=\"%1\" kilitli=%2").arg(name),
                                  EditKind::Bool,
                                  {}});
            group.rows.push_back(
                {tr("renk"),
                 QStringLiteral("#%1").arg(l->appearance.rgba, 8, 16, QLatin1Char('0')).toUpper(),
                 {},
                 false,
                 tr("KATMAN ad=\"%1\" renk=%2").arg(name),
                 EditKind::Colour,
                 {}});
            group.rows.push_back({tr("kalinlik"),
                                  tr("%1 mm").arg(l->appearance.width_um / 1000.0, 0, 'f', 2),
                                  {},
                                  false,
                                  tr("STİL katman=\"%1\" kalinlik=%2").arg(name),
                                  EditKind::Text,
                                  {}});
            group.rows.push_back(
                {tr("grup"),
                 l->group.empty() ? QStringLiteral("—") : QString::fromStdString(l->group),
                 l->group.empty() ? tr("BOŞ") : QString(),
                 false,
                 tr("KATMAN ad=\"%1\" grup=\"%2\"").arg(name),
                 EditKind::Text,
                 {}});
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

        QStringList layerNames;
        for (const core::Layer& l : doc.layers())
            layerNames << QString::fromStdString(l.name);

        AttributeGroup identity{tr("BELGE"), {}, true};
        identity.rows.push_back({tr("koordinat_sistemi"),
                                 QString::fromStdString(doc.crs().id()),
                                 {},
                                 false,
                                 tr("AYAR ad=koordinat_sistemi deger=%1"),
                                 EditKind::Text,
                                 {}});
        identity.rows.push_back({tr("aktif_katman"),
                                 controller_.activeLayerName(),
                                 {},
                                 false,
                                 tr("KATMAN ad=\"%1\""),
                                 EditKind::Choice,
                                 layerNames});
        identity.rows.push_back({tr("surum"),
                                 QString::number(doc.revision()),
                                 tr("HESAP"),
                                 true,
                                 {},
                                 EditKind::None,
                                 {}});
        groups_.push_back(identity);

        AttributeGroup extent{tr("KAPSAM"), {}, true};
        const core::Box2 box = doc.extent();
        if (box.empty()) {
            extent.rows.push_back(
                {tr("durum"), tr("boş çizim"), tr("BOŞ"), false, {}, EditKind::None, {}});
        } else {
            extent.rows.push_back(
                {tr("saga_min"), metres(box.min_x), tr("HESAP"), true, {}, EditKind::None, {}});
            extent.rows.push_back(
                {tr("saga_max"), metres(box.max_x), tr("HESAP"), true, {}, EditKind::None, {}});
            extent.rows.push_back(
                {tr("yukari_min"), metres(box.min_y), tr("HESAP"), true, {}, EditKind::None, {}});
            extent.rows.push_back(
                {tr("yukari_max"), metres(box.max_y), tr("HESAP"), true, {}, EditKind::None, {}});
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

        // THROUGH THE DOCUMENT, NOT INTO THE COLUMN. An attribute column is
        // indexed by GEOMETRY slot (`Document::set_attribute` writes
        // `entities_.slot[e]`), and this panel was reading it with the ENTITY
        // slot. The two agree only while every entity was created in order and
        // none was edited — so the panel showed the right value on a fresh
        // drawing and a DIFFERENT parcel's value on a real one.
        //
        // `Document::attribute` does the mapping, and it is the only reader that
        // can be right by construction.
        const auto stored  = doc.attribute(static_cast<core::AttrId>(c), slot);
        const bool present = stored.ok() && stored.value().present;
        const QString shown =
            present ? (stored.value().type == core::AttrType::Text ||
                               stored.value().type == core::AttrType::CodeRef
                           ? QString::fromStdString(stored.value().text)
                           : QString::number(static_cast<qlonglong>(stored.value().number)))
                    : QStringLiteral("—");

        // ONE COMMAND PER OBJECT, and the object is named by its PERMANENT key
        // rather than by the slot it happens to occupy: a slot is a storage
        // detail that a later edit may reuse, and a journal replay that resolved
        // one would write the value onto a different parsel (model.md R2).
        //
        // Written for the first selected object. A multi-object edit sends one
        // command per key, which is what `commitEdit` does.
        attrs.rows.push_back(
            {QString::fromStdString(column->spec().name_tr.empty() ? column->spec().id
                                                                   : column->spec().name_tr),
             shown,
             present ? QString() : tr("BOŞ"),
             false,
             QStringLiteral("ÖZNİTELİK ad=\"%1\" nesne=%2 deger=\"%3\"")
                 .arg(QString::fromStdString(column->spec().id))
                 .arg(static_cast<qulonglong>(key))
                 .arg(QStringLiteral("%1")),
             EditKind::Text,
             {}});
    }
    if (attrs.rows.isEmpty())
        attrs.rows.push_back(
            {tr("sütun"), tr("tanımlı değil"), tr("BOŞ"), false, {}, EditKind::None, {}});
    groups_.push_back(attrs);

    update();
}

int AttributePanel::layout(QVector<QPair<int, int>>* headerBands,
                           QVector<std::array<int, 3>>* rowBands) const
{
    // The card first, then every group header and its rows. One walk, so the
    // painter, the hit test and the scroll range can never disagree.
    int y = kCardPadY + kCaptionPx + 4 + kChip + kCardPadY + 1;

    for (int g = 0; g < groups_.size(); ++g) {
        if (headerBands) headerBands->push_back({y, kGroupHeight});
        y += kGroupHeight;
        if (!groups_[g].open) continue;

        for (int r = 0; r < groups_[g].rows.size(); ++r) {
            if (rowBands) rowBands->push_back({y, g, r});
            y += kRowHeight;
        }
    }
    return y;
}

QRect AttributePanel::rowRect(int group, int index) const
{
    QVector<std::array<int, 3>> rows;
    layout(nullptr, &rows);
    for (const auto& band : rows)
        if (band[1] == group && band[2] == index)
            return QRect(kKeyWidth, band[0] - scroll_, width() - kKeyWidth, kRowHeight);
    return {};
}

void AttributePanel::beginEdit(int group, int index)
{
    if (group < 0 || group >= groups_.size()) return;
    if (index < 0 || index >= groups_[group].rows.size()) return;

    const AttributeRow& row = groups_[group].rows[index];
    if (row.command.isEmpty() || row.edit == EditKind::None) return;

    editingGroup_ = group;
    editingRow_   = index;

    // A BOOLEAN HAS NO EDITOR. Opening a text box to type "evet" would be a worse
    // control than the one word already on the row: activating it flips it.
    if (row.edit == EditKind::Bool) {
        const bool now = row.value == tr("evet");
        commitEdit(now ? tr("hayır") : tr("evet"));
        return;
    }

    if (row.edit == EditKind::Colour) {
        const QColor before = QColor::fromRgba(row.value.mid(1).toUInt(nullptr, 16));
        const QColor picked = QColorDialog::getColor(before, this, tr("Katman rengi"),
                                                     QColorDialog::ShowAlphaChannel);
        if (!picked.isValid()) {
            closeEditor();
            return;
        }
        // 0xAARRGGBB, which is what KATMAN renk= reads.
        commitEdit(QStringLiteral("0x%1").arg(picked.rgba(), 8, 16, QLatin1Char('0')).toUpper());
        return;
    }

    if (row.edit == EditKind::Choice) {
        bool ok = false;
        const QString value =
            QInputDialog::getItem(this, row.key, tr("Yeni değer"), row.choices,
                                  static_cast<int>(row.choices.indexOf(row.value)), false, &ok);
        if (!ok) {
            closeEditor();
            return;
        }
        commitEdit(value);
        return;
    }

    if (editor_ == nullptr) {
        editor_ = new QLineEdit(this);
        editor_->setObjectName(QStringLiteral("attributeEditor"));
        connect(editor_, &QLineEdit::returnPressed, this, [this] { commitEdit(editor_->text()); });

        auto* giveUp = new QShortcut(QKeySequence(Qt::Key_Escape), editor_);
        giveUp->setContext(Qt::WidgetShortcut);
        connect(giveUp, &QShortcut::activated, this, [this] { closeEditor(); });
    }

    const QRect box = rowRect(group, index);
    if (box.isEmpty()) {
        closeEditor();
        return;
    }
    editor_->setGeometry(box.adjusted(2, 2, -2, -2));
    // An empty cell reads as `—`; putting that in the box would make the user
    // delete a character that was never a value.
    editor_->setText(row.value == QStringLiteral("—") ? QString() : row.value);
    editor_->selectAll();
    editor_->show();
    editor_->setFocus(Qt::OtherFocusReason);
    update();
}

void AttributePanel::commitEdit(const QString& value)
{
    if (editingGroup_ < 0 || editingGroup_ >= groups_.size()) return;
    if (editingRow_ < 0 || editingRow_ >= groups_[editingGroup_].rows.size()) return;

    const QString command = groups_[editingGroup_].rows[editingRow_].command;
    closeEditor();
    if (command.isEmpty()) return;

    // THE ONLY ROAD OUT OF THIS PANEL. The cell builds a command line and hands
    // it to the bus; it never writes to the document. Editing `ada_no` here and
    // typing the same line at the command prompt are the same write, land in the
    // same journal and undo in one step (CLAUDE.md 1.1, 5.9).
    controller_.runLine(command.arg(value), command::Origin::Gui);
    refresh();
}

bool AttributePanel::editRowForProbe(const QString& key, const QString& value)
{
    for (int g = 0; g < groups_.size(); ++g)
        for (int r = 0; r < groups_[g].rows.size(); ++r) {
            const AttributeRow& row = groups_[g].rows[r];
            if (row.key != key || row.command.isEmpty() || row.edit == EditKind::None) continue;

            editingGroup_ = g;
            editingRow_   = r;
            commitEdit(value);
            return true;
        }
    return false;
}

void AttributePanel::closeEditor()
{
    editingGroup_ = -1;
    editingRow_   = -1;
    if (editor_ != nullptr) {
        editor_->hide();
        editor_->clear();
    }
    setFocus(Qt::OtherFocusReason);
    update();
}

void AttributePanel::mousePressEvent(QMouseEvent* event)
{
    QVector<QPair<int, int>> bands;
    QVector<std::array<int, 3>> rows;
    layout(&bands, &rows);

    const int at = static_cast<int>(event->position().y()) + scroll_;
    for (int g = 0; g < bands.size() && g < groups_.size(); ++g) {
        if (at < bands[g].first || at >= bands[g].first + bands[g].second) continue;
        groups_[g].open = !groups_[g].open;
        closeEditor();
        update();
        return;
    }

    // A single click SELECTS the row; the double click opens it. Opening on the
    // first click would put an editor under every pointer that crossed the panel.
    for (const auto& band : rows) {
        if (at < band[0] || at >= band[0] + kRowHeight) continue;
        hotRowGroup_ = band[1];
        hotRow_      = band[2];
        setFocus(Qt::MouseFocusReason);
        update();
        return;
    }
}

void AttributePanel::mouseDoubleClickEvent(QMouseEvent* event)
{
    QVector<std::array<int, 3>> rows;
    layout(nullptr, &rows);

    const int at = static_cast<int>(event->position().y()) + scroll_;
    for (const auto& band : rows) {
        if (at < band[0] || at >= band[0] + kRowHeight) continue;
        hotRowGroup_ = band[1];
        hotRow_      = band[2];
        beginEdit(band[1], band[2]);
        return;
    }
}

void AttributePanel::keyPressEvent(QKeyEvent* event)
{
    // ui.md R21: everything here is reachable with no mouse at all.
    QVector<std::array<int, 3>> rows;
    layout(nullptr, &rows);
    if (rows.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    int current = -1;
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i][1] == hotRowGroup_ && rows[i][2] == hotRow_) current = i;

    const auto move = [&](int to) {
        const int clamped = std::clamp(to, 0, static_cast<int>(rows.size()) - 1);
        hotRowGroup_      = rows[clamped][1];
        hotRow_           = rows[clamped][2];

        // Follow the selection with the viewport, or the keyboard walks off the
        // bottom of a panel that never scrolls.
        const int top = rows[clamped][0];
        if (top - scroll_ < 0) scroll_ = top;
        if (top - scroll_ + kRowHeight > height()) scroll_ = top + kRowHeight - height();
        update();
    };

    switch (event->key()) {
    case Qt::Key_Down: move(current + 1); return;
    case Qt::Key_Up: move(current < 0 ? 0 : current - 1); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
        if (current >= 0) beginEdit(hotRowGroup_, hotRow_);
        return;
    case Qt::Key_Space:
        if (current >= 0 && groups_[hotRowGroup_].rows[hotRow_].edit == EditKind::Bool)
            beginEdit(hotRowGroup_, hotRow_);
        return;
    default: break;
    }
    QWidget::keyPressEvent(event);
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

        for (int r = 0; r < group.rows.size(); ++r) {
            const AttributeRow& row = group.rows[r];

            // design.md §2's one selected-row pattern, used by every list in the
            // program: an accent wash plus a 2 px inset edge. Stated with a shape
            // as well as a colour, per §13.
            if (g == hotRowGroup_ && r == hotRow_) {
                p.fillRect(QRect(0, y, width(), kRowHeight - 1), t.accentWash);
                p.fillRect(QRect(0, y, 2, kRowHeight - 1), t.accent);
            }

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

            // An editable value is written in the READING ink; one that cannot be
            // edited is a step back. The reader can tell what this panel will let
            // them change without clicking anything to find out.
            const bool editable = !row.command.isEmpty() && row.edit != EditKind::None;
            p.setPen(editable ? t.text : t.textDim);
            p.drawText(QRect(kKeyWidth + kValuePadX, y, right - kKeyWidth - kValuePadX, kRowHeight),
                       Qt::AlignVCenter | Qt::AlignLeft, row.value);

            p.fillRect(QRect(0, y + kRowHeight - 1, width(), 1), t.lineSoft);
            y += kRowHeight;
        }
    }
}

} // namespace kentos::app
