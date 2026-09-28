// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: cutting a corner off a run. See corner.hpp.
#include "kentos_cad/core/corner.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/fillet.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace kentos::core {
namespace {

struct Dir
{
    double x{0.0};
    double y{0.0};
};

Dir unit_from(Point2 from, Point2 to)
{
    // Metres first: the square of a TM3 coordinate difference in millimetres
    // leaves the 53-bit mantissa long before it leaves int64 (core.md R3).
    const double dx  = mm_to_metres(to.x - from.x);
    const double dy  = mm_to_metres(to.y - from.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) return Dir{};
    return Dir{dx / len, dy / len};
}

Mm length_of(Point2 a, Point2 b)
{
    const double dx = mm_to_metres(b.x - a.x);
    const double dy = mm_to_metres(b.y - a.y);
    return mm_round(std::sqrt(dx * dx + dy * dy) * static_cast<double>(kMmPerMetre));
}

Point2 along(Point2 v, Dir d, Mm distance)
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

namespace {

/// Closer than this, a tangent point IS the neighbouring vertex and the edge
/// between them is used up: the rounding of two tangent points computed from
/// two different corners.
constexpr Mm kUsedUp = 1;

/// A corner smoother than this is no corner: a piece that runs on into the next
/// within a degree, as a straight edge runs into the arc a fillet left.
constexpr std::int64_t kSmoothUdeg = 1'000'000;

/// How one corner is cut: where the cut meets each of its two pieces, the piece
/// that joins them, and how much of each piece it takes, measured along the
/// piece from the corner.
struct CornerPlan
{
    Point2 a{};      ///< on the incoming piece
    Point2 b{};      ///< on the outgoing piece
    PathPiece link;  ///< from `a` to `b`: the fillet arc or the chamfer edge
    Mm take_in{0};   ///< taken off the incoming piece's end
    Mm take_out{0};  ///< taken off the outgoing piece's start
    Point2 centre{}; ///< a fillet's centre
    Mm radius{0};    ///< a fillet's radius
};

bool is_arc(const PathPiece& p) noexcept
{
    return p.kind == PathPiece::Kind::Arc;
}

/// A piece's length in millimetres: a segment's, an arc's along the arc.
Mm piece_length(const PathPiece& p)
{
    if (!is_arc(p)) return length_of(p.from, p.to);
    CurvePath one;
    one.pieces.push_back(p);
    return path_length(one);
}

/// The part of `p` from `from` to `to`, both on it: a segment's own, an arc
/// walked the way `p` is.
PathPiece part_of(const PathPiece& p, Point2 from, Point2 to)
{
    if (from == p.from && to == p.to) return p;
    if (!is_arc(p)) return PathPiece{.from = from, .to = to};
    return arc_piece(p.centre, p.radius, from, to, p.sweep_udeg >= 0);
}

/// The same piece started at `from` instead — a millimetre off, where a used-up
/// edge left two tangent points that should have been one.
PathPiece started_at(const PathPiece& p, Point2 from)
{
    if (!is_arc(p)) return PathPiece{.from = from, .to = p.to};
    return arc_piece(p.centre, p.radius, from, p.to, p.sweep_udeg >= 0);
}

/// THE CORNER BETWEEN TWO STRAIGHT PIECES, by the half-angle construction:
/// the tangent points `r / tan(θ/2)` back along each edge, the centre on the
/// bisector at `r / sin(θ/2)`, and nothing but +, *, / and sqrt.
Result<CornerPlan> straight_corner(const PathPiece& in, const PathPiece& out, Mm size, bool fillet)
{
    const Point2 v  = in.to;
    const Dir d1    = unit_from(v, in.from);
    const Dir d2    = unit_from(v, out.to);
    const double co = d1.x * d2.x + d1.y * d2.y;
    const double cr = d1.x * d2.y - d1.y * d2.x;
    const double si = std::abs(cr);

    // Collinear edges have no corner to cut, and a doubled-back edge has no
    // inside. Both are refused rather than divided by zero.
    if (si < 1e-12)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.");

    // HOW FAR ALONG EACH EDGE the cut lands. For a chamfer it is the distance
    // given; for a fillet it is r / tan(theta/2), written with the half-angle
    // identity tan(t/2) = sin t / (1 + cos t) so nothing but +, *, / and sqrt is
    // used.
    Mm tangent = size;
    if (fillet) tangent = mm_round(static_cast<double>(size) * (1.0 + co) / si);
    if (tangent <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşe için hesaplanan kesim sıfır ya da negatif çıkıyor.");

    CornerPlan plan;
    plan.a        = along(v, d1, tangent);
    plan.b        = along(v, d2, tangent);
    plan.take_in  = tangent;
    plan.take_out = tangent;
    if (!fillet) {
        plan.link = PathPiece{.from = plan.a, .to = plan.b};
        return plan;
    }

    // The arc's centre sits on the bisector, at r / sin(theta/2) from the vertex.
    // The half-angle sine comes from cos theta by identity, so there is still no
    // trigonometric call anywhere here.
    const double half_sin = std::sqrt((1.0 - co) * 0.5);
    if (half_sin <= 0.0)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşe yuvarlatılamıyor: kenarlar üst üste geliyor.");
    const Dir bis = [&] {
        const double bx  = d1.x + d2.x;
        const double by  = d1.y + d2.y;
        const double len = std::sqrt(bx * bx + by * by);
        return len > 0.0 ? Dir{bx / len, by / len} : Dir{};
    }();
    plan.radius = size;
    plan.centre = along(v, bis, mm_round(static_cast<double>(size) / half_sin));
    // A path turning LEFT at the corner walks its fillet counter-clockwise:
    // the edge back to the previous vertex crossed with the edge on is then
    // negative.
    plan.link = arc_piece(plan.centre, size, plan.a, plan.b, cr < 0.0);
    return plan;
}

/// THE CORNER WHERE A PIECE IS AN ARC: the circle of radius `size` tangent to
/// both, in the corner the two pieces make — `fillet_pair`'s construction, each
/// piece picked at its middle so the part toward its far end is the part kept.
Result<CornerPlan> curved_corner(const PathPiece& in, const PathPiece& out, Mm size, bool fillet)
{
    if (!fillet)
        return err(ErrorCode::InvalidArgument,
                   "PAH iki düz kenarın buluştuğu köşeyi keser; bu köşenin bir kenarı yay. "
                   "Köşeyi YUVARLA ile yuvarlatın.");

    // A SMOOTH JOIN IS NO CORNER: the straight edge a fillet ran into its arc,
    // an arc polyline's tangent bend.
    CurvePath a;
    a.pieces.push_back(in);
    CurvePath b;
    b.pieces.push_back(out);
    const std::int64_t turn =
        direction_at(b, PathPlace{0, 0.0}) - direction_at(a, PathPlace{0, 1.0});
    std::int64_t wrapped = turn % kUDegFullCircle;
    if (wrapped > kUDegFullCircle / 2) wrapped -= kUDegFullCircle;
    if (wrapped < -kUDegFullCircle / 2) wrapped += kUDegFullCircle;
    if (wrapped > -kSmoothUdeg && wrapped < kSmoothUdeg)
        return err(ErrorCode::InvalidArgument,
                   "Bu köşede kenarlar birbirine teğet uzanıyor; yuvarlatılacak bir köşe yok.");

    auto made =
        fillet_pair(a, point_at(a, PathPlace{0, 0.5}), b, point_at(b, PathPlace{0, 0.5}), size);
    if (!made) return made.error();
    const PairCorner& pc = made.value();
    // The two pieces, each cut back from the corner and nothing else: a result
    // that kept another part — the corner across the circle from this one — is
    // not this corner's fillet.
    if (!pc.has_link || pc.a.pieces.size() != 1 || pc.b.pieces.size() != 1 ||
        pc.a.pieces.front().from != in.from || pc.b.pieces.front().to != out.to)
        return err(ErrorCode::InvalidArgument,
                   "Bu yarıçapta, köşenin iki kenarına da teğet bir yay bu köşeye sığmıyor. "
                   "Daha küçük bir yarıçap verin.");
    CornerPlan plan;
    plan.a        = pc.on_a;
    plan.b        = pc.on_b;
    plan.link     = pc.link;
    plan.centre   = pc.centre;
    plan.radius   = size;
    plan.take_in  = piece_length(in) - piece_length(pc.a.pieces.front());
    plan.take_out = piece_length(out) - piece_length(pc.b.pieces.front());
    // A piece carried ON past the corner is the fillet of the corner across
    // from this one.
    if (plan.take_in < 0 || plan.take_out < 0)
        return err(ErrorCode::InvalidArgument,
                   "Bu yarıçapta, köşenin iki kenarına da teğet bir yay bu köşeye sığmıyor. "
                   "Daha küçük bir yarıçap verin.");
    return plan;
}

Result<CornerPlan> plan_corner(const PathPiece& in, const PathPiece& out, Mm size, bool fillet)
{
    if (in.kind == PathPiece::Kind::Segment && out.kind == PathPiece::Kind::Segment)
        return straight_corner(in, out, size, fillet);
    if ((in.kind == PathPiece::Kind::Segment || is_arc(in)) &&
        (out.kind == PathPiece::Kind::Segment || is_arc(out)))
        return curved_corner(in, out, size, fillet);
    return err(ErrorCode::Unsupported,
               "Köşe işlemleri doğru ve yay kenarlarında çalışır; bu köşede elips ya da spline "
               "var.");
}

/// Whether vertex `at` of a path of `n` pieces is a corner: an open path's ends
/// are not.
bool is_corner(const CurvePath& path, std::size_t at) noexcept
{
    const std::size_t n = path.pieces.size();
    return path.closed ? n >= 2 && at < n : at >= 1 && at < n;
}

/// The piece that ends at vertex `at`, and the one that starts there.
std::size_t piece_in(const CurvePath& path, std::size_t at) noexcept
{
    const std::size_t n = path.pieces.size();
    return at == 0 ? n - 1 : at - 1;
}

/// A cut corner, by its vertex.
struct Planned
{
    std::size_t vertex{0};
    CornerPlan plan;
};

/// THE PATH WITH `cuts` MADE, the one assembly both the single corner and every
/// corner use: each piece from where the corner before it cut it to where the
/// corner after it does, the joining piece between, and a piece used up left
/// out — its neighbours meeting where it was. A closed path keeps starting on
/// its first piece: the link of a corner cut at vertex 0 goes last.
CurvePath assemble(const CurvePath& path, std::span<const Planned> cuts)
{
    const std::size_t n = path.pieces.size();
    std::vector<const CornerPlan*> at(n + 1, nullptr);
    for (const Planned& c : cuts)
        at[c.vertex] = &c.plan;

    CurvePath out;
    out.closed      = path.closed;
    const auto push = [&out](const PathPiece& p) {
        // A USED-UP EDGE leaves the two tangent points a millimetre apart at
        // most; the next piece starts where the last one ended.
        if (!out.pieces.empty() && out.pieces.back().to != p.from)
            out.pieces.push_back(started_at(p, out.pieces.back().to));
        else
            out.pieces.push_back(p);
    };
    for (std::size_t j = 0; j < n; ++j) {
        const PathPiece& piece  = path.pieces[j];
        const CornerPlan* start = at[j];
        const CornerPlan* end   = path.closed ? at[(j + 1) % n] : at[j + 1];
        if (start != nullptr && (!path.closed || j != 0)) push(start->link);
        if (start == nullptr && end == nullptr) {
            push(piece);
            continue;
        }
        // WHAT IS LEFT OF IT, measured along it: an arc's two ends can be close
        // with most of the arc still between them.
        const Mm left = piece_length(piece) - (start != nullptr ? start->take_out : 0) -
                        (end != nullptr ? end->take_in : 0);
        if (left > kUsedUp)
            push(part_of(piece, start != nullptr ? start->b : piece.from,
                         end != nullptr ? end->a : piece.to));
    }
    if (path.closed && at[0] != nullptr) push(at[0]->link);
    // THE SEAM: the last piece ends where the first begins.
    if (out.closed && !out.pieces.empty() && out.pieces.back().to != out.pieces.front().from)
        out.pieces.back() =
            part_of(out.pieces.back(), out.pieces.back().from, out.pieces.front().from);
    return out;
}

} // namespace

Result<PathCorners> cut_path_corner(const CurvePath& path, std::size_t at, Mm size, bool fillet)
{
    if (!is_corner(path, at))
        return err(ErrorCode::InvalidArgument,
                   "Burada iki kenarın buluştuğu bir köşe yok. Açık bir çizginin uçları köşe "
                   "değildir; iki kenarın buluştuğu bir noktayı gösterin.");
    if (size <= 0)
        return err(ErrorCode::InvalidArgument,
                   std::string(fillet ? "Yarıçap" : "Mesafe") + " sıfırdan büyük olmalı.");

    const PathPiece& in  = path.pieces[piece_in(path, at)];
    const PathPiece& out = path.pieces[at];
    auto plan            = plan_corner(in, out, size, fillet);
    if (!plan) return plan.error();

    const Mm edge_in  = piece_length(in);
    const Mm edge_out = piece_length(out);
    if (plan.value().take_in > edge_in + kUsedUp || plan.value().take_out > edge_out + kUsedUp)
        return err(ErrorCode::InvalidArgument,
                   "Kesim komşu kenardan uzun: kenarlar " + metres(edge_in) + " ve " +
                       metres(edge_out) + ", gereken " +
                       metres(std::max(plan.value().take_in, plan.value().take_out)) +
                       ". Daha küçük bir değer verin.");

    PathCorners made;
    const std::array<Planned, 1> one{Planned{.vertex = at, .plan = plan.value()}};
    made.path   = assemble(path, one);
    made.cut    = 1;
    made.cut_a  = plan.value().a;
    made.cut_b  = plan.value().b;
    made.centre = plan.value().centre;
    made.radius = plan.value().radius;
    return made;
}

PathCorners cut_every_path_corner(const CurvePath& path, Mm size, bool fillet)
{
    PathCorners out;
    out.path            = path;
    const std::size_t n = path.pieces.size();
    if (size <= 0 || n < 2) return out;

    // HOW MUCH OF EACH PIECE THE CORNERS AT ITS TWO ENDS HAVE TAKEN, so a corner
    // is judged against its edges as the corners before it left them.
    std::vector<Mm> length(n);
    for (std::size_t j = 0; j < n; ++j)
        length[j] = piece_length(path.pieces[j]);
    std::vector<Mm> from_start(n, 0);
    std::vector<Mm> from_end(n, 0);

    std::vector<Planned> cuts;
    for (std::size_t v = path.closed ? 0 : 1; v < n; ++v) {
        const std::size_t in = piece_in(path, v);
        const std::size_t on = v;
        auto plan            = plan_corner(path.pieces[in], path.pieces[on], size, fillet);
        const bool fits = plan && plan.value().take_in + from_start[in] <= length[in] + kUsedUp &&
                          plan.value().take_out + from_end[on] <= length[on] + kUsedUp;
        if (!fits) {
            ++out.skipped;
            continue;
        }
        from_end[in]   = plan.value().take_in;
        from_start[on] = plan.value().take_out;
        cuts.push_back(Planned{.vertex = v, .plan = std::move(plan.value())});
    }
    out.cut = cuts.size();
    if (!cuts.empty()) out.path = assemble(path, cuts);
    return out;
}

std::vector<std::uint8_t> encode_corner_preview(const CornerPreview& preview)
{
    // version, fillet, vertex, key, and then — only when there are more
    // objects — their count and keys; little-endian as the machine writes it,
    // because the bytes never leave the process (a prompt to the canvas).
    constexpr std::size_t fixed = 2 + sizeof(std::uint32_t) + sizeof(std::int64_t);
    const auto more             = static_cast<std::uint32_t>(preview.also.size());
    std::vector<std::uint8_t> bytes(fixed +
                                    (more == 0 ? 0 : sizeof(more) + more * sizeof(std::int64_t)));
    bytes[0] = 1;
    bytes[1] = static_cast<std::uint8_t>((preview.fillet ? 1U : 0U) | (preview.every ? 2U : 0U));
    std::memcpy(bytes.data() + 2, &preview.at, sizeof(preview.at));
    std::memcpy(bytes.data() + 2 + sizeof(preview.at), &preview.key, sizeof(preview.key));
    if (more != 0) {
        std::memcpy(bytes.data() + fixed, &more, sizeof(more));
        std::memcpy(bytes.data() + fixed + sizeof(more), preview.also.data(),
                    more * sizeof(std::int64_t));
    }
    return bytes;
}

Result<CornerPreview> decode_corner_preview(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t fixed = 2 + sizeof(std::uint32_t) + sizeof(std::int64_t);
    CornerPreview preview;
    std::uint32_t more = 0;
    if (bytes.size() > fixed + sizeof(more)) std::memcpy(&more, bytes.data() + fixed, sizeof(more));
    const std::size_t want =
        fixed +
        (more == 0 ? 0 : sizeof(more) + static_cast<std::size_t>(more) * sizeof(std::int64_t));
    if (bytes.size() < fixed || bytes.size() != want || bytes[0] != 1 || bytes[1] > 3)
        return err(ErrorCode::InvalidArgument, "Köşe önizlemesinin baytları tanınmıyor.");
    preview.fillet = (bytes[1] & 1U) != 0;
    preview.every  = (bytes[1] & 2U) != 0;
    std::memcpy(&preview.at, bytes.data() + 2, sizeof(preview.at));
    std::memcpy(&preview.key, bytes.data() + 2 + sizeof(preview.at), sizeof(preview.key));
    preview.also.resize(more);
    if (more != 0)
        std::memcpy(preview.also.data(), bytes.data() + fixed + sizeof(more),
                    static_cast<std::size_t>(more) * sizeof(std::int64_t));
    return preview;
}

} // namespace kentos::core
