// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace piricad::core {
namespace {

const std::string& empty_string()
{
    static const std::string s;
    return s;
}

const JsonArray& empty_array()
{
    static const JsonArray a;
    return a;
}

const JsonObject& empty_object()
{
    static const JsonObject o;
    return o;
}

void escape_into(std::string& out, std::string_view s)
{
    out += '"';
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                // Six characters and a terminator, so it fits — but "it fits" is an
                // argument, not a guarantee, and a truncated escape is malformed
                // JSON that only shows up in the file nobody can reopen.
                const int written = std::snprintf(buf, sizeof buf, "\\u%04x", c);
                if (written < 0 || static_cast<std::size_t>(written) >= sizeof buf) {
                    out += "\\ufffd"; // replacement character: lossy, but valid
                    continue;
                }
                out += buf;
            } else {
                out += static_cast<char>(c); // UTF-8 passes through verbatim
            }
        }
    }
    out += '"';
}

/// Locale-independent shortest round-trip formatting.
void number_into(std::string& out, double v)
{
    if (!std::isfinite(v)) {
        out += "null";
        return;
    }
    char buf[40];
    for (int prec = 1; prec <= 17; ++prec) {
        const int written = std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (written < 0 || static_cast<std::size_t>(written) >= sizeof buf) {
            out += "null"; // unrepresentable in the space we allow; never a truncation
            return;
        }
        if (std::strtod(buf, nullptr) == v) break;
    }
    // %g may emit a locale decimal separator; normalise to '.'
    for (char& c : buf) {
        if (c == ',') c = '.';
        if (c == '\0') break;
    }
    out += buf;
}

struct Parser
{
    std::string_view s;
    std::size_t i{0};

    void skip_ws()
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
            ++i;
    }

    Error fail(std::string what) const
    {
        return err(ErrorCode::ParseError,
                   "JSON parse error at offset " + std::to_string(i) + ": " + std::move(what));
    }

    Result<Json> parse_value(int depth)
    {
        if (depth > 64) return fail("nesting deeper than 64 levels");
        skip_ws();
        if (i >= s.size()) return fail("unexpected end of input");

        switch (s[i]) {
        case '{': return parse_object(depth);
        case '[': return parse_array(depth);
        case '"': {
            auto r = parse_string();
            if (!r) return r.error();
            return Json::string(std::move(r.value()));
        }
        case 't':
            if (s.compare(i, 4, "true") == 0) {
                i += 4;
                return Json::boolean(true);
            }
            return fail("expected 'true'");
        case 'f':
            if (s.compare(i, 5, "false") == 0) {
                i += 5;
                return Json::boolean(false);
            }
            return fail("expected 'false'");
        case 'n':
            if (s.compare(i, 4, "null") == 0) {
                i += 4;
                return Json::null();
            }
            return fail("expected 'null'");
        default: return parse_number();
        }
    }

    Result<std::string> parse_string()
    {
        if (i >= s.size() || s[i] != '"') return fail("expected '\"'");
        ++i;
        std::string out;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') {
                ++i;
                if (i >= s.size()) return fail("unterminated escape");
                switch (s[i]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (i + 4 >= s.size()) return fail("truncated \\u escape");
                    unsigned cp = 0;
                    for (std::size_t k = 1; k <= 4; ++k) {
                        const char c = s[i + k];
                        cp <<= 4;
                        if (c >= '0' && c <= '9')
                            cp |= unsigned(c - '0');
                        else if (c >= 'a' && c <= 'f')
                            cp |= unsigned(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F')
                            cp |= unsigned(c - 'A' + 10);
                        else
                            return fail("bad hex digit in \\u escape");
                    }
                    i += 4;
                    const auto emit = [&out](unsigned byte) {
                        out += static_cast<char>(static_cast<unsigned char>(byte));
                    };
                    if (cp < 0x80) {
                        emit(cp);
                    } else if (cp < 0x800) {
                        emit(0xC0u | (cp >> 6));
                        emit(0x80u | (cp & 0x3Fu));
                    } else {
                        emit(0xE0u | (cp >> 12));
                        emit(0x80u | ((cp >> 6) & 0x3Fu));
                        emit(0x80u | (cp & 0x3Fu));
                    }
                    break;
                }
                default: return fail("unknown escape");
                }
                ++i;
            } else {
                out += s[i++];
            }
        }
        if (i >= s.size()) return fail("unterminated string");
        ++i;
        return out;
    }

    Result<Json> parse_number()
    {
        const std::size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
        bool is_double = false;
        while (i < s.size()) {
            const char c = s[i];
            if (c >= '0' && c <= '9') {
                ++i;
                continue;
            }
            if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                is_double = true;
                ++i;
                continue;
            }
            break;
        }
        if (i == start) return fail("expected a value");

        const std::string tok(s.substr(start, i - start));
        if (is_double) return Json::number(std::strtod(tok.c_str(), nullptr));
        return Json::integer(std::strtoll(tok.c_str(), nullptr, 10));
    }

    Result<Json> parse_array(int depth)
    {
        ++i; // '['
        JsonArray items;
        skip_ws();
        if (i < s.size() && s[i] == ']') {
            ++i;
            return Json::array(std::move(items));
        }
        while (true) {
            auto v = parse_value(depth + 1);
            if (!v) return v.error();
            items.push_back(std::move(v.value()));
            skip_ws();
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            if (i < s.size() && s[i] == ']') {
                ++i;
                break;
            }
            return fail("expected ',' or ']'");
        }
        return Json::array(std::move(items));
    }

    Result<Json> parse_object(int depth)
    {
        ++i; // '{'
        JsonObject members;
        skip_ws();
        if (i < s.size() && s[i] == '}') {
            ++i;
            return Json::object(std::move(members));
        }
        while (true) {
            skip_ws();
            auto k = parse_string();
            if (!k) return k.error();
            skip_ws();
            if (i >= s.size() || s[i] != ':') return fail("expected ':'");
            ++i;
            auto v = parse_value(depth + 1);
            if (!v) return v.error();
            members.emplace_back(std::move(k.value()), std::move(v.value()));
            skip_ws();
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            if (i < s.size() && s[i] == '}') {
                ++i;
                break;
            }
            return fail("expected ',' or '}'");
        }
        return Json::object(std::move(members));
    }
};

} // namespace

Json Json::null()
{
    return Json{};
}

Json Json::boolean(bool v)
{
    Json j;
    j.type_ = Type::Bool;
    j.b_    = v;
    return j;
}

Json Json::integer(std::int64_t v)
{
    Json j;
    j.type_ = Type::Int;
    j.i_    = v;
    return j;
}

Json Json::number(double v)
{
    Json j;
    j.type_ = Type::Double;
    j.d_    = v;
    return j;
}

Json Json::string(std::string v)
{
    Json j;
    j.type_ = Type::String;
    j.s_    = std::move(v);
    return j;
}

Json Json::array(JsonArray v)
{
    Json j;
    j.type_ = Type::Array;
    j.a_    = std::move(v);
    return j;
}

Json Json::object(JsonObject v)
{
    Json j;
    j.type_ = Type::Object;
    j.o_    = std::move(v);
    return j;
}

bool Json::as_bool(bool d) const
{
    return type_ == Type::Bool ? b_ : d;
}

std::int64_t Json::as_int(std::int64_t d) const
{
    if (type_ == Type::Int) return i_;
    if (type_ == Type::Double) return static_cast<std::int64_t>(d_ >= 0 ? d_ + 0.5 : d_ - 0.5);
    return d;
}

double Json::as_double(double d) const
{
    if (type_ == Type::Double) return d_;
    if (type_ == Type::Int) return static_cast<double>(i_);
    return d;
}

const std::string& Json::as_string() const
{
    return type_ == Type::String ? s_ : empty_string();
}

const JsonArray& Json::as_array() const
{
    return type_ == Type::Array ? a_ : empty_array();
}

const JsonObject& Json::as_object() const
{
    return type_ == Type::Object ? o_ : empty_object();
}

const Json* Json::find(std::string_view key) const
{
    if (type_ != Type::Object) return nullptr;
    for (const auto& [k, v] : o_)
        if (k == key) return &v;
    return nullptr;
}

void Json::set(std::string key, Json value)
{
    if (type_ != Type::Object) {
        type_ = Type::Object;
        o_.clear();
    }
    for (auto& [k, v] : o_) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    o_.emplace_back(std::move(key), std::move(value));
}

void Json::push(Json value)
{
    if (type_ != Type::Array) {
        type_ = Type::Array;
        a_.clear();
    }
    a_.push_back(std::move(value));
}

void Json::dump_to(std::string& out, int indent, int depth) const
{
    const bool pretty  = indent > 0;
    const auto newline = [&](int d) {
        if (!pretty) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(d), ' ');
    };

    switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += b_ ? "true" : "false"; break;
    case Type::Int: out += std::to_string(i_); break;
    case Type::Double: number_into(out, d_); break;
    case Type::String: escape_into(out, s_); break;
    case Type::Array:
        if (a_.empty()) {
            out += "[]";
            break;
        }
        out += '[';
        for (std::size_t k = 0; k < a_.size(); ++k) {
            if (k) out += ',';
            newline(depth + 1);
            a_[k].dump_to(out, indent, depth + 1);
        }
        newline(depth);
        out += ']';
        break;
    case Type::Object:
        if (o_.empty()) {
            out += "{}";
            break;
        }
        out += '{';
        for (std::size_t k = 0; k < o_.size(); ++k) {
            if (k) out += ',';
            newline(depth + 1);
            escape_into(out, o_[k].first);
            out += ':';
            if (pretty) out += ' ';
            o_[k].second.dump_to(out, indent, depth + 1);
        }
        newline(depth);
        out += '}';
        break;
    }
}

std::string Json::dump() const
{
    std::string out;
    dump_to(out, 0, 0);
    return out;
}

std::string Json::dump_pretty(int indent) const
{
    std::string out;
    dump_to(out, indent, 0);
    return out;
}

Result<Json> Json::parse(std::string_view text)
{
    Parser p{text, 0};
    auto v = p.parse_value(0);
    if (!v) return v;
    p.skip_ws();
    if (p.i != text.size()) return p.fail("trailing content after value");
    return v;
}

} // namespace piricad::core
