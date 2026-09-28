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
//
// ROUND IS AN ARC (TODOS O-3, CLAUDE.md 2.11). With round corners and round
// ends — the defaults — the buffer goes through the geometry kernel: a point's
// disc is a round area, a line's band is closed by half circles, a face grows
// with an arc at every corner, an arc's band is two concentric arcs — where the
// polygon road leaves a fan of short edges. The polygon road stays for the
// sharp and bevelled corners and the flat and square ends, and for an object
// the kernel does not take whole (an ellipse, a spline, a face with holes),
// whose piece is then unioned with the rest by the kernel.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/command/area_face.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/kernel.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

    // A LINE OR AN AREA WITH ARC EDGES is buffered as it is drawn, as a curve
    // is: its rings are its corners, and the buffer of those would cut every
    // rounded corner off by its chord.
    const std::vector<InputEntity::Ring>& shape = e.drawn.empty() ? e.rings : e.drawn;
    switch (e.cls) {
    case Applies::Points:
        if (!e.rings.empty() && !e.rings.front().points.empty())
            source.points.push_back(e.rings.front().points.front());
        break;
    case Applies::Lines:
        for (const InputEntity::Ring& ring : shape)
            if (ring.points.size() >= 2) source.runs.push_back(ring.points);
        break;
    case Applies::Faces: faces_of(shape); break;
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

/// THE KERNEL'S ROAD IS FOR A PATH OF A FEW HUNDRED PIECES. Each piece is one
/// kernel offset and one argument of the union, a few milliseconds apiece: a
/// thousand-piece line took 2.6 s where the polygon road takes milliseconds,
/// and the kernel cannot be stopped half way through a union. A longer path —
/// a stream digitised vertex by vertex — sends the whole run by the polygon
/// road, and that is said. Not a regulatory value: how long a machine takes.
constexpr std::size_t kKernelPieces = 256;

/// The kernel's buffer of one object: the ground within `d` of it, its round
/// corners and ends true arcs. Nothing when the kernel does not take it whole —
/// an ellipse, a spline, a face with holes — and the polygon road does.
std::optional<std::vector<core::KernelFace>> kernel_buffer(const InputEntity& e, core::Mm d)
{
    if (e.cls == Applies::Points) {
        std::vector<core::KernelFace> out;
        if (d > 0 && !e.rings.empty() && !e.rings.front().points.empty()) {
            const core::Point2 c = e.rings.front().points.front();
            const core::Point2 rim{c.x + d, c.y};
            core::CurvePath disc;
            disc.closed = true;
            disc.pieces.push_back(core::arc_piece(c, d, rim, rim, true));
            out.push_back(core::KernelFace{.outer = std::move(disc), .holes = {}});
        }
        return out;
    }
    if (!e.path) return std::nullopt;
    if (e.path->closed) {
        // A CLOSED PATH as the face it bounds, grown or eroded: the kernel's
        // face offset trims its own loops and keeps a courtyard it closes.
        auto rings = core::kernel_offset(*e.path, d, core::OffsetCorner::Round, false);
        if (!rings) return std::nullopt;
        return core::kernel_faces_of(std::move(rings.value()));
    }
    // AN OPEN LINE HAS NO INSIDE: eroding it leaves nothing, as on the polygon road.
    if (d <= 0) return std::vector<core::KernelFace>{};
    // PIECE BY PIECE, then one union. The kernel's two-sided offset of an open
    // wire does not resolve a band that runs over itself — a U whose arms come
    // close lost the hole it closes, and the ring it gave back crossed itself.
    // The band round each piece is simple — a segment's stadium, an arc's
    // bent one — and their union IS the buffer: the round ends of two pieces
    // meet as the round corner between them (one circle, which the union makes
    // one arc), and every overlap and every closed-off hole comes out true.
    std::vector<core::KernelFace> bands;
    for (const core::PathPiece& piece : e.path->pieces) {
        core::CurvePath one;
        one.pieces.push_back(piece);
        auto rings = core::kernel_offset(one, d, core::OffsetCorner::Round, true);
        if (!rings) return std::nullopt;
        std::ranges::move(core::kernel_faces_of(std::move(rings.value())),
                          std::back_inserter(bands));
    }
    if (bands.size() <= 1) return bands;
    auto merged = core::kernel_boolean(std::span(bands).first(1), std::span(bands).subspan(1),
                                       core::BooleanOp::Union);
    if (!merged) return std::nullopt;
    return std::move(merged.value());
}

/// The vertices a ring is drawn with, without a repeated closing vertex.
std::vector<core::Point2> drawn_ring(const core::CurvePath& path)
{
    std::vector<core::Mm> xs;
    std::vector<core::Mm> ys;
    core::path_outline(path, xs, ys);
    std::vector<core::Point2> out;
    out.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(core::Point2{xs[v], ys[v]});
    if (out.size() > 1 && out.front() == out.back()) out.pop_back();
    return out;
}

bool bends(const core::KernelFace& f)
{
    const auto curved = [](const core::CurvePath& p) {
        return std::ranges::any_of(p.pieces, [](const core::PathPiece& x) {
            return x.kind != core::PathPiece::Kind::Segment;
        });
    };
    return curved(f.outer) || std::ranges::any_of(f.holes, curved);
}

/// One face of a buffer into the output, in the kind that holds it: a polyline
/// face when nothing bends, an arc polyline when its boundary does. A face
/// bounded by arcs that ALSO has a hole is drawn as short edges and said — an
/// object whose edges bend keeps one ring (model.md R9b).
void publish_face(ToolOutput& output, const core::KernelFace& f, std::vector<std::int64_t> from,
                  bool& chorded)
{
    if (!bends(f)) {
        const core::Polygon p = command::face_polygon(f);
        output.faces.push_back(
            ToolOutput::Face{.exterior = p.exterior, .holes = p.holes, .sources = std::move(from)});
        return;
    }
    if (f.holes.empty()) {
        core::PathRecord rec = core::area_record(f.outer);
        output.records.push_back(ToolOutput::Record{
            .kind    = rec.kind,
            .ring    = std::move(rec.ring),
            .role    = rec.role,
            .payload = std::move(rec.payload),
            .sources = std::move(from),
        });
        return;
    }
    chorded = true;
    ToolOutput::Face face;
    face.exterior = drawn_ring(f.outer);
    for (const core::CurvePath& hole : f.holes)
        face.holes.push_back(drawn_ring(hole));
    face.sources = std::move(from);
    output.faces.push_back(std::move(face));
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
        if (join == core::JoinStyle::Round && end == core::EndStyle::Round &&
            core::kernel_available()) {
            const bool fits = std::ranges::all_of(input.entities, [](const InputEntity& e) {
                return !e.path || e.path->pieces.size() <= kKernelPieces;
            });
            if (fits) return run_round(input, output, progress, distance, dissolve);
            output.notes.emplace_back("Kapsamda " + std::to_string(kKernelPieces) +
                                      " köşeden uzun bir çizgi var: tampon hızlı yoldan çizildi, "
                                      "yuvarlak köşeler ve uçlar kısa kenarlarla.");
        }

        // Each face with its origin: the one object a kept-apart buffer was
        // drawn round, or — dissolved — every object, which the runner assumes.
        const auto publish = [&output](const std::vector<core::Polygon>& made,
                                       std::vector<std::int64_t> from) {
            for (const core::Polygon& p : made)
                output.faces.push_back(ToolOutput::Face{p.exterior, p.holes, from});
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
                publish(made.value(), {e.key});
            }
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        if (dissolve && output.touched > 0) {
            auto made = core::buffer(all, distance, join, end);
            if (!made) return made.error();
            publish(made.value(), {});
        }

        // NOTHING IS AN ANSWER, and it is said: eroding a face past half its
        // width leaves no ground at all.
        if (output.touched > 0 && output.faces.empty())
            output.notes.push_back("Bu mesafede tampon kalmıyor: şekiller " + metres_of(distance) +
                                   " içeri alınınca kendi içinde kapanıyor.");
        return core::ok();
    }

private:
    /// The kernel's road: every object's buffer with true arcs, unioned by the
    /// kernel when dissolved.
    static core::Status run_round(const ToolInput& input, ToolOutput& output,
                                  const Progress& progress, core::Mm distance, bool dissolve)
    {
        std::vector<core::KernelFace> all;
        bool chorded     = false;
        std::size_t done = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            std::vector<core::KernelFace> own;
            if (auto arcs = kernel_buffer(e, distance)) {
                own = std::move(*arcs);
            } else {
                // THE POLYGON ROAD FOR THIS ONE, as it is drawn; its piece is
                // unioned with the others by the kernel all the same.
                core::BufferSource one;
                gather(e, one);
                auto made =
                    core::buffer(one, distance, core::JoinStyle::Round, core::EndStyle::Round);
                if (!made) return made.error();
                for (const core::Polygon& p : made.value())
                    own.push_back(command::polygon_face(p));
            }
            if (dissolve) {
                std::ranges::move(own, std::back_inserter(all));
            } else {
                for (const core::KernelFace& f : own)
                    publish_face(output, f, {e.key}, chorded);
            }
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        if (dissolve && !all.empty()) {
            std::vector<core::KernelFace> merged;
            if (all.size() == 1) {
                merged = std::move(all);
            } else {
                auto one = core::kernel_boolean(std::span(all).first(1), std::span(all).subspan(1),
                                                core::BooleanOp::Union);
                if (!one) return one.error();
                merged = std::move(one.value());
            }
            for (const core::KernelFace& f : merged)
                publish_face(output, f, {}, chorded);
        }
        if (output.touched > 0 && output.faces.empty() && output.records.empty())
            output.notes.push_back("Bu mesafede tampon kalmıyor: şekiller " + metres_of(distance) +
                                   " içeri alınınca kendi içinde kapanıyor.");
        if (chorded)
            output.notes.emplace_back("İçinde boşluk kalan yaylı kenarlı tampon kısa kenarlarla "
                                      "yazıldı: yaylı kenarlı bir nesne boşluk taşıyamaz.");
        return core::ok();
    }

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
