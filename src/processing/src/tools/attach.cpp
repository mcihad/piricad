// SPDX-License-Identifier: GPL-3.0-or-later
// islem.bagla — BAĞLA: captions made to FOLLOW an object.
//
// UZUNLUKYAZ and KÖŞENUMARALA attach what they write as they write it. A caption
// that arrived some other way — typed with METİN, imported from a DXF, drawn
// before attachments existed — is free, and this is how it is tied to the line
// or the parcel it is about: pick the captions, name the object, say whether
// each hangs off the nearest edge or the nearest corner, and whether its words
// are its own or the edge's length. Nothing moves on attach: the caption stays
// exactly where it is and the difference to the rule's place is recorded as the
// hand's offset, so following begins from where the caption already stands.
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

/// Exact squared distance, for choosing the nearest ring of a multi-ring source.
core::Int128 distance2(core::Point2 a, core::Point2 b) noexcept
{
    const core::Int128 dx = static_cast<core::Int128>(a.x) - static_cast<core::Int128>(b.x);
    const core::Int128 dy = static_cast<core::Int128>(a.y) - static_cast<core::Int128>(b.y);
    return dx * dx + dy * dy;
}

class Attach final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        if (!input.args.has("kaynak") || input.references.empty())
            return core::err(core::ErrorCode::InvalidArgument,
                             "Bağlanılacak nesne verilmedi: kaynak=<kimlik> yazın ya da karttaki "
                             "Kaynak alanından sahneden seçin.");
        const InputEntity& source = input.references.front();
        if (source.rings.empty() || source.rings.front().points.size() < 2)
            return core::err(core::ErrorCode::InvalidArgument,
                             "Kaynak nesnenin bağlanılacak bir kenarı ya da köşesi yok: " +
                                 std::to_string(source.key));

        const std::string bag        = input.args.get("bag").as_text();
        const std::string tur        = input.args.get("tur").as_text();
        const bool by_vertex         = bag == "kose";
        const bool by_centre         = bag == "merkez";
        const bool derive_length     = tur == "uzunluk";
        const core::DrawingUnit unit = unit_named(input.args.get("birim").as_text());
        const auto precision = static_cast<std::uint8_t>(input.args.get("ondalik").as_int());
        const char separator = input.args.get("ayrac").as_text() == "nokta" ? '.' : ',';
        std::string format   = input.args.get("bicim").as_text();
        // WHAT A CAPTION FILLED FROM ITS SOURCE SAYS (command/text_fields.hpp):
        // its area, its length when it hangs off the whole object rather than
        // one edge, or the user's own format with any of those and any column.
        const bool derive_fields = tur == "alan" || tur == "bicim" || (derive_length && by_centre);
        if (tur == "alan" && format.empty()) format = "{#alan} m²";
        if (derive_length && by_centre && format.empty())
            format = std::string("{#uzunluk}") +
                     core::attach_unit_suffix(static_cast<std::uint8_t>(unit));
        if (tur == "bicim" && format.empty())
            return core::err(core::ErrorCode::InvalidArgument,
                             "tur=bicim bir kalıp ister: bicim=\"Ada {ada} · {#alan} m²\" gibi.");
        if (format.empty())
            format = std::string("{}") + core::attach_unit_suffix(static_cast<std::uint8_t>(unit));

        std::size_t done = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            if (e.rings.empty() || e.rings.front().points.empty() ||
                static_cast<std::int64_t>(e.key) == source.key) {
                progress.at(++done, input.entities.size());
                continue;
            }
            // The caption's centre is the first vertex of its baseline.
            const core::Point2 at = e.rings.front().points.front();

            // THE MIDDLE OF THE OBJECT: its outer ring's box, and the offset that
            // keeps the caption where it stands.
            if (by_centre) {
                const InputEntity::Ring& outer = source.rings.front();
                core::Attachment a;
                a.source = static_cast<core::EntityKey>(static_cast<std::uint64_t>(source.key));
                a.anchor = core::AttachAnchor::Centre;
                if (derive_fields) {
                    a.derive    = core::AttachDerive::Fields;
                    a.unit      = static_cast<std::uint8_t>(unit);
                    a.precision = precision;
                    a.separator = separator;
                    a.format    = format;
                }
                const auto rule = core::attach_place(
                    outer.points, outer.role != core::RingRole::Open, a, e.text_height, false);
                if (rule) {
                    core::attach_measure_offset(*rule, at, a);
                    ToolOutput::Replacement r;
                    r.key    = e.key;
                    r.attach = a;
                    output.replacements.push_back(std::move(r));
                    ++output.touched;
                }
                progress.at(++done, input.entities.size());
                continue;
            }

            // The ring of the source nearest the caption, then the feature of
            // that ring nearest it.
            std::size_t best_ring = 0;
            core::Int128 best_d   = 0;
            bool have             = false;
            for (std::size_t r = 0; r < source.rings.size(); ++r) {
                const InputEntity::Ring& ring = source.rings[r];
                if (ring.points.size() < 2) continue;
                const bool closed     = ring.role != core::RingRole::Open;
                const std::uint32_t i = core::attach_nearest(
                    ring.points, closed,
                    by_vertex ? core::AttachAnchor::Vertex : core::AttachAnchor::Edge, at);
                core::Point2 feature = ring.points[i];
                if (!by_vertex) {
                    const core::Point2 q = ring.points[(i + 1) % ring.points.size()];
                    feature = core::Point2{(feature.x + q.x) / 2, (feature.y + q.y) / 2};
                }
                const core::Int128 d = distance2(feature, at);
                if (!have || d < best_d) {
                    have      = true;
                    best_d    = d;
                    best_ring = r;
                }
            }
            if (!have) {
                progress.at(++done, input.entities.size());
                continue;
            }
            const InputEntity::Ring& ring = source.rings[best_ring];
            const bool closed             = ring.role != core::RingRole::Open;

            core::Attachment a;
            a.source = static_cast<core::EntityKey>(static_cast<std::uint64_t>(source.key));
            a.anchor = by_vertex ? core::AttachAnchor::Vertex : core::AttachAnchor::Edge;
            a.ring   = static_cast<std::uint16_t>(best_ring);
            a.index  = core::attach_nearest(ring.points, closed, a.anchor, at);
            a.side   = by_vertex ? core::AttachSide::Outside
                                 : core::attach_side_of(ring.points, closed, a.index, at);
            a.gap    = e.text_height > 0 ? e.text_height / 2 : 0;
            if (derive_fields || (derive_length && !by_vertex)) {
                a.derive = derive_fields ? core::AttachDerive::Fields : core::AttachDerive::Length;
                a.unit   = static_cast<std::uint8_t>(unit);
                a.precision = precision;
                a.separator = separator;
                a.format    = format;
            }

            // THE HAND'S OFFSET is whatever carries the rule's place to where the
            // caption already is, so attaching moves nothing.
            const auto rule = core::attach_place(ring.points, closed, a, e.text_height, false);
            if (!rule) {
                progress.at(++done, input.entities.size());
                continue;
            }
            core::attach_measure_offset(*rule, at, a);

            ToolOutput::Replacement r;
            r.key    = e.key;
            r.attach = a;
            if (a.derive == core::AttachDerive::Length)
                r.text = core::attach_text(ring.points, closed, a);
            output.replacements.push_back(std::move(r));
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        if (output.touched == 0)
            output.notes.push_back("Kapsamdaki yazıların hiçbiri bağlanamadı.");
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id     = "islem.bagla",
        .python = "attach",
        .names  = {"BAĞLA", "BAGLA", "ATTACH", "BĞ", "BG"},
        .title  = "Yazıyı nesneye bağla",
        .summary =
            "Kapsamdaki yazıları seçilen nesnenin en yakın kenarına, köşesine ya da ortasına "
            "bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu, nesnenin "
            "alanı ya da sütunlarıyla doldurulan bir kalıp olur ve nesne değişince "
            "yeniden yazılır.",
        .group   = "Etiketleme",
        .icon    = "yazi",
        .applies = Applies::Texts,
        .params =
            {
                ToolParam::object("kaynak", "Yazıların bağlanacağı nesne (çizgi ya da alan)")
                    .en("source"),
                ToolParam::choice("bag",
                                  "Neye bağlanacağı: en yakın kenar, en yakın köşe ya da nesnenin "
                                  "ortası",
                                  {"kenar", "kose", "merkez"}, "kenar")
                    .en("attach_to"),
                ToolParam::choice("tur",
                                  "Yazının sözü: kendi yazısı (sabit), kenarın ya da nesnenin "
                                  "uzunluğu, nesnenin alanı ya da bicim kalıbı",
                                  {"sabit", "uzunluk", "alan", "bicim"}, "sabit")
                    .en("type"),
                ToolParam::choice("birim", "Uzunluğun birimi (tur=uzunluk)",
                                  {"metre", "santimetre", "milimetre", "kilometre"}, "metre")
                    .en("unit"),
                ToolParam::integer("ondalik", "Virgülden sonraki basamak sayısı (tur=uzunluk)", 2,
                                   0, 6)
                    .en("decimals"),
                ToolParam::text("bicim",
                                "Yazının kalıbı: {} sayının yerini tutar; {#alan}, {#cevre}, "
                                "{#uzunluk} ölçülür, {sutun} sütunun değeridir")
                    .en("format"),
                ToolParam::choice("ayrac", "Ondalık ayracı (tur=uzunluk)", {"virgul", "nokta"},
                                  "virgul")
                    .en("decimal_separator"),
            },
        .output        = OutputShape::InPlace,
        .output_suffix = "",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(attach)
{
    static const Attach tool;
    return tool;
}

} // namespace kentos::processing
