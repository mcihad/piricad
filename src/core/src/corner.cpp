// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: cutting a corner off a run. See corner.hpp.
#include "kentos_cad/core/corner.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace kentos::core {
namespace {

struct Unit
{
    double x{0.0};
    double y{0.0};
};

Unit unit_from(Point2 from, Point2 to)
{
    // Metres first: the square of a TM3 coordinate difference in millimetres
    // leaves the 53-bit mantissa long before it leaves int64 (core.md R3).
    const double dx  = mm_to_metres(to.x - from.x);
    const double dy  = mm_to_metres(to.y - from.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) return Unit{};
    return Unit{dx / len, dy / len};
}

Mm length_of(Point2 a, Point2 b)
{
    const double dx = mm_to_metres(b.x - a.x);
    const double dy = mm_to_metres(b.y - a.y);
    return mm_round(std::sqrt(dx * dx + dy * dy) * static_cast<double>(kMmPerMetre));
}

Point2 along(Point2 v, Unit d, Mm distance)
{
    const double m = mm_to_metres(distance);
    return Point2{v.x + mm_round(d.x * m * static_cast<double>(kMmPerMetre)),
                  v.y + mm_round(d.y * m * static_cast<double>(kMmPerMetre))};
}

/// Millimetres as the metres a user reads, three decimals, Turkish comma.
/// Integer arithmetic, so the sentence is the same on every platform.
std::string metres(Mm value)
{
    const bool negative = value < 0;
    const auto whole    = static_cast<std::uint64_t>(negative ? -value : value);
    std::string frac    = std::to_string(whole % 1000);
    while (frac.size() < 3)
        frac.insert(frac.begin(), '0');
    return (negative ? "-" : "") + std::to_string(whole / 1000) + "," + frac + " m";
}

} // namespace

std::optional<std::size_t> nearest_corner(std::span<const Point2> run, bool closed, Point2 probe)
{
    if (run.size() < 3) return std::nullopt;

    double best = -1.0;
    std::size_t at{0};
    for (std::size_t i = 0; i < run.size(); ++i) {
        const double d = distance_squared(run[i], probe);
        if (best < 0.0 || d < best) {
            best = d;
            at   = i;
        }
    }

    // An open run's first and last vertices are ENDS, not corners: there is only
    // one edge at them and nothing to cut across. A point nearest an end is
    // answered with nothing rather than with the corner beside it — which corner
    // was meant is not in the point, and cutting the wrong one of a parcel's
    // corners is not a guess this program makes.
    if (!closed && (at == 0 || at + 1 == run.size())) return std::nullopt;
    return at;
}

Result<CornerCut> cut_corner(std::span<const Point2> run, bool closed, std::size_t at, Mm size,
                             bool fillet)
{
    if (run.size() < 3 || at >= run.size() || (!closed && (at == 0 || at + 1 == run.size())))
        return err(ErrorCode::InvalidArgument,
                   "Burada iki kenarın buluştuğu bir köşe yok. Açık bir çizginin uçları köşe "
                   "değildir; iki kenarın buluştuğu bir noktayı gösterin.");
    if (size <= 0)
        return err(ErrorCode::InvalidArgument,
                   std::string(fillet ? "Yarıçap" : "Mesafe") + " sıfırdan büyük olmalı.");

    const std::size_t prev = at == 0 ? run.size() - 1 : at - 1;
    const std::size_t next = at + 1 == run.size() ? 0 : at + 1;

    const Point2 v  = run[at];
    const Unit d1   = unit_from(v, run[prev]);
    const Unit d2   = unit_from(v, run[next]);
    const double co = d1.x * d2.x + d1.y * d2.y;
    const double cr = d1.x * d2.y - d1.y * d2.x;
    const double si = std::abs(cr);

    // Collinear edges have no corner to cut, and a doubled-back edge has no
    // inside. Both are refused rather than divided by zero.
    if (si < 1e-12)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.");

    const Mm edge1 = length_of(v, run[prev]);
    const Mm edge2 = length_of(v, run[next]);

    // HOW FAR ALONG EACH EDGE the cut lands. For a chamfer it is the distance
    // given; for a fillet it is r / tan(theta/2), written with the half-angle
    // identity tan(t/2) = sin t / (1 + cos t) so nothing but +, *, / and sqrt is
    // used.
    Mm tangent = size;
    if (fillet) tangent = mm_round(static_cast<double>(size) * (1.0 + co) / si);

    if (tangent <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşe için hesaplanan kesim sıfır ya da negatif çıkıyor.");
    if (tangent >= edge1 || tangent >= edge2)
        return err(ErrorCode::InvalidArgument,
                   "Kesim komşu kenardan uzun: kenarlar " + metres(edge1) + " ve " + metres(edge2) +
                       ", gereken " + metres(tangent) + ". Daha küçük bir değer verin.");

    CornerCut cut;
    cut.cut_a = along(v, d1, tangent);
    cut.cut_b = along(v, d2, tangent);

    if (!fillet) {
        // A CHAMFER stays one object: the corner vertex is replaced by its two
        // tangent points and the straight edge between them IS the chamfer. The
        // ring's own order decides which comes first — `cut_a` lies toward the
        // PREVIOUS vertex.
        cut.kept.reserve(run.size() + 1);
        for (std::size_t i = 0; i < run.size(); ++i) {
            if (i != at) {
                cut.kept.push_back(run[i]);
                continue;
            }
            cut.kept.push_back(cut.cut_a);
            cut.kept.push_back(cut.cut_b);
        }
        return cut;
    }

    // The arc's centre sits on the bisector, at r / sin(theta/2) from the vertex.
    // The half-angle sine comes from cos theta by identity, so there is still no
    // trigonometric call anywhere here.
    const double half_sin = std::sqrt((1.0 - co) * 0.5);
    if (half_sin <= 0.0)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşe yuvarlatılamıyor: kenarlar üst üste geliyor.");

    const Unit bis = [&] {
        const double bx  = d1.x + d2.x;
        const double by  = d1.y + d2.y;
        const double len = std::sqrt(bx * bx + by * by);
        return len > 0.0 ? Unit{bx / len, by / len} : Unit{};
    }();

    cut.radius = size;
    cut.centre = along(v, bis, mm_round(static_cast<double>(size) / half_sin));
    // THE SWEEP IS COUNTER-CLOCKWISE from the first end to the second
    // (core/arc.hpp), so which tangent point starts the arc depends on which way
    // the corner turns.
    cut.start = cr > 0.0 ? cut.cut_b : cut.cut_a;
    cut.end   = cr > 0.0 ? cut.cut_a : cut.cut_b;

    if (closed) {
        // A CLOSED RING IS ROUNDED IN PLACE. It cannot be broken in two the way
        // an open line is — a parcel turned into two lines and an arc encloses
        // nothing — and a ring here is made of corners (model.md R9-R12), so
        // the arc is drawn INTO it: the two tangent points and, between them,
        // the points `arc_outline` draws the same arc with, the routine a YAY is
        // drawn by. The object stays the object it was — its key, its
        // attributes, whatever is attached to it — and stays a face, which is
        // what an ifraz, an area and a buffer need. How far the drawn corner
        // strays from the true arc is measured and handed back (`deviation`),
        // so the command can say it rather than hide it.
        std::vector<Mm> xs;
        std::vector<Mm> ys;
        arc_outline(cut.centre, cut.radius, cut.start, cut.end, xs, ys);
        std::vector<Point2> bend;
        bend.reserve(xs.size());
        for (std::size_t i = 0; i < xs.size(); ++i)
            bend.push_back(Point2{xs[i], ys[i]});
        // In the RING'S order, from the tangent point toward the previous vertex
        // to the one toward the next: the arc is counter-clockwise, the ring may
        // not be.
        if (cr > 0.0) std::ranges::reverse(bend);
        if (bend.size() < 2) return err(ErrorCode::InvalidArgument, "Bu köşenin yayı çizilemiyor.");
        bend.front() = cut.cut_a; // the ends ARE the tangent points, exactly
        bend.back()  = cut.cut_b;

        cut.kept.reserve(run.size() + bend.size());
        for (std::size_t i = 0; i < run.size(); ++i) {
            if (i == at)
                cut.kept.insert(cut.kept.end(), bend.begin(), bend.end());
            else
                cut.kept.push_back(run[i]);
        }

        // THE WORST CHORD: the midpoint of each drawn edge against the circle.
        for (std::size_t i = 0; i + 1 < bend.size(); ++i) {
            const double mx = (mm_to_metres(bend[i].x) + mm_to_metres(bend[i + 1].x)) * 0.5 -
                              mm_to_metres(cut.centre.x);
            const double my = (mm_to_metres(bend[i].y) + mm_to_metres(bend[i + 1].y)) * 0.5 -
                              mm_to_metres(cut.centre.y);
            const Mm inside = cut.radius - mm_round(std::sqrt(mx * mx + my * my) *
                                                    static_cast<double>(kMmPerMetre));
            cut.deviation   = std::max(cut.deviation, inside);
        }
        cut.rounded = true;
        cut.edges   = bend.size() - 1;
        return cut;
    }

    // AN OPEN LINE BREAKS IN TWO, and it has to: both tangent points in one run
    // would draw a straight chord between them AND the arc over it — a lens
    // where a rounded corner should be. The arc is its own object between them.
    cut.kept.assign(run.begin(), run.begin() + static_cast<std::ptrdiff_t>(at));
    cut.kept.push_back(cut.cut_a);
    cut.second.push_back(cut.cut_b);
    cut.second.insert(cut.second.end(), run.begin() + static_cast<std::ptrdiff_t>(at) + 1,
                      run.end());
    if (cut.kept.size() < 2 || cut.second.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşe yuvarlatılınca kenarlardan biri tek noktaya iniyor.");
    cut.arc = true;
    return cut;
}

std::vector<std::uint8_t> encode_corner_preview(const CornerPreview& preview)
{
    // version, fillet, vertex, key — little-endian as the machine writes it,
    // because the bytes never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(2 + sizeof(preview.at) + sizeof(preview.key));
    bytes[0] = 1;
    bytes[1] = preview.fillet ? 1 : 0;
    std::memcpy(bytes.data() + 2, &preview.at, sizeof(preview.at));
    std::memcpy(bytes.data() + 2 + sizeof(preview.at), &preview.key, sizeof(preview.key));
    return bytes;
}

Result<CornerPreview> decode_corner_preview(std::span<const std::uint8_t> bytes)
{
    CornerPreview preview;
    if (bytes.size() != 2 + sizeof(preview.at) + sizeof(preview.key) || bytes[0] != 1 ||
        bytes[1] > 1)
        return err(ErrorCode::InvalidArgument, "Köşe önizlemesinin baytları tanınmıyor.");
    preview.fillet = bytes[1] == 1;
    std::memcpy(&preview.at, bytes.data() + 2, sizeof(preview.at));
    std::memcpy(&preview.key, bytes.data() + 2 + sizeof(preview.at), sizeof(preview.key));
    return preview;
}

} // namespace kentos::core
