// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/parser.hpp"

#include "piricad/core/text.hpp"

#include <cmath>
#include <cstdlib>

namespace piricad::command {
namespace {

using core::err;
using core::ErrorCode;
using core::Point2;

constexpr double kPi = 3.14159265358979323846;

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// ---- expression evaluator: + - * / % ^, parentheses, unary +/- ----
struct ExprParser
{
    explicit ExprParser(std::string_view text) : s(text) {}

    std::string_view s;
    std::size_t i{0};
    bool failed{false};
    std::string why;

    void skip()
    {
        while (i < s.size() && is_space(s[i]))
            ++i;
    }

    double parse()
    {
        const double v = additive();
        skip();
        if (!failed && i != s.size()) {
            failed = true;
            why    = "beklenmeyen '" + std::string(1, s[i]) + "' karakteri (konum " +
                  std::to_string(i) + ")";
        }
        return v;
    }

    double additive()
    {
        double v = multiplicative();
        while (!failed) {
            skip();
            if (i < s.size() && s[i] == '+') {
                ++i;
                v += multiplicative();
            } else if (i < s.size() && s[i] == '-') {
                ++i;
                v -= multiplicative();
            } else
                break;
        }
        return v;
    }

    double multiplicative()
    {
        double v = power();
        while (!failed) {
            skip();
            if (i < s.size() && s[i] == '*') {
                ++i;
                v *= power();
            } else if (i < s.size() && s[i] == '/') {
                ++i;
                const double d = power();
                if (d == 0.0) {
                    failed = true;
                    why    = "sıfıra bölme";
                    return 0.0;
                }
                v /= d;
            } else if (i < s.size() && s[i] == '%') {
                ++i;
                const double d = power();
                if (d == 0.0) {
                    failed = true;
                    why    = "sıfıra göre mod";
                    return 0.0;
                }
                v = std::fmod(v, d);
            } else
                break;
        }
        return v;
    }

    double power()
    {
        const double base = unary();
        skip();
        if (!failed && i < s.size() && s[i] == '^') {
            ++i;
            return std::pow(base, power()); // right-associative
        }
        return base;
    }

    double unary()
    {
        skip();
        if (i < s.size() && s[i] == '-') {
            ++i;
            return -unary();
        }
        if (i < s.size() && s[i] == '+') {
            ++i;
            return unary();
        }
        return primary();
    }

    double primary()
    {
        skip();
        if (i >= s.size()) {
            failed = true;
            why    = "ifade beklenmedik yerde bitti";
            return 0.0;
        }

        if (s[i] == '(') {
            ++i;
            const double v = additive();
            skip();
            if (i >= s.size() || s[i] != ')') {
                failed = true;
                why    = "kapanmamış parantez";
                return 0.0;
            }
            ++i;
            return v;
        }

        const std::size_t start = i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.'))
            ++i;
        if (i == start) {
            failed = true;
            why    = "sayı bekleniyordu (konum " + std::to_string(i) + ")";
            return 0.0;
        }
        return std::strtod(std::string(s.substr(start, i - start)).c_str(), nullptr);
    }
};

/// Splits "a,b" at the top-level comma (parentheses are respected).
bool split_pair(std::string_view s, std::string_view& left, std::string_view& right)
{
    int depth = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(')
            ++depth;
        else if (s[i] == ')')
            --depth;
        else if (s[i] == ',' && depth == 0) {
            left  = s.substr(0, i);
            right = s.substr(i + 1);
            return true;
        }
    }
    return false;
}

bool split_polar(std::string_view s, std::string_view& dist, std::string_view& angle)
{
    int depth = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(')
            ++depth;
        else if (s[i] == ')')
            --depth;
        else if (s[i] == '<' && depth == 0) {
            dist  = s.substr(0, i);
            angle = s.substr(i + 1);
            return true;
        }
    }
    return false;
}

core::Result<Token> classify(std::string_view raw)
{
    Token t;

    // Quoted text is already unquoted by the tokeniser.
    // Keyword argument: key=<value>
    if (const std::size_t eq = raw.find('='); eq != std::string_view::npos && eq > 0) {
        const std::string_view key  = raw.substr(0, eq);
        const std::string_view rest = raw.substr(eq + 1);
        bool key_is_word            = true;
        for (char c : key) {
            if (c == ',' || c == '@' || c == '<' || c == '(' || c == ')') {
                key_is_word = false;
                break;
            }
        }
        if (key_is_word) {
            auto inner = classify(rest);
            if (!inner) return inner;
            t.kind = Token::Kind::KeyValue;
            t.word = std::string(key);
            t.nested.push_back(std::move(inner.value()));
            return t;
        }
    }

    // Relative or polar: leading '@'
    if (!raw.empty() && raw.front() == '@') {
        const std::string_view body = raw.substr(1);
        std::string_view a, b;

        if (split_polar(body, a, b)) {
            ExprParser pa(a);
            const double d = pa.parse();
            ExprParser pb(b);
            const double ang = pb.parse();
            if (pa.failed) return err(ErrorCode::ParseError, "Kutupsal mesafe: " + pa.why);
            if (pb.failed) return err(ErrorCode::ParseError, "Kutupsal açı: " + pb.why);
            t.kind = Token::Kind::Polar;
            t.a    = d;
            t.b    = ang;
            return t;
        }

        if (split_pair(body, a, b)) {
            ExprParser pa(a);
            const double dx = pa.parse();
            ExprParser pb(b);
            const double dy = pb.parse();
            if (pa.failed) return err(ErrorCode::ParseError, "Göreli dx: " + pa.why);
            if (pb.failed) return err(ErrorCode::ParseError, "Göreli dy: " + pb.why);
            t.kind = Token::Kind::Relative;
            t.a    = dx;
            t.b    = dy;
            return t;
        }

        return err(ErrorCode::ParseError,
                   "Beklenen: '@dx,dy' veya '@mesafe<açı'. Girilen: '" + std::string(raw) + "'");
    }

    // Absolute point: x,y
    {
        std::string_view a, b;
        if (split_pair(raw, a, b)) {
            ExprParser pa(a);
            const double x = pa.parse();
            ExprParser pb(b);
            const double y = pb.parse();
            if (pa.failed) return err(ErrorCode::ParseError, "X koordinatı: " + pa.why);
            if (pb.failed) return err(ErrorCode::ParseError, "Y koordinatı: " + pb.why);
            t.kind = Token::Kind::Absolute;
            t.a    = x;
            t.b    = y;
            return t;
        }
    }

    // Bare number or parenthesised expression
    {
        const char c = raw.empty() ? '\0' : raw.front();
        const bool numeric_start =
            (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == '(';
        if (numeric_start) {
            ExprParser p(raw);
            const double v = p.parse();
            if (!p.failed) {
                t.kind = Token::Kind::Number;
                t.a    = v;
                return t;
            }
        }
    }

    t.kind = Token::Kind::Word;
    t.word = std::string(raw);
    return t;
}

} // namespace

core::Result<double> evaluate_expression(std::string_view expr)
{
    ExprParser p(expr);
    const double v = p.parse();
    if (p.failed)
        return err(ErrorCode::ParseError, "'" + std::string(expr) + "' ifadesi: " + p.why);
    return v;
}

core::Result<ParsedLine> parse_line(std::string_view line)
{
    ParsedLine out;

    std::vector<std::pair<std::string, bool>> raw; // (text, was_quoted)
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && is_space(line[i]))
            ++i;
        if (i >= line.size()) break;

        if (line[i] == '"') {
            ++i;
            std::string text;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) ++i;
                text += line[i++];
            }
            if (i >= line.size())
                return err(ErrorCode::ParseError, "Komut satırında kapanmamış tırnak var.");
            ++i;
            raw.emplace_back(std::move(text), true);
            continue;
        }

        // A bare token ends at whitespace, but a quoted run inside it — the value
        // of `ad="YOL KENARI"` — is absorbed whole, quotes stripped.
        std::string token;
        int depth   = 0;
        bool quoted = false;

        while (i < line.size()) {
            const char c = line[i];

            if (c == '"') {
                ++i;
                quoted = true;
                while (i < line.size() && line[i] != '"') {
                    if (line[i] == '\\' && i + 1 < line.size()) ++i;
                    token += line[i++];
                }
                if (i >= line.size())
                    return err(ErrorCode::ParseError, "Komut satırında kapanmamış tırnak var.");
                ++i;
                continue;
            }

            if (c == '(')
                ++depth;
            else if (c == ')')
                --depth;
            else if (is_space(c) && depth == 0)
                break;

            token += c;
            ++i;
        }
        raw.emplace_back(std::move(token), false);
        (void)quoted;
    }

    if (raw.empty()) return err(ErrorCode::ParseError, "Boş komut satırı.");

    out.command = raw.front().first;
    out.tokens.reserve(raw.size() - 1);

    for (std::size_t k = 1; k < raw.size(); ++k) {
        if (raw[k].second) {
            Token t;
            t.kind = Token::Kind::Text;
            t.text = raw[k].first;
            out.tokens.push_back(std::move(t));
            continue;
        }
        auto t = classify(raw[k].first);
        if (!t) return t.error();
        out.tokens.push_back(std::move(t.value()));
    }
    return out;
}

bool is_coordinate(const Token& t)
{
    return t.kind == Token::Kind::Absolute || t.kind == Token::Kind::Relative ||
           t.kind == Token::Kind::Polar;
}

core::Result<Point2> resolve_point(const Token& t, Point2 last)
{
    switch (t.kind) {
    case Token::Kind::Absolute: return Point2{core::mm_from_metres(t.a), core::mm_from_metres(t.b)};
    case Token::Kind::Relative:
        return Point2{last.x + core::mm_from_metres(t.a), last.y + core::mm_from_metres(t.b)};
    case Token::Kind::Polar: {
        const double rad = t.b * kPi / 180.0;
        return Point2{last.x + core::mm_from_metres(t.a * std::cos(rad)),
                      last.y + core::mm_from_metres(t.a * std::sin(rad))};
    }
    default:
        return err(ErrorCode::InvalidArgument,
                   "Beklenen: koordinat (x,y | @dx,dy | @mesafe<açı). Girilen: " + describe(t));
    }
}

std::string describe(const Token& t)
{
    switch (t.kind) {
    case Token::Kind::Word: return "'" + t.word + "'";
    case Token::Kind::Number: return std::to_string(t.a);
    case Token::Kind::Text: return "\"" + t.text + "\"";
    case Token::Kind::Absolute:
        return "point(" + std::to_string(t.a) + "," + std::to_string(t.b) + ")";
    case Token::Kind::Relative: return "@(" + std::to_string(t.a) + "," + std::to_string(t.b) + ")";
    case Token::Kind::Polar: return "@" + std::to_string(t.a) + "<" + std::to_string(t.b);
    case Token::Kind::KeyValue:
        return t.word + "=" + (t.nested.empty() ? std::string("?") : describe(t.nested.front()));
    }
    return "?";
}

} // namespace piricad::command
