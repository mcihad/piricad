// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/point_list.hpp"

#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace kentos::io {
namespace {

constexpr std::size_t kMaxLine   = 4096;    ///< a point line is short; anything longer is not one
constexpr std::size_t kMaxPoints = 5000000; ///< bounded so a hostile file cannot exhaust memory

/// Parses a decimal number written in metres into exact millimetres.
///
/// BY DIGITS, not through `strtod`. A coordinate is stored as an integer number
/// of millimetres (Article 2.4) and going through a double to get there loses the
/// last digit of a nine-figure easting — 485320.543 is not exactly representable,
/// and the value that comes back is a millimetre off often enough to matter on a
/// parcel corner. Reading the integer and fractional parts separately is exact.
///
/// Accepts `.` or `,` as the decimal mark: a Turkish-locale export writes
/// `485320,543` and refusing it would refuse most real files.
core::Result<core::Mm> parse_metres(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);

    if (text.empty()) return core::err(core::ErrorCode::ParseError, "sayı boş");

    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) return core::err(core::ErrorCode::ParseError, "sayı yalnız işaretten ibaret");

    std::int64_t whole = 0;
    std::size_t i      = 0;
    for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
        // A coordinate beyond ten million metres is not on this planet's grid, and
        // stopping here is what keeps the millimetre multiply below in range.
        if (whole > 10'000'000) return core::err(core::ErrorCode::ParseError, "sayı çok büyük");
        whole = whole * 10 + (text[i] - '0');
    }

    std::int64_t frac  = 0;
    std::int64_t scale = 1;
    if (i < text.size() && (text[i] == '.' || text[i] == ',')) {
        ++i;
        // Three digits, which is the millimetre. A fourth is read and used to
        // round rather than dropped, so 485320.5435 lands on 485320544 and not on
        // 485320543.
        std::int64_t fourth = 0;
        std::size_t taken   = 0;
        for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i, ++taken) {
            if (taken < 3) {
                frac = frac * 10 + (text[i] - '0');
                scale *= 10;
            } else if (taken == 3) {
                fourth = text[i] - '0';
            }
        }
        while (scale < 1000) {
            frac *= 10;
            scale *= 10;
        }
        if (fourth >= 5) ++frac;
    }

    if (i != text.size()) return core::err(core::ErrorCode::ParseError, "sayı olmayan karakter");

    const std::int64_t mm = whole * core::kMmPerMetre + frac;
    return static_cast<core::Mm>(negative ? -mm : mm);
}

/// Splits `line` on the delimiter this file uses. Runs of spaces count as one.
std::vector<std::string_view> split(std::string_view line, char delimiter)
{
    std::vector<std::string_view> out;
    if (delimiter == ' ') {
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
                ++i;
            const std::size_t start = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t')
                ++i;
            if (i > start) out.push_back(line.substr(start, i - start));
        }
        return out;
    }

    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == delimiter) {
            out.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

/// Which delimiter a line uses. Checked in order of how unambiguous each is.
char delimiter_of(std::string_view line)
{
    if (line.find(';') != std::string_view::npos) return ';';
    if (line.find('\t') != std::string_view::npos) return '\t';
    if (line.find(',') != std::string_view::npos) return ',';
    return ' ';
}

std::string trimmed(std::string_view v)
{
    while (!v.empty() && (v.front() == ' ' || v.front() == '\t'))
        v.remove_prefix(1);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r'))
        v.remove_suffix(1);
    return std::string(v);
}

} // namespace

core::Result<std::vector<SurveyPoint>> read_point_list(const std::string& path, PointOrder order)
{
    std::ifstream in(path);
    if (!in) return core::err(core::ErrorCode::NotFound, "Nokta listesi açılamadı: " + path);

    std::vector<SurveyPoint> points;
    std::string line;
    std::size_t number = 0;
    char delimiter     = 0;

    while (std::getline(in, line)) {
        ++number;
        if (line.size() > kMaxLine)
            return core::err(core::ErrorCode::ParseError,
                             path + ":" + std::to_string(number) + " — satır çok uzun (" +
                                 std::to_string(line.size()) +
                                 " karakter). Bu bir nokta listesi değil.");

        const std::string text = trimmed(line);
        if (text.empty()) continue;

        // A comment or a header row. Netcad and most instruments write one, and
        // refusing a file because of it would refuse most real files.
        if (text[0] == '#' || text[0] == '/' || text[0] == '*') continue;

        if (delimiter == 0) delimiter = delimiter_of(text);
        const std::vector<std::string_view> field = split(text, delimiter);

        if (field.size() < 3) {
            // A header row reads as too few numeric fields; skip it once, at the
            // top, and refuse it anywhere else — a short line in the middle is a
            // truncated point and losing it would lose a corner.
            if (points.empty()) continue;
            return core::err(core::ErrorCode::ParseError,
                             path + ":" + std::to_string(number) + " — bir noktanın en az " +
                                 "numarası ve iki koordinatı olmalı; " +
                                 std::to_string(field.size()) + " alan var.");
        }

        // `Y` IS THE EASTING. See the header: this is the one line in this file
        // whose reversal would be both invisible and ruinous.
        const std::size_t first  = order == PointOrder::NumberEastingNorthing ? 1 : 2;
        const std::size_t second = order == PointOrder::NumberEastingNorthing ? 2 : 1;

        auto easting  = parse_metres(field[first]);
        auto northing = parse_metres(field[second]);
        if (!easting || !northing) {
            if (points.empty()) continue; // a header whose columns are words
            return core::err(core::ErrorCode::ParseError,
                             path + ":" + std::to_string(number) + " — koordinat okunamadı ('" +
                                 trimmed(field[first]) + "', '" + trimmed(field[second]) + "').");
        }

        SurveyPoint p;
        p.number = trimmed(field[0]);
        p.at     = core::Point2{easting.value(), northing.value()};

        if (field.size() > 3) {
            if (auto z = parse_metres(field[3]); z) {
                p.height     = z.value();
                p.has_height = true;
            } else {
                // A fourth column that is not a number is a CODE, not a height:
                // plenty of lists carry `no;Y;X;AGAC` with no Z at all.
                p.code = trimmed(field[3]);
            }
        }
        if (field.size() > 4 && p.code.empty()) p.code = trimmed(field[4]);

        points.push_back(std::move(p));
        if (points.size() > kMaxPoints)
            return core::err(core::ErrorCode::ParseError,
                             "Nokta listesi çok büyük: " + std::to_string(kMaxPoints) +
                                 " noktadan fazlası okunmuyor.");
    }

    if (points.empty())
        return core::err(core::ErrorCode::ParseError,
                         "Dosyada okunabilir nokta yok: " + path +
                             ". Beklenen biçim: nokta_no; Y; X; [Z]; [kod]");

    return points;
}

core::Status write_point_list(const std::string& path, const std::vector<SurveyPoint>& points,
                              PointOrder order)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return core::err(core::ErrorCode::IoFailure, "Nokta listesi yazılamadı: " + path);

    // The header names the columns in the order they are written, so the file
    // says what it is without anyone having to know which convention produced it.
    out << (order == PointOrder::NumberEastingNorthing
                ? "# nokta_no; Y(saga); X(yukari); Z; kod\n"
                : "# nokta_no; X(yukari); Y(saga); Z; kod\n");

    const auto metres = [](core::Mm v) {
        const bool negative = v < 0;
        const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
        char buffer[48];
        (void)std::snprintf(buffer, sizeof buffer, "%s%llu.%03llu", negative ? "-" : "",
                            static_cast<unsigned long long>(abs_mm / 1000),
                            static_cast<unsigned long long>(abs_mm % 1000));
        return std::string(buffer);
    };

    for (const SurveyPoint& p : points) {
        const std::string a = metres(order == PointOrder::NumberEastingNorthing ? p.at.x : p.at.y);
        const std::string b = metres(order == PointOrder::NumberEastingNorthing ? p.at.y : p.at.x);

        out << p.number << ';' << a << ';' << b << ';';
        if (p.has_height) out << metres(p.height);
        out << ';' << p.code << '\n';
    }

    if (!out)
        return core::err(core::ErrorCode::IoFailure, "Nokta listesi yazılırken hata: " + path);
    return core::ok();
}

} // namespace kentos::io
