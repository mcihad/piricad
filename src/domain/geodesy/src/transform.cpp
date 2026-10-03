// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/geodesy/transform.hpp"

#include "piricad/core/text.hpp"

#ifdef PIRICAD_HAVE_PROJ
#include <proj.h>
#endif

#include <algorithm>
#include <cmath>

namespace piricad::domain::geodesy {

std::string TransformInfo::sentence() const
{
    if (!known()) return "PROJ işlem adı vermedi.";
    std::string out = "PROJ işlemi: " + name + ".";
    if (ballpark)
        out += " KABA (ballpark) işlem: datum farkı modellenemedi, sonuç metrelerce kayabilir.";
    else if (accuracy_m < 0.0)
        out += " Doğruluk: PROJ bildirmiyor.";
    else if (accuracy_m == 0.0)
        out += " Doğruluk: aynı datum içinde, dönüşüm tam (izdüşüm değişimi).";
    else {
        std::string metres = core::format_general(accuracy_m, 3);
        std::replace(metres.begin(), metres.end(), '.', ','); // the manual writes 0,01 m² too
        out += " Doğruluk: yaklaşık " + metres + " m.";
    }
    for (const Grid& g : grids)
        out += std::string(" Grid: ") + g.name + (g.available ? "." : " (bu makinede YOK).");
    return out;
}

#ifdef PIRICAD_HAVE_PROJ

struct Transform::Impl
{
    PJ_CONTEXT* ctx{nullptr};
    PJ* pj{nullptr};

    ~Impl()
    {
        if (pj) proj_destroy(pj);
        if (ctx) proj_context_destroy(ctx);
    }
};

bool Transform::available() noexcept
{
    return true;
}

std::string Transform::backend_version()
{
    return std::string("PROJ ") + proj_info().version;
}

namespace {

/// A PROJ object that releases itself: every early return below would leak one.
struct Handle
{
    PJ* pj{nullptr};

    explicit Handle(PJ* p = nullptr) : pj(p) {}

    ~Handle()
    {
        if (pj != nullptr) proj_destroy(pj);
    }

    Handle(const Handle&)            = delete;
    Handle& operator=(const Handle&) = delete;
};

/// PROJ's name for a pipeline, without the axis-order steps this program adds itself to keep
/// easting first (`proj_normalize_for_visualization`): they are plumbing, and a reader of
/// "axis order change (2D) + Inverse of … + axis order change (2D)" looks for a datum shift in
/// them.
std::string without_axis_steps(const std::string& name)
{
    std::string out;
    std::size_t at = 0;
    while (at <= name.size()) {
        std::size_t end = name.find(" + ", at);
        if (end == std::string::npos) end = name.size();
        const std::string step = name.substr(at, end - at);
        if (step != "axis order change (2D)" && step != "axis order change (3D)") {
            if (!out.empty()) out += " + ";
            out += step;
        }
        at = end + 3;
    }
    return out;
}

/// What PROJ says about one coordinate operation: its name, accuracy, ballpark flag and grids.
TransformInfo info_of(PJ_CONTEXT* ctx, PJ* op)
{
    TransformInfo info;
    if (const char* name = proj_get_name(op); name != nullptr) info.name = without_axis_steps(name);
    info.accuracy_m = proj_coordoperation_get_accuracy(ctx, op);
    info.ballpark   = proj_coordoperation_has_ballpark_transformation(ctx, op) != 0;
    const int n     = proj_coordoperation_get_grid_used_count(ctx, op);
    for (int i = 0; i < n; ++i) {
        const char* short_name = nullptr;
        const char* url        = nullptr;
        int direct = 0, open = 0, available = 0;
        if (proj_coordoperation_get_grid_used(ctx, op, i, &short_name, nullptr, nullptr, &url,
                                              &direct, &open, &available) == 0)
            continue;
        info.grids.push_back(
            {short_name != nullptr ? short_name : "", available != 0, url != nullptr ? url : ""});
    }
    return info;
}

/// The grids the BEST candidate operations between two systems read that this machine lacks.
/// PROJ would otherwise quietly use the next-best operation; this is how the refusal can say what
/// to install rather than only that something is missing.
std::vector<TransformInfo::Grid> missing_grids(PJ_CONTEXT* ctx, PJ* source, PJ* target)
{
    std::vector<TransformInfo::Grid> missing;
    PJ_OPERATION_FACTORY_CONTEXT* factory = proj_create_operation_factory_context(ctx, nullptr);
    if (factory == nullptr) return missing;
    proj_operation_factory_context_set_grid_availability_use(ctx, factory,
                                                             PROJ_GRID_AVAILABILITY_IGNORED);
    proj_operation_factory_context_set_spatial_criterion(
        ctx, factory, PROJ_SPATIAL_CRITERION_PARTIAL_INTERSECTION);
    if (PJ_OBJ_LIST* ops = proj_create_operations(ctx, source, target, factory); ops != nullptr) {
        const int count = proj_list_get_count(ops);
        for (int i = 0; i < count; ++i) {
            Handle op(proj_list_get(ctx, ops, i));
            if (op.pj == nullptr || proj_coordoperation_is_instantiable(ctx, op.pj) != 0) continue;
            for (const TransformInfo::Grid& g : info_of(ctx, op.pj).grids)
                if (!g.available &&
                    std::none_of(missing.begin(), missing.end(),
                                 [&g](const TransformInfo::Grid& m) { return m.name == g.name; }))
                    missing.push_back(g);
        }
        proj_list_destroy(ops);
    }
    proj_operation_factory_context_destroy(factory);
    return missing;
}

} // namespace

core::Result<Transform> Transform::between(const std::string& source, const std::string& target,
                                           TransformOptions options)
{
    auto impl = std::make_unique<Impl>();

    impl->ctx = proj_context_create();
    if (!impl->ctx) return core::err(core::ErrorCode::Internal, "PROJ bağlamı oluşturulamadı");
    // AN ID PROJ DOES NOT KNOW is an answer, said below in the user's words, and PROJ would
    // otherwise print its own English on the console of a desktop program.
    proj_log_level(impl->ctx, PJ_LOG_NONE);

    Handle from(proj_create(impl->ctx, source.c_str()));
    if (from.pj == nullptr)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Koordinat sistemi tanınmıyor: '" + source +
                             "'. EPSG kodu (EPSG:5254), TUREF/TM30 gibi bir ad ya da PROJ/WKT "
                             "tanımı yazın.");
    Handle to(proj_create(impl->ctx, target.c_str()));
    if (to.pj == nullptr)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Koordinat sistemi tanınmıyor: '" + target +
                             "'. EPSG kodu (EPSG:5254), TUREF/TM30 gibi bir ad ya da PROJ/WKT "
                             "tanımı yazın.");

    // THE BEST OPERATION OR NONE, unless the caller has consented to less. PROJ's default is to
    // fall back, silently, to the next operation whose grids it HAS and, failing that, to a
    // ballpark shift that treats two datums as one: a coordinate that lands metres away and looks
    // exactly like a right one (TODOS G-01).
    const char* strict[] = {"ONLY_BEST=YES", "ALLOW_BALLPARK=NO", nullptr};
    const char* loose[]  = {"ALLOW_BALLPARK=YES", nullptr};
    PJ* raw              = proj_create_crs_to_crs_from_pj(impl->ctx, from.pj, to.pj, nullptr,
                                             options.allow_ballpark ? loose : strict);
    if (!raw) {
        const int code      = proj_context_errno(impl->ctx);
        const char* why     = proj_context_errno_string(impl->ctx, code);
        std::string refusal = "'" + source + "' -> '" + target + "' dönüşümü kurulamadı";
        if (!options.allow_ballpark) {
            // WAS IT THE STRICTNESS? The same pair asked loosely: if PROJ then builds one, the
            // refusal is ours and the message says what is missing and how to consent.
            Handle loosely(
                proj_create_crs_to_crs_from_pj(impl->ctx, from.pj, to.pj, nullptr, loose));
            if (loosely.pj != nullptr) {
                refusal += ": PROJ bu ikili için doğruluğu bilinen, kullanılabilir bir işlem "
                           "bulamadı";
                if (const auto lacking = missing_grids(impl->ctx, from.pj, to.pj);
                    !lacking.empty()) {
                    refusal += "; en iyi işlem şu grid dosyasını okuyor ve bu makinede yok:";
                    for (const TransformInfo::Grid& g : lacking)
                        refusal += " " + g.name + (g.url.empty() ? "" : " (" + g.url + ")");
                    refusal += ". Dosyayı PROJ veri dizinine koyun";
                } else {
                    refusal += " (yalnız kaba/ballpark: iki datum aynıymış gibi davranılır, "
                               "sonuç metrelerce kayabilir)";
                }
                refusal += ". Yine de istiyorsanız kaba=evet ile açıkça izin verin.";
                return core::err(core::ErrorCode::InvalidArgument, refusal);
            }
        }
        return core::err(core::ErrorCode::InvalidArgument,
                         refusal + ": " + (why ? why : "bilinmeyen PROJ hatası"));
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
    t.source_angular_ =
        proj_degree_input(impl->pj, PJ_FWD) != 0 || proj_angular_input(impl->pj, PJ_FWD) != 0;
    t.target_angular_ =
        proj_degree_output(impl->pj, PJ_FWD) != 0 || proj_angular_output(impl->pj, PJ_FWD) != 0;
    t.impl_   = std::move(impl);
    t.source_ = source;
    t.target_ = target;
    return t;
}

bool Transform::forward(double& easting, double& northing) const
{
    PJ_COORD in  = proj_coord(easting, northing, 0.0, 0.0);
    PJ_COORD out = proj_trans(impl_->pj, PJ_FWD, in);
    if (out.xyzt.x == HUGE_VAL || out.xyzt.y == HUGE_VAL) return false;
    easting  = out.xyzt.x;
    northing = out.xyzt.y;
    return true;
}

bool Transform::inverse(double& easting, double& northing) const
{
    PJ_COORD in  = proj_coord(easting, northing, 0.0, 0.0);
    PJ_COORD out = proj_trans(impl_->pj, PJ_INV, in);
    if (out.xyzt.x == HUGE_VAL || out.xyzt.y == HUGE_VAL) return false;
    easting  = out.xyzt.x;
    northing = out.xyzt.y;
    return true;
}

core::Result<TransformInfo> Transform::info_at(double easting, double northing) const
{
    // The operation is only known after a coordinate has been run through it: PROJ picks among the
    // candidates by the point's place. A scratch copy of the point, so asking changes nothing.
    PJ_COORD in  = proj_coord(easting, northing, 0.0, 0.0);
    PJ_COORD out = proj_trans(impl_->pj, PJ_FWD, in);
    if (out.xyzt.x == HUGE_VAL || out.xyzt.y == HUGE_VAL)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Nokta dönüşümün geçerli alanının dışında; işlem sorulamadı.");
    Handle used(proj_trans_get_last_used_operation(impl_->pj));
    if (used.pj == nullptr) return TransformInfo{};
    return info_of(impl_->ctx, used.pj);
}

#else // ---------------------------------------------------------------------

struct Transform::Impl
{};

bool Transform::available() noexcept
{
    return false;
}

std::string Transform::backend_version()
{
    return "PROJ yok";
}

core::Result<Transform> Transform::between(const std::string& source, const std::string& target,
                                           TransformOptions)
{
    // Never an identity fallback. A transform that silently does nothing produces
    // a document whose coordinates are in the wrong system and look right (§12).
    return core::err(core::ErrorCode::Unsupported,
                     "'" + source + "' -> '" + target +
                         "' dönüşümü yapılamıyor: PROJ "
                         "derlenmemiş. -DPIRICAD_WITH_PROJ=ON ile yapılandırın "
                         "(Debian/Ubuntu: libproj-dev).");
}

bool Transform::forward(double&, double&) const
{
    return false;
}

bool Transform::inverse(double&, double&) const
{
    return false;
}

core::Result<TransformInfo> Transform::info_at(double, double) const
{
    return core::err(core::ErrorCode::Unsupported, "PROJ derlenmemiş; işlem sorulamaz.");
}

#endif

Transform::~Transform()                               = default;
Transform::Transform(Transform&&) noexcept            = default;
Transform& Transform::operator=(Transform&&) noexcept = default;

namespace {

/// Millimetres in, metres through PROJ, millimetres out. The rounding is
/// `core::mm_from_metres`, the same round-half-away-from-zero used everywhere
/// else, so a transform does not introduce a second rounding convention.
template<class Fn>
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

core::Error angular_refusal(const std::string& source, const std::string& target, const char* which)
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
