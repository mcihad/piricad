// SPDX-License-Identifier: GPL-3.0-or-later
// islem.alan_duzenle — ALANDÜZENLE: a face brought to the area the document says.
//
// The tapu says 1 250,00 m²; the drawn parcel says 1 248,71. The correction is
// made three ways, and the surveyor chooses: shrink or grow the whole parcel a
// hair on every side (the command line's way, and the batch way for many), slide
// ONE edge along its normal with the neighbours following, or pull ONE corner.
// The last two are interactive: the hand takes hold of the edge or the corner,
// a ghost follows it, snaps onto the exact figure when it comes near, and Enter
// commits the figure. The original stays on screen until then; nothing is
// written that is not the figure asked for. The arithmetic is core's
// (core/area_edit.hpp), deterministic, in millimetres.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/area_edit.hpp"
#include "kentos_cad/core/offset.hpp"

#include <cmath>
#include <string>

namespace kentos::processing {
namespace {

core::Mm2 target_of(const command::Args& args)
{
    // Square metres on the command line; square millimetres in the record.
    const double m2 = args.get("alan").as_number();
    return static_cast<core::Mm2>(std::llround(m2 * 1'000'000.0));
}

core::AreaEditMode mode_of(const std::string& word)
{
    if (word == "kenar") return core::AreaEditMode::Edge;
    if (word == "kose") return core::AreaEditMode::Vertex;
    return core::AreaEditMode::Uniform;
}

/// The exterior ring of a face, as points.
const std::vector<core::Point2>* exterior(const InputEntity& e)
{
    for (const InputEntity::Ring& r : e.rings)
        if (r.role == core::RingRole::Exterior) return &r.points;
    return nullptr;
}

/// Distance from `p` to the segment a→b.
double segment_distance(core::Point2 p, core::Point2 a, core::Point2 b)
{
    const auto ax   = static_cast<double>(b.x - a.x);
    const auto ay   = static_cast<double>(b.y - a.y);
    const auto px   = static_cast<double>(p.x - a.x);
    const auto py   = static_cast<double>(p.y - a.y);
    const double l2 = ax * ax + ay * ay;
    double t        = l2 > 0.0 ? (px * ax + py * ay) / l2 : 0.0;
    t               = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    const double dx = px - ax * t;
    const double dy = py - ay * t;
    return std::sqrt(dx * dx + dy * dy);
}

class AreaEdit final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    command::Task<core::Status> interact(command::Context& ctx, ToolInput& input) const override
    {
        const std::string mode = input.args.get("mod").as_text();
        if (mode == "hepsi") co_return core::ok();
        if (input.entities.size() != 1)
            co_return core::err(core::ErrorCode::InvalidArgument,
                                "Kenardan ya da köşeden çekmek tek bir alan ister; kapsamda " +
                                    std::to_string(input.entities.size()) + " nesne var.");
        const InputEntity& face               = input.entities.front();
        const std::vector<core::Point2>* ring = exterior(face);
        if (ring == nullptr || ring->size() < 3)
            co_return core::err(core::ErrorCode::InvalidArgument, "Nesne kapalı bir alan değil.");
        const core::Mm2 target = target_of(input.args);
        if (target <= 0)
            co_return core::err(core::ErrorCode::InvalidArgument,
                                "Hedef alan sıfırdan büyük olmalı: alan=<m²>.");

        // WHICH edge or corner. Given on the line, nothing is asked: `run`
        // lands the figure exactly along the normal. Not given, the hand says —
        // the click nearest an edge or a corner — and then where it goes, with
        // the ghost following and Enter sending the figure's own point
        // (map_canvas.cpp, RubberShape::AreaEdit).
        const char* which = mode == "kenar" ? "kenar" : "kose";
        if (input.args.has(which)) co_return core::ok();

        core::AreaEditRequest request;
        request.key    = face.key;
        request.mode   = mode_of(mode);
        request.target = target;
        auto picked    = co_await ctx.point("nokta", mode == "kenar" ? "Çekilecek kenarı tıklayın"
                                                                     : "Çekilecek köşeyi tıklayın");
        if (!picked)
            co_return core::err(core::ErrorCode::InvalidArgument,
                                std::string(mode == "kenar" ? "Kenar" : "Köşe") +
                                    " seçilmedi. Arayüzde tıklayın; komut satırında " + which +
                                    "=<sıra> verin.");
        const std::size_t n = ring->size();
        double best         = -1.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = request.mode == core::AreaEditMode::Vertex
                                 ? segment_distance(*picked, (*ring)[i], (*ring)[i])
                                 : segment_distance(*picked, (*ring)[i], (*ring)[(i + 1) % n]);
            if (best < 0.0 || d < best) {
                best          = d;
                request.index = static_cast<std::uint32_t>(i);
            }
        }
        request.grab = *picked;
        input.args.set(which,
                       command::Value::integer(static_cast<std::int64_t>(request.index) + 1));

        auto to = co_await ctx.point(
            "nokta",
            std::string(mode == "kenar" ? "Kenarı" : "Köşeyi") +
                " yeni yerine sürükleyin; hedef alana oturunca Enter kabul eder",
            command::PointOptions{.rubber_band    = true,
                                  .rubber_origin  = request.grab,
                                  .rubber_shape   = command::RubberShape::AreaEdit,
                                  .rubber_payload = core::encode_area_edit(request)});
        if (!to) co_return core::err(core::ErrorCode::Cancelled, "");
        input.args.set("nokta", command::Value::point(*to));
        co_return core::ok();
    }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        const core::Mm2 target = target_of(input.args);
        if (target <= 0)
            return core::err(core::ErrorCode::InvalidArgument,
                             "Hedef alan sıfırdan büyük olmalı: alan=<m²>.");
        const std::string mode = input.args.get("mod").as_text();

        std::size_t done = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            const std::vector<core::Point2>* ring = exterior(e);
            if (ring == nullptr || ring->size() < 3) {
                progress.at(++done, input.entities.size());
                continue;
            }

            core::AreaEditRequest request;
            request.key    = e.key;
            request.mode   = mode_of(mode);
            request.target = target;
            core::Point2 at{};
            if (request.mode != core::AreaEditMode::Uniform) {
                if (input.entities.size() != 1)
                    return core::err(core::ErrorCode::InvalidArgument,
                                     "Kenardan ya da köşeden çekmek tek bir alan ister.");
                const std::int64_t one_based =
                    input.args.get(request.mode == core::AreaEditMode::Edge ? "kenar" : "kose")
                        .as_int();
                if (one_based < 1 || static_cast<std::size_t>(one_based) > ring->size())
                    return core::err(core::ErrorCode::InvalidArgument,
                                     "kenar= ya da kose= 1 ile " + std::to_string(ring->size()) +
                                         " arasında olmalı.");
                request.index = static_cast<std::uint32_t>(one_based - 1);
                if (input.args.has("nokta")) {
                    at = input.args.get("nokta").as_point();
                } else {
                    // No hand: the figure's own point, straight along the normal.
                    const core::Point2 v    = (*ring)[request.index];
                    const core::AreaGhost g = core::area_edit_ghost(*ring, request, v, 0);
                    if (g.points.empty())
                        return core::err(core::ErrorCode::ValidationFailed,
                                         "Alan bu kenar ya da köşeyle hedefe getirilemiyor.");
                    at = g.commit;
                }
            }

            auto shaped = core::area_edit_apply(*ring, request, at);
            if (!shaped) return shaped.error();
            const core::Mm2 got = core::ring_area(shaped.value()) < 0
                                      ? -core::ring_area(shaped.value())
                                      : core::ring_area(shaped.value());

            ToolOutput::Replacement r;
            r.key = e.key;
            for (const InputEntity::Ring& old : e.rings) {
                InputEntity::Ring ring_out = old;
                if (old.role == core::RingRole::Exterior) ring_out.points = shaped.value();
                r.rings.push_back(std::move(ring_out));
            }
            output.replacements.push_back(std::move(r));
            const core::Mm2 before =
                core::ring_area(*ring) < 0 ? -core::ring_area(*ring) : core::ring_area(*ring);
            output.notes.push_back(
                "nesne " + std::to_string(e.key) + ": " + core::format_square_metres(before) +
                " → " + core::format_square_metres(got) +
                (got == target ? "" : " (hedef " + core::format_square_metres(target) + ")"));
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id      = "islem.alan_duzenle",
        .names   = {"ALANDÜZENLE", "ALANDUZENLE", "ADJUSTAREA", "ADZ"},
        .title   = "Alanı düzenle",
        .summary = "Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp "
                   "genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz.",
        .group   = "Düzenleme",
        .icon    = "alan",
        .applies = Applies::Faces,
        .params =
            {
                ToolParam::number("alan", "Hedef alan, metrekare"),
                ToolParam::choice("mod", "Nasıl getirileceği", {"hepsi", "kenar", "kose"}, "hepsi"),
                ToolParam::integer_optional(
                    "kenar", "Kaydırılacak kenar (ilk köşeden çıkan kenar 1); mod=kenar", 1,
                    1000000),
                ToolParam::integer_optional("kose", "Çekilecek köşe; mod=kose", 1, 1000000),
                ToolParam::point(
                    "nokta", "Kenarın ya da köşenin gideceği yer; verilmezse arayüz sürükletir, "
                             "komut satırı hedefe tam oturtur"),
            },
        .output        = OutputShape::InPlace,
        .output_suffix = "",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(area_edit)
{
    static const AreaEdit tool;
    return tool;
}

} // namespace kentos::processing
