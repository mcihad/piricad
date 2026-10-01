// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core (private): an ellipse or a spline piece as the exact curve
// it is. See curve_eval.hpp.
#include "curve_eval.hpp"
#include "piricad/core/kernel.hpp"

#include "piricad/core/result.hpp"
#include "piricad/core/trig.hpp"
#include "piricad/core/units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace piricad::core::curve {
namespace {

/// Radians in one micro-degree.
constexpr double kUdegToRad = kPi / (180.0 * 1000000.0);

/// Steps a turn an arc or an ellipse is sampled at, and steps a knot span a
/// spline is.
constexpr int kStepsPerTurn = 256;
constexpr int kStepsPerSpan = 32;

Vec rel(Point2 p, Point2 o) noexcept
{
    return Vec{mm_to_metres(p.x - o.x), mm_to_metres(p.y - o.y)};
}

Vec sub(Vec a, Vec b) noexcept
{
    return Vec{a.x - b.x, a.y - b.y};
}

double dot(Vec a, Vec b) noexcept
{
    return (a.x * b.x) + (a.y * b.y);
}

double norm(Vec a) noexcept
{
    return std::sqrt(dot(a, a));
}

Vec lerp(Vec a, Vec b, double f) noexcept
{
    return Vec{a.x + ((b.x - a.x) * f), a.y + ((b.y - a.y) * f)};
}

/// A spline piece's knots, written out: its own, or the uniform clamped knots
/// SPLINE draws with when it has none.
std::vector<std::int64_t> knots_of(const PathPiece& piece)
{
    if (!piece.spline.knots_nano.empty()) return piece.spline.knots_nano;
    return uniform_clamped_knots(piece.controls.size(), piece.spline.degree);
}

/// The Gauss–Legendre rule of ten points on [−1, 1]: the nodes' positive half
/// and their weights, fixed, so a length is summed alike everywhere.
constexpr std::array<double, 5> kGaussNode{0.1488743389816312, 0.4333953941292472,
                                           0.6794095682990244, 0.8650633666889845,
                                           0.9739065285171717};
constexpr std::array<double, 5> kGaussWeight{0.2955242247147529, 0.2692667193099963,
                                             0.2190863625159820, 0.1494513491505806,
                                             0.0666713443086881};

/// The length of `eval` between `a` and `b`: ten nodes over one interval.
double gauss(const Eval& eval, double a, double b)
{
    const double half = (b - a) / 2.0;
    const double mid  = (a + b) / 2.0;
    double sum        = 0.0;
    for (std::size_t i = 0; i < kGaussNode.size(); ++i) {
        const double d = half * kGaussNode[i];
        sum += kGaussWeight[i] * (norm(eval.tangent(mid - d)) + norm(eval.tangent(mid + d)));
    }
    return sum * half;
}

/// The places along `piece` a length is summed between: its knot spans, each
/// in two, for a spline; every sixteenth of a turn for an arc or an ellipse.
std::vector<double> breaks(const PathPiece& piece, double t0, double t1)
{
    std::vector<double> out{t0};
    if (piece.kind == PathPiece::Kind::Spline) {
        const std::vector<std::int64_t> knots = knots_of(piece);
        const std::size_t n                   = piece.controls.size();
        const auto p                          = static_cast<std::size_t>(piece.spline.degree);
        if (knots.size() == n + p + 1) {
            const auto u0 = static_cast<double>(knots[p]);
            const auto u1 = static_cast<double>(knots[n]);
            for (std::size_t k = p + 1; k < n; ++k) {
                if (knots[k] == knots[k - 1]) continue;
                const double t = (static_cast<double>(knots[k]) - u0) / (u1 - u0);
                const double h = (static_cast<double>(knots[k - 1]) - u0) / (u1 - u0);
                const double m = (t + h) / 2.0;
                if (m > t0 && m < t1) out.push_back(m);
                if (t > t0 && t < t1) out.push_back(t);
            }
            const double last = (static_cast<double>(knots[n - 1]) - u0) / (u1 - u0);
            const double tail = (last + 1.0) / 2.0;
            if (tail > t0 && tail < t1) out.push_back(tail);
        }
    } else if (piece.kind != PathPiece::Kind::Segment) {
        const double turns = std::abs(static_cast<double>(piece.sweep_udeg)) /
                             static_cast<double>(kUDegFullCircle) * (t1 - t0);
        const int parts    = std::max(1, static_cast<int>(std::ceil(turns * 16.0)));
        for (int i = 1; i < parts; ++i)
            out.push_back(t0 + ((t1 - t0) * static_cast<double>(i) / static_cast<double>(parts)));
    }
    std::sort(out.begin(), out.end());
    out.push_back(t1);
    return out;
}

/// The golden-section minimum of `f` on [lo, hi]: point evaluations only, a
/// fixed number of steps.
template<class F> double golden_min(F&& f, double lo, double hi)
{
    constexpr double kInv = 0.6180339887498949; // (√5 − 1) / 2
    double x1             = hi - (kInv * (hi - lo));
    double x2             = lo + (kInv * (hi - lo));
    double f1             = f(x1);
    double f2             = f(x2);
    for (int i = 0; i < 90 && hi - lo > 1e-13; ++i) {
        if (f1 <= f2) {
            hi = x2;
            x2 = x1;
            f2 = f1;
            x1 = hi - (kInv * (hi - lo));
            f1 = f(x1);
        } else {
            lo = x1;
            x1 = x2;
            f1 = f2;
            x2 = lo + (kInv * (hi - lo));
            f2 = f(x2);
        }
    }
    return (lo + hi) / 2.0;
}

/// The parameter of the piece `e` evaluates nearest `probe` — a point in
/// `e`'s own frame, unrounded, so a point that lies on the curve is found at
/// distance nothing and not at the half millimetre its rounding would cost.
double nearest_in(const PathPiece& piece, const Eval& e, Vec probe);

// ---- the spline's knot insertion (The NURBS Book, algorithm A5.1) ----------

/// Homogeneous control points, in millimetres about the spline's first
/// control point: (w·x, w·y, w).
struct Homog
{
    std::vector<double> x; ///< w·x
    std::vector<double> y; ///< w·y
    std::vector<double> w; ///< w
};

Homog homog_of(const SplineParts& s)
{
    Homog h;
    const Point2 o = s.controls.front();
    for (std::size_t i = 0; i < s.controls.size(); ++i) {
        const double w = s.def.weights_nano.empty() ? 1.0
                                                    : static_cast<double>(s.def.weights_nano[i]) /
                                                          static_cast<double>(kNano);
        h.x.push_back(static_cast<double>(s.controls[i].x - o.x) * w);
        h.y.push_back(static_cast<double>(s.controls[i].y - o.y) * w);
        h.w.push_back(w);
    }
    return h;
}

/// Inserts knot `u` into `knots` `times` times, `h` changing with it.
void insert_knot(Homog& h, std::vector<std::int64_t>& knots, int degree, std::int64_t u, int times)
{
    if (times <= 0) return;
    const auto p  = static_cast<std::ptrdiff_t>(degree);
    const auto r  = static_cast<std::ptrdiff_t>(times);
    const auto np = static_cast<std::ptrdiff_t>(h.x.size()) - 1;
    const auto mp = static_cast<std::ptrdiff_t>(knots.size()) - 1;
    // The span: the LAST knot at or before `u` — where its copies end, which
    // the algorithm assumes — and never before the first span. For a cut
    // inside the domain that is the span holding `u`; for an unclamped end
    // being clamped it may lie past the last span, where the same local
    // formulas still reach only controls that exist.
    std::ptrdiff_t k = p;
    for (std::ptrdiff_t i = p; i <= mp; ++i)
        if (knots[static_cast<std::size_t>(i)] <= u) k = i;
    k                = std::min(k, mp - 1);
    std::ptrdiff_t s = 0;
    for (const std::int64_t v : knots)
        if (v == u) ++s;

    std::vector<std::int64_t> uq(knots.size() + static_cast<std::size_t>(r));
    for (std::ptrdiff_t i = 0; i <= k; ++i)
        uq[static_cast<std::size_t>(i)] = knots[static_cast<std::size_t>(i)];
    for (std::ptrdiff_t i = 1; i <= r; ++i)
        uq[static_cast<std::size_t>(k + i)] = u;
    for (std::ptrdiff_t i = k + 1; i <= mp; ++i)
        uq[static_cast<std::size_t>(i + r)] = knots[static_cast<std::size_t>(i)];

    Homog q;
    q.x.assign(h.x.size() + static_cast<std::size_t>(r), 0.0);
    q.y.assign(q.x.size(), 0.0);
    q.w.assign(q.x.size(), 0.0);
    const auto put = [&q](std::ptrdiff_t i, double x, double y, double w) {
        q.x[static_cast<std::size_t>(i)] = x;
        q.y[static_cast<std::size_t>(i)] = y;
        q.w[static_cast<std::size_t>(i)] = w;
    };
    for (std::ptrdiff_t i = 0; i <= k - p; ++i)
        put(i, h.x[static_cast<std::size_t>(i)], h.y[static_cast<std::size_t>(i)],
            h.w[static_cast<std::size_t>(i)]);
    for (std::ptrdiff_t i = k - s; i <= np; ++i)
        put(i + r, h.x[static_cast<std::size_t>(i)], h.y[static_cast<std::size_t>(i)],
            h.w[static_cast<std::size_t>(i)]);
    std::vector<double> rx;
    std::vector<double> ry;
    std::vector<double> rw;
    for (std::ptrdiff_t i = 0; i <= p - s; ++i) {
        rx.push_back(h.x[static_cast<std::size_t>(k - p + i)]);
        ry.push_back(h.y[static_cast<std::size_t>(k - p + i)]);
        rw.push_back(h.w[static_cast<std::size_t>(k - p + i)]);
    }
    std::ptrdiff_t first = 0;
    for (std::ptrdiff_t j = 1; j <= r; ++j) {
        first = k - p + j;
        for (std::ptrdiff_t i = 0; i <= p - j - s; ++i) {
            const auto lo      = static_cast<double>(knots[static_cast<std::size_t>(first + i)]);
            const auto hi      = static_cast<double>(knots[static_cast<std::size_t>(i + k + 1)]);
            const double alpha = hi == lo ? 0.0 : (static_cast<double>(u) - lo) / (hi - lo);
            const auto a       = static_cast<std::size_t>(i);
            rx[a]              = (alpha * rx[a + 1]) + ((1.0 - alpha) * rx[a]);
            ry[a]              = (alpha * ry[a + 1]) + ((1.0 - alpha) * ry[a]);
            rw[a]              = (alpha * rw[a + 1]) + ((1.0 - alpha) * rw[a]);
        }
        put(first, rx[0], ry[0], rw[0]);
        const auto last = static_cast<std::size_t>(p - j - s);
        put(k + r - j - s, rx[last], ry[last], rw[last]);
    }
    first = k - p + r;
    for (std::ptrdiff_t i = first + 1; i <= k - s - 1; ++i) {
        const auto a = static_cast<std::size_t>(i - first);
        put(i, rx[a], ry[a], rw[a]);
    }
    h     = std::move(q);
    knots = std::move(uq);
}

/// The spline made of controls [first, last) of `h` over `knots`.
SplineParts parts_of(const Homog& h, std::size_t first, std::size_t last,
                     std::vector<std::int64_t> knots, const SplineDef& base, Point2 origin)
{
    SplineParts out;
    out.def              = base;
    out.def.knots_nano   = std::move(knots);
    out.def.closed       = false;
    out.def.periodic     = false;
    out.def.has_fit      = false;
    const bool rational  = !base.weights_nano.empty();
    out.def.weights_nano = {};
    for (std::size_t i = first; i < last; ++i) {
        const double w = h.w[i] == 0.0 ? 1.0 : h.w[i];
        out.controls.push_back(
            Point2{origin.x + mm_round(h.x[i] / w), origin.y + mm_round(h.y[i] / w)});
        if (rational) out.def.weights_nano.push_back(std::llround(w * static_cast<double>(kNano)));
    }
    return out;
}

/// Inserts `u` until it is a knot `degree` times over; the index of its first
/// occurrence and how many times it now occurs.
std::pair<std::size_t, std::size_t> to_multiplicity(Homog& h, std::vector<std::int64_t>& knots,
                                                    int degree, std::int64_t u)
{
    std::size_t s = 0;
    for (const std::int64_t v : knots)
        if (v == u) ++s;
    if (s < static_cast<std::size_t>(degree))
        insert_knot(h, knots, degree, u, degree - static_cast<int>(s));
    std::size_t first = 0;
    while (first < knots.size() && knots[first] != u)
        ++first;
    std::size_t count = 0;
    while (first + count < knots.size() && knots[first + count] == u)
        ++count;
    return {first, count};
}

} // namespace

// ---- Eval --------------------------------------------------------------------

Eval::Eval(const PathPiece& piece, Point2 origin) : kind_(piece.kind), origin_(origin)
{
    switch (piece.kind) {
    case PathPiece::Kind::Segment:
        a_ = rel(piece.from, origin);
        b_ = rel(piece.to, origin);
        break;
    case PathPiece::Kind::Arc: {
        c_             = rel(piece.centre, origin);
        const double r = mm_to_metres(piece.radius);
        u_             = Vec{r, 0.0};
        v_             = Vec{0.0, r};
        start_ = static_cast<double>(
                     atan2_udeg(piece.from.y - piece.centre.y, piece.from.x - piece.centre.x)) *
                 kUdegToRad;
        sweep_ = static_cast<double>(piece.sweep_udeg) * kUdegToRad;
        break;
    }
    case PathPiece::Kind::Ellipse:
        c_     = rel(piece.centre, origin);
        u_     = rel(piece.major, piece.centre);
        v_     = rel(piece.minor, piece.centre);
        start_ = static_cast<double>(piece.start_udeg) * kUdegToRad;
        sweep_ = static_cast<double>(piece.sweep_udeg) * kUdegToRad;
        break;
    case PathPiece::Kind::Spline: {
        degree_                               = piece.spline.degree;
        const std::vector<std::int64_t> knots = knots_of(piece);
        const std::size_t n                   = piece.controls.size();
        const auto p                          = static_cast<std::size_t>(degree_);
        if (degree_ < 1 || n < p + 1 || knots.size() != n + p + 1) {
            // Not a curve this can walk: a point where its first control is.
            kind_ = PathPiece::Kind::Segment;
            a_    = n == 0 ? Vec{} : rel(piece.controls.front(), origin);
            b_    = a_;
            break;
        }
        for (const std::int64_t k : knots)
            knots_.push_back(static_cast<double>(k) / static_cast<double>(kNano));
        for (std::size_t i = 0; i < n; ++i) {
            const double w =
                piece.spline.weights_nano.size() == n
                    ? static_cast<double>(piece.spline.weights_nano[i]) / static_cast<double>(kNano)
                    : 1.0;
            const Vec at = rel(piece.controls[i], origin);
            hx_.push_back(at.x * w);
            hy_.push_back(at.y * w);
            hw_.push_back(w);
        }
        // THE DERIVATIVE IS A SPLINE TOO, one degree lower, over the knots
        // without their first and last: Q_i = p (H_{i+1} − H_i) / (u_{i+p+1} − u_{i+1}).
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const double den = knots_[i + p + 1] - knots_[i + 1];
            const double f   = den > 0.0 ? static_cast<double>(degree_) / den : 0.0;
            qx_.push_back(f * (hx_[i + 1] - hx_[i]));
            qy_.push_back(f * (hy_[i + 1] - hy_[i]));
            qw_.push_back(f * (hw_[i + 1] - hw_[i]));
        }
        u0_ = knots_[p];
        u1_ = knots_[n];
        break;
    }
    }
}

void Eval::spline_at(double u, double& x, double& y, double& w, double& dx, double& dy,
                     double& dw) const
{
    const auto p        = static_cast<std::size_t>(degree_);
    const std::size_t n = hx_.size();
    const double uu     = std::clamp(u, u0_, u1_);
    // The span: knots[k] ≤ u < knots[k+1]; the domain's end belongs to the
    // last span that is not empty.
    const std::size_t k = [&] {
        if (uu >= u1_) {
            std::size_t last = n - 1;
            while (last > p && !(knots_[last + 1] > knots_[last]))
                --last;
            return last;
        }
        std::size_t lo = p;
        std::size_t hi = n;
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (uu >= knots_[mid])
                lo = mid;
            else
                hi = mid;
        }
        return lo;
    }();

    std::array<double, 16> ex{};
    std::array<double, 16> ey{};
    std::array<double, 16> ew{};
    for (std::size_t j = 0; j <= p; ++j) {
        ex[j] = hx_[k - p + j];
        ey[j] = hy_[k - p + j];
        ew[j] = hw_[k - p + j];
    }
    for (std::size_t r = 1; r <= p; ++r)
        for (std::size_t j = p; j >= r; --j) {
            const std::size_t i = k - p + j;
            const double den    = knots_[i + p - r + 1] - knots_[i];
            const double alpha  = den == 0.0 ? 0.0 : (uu - knots_[i]) / den;
            ex[j]               = ((1.0 - alpha) * ex[j - 1]) + (alpha * ex[j]);
            ey[j]               = ((1.0 - alpha) * ey[j - 1]) + (alpha * ey[j]);
            ew[j]               = ((1.0 - alpha) * ew[j - 1]) + (alpha * ew[j]);
        }
    x = ex[p];
    y = ey[p];
    w = ew[p];

    if (p == 1) {
        dx = qx_[k - 1];
        dy = qy_[k - 1];
        dw = qw_[k - 1];
        return;
    }
    // The derivative's de Boor: degree p − 1, span k − 1 of the shortened knots.
    const std::size_t q = p - 1;
    for (std::size_t j = 0; j <= q; ++j) {
        ex[j] = qx_[k - p + j];
        ey[j] = qy_[k - p + j];
        ew[j] = qw_[k - p + j];
    }
    for (std::size_t r = 1; r <= q; ++r)
        for (std::size_t j = q; j >= r; --j) {
            const std::size_t i = k - p + j;
            const double lo     = knots_[i + 1];
            const double den    = knots_[i + q - r + 2] - lo;
            const double alpha  = den == 0.0 ? 0.0 : (uu - lo) / den;
            ex[j]               = ((1.0 - alpha) * ex[j - 1]) + (alpha * ex[j]);
            ey[j]               = ((1.0 - alpha) * ey[j - 1]) + (alpha * ey[j]);
            ew[j]               = ((1.0 - alpha) * ew[j - 1]) + (alpha * ew[j]);
        }
    dx = ex[q];
    dy = ey[q];
    dw = ew[q];
}

Vec Eval::point(double t) const
{
    switch (kind_) {
    case PathPiece::Kind::Segment: return lerp(a_, b_, t);
    case PathPiece::Kind::Arc:
    case PathPiece::Kind::Ellipse: {
        const SinCos sc = sin_cos_rad(start_ + (sweep_ * t));
        return Vec{c_.x + (sc.cos * u_.x) + (sc.sin * v_.x),
                   c_.y + (sc.cos * u_.y) + (sc.sin * v_.y)};
    }
    case PathPiece::Kind::Spline: {
        double x  = 0.0;
        double y  = 0.0;
        double w  = 1.0;
        double dx = 0.0;
        double dy = 0.0;
        double dw = 0.0;
        spline_at(u0_ + ((u1_ - u0_) * t), x, y, w, dx, dy, dw);
        return w == 0.0 ? Vec{x, y} : Vec{x / w, y / w};
    }
    }
    return Vec{};
}

Vec Eval::tangent(double t) const
{
    switch (kind_) {
    case PathPiece::Kind::Segment: return sub(b_, a_);
    case PathPiece::Kind::Arc:
    case PathPiece::Kind::Ellipse: {
        const SinCos sc = sin_cos_rad(start_ + (sweep_ * t));
        return Vec{sweep_ * ((-sc.sin * u_.x) + (sc.cos * v_.x)),
                   sweep_ * ((-sc.sin * u_.y) + (sc.cos * v_.y))};
    }
    case PathPiece::Kind::Spline: {
        double x  = 0.0;
        double y  = 0.0;
        double w  = 1.0;
        double dx = 0.0;
        double dy = 0.0;
        double dw = 0.0;
        spline_at(u0_ + ((u1_ - u0_) * t), x, y, w, dx, dy, dw);
        if (w == 0.0) return Vec{};
        // (A′ − w′ C) / w, C = A / w; times du/dt.
        const double cx    = x / w;
        const double cy    = y / w;
        const double speed = u1_ - u0_;
        return Vec{speed * (dx - (dw * cx)) / w, speed * (dy - (dw * cy)) / w};
    }
    }
    return Vec{};
}

Point2 Eval::world(double t) const
{
    const Vec v = point(t);
    return Point2{origin_.x + mm_round(v.x * static_cast<double>(kMmPerMetre)),
                  origin_.y + mm_round(v.y * static_cast<double>(kMmPerMetre))};
}

// ---- sampling, nearest, length -----------------------------------------------

void samples(const PathPiece& piece, const Eval& eval, std::vector<double>& ts,
             std::vector<Vec>& pts)
{
    ts.clear();
    pts.clear();
    switch (piece.kind) {
    case PathPiece::Kind::Segment: ts = {0.0, 1.0}; break;
    case PathPiece::Kind::Arc:
    case PathPiece::Kind::Ellipse: {
        const double turns =
            std::abs(static_cast<double>(piece.sweep_udeg)) / static_cast<double>(kUDegFullCircle);
        const int steps =
            std::max(4, static_cast<int>(std::ceil(turns * static_cast<double>(kStepsPerTurn))));
        for (int i = 0; i <= steps; ++i)
            ts.push_back(static_cast<double>(i) / static_cast<double>(steps));
        break;
    }
    case PathPiece::Kind::Spline: {
        const std::vector<std::int64_t> knots = knots_of(piece);
        const std::size_t n                   = piece.controls.size();
        const auto p                          = static_cast<std::size_t>(piece.spline.degree);
        if (knots.size() != n + p + 1 || knots[n] <= knots[p]) {
            ts = {0.0, 1.0};
            break;
        }
        const auto u0 = static_cast<double>(knots[p]);
        const auto u1 = static_cast<double>(knots[n]);
        ts.push_back(0.0);
        for (std::size_t k = p; k < n; ++k) {
            if (knots[k + 1] <= knots[k]) continue;
            const double a = (static_cast<double>(knots[k]) - u0) / (u1 - u0);
            const double b = (static_cast<double>(knots[k + 1]) - u0) / (u1 - u0);
            for (int i = 1; i <= kStepsPerSpan; ++i)
                ts.push_back(
                    a + ((b - a) * static_cast<double>(i) / static_cast<double>(kStepsPerSpan)));
        }
        ts.back() = 1.0;
        break;
    }
    }
    pts.reserve(ts.size());
    for (const double t : ts)
        pts.push_back(eval.point(t));
}

namespace {

double nearest_in(const PathPiece& piece, const Eval& e, Vec probe)
{
    std::vector<double> ts;
    std::vector<Vec> pts;
    samples(piece, e, ts, pts);
    const auto far = [&e, probe](double t) {
        const Vec v = sub(e.point(t), probe);
        return dot(v, v);
    };
    std::size_t best = 0;
    for (std::size_t i = 1; i < pts.size(); ++i)
        if (dot(sub(pts[i], probe), sub(pts[i], probe)) <
            dot(sub(pts[best], probe), sub(pts[best], probe)))
            best = i;
    const double lo = ts[best == 0 ? 0 : best - 1];
    const double hi = ts[std::min(best + 1, ts.size() - 1)];
    const double t  = golden_min(far, lo, hi);
    return far(t) <= far(ts[best]) ? t : ts[best];
}

} // namespace

double nearest_t(const PathPiece& piece, Point2 probe)
{
    const Eval e(piece, probe);
    return nearest_in(piece, e, Vec{});
}

double length(const PathPiece& piece, double t0, double t1)
{
    if (t1 <= t0) return 0.0;
    const Eval e(piece, piece.from);
    if (piece.kind == PathPiece::Kind::Segment) return norm(e.tangent(0.0)) * (t1 - t0);
    const std::vector<double> at = breaks(piece, t0, t1);
    double sum                   = 0.0;
    for (std::size_t i = 0; i + 1 < at.size(); ++i)
        if (at[i + 1] > at[i]) sum += gauss(e, at[i], at[i + 1]);
    return sum;
}

double twice_area(const PathPiece& piece, Point2 origin)
{
    // ∫ (x·y′ − y·x′) dt over the piece, in square metres about `origin`: the
    // piece's share of ½∮(x dy − y dx). The same fixed rule and the same breaks
    // `length` sums over, so an area is summed alike on every platform — and
    // for a spline the integrand is a polynomial on each knot span, which ten
    // Gauss nodes integrate exactly up to degree 19.
    const Eval e(piece, origin);
    if (piece.kind == PathPiece::Kind::Segment) {
        const Vec a = e.point(0.0);
        const Vec b = e.point(1.0);
        return (a.x * b.y) - (b.x * a.y);
    }
    const std::vector<double> at = breaks(piece, 0.0, 1.0);
    double sum                   = 0.0;
    for (std::size_t i = 0; i + 1 < at.size(); ++i) {
        if (!(at[i + 1] > at[i])) continue;
        const double half = (at[i + 1] - at[i]) / 2.0;
        const double mid  = (at[i] + at[i + 1]) / 2.0;
        double part       = 0.0;
        for (std::size_t k = 0; k < kGaussNode.size(); ++k) {
            const double d = half * kGaussNode[k];
            for (const double t : {mid - d, mid + d}) {
                const Vec p = e.point(t);
                const Vec v = e.tangent(t);
                part += kGaussWeight[k] * ((p.x * v.y) - (p.y * v.x));
            }
        }
        sum += part * half;
    }
    return sum;
}

double t_at_length(const PathPiece& piece, double metres)
{
    const double total = length(piece, 0.0, 1.0);
    if (metres <= 0.0 || total <= 0.0) return 0.0;
    if (metres >= total) return 1.0;
    const Eval e(piece, piece.from);
    double lo = 0.0;
    double hi = 1.0;
    double t  = metres / total;
    for (int i = 0; i < 60; ++i) {
        const double f = length(piece, 0.0, t) - metres;
        if (std::abs(f) < 1e-9) break;
        if (f > 0.0)
            hi = t;
        else
            lo = t;
        const double speed = norm(e.tangent(t));
        double next        = speed > 0.0 ? t - (f / speed) : (lo + hi) / 2.0;
        if (!(next > lo && next < hi)) next = (lo + hi) / 2.0;
        t = next;
    }
    return t;
}

// ---- splines cut, clamped, reversed -------------------------------------------

std::pair<SplineParts, SplineParts> split_spline(const SplineParts& whole, std::int64_t u)
{
    SplineParts all = whole;
    if (all.def.knots_nano.empty())
        all.def.knots_nano = uniform_clamped_knots(all.controls.size(), all.def.degree);
    const std::size_t n             = all.controls.size();
    const auto p                    = static_cast<std::size_t>(all.def.degree);
    std::vector<std::int64_t> knots = all.def.knots_nano;
    if (n < p + 1 || knots.size() != n + p + 1 || u <= knots[p] || u >= knots[n])
        return {all, SplineParts{}};

    Homog h                  = homog_of(all);
    const Point2 origin      = all.controls.front();
    const auto [first, many] = to_multiplicity(h, knots, all.def.degree, u);
    // THE CUT IS A CONTROL POINT NOW: `u` occurs p times from `first`, and the
    // curve passes through the control before that run.
    const std::size_t left_end    = first;                // controls [0, first)
    const std::size_t right_start = first + many - p - 1; // the shared one
    std::vector<std::int64_t> left(knots.begin(),
                                   knots.begin() + static_cast<std::ptrdiff_t>(first + p));
    left.push_back(u);
    std::vector<std::int64_t> right{u};
    right.insert(right.end(), knots.begin() + static_cast<std::ptrdiff_t>(first + many - p),
                 knots.end());
    return {parts_of(h, 0, left_end, std::move(left), all.def, origin),
            parts_of(h, right_start, h.x.size(), std::move(right), all.def, origin)};
}

SplineParts clamped(const SplineParts& whole)
{
    SplineParts all = whole;
    if (all.def.knots_nano.empty())
        all.def.knots_nano = uniform_clamped_knots(all.controls.size(), all.def.degree);
    const std::size_t n = all.controls.size();
    const auto p        = static_cast<std::size_t>(all.def.degree);
    if (n < p + 1 || all.def.knots_nano.size() != n + p + 1) return all;

    std::vector<std::int64_t> knots = all.def.knots_nano;
    Homog h                         = homog_of(all);
    const Point2 origin             = all.controls.front();
    // THE START: the first p + 1 knots the domain's first.
    const std::int64_t a = knots[p];
    if (!std::all_of(knots.begin(), knots.begin() + static_cast<std::ptrdiff_t>(p + 1),
                     [a](std::int64_t k) { return k == a; })) {
        const auto [first, many] = to_multiplicity(h, knots, all.def.degree, a);
        const std::size_t from   = first + many - p - 1;
        std::vector<std::int64_t> cut{a};
        cut.insert(cut.end(), knots.begin() + static_cast<std::ptrdiff_t>(first + many - p),
                   knots.end());
        Homog rest;
        rest.x.assign(h.x.begin() + static_cast<std::ptrdiff_t>(from), h.x.end());
        rest.y.assign(h.y.begin() + static_cast<std::ptrdiff_t>(from), h.y.end());
        rest.w.assign(h.w.begin() + static_cast<std::ptrdiff_t>(from), h.w.end());
        h     = std::move(rest);
        knots = std::move(cut);
    }
    // THE END: the last p + 1 knots the domain's last.
    const std::size_t m  = h.x.size();
    const std::int64_t b = knots[m];
    if (!std::all_of(knots.end() - static_cast<std::ptrdiff_t>(p + 1), knots.end(),
                     [b](std::int64_t k) { return k == b; })) {
        const auto [first, many] = to_multiplicity(h, knots, all.def.degree, b);
        (void)many;
        std::vector<std::int64_t> cut(knots.begin(),
                                      knots.begin() + static_cast<std::ptrdiff_t>(first + p));
        cut.push_back(b);
        h.x.resize(first);
        h.y.resize(first);
        h.w.resize(first);
        knots = std::move(cut);
    }
    SplineParts out = parts_of(h, 0, h.x.size(), std::move(knots), all.def, origin);
    out.def.closed  = whole.def.closed;
    return out;
}

SplineParts reversed_spline(const SplineParts& whole)
{
    SplineParts out = whole;
    if (out.def.knots_nano.empty())
        out.def.knots_nano = uniform_clamped_knots(out.controls.size(), out.def.degree);
    std::reverse(out.controls.begin(), out.controls.end());
    std::reverse(out.def.weights_nano.begin(), out.def.weights_nano.end());
    const std::vector<std::int64_t> k = out.def.knots_nano;
    if (!k.empty()) {
        const std::int64_t sum = k.front() + k.back();
        for (std::size_t i = 0; i < k.size(); ++i)
            out.def.knots_nano[i] = sum - k[k.size() - 1 - i];
    }
    return out;
}

// ---- meeting -----------------------------------------------------------------

Meets meets(const PathPiece& p, const PathPiece& q)
{
    Meets out;
    auto solved = kernel_meets(p, q);
    if (!solved) {
        out.unresolved = true;
        return out;
    }
    for (const PathCrossing& hit : solved.value().crossings)
        out.hits.push_back(
            Hit{.s = hit.at.t, .t = 0.0, .point = hit.point, .touching = hit.touching});
    for (const PathOverlap& span : solved.value().overlaps)
        out.overlaps.push_back(Span{.s0 = span.from.t, .s1 = span.to.t});
    out.unresolved = solved.value().unresolved;
    return out;
}

Box2 bounds(const PathPiece& piece)
{
    Box2 box;
    const Eval e(piece, piece.from);
    std::vector<double> ts;
    std::vector<Vec> pts;
    samples(piece, e, ts, pts);
    const auto grow = [&box, &e](double t) { box.extend(e.world(t)); };
    for (const double t : ts)
        grow(t);
    // The extremes between samples, each refined where a sample is a local
    // extreme of its coordinate.
    for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
        for (int axis = 0; axis < 2; ++axis) {
            const auto c    = [axis](const Vec& v) { return axis == 0 ? v.x : v.y; };
            const bool high = c(pts[i]) >= c(pts[i - 1]) && c(pts[i]) >= c(pts[i + 1]);
            const bool low  = c(pts[i]) <= c(pts[i - 1]) && c(pts[i]) <= c(pts[i + 1]);
            if (!high && !low) continue;
            const double sign = high ? -1.0 : 1.0;
            const double t = golden_min([&e, &c, sign](double x) { return sign * c(e.point(x)); },
                                        ts[i - 1], ts[i + 1]);
            grow(t);
        }
    }
    return box;
}

} // namespace piricad::core::curve
