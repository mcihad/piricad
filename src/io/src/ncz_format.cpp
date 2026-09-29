// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): the Netcad NCZ drawing format, parsed.
//
// Copyright (C) 2026 Erdinç Örsan ÜNAL
//     The NCZ parser this file ports: `ncz_pure.py` of his QGIS plugin
//     "NCZ Reader", version 1.4.3,
//     https://github.com/erdincunal/Jeomatik-NCZ-Reader — licensed GPL-2.0-or-later.
// Copyright (C) 2026 KentOSCad contributors
//     The C++ port, 28 September 2026 (GPLv3 §5a: this is a modified version).
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. See /NOTICE for the attribution and the trademark note.
//
// Each function below carries the name of the reference function it ports, so
// the two can be read side by side. The arithmetic is the reference's operation
// for operation — the same products, the same order, the same Python `%`, `min`
// and `max` — because under `-ffp-contract=off` (CLAUDE.md 2.5) that is what
// makes a double come out bit for bit the same. See ncz_format.hpp for the four
// places this port differs on purpose.
#include "ncz_format.hpp"

#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace kentos::io::ncz {
namespace {

constexpr std::uint8_t kLayerTable       = 6;
constexpr std::uint8_t kGeometry         = 21;
constexpr std::uint8_t kGeometryExtended = 22;
constexpr std::uint8_t kVersion          = 25;
constexpr std::uint8_t kNamedData        = 28;
constexpr std::size_t kExtendedHeader    = 28;

/// io.md R15: the stop is asked at least this often, in bytes walked.
constexpr std::size_t kStopStride = std::size_t{4} << 20;

/// `GEOMETRY_MINIMUM_BYTES`, or 0 for a type the reference has no entry for.
constexpr std::size_t minimum_bytes(std::uint8_t type) noexcept
{
    switch (type) {
    case 1: return 87;
    case 2: return 39;
    case 3: return 74;
    case 4: return 120;
    case 5: return 94;
    case 6: return 95;
    case 7: return 113;
    case 9: return 24;
    case 10: return 124;
    case 11: return 82;
    case 12: return 122;
    case 13: return 122;
    case 15: return 90;
    default: return 0;
    }
}

/// `EXTENDED_HEADER_GEOMETRY_TYPES`.
constexpr bool extended_header_type(std::uint8_t type) noexcept
{
    return type == 1 || type == 4 || type == 5 || type == 6 || type == 7 || type == 9 ||
           type == 10 || type == 13;
}

/// `EMBEDDED_GEOMETRY_CONTAINER_TYPES`.
constexpr bool container_type(std::uint8_t type) noexcept
{
    return type == 0 || type == 5 || type == 14 || type == 48 || type == 108 || type == 111 ||
           type == 132 || type == 150 || type == 180;
}

// ------------------------------------------------------ Python arithmetic ----

/// Python's `x % y` for floats: the sign of the divisor, and `+0.0` for an
/// exact zero (CPython `float_rem`). `std::fmod` is exact, so this is too.
double py_mod(double x, double y) noexcept
{
    double mod = std::fmod(x, y);
    if (mod != 0.0 || std::isnan(mod)) {
        if ((y < 0.0) != (mod < 0.0)) mod += y;
    } else {
        mod = std::copysign(0.0, y);
    }
    return mod;
}

/// Python's two-argument `min`: the second only when it is strictly smaller,
/// which decides what a NaN does.
double py_min(double a, double b) noexcept
{
    return b < a ? b : a;
}

/// Python's two-argument `max`, likewise.
double py_max(double a, double b) noexcept
{
    return b > a ? b : a;
}

/// `180.0 / math.pi` and `math.pi / 180.0`, the two constants the reference
/// multiplies by — and what `math.degrees` and `math.radians` multiply by.
constexpr double kRadToDeg = 180.0 / core::kPi;
constexpr double kDegToRad = core::kPi / 180.0;

/// `_is_token_byte`.
constexpr bool token_byte(std::uint8_t v) noexcept
{
    return (v >= '0' && v <= '9') || (v >= 'A' && v <= 'Z') || (v >= 'a' && v <= 'z') ||
           v == '-' || v == '_';
}

/// A byte that `str.strip()` would take as white space once decoded: the
/// ASCII controls 9–13 and 28–31, the space, and U+0085 and U+00A0, which the
/// legacy decoding maps 0x85 and 0xA0 to.
constexpr bool py_space_byte(std::uint8_t v) noexcept
{
    return (v >= 9 && v <= 13) || (v >= 28 && v <= 32) || v == 0x85 || v == 0xA0;
}

/// `_decode_legacy_char`, appended as UTF-8: the six Turkish letters of
/// Windows-1254 the reference names, every other byte as Latin-1.
void append_legacy(std::string& out, std::uint8_t b)
{
    std::uint32_t cp = b;
    switch (b) {
    case 221: cp = 0x0130; break; // `İ`
    case 222: cp = 0x015E; break; // `Ş`
    case 208: cp = 0x011E; break; // `Ğ`
    case 240: cp = 0x011F; break; // `ğ`
    case 253: cp = 0x0131; break; // `ı`
    case 254: cp = 0x015F; break; // `ş`
    default: break;
    }
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

/// The bytes `[from, to)` decoded, with the NUL and the space stripped from
/// both ends first — `str.strip('\x00 ')`, done on the bytes, which is the same
/// thing because both are single bytes in UTF-8 and in the legacy code page.
std::string decode_stripped(const std::uint8_t* from, const std::uint8_t* to)
{
    while (from < to && (*from == 0 || *from == ' '))
        ++from;
    while (to > from && (to[-1] == 0 || to[-1] == ' '))
        --to;
    std::string out;
    out.reserve(static_cast<std::size_t>(to - from));
    for (const std::uint8_t* p = from; p < to; ++p)
        append_legacy(out, *p);
    return out;
}

/// Whether every byte of `[from, to)` decodes to a character at or above 32 or
/// a tab — the reference's `all(ord(char) >= 32 or char == '\t' ...)`.
bool printable_or_tab(const std::uint8_t* from, const std::uint8_t* to) noexcept
{
    for (const std::uint8_t* p = from; p < to; ++p)
        if (*p < 32 && *p != '\t') return false;
    return true;
}

// ------------------------------------------------------------- the tables ----

/// `_to_argb`.
std::uint32_t to_argb(std::uint32_t r, std::uint32_t g, std::uint32_t b) noexcept
{
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/// `_normalize_layer_color`.
std::uint32_t normalize_layer_color(std::uint32_t argb) noexcept
{
    const std::uint32_t red   = (argb >> 16) & 255u;
    const std::uint32_t green = (argb >> 8) & 255u;
    const std::uint32_t blue  = argb & 255u;
    if (red == 0 && green == 0 && blue <= 1) return to_argb(0, 0, 0);
    return argb;
}

/// `_geometry_color` against the tables in `h`.
std::optional<std::uint32_t> geometry_color(const Header& h, std::uint8_t layer_code,
                                                   std::uint8_t color_code) noexcept
{
    if (color_code == 1) return to_argb(0, 0, 255);
    if (color_code == 255) return to_argb(255, 0, 0);
    if (color_code != 0) return std::nullopt;
    const std::size_t n = h.layer_colors.size();
    if (layer_code < n) return normalize_layer_color(h.layer_colors[layer_code]);
    if (layer_code >= 1 && static_cast<std::size_t>(layer_code - 1) < n)
        return normalize_layer_color(h.layer_colors[layer_code - 1]);
    return std::nullopt;
}

/// `_layer_name` against the tables in `h`.
const std::string& layer_name(const Header& h, std::uint8_t layer_code) noexcept
{
    static const std::string none;
    const std::size_t n = h.layer_names.size();
    if (layer_code < n) return h.layer_names[layer_code];
    if (layer_code >= 1 && static_cast<std::size_t>(layer_code - 1) < n)
        return h.layer_names[layer_code - 1];
    return none;
}

// ----------------------------------------------------------------- parser ----

/// Which of the two passes a `Parser` is running.
enum class Mode : std::uint8_t {
    Header,   ///< tables, the declared system, and whether a SmartObject exists
    Entities, ///< everything, each entity handed to the sink
};

/// `_NCZParser`, one pass of it. The fields are the reference's; `final_` is the
/// header the first pass left, standing in for `_finalize_entities`.
class Parser
{
public:
    Parser(std::span<const std::uint8_t> data, Mode mode, Header& header, Sink* sink,
           const Header* final_header)
        : data_(data.data()), size_(data.size()), mode_(mode), header_(header), sink_(sink),
          final_(final_header)
    {
    }

    /// `_scan_blocks`.
    Outcome scan(const std::stop_token& stop)
    {
        std::size_t cursor     = 0;
        std::size_t next_check = 0;
        while (cursor + 5 < size_) {
            if (cursor >= next_check) {
                if (stop.stop_requested()) return Outcome::Cancelled;
                next_check = cursor + kStopStride;
            }
            if (stopped_) return Outcome::Stopped;

            const std::uint64_t block_size = static_cast<std::uint64_t>(u32(cursor + 1)) + 4;
            const std::uint64_t total      = block_size + 1;
            if (block_size < 4 || cursor + total > size_) {
                ++cursor;
                continue;
            }
            const auto size        = static_cast<std::size_t>(block_size);
            const std::uint8_t typ = data_[cursor];
            std::uint64_t advance  = total;
            if (typ == kVersion && header_.version_name.empty()) {
                header_.version_name = legacy_string(cursor + 6, data_[cursor + 5]);
            } else if (typ == kNamedData) {
                parse_named_data_block(cursor, size);
            } else if (typ == kLayerTable) {
                parse_layer_table(cursor, size);
            } else if ((typ == kGeometry || typ == kGeometryExtended) && size >= 7) {
                parse_geometry(cursor, size, typ == kGeometryExtended ? kExtendedHeader : 0);
                advance = std::max<std::uint64_t>(total, smart_extent(cursor));
            } else if (container_type(typ)) {
                if (auto done = parse_embedded_geometry(cursor, size, stop);
                    done != Outcome::Complete)
                    return done;
            } else {
                // A type the reference does not know: searched as a container is
                // (see ncz_format.hpp). Netcad 8's interleaved settings make the
                // walk land on a letter as a "block type", and what follows it is
                // geometry.
                const std::uint64_t before = appended_;
                if (auto done = parse_embedded_geometry(cursor, size, stop);
                    done != Outcome::Complete)
                    return done;
                if (appended_ != before && mode_ == Mode::Entities) {
                    ++header_.swept_blocks;
                    header_.swept_entities += appended_ - before;
                }
            }
            cursor += static_cast<std::size_t>(advance);
        }
        return stopped_ ? Outcome::Stopped : Outcome::Complete;
    }

private:
    // ---- bytes ---------------------------------------------------------------

    std::uint8_t byte(std::size_t at) const noexcept { return at < size_ ? data_[at] : 0; }

    /// `_read_uint32`: little-endian, and — like `int.from_bytes` over a short
    /// slice — whatever bytes the file still has.
    std::uint32_t u32(std::size_t at) const noexcept
    {
        std::uint32_t v = 0;
        for (std::size_t k = 0; k < 4 && at + k < size_; ++k)
            v |= static_cast<std::uint32_t>(data_[at + k]) << (8 * k);
        return v;
    }

    /// `_read_float64`. Every call site reads inside the bytes the minimum-size
    /// check has already proved present; the bound is for a fuzzer, not a file.
    double f64(std::size_t at) const noexcept
    {
        if (at > size_ || size_ - at < 8) return std::nan("");
        double v = 0.0;
        std::memcpy(&v, data_ + at, sizeof v);
        return v;
    }

    /// `_read_float32`, widened exactly as `struct.unpack_from('<f')` widens it.
    double f32(std::size_t at) const noexcept
    {
        if (at > size_ || size_ - at < 4) return std::nan("");
        float v = 0.0F;
        std::memcpy(&v, data_ + at, sizeof v);
        return static_cast<double>(v);
    }

    /// `_read_legacy_string`: at most `length` bytes, cut at the end of the
    /// file, decoded, trailing NULs stripped.
    std::string legacy_string(std::size_t offset, std::size_t length) const
    {
        if (offset >= size_) return {};
        const std::size_t end = std::min(size_, offset + length);
        std::size_t last      = end;
        while (last > offset && data_[last - 1] == 0)
            --last;
        std::string out;
        out.reserve(last - offset);
        for (std::size_t i = offset; i < last; ++i)
            append_legacy(out, data_[i]);
        return out;
    }

    /// The same bytes `legacy_string` decodes, as a range: for the tests that
    /// the reference makes on the decoded characters.
    std::pair<std::size_t, std::size_t> legacy_range(std::size_t offset,
                                                     std::size_t length) const noexcept
    {
        if (offset >= size_) return {offset, offset};
        const std::size_t end = std::min(size_, offset + length);
        std::size_t last      = end;
        while (last > offset && data_[last - 1] == 0)
            --last;
        return {offset, last};
    }

    /// `_read_positive_float`.
    std::optional<double> positive_float(std::size_t offset) const noexcept
    {
        if (offset > size_ || size_ - offset < 4) return std::nullopt;
        const double value = f32(offset);
        if (!std::isfinite(value) || value <= 0.0 || value > 100000.0) return std::nullopt;
        return value;
    }

    /// `_read_length_prefixed_text`.
    std::string length_prefixed_text(std::size_t length_offset, std::size_t text_offset) const
    {
        if (length_offset >= size_ || text_offset >= size_) return {};
        const std::size_t text_length = data_[length_offset];
        if (text_length == 0 || text_length > 240 || text_offset + text_length > size_) return {};
        const auto [from, to] = legacy_range(text_offset, text_length);
        return decode_stripped(data_ + from, data_ + to);
    }

    /// `_read_text_payload`.
    std::string text_payload(std::size_t offset, std::size_t ext) const
    {
        std::string text = length_prefixed_text(offset + ext + 97, offset + ext + 98);
        if (!text.empty()) return text;
        text = length_prefixed_text(offset + ext + 86, offset + ext + 87);
        if (!text.empty()) return text;
        text = length_prefixed_text(offset + 97, offset + 98);
        if (!text.empty()) return text;
        return length_prefixed_text(offset + 86, offset + 87);
    }

    /// `_read_length_prefixed_name`.
    std::string length_prefixed_name(std::size_t start, std::size_t end) const
    {
        const std::size_t bounded_end = std::min(end, size_);
        const std::size_t stop_at     = bounded_end >= 2 ? bounded_end - 2 : 0;
        for (std::size_t index = start; index < stop_at; ++index) {
            const std::size_t value_length = data_[index];
            if (value_length == 0 || value_length > 64 || index + 1 + value_length > bounded_end)
                continue;
            const auto [from, to] = legacy_range(index + 1, value_length);
            const std::uint8_t* a = data_ + from;
            const std::uint8_t* b = data_ + to;
            while (a < b && (*a == 0 || *a == ' '))
                ++a;
            while (b > a && (b[-1] == 0 || b[-1] == ' '))
                --b;
            if (a < b && printable_or_tab(a, b)) return decode_stripped(a, b);
        }
        return {};
    }

    /// `_read_ascii_token`.
    std::string ascii_token(std::size_t start, std::size_t end) const
    {
        const std::size_t bounded_end = std::min(end, size_);
        std::size_t cursor            = start;
        while (cursor < bounded_end) {
            if (!token_byte(data_[cursor])) {
                ++cursor;
                continue;
            }
            const std::size_t token_start = cursor;
            while (cursor < bounded_end && token_byte(data_[cursor]))
                ++cursor;
            if (cursor - token_start >= 3)
                return std::string(reinterpret_cast<const char*>(data_ + token_start),
                                   cursor - token_start);
        }
        return {};
    }

    /// `_read_plan_box_name`.
    std::string plan_box_name(std::size_t offset, std::size_t block_size) const
    {
        const std::size_t end   = std::min(size_, offset + block_size + 1);
        const std::size_t limit = end >= 4 ? std::max(offset, end - 4) : offset;
        for (std::size_t index = offset; index < limit; ++index) {
            // `bytes.lower()` folds ASCII A–Z only.
            const auto low = [&](std::size_t k) {
                const std::uint8_t c = data_[index + k];
                return static_cast<std::uint8_t>(c >= 'A' && c <= 'Z' ? c + 32 : c);
            };
            if (low(0) != 'p' || low(1) != 'l' || low(2) != 'a' || low(3) != 'n') continue;
            std::size_t cursor = index + 4;
            while (cursor < end && cursor - index < 32 && token_byte(data_[cursor]))
                ++cursor;
            if (cursor <= index + 4) continue;
            bool digits = true;
            for (std::size_t k = index + 4; k < cursor; ++k)
                digits = digits && data_[k] >= '0' && data_[k] <= '9';
            if (digits)
                return std::string(reinterpret_cast<const char*>(data_ + index), cursor - index);
        }
        return {};
    }

    /// `_read_epsg`: from the first `SRS` to the next `>` — which the reference
    /// looks for to the end of the FILE, not of the block, and so does this.
    std::string read_epsg(std::size_t offset, std::size_t max_length) const
    {
        const std::size_t tries = max_length >= 3 ? max_length - 3 : 0;
        for (std::size_t index = 0; index < tries; ++index) {
            if (offset + index + 2 >= size_) break;
            const std::uint8_t* p = data_ + offset + index;
            if (p[0] != 'S' || p[1] != 'R' || p[2] != 'S') continue;
            std::string chars;
            for (std::size_t c = offset + index; c < size_ && data_[c] != '>'; ++c)
                append_legacy(chars, data_[c]);
            // `.replace('SRS:', '').replace('"', '')`
            std::string out;
            out.reserve(chars.size());
            for (std::size_t k = 0; k < chars.size();) {
                if (chars.compare(k, 4, "SRS:") == 0) {
                    k += 4;
                    continue;
                }
                out.push_back(chars[k]);
                ++k;
            }
            std::erase(out, '"');
            return out;
        }
        return {};
    }

    // ---- Netcad 8 SmartObjects ---------------------------------------------

    /// A length or a count in the property block: 7-bit groups, least
    /// significant first, as Delphi's streaming writes them.
    static bool varint(const std::uint8_t* p, std::size_t end, std::size_t& at,
                       std::size_t& value) noexcept
    {
        value = 0;
        for (int shift = 0; shift < 28; shift += 7) {
            if (at >= end) return false;
            const std::uint8_t c = p[at++];
            value |= static_cast<std::size_t>(c & 0x7F) << shift;
            if (c < 0x80) return true;
        }
        return false;
    }

    /// The property block Netcad 8 writes after a SmartObject's geometry, or
    /// `false` when the record carries none (a Netcad 5 SmartObject).
    ///
    /// THE LAYOUT, worked out from 15 722 objects of a real UİP — nothing
    /// publishes it. At +94 the class GUID; at +110 `-2` as an int32 and at +114
    /// the length of the record from +110, which runs 81 bytes PAST the size
    /// the record's own header declares — which is why the block walk never saw
    /// it; on every one of those objects the next record starts exactly there.
    /// At +118 a version byte, the GUID again, four constant bytes, and at +139
    /// the property count; the list ends four bytes before the record does. Then each property: its name, a type byte, its value,
    /// the label Netcad shows for it — each a length and bytes, the lengths
    /// 7-bit varints — and seven bytes of flags whose last says whether the
    /// value is the user's. Every value is text. A record that does not hold
    /// together — a count past the payload, a GUID that does not repeat — is not
    /// one, and is read the reference's way.
    /// Where a Netcad 8 SmartObject really ends, from `offset`: past its
    /// property block, which the record's header does not count; 0 for any
    /// other record. The walk resumes there, so the block's last 89 bytes are
    /// never read as blocks of their own — which, at the top of a file, they
    /// would be, and a "block" found in a property's text swallows the objects
    /// after it. (The record's header declares 81 bytes fewer.)
    std::size_t smart_extent(std::size_t offset) const noexcept
    {
        if (offset + 143 > size_ || data_[offset + 6] != 15) return 0;
        const std::uint8_t* p = data_ + offset;
        if (u32(offset + 110) != 0xFFFFFFFEu) return 0;
        if (std::memcmp(p + 94, p + 119, 16) != 0) return 0;
        if (u32(offset + 139) > 4096) return 0;
        return std::min(size_ - offset, std::size_t{110} + u32(offset + 114));
    }

    bool smart_properties(std::size_t offset, Entity& e) const
    {
        const std::size_t end = smart_extent(offset);
        if (end < 143) return false;
        const std::uint8_t* p   = data_ + offset;
        const std::size_t count = u32(offset + 139);

        static constexpr std::array<std::pair<const char*, SmartClass>, 5> kClasses{{
            {"89383f961207d34fba5a4e5f523cf239", SmartClass::Road},
            {"7b865f8e160c474cbc7b523f411216df", SmartClass::Settlement},
            {"3ed066ec376b7041ad0eebca2d350e10", SmartClass::Construction},
            {"f86f2b1e0c328441843d2c566b316d5c", SmartClass::PlanNote},
            {"89897e489d60c84388835aaf333c3074", SmartClass::FunctionName},
        }};
        static constexpr char kHex[] = "0123456789abcdef";
        std::string guid;
        for (std::size_t k = 94; k < 110; ++k) {
            guid.push_back(kHex[p[k] >> 4]);
            guid.push_back(kHex[p[k] & 15]);
        }
        e.smart = SmartClass::Other;
        for (const auto& [id, cls] : kClasses)
            if (guid == id) e.smart = cls;

        std::vector<SmartProperty> props;
        std::size_t at = 143;
        const auto text = [&](std::string& out) {
            std::size_t n = 0;
            if (!varint(p, end, at, n) || n > end - at) return false;
            out.assign(reinterpret_cast<const char*>(p + at), n);
            at += n;
            return true;
        };
        for (std::size_t k = 0; k < count; ++k) {
            SmartProperty prop;
            if (!text(prop.name)) return false;
            if (at >= end) return false;
            ++at; // the type byte: every value is text whatever it says
            if (!text(prop.value) || !text(prop.display)) return false;
            if (at + 7 > end) return false;
            prop.user = p[at + 6] == 1;
            at += 7;
            props.push_back(std::move(prop));
        }

        // `chkKatIsNull = True` switches `kat` off; the flags themselves are not
        // properties of the symbol. `txtOn` pairs with `chkOnIsNull`, and the
        // case differs (`yEncok`, `chkYEnCokIsNull`), so the match is ASCII
        // case-insensitive on the name without its `txt`.
        const auto lower = [](std::string_view v) {
            std::string out(v);
            for (char& ch : out)
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            return out;
        };
        std::vector<std::string> nulls;
        for (const SmartProperty& prop : props) {
            const std::string_view n = prop.name;
            if (n.size() > 9 && n.substr(0, 3) == "chk" && n.substr(n.size() - 6) == "IsNull" &&
                prop.value == "True")
                nulls.push_back(lower(n.substr(3, n.size() - 9)));
        }
        for (SmartProperty& prop : props) {
            const std::string_view n = prop.name;
            if (n.size() > 9 && n.substr(0, 3) == "chk" && n.substr(n.size() - 6) == "IsNull")
                continue;
            std::string key = lower(n);
            if (key.size() > 3 && key.substr(0, 3) == "txt") key = key.substr(3);
            prop.null = std::find(nulls.begin(), nulls.end(), key) != nulls.end();
            e.properties.push_back(std::move(prop));
        }
        return true;
    }

    // ---- values ------------------------------------------------------------

    /// `_valid_xy`.
    static bool valid_xy(double x, double y) noexcept
    {
        return std::isfinite(x) && std::isfinite(y) && std::fabs(x) <= 100000000.0 &&
               std::fabs(y) <= 100000000.0;
    }

    /// `_coordinate`: the file's northing first, easting second, swapped.
    static Coord coordinate(double raw_x, double raw_y, double z) noexcept
    {
        return Coord{raw_y, raw_x, z};
    }

    // ---- blocks ------------------------------------------------------------

    /// `_parse_named_data_block`.
    void parse_named_data_block(std::size_t offset, std::size_t block_size)
    {
        const std::size_t block_end = std::min(size_, offset + block_size + 1);
        if (offset + 6 > block_end) return;

        const std::string block_name = legacy_string(offset + 6, data_[offset + 5]);
        if (block_name == "MPROJ" && offset + 22 <= block_end) {
            const std::uint8_t projection = data_[offset + 16];
            const std::uint8_t datum      = data_[offset + 17];
            const std::uint8_t zone       = data_[offset + 21];
            const char* p = projection == 1 ? "Geographic"
                            : projection == 2 ? "6"
                            : projection == 3 ? "3"
                                              : "Undefined";
            const char* d = datum == 0     ? "WGS-84"
                            : datum == 1   ? "ITRF"
                            : datum == 4   ? "ED50"
                            : datum == 254 ? "ED50-HGK"
                                           : "Undefined";
            header_.projection_text =
                std::string(d) + " / " + p + " / Zone " + std::to_string(zone);
            header_.mproj      = true;
            header_.projection = projection;
            header_.datum      = datum;
            header_.zone       = zone;
        } else if (block_name == "TILED_XML") {
            header_.epsg = read_epsg(offset, block_end - offset);
        } else if (block_name == "LEX.ST2" && offset + 21 <= block_end) {
            const std::size_t layer_count = data_[offset + 20];
            for (std::size_t index = 0; index < layer_count; ++index) {
                const std::size_t item = offset + 79 + index * 256;
                if (item + 3 > block_end) break;
                header_.layer_colors.push_back(
                    to_argb(data_[item], data_[item + 1], data_[item + 2]));
            }
        }
    }

    /// `_parse_layer_table`.
    void parse_layer_table(std::size_t offset, std::size_t block_size)
    {
        const std::size_t block_end = std::min(size_, offset + block_size + 1);
        if (offset + 18 > block_end) return;

        const std::size_t layer_count =
            static_cast<std::size_t>(data_[offset + 16]) + data_[offset + 17] * std::size_t{256};
        for (std::size_t index = 0; index < layer_count; ++index) {
            const std::size_t item = offset + 18 + index * 29;
            if (item + 29 > block_end) break;
            const auto [from, to] = legacy_range(item + 5, data_[item + 4]);
            // `layer_name.strip()`: kept when anything but white space is left.
            bool blank = true;
            for (std::size_t k = from; k < to && blank; ++k)
                blank = py_space_byte(data_[k]);
            if (!blank) header_.layer_names.push_back(legacy_string(item + 5, data_[item + 4]));
        }
    }

    /// `_parse_embedded_geometry`. The inner walk ends at `offset + block_size`
    /// — one byte short of where the outer walk says the block ends, as in the
    /// reference.
    Outcome parse_embedded_geometry(std::size_t offset, std::size_t block_size,
                                    const std::stop_token& stop)
    {
        std::size_t cursor     = offset + 5;
        const std::size_t end  = std::min(size_, offset + block_size);
        std::size_t next_check = cursor + kStopStride;
        while (cursor + 6 < end) {
            if (cursor >= next_check) {
                if (stop.stop_requested()) return Outcome::Cancelled;
                next_check = cursor + kStopStride;
            }
            if (stopped_) return Outcome::Stopped;

            const bool is_geometry = data_[cursor] == kGeometry || data_[cursor] == kGeometryExtended;
            const bool matching    = data_[cursor + 5] == data_[cursor + 6];
            if (!is_geometry || !matching) {
                ++cursor;
                continue;
            }
            const std::uint64_t inner = static_cast<std::uint64_t>(u32(cursor + 1)) + 4;
            const std::uint64_t total = inner + 1;
            if (inner < 7 || cursor + total > end) {
                ++cursor;
                continue;
            }
            parse_geometry(cursor, static_cast<std::size_t>(inner),
                           data_[cursor] == kGeometryExtended ? kExtendedHeader : 0);
            cursor += static_cast<std::size_t>(std::max<std::uint64_t>(total, smart_extent(cursor)));
        }
        return Outcome::Complete;
    }

    /// `_parse_geometry`.
    void parse_geometry(std::size_t offset, std::size_t block_size, std::size_t ext)
    {
        if (block_size < 7 || offset + 6 >= size_) return;
        const std::uint8_t type = data_[offset + 6];

        // The first pass needs one fact from the geometry: whether a
        // SmartObject exists. Everything else waits for the second.
        if (mode_ == Mode::Header && type != 15) return;

        std::size_t minimum = minimum_bytes(type);
        if (minimum != 0) {
            if (extended_header_type(type)) minimum += ext;
            if (block_size + 1 < minimum) {
                drop(type);
                return;
            }
        }
        const std::uint64_t before = appended_;
        record_ = offset;
        switch (type) {
        case 1: parse_point(offset, ext); break;
        case 2: parse_line(offset, block_size); break;
        case 3: parse_circle(offset); break;
        case 4: parse_arc(offset, ext); break;
        case 5: parse_text(offset, ext); break;
        case 6: parse_symbol(offset, block_size, ext); break;
        case 7: parse_multiline(offset, block_size, ext); break;
        case 9: parse_compressed_curve(offset, block_size, ext); break;
        case 10: parse_box(offset, block_size, ext); break;
        case 11: parse_map_sheet(offset, block_size); break;
        case 12: parse_triangle(offset); break;
        case 13: parse_block_reference(offset, block_size, ext); break;
        case 15: parse_smart_object(offset, block_size); break;
        default:
            if (mode_ == Mode::Entities) ++header_.unsupported[type];
            return;
        }
        if (appended_ == before) drop(type);
    }

    void drop(std::uint8_t type) noexcept
    {
        if (mode_ == Mode::Entities) ++header_.dropped[type];
    }

    // ---- entities ------------------------------------------------------------

    /// `_parse_point`.
    void parse_point(std::size_t offset, std::size_t ext)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x            = f64(offset + 8);
        const double raw_y            = f64(offset + 16);
        double z                      = f32(offset + 24);
        if (z == 0.0) z = f32(offset + 28);
        if (!valid_xy(raw_x, raw_y)) return;
        Entity& e = begin(Kind::Point);
        e.coords.push_back(coordinate(raw_x, raw_y, z));
        e.name = legacy_string(offset + ext + 87, byte(offset + ext + 86));
        e.set |= kName;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_line`.
    void parse_line(std::size_t offset, std::size_t block_size)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x1           = f64(offset + 8);
        const double raw_y1           = f64(offset + 16);
        const double z1               = f32(offset + 24);
        const double raw_x2           = f64(offset + block_size - 19);
        const double raw_y2           = f64(offset + block_size - 11);
        const double z2               = f32(offset + block_size - 3);
        if (!valid_xy(raw_x1, raw_y1) || !valid_xy(raw_x2, raw_y2)) return;
        Entity& e = begin(Kind::Line);
        e.coords.push_back(coordinate(raw_x1, raw_y1, z1));
        e.coords.push_back(coordinate(raw_x2, raw_y2, z2));
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_multiline`.
    void parse_multiline(std::size_t offset, std::size_t block_size, std::size_t ext)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        std::string text              = legacy_string(offset + ext + 87, byte(offset + ext + 86));
        // Floor division of a non-negative number: the minimum-size check made
        // `block_size + 1` at least `113 + ext`.
        const std::size_t point_count = (block_size + 1 - 113 - ext) / 24;
        if (point_count < 2) return;
        const std::size_t block_end = std::min(size_, offset + block_size + 1);

        Entity& e = begin(Kind::Polyline);
        e.coords.reserve(point_count + 1);
        for (std::size_t index = 0; index < point_count; ++index) {
            const std::size_t at = index * 24 + (offset + ext + 113);
            if (at + 24 > block_end) break;
            e.coords.push_back(coordinate(f64(at), f64(at + 8), f64(at + 16)));
        }
        if (e.coords.size() < 2) return;

        const bool is_closed = nearly_closed(e.coords);
        if (is_closed && !same_coordinate(e.coords.front(), e.coords.back()))
            e.coords.push_back(e.coords.front());
        double width      = 0.0;
        double height     = 0.0;
        double rotation   = 0.0;
        const bool is_box = box_metrics(e.coords, width, height, rotation);

        e.kind       = is_closed ? Kind::Polygon : Kind::Polyline;
        e.label      = std::move(text);
        e.closed     = is_closed;
        e.box_width  = width;
        e.box_height = height;
        e.rotation   = is_box ? rotation : 0.0;
        e.set |= kLabel | kClosed | kBoxWidth | kBoxHeight | kRotation;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_compressed_curve`.
    void parse_compressed_curve(std::size_t offset, std::size_t block_size, std::size_t ext)
    {
        const double origin_x = f64(offset + 8);
        const double origin_y = f64(offset + 16);
        if (!valid_xy(origin_x, origin_y)) return;
        const std::size_t point_data = offset + ext + 122;
        const std::size_t end_offset = offset + block_size + 1;
        if (point_data + 8 > end_offset) return;

        Entity& e      = begin(Kind::Polyline);
        int invalid    = 0;
        const auto bad = [&] {
            ++invalid;
            return !e.coords.empty() && invalid >= 4;
        };
        for (std::size_t at = point_data; at < end_offset - 7; at += 18) {
            const double delta_x = f32(at);
            const double delta_y = f32(at + 4);
            if (!std::isfinite(delta_x) || !std::isfinite(delta_y)) {
                if (bad()) break;
                continue;
            }
            const double x = origin_x + delta_x;
            const double y = origin_y + delta_y;
            if (!valid_xy(x, y)) {
                if (bad()) break;
                continue;
            }
            invalid          = 0;
            const Coord here = coordinate(x, y, 0.0);
            if (!e.coords.empty()) {
                const Coord& prev = e.coords.back();
                if (std::fabs(prev.x - here.x) < 0.0001 && std::fabs(prev.y - here.y) < 0.0001)
                    continue;
            }
            e.coords.push_back(here);
        }
        if (e.coords.size() < 2) return;
        append(data_[offset + 7], data_[offset + 37]);
    }

    /// `_parse_circle`.
    void parse_circle(std::size_t offset)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x            = f64(offset + 8);
        const double raw_y            = f64(offset + 16);
        const double z                = f32(offset + 24);
        if (!valid_xy(raw_x, raw_y)) return;
        const double x2 = f64(offset + 50);
        const double x3 = f64(offset + 66);
        Entity& e       = begin(Kind::Circle);
        e.coords.push_back(coordinate(raw_x, raw_y, z));
        e.radius = std::fabs(x2 - x3) / 2.0;
        e.set |= kRadius;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_arc`.
    void parse_arc(std::size_t offset, std::size_t ext)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x            = f64(offset + 8);
        const double raw_y            = f64(offset + 16);
        const double z                = f32(offset + 24);
        if (!valid_xy(raw_x, raw_y)) return;
        Entity& e = begin(Kind::Arc);
        e.coords.push_back(coordinate(raw_x, raw_y, z));
        e.radius      = f64(offset + ext + 86);
        e.start_angle = f64(offset + ext + 104);
        e.end_angle   = f64(offset + ext + 112);
        e.set |= kRadius | kStartAngle | kEndAngle;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_text`.
    void parse_text(std::size_t offset, std::size_t ext)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x            = f64(offset + 8);
        const double raw_y            = f64(offset + 16);
        double z                      = f32(offset + 24);
        if (z == 0.0) z = f32(offset + 28);
        if (!valid_xy(raw_x, raw_y)) return;
        std::string text = text_payload(offset, ext);
        if (text.empty()) return;
        std::optional<double> height = positive_float(offset + ext + 86);
        if (!height) height = positive_float(offset + 86);
        if (!height) return;
        const double rotation = py_mod(f32(offset + ext + 90) * kRadToDeg, 360.0);
        Entity& e             = begin(Kind::Text);
        e.coords.push_back(coordinate(raw_x, raw_y, z));
        e.label       = std::move(text);
        e.text_height = *height;
        e.rotation    = rotation;
        e.set |= kLabel | kTextHeight | kRotation;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_symbol`.
    void parse_symbol(std::size_t offset, std::size_t block_size, std::size_t ext)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x            = f64(offset + 8);
        const double raw_y            = f64(offset + 16);
        const double z                = f32(offset + 24);
        if (!valid_xy(raw_x, raw_y)) return;
        const std::size_t block_end = std::min(size_, offset + block_size + 1);
        std::size_t symbol_offset   = offset + ext + 94;
        if (symbol_offset >= block_end) symbol_offset = offset + 94;
        const std::uint8_t code = symbol_offset < block_end ? data_[symbol_offset] : 0;
        std::optional<double> size = positive_float(offset + ext + 86);
        if (!size) size = positive_float(offset + 86);
        const double rotation = py_mod(f32(offset + ext + 90) * kRadToDeg, 360.0);
        Entity& e             = begin(Kind::Symbol);
        e.coords.push_back(coordinate(raw_x, raw_y, z));
        e.label       = "S" + std::to_string(code);
        e.text_height = size ? *size : 5.0;
        e.rotation    = rotation;
        e.set |= kLabel | kTextHeight | kRotation;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_block_reference`.
    void parse_block_reference(std::size_t offset, std::size_t block_size, std::size_t ext)
    {
        const double raw_x = f64(offset + 8);
        const double raw_y = f64(offset + 16);
        const double z     = f32(offset + 24);
        if (!valid_xy(raw_x, raw_y)) return;
        std::string name      = length_prefixed_name(offset + ext + 86, offset + block_size + 1);
        const double rotation = py_mod(f32(offset + ext + 118) * kRadToDeg, 360.0);
        Entity& e             = begin(Kind::Block);
        e.coords.push_back(coordinate(raw_x, raw_y, z));
        e.label    = std::move(name);
        e.rotation = rotation;
        e.set |= kLabel | kRotation;
        append(data_[offset + 7], data_[offset + 37]);
    }

    /// `_parse_box`: a rectangle from its bottom-left corner, the far corner,
    /// and a rotation stored in radians.
    void parse_box(std::size_t offset, std::size_t block_size, std::size_t ext)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x1           = f64(offset + 8);
        const double raw_y1           = f64(offset + 16);
        const double raw_x2           = f64(offset + ext + 104);
        const double raw_y2           = f64(offset + ext + 112);
        const double rotation_radians = f32(offset + ext + 120);
        if (!valid_xy(raw_x1, raw_y1) || !valid_xy(raw_x2, raw_y2)) return;
        const double width            = std::fabs(raw_x2 - raw_x1);
        const double height           = std::fabs(raw_y2 - raw_y1);
        const double rotation_degrees = py_mod(rotation_radians * kRadToDeg, 360.0);
        const double angle_radians    = rotation_degrees * kDegToRad;
        // A NaN rotation — a float the file does not hold as a number — gives NaN
        // corners, as `math.sin(nan)` does; the reader then refuses the entity.
        const core::SinCos t = std::isfinite(angle_radians)
                                   ? core::sin_cos_rad(angle_radians)
                                   : core::SinCos{std::nan(""), std::nan("")};
        const double side_x           = t.sin;
        const double side_y           = t.cos;
        const double bottom_x         = t.cos;
        const double bottom_y         = -t.sin;
        const double p0x              = raw_x1;
        const double p0y              = raw_y1;
        const double p1x              = p0x + bottom_x * width;
        const double p1y              = p0y + bottom_y * width;
        const double p2x              = p1x + side_x * height;
        const double p2y              = p1y + side_y * height;
        const double p3x              = p0x + side_x * height;
        const double p3y              = p0y + side_y * height;

        Entity& e = begin(Kind::Polygon);
        e.coords.push_back(coordinate(p0x, p0y, 0.0));
        e.coords.push_back(coordinate(p1x, p1y, 0.0));
        e.coords.push_back(coordinate(p2x, p2y, 0.0));
        e.coords.push_back(coordinate(p3x, p3y, 0.0));
        e.coords.push_back(coordinate(p0x, p0y, 0.0));
        e.closed     = true;
        e.box_width  = width;
        e.box_height = height;
        e.rotation   = rotation_degrees;
        e.label      = plan_box_name(offset, block_size);
        e.set |= kClosed | kBoxWidth | kBoxHeight | kRotation | kLabel;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_map_sheet`: a pafta frame, axis-aligned, with its sheet name.
    void parse_map_sheet(std::size_t offset, std::size_t block_size)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const double raw_x1           = f64(offset + 50);
        const double raw_y1           = f64(offset + 58);
        const double raw_x2           = f64(offset + 66);
        const double raw_y2           = f64(offset + 74);
        if (!valid_xy(raw_x1, raw_y1) || !valid_xy(raw_x2, raw_y2)) return;
        const double min_x = py_min(raw_x1, raw_x2);
        const double max_x = py_max(raw_x1, raw_x2);
        const double min_y = py_min(raw_y1, raw_y2);
        const double max_y = py_max(raw_y1, raw_y2);
        if (std::fabs(max_x - min_x) < 0.001 || std::fabs(max_y - min_y) < 0.001) return;
        std::string sheet = length_prefixed_name(offset + 86, offset + block_size + 1);

        Entity& e = begin(Kind::MapSheet);
        e.coords.push_back(coordinate(min_x, min_y, 0.0));
        e.coords.push_back(coordinate(max_x, min_y, 0.0));
        e.coords.push_back(coordinate(max_x, max_y, 0.0));
        e.coords.push_back(coordinate(min_x, max_y, 0.0));
        e.coords.push_back(coordinate(min_x, min_y, 0.0));
        e.closed     = true;
        e.box_width  = max_x - min_x;
        e.box_height = max_y - min_y;
        e.label      = std::move(sheet);
        e.set |= kClosed | kBoxWidth | kBoxHeight | kLabel;
        append(layer_code, data_[offset + 37]);
    }

    /// `_parse_triangle_vertex`.
    std::optional<Coord> triangle_vertex(std::size_t offset, std::size_t x_offset,
                                         std::size_t y_offset, std::size_t z_offset,
                                         bool has_z) const noexcept
    {
        if (offset + x_offset + 8 > size_ || offset + y_offset + 8 > size_) return std::nullopt;
        const double x = f64(offset + x_offset);
        const double y = f64(offset + y_offset);
        double z       = 0.0;
        if (has_z && offset + z_offset + 4 <= size_) z = f32(offset + z_offset);
        if (!valid_xy(x, y)) return std::nullopt;
        return coordinate(x, y, z);
    }

    /// `_parse_triangle`.
    void parse_triangle(std::size_t offset)
    {
        const auto a = triangle_vertex(offset, 8, 16, 24, true);
        const auto b = triangle_vertex(offset, 86, 94, 0, false);
        const auto c = triangle_vertex(offset, 106, 114, 0, false);
        if (!a || !b || !c) return;
        const double area2 =
            std::fabs((b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x));
        if (area2 <= 0.0001) return;
        Entity& e = begin(Kind::Triangle);
        e.coords.push_back(*a);
        e.coords.push_back(*b);
        e.coords.push_back(*c);
        append(data_[offset + 7], data_[offset + 37]);
    }

    /// `_parse_smart_object`: Netcad's types 11 and 15 — a rotated rectangle
    /// with a width, a height, a grid and a scale. Its rotation is in GRADS.
    void parse_smart_object(std::size_t offset, std::size_t block_size)
    {
        const std::uint8_t layer_code = data_[offset + 7];
        const std::size_t block_end   = std::min(size_, offset + block_size + 1);
        const double raw_x1           = f64(offset + 8);
        const double raw_y1           = f64(offset + 16);
        if (!valid_xy(raw_x1, raw_y1)) return;
        double width        = offset + 177 <= block_end ? f64(offset + 169) : 0.0;
        double height       = offset + 185 <= block_end ? f64(offset + 177) : 0.0;
        const double grid_x = offset + 193 <= block_end ? f64(offset + 185) : 0.0;
        const double grid_y = offset + 201 <= block_end ? f64(offset + 193) : 0.0;
        const double raw_x2 = f64(offset + 66);
        const double raw_y2 = f64(offset + 74);
        if (width <= 0.0 || height <= 0.0) {
            // No far corner either: the reference drops the object here, and the
            // size stays zero, which keeps it as the point below.
            if (valid_xy(raw_x2, raw_y2)) {
                width  = std::fabs(raw_x2 - raw_x1);
                height = std::fabs(raw_y2 - raw_y1);
            } else {
                width  = 0.0;
                height = 0.0;
            }
        }
        // NO RECTANGLE: a plan notation anchored at a point — no size, or a
        // size field that holds something else (Netcad 8 writes 10^222 there
        // for some kinds). The reference drops the first and draws the second
        // past the edge of the world; this port keeps their place and their
        // label (see ncz_format.hpp). What the reference would have KEPT still
        // decides the `S0` rule, so that stays the reference's.
        const bool reference_keeps = !(width < 0.001 || height < 0.001);
        const bool rectangle       = width >= 0.001 && height >= 0.001 && width <= 100000000.0 &&
                               height <= 100000000.0;
        const bool point           = !rectangle;
        const double angle_grads = f32(offset + 82);
        const double rotation_degrees =
            std::isfinite(angle_grads) ? py_mod(angle_grads * 0.9, 360.0) : 0.0;
        double scale = f32(offset + 86);
        if (!std::isfinite(scale)) scale = 0.0;

        if (mode_ == Mode::Header) {
            // Everything that could still refuse the object has been asked. Only
            // an object the reference itself keeps counts for the `S0` rule.
            if (reference_keeps) header_.smart_object = true;
            ++appended_;
            return;
        }

        static constexpr std::string_view kBasicLabel = "BASIC";

        // A NETCAD 8 SMARTOBJECT: a Planet symbol with its properties. Its
        // anchor, its size and its turn are all the reader needs to draw it.
        {
            Entity& e = begin(Kind::SmartObject);
            if (smart_properties(offset, e)) {
                e.coords.push_back(coordinate(raw_x1, raw_y1, 0.0));
                e.rotation = rotation_degrees;
                e.scale    = scale;
                e.label    = smart_class_name(e.smart);
                e.set |= kRotation | kScale | kLabel;
                ++header_.planet_symbols;
                append(layer_code, data_[offset + 37]);
                return;
            }
        }

        if (point) {
            const std::string_view whole(reinterpret_cast<const char*>(data_ + offset),
                                         block_end - offset);
            std::string label = whole.find(kBasicLabel) != std::string_view::npos
                                    ? std::string(kBasicLabel)
                                    : ascii_token(offset + 145, block_end);
            Entity& e = begin(Kind::SmartObject);
            e.coords.push_back(coordinate(raw_x1, raw_y1, 0.0));
            e.rotation = rotation_degrees;
            e.scale    = scale;
            e.grid_x   = grid_x;
            e.grid_y   = grid_y;
            e.label    = std::move(label);
            e.set |= kRotation | kScale | kGridX | kGridY | kLabel;
            ++header_.point_smart_objects;
            append(layer_code, data_[offset + 37]);
            return;
        }

        const core::SinCos t  = core::sin_cos_rad(rotation_degrees * kDegToRad);
        const double bottom_x = t.sin;
        const double bottom_y = t.cos;
        const double side_x   = t.cos;
        const double side_y   = -t.sin;
        const double p0x      = raw_x1;
        const double p0y      = raw_y1;
        const double p1x      = p0x + bottom_x * width;
        const double p1y      = p0y + bottom_y * width;
        const double p2x      = p1x + side_x * height;
        const double p2y      = p1y + side_y * height;
        const double p3x      = p0x + side_x * height;
        const double p3y      = p0y + side_y * height;

        const std::string_view payload(reinterpret_cast<const char*>(data_ + offset),
                                       block_end - offset);
        std::string label = payload.find(kBasicLabel) != std::string_view::npos
                                ? std::string(kBasicLabel)
                                : ascii_token(offset + 145, block_end);

        Entity& e = begin(Kind::SmartObject);
        e.coords.push_back(coordinate(p0x, p0y, 0.0));
        e.coords.push_back(coordinate(p1x, p1y, 0.0));
        e.coords.push_back(coordinate(p2x, p2y, 0.0));
        e.coords.push_back(coordinate(p3x, p3y, 0.0));
        e.coords.push_back(coordinate(p0x, p0y, 0.0));
        e.closed     = true;
        e.box_width  = width;
        e.box_height = height;
        e.rotation   = rotation_degrees;
        e.scale      = scale;
        e.grid_x     = grid_x;
        e.grid_y     = grid_y;
        e.label      = std::move(label);
        e.set |= kClosed | kBoxWidth | kBoxHeight | kRotation | kScale | kGridX | kGridY | kLabel;
        append(layer_code, data_[offset + 37]);
    }

    // ---- the reference's geometric tests -------------------------------------

    /// `_same_coordinate`: within a millimetre on all three axes.
    static bool same_coordinate(const Coord& a, const Coord& b) noexcept
    {
        return std::fabs(a.x - b.x) < 0.001 && std::fabs(a.y - b.y) < 0.001 &&
               std::fabs(a.z - b.z) < 0.001;
    }

    /// `_distance`, in three dimensions as the reference measures it.
    static double distance(const Coord& a, const Coord& b) noexcept
    {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        const double dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /// `_is_nearly_closed`: the last point within a fifth of the shorter end
    /// edge (at least five centimetres) of the first.
    static bool nearly_closed(const std::vector<Coord>& c) noexcept
    {
        if (c.size() < 4) return false;
        const Coord& first = c.front();
        const Coord& last  = c.back();
        if (same_coordinate(first, last)) return true;
        if (c.size() < 5) return false;
        const double first_edge  = distance(first, c[1]);
        const double last_edge   = distance(c[c.size() - 2], last);
        const double closure_gap = distance(first, last);
        const double reference   = py_min(first_edge, last_edge);
        if (reference <= 0.001) return false;
        const double tolerance = py_max(reference * 0.2, 0.05);
        return closure_gap <= tolerance;
    }

    /// `_nearly_orthogonal`.
    static bool nearly_orthogonal(double ax, double ay, double bx, double by, double a_length,
                                  double b_length) noexcept
    {
        const double normalized_dot = std::fabs((ax * bx + ay * by) / (a_length * b_length));
        return normalized_dot <= 0.03;
    }

    /// `_nearly_equal`.
    static bool nearly_equal(double a, double b) noexcept
    {
        const double tolerance = py_max(py_max(std::fabs(a), std::fabs(b)) * 0.02, 0.02);
        return std::fabs(a - b) <= tolerance;
    }

    /// `_simplify_collinear_ring`, to the four corners it leaves when there are
    /// four; `false` when more than four survive.
    ///
    /// THE SAME REMOVALS IN THE SAME ORDER, FOUND FASTER. The reference removes
    /// the first removable vertex — a zero-length edge on either side, or a turn
    /// whose normalised cross product is at most 0.02 — and scans the ring again
    /// from the start, until nothing is removable or four are left. A removal
    /// changes the answer only for its two neighbours, so every vertex the last
    /// scan passed is still not removable unless it was one of them. Keeping the
    /// vertices whose answer may have changed in an ordered set, and taking the
    /// lowest index each time, visits exactly the vertices the reference would
    /// have found removable, in its order, with its arithmetic — without its
    /// O(n²) rescans.
    static bool simplify_collinear_ring(const std::vector<Coord>& points, std::size_t count,
                                        std::array<Coord, 4>& out)
    {
        if (count < 4) return false;
        if (count == 4) {
            std::copy_n(points.begin(), 4, out.begin());
            return true;
        }
        std::vector<std::size_t> prev(count), next(count);
        for (std::size_t i = 0; i < count; ++i) {
            prev[i] = i == 0 ? count - 1 : i - 1;
            next[i] = i + 1 == count ? 0 : i + 1;
        }
        std::vector<bool> alive(count, true);
        std::set<std::size_t> pending;
        for (std::size_t i = 0; i < count; ++i)
            pending.insert(pending.end(), i);

        const auto removable = [&](std::size_t i) {
            const Coord& p  = points[prev[i]];
            const Coord& c  = points[i];
            const Coord& n  = points[next[i]];
            const double ax = c.x - p.x;
            const double ay = c.y - p.y;
            const double bx = n.x - c.x;
            const double by = n.y - c.y;
            const double a_len = std::sqrt(ax * ax + ay * ay);
            const double b_len = std::sqrt(bx * bx + by * by);
            if (a_len < 0.001 || b_len < 0.001) return true;
            const double cross = std::fabs(ax * by - ay * bx) / (a_len * b_len);
            return cross <= 0.02;
        };

        std::size_t left = count;
        while (left > 4) {
            std::optional<std::size_t> found;
            while (!pending.empty()) {
                const std::size_t v = *pending.begin();
                pending.erase(pending.begin());
                if (!alive[v]) continue;
                if (removable(v)) {
                    found = v;
                    break;
                }
            }
            if (!found) break;
            const std::size_t v = *found;
            alive[v]            = false;
            next[prev[v]]       = next[v];
            prev[next[v]]       = prev[v];
            pending.insert(prev[v]);
            pending.insert(next[v]);
            --left;
        }
        if (left != 4) return false;
        std::size_t k = 0;
        for (std::size_t i = 0; i < count && k < 4; ++i)
            if (alive[i]) out[k++] = points[i];
        return true;
    }

    /// `_box_metrics`: whether a closed run is a rectangle, and its sides and
    /// rotation when it is.
    static bool box_metrics(const std::vector<Coord>& c, double& width, double& height,
                            double& rotation)
    {
        if (c.size() < 5) return false;
        std::size_t count = c.size();
        if (same_coordinate(c.front(), c.back())) --count;
        std::array<Coord, 4> u;
        if (!simplify_collinear_ring(c, count, u)) return false;

        using Edge                = std::array<double, 2>;
        const std::array<Edge, 4> e = {Edge{u[1].x - u[0].x, u[1].y - u[0].y},
                                       Edge{u[2].x - u[1].x, u[2].y - u[1].y},
                                       Edge{u[3].x - u[2].x, u[3].y - u[2].y},
                                       Edge{u[0].x - u[3].x, u[0].y - u[3].y}};
        std::array<double, 4> lengths{};
        for (std::size_t k = 0; k < 4; ++k)
            lengths[k] = std::sqrt(e[k][0] * e[k][0] + e[k][1] * e[k][1]);
        for (const double l : lengths)
            if (l < 0.001) return false;
        const bool opposite_equal =
            nearly_equal(lengths[0], lengths[2]) && nearly_equal(lengths[1], lengths[3]);
        bool right_angles = true;
        for (std::size_t k = 0; k < 4; ++k) {
            const std::size_t j = (k + 1) % 4;
            right_angles =
                nearly_orthogonal(e[k][0], e[k][1], e[j][0], e[j][1], lengths[k], lengths[j]) &&
                right_angles;
        }
        if (!opposite_equal || !right_angles) return false;
        width    = lengths[0];
        height   = lengths[1];
        rotation = py_mod(core::atan2_rad(e[0][1], e[0][0]) * kRadToDeg, 360.0);
        return true;
    }

    // ---- the entity list -----------------------------------------------------

    /// A fresh entity in the one buffer this parser reuses: a file of a million
    /// entities allocates a million coordinate runs in the reference and one
    /// here.
    Entity& begin(Kind kind)
    {
        entity_.kind = kind;
        entity_.layer_name.clear();
        entity_.color.reset();
        entity_.name.clear();
        entity_.label.clear();
        entity_.text_height = entity_.rotation = entity_.box_width = entity_.box_height = 0.0;
        entity_.scale = entity_.grid_x = entity_.grid_y = 0.0;
        entity_.radius = entity_.start_angle = entity_.end_angle = 0.0;
        entity_.line_width                                      = 0.0;
        entity_.closed                                          = false;
        entity_.smart                                           = SmartClass::None;
        entity_.properties.clear();
        entity_.set    = 0;
        entity_.coords.clear();
        return entity_;
    }

    /// `_append_entity`, and then what `_finalize_entities` would do to it.
    void append(std::uint8_t layer_code, std::uint8_t color_code)
    {
        ++appended_;
        if (mode_ != Mode::Entities) return;
        Entity& e    = entity_;
        e.layer_code = layer_code;
        e.line_width = f32(record_ + 28);
        e.layer_name = layer_name(header_, layer_code);
        e.color      = geometry_color(header_, layer_code, color_code);

        // `_finalize_entities`, which runs once the whole file is read. The first
        // pass has read it, so its tables are the final ones.
        if (final_ != nullptr) {
            if (final_->smart_object && e.kind == Kind::Symbol && e.layer_code == 0 &&
                e.label == "S0") {
                ++header_.smart_marks;
                return;
            }
            if (e.layer_name.empty()) e.layer_name = layer_name(*final_, layer_code);
            if (!e.color) e.color = geometry_color(*final_, layer_code, 0);
        }
        if (sink_ != nullptr && !sink_->entity(e)) stopped_ = true;
    }

    const std::uint8_t* data_;
    std::size_t size_;
    Mode mode_;
    Header& header_;
    Sink* sink_;
    const Header* final_;
    Entity entity_;
    std::size_t record_{0}; ///< the offset of the geometry record being read
    std::uint64_t appended_{0};
    bool stopped_{false};
};

// ------------------------------------------------------- attribute tables ----

/// `_safe_round`: `None` for a non-finite value, `0.0` below 1e-12, otherwise
/// `round(value, 6)` — Python's correctly rounded one, which `std::to_chars`
/// and `std::from_chars` reproduce: both round the exact binary value.
Cell safe_round(double value)
{
    if (!std::isfinite(value)) return std::monostate{};
    if (std::fabs(value) < 1e-12) return 0.0;
    std::array<char, 400> buf{};
    const auto wrote =
        std::to_chars(buf.data(), buf.data() + buf.size(), value, std::chars_format::fixed, 6);
    if (wrote.ec != std::errc{}) return value;
    double back     = 0.0;
    const auto read = std::from_chars(buf.data(), wrote.ptr, back);
    if (read.ec != std::errc{}) return value;
    return back;
}

/// The value of a rounded cell, for `_looks_like_xy`.
std::optional<double> number_of(const Cell& c) noexcept
{
    if (const double* d = std::get_if<double>(&c)) return *d;
    return std::nullopt;
}

/// `_looks_like_xy`.
bool looks_like_xy(const Cell& cx, const Cell& cy) noexcept
{
    const auto x = number_of(cx);
    const auto y = number_of(cy);
    return x && y && std::isfinite(*x) && std::isfinite(*y) && std::fabs(*x) <= 100000000.0 &&
           std::fabs(*y) <= 100000000.0 && (std::fabs(*x) >= 1000.0 || std::fabs(*y) >= 1000.0);
}

/// One record's bytes and the readers `_parse_attribute_row` uses on them.
struct Chunk
{
    const std::uint8_t* data;
    std::size_t size;

    std::int64_t u16(std::size_t at) const noexcept
    {
        if (at + 2 > size) return 0;
        return static_cast<std::int64_t>(data[at]) | (static_cast<std::int64_t>(data[at + 1]) << 8);
    }
    std::int64_t u32(std::size_t at) const noexcept
    {
        if (at + 4 > size) return 0;
        std::uint32_t v = 0;
        for (std::size_t k = 0; k < 4; ++k)
            v |= static_cast<std::uint32_t>(data[at + k]) << (8 * k);
        return static_cast<std::int64_t>(v);
    }
    double f32(std::size_t at) const noexcept
    {
        if (at + 4 > size) return 0.0;
        float v = 0.0F;
        std::memcpy(&v, data + at, sizeof v);
        return static_cast<double>(v);
    }
    double f64(std::size_t at) const noexcept
    {
        if (at + 8 > size) return 0.0;
        double v = 0.0;
        std::memcpy(&v, data + at, sizeof v);
        return v;
    }
    std::int64_t at_or_zero(std::size_t at) const noexcept { return at < size ? data[at] : 0; }
};

/// `_collect_ascii_fields`, joined the way its caller joins them.
///
/// The reference tests up to 64 bytes behind every byte of the record. The run
/// of printable bytes that starts at each position is worked out once here,
/// backwards, so "are these L bytes printable" is one comparison.
std::string ascii_values(const Chunk& chunk, const std::string& table_ref)
{
    if (chunk.size < 2) return {};
    std::vector<std::uint32_t> run(chunk.size + 1, 0);
    for (std::size_t i = chunk.size; i-- > 0;)
        run[i] = (chunk.data[i] >= 32 && chunk.data[i] < 127) ? run[i + 1] + 1 : 0;

    std::vector<std::string_view> values;
    std::unordered_set<std::string_view> seen;
    for (std::size_t index = 0; index + 1 < chunk.size; ++index) {
        const std::size_t length = chunk.data[index];
        if (length == 0 || length > 64 || index + 1 + length > chunk.size) continue;
        if (run[index + 1] < length) continue;
        std::string_view value(reinterpret_cast<const char*>(chunk.data + index + 1), length);
        while (!value.empty() && value.front() == ' ')
            value.remove_prefix(1);
        while (!value.empty() && value.back() == ' ')
            value.remove_suffix(1);
        if (value.empty()) continue;
        if (!seen.insert(value).second) continue;
        values.push_back(value);
    }
    std::string out;
    bool first = true;
    for (const std::string_view v : values) {
        if (v == table_ref) continue;
        if (!first) out += " | ";
        out += v;
        first = false;
    }
    return out;
}

/// `_parse_attribute_row`.
AttributeRow parse_attribute_row(const Chunk& r, const std::string& table_ref,
                                 std::size_t row_index)
{
    AttributeRow row;
    row.row_index = row_index;
    auto& col     = row.columns;
    col.emplace_back("row_variant", std::string("unknown"));
    col.emplace_back("record_length", static_cast<std::int64_t>(r.size));
    const auto variant = [&](const char* name) { col[0].second = std::string(name); };

    if (r.size >= 11) {
        // `.decode('ascii', errors='ignore').strip('\x00 ')`
        std::string inline_ref;
        for (std::size_t k = 1; k < 11; ++k)
            if (r.data[k] < 0x80) inline_ref.push_back(static_cast<char>(r.data[k]));
        std::size_t a = 0;
        std::size_t b = inline_ref.size();
        while (a < b && (inline_ref[a] == '\0' || inline_ref[a] == ' '))
            ++a;
        while (b > a && (inline_ref[b - 1] == '\0' || inline_ref[b - 1] == ' '))
            --b;
        col.emplace_back("table_ref_inline", inline_ref.substr(a, b - a));
    }

    const std::size_t label_length = r.size > 28 ? r.data[28] : 0;
    std::string label_text;
    if (label_length >= 1 && label_length <= 64 && 29 + label_length <= r.size) {
        bool printable = true;
        for (std::size_t k = 29; k < 29 + label_length; ++k)
            printable = printable && r.data[k] >= 32 && r.data[k] < 127;
        if (printable) {
            std::size_t a = 29;
            std::size_t b = 29 + label_length;
            while (a < b && r.data[a] == ' ')
                ++a;
            while (b > a && r.data[b - 1] == ' ')
                --b;
            label_text.assign(reinterpret_cast<const char*>(r.data + a), b - a);
        }
    }

    if (!label_text.empty()) {
        const std::size_t sep = 29 + label_length;
        Cell c1x = safe_round(r.f64(sep + 8));
        Cell c1y = safe_round(r.f64(sep + 16));
        Cell c2x = safe_round(r.f64(sep + 50));
        Cell c2y = safe_round(r.f64(sep + 58));
        Cell c3x = safe_round(r.f64(sep + 66));
        Cell c3y = safe_round(r.f64(sep + 74));
        variant("label");
        col.emplace_back("label", std::move(label_text));
        col.emplace_back("label_length", static_cast<std::int64_t>(label_length));
        col.emplace_back("prefix_float", safe_round(r.f32(17)));
        col.emplace_back("code_u16", r.u16(25));
        col.emplace_back("separator_1", r.at_or_zero(sep));
        col.emplace_back("style_code", r.u32(sep + 1));
        col.emplace_back("flag_1", r.at_or_zero(sep + 5));
        col.emplace_back("flag_2", r.at_or_zero(sep + 6));
        col.emplace_back("flag_3", r.at_or_zero(sep + 7));
        col.emplace_back("coord_1_x", std::move(c1x));
        col.emplace_back("coord_1_y", std::move(c1y));
        col.emplace_back("separator_2", r.at_or_zero(sep + 35));
        col.emplace_back("scale_float", safe_round(r.f32(sep + 46)));
        col.emplace_back("coord_2_x", std::move(c2x));
        col.emplace_back("coord_2_y", std::move(c2y));
        col.emplace_back("coord_3_x", std::move(c3x));
        col.emplace_back("coord_3_y", std::move(c3y));
        return row;
    }

    if (r.size >= 119) {
        Cell c0x = safe_round(r.f64(17));
        Cell c0y = safe_round(r.f64(25));
        Cell c1x = safe_round(r.f64(45));
        Cell c1y = safe_round(r.f64(53));
        Cell c2x = safe_round(r.f64(87));
        Cell c2y = safe_round(r.f64(95));
        Cell c3x = safe_round(r.f64(103));
        Cell c3y = safe_round(r.f64(111));
        const bool plausible =
            looks_like_xy(c0x, c0y) && looks_like_xy(c1x, c1y) && looks_like_xy(c2x, c2y);
        if (!plausible) {
            col.emplace_back("ascii_values", ascii_values(r, table_ref));
            return row;
        }
        variant("segment");
        col.emplace_back("coord_0_x", std::move(c0x));
        col.emplace_back("coord_0_y", std::move(c0y));
        col.emplace_back("style_code", r.u32(37));
        col.emplace_back("flag_1", r.at_or_zero(41));
        col.emplace_back("flag_2", r.at_or_zero(42));
        col.emplace_back("flag_3", r.at_or_zero(43));
        col.emplace_back("flag_4", r.at_or_zero(44));
        col.emplace_back("coord_1_x", std::move(c1x));
        col.emplace_back("coord_1_y", std::move(c1y));
        col.emplace_back("separator_2", r.at_or_zero(72));
        col.emplace_back("coord_2_x", std::move(c2x));
        col.emplace_back("coord_2_y", std::move(c2y));
        col.emplace_back("coord_3_x", std::move(c3x));
        col.emplace_back("coord_3_y", std::move(c3y));
        return row;
    }

    col.emplace_back("ascii_values", ascii_values(r, table_ref));
    return row;
}

} // namespace

const char* smart_class_name(SmartClass c) noexcept
{
    switch (c) {
    case SmartClass::None: return "";
    case SmartClass::Settlement: return "Yerleşim";
    case SmartClass::Construction: return "Yapılaşma";
    case SmartClass::Road: return "Yol";
    case SmartClass::PlanNote: return "Plan Notu";
    case SmartClass::FunctionName: return "Fonksiyon Adı";
    case SmartClass::Other: return "Akıllı Nesne";
    }
    return "";
}

const char* kind_name(Kind k) noexcept
{
    switch (k) {
    case Kind::Point: return "Point";
    case Kind::Line: return "Line";
    case Kind::Polyline: return "Polyline";
    case Kind::Polygon: return "Polygon";
    case Kind::Circle: return "Circle";
    case Kind::Arc: return "Arc";
    case Kind::Text: return "Text";
    case Kind::Symbol: return "Symbol";
    case Kind::Block: return "Block";
    case Kind::MapSheet: return "MapSheet";
    case Kind::Triangle: return "Triangle";
    case Kind::SmartObject: return "SmartObject";
    }
    return "?";
}

std::optional<std::uint32_t> layer_color(const Header& h, std::uint8_t layer_code) noexcept
{
    return geometry_color(h, layer_code, 0);
}

Outcome read_header(std::span<const std::uint8_t> data, Header& header, std::stop_token stop)
{
    header = Header{};
    Parser tables(data, Mode::Header, header, nullptr, nullptr);
    return tables.scan(stop);
}

Outcome read(std::span<const std::uint8_t> data, Sink& sink, Header& header, std::stop_token stop)
{
    // THE FIRST PASS: the tables as they stand at the end of the file, and
    // whether any SmartObject exists — the two things `_finalize_entities`
    // needs and a single forward pass cannot know in time.
    Header final_header;
    if (const Outcome first = read_header(data, final_header, stop); first != Outcome::Complete)
        return first;

    if (!sink.begin(final_header)) return Outcome::Stopped;

    // THE SECOND: the reference's own pass, the tables rebuilt as it meets them
    // so each entity sees what the reference's entity saw, then finalised
    // against the first pass's.
    header = Header{};
    Parser entities(data, Mode::Entities, header, &sink, &final_header);
    const Outcome second = entities.scan(stop);
    header.smart_object  = final_header.smart_object;
    return second;
}

std::optional<std::vector<AttributeTable>> attribute_tables(std::span<const std::uint8_t> data,
                                                            std::stop_token stop)
{
    struct Marker
    {
        std::size_t record_start;
        std::string table_ref;
    };

    const std::string_view bytes(reinterpret_cast<const char*>(data.data()), data.size());
    std::vector<Marker> markers;
    std::size_t cursor     = 0;
    std::size_t next_check = 0;
    for (;;) {
        if (cursor >= next_check) {
            if (stop.stop_requested()) return std::nullopt;
            next_check = cursor + kStopStride;
        }
        const std::size_t marker = bytes.find("@TAB", cursor);
        if (marker == std::string_view::npos) break;
        std::size_t end = marker + 4;
        while (end < bytes.size() && bytes[end] >= '0' && bytes[end] <= '9')
            ++end;
        const std::size_t ref_length = end - marker;
        std::size_t record_start     = marker;
        if (marker > 0 && static_cast<std::uint8_t>(bytes[marker - 1]) == ref_length)
            record_start = marker - 1;
        markers.push_back(Marker{record_start, std::string(bytes.substr(marker, ref_length))});
        cursor = end;
    }
    if (markers.empty()) return std::vector<AttributeTable>{};

    std::map<std::string, std::vector<AttributeRow>> tables;
    for (std::size_t index = 0; index < markers.size(); ++index) {
        if ((index % 4096) == 0 && stop.stop_requested()) return std::nullopt;
        const Marker& m        = markers[index];
        std::size_t next_start = data.size();
        if (index + 1 < markers.size() && markers[index + 1].record_start > m.record_start)
            next_start = markers[index + 1].record_start;
        const std::size_t record_end = std::min(data.size(), next_start);
        if (record_end <= m.record_start) continue;
        const Chunk chunk{data.data() + m.record_start, record_end - m.record_start};
        auto& rows = tables[m.table_ref];
        rows.push_back(parse_attribute_row(chunk, m.table_ref, rows.size() + 1));
    }

    std::vector<AttributeTable> out;
    out.reserve(tables.size());
    for (auto& [ref, rows] : tables)
        if (!rows.empty()) out.push_back(AttributeTable{ref, std::move(rows)});
    return out;
}

} // namespace kentos::io::ncz
