// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/measure_text.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/entity_kind.hpp"

#include <span>

#include <QCoreApplication>

namespace kentos::app::measure {
namespace {

/// `Q_DECLARE_TR_FUNCTIONS` is a CLASS macro — it emits `public:` — and these are
/// free functions in a namespace. `QCoreApplication::translate` is the same
/// mechanism `tr()` compiles down to and `lupdate` reads it the same way, so the
/// strings still reach `kentos_tr.ts` (Article 6.9).
QString tr(const char* text)
{
    return QCoreApplication::translate("kentos::app::measure", text);
}

} // namespace

QString metres(core::Mm v)
{
    return QString::number(static_cast<double>(v) / core::kMmPerMetre, 'f', 3);
}

QString metresWithUnit(core::Mm v)
{
    return metres(v) + QStringLiteral(" m");
}

QString squareMetres(core::Mm2 v)
{
    return QString::number(core::mm2_to_m2(v), 'f', 2) + QStringLiteral(" m²");
}

QString kindName(core::KindId kind)
{
    if (const core::KindSpec* spec = core::builtin_kinds().find(kind); spec != nullptr) {
        if (spec->names[0] != nullptr && *spec->names[0] != 0)
            return QString::fromUtf8(spec->names[0]);
        return QString::fromUtf8(spec->stable_id);
    }
    return tr("bilinmeyen tür (%1)").arg(kind);
}

QString shapeName(const core::Document& doc, core::KindId kind, std::uint32_t gslot)
{
    const QString family = kindName(kind);
    if (kind != core::kPolylineKind) return family;

    const core::RingSpan rings = doc.geometry().rings_of(gslot);
    bool face                  = false;
    bool holes                 = false;
    for (std::uint32_t r = 0; r < rings.count; ++r) {
        const core::RingRole role = doc.geometry().ring_role[rings.first + r];
        if (role == core::RingRole::Exterior) face = true;
        if (role == core::RingRole::Interior) holes = true;
    }

    if (!face) return family;
    return holes ? tr("ALAN (delikli)") : tr("ALAN");
}

QString spacedThousands(const QString& text)
{
    // The LEADING number, measured by hand rather than by `toDouble` on the whole
    // string: `toDouble` refuses `1600.00 m²` outright, and that is the shape
    // half the callers pass.
    qsizetype end = 0;
    if (end < text.size() && text[end] == QLatin1Char('-')) ++end;

    qsizetype dot = -1;
    while (end < text.size()) {
        const QChar ch = text[end];
        if (ch.isDigit()) {
            ++end;
        } else if (ch == QLatin1Char('.') && dot < 0) {
            dot = end;
            ++end;
        } else {
            break;
        }
    }

    // No fraction, no grouping: an identifier stays the way it was written.
    if (dot < 0 || end == 0) return text;

    QString out          = text;
    const qsizetype stop = out.startsWith(QLatin1Char('-')) ? 1 : 0;
    for (qsizetype at = dot - 3; at > stop; at -= 3)
        out.insert(at, QLatin1Char(' '));
    return out;
}

QString sizeSummary(const core::Document& doc, core::EntityId entity)
{
    if (entity >= doc.entities().size() || !doc.alive(entity)) return {};

    const core::KindId kind     = doc.entities().kind[entity];
    const std::uint32_t gslot   = doc.entities().slot[entity];
    const core::RingGeometry& g = doc.geometry();

    // A CURVE IS ITS RADIUS. Two circles around one monument differ by radius and
    // by nothing else a list can show; their areas are both "a circle" and their
    // bounding boxes are concentric.
    if (kind == core::kCircleKind)
        return tr("r %1").arg(metresWithUnit(core::circle_radius_of(g, gslot)));
    if (kind == core::kArcKind)
        return tr("r %1").arg(metresWithUnit(core::arc_radius_of(g, gslot)));

    // THE KIND ANSWERS, so a shape reports the area its own definition gives it
    // rather than the area of whatever polygon it is drawn with. Same call
    // `ALANÖLÇ` and the property panel make — one measurement, one implementation.
    core::Mm2 area{0};
    core::Mm length = g.perimeter_of(gslot);
    if (const core::KindSpec* spec = core::builtin_kinds().find(kind); spec != nullptr) {
        const std::uint32_t one[1]{gslot};
        spec->area(g, core::SlotSpan(one, 1), std::span<core::Mm2>(&area, 1));
        if (spec->perimeter != nullptr)
            spec->perimeter(g, core::SlotSpan(one, 1), std::span<core::Mm>(&length, 1));
    }

    if (area != 0) return squareMetres(area);
    if (length != 0) return metresWithUnit(length);

    // A point encloses nothing and runs nowhere, and saying `0.000 m` about one
    // would be a measurement rather than the absence of one.
    return {};
}

} // namespace kentos::app::measure
