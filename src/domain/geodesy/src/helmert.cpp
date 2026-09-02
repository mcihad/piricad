// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/geodesy/helmert.hpp"

#include "piricad/core/units.hpp"

#include <cmath>
#include <string>

namespace piricad::domain::geodesy {

core::Point2 Helmert2D::apply(core::Point2 p) const noexcept
{
    const double x = static_cast<double>(p.x);
    const double y = static_cast<double>(p.y);
    return core::Point2{core::mm_round(a * x - b * y) + tx, core::mm_round(b * x + a * y) + ty};
}

core::Result<Helmert2D> fit_helmert(const std::vector<ControlPoint>& points, bool lock_scale)
{
    if (points.size() < 2)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Oturtma en az iki kontrol noktası ister. Verilen: " +
                             std::to_string(points.size()) +
                             ". Tek nokta yalnız ötelemeyi verir; dönüklük ve ölçek bilinmez.");

    // ---- reduce to the centroids, in integers ----
    //
    // The sums below are of PRODUCTS of coordinates, and a TUREF easting is
    // 8·10^8 mm: its square is 6·10^17, which leaves a double's 53-bit mantissa
    // at once and would overflow int64 after a handful of terms. Reduced to the
    // centroid the differences are the size of the site — metres to kilometres —
    // and every product stays exact.
    std::int64_t sum_lx = 0, sum_ly = 0, sum_mx = 0, sum_my = 0;
    for (const ControlPoint& p : points) {
        sum_lx += p.local.x;
        sum_ly += p.local.y;
        sum_mx += p.map.x;
        sum_my += p.map.y;
    }
    const auto n     = static_cast<std::int64_t>(points.size());
    const core::Mm lx = sum_lx / n;
    const core::Mm ly = sum_ly / n;
    const core::Mm mx = sum_mx / n;
    const core::Mm my = sum_my / n;

    // ---- the closed-form least squares ----
    //
    // a = Σ(dx·dX + dy·dY) / Σ(dx² + dy²)
    // b = Σ(dx·dY − dy·dX) / Σ(dx² + dy²)
    //
    // This is the exact minimiser of the squared residuals for a similarity, and
    // it is four sums and one division — no matrix, no iteration, and nothing
    // whose answer could depend on a pivoting order.
    double s_cross = 0.0, s_skew = 0.0, s_norm = 0.0;
    for (const ControlPoint& p : points) {
        const double dx = static_cast<double>(p.local.x - lx);
        const double dy = static_cast<double>(p.local.y - ly);
        const double dX = static_cast<double>(p.map.x - mx);
        const double dY = static_cast<double>(p.map.y - my);

        s_cross += dx * dX + dy * dY;
        s_skew += dx * dY - dy * dX;
        s_norm += dx * dx + dy * dy;
    }

    if (s_norm <= 0.0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Kontrol noktalarının hepsi aynı yerde; dönüklük ve ölçek "
                         "belirlenemez. Birbirinden ayrı noktalar verin.");

    Helmert2D fit;
    fit.a = s_cross / s_norm;
    fit.b = s_skew / s_norm;

    const double magnitude = std::sqrt(fit.a * fit.a + fit.b * fit.b);
    if (magnitude <= 0.0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Kontrol noktaları bir dönüşüm tanımlamıyor: ölçek sıfır çıktı.");

    if (lock_scale) {
        // The rotation the least squares found, at unit scale. Normalising keeps
        // the direction and throws away the length, which is exactly what "the
        // tape was right, do not rescale my measurements" means.
        fit.a /= magnitude;
        fit.b /= magnitude;
        fit.scale = 1.0;
    } else {
        fit.scale = magnitude;
    }

    // The translation closes the fit on the centroids, so the fitted drawing and
    // the control agree at their common centre whatever the residuals are.
    fit.tx = mx - core::mm_round(fit.a * static_cast<double>(lx) - fit.b * static_cast<double>(ly));
    fit.ty = my - core::mm_round(fit.b * static_cast<double>(lx) + fit.a * static_cast<double>(ly));

    // ---- what it cost ----
    //
    // The residuals are measured with the transformation that will ACTUALLY be
    // applied, rounding included, so the report cannot promise a fit the drawing
    // does not get.
    std::int64_t sum_squares = 0;
    fit.residuals.reserve(points.size());
    for (const ControlPoint& p : points) {
        const core::Point2 landed = fit.apply(p.local);
        const core::Mm d          = core::segment_length(landed, p.map);
        fit.residuals.push_back(d);
        if (d > fit.worst) fit.worst = d;
        sum_squares += d * d;
    }
    fit.rms = core::mm_round(
        std::sqrt(static_cast<double>(sum_squares) / static_cast<double>(points.size())));

    // HOW FAR THE DRAWING WAS TURNED, CLOCKWISE, in grad.
    //
    // `atan2(b, a)` is the mathematical angle and runs counter-clockwise; a
    // surveyor reads rotations the way they read azimuths, clockwise, so the sign
    // is flipped before it is reported. Getting this backwards would print 300
    // grad for a quarter turn that was really 100 — a number an engineer would
    // check against their own notes and find wrong.
    //
    // `atan2` is allowed here for the reason the canvas label uses it: nothing
    // transforms with this number and no stored value passes through it (§7.3).
    double turns = -std::atan2(fit.b, fit.a) / (2.0 * 3.14159265358979323846);
    turns -= std::floor(turns); // into [0, 1) whatever the sign
    fit.rotation_grad = turns * 400.0;

    return fit;
}

} // namespace piricad::domain::geodesy
