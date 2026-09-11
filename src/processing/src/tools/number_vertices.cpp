// SPDX-License-Identifier: GPL-3.0-or-later
// islem.kose_numarala — KÖŞENUMARALA: the corners of a parcel, numbered.
//
// A parcel's corners are numbered on the sheet and in the coordinate table, and
// the two have to agree: the numbering starts at a chosen corner (the röper the
// surveyor names), runs one way round, and is written in the office's own form
// — `A00001`, `K-7`, `1`. The tool writes each number outside the corner, along
// the corner's outward bisector, so it never sits on the parcel's own line.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::processing {
namespace {

struct Dir
{
    double x{0.0};
    double y{0.0};
};

Dir unit_between(core::Point2 from, core::Point2 to)
{
    const auto dx  = static_cast<double>(to.x - from.x);
    const auto dy  = static_cast<double>(to.y - from.y);
    const double l = std::sqrt(dx * dx + dy * dy);
    if (l == 0.0) return Dir{};
    return Dir{dx / l, dy / l};
}

/// `number` as `prefix + padded digits + suffix`.
std::string label(std::int64_t number, const std::string& prefix, std::int64_t width,
                  const std::string& fill, const std::string& suffix)
{
    std::string digits = std::to_string(number);
    std::string pad;
    const std::string one = fill.empty() ? std::string("0") : fill;
    while (static_cast<std::int64_t>(digits.size()) +
               static_cast<std::int64_t>(pad.size() / one.size()) <
           width)
        pad += one;
    return prefix + pad + digits + suffix;
}

class NumberVertices final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        const std::string prefix = input.args.get("onek").as_text();
        const std::string suffix = input.args.get("sonek").as_text();
        const std::string fill   = input.args.get("dolgu").as_text();
        const std::int64_t width = input.args.get("basamak").as_int();
        const std::int64_t first = input.args.get("ilk").as_int();
        const bool clockwise     = input.args.get("yon").as_text() == "saat";
        const bool has_start     = input.args.has("baslangic");
        const core::Point2 start =
            has_start ? input.args.get("baslangic").as_points().front() : core::Point2{};

        core::Mm height = input.args.get("yukseklik").as_int();
        if (height <= 0)
            height = std::max<core::Mm>(1, core::mul_div_round(2500, input.plan_scale, 1000));
        core::Mm gap = input.args.get("bosluk").as_int();
        if (gap <= 0) gap = height / 2;
        const auto offset = static_cast<double>(gap) + static_cast<double>(height) / 2.0;

        std::size_t done = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            if (e.rings.empty() || e.rings.front().points.size() < 2) {
                progress.at(++done, input.entities.size());
                continue;
            }
            const InputEntity::Ring& ring      = e.rings.front();
            const std::vector<core::Point2>& v = ring.points;
            const std::size_t n                = v.size();
            const bool closed                  = ring.role != core::RingRole::Open;

            // The ring's turn, for "counter-clockwise" and for which side is out.
            double twice = 0.0;
            if (closed)
                for (std::size_t i = 0; i < n; ++i) {
                    const core::Point2 a = v[i];
                    const core::Point2 b = v[(i + 1) % n];
                    twice += static_cast<double>(a.x) * static_cast<double>(b.y) -
                             static_cast<double>(b.x) * static_cast<double>(a.y);
                }
            const bool ccw = twice > 0.0;

            // Where the count starts: the corner nearest the given point, else
            // the first; on an open line only an END can start it.
            std::size_t begin = 0;
            if (has_start) {
                double best = -1.0;
                for (std::size_t i = 0; i < n; ++i) {
                    if (!closed && i != 0 && i != n - 1) continue;
                    const auto dx  = static_cast<double>(v[i].x - start.x);
                    const auto dy  = static_cast<double>(v[i].y - start.y);
                    const double d = dx * dx + dy * dy;
                    if (best < 0.0 || d < best) {
                        best  = d;
                        begin = i;
                    }
                }
            }
            // Walking direction in index space: on a closed ring, the stored
            // order is one turn and its reverse the other; on a line, away
            // from the starting end.
            int step = 1;
            if (closed)
                step = (ccw != clockwise) ? 1 : -1;
            else
                step = begin == 0 ? 1 : -1;

            std::vector<core::Mm> xs(n);
            std::vector<core::Mm> ys(n);
            for (std::size_t i = 0; i < n; ++i) {
                xs[i] = v[i].x;
                ys[i] = v[i].y;
            }

            for (std::size_t k = 0; k < n; ++k) {
                const std::size_t i = static_cast<std::size_t>(
                    (static_cast<long long>(begin) +
                     static_cast<long long>(step) * static_cast<long long>(k) +
                     static_cast<long long>(n) * static_cast<long long>(k + 1)) %
                    static_cast<long long>(n));
                const core::Point2 corner = v[i];

                // The outward bisector: away from both neighbours; for a reflex
                // corner that points inside, so the ring is asked.
                Dir out{};
                const bool has_prev = closed || i > 0;
                const bool has_next = closed || i + 1 < n;
                const Dir to_prev   = has_prev ? unit_between(corner, v[(i + n - 1) % n]) : Dir{};
                const Dir to_next   = has_next ? unit_between(corner, v[(i + 1) % n]) : Dir{};
                Dir d{to_prev.x + to_next.x, to_prev.y + to_next.y};
                const double dl = std::sqrt(d.x * d.x + d.y * d.y);
                if (dl > 1e-9) {
                    out = Dir{-d.x / dl, -d.y / dl};
                    if (closed) {
                        const core::Point2 probe{corner.x + core::mm_round(out.x * offset * 2.0),
                                                 corner.y + core::mm_round(out.y * offset * 2.0)};
                        if (core::ring_contains(xs, ys, probe)) out = Dir{-out.x, -out.y};
                    }
                } else {
                    // Straight through (or an end of a line): the right of the
                    // walk is outside a counter-clockwise ring.
                    const Dir u = has_next ? to_next : Dir{-to_prev.x, -to_prev.y};
                    out         = Dir{u.y, -u.x};
                    if (closed && !ccw) out = Dir{-out.x, -out.y};
                    if (!closed && !has_next) out = Dir{-to_prev.x, -to_prev.y};
                    if (!closed && !has_prev) out = Dir{-to_next.x, -to_next.y};
                }

                ToolOutput::Caption cap;
                cap.centre = core::Point2{corner.x + core::mm_round(out.x * offset),
                                          corner.y + core::mm_round(out.y * offset)};
                cap.height = height;
                cap.text = label(first + static_cast<std::int64_t>(k), prefix, width, fill, suffix);
                output.captions.push_back(std::move(cap));
            }
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id      = "islem.kose_numarala",
        .names   = {"KÖŞENUMARALA", "KOSENUMARALA", "NUMBERVERTICES", "KNM"},
        .title   = "Köşeleri numarala",
        .summary = "Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak "
                   "sırayla numaralar ve numarayı köşenin dışına yazar.",
        .group   = "Etiketleme",
        .icon    = "koordinat",
        .applies = Applies::Faces | Applies::Lines,
        .params =
            {
                ToolParam::point("baslangic",
                                 "Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe"),
                ToolParam::choice("yon", "Sayım yönü", {"ters", "saat"}, "ters"),
                ToolParam::text("onek", "Numaranın önüne gelen yazı (örnek: A, K-)"),
                ToolParam::integer("basamak",
                                   "Numaranın en az basamak sayısı; eksikler dolgu ile tamamlanır",
                                   0, 0, 12),
                ToolParam::text("dolgu", "Basamak dolgusu", "0"),
                ToolParam::integer("ilk", "İlk köşenin numarası", 1, 0, 1000000000),
                ToolParam::text("sonek", "Numaranın arkasına gelen yazı"),
                ToolParam::integer("yukseklik",
                                   "Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm",
                                   0, 0, 100000000),
                ToolParam::integer("bosluk",
                                   "Köşe ile yazı arası, milimetre; 0 = yüksekliğin yarısı", 0, 0,
                                   100000000),
            },
        .output        = OutputShape::NewEntities,
        .output_suffix = "kose",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(number_vertices)
{
    static const NumberVertices tool;
    return tool;
}

} // namespace kentos::processing
