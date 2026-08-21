// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/geodesy/transform.hpp"

#ifdef PIRICAD_HAVE_PROJ
#include <proj.h>
#endif

namespace piricad::domain::geodesy {

#ifdef PIRICAD_HAVE_PROJ

struct Transform::Impl {
    PJ_CONTEXT* ctx{nullptr};
    PJ*         pj{nullptr};

    ~Impl()
    {
        if (pj) proj_destroy(pj);
        if (ctx) proj_context_destroy(ctx);
    }
};

bool Transform::available() noexcept { return true; }

std::string Transform::backend_version()
{
    return std::string("PROJ ") + proj_info().version;
}

core::Result<Transform> Transform::between(const std::string& source, const std::string& target)
{
    auto impl = std::make_unique<Impl>();

    impl->ctx = proj_context_create();
    if (!impl->ctx)
        return core::err(core::ErrorCode::Internal, "PROJ bağlamı oluşturulamadı");

    PJ* raw = proj_create_crs_to_crs(impl->ctx, source.c_str(), target.c_str(), nullptr);
    if (!raw) {
        const int  code = proj_context_errno(impl->ctx);
        const char* why = proj_context_errno_string(impl->ctx, code);
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + source + "' -> '" + target + "' dönüşümü kurulamadı: " +
                             (why ? why : "bilinmeyen PROJ hatası"));
    }

    // THE line. Without it EPSG:5254 expects (northing, easting) because that is
    // the axis order it declares, and every coordinate silently flips — a result
    // that looks plausible and is wrong (.claude/model.md R37a).
    impl->pj = proj_normalize_for_visualization(impl->ctx, raw);
    proj_destroy(raw);

    if (!impl->pj)
        return core::err(core::ErrorCode::Internal,
                         "PROJ eksen sırası normalleştirilemedi; koordinat sırası güvenilmez");

    Transform t;
    // Ask PROJ what units each side speaks, rather than guessing from the code.
    // After proj_normalize_for_visualization the geographic side is DEGREES, not
    // radians, so proj_degree_* is the right question — proj_angular_* asks about
    // radians and answers 0 here, which would let a degree value be rounded to the
    // nearest millimetre and move the point a hundred metres.
    t.source_angular_ = proj_degree_input(impl->pj, PJ_FWD) != 0 ||
                        proj_angular_input(impl->pj, PJ_FWD) != 0;
    t.target_angular_ = proj_degree_output(impl->pj, PJ_FWD) != 0 ||
                        proj_angular_output(impl->pj, PJ_FWD) != 0;
    t.impl_           = std::move(impl);
    t.source_         = source;
    t.target_         = target;
    return t;
}

bool Transform::forward(double& easting, double& northing) const
{
    PJ_COORD in = proj_coord(easting, northing, 0.0, 0.0);
    PJ_COORD out = proj_trans(impl_->pj, PJ_FWD, in);
    if (out.xyzt.x == HUGE_VAL || out.xyzt.y == HUGE_VAL) return false;
    easting  = out.xyzt.x;
    northing = out.xyzt.y;
    return true;
}

bool Transform::inverse(double& easting, double& northing) const
{
    PJ_COORD in = proj_coord(easting, northing, 0.0, 0.0);
    PJ_COORD out = proj_trans(impl_->pj, PJ_INV, in);
    if (out.xyzt.x == HUGE_VAL || out.xyzt.y == HUGE_VAL) return false;
    easting  = out.xyzt.x;
    northing = out.xyzt.y;
    return true;
}

#else // ---------------------------------------------------------------------

struct Transform::Impl {};

bool Transform::available() noexcept { return false; }

std::string Transform::backend_version() { return "PROJ yok"; }

core::Result<Transform> Transform::between(const std::string& source, const std::string& target)
{
    // Never an identity fallback. A transform that silently does nothing produces
    // a document whose coordinates are in the wrong system and look right (§12).
    return core::err(core::ErrorCode::Unsupported,
                     "'" + source + "' -> '" + target + "' dönüşümü yapılamıyor: PROJ "
                     "derlenmemiş. -DPIRICAD_WITH_PROJ=ON ile yapılandırın "
                     "(Debian/Ubuntu: libproj-dev).");
}

bool Transform::forward(double&, double&) const { return false; }
bool Transform::inverse(double&, double&) const { return false; }

#endif

Transform::~Transform()                            = default;
Transform::Transform(Transform&&) noexcept         = default;
Transform& Transform::operator=(Transform&&) noexcept = default;

namespace {

/// Millimetres in, metres through PROJ, millimetres out. The rounding is
/// `core::mm_from_metres`, the same round-half-away-from-zero used everywhere
/// else, so a transform does not introduce a second rounding convention.
template <class Fn>
core::Status apply(std::span<core::Point2> points, Fn&& op, const char* direction)
{
    for (std::size_t i = 0; i < points.size(); ++i) {
        double easting  = core::mm_to_metres(points[i].x);
        double northing = core::mm_to_metres(points[i].y);

        if (!op(easting, northing))
            return core::err(core::ErrorCode::ValidationFailed,
                             std::string("Nokta ") + std::to_string(i) + " " + direction +
                                 " dönüşümde geçersiz: koordinat sistemin geçerli alanı dışında.");

        points[i].x = core::mm_from_metres(easting);
        points[i].y = core::mm_from_metres(northing);
    }
    return core::ok();
}

} // namespace

namespace {

core::Error angular_refusal(const std::string& source, const std::string& target,
                            const char* which)
{
    return core::err(core::ErrorCode::InvalidArgument,
                     "'" + source + "' -> '" + target + "' dönüşümünün " + which +
                         " tarafı coğrafi (derece). Doküman geometrisi projeksiyonlu "
                         "milimetre saklar; dereceyi milimetreye yuvarlamak noktayı "
                         "yüz metre kaydırır. Derece için skaler forward/inverse "
                         "kullanın.");
}

} // namespace

core::Status Transform::forward(std::span<core::Point2> points) const
{
    if (target_angular_) return angular_refusal(source_, target_, "hedef");
    if (source_angular_) return angular_refusal(source_, target_, "kaynak");
    return apply(points, [this](double& e, double& n) { return forward(e, n); }, "ileri");
}

core::Status Transform::inverse(std::span<core::Point2> points) const
{
    if (target_angular_) return angular_refusal(source_, target_, "hedef");
    if (source_angular_) return angular_refusal(source_, target_, "kaynak");
    return apply(points, [this](double& e, double& n) { return inverse(e, n); }, "geri");
}

} // namespace piricad::domain::geodesy
