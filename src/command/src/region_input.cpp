// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/region_input.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/measure_mark.hpp"
#include "piricad/command/session.hpp"

#include <algorithm>
#include <set>

namespace piricad::command {

namespace {

/// The open ends, as the canvas marks them: the ones a gap away from other
/// linework first — nearest the click first, each end in one gap only, so two
/// ends that see each other are one line and a corner gets no second, wider
/// one — and bare ends, a line stopping in the open, only when no end has a gap
/// to show.
void mark_open_ends(const Context& ctx, const std::vector<core::OpenEnd>& open)
{
    // A dozen is what a person reads; the rest are counted in the sentence.
    constexpr std::size_t kShown = 12;
    std::set<core::Point2> used;
    std::size_t shown = 0;
    for (const core::OpenEnd& end : open) {
        if (shown == kShown) break;
        if (!end.has_nearest || used.contains(end.at) || used.contains(end.nearest)) continue;
        used.insert(end.at);
        used.insert(end.nearest);
        MeasureMark m;
        m.shape  = MeasureMark::Shape::Gap;
        m.points = {end.at, end.nearest};
        m.labels = {"boşluk " + gap_words(end.distance)};
        ctx.mark(m);
        ++shown;
    }
    if (shown > 0) return;
    for (std::size_t i = 0; i < open.size() && i < 3; ++i) {
        MeasureMark m;
        m.shape  = MeasureMark::Shape::Gap;
        m.points = {open[i].at};
        m.labels = {"açık uç"};
        ctx.mark(m);
    }
}

} // namespace

std::string gap_words(core::Mm v)
{
    const core::Mm a = v < 0 ? -v : v;
    if (a < 1000) {
        if (a % 10 == 0) return std::to_string(a / 10) + " cm";
        return std::to_string(a) + " mm";
    }
    std::string frac = std::to_string((a % 1000 + 5) / 10);
    if (frac.size() < 2) frac = "0" + frac;
    if (frac == "100") return std::to_string(a / 1000 + 1) + ",00 m";
    return std::to_string(a / 1000) + "," + frac + " m";
}

Task<std::optional<FoundRegion>> ask_region(Context& ctx, std::string prompt, bool boundary_set)
{
    Bus& bus                  = ctx.session().bus();
    const core::Document& doc = ctx.document();

    FoundRegion out;
    core::RegionQuery& query = out.query;

    query.islands        = ctx.has_argument("ada") ? ctx.argument("ada").as_bool(true) : true;
    query.node_tolerance = bus.project_settings().get("core.topoloji.dugum_toleransi").as_length();
    query.bridge         = ctx.has_argument("bosluk") ? ctx.argument("bosluk").as_int() : 0;
    if (query.bridge < 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Köprülenecek boşluk eksi olamaz; milimetre olarak 0 ya da daha büyük verin.");
        co_return std::nullopt;
    }
    if (boundary_set && ctx.has_argument("nesneler")) {
        out.keys = ctx.argument("nesneler").as_ids();
        for (const std::int64_t raw : out.keys) {
            const core::EntityId slot =
                raw > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)))
                        : core::kNoEntity;
            if (slot == core::kNoEntity || !doc.alive(slot)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
                co_return std::nullopt;
            }
            query.only.push_back(slot);
        }
    }

    // THE POINT: given, or shown — and while it is shown the region under the
    // cursor is drawn, found by the same call the click makes. No aid touches
    // the click (`PointOptions::aids`).
    if (const Value given = ctx.argument("nokta"); !given.empty() && !given.as_points().empty()) {
        query.at = given.as_points().front();
    } else {
        const core::RegionPreview preview{query.islands, query.node_tolerance, query.bridge,
                                          out.keys};
        auto pointed =
            co_await ctx.point("nokta", std::move(prompt),
                               PointOptions{.rubber_band    = true,
                                            .rubber_base    = false,
                                            .rubber_shape   = RubberShape::Region,
                                            .rubber_payload = core::encode_region_preview(preview),
                                            .aids           = false});
        if (!pointed) co_return std::nullopt;
        query.at = *pointed;
    }

    auto found = core::region_at(doc, query);
    if (!found) {
        ctx.refuse(found.error());
        co_return std::nullopt;
    }
    out.region = std::move(found.value());

    const core::Region& region = out.region;
    if (region.on_linework) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Nokta bir çizginin üstünde; bölgenin İÇİNE, çizgiden uzağa tıklayın.");
        co_return std::nullopt;
    }
    if (!region.face) {
        if (region.open.empty()) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Bu noktayı çevreleyen kapalı bir çizgi yok. Bölgenin içine tıklayın; "
                       "gizli katmanlardaki çizgiler sınır sayılmaz.");
            co_return std::nullopt;
        }
        mark_open_ends(ctx, region.open);
        // The nearest end to the click that is a GAP away from other linework;
        // an end in the open, with nothing near it, is counted but not measured.
        const auto gap =
            std::ranges::find_if(region.open, [](const core::OpenEnd& e) { return e.has_nearest; });
        std::string why =
            "Bu bölge kapanmıyor: " + std::to_string(region.open.size()) + " açık uç var";
        if (gap != region.open.end())
            why +=
                "; tıkladığınız yere en yakını bir çizgiye " + gap_words(gap->distance) + " uzakta";
        why += ". Uçlar tuvalde işaretlendi. Boşluğu yakalamayla kapatın ya da köprülemek için "
               "bosluk=<mm> verin.";
        ctx.refuse(core::ErrorCode::InvalidArgument, why);
        co_return std::nullopt;
    }
    co_return out;
}

void record_region(Context& ctx, const FoundRegion& found)
{
    ctx.record("nokta", Value::point(found.query.at));
    if (ctx.has_argument("ada")) ctx.record("ada", Value::boolean(found.query.islands));
    if (found.query.bridge > 0) ctx.record("bosluk", Value::integer(found.query.bridge));
    if (!found.keys.empty()) ctx.record("nesneler", Value::ids(found.keys));
}

} // namespace piricad::command
