// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): the drawing of a Netcad 8 SmartObject.
//
// See ncz_symbols.hpp for what each symbol looks like and where that was
// learned. The proportions below are fractions of the symbol's radius, measured
// on Netcad's own screenshots of the dialogs that make them; the radius is ten
// metres at an object size of one.
#include "ncz_symbols.hpp"

#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace kentos::io::ncz {
namespace {

/// The symbol's radius at an object size of one, in metres.
constexpr double kR = 10.0;

/// The value of property `name` when the object shows it: present, not
/// switched off by its `chk…IsNull`, and not empty.
std::optional<std::string> shown(const Entity& e, std::string_view name)
{
    for (const SmartProperty& p : e.properties) {
        if (p.name != name) continue;
        if (p.null) return std::nullopt;
        std::string_view v = p.value;
        while (!v.empty() && v.front() == ' ')
            v.remove_prefix(1);
        while (!v.empty() && v.back() == ' ')
            v.remove_suffix(1);
        if (v.empty()) return std::nullopt;
        return std::string(v);
    }
    return std::nullopt;
}

/// The value of property `name` whatever its switch says: the symbol's own
/// settings (`choiceType`, `numberOfDecimalPlace`) have none.
std::optional<std::string> setting(const Entity& e, std::string_view name)
{
    for (const SmartProperty& p : e.properties)
        if (p.name == name) return p.value;
    return std::nullopt;
}

/// `value` as a number, `.` or `,` for the decimal point.
std::optional<double> number(std::string_view value)
{
    std::string v(value);
    std::replace(v.begin(), v.end(), ',', '.');
    double out = 0.0;
    const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    if (ec != std::errc{} || ptr != v.data() + v.size() || !std::isfinite(out)) return std::nullopt;
    return out;
}

/// `value` with `decimals` places, as Netcad prints a TAKS or a road width —
/// `0.4` is `0.40` — or the value as written when it is not a number.
std::string fixed(std::string_view value, int decimals)
{
    const auto v = number(value);
    if (!v) return std::string(value);
    std::array<char, 64> buf{};
    const auto wrote = std::to_chars(buf.data(), buf.data() + buf.size(), *v,
                                     std::chars_format::fixed, std::clamp(decimals, 0, 6));
    if (wrote.ec != std::errc{}) return std::string(value);
    return std::string(buf.data(), wrote.ptr);
}

/// The letters a settlement symbol writes for its order. Only the three
/// Netcad's documentation shows and a real plan uses are shortened — `A`,
/// `B`, `BL`; any other order is written out, which is readable, rather than
/// abbreviated by a guess, which could be wrong.
std::string order_code(std::string_view nizam)
{
    const std::string folded = core::turkish_fold_key(nizam);
    if (folded == "AYRIK") return "A";
    if (folded == "BITISIK") return "B";
    if (folded == "BLOK") return "BL";
    return std::string(nizam);
}

Stroke circle(double r)
{
    Stroke s;
    s.type   = Stroke::Type::Circle;
    s.radius = r;
    return s;
}

Stroke line(double x0, double y0, double x1, double y1)
{
    Stroke s;
    s.type   = Stroke::Type::Polyline;
    s.points = {{x0, y0}, {x1, y1}};
    return s;
}

Stroke text(std::string words, core::TextAnchor anchor, double x, double y, double height)
{
    Stroke s;
    s.type   = Stroke::Type::Text;
    s.text   = std::move(words);
    s.anchor = anchor;
    s.centre = {x, y};
    s.height = height;
    return s;
}

/// `Yerleşim`: the garden distances over and under a dash, the order and the
/// storeys either side of it — `5 / A–3 / 3`.
std::optional<Symbol> settlement(const Entity& e)
{
    const auto on    = shown(e, "txtOn");
    const auto arka  = shown(e, "txtArka");
    const auto yan   = shown(e, "txtYan");
    const auto kat   = shown(e, "kat");
    const auto nizam = shown(e, "nizam");
    const std::string code = nizam ? order_code(*nizam) : std::string();
    std::string top;
    if (on) top = *on;
    if (arka) top += (top.empty() ? "" : "-") + *arka;

    Symbol s;
    constexpr double h = 0.45 * kR;
    s.strokes.push_back(circle(kR));
    if (kat) {
        if (!code.empty())
            s.strokes.push_back(text(code, core::TextAnchor::MiddleRight, -0.16 * kR, 0.0, h));
        s.strokes.push_back(line(-0.1 * kR, 0.0, 0.1 * kR, 0.0));
        s.strokes.push_back(text(*kat, core::TextAnchor::MiddleLeft, 0.16 * kR, 0.0, h));
    } else if (!code.empty()) {
        s.strokes.push_back(text(code, core::TextAnchor::MiddleCentre, 0.0, 0.0, h));
    }
    if (!top.empty()) s.strokes.push_back(text(top, core::TextAnchor::MiddleCentre, 0.0, 0.52 * kR, h));
    if (yan) s.strokes.push_back(text(*yan, core::TextAnchor::MiddleCentre, 0.0, -0.52 * kR, h));
    if (s.strokes.size() == 1) return std::nullopt; // nothing to say
    const std::string middle = code + (kat ? "-" + *kat : std::string());
    s.key     = "NCZ Yerleşim " + top + "_" + middle + "_" + yan.value_or("");
    s.summary = "Netcad yerleşim sembolü: " + middle;
    return s;
}

/// `Yapılaşma`: TAKS over KAKS in a split circle, or Emsal, Hmax and Yençok
/// as lines of text.
std::optional<Symbol> construction(const Entity& e)
{
    int decimals = 2;
    if (const auto d = setting(e, "numberOfDecimalPlace"))
        if (const auto v = number(*d); v && *v >= 0.0 && *v <= 6.0) decimals = static_cast<int>(*v);
    const auto pair = [&](const char* value, const char* minimum) -> std::string {
        const auto v = shown(e, value);
        if (!v) return {};
        const auto m = shown(e, minimum);
        return m ? fixed(*m, decimals) + "-" + fixed(*v, decimals) : fixed(*v, decimals);
    };
    const std::string taks = pair("taks", "minTaks");
    const std::string kaks = pair("kaks", "minKaks");
    const bool ratios      = setting(e, "choiceType").value_or("1") != "0" &&
                        (!taks.empty() || !kaks.empty());

    Symbol s;
    if (ratios) {
        const bool range = shown(e, "minTaks").has_value() || shown(e, "minKaks").has_value();
        const double h   = (range ? 0.27 : 0.48) * kR;
        s.strokes.push_back(circle(kR));
        if (!taks.empty() && !kaks.empty()) {
            const double w = (range ? 0.9 : 0.62) * kR;
            s.strokes.push_back(line(-w, 0.0, w, 0.0));
            s.strokes.push_back(text(taks, core::TextAnchor::MiddleCentre, 0.0, 0.36 * kR, h));
            s.strokes.push_back(text(kaks, core::TextAnchor::MiddleCentre, 0.0, -0.36 * kR, h));
        } else {
            s.strokes.push_back(text(taks.empty() ? kaks : taks, core::TextAnchor::MiddleCentre,
                                     0.0, 0.0, h));
        }
        s.key     = "NCZ Yapılaşma " + taks + "_" + kaks;
        s.summary = "Netcad yapılaşma sembolü: TAKS " + taks + ", KAKS " + kaks;
        return s;
    }

    std::vector<std::string> lines;
    if (const auto v = shown(e, "emsal")) lines.push_back("E=" + fixed(*v, decimals));
    if (const auto v = shown(e, "hmax")) {
        const auto unit = shown(e, "HmaxType");
        lines.push_back("Hmax=" + fixed(*v, decimals) + (unit ? " " + *unit : std::string()));
    }
    // In metres unless the symbol names its unit (`Kat`): the plan's own text
    // beside such a symbol reads `Yençok=15.50 m`.
    if (const auto v = shown(e, "yEncok"))
        lines.push_back("Yençok=" + fixed(*v, decimals) + " " + shown(e, "YencokType").value_or("m"));
    if (lines.empty()) return std::nullopt;
    const auto n = static_cast<double>(lines.size());
    std::string joined;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const double y = (-0.15 + ((n - 1.0) / 2.0 - static_cast<double>(i)) * 0.93) * kR;
        s.strokes.push_back(text(lines[i], core::TextAnchor::MiddleLeft, -1.65 * kR, y, 0.5 * kR));
        joined += (i == 0 ? "" : "_") + lines[i];
    }
    s.key     = "NCZ Yapılaşma " + joined;
    s.summary = "Netcad yapılaşma sembolü: " + joined;
    return s;
}

/// `Yol`: the width's whole metres, its decimals raised, small, underlined.
std::optional<Symbol> road(const Entity& e)
{
    const auto w = shown(e, "genislik");
    if (!w) return std::nullopt;
    const std::string full = fixed(*w, 2);
    const std::size_t dot  = full.find('.');
    const std::string whole = dot == std::string::npos ? full : full.substr(0, dot);
    const std::string frac  = dot == std::string::npos ? std::string("00") : full.substr(dot + 1);

    Symbol s;
    constexpr double small = 0.28 * kR;
    s.strokes.push_back(circle(kR));
    s.strokes.push_back(text(whole, core::TextAnchor::MiddleRight, -0.05 * kR, 0.0, 0.55 * kR));
    s.strokes.push_back(text(frac, core::TextAnchor::BaselineLeft, 0.12 * kR, 0.06 * kR, small));
    const double under =
        core::mm_to_metres(core::text_width(frac, core::mm_from_metres(small)));
    s.strokes.push_back(line(0.12 * kR, 0.02 * kR, 0.12 * kR + under, 0.02 * kR));
    s.key     = "NCZ Yol " + full;
    s.summary = "Netcad yol genişliği sembolü: " + full + " m";
    return s;
}

/// `Fonksiyon Adı`: the function's name, from its anchor.
std::optional<Symbol> function_name(const Entity& e)
{
    const auto name = shown(e, "adi");
    if (!name) return std::nullopt;
    Symbol s;
    s.strokes.push_back(text(*name, core::TextAnchor::MiddleLeft, 0.0, 0.05 * kR, 0.5 * kR));
    s.key     = "NCZ Fonksiyon " + *name;
    s.summary = "Netcad fonksiyon adı: " + *name;
    return s;
}

/// `Plan Notu`: its text in its box, the box's top left at the anchor.
std::optional<Symbol> plan_note(const Entity& e)
{
    const auto data = setting(e, "rtfData");
    const auto wv   = setting(e, "width");
    const auto hv   = setting(e, "height");
    if (!data || !wv || !hv) return std::nullopt;
    const auto w = number(*wv);
    const auto h = number(*hv);
    if (!w || !h || *w <= 0.0 || *h <= 0.0 || *w > 100000.0 || *h > 100000.0) return std::nullopt;
    const std::string body = rtf_text(base64_decode(*data));
    if (body.empty()) return std::nullopt;

    // As tall as the note's paragraphs fit its box, and never wider than the
    // longest line allows: the RTF's own point sizes are relative to a page
    // Netcad scales into the box, so the box is what is known.
    std::size_t lines = 1;
    std::size_t longest = 0;
    std::size_t run     = 0;
    for (const char ch : body) {
        if (ch == '\n') {
            ++lines;
            longest = std::max(longest, run);
            run     = 0;
        } else if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) {
            ++run; // a character, not a UTF-8 continuation byte
        }
    }
    longest = std::max(longest, run);
    double height = *h / (static_cast<double>(lines) * 1.45);
    if (longest > 0) height = std::min(height, *w / (static_cast<double>(longest) * 0.62));
    height = std::max(height, *h / 400.0);

    Symbol s;
    Stroke box;
    box.type   = Stroke::Type::Polyline;
    box.points = {{0.0, 0.0}, {*w, 0.0}, {*w, -*h}, {0.0, -*h}};
    box.closed = true;
    s.strokes.push_back(std::move(box));
    Stroke words = text(body, core::TextAnchor::TopLeft, 0.02 * *w, -0.02 * *h, height);
    words.wrap_width = 0.96 * *w;
    s.strokes.push_back(std::move(words));

    // The name says what the note is and tells two notes apart.
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char ch : body + *wv + *hv) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ULL;
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string tag;
    for (int k = 0; k < 8; ++k)
        tag.push_back(kHex[(hash >> (60 - 4 * k)) & 15]);
    std::string first = body.substr(0, std::min<std::size_t>(body.find('\n'), 40));
    s.key     = "NCZ Plan notu " + tag;
    s.summary = "Netcad plan notu: " + first;
    return s;
}

/// Windows-1254, the byte `\'hh` names: Latin-1 but for its six Turkish letters.
void append_1254(std::string& out, unsigned byte)
{
    std::uint32_t cp = byte;
    switch (byte) {
    case 0xD0: cp = 0x011E; break;
    case 0xDD: cp = 0x0130; break;
    case 0xDE: cp = 0x015E; break;
    case 0xF0: cp = 0x011F; break;
    case 0xFD: cp = 0x0131; break;
    case 0xFE: cp = 0x015F; break;
    default: break;
    }
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void append_utf8(std::string& out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

std::optional<Symbol> planet_symbol(const Entity& e)
{
    switch (e.smart) {
    case SmartClass::Settlement: return settlement(e);
    case SmartClass::Construction: return construction(e);
    case SmartClass::Road: return road(e);
    case SmartClass::FunctionName: return function_name(e);
    case SmartClass::PlanNote: return plan_note(e);
    case SmartClass::None:
    case SmartClass::Other: return std::nullopt;
    }
    return std::nullopt;
}

std::string base64_decode(std::string_view in)
{
    const auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    out.reserve(in.size() * 3 / 4);
    std::uint32_t acc = 0;
    int bits          = 0;
    for (const char c : in) {
        if (c == '=') break;
        const int v = value(c);
        if (v < 0) {
            if (c == '\r' || c == '\n' || c == ' ') continue;
            return {};
        }
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

std::string rtf_text(std::string_view rtf)
{
    // Destinations that hold no text of the document.
    static constexpr std::array<std::string_view, 16> kSkip{
        "fonttbl", "colortbl", "stylesheet", "info",       "pict",      "object",
        "themedata", "colorschememapping", "latentstyles", "datastore", "xmlnstbl",
        "listtable", "listoverridetable", "rsidtbl",     "generator", "header"};

    struct Group
    {
        bool skip;
        int uc;
    };
    std::vector<Group> stack{{false, 1}};
    std::string out;
    int fallback    = 0; // characters still to drop after a `\u`
    bool group_head = false;
    std::size_t i   = 0;
    const auto emit = [&](const std::string& piece) {
        if (!stack.back().skip) out += piece;
    };
    while (i < rtf.size()) {
        const char c = rtf[i];
        if (c == '{') {
            stack.push_back(stack.back());
            group_head = true;
            ++i;
            continue;
        }
        if (c == '}') {
            if (stack.size() > 1) stack.pop_back();
            group_head = false;
            ++i;
            continue;
        }
        if (c == '\r' || c == '\n') {
            ++i;
            continue;
        }
        if (c != '\\') {
            group_head = false;
            if (fallback > 0) {
                --fallback;
            } else {
                emit(std::string(1, c));
            }
            ++i;
            continue;
        }
        // A control sequence.
        if (i + 1 >= rtf.size()) break;
        const char next = rtf[i + 1];
        if (next == '\\' || next == '{' || next == '}') {
            if (fallback > 0)
                --fallback;
            else
                emit(std::string(1, next));
            i += 2;
            continue;
        }
        if (next == '*') {
            stack.back().skip = true; // an ignorable destination
            i += 2;
            continue;
        }
        if (next == '\'') {
            if (i + 4 <= rtf.size()) {
                unsigned byte = 0;
                const auto [p, ec] = std::from_chars(rtf.data() + i + 2, rtf.data() + i + 4, byte, 16);
                if (ec == std::errc{} && p == rtf.data() + i + 4) {
                    if (fallback > 0) {
                        --fallback;
                    } else if (!stack.back().skip) {
                        append_1254(out, byte);
                    }
                }
            }
            i += 4;
            continue;
        }
        if (next == '~') {
            emit(" ");
            i += 2;
            continue;
        }
        if (!((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z'))) {
            i += 2; // `\-`, `\_` and the like: nothing to show
            continue;
        }
        std::size_t j = i + 1;
        while (j < rtf.size() && ((rtf[j] >= 'a' && rtf[j] <= 'z') || (rtf[j] >= 'A' && rtf[j] <= 'Z')))
            ++j;
        const std::string_view word = rtf.substr(i + 1, j - i - 1);
        bool has_arg                = false;
        long arg                    = 0;
        if (j < rtf.size() && (rtf[j] == '-' || (rtf[j] >= '0' && rtf[j] <= '9'))) {
            const auto [p, ec] = std::from_chars(rtf.data() + j, rtf.data() + rtf.size(), arg);
            if (ec == std::errc{}) {
                has_arg = true;
                j       = static_cast<std::size_t>(p - rtf.data());
            }
        }
        if (j < rtf.size() && rtf[j] == ' ') ++j; // the delimiter belongs to the word
        i = j;

        if (group_head && std::find(kSkip.begin(), kSkip.end(), word) != kSkip.end())
            stack.back().skip = true;
        group_head = false;
        if (word == "par" || word == "line") {
            emit("\n");
        } else if (word == "tab") {
            emit(" ");
        } else if (word == "uc" && has_arg) {
            stack.back().uc = static_cast<int>(std::clamp<long>(arg, 0, 8));
        } else if (word == "u" && has_arg) {
            const std::uint32_t cp = static_cast<std::uint32_t>(arg < 0 ? arg + 65536 : arg);
            if (!stack.back().skip) append_utf8(out, cp);
            fallback = stack.back().uc;
        }
    }
    // Trailing breaks and spaces say nothing.
    while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
        out.pop_back();
    std::size_t lead = 0;
    while (lead < out.size() && (out[lead] == '\n' || out[lead] == ' '))
        ++lead;
    return out.substr(lead);
}

} // namespace kentos::io::ncz
