// SPDX-License-Identifier: GPL-3.0-or-later
// core.to_area (ALANAÇEVİR) — turns a closed run of lines into one face.
//
// This is how a parcel usually arrives. A boundary comes in from a DXF, off a
// total station, or out of somebody's hand as SEPARATE line segments that happen
// to meet at their ends; the drawing LOOKS like a parsel and the document holds
// four unrelated polylines. Nothing can be said about it: an open ring encloses
// nothing, so it has no area, no perimeter that closes, and no place in an ifraz
// (model.md R9, R10).
//
// So the lines are chained end to end and, if the chain closes, ONE face replaces
// them. The source lines are erased in the same transaction, because a boundary
// that exists twice — once as a face and once as the lines under it — is a
// topology error waiting to be exported.
//
// THE TOLERANCE IS THE PROJECT'S, not this command's. Two ends a tenth of a
// millimetre apart are one corner or two depending on a number, and that number
// changes the coordinates an ifraz produces — which is why `core.topoloji.
// dugum_toleransi` is project-scoped and why this reads it rather than inventing
// one (settings R40).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/geometry.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One source line, as a run of vertices that may be walked either way round.
struct Strand
{
    core::EntityId entity{core::kNoEntity};
    std::vector<core::Point2> points;
    bool used{false};
};

bool within(core::Point2 a, core::Point2 b, core::Mm tol)
{
    // Chebyshev, not Euclid, and on purpose: the comparison stays in integers, so
    // two documents on two machines can never disagree about whether two corners
    // are the same corner (Article 2.4, §7.3). A square tolerance box a tenth of a
    // millimetre wide is the same answer as a round one at every scale a cadastral
    // drawing is worked at.
    const auto dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const auto dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx <= tol && dy <= tol;
}

/// Collects the OPEN rings of the requested entities. A face is refused rather
/// than ignored: asking to convert something that is already converted is a
/// mistake worth naming.
bool collect(Context& ctx, const std::vector<std::int64_t>& ids, std::vector<Strand>& out)
{
    const core::Document& doc = ctx.document();

    for (std::int64_t raw : ids) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            return false;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            return false;
        }

        // A curve's vertices are its definition — a centre, a radius handle, and
        // for an arc its two ends — not a run of boundary points. Chaining them
        // would build a face out of numbers that never described one.
        if (doc.entities().kind[slot] != core::kPolylineKind) {
            ctx.echo("Nesne " + std::to_string(raw) +
                     " bir eğri; eğri çizgi gibi birleştirilemez.");
            return false;
        }

        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
        if (span.count != 1) {
            ctx.echo("Nesne " + std::to_string(raw) +
                     " tek parçalı bir çizgi değil; ALANAÇEVİR yalnız açık çizgileri "
                     "birleştirir.");
            return false;
        }
        if (doc.geometry().ring_role[span.first] != core::RingRole::Open) {
            ctx.echo("Nesne " + std::to_string(raw) +
                     " zaten kapalı bir alan. Kapalı bir alan yeniden çevrilmez.");
            return false;
        }

        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        if (xs.size() < 2) continue;

        Strand s;
        s.entity = slot;
        s.points.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            s.points.push_back(core::Point2{xs[v], ys[v]});
        out.push_back(std::move(s));
    }
    return true;
}

Task<void> run(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    std::vector<std::int64_t> requested;
    if (const Value given = ctx.argument("nesneler"); !given.empty()) {
        requested = given.as_ids();
    } else {
        // The active selection, which is the select-then-act order every CAD user
        // works in. Copied out here so the journal records the ids: a replay must
        // not depend on what happened to be highlighted (model.md R43).
        for (core::EntityKey k : bus.selection().keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

        if (requested.empty()) {
            ctx.echo("Çevrilecek çizgi belirtilmedi ve seçim boş. "
                     "Örnek: ALANAÇEVİR nesneler=1 nesneler=2");
            co_return;
        }
    }

    std::vector<Strand> strands;
    if (!collect(ctx, requested, strands)) co_return;

    if (strands.size() < 2 && (strands.empty() || strands.front().points.size() < 3)) {
        ctx.echo("Bir alan kapatmak için en az üç köşe gerekir; verilen çizgiler yetmiyor.");
        co_return;
    }

    const core::Mm tol =
        bus.project_settings().get("core.topoloji.dugum_toleransi").as_length();

    // Walk the strands end to end, starting from the first. Each step looks for a
    // strand that begins — or, reversed, ends — where the chain currently stops.
    std::vector<core::Point2> ring = strands.front().points;
    strands.front().used           = true;
    std::size_t joined             = 1;

    while (joined < strands.size()) {
        const core::Point2 tail = ring.back();

        bool advanced = false;
        for (Strand& s : strands) {
            if (s.used) continue;

            const bool head_matches = within(s.points.front(), tail, tol);
            const bool tail_matches = within(s.points.back(), tail, tol);
            if (!head_matches && !tail_matches) continue;

            // Appended WITHOUT the shared corner: it is already the last vertex of
            // the chain, and storing it twice would count it twice in the perimeter
            // and write it twice into every exported file (R10's reason, applied to
            // the join rather than to the closure).
            if (head_matches)
                ring.insert(ring.end(), s.points.begin() + 1, s.points.end());
            else
                ring.insert(ring.end(), s.points.rbegin() + 1, s.points.rend());

            s.used   = true;
            advanced = true;
            ++joined;
            break;
        }

        if (!advanced) {
            ctx.echo("Çizgiler tek bir zincir oluşturmuyor: " + std::to_string(joined) + " / " +
                     std::to_string(strands.size()) +
                     " çizgi birleşti, kalanların ucu zincire değmiyor. Uçları "
                     "yakalama açıkken yeniden çizin ya da düğüm toleransını büyütün.");
            co_return;
        }
    }

    // The chain has to come back to where it started, or it is not a boundary.
    if (!within(ring.front(), ring.back(), tol)) {
        ctx.echo("Zincir kapanmıyor: ilk köşe ile son köşe birbirine değmiyor. "
                 "Kapalı bir alan için uçların buluşması gerekir.");
        co_return;
    }

    // The closing vertex is IMPLIED, never stored (R10): drop the repeat the walk
    // arrived back at.
    if (ring.size() > 1 && within(ring.front(), ring.back(), tol)) ring.pop_back();

    if (ring.size() < 3) {
        ctx.echo("Bir alan en az üç köşe ister; zincir " + std::to_string(ring.size()) +
                 " köşe bıraktı.");
        co_return;
    }

    const core::RingGeometry::RingInput face{ring, core::RingRole::Exterior, 0};
    auto created = ctx.transaction().add_area(ctx.active_layer(), {&face, 1});
    if (!created) {
        // A ring that crosses itself is refused here, and the message names it.
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    // The lines are GONE, in the same transaction that made the face. A boundary
    // that exists both as a face and as the lines beneath it is a duplicate the
    // next topology check — or the next export — has to answer for.
    for (const Strand& s : strands) {
        auto st = ctx.transaction().erase_entity(s.entity);
        if (!st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    // A retired key never comes back (R4), so a selection still holding one would
    // point at nothing for the rest of the session.
    bool touched = false;
    for (std::int64_t raw : requested)
        touched |=
            bus.selection().remove(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
    if (touched && bus.on_selection_changed) bus.on_selection_changed();

    ctx.record("nesneler", Value::ids(requested));
    ctx.echo(std::to_string(strands.size()) + " çizgi tek bir alana çevrildi; " +
             std::to_string(ring.size()) + " köşe.");
}

} // namespace

KENTOS_COMMAND(to_area)
{
    return CommandSpec{
        .id       = "core.to_area",
        .names    = {"ALANAÇEVİR", "ALANACEVIR", "TOAREA", "ALÇ"},
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim"}},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Uç uca değen çizgileri tek bir kapalı alana çevirir.",
        .run      = &run,
    };
}

} // namespace kentos::command
