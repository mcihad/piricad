// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/geodesy/crs_service.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace piricad::domain::geodesy {
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
    if (zone == nullptr) return crs;

    // The epoch is DELIBERATELY EMPTY. TUREF's realisation epoch is a regulatory
    // fact and /data/crs/tm3-dilimleri.json does not carry it yet, so filling it
    // in from memory would put an unsourced regulatory value into C++ — which
    // CLAUDE.md 5.13 forbids and which a domain expert has to sign off (6.11).
    // The FIELD exists, which is what R36 asked for; the value arrives with the
    // data package that cites it.
    crs.resolve(zone->epsg, /*epoch=*/std::string{}, zone->central_meridian,
                /*geoid_model=*/std::string{}, catalogue_.parameters().datum);
    return crs;
}

} // namespace piricad::domain::geodesy
