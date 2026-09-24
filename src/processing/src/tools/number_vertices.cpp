// SPDX-License-Identifier: GPL-3.0-or-later
// islem.kose_numarala — KÖŞENUMARALA: the corners of a parcel, numbered.
//
// A parcel's corners are numbered on the sheet and in the coordinate table, and
// the two have to agree: the numbering starts at a chosen corner (the röper the
// surveyor names), runs one way round, and is written in the office's own form
// — `A00001`, `K-7`, `1`. The tool writes each number outside the corner, along
// the corner's outward bisector, so it never sits on the parcel's own line.
//
// AND THE NUMBER FOLLOWS ITS CORNER. Unless told not to, every number written is
// ATTACHED to the corner it names (core/attach.hpp): the command that later moves
// the corner — KÖŞETAŞI, TAŞI, DÖNDÜR — re-places the number beside it. The text
// itself stays what it was; a corner keeps its number when it moves.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>

namespace kentos::processing {
namespace {

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

/// Twice the signed area: positive for a counter-clockwise ring.
double twice_area(const std::vector<core::Point2>& v)
{
    double twice = 0.0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const core::Point2 a = v[i];
        const core::Point2 b = v[(i + 1) % v.size()];
        twice += static_cast<double>(a.x - v[0].x) * static_cast<double>(b.y - v[0].y) -
                 static_cast<double>(b.x - v[0].x) * static_cast<double>(a.y - v[0].y);
    }
    return twice;
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
        const bool attach        = input.args.get("bagla").as_bool(true);
        const core::Point2 start =
            has_start ? input.args.get("baslangic").as_points().front() : core::Point2{};

        core::Mm height = input.args.get("yukseklik").as_int();
        if (height <= 0)
            height = std::max<core::Mm>(1, core::mul_div_round(2500, input.plan_scale, 1000));
        core::Mm gap = input.args.get("bosluk").as_int();
        if (gap <= 0) gap = height / 2;

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
            const bool ccw                     = closed && twice_area(v) > 0.0;

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

            // THE RULE, once per corner: the outward bisector placement lives in
            // `core::attach_place`, so the number written here and the number
            // re-placed after the corner moved stand in the same spot.
            core::Attachment rule;
            rule.source = static_cast<core::EntityKey>(static_cast<std::uint64_t>(e.key));
            rule.anchor = core::AttachAnchor::Vertex;
            rule.derive = core::AttachDerive::Keep;
            rule.ring   = 0;
            rule.gap    = gap;

            for (std::size_t k = 0; k < n; ++k) {
                const std::size_t i = static_cast<std::size_t>(
                    (static_cast<long long>(begin) +
                     static_cast<long long>(step) * static_cast<long long>(k) +
                     static_cast<long long>(n) * static_cast<long long>(k + 1)) %
                    static_cast<long long>(n));
                rule.index       = static_cast<std::uint32_t>(i);
                const auto place = core::attach_place(v, closed, rule, height);
                if (!place) continue;

                ToolOutput::Caption cap;
                cap.centre = place->centre;
                cap.height = height;
                cap.text = label(first + static_cast<std::int64_t>(k), prefix, width, fill, suffix);
                if (attach) cap.attach = rule;
                output.captions.push_back(std::move(cap));
            }
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id     = "islem.kose_numarala",
        .python = "number_vertices",
        .names  = {"KÖŞENUMARALA", "KOSENUMARALA", "NUMBERVERTICES", "KNM"},
        .title  = "Köşeleri numarala",
        .summary = "Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak "
                   "sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, "
                   "köşe taşınınca izler.",
        .group   = "Etiketleme",
        .icon    = "kose_no",
        .applies = Applies::Faces | Applies::Lines,
        .params =
            {
                ToolParam::point("baslangic",
                                 "Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe")
                    .en("start"),
                ToolParam::choice("yon", "Sayım yönü", {"ters", "saat"}, "ters").en("direction"),
                ToolParam::text("onek", "Numaranın önüne gelen yazı (örnek: A, K-)").en("prefix"),
                ToolParam::integer("basamak",
                                   "Numaranın en az basamak sayısı; eksikler dolgu ile tamamlanır",
                                   0, 0, 12)
                    .en("digits"),
                ToolParam::text("dolgu", "Basamak dolgusu", "0").en("pad"),
                ToolParam::integer("ilk", "İlk köşenin numarası", 1, 0, 1000000000)
                    .en("first_number"),
                ToolParam::text("sonek", "Numaranın arkasına gelen yazı").en("suffix"),
                ToolParam::integer("yukseklik",
                                   "Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm",
                                   0, 0, 100000000)
                    .en("height"),
                ToolParam::integer("bosluk",
                                   "Köşe ile yazı arası, milimetre; 0 = yüksekliğin yarısı", 0, 0,
                                   100000000)
                    .en("gap"),
                ToolParam::boolean("bagla", "Numarayı köşesine bağla: köşe taşınınca numara izler",
                                   true)
                    .en("attach"),
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
