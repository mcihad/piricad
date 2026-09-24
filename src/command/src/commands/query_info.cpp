// SPDX-License-Identifier: GPL-3.0-or-later
// core.entity_info — NESNEBİLGİ, and core.measure_angle — AÇIÖLÇ.
//
// NESNEBİLGİ ANSWERS "WHAT IS THIS". Click a parcel and the program says what it
// is, which layer it belongs to, how many corners it has, how long its boundary
// is, what its area is and what its attributes hold. Every one of those numbers
// already existed somewhere and none of them could be ASKED FOR in one question:
// the area came from ALANÖLÇ, the layer from a panel, the ada number from the
// attribute table — three roads for one glance.
//
// STRUCTURED (command.md R26), because the answer is a table. An agent, a script
// and a panel read the report rather than scraping the transcript, which is also
// what makes it a useful READ TOOL: a model that can ask what a thing is can
// talk about it without being told.
//
// THE TYPE'S NAME COMES FROM THE KIND TABLE, never from a switch in this file. A
// hand-written kind-to-Turkish table here would be a second list of the kinds
// (CLAUDE.md 5.10) and would silently print "bilinmeyen" for the next kind a
// plugin registers.
//
// AÇIÖLÇ MEASURES AND WRITES NOTHING. `ÖLÇÜ tur=acisal` DRAWS an angular
// dimension; this is the question a hand asks with a tape and then forgets — what
// is the angle at this corner — and its answer is written under the session's own
// convention (TODOS-CAD P0-4), so what it prints and what `@mesafe<açı` reads are
// one setting pair.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// An area in square metres to two decimals — the precision a tapu carries.
/// Divided in integers so the printed figure is the stored one rounded, never a
/// double that lost a digit on the way (Article 2.4).
std::string square_metres_text(core::Mm2 v)
{
    const bool negative     = v < 0;
    const auto abs_mm2      = static_cast<std::uint64_t>(negative ? -v : v);
    const std::uint64_t cm2 = (abs_mm2 + 5000) / 10000; // hundredths of a square metre
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return (negative ? "-" : "") + std::to_string(cm2 / 100) + "," + frac;
}

/// The kind's Turkish name, from the kind table's own `.names` (CLAUDE.md 2.6
/// order, so entry zero is the Turkish one).
const char* kind_word(core::KindId k)
{
    const core::KindSpec* spec = core::builtin_kinds().find(k);
    if (spec == nullptr || spec->names[0] == nullptr) return "bilinmeyen tür";
    return spec->names[0];
}

/// The feature of its object a dimension's point is tied to, in the words the
/// dimension page uses — without a case suffix, which after a number would have
/// to follow how the number is read aloud.
std::string anchor_word(const core::DimLink& l)
{
    switch (l.anchor) {
    case core::DimAnchor::Vertex: return "köşe " + std::to_string(l.index + 1);
    case core::DimAnchor::Centre: return "merkez";
    case core::DimAnchor::ArcStart: return "yay başı";
    case core::DimAnchor::ArcEnd: return "yay sonu";
    case core::DimAnchor::OnCircle: return "çember";
    }
    return "?";
}

/// The same feature as a stable word, for the report.
const char* anchor_id(core::DimAnchor a)
{
    switch (a) {
    case core::DimAnchor::Vertex: return "kose";
    case core::DimAnchor::Centre: return "merkez";
    case core::DimAnchor::ArcStart: return "yay_basi";
    case core::DimAnchor::ArcEnd: return "yay_sonu";
    case core::DimAnchor::OnCircle: return "cember";
    }
    return "?";
}

/// WHAT A DIMENSION IS TIED TO, AND WHAT IS TIED TO AN OBJECT (TODOS C-10): the
/// one question the canvas cannot answer by looking. A dimension says which of
/// its points follow which object, and which have lost theirs; an object says
/// which dimensions measure it — the thing to know before erasing it.
void describe_links(const core::Document& doc, core::EntityId slot, core::Json& row,
                    std::string& said)
{
    const core::DimLinkTable& table = doc.dimension_links();
    if (table.empty()) return;
    if (const std::vector<core::DimLink>* links = table.get(slot); links != nullptr) {
        core::Json out     = core::Json::array({});
        std::size_t broken = 0;
        std::string tied;
        for (const core::DimLink& l : *links) {
            core::Json one;
            one.set("nokta", core::Json::integer(static_cast<std::int64_t>(l.point) + 1));
            one.set("nesne", core::Json::integer(static_cast<std::int64_t>(core::raw(l.source))));
            one.set("ozellik", core::Json::string(anchor_id(l.anchor)));
            one.set("kopuk", core::Json::boolean(l.broken));
            out.push(std::move(one));
            if (l.broken) {
                ++broken;
                continue;
            }
            tied += (tied.empty() ? "" : ", ") + std::to_string(l.point + 1) + ". nokta → nesne " +
                    std::to_string(core::raw(l.source)) + " " + anchor_word(l);
        }
        row.set("olcu_baglari", std::move(out));
        if (!tied.empty()) said += "; bağları: " + tied;
        if (broken > 0)
            said += "; " + std::to_string(broken) +
                    " bağı kopuk (ölçtüğü nesne silinmiş; yazdığı değer onun eski ölçüsü)";
    }
    const core::EntityKey key = doc.entities().key[slot];
    core::Json measured_by    = core::Json::array({});
    std::size_t count         = 0;
    for (const core::EntityId dim : table.linked()) {
        if (!doc.alive(dim)) continue;
        for (const core::DimLink& l : *table.get(dim)) {
            if (l.broken || l.source != key) continue;
            measured_by.push(
                core::Json::integer(static_cast<std::int64_t>(core::raw(doc.entities().key[dim]))));
            ++count;
            break;
        }
    }
    if (count > 0) {
        row.set("olcen_olculer", std::move(measured_by));
        said += "; " + std::to_string(count) + " bağlı ölçü bunu ölçüyor";
    }
}

/// A dimension's MEASURED figure and its WRITTEN one, side by side (TODOS
/// C-10): the one question a sheet cannot answer by looking at it, because a
/// typed number and a measured one are printed in the same ink.
void describe_dimension(const core::Document& doc, core::EntityId slot, core::DrawingUnit unit,
                        core::Json& row, std::string& said)
{
    if (doc.entities().kind[slot] != core::kDimensionKind) return;
    const std::uint32_t gslot = doc.entities().slot[slot];
    auto decoded              = core::dimension_of(doc.geometry(), gslot);
    if (!decoded) return;
    const core::DimensionDef& def = decoded.value();
    const std::string measured    = core::dimension_value_text(def, unit);
    const std::string written(doc.texts().text(gslot));
    const bool manual = core::dimension_text_is_manual(def);

    core::Json dim;
    dim.set("tur", core::Json::string(core::dimension_type_name(def.type)));
    dim.set("olculen", core::Json::integer(def.measurement));
    dim.set("olculen_metin", core::Json::string(measured));
    dim.set("yazi", core::Json::string(written));
    dim.set("elle", core::Json::boolean(manual));
    if (!def.override_text.empty()) dim.set("metin", core::Json::string(def.override_text));
    if (!def.prefix.empty()) dim.set("onek", core::Json::string(def.prefix));
    if (!def.suffix.empty()) dim.set("sonek", core::Json::string(def.suffix));
    if (def.tolerance != core::DimTolerance::None) {
        core::Json tol;
        const char* how = "sinir";
        if (def.tolerance == core::DimTolerance::Symmetric) how = "simetrik";
        if (def.tolerance == core::DimTolerance::Deviation) how = "sapma";
        tol.set("bicim", core::Json::string(how));
        tol.set("ust", core::Json::integer(def.tolerance_plus));
        tol.set("alt", core::Json::integer(def.tolerance_minus));
        dim.set("tolerans", std::move(tol));
    }
    dim.set("birim",
            core::Json::string(core::dimension_is_angle(def)
                                   ? core::dimension_unit_word(def)
                                   : core::drawing_unit_name(core::dimension_unit(def, unit))));
    dim.set("hassasiyet", core::Json::integer(def.precision));
    dim.set("stil", core::Json::string(def.style));
    if (def.scale_basis > 0) dim.set("olcek", core::Json::integer(def.scale_basis));
    row.set("olcu", std::move(dim));

    said += "; ölçtüğü " + measured + ", yazdığı \"" + written + "\"";
    if (manual) said += " — ELLE YAZILMIŞ, ölçülen değer değil";
    if (def.scale_basis > 0) said += "; 1/" + std::to_string(def.scale_basis) + " paftası için";
}

/// WHAT A HATCH FOLLOWS, AND WHICH HATCHES FOLLOW AN OBJECT (TODOS C-11): the
/// pattern, the boundary objects, which of them are gone — and the one thing a
/// DXF's flag is not, a link this drawing can keep.
void describe_hatch(const core::Document& doc, core::EntityId slot, core::Json& row,
                    std::string& said)
{
    const core::HatchLinkTable& table = doc.hatch_links();
    if (doc.entities().kind[slot] == core::kHatchKind) {
        const std::vector<core::HatchSource>* sources = table.get(slot);
        auto def = core::hatch_of(doc.geometry(), doc.entities().slot[slot]);
        if (def) {
            core::Json hatch;
            hatch.set("desen", core::Json::string(def.value().name));
            hatch.set("aci_udeg", core::Json::integer(def.value().angle_udeg));
            hatch.set("olcek_pay", core::Json::integer(def.value().scale.num));
            hatch.set("olcek_payda", core::Json::integer(def.value().scale.den));
            hatch.set("dxf_iliskili", core::Json::boolean(def.value().associative));
            row.set("tarama", std::move(hatch));
            said += "; '" + def.value().name + "' deseni";
            if (def.value().associative && sources == nullptr)
                said += "; DXF'te ilişkili işaretli geldi, bu çizimde sınırına bağlı değil";
        }
        if (sources != nullptr) {
            core::Json out     = core::Json::array({});
            std::size_t broken = 0;
            std::string tied;
            for (const core::HatchSource& s : *sources) {
                core::Json one;
                one.set("nesne",
                        core::Json::integer(static_cast<std::int64_t>(core::raw(s.source))));
                one.set("kopuk", core::Json::boolean(s.broken));
                out.push(std::move(one));
                if (s.broken) {
                    ++broken;
                    continue;
                }
                tied += (tied.empty() ? "" : ", ") + std::to_string(core::raw(s.source));
            }
            row.set("tarama_sinirlari", std::move(out));
            if (!tied.empty()) said += "; sınırı: nesne " + tied;
            if (broken > 0)
                said += "; " + std::to_string(broken) +
                        " sınır bağı kopuk (nesnesi silinmiş ya da artık kapanmıyor)";
        }
    }
    if (table.empty()) return;
    const core::EntityKey key = doc.entities().key[slot];
    core::Json filled_by      = core::Json::array({});
    std::size_t count         = 0;
    for (const core::EntityId hatch : table.linked()) {
        if (!doc.alive(hatch)) continue;
        for (const core::HatchSource& s : *table.get(hatch)) {
            if (s.broken || s.source != key) continue;
            filled_by.push(core::Json::integer(
                static_cast<std::int64_t>(core::raw(doc.entities().key[hatch]))));
            ++count;
            break;
        }
    }
    if (count > 0) {
        row.set("taramalar", std::move(filled_by));
        said += "; " + std::to_string(count) + " bağlı tarama bunun sınırını izliyor";
    }
}

// ------------------------------------------------------------ NESNEBİLGİ ----

Task<void> run_entity_info(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesneler", "Bilgisi istenen nesneleri seçin, sonra Enter",
                               chosen, 0, "NESNEBİLGİ nesneler=1"))
        co_return;
    if (chosen.empty()) co_return;

    const core::Document& doc    = ctx.document();
    const core::AttrTable& attrs = doc.attributes();

    core::Json rows  = core::Json::array({});
    std::size_t told = 0;

    for (const std::int64_t raw : chosen) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            continue;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            continue;
        }

        const core::KindId kind   = doc.entities().kind[slot];
        const core::LayerId home  = doc.entities().layer[slot];
        const core::Layer* layer  = doc.layer(home);
        const std::uint32_t gslot = doc.entities().slot[slot];
        const core::RingSpan span = doc.geometry().rings_of(gslot);

        std::size_t vertices = 0;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            vertices += doc.geometry().ring_count[r];

        const core::Mm perimeter = doc.entity_perimeter(slot);
        const core::Mm2 area     = doc.entity_area(slot);
        const core::Box2 box     = doc.entities().box_of(slot);

        core::Json row;
        row.set("nesne", core::Json::integer(raw));
        row.set("tur", core::Json::string(kind_word(kind)));
        row.set("tur_no", core::Json::integer(static_cast<std::int64_t>(kind)));
        row.set("katman", core::Json::string(layer != nullptr ? layer->name : std::string("?")));
        row.set("halka", core::Json::integer(static_cast<std::int64_t>(span.count)));
        row.set("kose", core::Json::integer(static_cast<std::int64_t>(vertices)));
        row.set("cevre_mm", core::Json::integer(perimeter));
        row.set("alan_mm2", core::Json::integer(area));

        core::Json extent = core::Json::array({});
        extent.push(core::Json::integer(box.min_x));
        extent.push(core::Json::integer(box.min_y));
        extent.push(core::Json::integer(box.max_x));
        extent.push(core::Json::integer(box.max_y));
        row.set("kapsam", std::move(extent));

        // THE ATTRIBUTES TOO, because "what is this" is not answered by geometry
        // alone: a parcel's ada and parsel numbers are what it IS to a surveyor,
        // and reading them meant opening the table.
        core::Json cells   = core::Json::object({});
        std::size_t filled = 0;
        for (std::size_t c = 0; c < attrs.columns(); ++c) {
            const auto column           = static_cast<core::AttrId>(c);
            const core::AttrColumn* col = attrs.column(column);
            if (col == nullptr) continue;
            auto held = doc.attribute(column, slot);
            if (!held || !held.value().present) continue;
            ++filled;
            cells.set(col->spec().id, core::Json::string(core::attr_display(held.value())));
        }
        if (filled > 0) row.set("oznitelik", std::move(cells));

        std::string said = "Nesne " + std::to_string(raw) + " — " + kind_word(kind) + ", katman " +
                           (layer != nullptr ? layer->name : std::string("?"));
        if (vertices > 0) said += ", " + std::to_string(vertices) + " köşe";
        if (perimeter > 0) said += ", çevre " + metres_text(perimeter) + " m";
        if (area != 0) said += ", alan " + square_metres_text(area) + " m²";
        if (filled > 0) said += ", " + std::to_string(filled) + " öznitelik";
        describe_dimension(doc, slot, ctx.session().bus().drawing_unit(), row, said);
        describe_links(doc, slot, row, said);
        describe_hatch(doc, slot, row, said);
        ctx.echo(said + ".");

        rows.push(std::move(row));
        ++told;
    }

    if (told == 0) {
        ctx.refuse(core::ErrorCode::NotFound, "Bilgisi verilecek nesne bulunamadı.");
        co_return;
    }

    core::Json report;
    report.set("adet", core::Json::integer(static_cast<std::int64_t>(told)));
    report.set("nesneler", std::move(rows));
    report.set("surum", core::Json::integer(static_cast<std::int64_t>(doc.revision())));
    ctx.report(std::move(report));
}

// ---------------------------------------------------------------- AÇIÖLÇ ----

Task<void> run_measure_angle(Context& ctx)
{
    // THREE POINTS: the vertex and a point on each arm. "Two lines" is the other
    // form a plan gives, and it reduces to this one the moment the lines are
    // picked — so three points is what is asked for, and a line's end is what a
    // snap puts under the cursor.
    auto vertex = co_await ctx.point("tepe", "Açının tepe noktası");
    if (!vertex) co_return;
    auto first = co_await ctx.point("birinci", "Birinci kolun üzerinde bir nokta",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *vertex,
                                                 .rubber_shape  = RubberShape::Line});
    if (!first) co_return;
    // THE FIRST ARM STAYS, AND THE SWEEP IS DRAWN. Both arms were previewed as a
    // plain line from the vertex, so while the second was aimed the first had
    // left the screen — and the one thing the command measures, the angle
    // BETWEEN them, was nowhere on the canvas until the answer was already in
    // the transcript.
    auto second = co_await ctx.point("ikinci", "İkinci kolun üzerinde bir nokta",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *vertex,
                                                  .rubber_shape  = RubberShape::Angle,
                                                  .rubber_chain  = {*first}});
    if (!second) co_return;

    if (*first == *vertex || *second == *vertex) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Bir kolun noktası tepeyle aynı: o kolun doğrultusu yok."));
        co_return;
    }

    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    // THE TWO DIRECTIONS AND THE SWEEP BETWEEN THEM, all in turns — which add,
    // where readings in a unit do not. The sweep runs from the first arm to the
    // second IN THE RULE'S OWN DIRECTION (clockwise under semt, counter-clockwise
    // under matematik), folded into [0, 1). The other way round is reported
    // beside it, because "the angle at this corner" is two numbers whenever the
    // corner is reflex, and a program that picked one silently would be deciding
    // which side of a boundary is inside.
    const double from_first  = core::direction_turns(*vertex, *first, convention.rule);
    const double from_second = core::direction_turns(*vertex, *second, convention.rule);
    double between           = from_second - from_first;
    between -= std::floor(between);
    const double reflex = between == 0.0 ? 0.0 : 1.0 - between;

    const core::Mm arm_a = core::segment_length(*vertex, *first);
    const core::Mm arm_b = core::segment_length(*vertex, *second);

    core::Json report;
    report.set("aci_udeg", core::Json::integer(static_cast<std::int64_t>(std::llround(
                               between * static_cast<double>(core::kUDegFullCircle)))));
    report.set("ters_udeg", core::Json::integer(static_cast<std::int64_t>(std::llround(
                                reflex * static_cast<double>(core::kUDegFullCircle)))));
    report.set("aci_metin", core::Json::string(core::angle_text(between, convention.unit)));
    report.set("ters_metin", core::Json::string(core::angle_text(reflex, convention.unit)));
    report.set("birinci_kenar_mm", core::Json::integer(arm_a));
    report.set("ikinci_kenar_mm", core::Json::integer(arm_b));
    report.set("birim",
               core::Json::string(std::string(1, core::angle_unit_suffix(convention.unit))));
    report.set("kural", core::Json::string(core::angle_rule_label(convention.rule)));
    ctx.report(std::move(report));

    ctx.record("tepe", Value::point(*vertex));
    ctx.record("birinci", Value::point(*first));
    ctx.record("ikinci", Value::point(*second));

    ctx.echo("Açı: " + core::angle_text(between, convention.unit) +
             "   (ters: " + core::angle_text(reflex, convention.unit) +
             ")   kenarlar: " + metres_text(arm_a) + " m ve " + metres_text(arm_b) + " m   (" +
             core::angle_rule_label(convention.rule) + ")");

    // AND THE ANGLE STAYS DRAWN where it was read: the two arms, the sweep and
    // the figure on it, the way the preview showed it while it was being aimed.
    ctx.mark(MeasureMark{.shape  = MeasureMark::Shape::Angle,
                         .points = {*vertex, *first, *second},
                         .labels = {core::angle_text(between, convention.unit)}});
}

} // namespace

KENTOS_COMMAND(entity_info)
{
    return CommandSpec{
        .id       = "core.entity_info",
        .names    = {"NESNEBİLGİ", "NESNEBILGI", "OBJINFO", "NB"},
        .title    = "Nesne Bilgisi",
        .category = Category::Query,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Bilgisi istenen nesneler"}
                         .en("objects")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly | Flags::NoEffect |
                 Flags::AiAccessible,
        .summary = "Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve "
                   "özniteliklerini bildirir.",
        .run     = &run_entity_info,
    };
}

KENTOS_COMMAND(measure_angle)
{
    return CommandSpec{
        .id = "core.measure_angle",
        // `AÖ` IS ALANÖLÇ'S and must not be taken here: a registration that
        // collides only writes a log line, so claiming it made ALANÖLÇ vanish
        // from a clean build with a green suite. `AÇÖ` is this command's own.
        .names    = {"AÇIÖLÇ", "ACIOLC", "MEASUREANGLE", "AÇÖ"},
        .title    = "Açı Ölç",
        .category = Category::Query,
        .params =
            {
                Param::point("tepe", "Açının tepe noktası").en("apex"),
                Param::point("birinci", "Birinci kolun üzerinde bir nokta").en("first"),
                Param::point("ikinci", "İkinci kolun üzerinde bir nokta").en("second"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly | Flags::NoEffect |
                 Flags::AiAccessible,
        .summary = "Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla "
                   "yazar.",
        .run = &run_measure_angle,
    };
}

} // namespace kentos::command
