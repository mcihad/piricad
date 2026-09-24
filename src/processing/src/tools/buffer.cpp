// SPDX-License-Identifier: GPL-3.0-or-later
// islem.tampon — TAMPON: everything within a distance of the objects, as a face.
//
// The GIS question a CAD parallel does not answer: which part of the ground lies
// within 10 m of this stream, 5 m of this pipe, 50 m of this well. The answer is
// an AREA on both sides of a line, a disc round a point, a face grown round a
// face — and where two of them overlap it is ONE area, because a protection
// band is where the rule applies and the rule does not apply twice. The CAD
// parallel of a line is a line on one side (OFSET); keeping the two apart is
// TODOS C-03, and before it OFSET drew this band and called it a parallel.
//
// THE GEOMETRY IS CLIPPER2'S, behind `core::buffer`: CLAUDE.md 5.16 forbids
// hand-rolling a solved problem, and a hand-rolled buffer is wrong exactly where
// a real one is interesting — a line doubling back on itself, two bands meeting,
// a courtyard left inside a ring road.
//
// A CURVE IS BUFFERED AS IT IS DRAWN (`InputEntity::drawn`): its definition is a
// centre and a rim point, and the ground within 5 m of a circle is not the ground
// within 5 m of its centre.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>

namespace kentos::processing {
namespace {

/// Millimetres as the metres a user reads, three decimals, Turkish comma; the
/// sign dropped, because the sentence says which way.
std::string metres_of(core::Mm v)
{
    const auto whole = static_cast<std::uint64_t>(v < 0 ? -v : v);
    std::string frac = std::to_string(whole % 1000);
    while (frac.size() < 3)
        frac.insert(frac.begin(), '0');
    return std::to_string(whole / 1000) + "," + frac + " m";
}

core::JoinStyle corner_named(const std::string& word)
{
    if (word == "koseli") return core::JoinStyle::Miter;
    if (word == "pah") return core::JoinStyle::Bevel;
    return core::JoinStyle::Round;
}

core::EndStyle end_named(const std::string& word)
{
    if (word == "duz") return core::EndStyle::Butt;
    if (word == "kare") return core::EndStyle::Square;
    return core::EndStyle::Round;
}

/// What of `e` the buffer is taken of, added to `source`.
void gather(const InputEntity& e, core::BufferSource& source)
{
    // A face's rings: an exterior opens a face and the interiors after it are
    // its holes (R11 order).
    const auto faces_of = [&source](const std::vector<InputEntity::Ring>& rings) {
        for (const InputEntity::Ring& ring : rings) {
            if (ring.points.size() < 3) continue;
            if (ring.role == core::RingRole::Interior && !source.faces.empty())
                source.faces.back().holes.push_back(ring.points);
            else if (ring.role != core::RingRole::Open)
                source.faces.push_back(core::Polygon{ring.points, {}});
        }
    };

    switch (e.cls) {
    case Applies::Points:
        if (!e.rings.empty() && !e.rings.front().points.empty())
            source.points.push_back(e.rings.front().points.front());
        break;
    case Applies::Lines:
        for (const InputEntity::Ring& ring : e.rings)
            if (ring.points.size() >= 2) source.runs.push_back(ring.points);
        break;
    case Applies::Faces: faces_of(e.rings); break;
    case Applies::Curves:
        // A CLOSED CURVE ENCLOSES what it draws round, as a face does: the
        // ground within 5 m of a circular pond includes the pond.
        for (const InputEntity::Ring& ring : e.drawn) {
            if (ring.role == core::RingRole::Open) {
                if (ring.points.size() >= 2) source.runs.push_back(ring.points);
            } else if (ring.points.size() >= 3) {
                source.faces.push_back(core::Polygon{ring.points, {}});
            }
        }
        break;
    case Applies::None:
    case Applies::Texts: break;
    }
}

class Buffer final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        const double metres     = input.args.get("mesafe").as_number();
        const core::Mm distance = core::mm_round(metres * static_cast<double>(core::kMmPerMetre));
        if (distance == 0)
            return core::err(core::ErrorCode::InvalidArgument,
                             "Tampon mesafesi sıfır olamaz. Kaç metre istediğinizi yazın.");

        const core::JoinStyle join = corner_named(input.args.get("kose").as_text());
        const core::EndStyle end   = end_named(input.args.get("uc").as_text());
        const bool dissolve        = input.args.get("birlestir").as_bool(true);

        const auto publish = [&output](const std::vector<core::Polygon>& made) {
            for (const core::Polygon& p : made)
                output.faces.push_back(ToolOutput::Face{p.exterior, p.holes});
        };

        // ONE ANSWER, or one per object. Dissolved is what a protection band
        // is; kept apart is what a per-object report ("the land within 10 m of
        // EACH well") needs.
        core::BufferSource all;
        std::size_t done = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            if (dissolve) {
                gather(e, all);
            } else {
                core::BufferSource one;
                gather(e, one);
                auto made = core::buffer(one, distance, join, end);
                if (!made) return made.error();
                publish(made.value());
            }
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        if (dissolve && output.touched > 0) {
            auto made = core::buffer(all, distance, join, end);
            if (!made) return made.error();
            publish(made.value());
        }

        // NOTHING IS AN ANSWER, and it is said: eroding a face past half its
        // width leaves no ground at all.
        if (output.touched > 0 && output.faces.empty())
            output.notes.push_back("Bu mesafede tampon kalmıyor: şekiller " + metres_of(distance) +
                                   " içeri alınınca kendi içinde kapanıyor.");
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id      = "islem.tampon",
        .python  = "buffer",
        .names   = {"TAMPON", "BUFFER", "TMP"},
        .title   = "Tampon bölge",
        .summary = "Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak "
                   "çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen "
                   "tamponlar tek alan olur.",
        .group   = "Analiz",
        .icon    = "tampon",
        .applies = Applies::Points | Applies::Lines | Applies::Faces | Applies::Curves,
        .params =
            {
                ToolParam::number("mesafe",
                                  "Tampon mesafesi, metre; eksi değer yalnız alanları içeri "
                                  "aşındırır")
                    .en("distance"),
                ToolParam::boolean("birlestir",
                                   "Üst üste binen tamponları tek alanda birleştir; kapalıysa her "
                                   "nesnenin tamponu ayrı alan olur",
                                   true)
                    .en("dissolve"),
                ToolParam::choice("kose", "Dış köşelerin biçimi", {"yuvarlak", "koseli", "pah"},
                                  "yuvarlak")
                    .en("corner"),
                ToolParam::choice("uc", "Çizgi uçlarının biçimi", {"yuvarlak", "duz", "kare"},
                                  "yuvarlak")
                    .en("end"),
            },
        .output        = OutputShape::NewEntities,
        .output_suffix = "tampon",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(buffer)
{
    static const Buffer tool;
    return tool;
}

} // namespace kentos::processing
