// SPDX-License-Identifier: GPL-3.0-or-later
// core.combine — BİRLEŞTİR. The GENERIC merge, and deliberately not a tevhit.
//
// WHY THIS EXISTS SEPARATELY FROM TEVHİT. A tevhit is a cadastral act: it is only
// valid between ADJOINING parcels, the result is one parcel and never two, and the
// ada/pafta/malik columns of that new parcel follow from what TKGM will accept
// rather than from arithmetic. `src/domain/cadastre/src/merge_command.cpp` encodes
// all of that, correctly, and refuses the cases the regulation refuses.
//
// None of it is true of merging geometry on a map. Two woodland patches on
// opposite sides of a valley merge into one layer feature that happens to have two
// parts, and refusing that because "they do not adjoin" would be importing a
// cadastral rule into a GIS operation that has no such rule. So this command does
// the objective half — the union, or the end-to-end chain — and says plainly what
// it produced; the regulated half stays in TEVHİT where a harita mühendisi signs
// for it.
//
// IT LIVES IN `/src/command`, NOT `/src/domain`, and that is the dependency
// direction of Article 3.2 doing its job: the union is `core::polygon_boolean`,
// which is Clipper2 under `kentos_core`, so nothing here needs the cadastre
// target. A generic tool that had to reach into a domain library to work would be
// a sign the split above was drawn in the wrong place.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One input, read once: either a face (closed rings) or a run (one open ring).
struct Piece
{
    core::EntityId entity{core::kNoEntity};
    std::int64_t id{0};
    core::Polygon face;            ///< filled when `is_face`
    std::vector<core::Point2> run; ///< filled when not
    bool is_face{false};
    bool used{false}; ///< the chain walk's mark
};

bool within(core::Point2 a, core::Point2 b, core::Mm tol)
{
    // Chebyshev in integers, the same test `to_area.cpp` makes, and for the same
    // reason: whether two corners are one corner must not depend on the machine.
    const auto dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const auto dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx <= tol && dy <= tol;
}

/// Reads one entity into a `Piece`, or says why it cannot be merged.
bool read_piece(Context& ctx, std::int64_t raw, Piece& out)
{
    const core::Document& doc = ctx.document();

    if (raw <= 0) {
        ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) + ". Kimlikler 1'den başlar.");
        return false;
    }
    const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
    const core::EntityId slot = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
        return false;
    }

    // A curve's vertices are its DEFINITION — a centre and a radius handle — not a
    // boundary. Unioning them would union numbers that never described an outline.
    // Said rather than skipped, because silently dropping one of the objects the
    // user selected is how a merge loses a parcel.
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.echo("Nesne " + std::to_string(raw) +
                 " bir eğri ya da nokta; BİRLEŞTİR çizgiler ve alanlar üzerinde çalışır. "
                 "Önce DÖNÜŞTÜR ile çizgiye çevirin.");
        return false;
    }

    out.entity = slot;
    out.id     = raw;

    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);
    if (span.count == 0) {
        ctx.echo("Nesne " + std::to_string(raw) + " boş; birleştirilecek geometrisi yok.");
        return false;
    }

    const bool open = span.count == 1 && geom.ring_role[span.first] == core::RingRole::Open;
    out.is_face     = !open;

    const auto vertices = [&](std::uint32_t r) {
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        std::vector<core::Point2> ring;
        ring.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.push_back(core::Point2{xs[v], ys[v]});
        return ring;
    };

    if (open) {
        out.run = vertices(span.first);
        if (out.run.size() < 2) {
            ctx.echo("Nesne " + std::to_string(raw) + " tek noktadan ibaret; çizgi değil.");
            return false;
        }
        return true;
    }

    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geom.ring_role[r] == core::RingRole::Open) continue;
        std::vector<core::Point2> ring = vertices(r);
        if (geom.ring_role[r] == core::RingRole::Exterior && out.face.exterior.empty())
            out.face.exterior = std::move(ring);
        else if (ring.size() >= 3)
            out.face.holes.push_back(std::move(ring));
    }
    if (out.face.exterior.size() < 3) {
        ctx.echo("Nesne " + std::to_string(raw) + " kapalı bir alan kapatmıyor.");
        return false;
    }
    return true;
}

/// The layer the result belongs on: the inputs' own when they agree, the active
/// layer when they do not. A merge that silently moved every input onto whatever
/// layer happened to be active is how a boundary ends up on the label layer.
core::LayerId result_layer(const Context& ctx, const std::vector<Piece>& pieces)
{
    const core::LayerId first = ctx.document().entities().layer[pieces.front().entity];
    for (const Piece& p : pieces)
        if (ctx.document().entities().layer[p.entity] != first) return ctx.active_layer();
    return first;
}

/// The style the result carries: the inputs' own when they agree, by-layer when
/// they do not. Same principle as `result_layer` — never invent, never guess.
core::StyleId result_style(const Context& ctx, const std::vector<Piece>& pieces)
{
    const core::StyleId first = ctx.document().entities().style[pieces.front().entity];
    for (const Piece& p : pieces)
        if (ctx.document().entities().style[p.entity] != first) return core::kByLayerStyle;
    return first;
}

/// Copies every attribute column the inputs AGREE on onto `made`, and names the
/// ones it left empty.
///
/// The rule is `merge_command.cpp`'s and it is the only one that cannot be wrong:
/// a column whose value is the same on every input keeps it; a column where they
/// disagree comes back EMPTY and is reported. Picking the first object's value
/// would be inventing a record — which matters as much for a GIS feature as for a
/// parcel, because the attribute table is what the layer is queried and styled by.
bool carry_attributes(Context& ctx, const std::vector<Piece>& pieces, core::EntityId made,
                      std::vector<std::string>& dropped)
{
    const core::Document& doc    = ctx.document();
    const core::AttrTable& table = doc.attributes();

    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto col                 = static_cast<core::AttrId>(c);
        const core::AttrColumn* column = table.column(col);
        if (column == nullptr) continue;

        bool agreed = true;
        core::AttrValue shared{};
        for (std::size_t i = 0; i < pieces.size(); ++i) {
            auto had = doc.attribute(col, pieces[i].entity);
            if (!had) {
                agreed = false;
                break;
            }
            if (i == 0)
                shared = had.value();
            else if (!(had.value() == shared)) {
                agreed = false;
                break;
            }
        }

        if (!agreed) {
            if (!column->spec().id.empty()) dropped.push_back(column->spec().id);
            continue;
        }
        if (!shared.present) continue;

        if (auto st = ctx.transaction().set_attribute(col, made, shared); !st) {
            ctx.echo(st.error().message);
            return false;
        }
    }
    return true;
}

/// The union half: every input is a face.
Task<bool> combine_faces(Context& ctx, std::vector<Piece>& pieces, std::string& said)
{
    std::vector<core::Polygon> subject{pieces.front().face};
    std::vector<core::Polygon> clip;
    clip.reserve(pieces.size() - 1);
    for (std::size_t i = 1; i < pieces.size(); ++i)
        clip.push_back(pieces[i].face);

    auto merged = core::polygon_boolean(subject, clip, core::BooleanOp::Union);
    if (!merged) {
        ctx.echo(merged.error().message);
        co_return false;
    }
    if (merged.value().empty()) {
        ctx.echo("Birleşme sonucu boş çıktı; seçilen nesneler bir alan kapatmıyor.");
        co_return false;
    }

    const core::LayerId layer = result_layer(ctx, pieces);
    const core::StyleId style = result_style(ctx, pieces);

    // SEVERAL PIECES IS A RESULT, NOT AN ERROR — this is exactly where a generic
    // merge parts company with a tevhit. Disjoint inputs give one feature per
    // piece and the count is reported, because the user asked to merge two things
    // and is owed the news that they did not touch.
    std::vector<core::EntityId> made;
    made.reserve(merged.value().size());
    for (const core::Polygon& poly : merged.value()) {
        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{poly.exterior, core::RingRole::Exterior, 0});
        for (const std::vector<core::Point2>& hole : poly.holes)
            rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});

        auto created = ctx.transaction().add_area(layer, rings);
        if (!created) {
            ctx.echo(created.error().message);
            co_return false;
        }
        if (style != core::kByLayerStyle) {
            auto st = ctx.transaction().set_entity_style(created.value(), style);
            if (!st) {
                ctx.echo(st.error().message);
                co_return false;
            }
        }
        made.push_back(created.value());
    }

    std::vector<std::string> dropped;
    for (core::EntityId id : made)
        if (!carry_attributes(ctx, pieces, id, dropped)) co_return false;

    for (Piece& p : pieces) {
        auto st = ctx.transaction().erase_entity(p.entity);
        if (!st) {
            ctx.echo(st.error().message);
            co_return false;
        }
    }

    said = std::to_string(pieces.size()) + " alan birleştirildi";
    if (made.size() == 1) {
        said += ".";
    } else {
        said += ", sonuç " + std::to_string(made.size()) +
                " ayrı parça: seçilen alanlar birbirine değmiyor.";
    }
    if (!dropped.empty()) {
        // The columns are listed once even when several pieces came out: they were
        // dropped for the same reason on every one of them.
        std::vector<std::string> unique;
        for (const std::string& id : dropped)
            if (std::find(unique.begin(), unique.end(), id) == unique.end()) unique.push_back(id);
        said += "\n  Girdiler şu sütunlarda ayrıştığı için boş bırakıldı:";
        for (const std::string& id : unique)
            said += "\n    " + id;
        said += "\n  ÖZNİTELİK ile doldurabilirsiniz.";
    }
    co_return true;
}

/// The chain half: every input is an open run.
Task<bool> combine_runs(Context& ctx, std::vector<Piece>& pieces, std::string& said)
{
    const core::Mm tol =
        ctx.session().bus().project_settings().get("core.topoloji.dugum_toleransi").as_length();

    // Walk from the first run's tail, taking whichever unused run starts — or,
    // reversed, ends — where the chain currently stops. The same walk `ALANAÇEVİR`
    // makes; the difference is that this one does not demand the chain close.
    std::vector<core::Point2> chain = pieces.front().run;
    pieces.front().used             = true;
    std::size_t joined              = 1;

    while (joined < pieces.size()) {
        bool advanced = false;

        // BOTH ENDS ARE TRIED, unlike the to-area walk, which only ever grows the
        // tail. A ring closes whichever way it is walked, so growing one end is
        // enough there; an open chain does not, and a selection given in the order
        // 2,1,3 would otherwise be refused for an ordering that is not the user's
        // fault.
        for (int end = 0; end < 2 && !advanced; ++end) {
            const core::Point2 stop = end == 0 ? chain.back() : chain.front();

            for (Piece& p : pieces) {
                if (p.used) continue;

                const bool head = within(p.run.front(), stop, tol);
                const bool tail = within(p.run.back(), stop, tol);
                if (!head && !tail) continue;

                // The shared corner is already in the chain; appending it twice
                // would count it twice in every length this drawing reports and
                // write it twice into every file it is exported to.
                if (end == 0) {
                    if (head)
                        chain.insert(chain.end(), p.run.begin() + 1, p.run.end());
                    else
                        chain.insert(chain.end(), p.run.rbegin() + 1, p.run.rend());
                } else {
                    if (tail)
                        chain.insert(chain.begin(), p.run.begin(), p.run.end() - 1);
                    else
                        chain.insert(chain.begin(), p.run.rbegin(), p.run.rend() - 1);
                }

                p.used   = true;
                advanced = true;
                ++joined;
                break;
            }
        }

        if (!advanced) {
            ctx.echo("Çizgiler tek bir zincir oluşturmuyor: " + std::to_string(joined) + " / " +
                     std::to_string(pieces.size()) +
                     " çizgi birleşti, kalanların ucu zincire değmiyor. Uçları yakalama "
                     "açıkken yeniden çizin ya da düğüm toleransını büyütün "
                     "(AYAR düğüm_toleransı).");
            co_return false;
        }
    }

    // THE FIRST OBJECT KEEPS ITS IDENTITY, so its key, layer, style and attributes
    // travel with the joined line (model.md R4, R28) and every reference to it in
    // the document still resolves. The others go.
    const core::RingGeometry::RingInput ring{chain, core::RingRole::Open, 0};
    if (auto st = ctx.transaction().set_geometry(pieces.front().entity, {&ring, 1}); !st) {
        ctx.echo(st.error().message);
        co_return false;
    }

    for (std::size_t i = 1; i < pieces.size(); ++i) {
        auto st = ctx.transaction().erase_entity(pieces[i].entity);
        if (!st) {
            ctx.echo(st.error().message);
            co_return false;
        }
    }

    said = std::to_string(pieces.size()) +
           " çizgi tek çizgide birleştirildi: " + std::to_string(chain.size()) + " köşe.";

    // A chain that came back to its start is a boundary the user probably wants as
    // a face — said, not done, because turning a line into an area changes what the
    // object IS, and ALANAÇEVİR is the command that does it on purpose.
    if (chain.size() >= 4 && within(chain.front(), chain.back(), tol))
        said += "\n  Zincir kapanıyor; alana çevirmek için ALANAÇEVİR kullanın.";

    co_return true;
}

Task<void> run(Context& ctx)
{
    std::vector<std::int64_t> requested;
    if (!co_await want_objects(ctx, "nesneler",
                               "Birleştirilecek alanları ya da çizgileri seçin, Enter'a basın",
                               requested))
        co_return;

    if (requested.size() < 2) {
        ctx.echo("BİRLEŞTİR en az iki nesne ister. Seçili: " + std::to_string(requested.size()) +
                 ". Birleştirilecek alanları ya da uç uca değen çizgileri seçin.");
        co_return;
    }

    std::vector<Piece> pieces;
    pieces.reserve(requested.size());
    for (std::int64_t raw : requested) {
        Piece p;
        if (!read_piece(ctx, raw, p)) co_return; // the bus rolls the transaction back
        pieces.push_back(std::move(p));
    }

    // MIXED INPUT IS REFUSED BY NAME. Unioning a line with a face has no answer
    // that is not a guess — is the line a new edge of the face, or a hole in it, or
    // does it just cross it? — and guessing on somebody's boundary is the failure
    // this whole program is arranged to avoid.
    std::size_t faces = 0;
    for (const Piece& p : pieces)
        if (p.is_face) ++faces;

    if (faces != 0 && faces != pieces.size()) {
        ctx.echo("Seçimde hem alan hem çizgi var (" + std::to_string(faces) + " alan, " +
                 std::to_string(pieces.size() - faces) +
                 " çizgi). BİRLEŞTİR ya yalnız alanları ya yalnız çizgileri birleştirir; "
                 "çizgileri önce ALANAÇEVİR ile alana çevirin.");
        co_return;
    }

    std::string said;
    const bool ok = faces == pieces.size() ? co_await combine_faces(ctx, pieces, said)
                                           : co_await combine_runs(ctx, pieces, said);
    if (!ok) co_return;

    // Recorded as the ID LIST the command resolved, so a journal replay merges the
    // same objects whatever happened to be selected at replay time (model.md R43).
    ctx.record("nesneler", Value::ids(requested));
    ctx.echo(said);
}

} // namespace

KENTOS_COMMAND(combine)
{
    return CommandSpec{
        .id       = "core.combine",
        .names    = {"BİRLEŞTİR", "BIRLESTIR", "COMBINE", "BRL"},
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim"}},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek "
                    "çizgi yapar.",
        .run      = &run,
    };
}

} // namespace kentos::command
