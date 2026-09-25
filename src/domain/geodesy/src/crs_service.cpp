// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/domain/geodesy/crs_service.hpp"

#include "kentos_cad/core/text.hpp"

#ifdef KENTOS_HAVE_PROJ
#include <proj.h>
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace kentos::domain::geodesy {
namespace {

/// Strips a leading authority prefix and any surrounding whitespace.
///
/// `TUREF/TM30`, ` TM30 ` and `tm30` are the same zone written three ways, and a
/// resolver that accepted only one of them would push the other two onto the user
/// as an error message about something they got right.
std::string_view tail_after_slash(std::string_view id)
{
    const auto slash = id.rfind('/');
    return slash == std::string_view::npos ? id : id.substr(slash + 1);
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

/// The EPSG code in `EPSG:5254` or in a bare `5254`, or 0.
int epsg_from(std::string_view id)
{
    std::string_view digits = trim(id);
    if (digits.size() > 5 && core::turkish_iequals(std::string(digits.substr(0, 5)), "EPSG:"))
        digits = trim(digits.substr(5));

    if (digits.empty()) return 0;

    int value = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9') return 0;
        value = value * 10 + (c - '0');
        if (value > 100000) return 0; // no EPSG code this project uses is that long
    }
    return value;
}

/// Whether `id` names a local site grid — `YEREL`, or `LOCAL` — the two words
/// the status bar and KOORDİNAT already read that way.
///
/// A job not yet tied to the map counts in metres from wherever the surveyor
/// put the origin: the one system no registry can name, and the one a
/// municipality's site plans most often start in.
bool local_grid(std::string_view id)
{
    const std::string word{trim(id)};
    return core::turkish_iequals(word, "YEREL") || core::turkish_iequals(word, "LOCAL");
}

#ifdef KENTOS_HAVE_PROJ

/// A PROJ handle that releases itself: every early return below would leak one.
struct Owned
{
    PJ_CONTEXT* ctx{nullptr};
    PJ* pj{nullptr};

    Owned(PJ_CONTEXT* c, PJ* p) : ctx(c), pj(p) {}

    ~Owned()
    {
        if (pj != nullptr) proj_destroy(pj);
    }

    Owned(const Owned&)            = delete;
    Owned& operator=(const Owned&) = delete;
};

/// What the first axis of `crs`'s coordinate system counts, and its name.
std::pair<double, std::string> first_axis_unit(PJ_CONTEXT* ctx, const PJ* crs)
{
    const Owned cs(ctx, proj_crs_get_coordinate_system(ctx, crs));
    if (cs.pj == nullptr) return {0.0, {}};
    double factor         = 0.0;
    const char* unit_name = nullptr;
    if (proj_cs_get_axis_info(ctx, cs.pj, 0, nullptr, nullptr, nullptr, &factor, &unit_name,
                              nullptr, nullptr) == 0)
        return {0.0, {}};
    return {factor, unit_name != nullptr ? std::string(unit_name) : std::string{}};
}

/// What one coordinate of `definition` counts, asked of PROJ rather than
/// guessed from the digits — EPSG:4326 and EPSG:32636 look alike and one of
/// them is a globe.
///
/// A BOUND system (a WKT carrying TOWGS84) is asked about the system it binds,
/// and a COMPOUND one about its horizontal half: the height of a TM30 + TUDKA
/// system is metres too, but the question here is what the easting counts.
std::pair<core::CrsUnit, std::string> unit_of(std::string_view definition)
{
    PJ_CONTEXT* ctx = proj_context_create();
    if (ctx == nullptr) return {core::CrsUnit::Unknown, {}};
    // An id PROJ does not know is an answer here ("unknown"), not a fault, and
    // PROJ would otherwise print it on the console of a desktop program.
    proj_log_level(ctx, PJ_LOG_NONE);

    std::pair<core::CrsUnit, std::string> answer{core::CrsUnit::Unknown, {}};
    {
        const std::string text(definition);
        Owned crs(ctx, proj_create(ctx, text.c_str()));
        if (crs.pj != nullptr) {
            PJ* at = crs.pj;
            std::unique_ptr<Owned> inner;
            if (proj_get_type(at) == PJ_TYPE_BOUND_CRS) {
                inner = std::make_unique<Owned>(ctx, proj_get_source_crs(ctx, at));
                at    = inner->pj;
            }
            std::unique_ptr<Owned> horizontal;
            if (at != nullptr && proj_get_type(at) == PJ_TYPE_COMPOUND_CRS) {
                horizontal = std::make_unique<Owned>(ctx, proj_crs_get_sub_crs(ctx, at, 0));
                at         = horizontal->pj;
            }

            switch (at != nullptr ? proj_get_type(at) : PJ_TYPE_UNKNOWN) {
            case PJ_TYPE_GEOGRAPHIC_CRS:
            case PJ_TYPE_GEOGRAPHIC_2D_CRS:
            case PJ_TYPE_GEOGRAPHIC_3D_CRS: {
                const auto [factor, name] = first_axis_unit(ctx, at);
                (void)factor;
                answer = {core::CrsUnit::Degree, name.empty() ? std::string("degree") : name};
                break;
            }
            case PJ_TYPE_GEOCENTRIC_CRS:
                // Metres, but not a map: X/Y/Z from the centre of the Earth. Its
                // "easting" is six thousand kilometres from anything on a pafta.
                answer = {core::CrsUnit::Other, "yer merkezli metre (X/Y/Z)"};
                break;
            case PJ_TYPE_PROJECTED_CRS:
            case PJ_TYPE_DERIVED_PROJECTED_CRS:
            case PJ_TYPE_ENGINEERING_CRS: {
                const auto [factor, name] = first_axis_unit(ctx, at);
                if (factor <= 0.0) break; // no axis to read: nobody can tell
                // EXACTLY one: a metre is the SI unit and PROJ states it as 1,
                // while a US survey foot is 0,304800609601… and must not pass.
                answer = {factor == 1.0 ? core::CrsUnit::Metre : core::CrsUnit::Other,
                          name.empty() ? std::string("metre") : name};
                break;
            }
            default:
                // A vertical or temporal system, or not a system at all (a bare
                // conversion): none of them is a plane a drawing can be in, but
                // none of them is a unit mix-up either — left to the resolver.
                break;
            }
        }
    }
    proj_context_destroy(ctx);
    return answer;
}

#endif // KENTOS_HAVE_PROJ

} // namespace

CrsService::CrsService(command::Bus& bus, CrsCatalog catalogue)
    : bus_(bus), catalogue_(std::move(catalogue))
{
    bus_.on_crs_resolve = [this](std::string_view id) { return resolve(id); };
}

CrsService::~CrsService()
{
    bus_.on_crs_resolve = nullptr;
}

core::Crs CrsService::resolve(std::string_view id) const
{
    core::Crs crs{std::string(id)};

    const Tm3Zone* zone = nullptr;
    if (const int code = epsg_from(id); code != 0) zone = catalogue_.zone_by_epsg(code);
    if (zone == nullptr) zone = catalogue_.zone_by_name(trim(tail_after_slash(id)));

    // An id this catalogue cannot place comes back carrying only that id. NOT a
    // fallback to the nearest zone and not a default: a CRS guessed wrong moves
    // every coordinate in the document by kilometres, and it does so silently
    // because the numbers still look like Turkish coordinates.
    //
    // What it COUNTS is still asked, because that question has an answer the
    // zone catalogue does not need to know: EPSG:4326 is no zone of ours and is
    // plainly degrees (TODOS F-03). A local grid counts metres by definition.
    if (zone == nullptr) {
        if (local_grid(id)) {
            crs.set_unit(core::CrsUnit::Metre, "metre");
        } else {
#ifdef KENTOS_HAVE_PROJ
            auto [unit, name] = unit_of(id);
            crs.set_unit(unit, std::move(name));
#endif
        }
        return crs;
    }

    // The epoch is DELIBERATELY EMPTY. TUREF's realisation epoch is a regulatory
    // fact and /data/crs/tm3-dilimleri.json does not carry it yet, so filling it
    // in from memory would put an unsourced regulatory value into C++ — which
    // CLAUDE.md 5.13 forbids and which a domain expert has to sign off (6.11).
    // The FIELD exists, which is what R36 asked for; the value arrives with the
    // data package that cites it.
    crs.resolve(zone->epsg, /*epoch=*/std::string{}, zone->central_meridian,
                /*geoid_model=*/std::string{}, catalogue_.parameters().datum);
    // Every catalogue zone is a Transverse Mercator in metres; that is what the
    // catalogue is a list of.
    crs.set_unit(core::CrsUnit::Metre, "metre");
    return crs;
}

} // namespace kentos::domain::geodesy
