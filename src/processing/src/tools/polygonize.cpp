// SPDX-License-Identifier: GPL-3.0-or-later
// islem.alan_uret — ALANÜRET: every face a network of lines closes, as an area.
//
// THE PARCELS WERE DRAWN AS LINES. A cadastral sheet from a DXF, a digitised
// plan, a surveyor's boundary lines: the ground is divided, and nowhere in the
// document is a single parcel an object. SINIR answers one click at a time;
// this answers the whole network at once — every face it closes becomes an
// area on the output layer, each island inside a face a hole in it, the lines
// left as they were (TODOS C-09).
//
// ONE CORE WITH SINIR (core/planar.hpp): the same noding, the same node
// tolerance, the same rule for which kind a face is written as
// (`core::face_shape`). A face found here and a face found by a click inside it
// are the same face, vertex for vertex.
//
// NOTHING CLOSES SILENTLY. An end that meets nothing is counted, its gap to the
// nearest linework measured and marked on the canvas; a face it keeps from
// closing is simply not made. Bridging is asked for by name (`bosluk=`), and
// every bridge laid is said.
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/core/planar.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace kentos::processing {
namespace {

/// Millimetres as the metres a user reads: "5 cm" below a metre, two decimals
/// above it — the way SINIR says a gap.
std::string gap_words(core::Mm v)
{
    const core::Mm a = v < 0 ? -v : v;
    if (a < 1000) {
        if (a % 10 == 0) return std::to_string(a / 10) + " cm";
        return std::to_string(a) + " mm";
    }
    const core::Mm cm = (a + 5) / 10;
    std::string frac  = std::to_string(cm % 100);
    if (frac.size() < 2) frac.insert(frac.begin(), '0');
    return std::to_string(cm / 100) + "," + frac + " m";
}

/// An area in square metres to two decimals, divided in integers (Article 2.4).
std::string square_metres(core::Mm2 v)
{
    const auto whole        = static_cast<std::uint64_t>(v < 0 ? -v : v);
    const std::uint64_t cm2 = (whole + 5000) / 10000;
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac.insert(frac.begin(), '0');
    return std::to_string(cm2 / 100) + "," + frac + " m²";
}

/// The rings of a snapshot as the core reads them.
std::vector<core::StoredRing> stored(const std::vector<InputEntity::Ring>& rings)
{
    std::vector<core::StoredRing> out;
    out.reserve(rings.size());
    for (const InputEntity::Ring& ring : rings)
        out.push_back(core::StoredRing{ring.points, ring.role});
    return out;
}

class Polygonize final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        const bool islands   = input.args.get("ada").as_bool(true);
        const double metres  = input.args.get("bosluk").as_number();
        const core::Mm reach = core::mm_round(metres * static_cast<double>(core::kMmPerMetre));
        if (reach < 0)
            return core::err(core::ErrorCode::InvalidArgument,
                             "Köprülenecek boşluk eksi olamaz; metre olarak 0 ya da daha büyük "
                             "verin.");

        // ---- the linework, every object read the way SINIR reads it ----
        std::vector<core::NetworkPiece> pieces;
        const std::size_t total = input.entities.size();
        for (std::size_t i = 0; i < total; ++i) {
            if (progress.cancelled()) return cancelled();
            const InputEntity& e                      = input.entities[i];
            const std::vector<core::StoredRing> rings = stored(e.rings);
            const std::vector<core::StoredRing> drawn = stored(e.drawn);
            std::vector<core::NetworkPiece> own =
                core::record_pieces(e.kind, rings, e.payload, drawn, static_cast<std::uint32_t>(i));
            if (!own.empty()) ++output.touched;
            pieces.insert(pieces.end(), own.begin(), own.end());
            progress.at(i + 1, 2 * total);
        }
        if (pieces.empty()) return core::ok();

        auto built = core::Network::build(pieces, input.node_tolerance, reach);
        if (!built) return built.error();
        const core::Network& net = built.value();
        if (progress.cancelled()) return cancelled();

        // ---- every face, written as the kind that holds it ----
        const std::vector<core::NetworkFace> faces = net.faces(islands);
        core::Mm chords                            = 0;
        bool approximate                           = false;
        std::size_t holes                          = 0;
        core::Mm2 area                             = 0;
        for (std::size_t f = 0; f < faces.size(); ++f) {
            if (progress.cancelled()) return cancelled();
            const core::NetworkFace& face = faces[f];
            const core::FaceShape shape   = core::face_shape(face);
            if (shape.whole) {
                output.records.push_back(ToolOutput::Record{
                    shape.record.kind, shape.record.ring, shape.record.role, shape.record.payload});
            } else {
                ToolOutput::Face out;
                out.exterior = shape.rings.front();
                out.holes.assign(shape.rings.begin() + 1, shape.rings.end());
                output.faces.push_back(std::move(out));
            }
            chords      = std::max(chords, shape.chords);
            approximate = approximate || face.outer.approximate ||
                          std::ranges::any_of(
                              face.holes, [](const core::FaceRing& h) { return h.approximate; });
            holes += face.holes.size();
            area += face.area;
            progress.at(total + (f + 1) * total / std::max<std::size_t>(faces.size(), 1),
                        2 * total);
        }

        // ---- what the network did not close, said and shown ----
        const std::vector<core::OpenEnd> open = net.open_ends();
        std::size_t gaps                      = 0;
        core::Mm narrowest                    = 0;
        std::vector<const core::OpenEnd*> near;
        for (const core::OpenEnd& end : open) {
            if (!end.has_nearest) continue;
            ++gaps;
            if (gaps == 1 || end.distance < narrowest) narrowest = end.distance;
            near.push_back(&end);
        }
        // MARKED NARROWEST FIRST, AND AN END IN ONE GAP ONLY. Two ends that see
        // each other are one gap, and an end already in a narrower gap does not
        // send a second, wider line across the same corner.
        std::ranges::stable_sort(near, [](const core::OpenEnd* a, const core::OpenEnd* b) {
            return a->distance < b->distance;
        });
        std::set<core::Point2> used;
        for (const core::OpenEnd* end : near) {
            if (used.contains(end->at) || used.contains(end->nearest)) continue;
            used.insert(end->at);
            used.insert(end->nearest);
            command::MeasureMark m;
            m.shape  = command::MeasureMark::Shape::Gap;
            m.points = {end->at, end->nearest};
            m.labels = {"boşluk " + gap_words(end->distance)};
            output.marks.push_back(std::move(m));
        }

        if (!faces.empty()) {
            std::string said = std::to_string(faces.size()) + " kapalı göz bulundu, toplam alan " +
                               square_metres(area);
            if (holes > 0) said += ", " + std::to_string(holes) + " ada delik olarak bırakıldı";
            output.notes.push_back(said + ".");
        } else {
            output.notes.emplace_back("Çizgiler kapalı bir göz oluşturmuyor.");
        }
        if (gaps > 0)
            output.notes.push_back(std::to_string(gaps) +
                                   " açık uç bir çizgiye yakın ama değmiyor; en dar boşluk " +
                                   gap_words(narrowest) +
                                   ". Uçlar tuvalde işaretlendi; kapanmayan göz alan olmadı. "
                                   "Köprülemek için bosluk=<metre> verin.");
        if (!net.bridges().empty()) {
            std::string said = std::to_string(net.bridges().size()) + " boşluk köprülendi:";
            for (std::size_t b = 0; b < net.bridges().size(); ++b)
                said += (b == 0 ? " " : ", ") + gap_words(net.bridges()[b].width);
            output.notes.push_back(said + ".");
        }
        if (net.snaps().moved > 0)
            output.notes.push_back(std::to_string(net.snaps().moved) +
                                   " uç düğüm toleransıyla birleştirildi (en çok " +
                                   gap_words(net.snaps().largest) + ").");
        if (net.collapsed() > 0)
            output.notes.push_back(std::to_string(net.collapsed()) +
                                   " göz milimetreye yuvarlanınca alansız kaldığı için atlandı.");
        if (approximate) output.notes.emplace_back("Elips ya da spline çizildiği hâliyle izlendi.");
        if (chords > 0)
            output.notes.push_back("Delikli alanlar yay taşıyamadığı için yayları kirişlerle "
                                   "yazıldı (sapma ≤ " +
                                   gap_words(chords) + ").");
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id     = "islem.alan_uret",
        .python = "polygonize",
        .names  = {"ALANÜRET", "ALANURET", "POLYGONIZE", "ALÜ"},
        .title  = "Çizgilerden alan üret",
        .summary = "Kapsamdaki çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki "
                   "adalar delik olur, açık uçlar sayılıp gösterilir ve hiçbiri kendiliğinden "
                   "kapanmaz.",
        .group   = "Geometri",
        .icon    = "alan",
        .applies = Applies::Lines | Applies::Faces | Applies::Curves,
        .params =
            {
                ToolParam::boolean("ada",
                                   "Bir gözün içindeki kapalı çizgiler o alanın deliği olsun; "
                                   "kapalıysa göz dış sınırıyla dolu çizilir",
                                   true)
                    .en("islands"),
                ToolParam::number("bosluk",
                                  "Bu genişliğe kadar açık uçları köprüle, metre; 0: hiçbir "
                                  "boşluk kendiliğinden kapanmaz",
                                  "0")
                    .en("gap"),
            },
        .output        = OutputShape::NewEntities,
        .output_suffix = "alan",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(polygonize)
{
    static const Polygonize tool;
    return tool;
}

} // namespace kentos::processing
