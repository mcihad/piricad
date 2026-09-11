// SPDX-License-Identifier: GPL-3.0-or-later
// islem.uzunluk_yaz — UZUNLUKYAZ: every edge gets its length written along it.
//
// The surveyor's plan sheet writes the length of every parcel edge beside the
// edge, parallel to it, readable from the bottom of the sheet. Doing that by
// hand with METİN for two hundred edges is an afternoon; this is the same
// caption placed by the same rule for every edge in the scope, in the unit and
// the format the user asks for.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::processing {
namespace {

core::DrawingUnit unit_named(const std::string& word)
{
    if (word == "santimetre") return core::DrawingUnit::Centimetre;
    if (word == "milimetre") return core::DrawingUnit::Millimetre;
    if (word == "kilometre") return core::DrawingUnit::Kilometre;
    return core::DrawingUnit::Metre;
}

const char* unit_suffix(core::DrawingUnit unit)
{
    switch (unit) {
    case core::DrawingUnit::Centimetre: return " cm";
    case core::DrawingUnit::Millimetre: return " mm";
    case core::DrawingUnit::Kilometre: return " km";
    default: return " m";
    }
}

/// `{}` in the format becomes the figure; a format without it gets the figure
/// appended, so a typo cannot swallow the number.
std::string fill(const std::string& format, const std::string& figure)
{
    const auto at = format.find("{}");
    if (at == std::string::npos) return format + figure;
    return format.substr(0, at) + figure + format.substr(at + 2);
}

class LabelLength final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        const core::DrawingUnit unit = unit_named(input.args.get("birim").as_text());
        const auto precision         = static_cast<unsigned>(input.args.get("ondalik").as_int());
        const char separator         = input.args.get("ayrac").as_text() == "nokta" ? '.' : ',';
        std::string format           = input.args.get("bicim").as_text();
        if (format.empty()) format = std::string("{}") + unit_suffix(unit);
        // Which side: what was asked, or the sensible one — outside a face,
        // above (the reading direction's left) a line.
        const std::string asked = input.args.get("taraf").as_text();
        const core::Mm shortest = input.args.get("enaz").as_int();

        // The height: what was asked for, or 2,5 mm of paper at the plan scale.
        core::Mm height = input.args.get("yukseklik").as_int();
        if (height <= 0)
            height = std::max<core::Mm>(1, core::mul_div_round(2500, input.plan_scale, 1000));
        core::Mm gap = input.args.get("bosluk").as_int();
        if (gap <= 0) gap = height / 2;

        std::size_t done = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            bool any = false;
            for (const InputEntity::Ring& ring : e.rings) {
                const std::size_t n = ring.points.size();
                if (n < 2) continue;
                const bool closed      = ring.role != core::RingRole::Open;
                const std::size_t segs = closed ? n : n - 1;
                // Which way the ring turns decides which side is OUTSIDE.
                double twice = 0.0;
                if (closed)
                    for (std::size_t i = 0; i < n; ++i) {
                        const core::Point2 a = ring.points[i];
                        const core::Point2 b = ring.points[(i + 1) % n];
                        twice += static_cast<double>(a.x) * static_cast<double>(b.y) -
                                 static_cast<double>(b.x) * static_cast<double>(a.y);
                    }
                const bool ccw = twice > 0.0;

                for (std::size_t i = 0; i < segs; ++i) {
                    const core::Point2 a = ring.points[i];
                    const core::Point2 b = ring.points[(i + 1) % n];
                    const core::Mm len   = core::segment_length(a, b);
                    if (len <= 0 || len < shortest) continue;

                    // The edge's direction as walked, and the side the caption goes.
                    const auto dx   = static_cast<double>(b.x - a.x);
                    const auto dy   = static_cast<double>(b.y - a.y);
                    const double l  = std::sqrt(dx * dx + dy * dy);
                    const double ux = dx / l;
                    const double uy = dy / l;
                    const std::string side =
                        asked == "otomatik" ? (closed ? std::string("dis") : std::string("sol"))
                                            : asked;
                    // Left of the walk; for a closed ring the interior is on this
                    // side when the ring turns counter-clockwise.
                    double nx = -uy;
                    double ny = ux;
                    if (side == "dis" || side == "ic") {
                        const bool outward = side == "dis";
                        if (closed ? (ccw == outward) : !outward) {
                            nx = -nx;
                            ny = -ny;
                        }
                    } else {
                        // sol / sag are relative to the READING direction, which
                        // is the walk turned to read left to right.
                        double rx = ux;
                        double ry = uy;
                        if (rx < 0.0 || (rx == 0.0 && ry < 0.0)) {
                            rx = -rx;
                            ry = -ry;
                        }
                        nx = -ry;
                        ny = rx;
                        if (side == "sag") {
                            nx = -nx;
                            ny = -ny;
                        }
                    }

                    ToolOutput::Caption cap;
                    const auto offset =
                        static_cast<double>(gap) + static_cast<double>(height) / 2.0;
                    cap.centre = core::Point2{(a.x + b.x) / 2 + core::mm_round(nx * offset),
                                              (a.y + b.y) / 2 + core::mm_round(ny * offset)};
                    cap.dir_x  = ux;
                    cap.dir_y  = uy;
                    cap.height = height;
                    cap.text   = fill(format,
                                      core::format_dimension_length(len, unit, precision, separator));
                    output.captions.push_back(std::move(cap));
                    any = true;
                }
            }
            if (any) ++output.touched;
            progress.at(++done, input.entities.size());
        }
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id      = "islem.uzunluk_yaz",
        .names   = {"UZUNLUKYAZ", "UZUNLUKYAZ", "LABELLENGTH", "UZY"},
        .title   = "Kenar uzunluklarını yaz",
        .summary = "Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir "
                   "yazı olarak yazar.",
        .group   = "Etiketleme",
        .icon    = "cetvel",
        .applies = Applies::Lines | Applies::Faces,
        .params =
            {
                ToolParam::choice("birim", "Uzunluğun yazılacağı birim",
                                  {"metre", "santimetre", "milimetre", "kilometre"}, "metre"),
                ToolParam::integer("ondalik", "Virgülden sonraki basamak sayısı", 2, 0, 6),
                ToolParam::text(
                    "bicim", "Yazının kalıbı; {} sayının yerini tutar (örnek: \"{} m\", \"L={}\")"),
                ToolParam::choice("ayrac", "Ondalık ayracı", {"virgul", "nokta"}, "virgul"),
                ToolParam::choice("taraf", "Yazının kenarın hangi yanına düşeceği",
                                  {"otomatik", "sol", "sag", "dis", "ic"}, "otomatik"),
                ToolParam::integer("yukseklik",
                                   "Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm",
                                   0, 0, 100000000),
                ToolParam::integer("bosluk",
                                   "Kenar ile yazı arası, milimetre; 0 = yüksekliğin yarısı", 0, 0,
                                   100000000),
                ToolParam::integer("enaz", "Bundan kısa kenarlara yazı yazılmaz, milimetre", 0, 0,
                                   1000000000),
            },
        .output        = OutputShape::NewEntities,
        .output_suffix = "uzunluk",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(label_length)
{
    static const LabelLength tool;
    return tool;
}

} // namespace kentos::processing
