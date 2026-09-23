// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: breaking a path, KIR's cut. See break_run.hpp.
#include "kentos_cad/core/break_run.hpp"

#include <cstring>
#include <utility>

namespace kentos::core {

Result<PathBreak> break_path(const CurvePath& path, Point2 a, Point2 b)
{
    if (path.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Kırılma noktası çizgi üzerinde bulunamadı.");
    PathPlace pa        = place_of(path, a);
    PathPlace pb        = place_of(path, b);
    const bool together = !comes_before(pa, pb) && !comes_before(pb, pa);

    PathBreak out;
    if (path.closed) {
        if (together)
            return err(ErrorCode::InvalidArgument,
                       "Kapalı bir şekil tek noktadan kırılmaz; iki ayrı nokta verin.");
        out.first  = point_at(path, pa);
        out.second = point_at(path, pb);
        // FROM THE FIRST ROUND TO THE SECOND, the way the path is walked — past
        // the seam when the second comes before the first.
        out.gap        = sub_path(path, pa, pb);
        CurvePath rest = sub_path(path, pb, pa);
        rest.closed    = false;
        if (path_length(rest) <= 0 || path_length(out.gap) <= 0)
            return err(ErrorCode::InvalidArgument,
                       "Kırılma şeklin tamamını götürüyor; parça bırakmıyor. Silmek için SİL "
                       "kullanın.");
        out.kept.push_back(std::move(rest));
        return out;
    }

    // ORDERED ALONG THE PATH, not in the order they were given.
    if (comes_before(pb, pa)) std::swap(pa, pb);
    out.first      = point_at(path, pa);
    out.second     = point_at(path, pb);
    CurvePath head = sub_path(path, path_start(path), pa);
    CurvePath tail = sub_path(path, pb, path_end(path));
    if (!together) out.gap = sub_path(path, pa, pb);
    if (path_length(head) > 0) out.kept.push_back(std::move(head));
    if (path_length(tail) > 0) out.kept.push_back(std::move(tail));
    if (out.kept.empty() || (together && out.kept.size() < 2))
        return err(ErrorCode::InvalidArgument,
                   together ? "Kırılma noktası çizginin ucunda; bölünecek bir şey kalmıyor."
                            : "Kırılma çizginin tamamını götürüyor; parça bırakmıyor. Silmek "
                              "için SİL kullanın.");
    return out;
}

std::vector<std::uint8_t> encode_break_guide(const BreakGuide& guide)
{
    // version, key — little-endian as the machine writes it, because the bytes
    // never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(1 + sizeof(guide.key));
    bytes[0] = 1;
    std::memcpy(bytes.data() + 1, &guide.key, sizeof(guide.key));
    return bytes;
}

Result<BreakGuide> decode_break_guide(std::span<const std::uint8_t> bytes)
{
    BreakGuide guide;
    if (bytes.size() != 1 + sizeof(guide.key) || bytes[0] != 1)
        return err(ErrorCode::InvalidArgument, "Kırma önizlemesinin baytları tanınmıyor.");
    std::memcpy(&guide.key, bytes.data() + 1, sizeof(guide.key));
    return guide;
}

} // namespace kentos::core
