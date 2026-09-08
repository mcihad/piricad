// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/attribute_panel.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/measure_text.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"

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

} // namespace

// The five of them now live in `measure_text.hpp`, because the pick chooser says
// the same things about the same entities and two answers to "how big is it" is
// one answer too many.
using measure::kindName;
using measure::metres;
using measure::metresWithUnit;
using measure::shapeName;
using measure::squareMetres;

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
    rebuild();

    // WHAT THE USER OPENED STAYS OPEN. `rebuild()` makes every group afresh, so
    // without this a surveyor who opened GEOMETRİ to read an area would find it
    // shut again on the next parcel they clicked — at exactly the moment they
    // wanted it open. A group nobody has touched keeps the default the builder
    // chose: NESNE open, the rest closed, so the panel opens on what the object
    // IS rather than on four screens of numbers.
    for (AttributeGroup& group : groups_) {
        const auto remembered = disclosed_.constFind(group.title);
        if (remembered != disclosed_.constEnd()) group.open = *remembered;
    }

    update();
}

void AttributePanel::rebuild()
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
                                  field_of(FieldKind::Bool)});
            group.rows.push_back({tr("kilitli"), l->locked ? tr("evet") : tr("hayır"),
                                  l->locked ? tr("KİLİT") : QString(), false,
                                  tr("KATMAN ad=\"%1\" kilitli=%2").arg(name),
                                  field_of(FieldKind::Bool)});
            group.rows.push_back(
                {tr("renk"),
                 QStringLiteral("#%1").arg(l->appearance.rgba, 8, 16, QLatin1Char('0')).toUpper(),
                 {},
                 false,
                 tr("KATMAN ad=\"%1\" renk=%2").arg(name),
                 field_of(FieldKind::Colour)});
            group.rows.push_back({tr("kalinlik"),
                                  tr("%1 mm").arg(l->appearance.width_um / 1000.0, 0, 'f', 2),
                                  {},
                                  false,
                                  tr("STİL katman=\"%1\" kalinlik=%2").arg(name),
                                  {}});
            group.rows.push_back(
                {tr("grup"),
                 l->group.empty() ? QStringLiteral("—") : QString::fromStdString(l->group),
                 l->group.empty() ? tr("BOŞ") : QString(),
                 false,
                 tr("KATMAN ad=\"%1\" grup=\"%2\"").arg(name),
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
                                 {}});
        identity.rows.push_back({tr("aktif_katman"),
                                 controller_.activeLayerName(),
                                 {},
                                 false,
                                 tr("KATMAN ad=\"%1\""),
                                 combo_of(layerNames)});
        identity.rows.push_back(
            {tr("surum"), QString::number(doc.revision()), tr("HESAP"), true, {}, {}});
        groups_.push_back(identity);

        AttributeGroup extent{tr("KAPSAM"), {}, true};
        const core::Box2 box = doc.extent();
        if (box.empty()) {
            extent.rows.push_back({tr("durum"), tr("boş çizim"), tr("BOŞ"), false, {}, {}});
        } else {
            extent.rows.push_back({tr("saga_min"), metres(box.min_x), tr("HESAP"), true, {}, {}});
            extent.rows.push_back({tr("saga_max"), metres(box.max_x), tr("HESAP"), true, {}, {}});
            extent.rows.push_back({tr("yukari_min"), metres(box.min_y), tr("HESAP"), true, {}, {}});
            extent.rows.push_back({tr("yukari_max"), metres(box.max_y), tr("HESAP"), true, {}, {}});
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

    if (sel.size() == 1 && slot != core::kNoEntity && doc.alive(slot)) {
        const core::EntityTable& rows = doc.entities();
        const std::uint32_t gslot     = rows.slot[slot];
        const core::KindId kind       = rows.kind[slot];
        const core::LayerId on        = rows.layer[slot];
        const core::Layer* layer      = doc.layer(on);

        subtitle_ = tr("%1 · %2").arg(shapeName(doc, kind, gslot),
                                      layer != nullptr ? QString::fromStdString(layer->name)
                                                       : tr("katmansız"));

        // ---- what it IS -------------------------------------------------
        //
        // Read-only throughout. A panel row becomes editable by carrying the
        // command that changes it, and there is no command that changes an
        // object's KIND or its key — those are identity, not properties
        // (model.md R2). The layer has one and says so.
        AttributeGroup what{tr("NESNE"), {}, true}; // the one group that opens by default
        what.rows.push_back({tr("kimlik"),
                             QString::number(static_cast<qulonglong>(key)),
                             tr("SABİT"),
                             true,
                             {},
                             {}});
        what.rows.push_back({tr("tur"), shapeName(doc, kind, gslot), {}, true, {}, {}});

        QStringList layerNames;
        for (const core::Layer& l : doc.layers())
            layerNames << QString::fromStdString(l.name);
        what.rows.push_back(
            {tr("katman"),
             layer != nullptr ? QString::fromStdString(layer->name) : QStringLiteral("—"),
             {},
             false,
             QStringLiteral("KATMANAT nesneler=%1 katman=\"%2\"")
                 .arg(static_cast<qulonglong>(key))
                 .arg(QStringLiteral("%1")),
             combo_of(layerNames)});

        // KATMANDAN means the entity has no style of its own and draws with its
        // layer's — which is a different statement from "no style at all", and
        // the one a user needs before they wonder why a STİL on the object did
        // nothing.
        const core::StyleId style = rows.style[slot];
        if (style == core::kByLayerStyle) {
            what.rows.push_back({tr("stil"), tr("katmandan"), tr("MİRAS"), true, {}, {}});
        } else {
            const core::Symbol& sym = doc.styles().symbol_at(style);
            what.rows.push_back({tr("stil"),
                                 tr("#%1 · %2 katman").arg(style).arg(sym.layers.size()),
                                 {},
                                 true,
                                 {},
                                 {}});
            if (sym.max_scale != 0 || sym.min_scale != 0)
                what.rows.push_back(
                    {tr("olcek_penceresi"),
                     tr("1:%1 – 1:%2")
                         .arg(sym.min_scale == 0 ? tr("∞") : QString::number(sym.min_scale))
                         .arg(sym.max_scale == 0 ? tr("∞") : QString::number(sym.max_scale)),
                     {},
                     true,
                     {},
                     {}});
        }
        what.rows.push_back({tr("gorunur"),
                             rows.visible(slot) ? tr("evet") : tr("hayır"),
                             rows.visible(slot) ? QString() : tr("GİZLİ"),
                             true,
                             {},
                             {}});
        groups_.push_back(what);

        // ---- what it MEASURES -------------------------------------------
        //
        // THE KIND ANSWERS FOR THE AREA, so a circle reports pi*r² rather than
        // the area of whatever polygon it happens to be drawn with, and an arc
        // reports nothing because it encloses nothing. Same call `ALANÖLÇ`
        // makes — one measurement, one implementation.
        core::Mm2 area{0};
        core::Mm length = doc.geometry().perimeter_of(gslot);
        if (const core::KindSpec* spec = core::builtin_kinds().find(kind); spec != nullptr) {
            const std::uint32_t one[1]{gslot};
            spec->area(doc.geometry(), core::SlotSpan(one, 1), std::span<core::Mm2>(&area, 1));
            // And for the length too: the stored run of a circle is its radius.
            if (spec->perimeter != nullptr)
                spec->perimeter(doc.geometry(), core::SlotSpan(one, 1),
                                std::span<core::Mm>(&length, 1));
        }

        const core::RingSpan rings = doc.geometry().rings_of(gslot);
        std::size_t vertices       = 0;
        for (std::uint32_t r = 0; r < rings.count; ++r)
            vertices += doc.geometry().ring_xs(rings.first + r).size();

        AttributeGroup shape{tr("GEOMETRİ"), {}, false};
        shape.rows.push_back({tr("kose"), QString::number(vertices), tr("HESAP"), true, {}, {}});
        shape.rows.push_back(
            {tr("halka"), QString::number(rings.count), tr("HESAP"), true, {}, {}});
        // A closed ring's length is its perimeter and an open one's is its
        // length; naming both `uzunluk` would make a parcel's boundary read as a
        // distance somebody walked.
        shape.rows.push_back({area != 0 ? tr("cevre") : tr("uzunluk"),
                              metresWithUnit(length),
                              tr("HESAP"),
                              true,
                              {},
                              {}});
        if (area != 0)
            shape.rows.push_back({tr("alan"), squareMetres(area), tr("HESAP"), true, {}, {}});
        groups_.push_back(shape);

        // ---- where it IS -------------------------------------------------
        const core::Box2 box = doc.entity_extent(slot);
        if (!box.empty()) {
            // ONE ROW PER EDGE, and the range form was tried and abandoned: a TM
            // easting and a TM northing are ten and eleven digits, and two of them
            // with a dash between do not fit the 200 px this column has. A value
            // that is elided is worse than a row that is scrolled to.
            AttributeGroup where{tr("KAPSAM"), {}, false};
            where.rows.push_back({tr("saga_min"), metres(box.min_x), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("saga_max"), metres(box.max_x), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("yukari_min"), metres(box.min_y), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("yukari_max"), metres(box.max_y), tr("HESAP"), true, {}, {}});
            where.rows.push_back(
                {tr("genislik"), metresWithUnit(box.max_x - box.min_x), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("yukseklik"),
                                  metresWithUnit(box.max_y - box.min_y),
                                  tr("HESAP"),
                                  true,
                                  {},
                                  {}});
            groups_.push_back(where);
        }

        // ---- what it SAYS ------------------------------------------------
        //
        // Only when there is text. An empty METİN group on every parcel is a
        // group a user learns to scroll past, and then misses on the label that
        // does carry one.
        if (doc.texts().has(gslot)) {
            AttributeGroup says{tr("METİN"), {}, false};

            // EDITABLE NOW, and the note that stood here said exactly why it was
            // not: "a row becomes editable when a command exists that changes it,
            // and this one does not yet". `YAZIDÜZENLE` is that command. `METİN`
            // still only DRAWS — offering it here would have put a second label
            // on top of the first.
            const auto id = QString::number(
                static_cast<qulonglong>(static_cast<std::uint64_t>(doc.key_of(slot))));

            says.rows.push_back({tr("icerik"),
                                 QString::fromStdString(std::string(doc.texts().text(gslot))),
                                 {},
                                 false,
                                 tr("YAZIDÜZENLE nesneler=%1 yazi=\"%2\"").arg(id),
                                 {}});

            // MILLIMETRES IN THE EDITOR, metres in the reading: the command takes
            // ground millimetres and a user editing this row is answering the
            // command, not the label.
            says.rows.push_back({tr("yukseklik"),
                                 metresWithUnit(doc.texts().height(gslot)),
                                 {},
                                 false,
                                 tr("YAZIDÜZENLE nesneler=%1 yukseklik=%2").arg(id),
                                 {}});
            groups_.push_back(says);
        }
    } else if (sel.size() > 1) {
        // MANY OBJECTS: the totals, which is what a user selects a block of
        // parcels to find out. Per-object rows would be a table, and there is one
        // of those — `ÖZNİTELİK TABLOSU` — rather than a second one in a panel
        // 312 px wide.
        core::Mm2 area{0};
        core::Mm length{0};
        std::size_t alive = 0;
        core::Box2 box{};
        core::LayerId only = core::kNoLayer;
        bool mixed         = false;

        for (const core::EntityKey k : sel.keys()) {
            const core::EntityId e = doc.slot_of(k);
            if (e == core::kNoEntity || !doc.alive(e)) continue;
            ++alive;

            const std::uint32_t gslot = doc.entities().slot[e];
            if (const core::KindSpec* spec = core::builtin_kinds().find(doc.entities().kind[e]);
                spec != nullptr) {
                core::Mm2 one_area{0};
                const std::uint32_t one[1]{gslot};
                spec->area(doc.geometry(), core::SlotSpan(one, 1),
                           std::span<core::Mm2>(&one_area, 1));
                area += one_area;
            }
            length += doc.geometry().perimeter_of(gslot);

            const core::Box2 each = doc.entity_extent(e);
            if (!each.empty()) {
                box.extend(core::Point2{each.min_x, each.min_y});
                box.extend(core::Point2{each.max_x, each.max_y});
            }

            const core::LayerId on = doc.entities().layer[e];
            if (only == core::kNoLayer)
                only = on;
            else if (on != only)
                mixed = true;
        }

        const core::Layer* layer = mixed ? nullptr : doc.layer(only);
        subtitle_ =
            mixed ? tr("karışık katman")
                  : (layer != nullptr ? QString::fromStdString(layer->name) : tr("çoklu seçim"));

        AttributeGroup sum{tr("SEÇİM"), {}, true}; // the multi-selection headline
        sum.rows.push_back({tr("nesne"), QString::number(alive), tr("HESAP"), true, {}, {}});
        sum.rows.push_back(
            {tr("katman"),
             mixed ? tr("karışık")
                   : (layer != nullptr ? QString::fromStdString(layer->name) : QStringLiteral("—")),
             mixed ? tr("KARIŞIK") : QString(),
             true,
             {},
             {}});
        sum.rows.push_back(
            {tr("toplam_uzunluk"), metresWithUnit(length), tr("HESAP"), true, {}, {}});
        sum.rows.push_back({tr("toplam_alan"), squareMetres(area), tr("HESAP"), true, {}, {}});
        groups_.push_back(sum);

        if (!box.empty()) {
            AttributeGroup where{tr("KAPSAM"), {}, false};
            where.rows.push_back({tr("saga_min"), metres(box.min_x), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("saga_max"), metres(box.max_x), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("yukari_min"), metres(box.min_y), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("yukari_max"), metres(box.max_y), tr("HESAP"), true, {}, {}});
            where.rows.push_back(
                {tr("genislik"), metresWithUnit(box.max_x - box.min_x), tr("HESAP"), true, {}, {}});
            where.rows.push_back({tr("yukseklik"),
                                  metresWithUnit(box.max_y - box.min_y),
                                  tr("HESAP"),
                                  true,
                                  {},
                                  {}});
            groups_.push_back(where);
        }
    } else {
        subtitle_ = tr("silinmiş nesne");
    }

    // Every DECLARED column, in declaration order. Nothing here knows what a
    // column means — the panel shows what the catalogue put in the document,
    // which is the only way a legislation update stays a data release (5.13).
    AttributeGroup attrs{tr("ÖZNİTELİKLER"), {}, false};
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

        // THROUGH THE ONE FORMATTER, and it was not. This panel printed the raw
        // stored integer, which is right for a count and wrong for everything
        // else: a length came out in millimetres where the whole document says
        // metres, and a fixed-point rate came out as `40` where the plan note
        // says `0,40`. `core::attr_display` is the function that knows, and it is
        // the same one the attribute table and every export use.
        //
        // The POINT, not the comma. What is shown here is what the editor opens
        // with and what the command receives back, so it has to be the form the
        // command parses — the paper form belongs on paper.
        const QString shown = present ? QString::fromStdString(core::attr_display(
                                            stored.value(), core::DecimalMark::Point))
                                      : QStringLiteral("—");

        // WHAT THE COLUMN IS DECIDES WHAT OPENS. A `tarih` column gets a calendar,
        // an `evet_hayir` gets the two-word segment. Read from the ONE mapping in
        // `fields.hpp`, which the attribute table reads too: two copies of it is
        // how one of them keeps offering a line edit for a date long after the
        // other stopped.
        const FieldSpec editor = field_for(column->spec());

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
             shown, present ? QString() : tr("BOŞ"), false,
             QStringLiteral("ÖZNİTELİK ad=\"%1\" nesne=%2 deger=\"%3\"")
                 .arg(QString::fromStdString(column->spec().id))
                 .arg(static_cast<qulonglong>(key))
                 .arg(QStringLiteral("%1")),
             editor});
    }
    if (attrs.rows.isEmpty())
        attrs.rows.push_back({tr("sütun"), tr("tanımlı değil"), tr("BOŞ"), false, {}, {}});
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
    if (row.command.isEmpty()) return;

    const QRect box = rowRect(group, index);
    if (box.isEmpty()) return;

    editingGroup_ = group;
    editingRow_   = index;

    // ONE EDITOR PER KIND, and it is rebuilt when the kind changes rather than
    // kept and reconfigured: a date field and a number field differ in what they
    // hold as much as in how they look, and a widget carrying the leftovers of
    // the last row is how a calendar ends up over a floor count.
    if (editor_ != nullptr && editor_->kind() != row.field.kind) {
        editor_->deleteLater();
        editor_ = nullptr;
    }
    if (editor_ == nullptr) {
        // AS A CELL, not as a form control. The editor is replacing a value that
        // was already painted in this row, so what it must not do is look like a
        // box that appeared on top of the panel — see `FieldFrame`.
        editor_ = new Field(as_cell(row.field), this);
        editor_->applyTheme(theme_);
        connect(editor_, &Field::committed, this, &AttributePanel::commitAndAdvance);
        connect(editor_, &Field::cancelled, this, [this] { closeEditor(); });
    }

    // EXACTLY THE CELL. Not inset by two pixels, not a widget floating over the
    // row: the value rectangle the row was painted with, so the only thing that
    // changes on screen is that the value became selectable.
    editor_->setGeometry(box);

    // An empty cell reads as `—`; putting that in the box would make the user
    // delete a character that was never a value.
    editor_->setValue(row.value == QStringLiteral("—") ? QString() : row.value);
    editor_->show();
    editor_->beginEditing();
    update();
}

bool AttributePanel::nextEditable(int group, int index, int& outGroup, int& outIndex)
{
    int g = group;
    int r = index + 1;
    for (; g < groups_.size(); ++g, r = 0)
        for (; r < groups_[g].rows.size(); ++r) {
            if (groups_[g].rows[r].command.isEmpty()) continue;

            // OPENED ON THE WAY PAST. A collapsed group has no row rectangle and
            // `beginEdit` would refuse it — and a row the user cannot see is not
            // a row they were about to fill in.
            groups_[g].open = true;
            outGroup        = g;
            outIndex        = r;
            return true;
        }
    return false;
}

void AttributePanel::commitAndAdvance(const QString& value)
{
    // WHERE WE WERE, taken before the commit: `commitEdit` refreshes, which
    // rebuilds `groups_` and clears the editing position.
    const int wasGroup = editingGroup_;
    const int wasRow   = editingRow_;

    commitEdit(value);

    if (wasGroup < 0) return;

    int nextGroup = -1;
    int nextRow   = -1;
    if (!nextEditable(wasGroup, wasRow, nextGroup, nextRow)) return;

    // AFTER THE LAYOUT HAS CAUGHT UP. The group above may have just been opened,
    // and `beginEdit` asks for a rectangle that only exists once the panel has
    // laid itself out again.
    update();
    beginEdit(nextGroup, nextRow);
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

QStringList AttributePanel::probeRowKeys() const
{
    QStringList out;
    for (const AttributeGroup& group : groups_)
        for (const AttributeRow& row : group.rows)
            out << row.key;
    return out;
}

bool AttributePanel::openRowForProbe(const QString& key)
{
    for (int g = 0; g < groups_.size(); ++g)
        for (int r = 0; r < groups_[g].rows.size(); ++r) {
            if (groups_[g].rows[r].key != key || groups_[g].rows[r].command.isEmpty()) continue;

            // A COLLAPSED GROUP HAS NO ROW RECTANGLE, and `beginEdit` refuses a
            // row it cannot place. Opening the group is what a user does before
            // clicking the row, so the probe does it too.
            groups_[g].open = true;
            beginEdit(g, r);
            return editingGroup_ == g && editingRow_ == r;
        }
    return false;
}

bool AttributePanel::editRowForProbe(const QString& key, const QString& value)
{
    for (int g = 0; g < groups_.size(); ++g)
        for (int r = 0; r < groups_[g].rows.size(); ++r) {
            const AttributeRow& row = groups_[g].rows[r];
            if (row.key != key || row.command.isEmpty()) continue;

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
    if (editor_ != nullptr) editor_->hide();
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
        disclosed_.insert(groups_[g].title, groups_[g].open);
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
        if (current >= 0 && groups_[hotRowGroup_].rows[hotRow_].field.kind == FieldKind::Bool)
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

            // THE ROW BEING EDITED PAINTS NO VALUE. The editor sits exactly on
            // the value rectangle, and painting the stored text under it left the
            // old value and the typed one on top of each other — legibly enough
            // to read both and not enough to read either. The key and the rules
            // still paint: what the editor replaces is the value, not the row.
            const bool open = g == editingGroup_ && r == editingRow_;

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
            if (!row.badge.isEmpty() && !open) {
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
            const bool editable = !row.command.isEmpty();
            p.setPen(editable ? t.text : t.textDim);
            if (!open)
                p.drawText(
                    QRect(kKeyWidth + kValuePadX, y, right - kKeyWidth - kValuePadX, kRowHeight),
                    Qt::AlignVCenter | Qt::AlignLeft, row.value);

            p.fillRect(QRect(0, y + kRowHeight - 1, width(), 1), t.lineSoft);
            y += kRowHeight;
        }
    }
}

} // namespace kentos::app
