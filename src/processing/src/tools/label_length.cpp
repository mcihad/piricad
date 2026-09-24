// SPDX-License-Identifier: GPL-3.0-or-later
// islem.uzunluk_yaz — UZUNLUKYAZ: every edge gets its length written along it.
//
// The surveyor's plan sheet writes the length of every parcel edge beside the
// edge, parallel to it, readable from the bottom of the sheet. Doing that by
// hand with METİN for two hundred edges is an afternoon; this is the same
// caption placed by the same rule for every edge in the scope, in the unit and
// the format the user asks for.
//
// AND THE CAPTION FOLLOWS THE EDGE. Unless told not to, every length written is
// ATTACHED to its edge (core/attach.hpp): the command that later moves,
// stretches or turns the line re-places the caption beside the same edge, still
// parallel, and re-writes the figure to the new length — at that command's
// commit, in its one undo step. The rule that places it here is the rule that
// re-places it then; there is one of them, in core.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/units.hpp"

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

class LabelLength final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        const core::DrawingUnit unit = unit_named(input.args.get("birim").as_text());
        const auto precision = static_cast<std::uint8_t>(input.args.get("ondalik").as_int());
        const char separator = input.args.get("ayrac").as_text() == "nokta" ? '.' : ',';
        std::string format   = input.args.get("bicim").as_text();
        if (format.empty())
            format = std::string("{}") + core::attach_unit_suffix(static_cast<std::uint8_t>(unit));
        // Which side: what was asked, or the sensible one — outside a face,
        // above (the reading direction's left) a line.
        const std::string asked = input.args.get("taraf").as_text();
        const core::Mm shortest = input.args.get("enaz").as_int();
        const bool attach       = input.args.get("bagla").as_bool(true);

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
            for (std::size_t r = 0; r < e.rings.size(); ++r) {
                const InputEntity::Ring& ring = e.rings[r];
                const std::size_t n           = ring.points.size();
                if (n < 2) continue;
                const bool closed      = ring.role != core::RingRole::Open;
                const std::size_t segs = closed ? n : n - 1;

                // THE RULE, once per edge: where the caption goes and what it
                // says both come from `core::attach_place` / `attach_text`, so a
                // caption written here and one re-placed after the edge moved
                // are the same caption.
                core::Attachment rule;
                rule.source    = static_cast<core::EntityKey>(static_cast<std::uint64_t>(e.key));
                rule.anchor    = core::AttachAnchor::Edge;
                rule.derive    = core::AttachDerive::Length;
                rule.ring      = static_cast<std::uint16_t>(r);
                rule.gap       = gap;
                rule.unit      = static_cast<std::uint8_t>(unit);
                rule.precision = precision;
                rule.separator = separator;
                rule.format    = format;
                if (asked == "otomatik")
                    rule.side = closed ? core::AttachSide::Outside : core::AttachSide::Left;
                else
                    rule.side = core::attach_side_from_name(asked).value_or(core::AttachSide::Left);

                for (std::size_t i = 0; i < segs; ++i) {
                    const core::Point2 a = ring.points[i];
                    const core::Point2 b = ring.points[(i + 1) % n];
                    const core::Mm len   = core::segment_length(a, b);
                    if (len <= 0 || len < shortest) continue;

                    rule.index       = static_cast<std::uint32_t>(i);
                    const auto place = core::attach_place(ring.points, closed, rule, height);
                    const auto text  = core::attach_text(ring.points, closed, rule);
                    if (!place || !text) continue;

                    ToolOutput::Caption cap;
                    cap.centre = place->centre;
                    cap.dir_x  = place->dir_x;
                    cap.dir_y  = place->dir_y;
                    cap.height = height;
                    cap.text   = *text;
                    if (attach) cap.attach = rule;
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
        .id     = "islem.uzunluk_yaz",
        .python = "label_length",
        .names  = {"UZUNLUKYAZ", "UZUNLUKYAZ", "LABELLENGTH", "UZY"},
        .title  = "Kenar uzunluklarını yaz",
        .summary = "Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir "
                   "yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir.",
        .group   = "Etiketleme",
        .icon    = "uzunluk",
        .applies = Applies::Lines | Applies::Faces,
        .params =
            {
                ToolParam::choice("birim", "Uzunluğun yazılacağı birim",
                                  {"metre", "santimetre", "milimetre", "kilometre"}, "metre")
                    .en("unit"),
                ToolParam::integer("ondalik", "Virgülden sonraki basamak sayısı", 2, 0, 6)
                    .en("decimals"),
                ToolParam::text(
                    "bicim", "Yazının kalıbı; {} sayının yerini tutar (örnek: \"{} m\", \"L={}\")")
                    .en("format"),
                ToolParam::choice("ayrac", "Ondalık ayracı", {"virgul", "nokta"}, "virgul")
                    .en("decimal_separator"),
                ToolParam::choice("taraf", "Yazının kenarın hangi yanına düşeceği",
                                  {"otomatik", "sol", "sag", "dis", "ic"}, "otomatik")
                    .en("side"),
                ToolParam::integer("yukseklik",
                                   "Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm",
                                   0, 0, 100000000)
                    .en("height"),
                ToolParam::integer("bosluk",
                                   "Kenar ile yazı arası, milimetre; 0 = yüksekliğin yarısı", 0, 0,
                                   100000000)
                    .en("gap"),
                ToolParam::integer("enaz", "Bundan kısa kenarlara yazı yazılmaz, milimetre", 0, 0,
                                   1000000000)
                    .en("min_length"),
                ToolParam::boolean("bagla",
                                   "Yazıyı kenarına bağla: kenar taşınınca yazı izler, uzunluk "
                                   "yeniden yazılır",
                                   true)
                    .en("attach"),
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
