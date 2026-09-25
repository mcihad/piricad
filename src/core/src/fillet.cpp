// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the corner between two objects. See fillet.hpp.
#include "kentos_cad/core/fillet.hpp"
#include "kentos_cad/core/precision.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

namespace kentos::core {
namespace {

constexpr std::int64_t kTurn = kUDegFullCircle;

/// A fraction this close to a piece's end is the end.
constexpr double kEnd = 1e-9;

/// Half a millimetre: closer than this a point is on a line or a circle.
constexpr double kOn = kOnCurveMm;

std::int64_t wrap(std::int64_t a) noexcept
{
    a %= kTurn;
    return a < 0 ? a + kTurn : a;
}

std::int64_t angle_of(Point2 centre, Point2 p) noexcept
{
    return atan2_udeg(p.y - centre.y, p.x - centre.x);
}

/// The distance between two points in millimetres, unrounded — metres before
/// squaring, as everywhere in this core (core.md R3).
double gap_mm(Point2 a, Point2 b) noexcept
{
    const double dx = mm_to_metres(b.x - a.x);
    const double dy = mm_to_metres(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy) * static_cast<double>(kMmPerMetre);
}

bool is_arc(const PathPiece& p) noexcept
{
    return p.kind == PathPiece::Kind::Arc;
}

std::int64_t turn_of(const PathPiece& p) noexcept
{
    return p.sweep_udeg < 0 ? -p.sweep_udeg : p.sweep_udeg;
}

/// A piece's length in metres.
double piece_metres(const PathPiece& p)
{
    if (!is_arc(p)) return mm_to_metres(segment_length(p.from, p.to));
    return mm_to_metres(p.radius) * static_cast<double>(turn_of(p)) * (kPi / 180.0 / 1000000.0);
}

/// How far along piece `p` the point `q` of its own line or circle lies: 0 at
/// its start and 1 at its end, and outside [0, 1] past either — the way the
/// piece is walked, nearer end first for an arc point off the sweep.
double fraction(const PathPiece& p, Point2 q)
{
    if (!is_arc(p)) {
        Point2 foot{};
        double t = 0.0;
        if (!closest_point_on_line(p.from, p.to, q, foot, t)) return 0.0;
        return t;
    }
    const std::int64_t s = turn_of(p);
    if (s <= 0) return 0.0;
    const std::int64_t delta = angle_of(p.centre, q) - angle_of(p.centre, p.from);
    const std::int64_t d     = wrap(p.sweep_udeg < 0 ? -delta : delta);
    if (s >= kTurn || d <= s) return static_cast<double>(d) / static_cast<double>(s);
    const std::int64_t beyond = d - s;
    const std::int64_t before = kTurn - d;
    return beyond <= before ? 1.0 + static_cast<double>(beyond) / static_cast<double>(s)
                            : -static_cast<double>(before) / static_cast<double>(s);
}

/// Metres from the start of `path` to where the point `q` of piece `i`'s own
/// line or circle lies, past the piece's ends when it is off it.
double along(const CurvePath& path, std::size_t i, Point2 q)
{
    double before = 0.0;
    for (std::size_t k = 0; k < i; ++k)
        before += piece_metres(path.pieces[k]);
    return before + fraction(path.pieces[i], q) * piece_metres(path.pieces[i]);
}

/// +1 or −1 for a distance past `kOn` either way, 0 within it.
int sign_beyond(double mm)
{
    if (mm > kOn) return 1;
    if (mm < -kOn) return -1;
    return 0;
}

/// Which side of piece `p`'s line or circle `q` is on: +1, −1, or 0 on it.
int side_of(const PathPiece& p, Point2 q)
{
    if (!is_arc(p)) {
        const double ex  = mm_to_metres(p.to.x - p.from.x);
        const double ey  = mm_to_metres(p.to.y - p.from.y);
        const double qx  = mm_to_metres(q.x - p.from.x);
        const double qy  = mm_to_metres(q.y - p.from.y);
        const double len = std::sqrt(ex * ex + ey * ey);
        if (len <= 0.0) return 0;
        return sign_beyond((ex * qy - ey * qx) / len * 1000.0); // millimetres off the line
    }
    return sign_beyond(gap_mm(p.centre, q) - static_cast<double>(p.radius));
}

/// Where a circle of radius `r` centred at `c` touches piece `p`'s line or
/// circle: the foot of `c` on a line; on a circle the point toward `c` — or
/// away from it, when the fillet circle holds the other inside it.
Point2 touch_on(const PathPiece& p, Point2 c, Mm r)
{
    if (!is_arc(p)) {
        Point2 foot{};
        double t = 0.0;
        if (!closest_point_on_line(p.from, p.to, c, foot, t)) return p.from;
        return foot;
    }
    const double dx = mm_to_metres(c.x - p.centre.x);
    const double dy = mm_to_metres(c.y - p.centre.y);
    const double d  = std::sqrt(dx * dx + dy * dy);
    if (d <= 0.0) return Point2{p.centre.x + p.radius, p.centre.y};
    const double big_r = mm_to_metres(p.radius);
    const double small = mm_to_metres(r);
    const bool holds =
        small > big_r && std::abs(d - (small - big_r)) < std::abs(d - (small + big_r));
    const double k = (holds ? -big_r : big_r) / d;
    return Point2{p.centre.x + mm_round(dx * k * static_cast<double>(kMmPerMetre)),
                  p.centre.y + mm_round(dy * k * static_cast<double>(kMmPerMetre))};
}

/// A place a fillet's centre can be: a line (through `a` and `b`) or a circle.
struct Locus
{
    bool line{true};
    Point2 a{};
    Point2 b{};
    Point2 centre{};
    Mm radius{0};
};

/// Where a centre at `r` from piece `p` can be: the line's two parallels, or
/// the circles R + r and |R − r|; with `r == 0`, the line or circle itself.
std::vector<Locus> loci_of(const PathPiece& p, Mm r)
{
    if (!is_arc(p)) {
        if (r == 0) return {Locus{.line = true, .a = p.from, .b = p.to}};
        const double ex  = mm_to_metres(p.to.x - p.from.x);
        const double ey  = mm_to_metres(p.to.y - p.from.y);
        const double len = std::sqrt(ex * ex + ey * ey);
        if (len <= 0.0) return {};
        const double m = mm_to_metres(r) / len * static_cast<double>(kMmPerMetre);
        const Point2 up{mm_round(-ey * m), mm_round(ex * m)};
        return {Locus{.line = true, .a = p.from + up, .b = p.to + up},
                Locus{.line = true, .a = p.from - up, .b = p.to - up}};
    }
    if (r == 0) return {Locus{.line = false, .centre = p.centre, .radius = p.radius}};
    std::vector<Locus> out{Locus{.line = false, .centre = p.centre, .radius = p.radius + r}};
    if (p.radius != r)
        out.push_back(Locus{.line   = false,
                            .centre = p.centre,
                            .radius = p.radius > r ? p.radius - r : r - p.radius});
    return out;
}

/// Every point two loci share.
std::vector<Point2> meets_of(const Locus& u, const Locus& v)
{
    std::vector<Point2> out;
    if (u.line && v.line) {
        Point2 at{};
        double t = 0.0;
        double s = 0.0;
        if (line_intersection(u.a, u.b, v.a, v.b, at, t, s)) out.push_back(at);
        return out;
    }
    if (u.line != v.line) {
        const Locus& line   = u.line ? u : v;
        const Locus& circle = u.line ? v : u;
        const Point2 seam{circle.centre.x + circle.radius, circle.centre.y};
        const PathPiece whole{.kind       = PathPiece::Kind::Arc,
                              .from       = seam,
                              .to         = seam,
                              .centre     = circle.centre,
                              .radius     = circle.radius,
                              .sweep_udeg = kTurn};
        for (const LineMeet& m : line_meets(line.a, line.b, whole))
            out.push_back(m.point);
        return out;
    }
    Point2 left{};
    Point2 right{};
    const CircleMeet m = circle_intersection(u.centre, u.radius, v.centre, v.radius, left, right);
    if (m == CircleMeet::Two) {
        out.push_back(left);
        out.push_back(right);
    } else if (m == CircleMeet::Tangent) {
        out.push_back(left);
    }
    return out;
}

/// The part of `path` kept when piece `i` is cut or carried on to `target`, a
/// point of its own line or circle: the start side up to `target`, or the end
/// side from it. Only an END piece is carried past its end — a piece in the
/// middle of a polyline has neighbours there.
Result<CurvePath> trim_toward(const CurvePath& path, std::size_t i, Point2 target, bool keep_start)
{
    const PathPiece& p = path.pieces[i];
    const double f     = fraction(p, target);
    const bool last    = i + 1 == path.pieces.size();
    if ((keep_start && f > 1.0 + kEnd && !last) || (!keep_start && f < -kEnd && i != 0))
        return err(ErrorCode::InvalidArgument,
                   "Köşe, çoklu çizginin ortadaki bir kenarının uzantısına düşüyor; ortadaki kenar "
                   "uzatılamaz. Çizginin ucundaki kenarı seçin.");

    const auto part = [&p](Point2 from, Point2 to) {
        return is_arc(p) ? arc_piece(p.centre, p.radius, from, to, p.sweep_udeg >= 0)
                         : PathPiece{.from = from, .to = to};
    };
    CurvePath out;
    if (keep_start) {
        if (f <= kEnd && i == 0)
            return err(
                ErrorCode::InvalidArgument,
                "Köşe sığmıyor: seçtiğiniz parçanın tamamını götürüyor. Daha küçük bir değer "
                "verin.");
        out.pieces.assign(path.pieces.begin(),
                          path.pieces.begin() + static_cast<std::ptrdiff_t>(i));
        if (f > kEnd)
            out.pieces.push_back(part(p.from, target));
        else if (!out.pieces.empty())
            out.pieces.back() = part(out.pieces.back().from, target);
    } else {
        if (f >= 1.0 - kEnd && last)
            return err(
                ErrorCode::InvalidArgument,
                "Köşe sığmıyor: seçtiğiniz parçanın tamamını götürüyor. Daha küçük bir değer "
                "verin.");
        if (f < 1.0 - kEnd) out.pieces.push_back(part(target, p.to));
        out.pieces.insert(out.pieces.end(),
                          path.pieces.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                          path.pieces.end());
        if (f >= 1.0 - kEnd && !out.pieces.empty())
            out.pieces.front() = part(target, out.pieces.front().to);
    }
    if (out.pieces.empty())
        return err(ErrorCode::InvalidArgument,
                   "Köşe sığmıyor: seçtiğiniz parçanın tamamını götürüyor. Daha küçük bir değer "
                   "verin.");
    return out;
}

/// The piece of `path` nearest `pick`, and how far along the path the pick is.
struct Picked
{
    std::size_t piece{0};
    double at{0.0}; ///< metres from the path's start
};

Picked picked(const CurvePath& path, Point2 pick)
{
    const PathPlace place = place_of(path, pick);
    Picked out;
    out.piece     = place.piece;
    double before = 0.0;
    for (std::size_t k = 0; k < place.piece; ++k)
        before += piece_metres(path.pieces[k]);
    out.at = before + place.t * piece_metres(path.pieces[place.piece]);
    return out;
}

/// Where the two pieces' own lines or circles cross, nearest `near`.
std::optional<Point2> crossing_near(const PathPiece& a, const PathPiece& b, Point2 near)
{
    std::optional<Point2> best;
    double best_d = 0.0;
    for (const Locus& u : loci_of(a, 0))
        for (const Locus& v : loci_of(b, 0))
            for (const Point2 x : meets_of(u, v)) {
                const double d = distance_squared(x, near);
                if (!best || d < best_d) {
                    best   = x;
                    best_d = d;
                }
            }
    return best;
}

/// Cuts or carries `path` to `target` along piece `i`, keeping the part the
/// pick is on as seen from `from` — the corner's crossing, so a tangent point
/// past the pick still keeps the picked side; a closed path — a circle a line
/// is rounded against — stays whole, the way every CAD leaves one.
Result<CurvePath> keep_picked(const CurvePath& path, std::size_t i, Point2 target, double pick_at,
                              Point2 from)
{
    if (path.closed) return path;
    return trim_toward(path, i, target, pick_at < along(path, i, from));
}

} // namespace

Result<PairCorner> fillet_pair(const CurvePath& a, Point2 pick_a, const CurvePath& b, Point2 pick_b,
                               Mm radius)
{
    if (a.pieces.empty() || b.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Yuvarlatılacak iki nesneden biri boş.");
    if (radius < 0) return err(ErrorCode::InvalidArgument, "Yarıçap eksi olamaz.");

    const Picked pa    = picked(a, pick_a);
    const Picked pb    = picked(b, pick_b);
    const PathPiece& u = a.pieces[pa.piece];
    const PathPiece& v = b.pieces[pb.piece];

    PairCorner out;
    out.radius = radius;
    // What the picked side is judged from: the crossing when there is one,
    // else the tangent point itself.
    Point2 reference_a{};
    Point2 reference_b{};

    if (radius == 0) {
        // A SHARP CORNER: the two carried or cut to where they cross, nearest
        // the picks, and nothing between them.
        const Point2 mid{(pick_a.x + pick_b.x) / 2, (pick_a.y + pick_b.y) / 2};
        const auto x = crossing_near(u, v, mid);
        if (!x)
            return err(ErrorCode::InvalidArgument,
                       "İki nesne hiçbir yerde kesişmiyor; keskin köşe kurulamaz. Bir yarıçap "
                       "verin ya da nesneleri kesişecek biçimde çizin.");
        out.on_a    = *x;
        out.on_b    = *x;
        reference_a = *x;
        reference_b = *x;
    } else {
        // EVERY TANGENT CIRCLE OF THIS RADIUS, then the one the picks name.
        const int want_a =
            side_of(u, pick_b); ///< the centre on the second pick's side of the first
        const int want_b = side_of(v, pick_a); ///< and on the first pick's side of the second
        std::optional<Point2> best;
        double best_d = 0.0;
        for (const Locus& lu : loci_of(u, radius))
            for (const Locus& lv : loci_of(v, radius))
                for (const Point2 c : meets_of(lu, lv)) {
                    if (want_a != 0 && side_of(u, c) != want_a) continue;
                    if (want_b != 0 && side_of(v, c) != want_b) continue;
                    const double d = distance_squared(touch_on(u, c, radius), pick_a) +
                                     distance_squared(touch_on(v, c, radius), pick_b);
                    if (!best || d < best_d) {
                        best   = c;
                        best_d = d;
                    }
                }
        if (!best)
            return err(ErrorCode::InvalidArgument,
                       "Bu yarıçapta, seçtiğiniz taraflarda iki nesneye de teğet bir yay yok. "
                       "Yarıçapı değiştirin ya da nesneleri köşeye yakın yerlerinden seçin.");
        out.centre  = *best;
        out.on_a    = touch_on(u, out.centre, radius);
        out.on_b    = touch_on(v, out.centre, radius);
        reference_a = out.on_a;
        reference_b = out.on_b;

        // IT MUST SIT IN THE PICKED CORNER: each tangent point on the pick's
        // side of where the two cross. On the far side the arc would round
        // the corner across from the one named — never taken. A tangent point
        // past the pick is fine, as in every CAD: the part the pick is on
        // stays, cut back to it, and a radius that eats all of that part is
        // refused by the trim below.
        if (const auto x = crossing_near(u, v, out.centre)) {
            reference_a        = *x;
            reference_b        = *x;
            const auto crossed = [](const CurvePath& p, const Picked& at, Point2 on, Point2 x) {
                const double cross = along(p, at.piece, x);
                const double touch = along(p, at.piece, on) - cross;
                return !p.closed && std::abs(touch) > 0.001 && (at.at - cross) * touch < 0.0;
            };
            if (std::abs(pa.at - along(a, pa.piece, *x)) <= 0.001 && !a.closed)
                return err(ErrorCode::InvalidArgument,
                           "Nesneleri köşenin kendisinden değil, kalacak parçalarından seçin.");
            if (std::abs(pb.at - along(b, pb.piece, *x)) <= 0.001 && !b.closed)
                return err(ErrorCode::InvalidArgument,
                           "Nesneleri köşenin kendisinden değil, kalacak parçalarından seçin.");
            if (crossed(a, pa, out.on_a, *x))
                return err(ErrorCode::InvalidArgument,
                           "Yarıçap sığmıyor: birinci nesnede teğet noktası köşenin öbür yanına "
                           "düşüyor. Daha küçük bir yarıçap verin.");
            if (crossed(b, pb, out.on_b, *x))
                return err(ErrorCode::InvalidArgument,
                           "Yarıçap sığmıyor: ikinci nesnede teğet noktası köşenin öbür yanına "
                           "düşüyor. Daha küçük bir yarıçap verin.");
        }
        // THE MINOR ARC between the tangent points: the one inside the corner.
        const double ax = mm_to_metres(out.on_a.x - out.centre.x);
        const double ay = mm_to_metres(out.on_a.y - out.centre.y);
        const double bx = mm_to_metres(out.on_b.x - out.centre.x);
        const double by = mm_to_metres(out.on_b.y - out.centre.y);
        const bool ccw  = ax * by - ay * bx > 0.0;
        out.link        = arc_piece(out.centre, radius, out.on_a, out.on_b, ccw);
        out.has_link    = out.on_a != out.on_b && out.link.sweep_udeg != 0;
    }

    auto kept_a = keep_picked(a, pa.piece, out.on_a, pa.at, reference_a);
    if (!kept_a) return kept_a.error();
    auto kept_b = keep_picked(b, pb.piece, out.on_b, pb.at, reference_b);
    if (!kept_b) return kept_b.error();
    out.a = std::move(kept_a.value());
    out.b = std::move(kept_b.value());
    return out;
}

Result<PairCorner> chamfer_pair(const CurvePath& a, Point2 pick_a, const CurvePath& b,
                                Point2 pick_b, Mm distance_a, Mm distance_b)
{
    if (a.pieces.empty() || b.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Pah kırılacak iki nesneden biri boş.");
    if (distance_a <= 0 || distance_b <= 0)
        return err(ErrorCode::InvalidArgument, "Pah mesafesi sıfırdan büyük olmalı.");
    const Picked pa    = picked(a, pick_a);
    const Picked pb    = picked(b, pick_b);
    const PathPiece& u = a.pieces[pa.piece];
    const PathPiece& v = b.pieces[pb.piece];
    if (is_arc(u) || is_arc(v))
        return err(ErrorCode::Unsupported,
                   "Pah iki düz kenar arasında kırılır; yay ile köşe için YUVARLA kullanın.");

    Point2 x{};
    double t = 0.0;
    double s = 0.0;
    if (!line_intersection(u.from, u.to, v.from, v.to, x, t, s))
        return err(ErrorCode::InvalidArgument, "İki çizgi paralel; aralarında köşe yok.");

    // FROM THE CROSSING, TOWARD THE PICKED PART of each line.
    const auto toward = [&x](const PathPiece& p, Point2 pick, Mm d) -> std::optional<Point2> {
        Point2 foot{};
        double tt = 0.0;
        if (!closest_point_on_line(p.from, p.to, pick, foot, tt)) return std::nullopt;
        const double dx  = mm_to_metres(foot.x - x.x);
        const double dy  = mm_to_metres(foot.y - x.y);
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0) return std::nullopt;
        const double k = mm_to_metres(d) / len * static_cast<double>(kMmPerMetre);
        return Point2{x.x + mm_round(dx * k), x.y + mm_round(dy * k)};
    };
    const auto on_a = toward(u, pick_a, distance_a);
    const auto on_b = toward(v, pick_b, distance_b);
    if (!on_a || !on_b)
        return err(ErrorCode::InvalidArgument,
                   "Nesneleri köşenin kendisinden değil, kalacak parçalarından seçin.");

    PairCorner out;
    out.on_a = *on_a;
    out.on_b = *on_b;
    // The part that stays is the one away from the crossing.
    auto kept_a =
        trim_toward(a, pa.piece, out.on_a, along(a, pa.piece, out.on_a) < along(a, pa.piece, x));
    if (!kept_a) return kept_a.error();
    auto kept_b =
        trim_toward(b, pb.piece, out.on_b, along(b, pb.piece, out.on_b) < along(b, pb.piece, x));
    if (!kept_b) return kept_b.error();
    out.a        = std::move(kept_a.value());
    out.b        = std::move(kept_b.value());
    out.link     = PathPiece{.from = out.on_a, .to = out.on_b};
    out.has_link = out.on_a != out.on_b;
    return out;
}

std::vector<std::uint8_t> encode_pair_corner_guide(const PairCornerGuide& guide)
{
    // version, then the fields as the machine writes them: the bytes never
    // leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(1 + 8 + 8 + 16 + 16 + 2);
    std::uint8_t* at = bytes.data();
    *at++            = 1;
    std::memcpy(at, &guide.key_a, 8);
    at += 8;
    std::memcpy(at, &guide.key_b, 8);
    at += 8;
    std::memcpy(at, &guide.pick_a.x, 8);
    std::memcpy(at + 8, &guide.pick_a.y, 8);
    at += 16;
    std::memcpy(at, &guide.pick_b.x, 8);
    std::memcpy(at + 8, &guide.pick_b.y, 8);
    at += 16;
    *at++ = guide.fillet ? 1 : 0;
    *at   = guide.trim ? 1 : 0;
    return bytes;
}

Result<PairCornerGuide> decode_pair_corner_guide(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 1 + 8 + 8 + 16 + 16 + 2 || bytes[0] != 1 || bytes[49] > 1 || bytes[50] > 1)
        return err(ErrorCode::InvalidArgument, "Köşe önizlemesinin baytları tanınmıyor.");
    PairCornerGuide guide;
    const std::uint8_t* at = bytes.data() + 1;
    std::memcpy(&guide.key_a, at, 8);
    at += 8;
    std::memcpy(&guide.key_b, at, 8);
    at += 8;
    std::memcpy(&guide.pick_a.x, at, 8);
    std::memcpy(&guide.pick_a.y, at + 8, 8);
    at += 16;
    std::memcpy(&guide.pick_b.x, at, 8);
    std::memcpy(&guide.pick_b.y, at + 8, 8);
    at += 16;
    guide.fillet = *at++ == 1;
    guide.trim   = *at == 1;
    return guide;
}

} // namespace kentos::core
