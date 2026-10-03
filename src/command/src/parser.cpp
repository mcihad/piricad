// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/parser.hpp"

#include "piricad/command/expression.hpp"

#include "point_function.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <locale>
#include <sstream>

namespace piricad::command {
namespace {

/// Appends the character an escape sequence names, and returns how far to advance.
///
/// `\n` is a NEWLINE, not the letter n. The lexer used to drop the backslash and
/// keep whatever followed, so `bicim="{taks}\n{kaks}"` silently produced the
/// letter `n` between two numbers — a plan sheet with `0,30n1,50` written in its
/// `yapılaşma` circle. Every quoting convention in the world reads these two
/// characters as a line break and so does this one now.
///
/// An unrecognised escape keeps the character that follows it, which is what lets
/// `\"` and `\\` work without a table of every letter.
std::size_t append_escape(std::string& out, std::string_view line, std::size_t at)
{
    if (at + 1 >= line.size()) {
        out += line[at];
        return 1;
    }

    switch (line[at + 1]) {
    case 'n': out += '\n'; break;
    case 't': out += '\t'; break;
    default: out += line[at + 1]; break;
    }
    return 2;
}

} // namespace

namespace {

using core::err;
using core::ErrorCode;
using core::Point2;

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

    /// LENGTH UNITS ARE OPT-IN (TODOS U-02). The command line's numbers and coordinates ask for
    /// them; the filter predicate does not, because a column's number has no unit to be
    /// converted to and `ada > 5m` would otherwise mean something the table never said.
    bool allow_units{false};

    /// The power of ten, in metres, of the unit the answer is wanted in: 0 metres, -3
    /// millimetres. A literal's unit shifts its DECIMAL exponent by the difference, so the
    /// same quantity written two ways reads as the same double (`1250 cm` and `12.5 m` are
    /// both 12.5e0), which a multiplication by 0.01 does not promise.
    int target_exp{0};

    /// What the last value computed IS: 0 a plain number, 1 a length. Tracked so that `2m+50`
    /// and `2m*3m` are refused instead of computed in a unit nobody named.
    int dim{0};
    bool used_unit{false}; ///< some literal carried a unit

    void fail(std::string reason)
    {
        failed = true;
        why    = std::move(reason);
    }

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
        double v    = multiplicative();
        const int d = dim;
        while (!failed) {
            skip();
            const char op = i < s.size() ? s[i] : '\0';
            if (op != '+' && op != '-') break;
            ++i;
            const double w = multiplicative();
            if (failed) return 0.0;
            if (dim != d) {
                fail("uzunluk ile birimsiz sayı toplanamaz; birimi her sayıya yazın, örneğin "
                     "(2m+50cm)");
                return 0.0;
            }
            v = op == '+' ? v + w : v - w;
        }
        dim = d;
        return v;
    }

    double multiplicative()
    {
        double v = power();
        int d    = dim;
        while (!failed) {
            skip();
            const char op = i < s.size() ? s[i] : '\0';
            if (op != '*' && op != '/' && op != '%') break;
            ++i;
            const double w = power();
            if (failed) return 0.0;
            if (op == '*') {
                if (d == 1 && dim == 1) {
                    fail("uzunluk ile uzunluk çarpılamaz; alan birimi desteklenmez");
                    return 0.0;
                }
                d += dim;
                v *= w;
            } else if (op == '/') {
                if (w == 0.0) {
                    fail("sıfıra bölme");
                    return 0.0;
                }
                if (dim == 1) {
                    if (d != 1) {
                        fail("birimsiz sayı uzunluğa bölünemez");
                        return 0.0;
                    }
                    d = 0; // a length over a length is a ratio: the unit cancels
                }
                v /= w;
            } else {
                if (w == 0.0) {
                    fail("sıfıra göre mod");
                    return 0.0;
                }
                if (dim != d) {
                    fail("uzunluk ile birimsiz sayının kalanı alınamaz");
                    return 0.0;
                }
                v = std::fmod(v, w);
            }
        }
        dim = d;
        return v;
    }

    double power()
    {
        const double base = unary();
        const int db      = dim;
        skip();
        if (!failed && i < s.size() && s[i] == '^') {
            ++i;
            const double exponent = power(); // right-associative
            if (!failed && (db == 1 || dim == 1)) {
                fail("uzunluk bir üsse yükseltilemez ve üs olamaz");
                return 0.0;
            }
            dim = 0;
            return std::pow(base, exponent);
        }
        dim = db;
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

    /// The unit word after a number — `m`, `cm` — as the power of ten it stands for, and the
    /// position after it. Spaces before it are allowed (`12.5 m`); letters stuck to a digit or
    /// to another letter are not a unit and are refused by name.
    bool unit_after_number(int& exponent)
    {
        std::size_t j = i;
        while (j < s.size() && is_space(s[j]))
            ++j;
        std::size_t w = j;
        while (w < s.size() && ((s[w] >= 'a' && s[w] <= 'z') || (s[w] >= 'A' && s[w] <= 'Z')))
            ++w;
        if (w == j) return false;

        std::string word;
        for (std::size_t k = j; k < w; ++k)
            word += static_cast<char>(s[k] | 0x20); // ASCII letters only: lower-cased, no locale
        if (word == "mm")
            exponent = -3;
        else if (word == "cm")
            exponent = -2;
        else if (word == "dm")
            exponent = -1;
        else if (word == "m")
            exponent = 0;
        else if (word == "km")
            exponent = 3;
        else {
            fail("bilinmeyen birim '" + word + "' (mm, cm, dm, m ya da km yazın)");
            return false;
        }
        i = w;
        return true;
    }

    double primary()
    {
        skip();
        dim = 0;
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

        // Hexadecimal, because a colour is written 0xAARRGGBB everywhere a user
        // meets one — in TERCİH, in the settings file, in the catalogue, in every
        // CAD manual. The settings parser already accepted it and this one did
        // not, so `STİL renk=0xFF2E7D32` failed while `TERCİH arkaplan 0xFF101418`
        // worked: the same kind of value, two notations, one of them
        // silently wrong. There is still ONE parser (CLAUDE.md 5.11); it just
        // reads one more spelling of a number.
        if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
            const std::size_t digits = i + 2;
            std::uint64_t value      = 0;
            std::size_t j            = digits;
            for (; j < s.size(); ++j) {
                const char c = s[j];
                int d        = -1;
                if (c >= '0' && c <= '9')
                    d = c - '0';
                else if (c >= 'a' && c <= 'f')
                    d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F')
                    d = c - 'A' + 10;
                else
                    break;

                // 0xAARRGGBB is eight digits; refusing past sixteen keeps the
                // shift below defined rather than wrapping into a silent colour.
                if (j - digits >= 16) {
                    failed = true;
                    why    = "onaltılık sayı çok uzun (en fazla 16 basamak)";
                    return 0.0;
                }
                value = (value << 4) | static_cast<std::uint64_t>(d);
            }
            if (j == digits) {
                failed = true;
                why    = "'0x' sonrası onaltılık basamak bekleniyordu";
                return 0.0;
            }
            i = j;
            return static_cast<double>(value);
        }

        const std::size_t start = i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.'))
            ++i;
        if (i == start) {
            failed = true;
            why    = "sayı bekleniyordu (konum " + std::to_string(i) + ")";
            return 0.0;
        }
        const std::string literal(s.substr(start, i - start));

        // `1.2.3` used to be read as 1.2 and the rest dropped without a word — a date, a
        // version or a sheet number typed as a value became a different number. It is not a
        // number, and the caller that falls back to a word now gets to.
        if (std::ranges::count(literal, '.') > 1) {
            fail("geçersiz sayı '" + literal + "' (birden çok ondalık nokta)");
            return 0.0;
        }

        int unit_exp = 0;
        if (allow_units && unit_after_number(unit_exp)) {
            used_unit = true;
            dim       = 1;
            return core::parse_decimal(literal + "e" + std::to_string(unit_exp - target_exp))
                .value_or(0.0);
        }
        if (failed) return 0.0;
        return core::parse_decimal(literal).value_or(0.0);
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

} // namespace

/// THE classifier: what one already-unquoted token of a command line is.
///
/// `depth` is how many point functions this token sits inside; it is passed on
/// rather than tracked, so the recursion between a call and its arguments is
/// bounded by `detail::kMaxCallDepth` from wherever it was entered
/// (point_function.hpp).
core::Result<Token> detail::classify(std::string_view raw, int depth)
{
    Token t;

    // A POINT FUNCTION, before anything else, because `orta(0,0,10,10)` is one
    // token of a shape nothing else here reads: `is_call_text` accepts only a
    // name this grammar declares, followed by a parenthesis that closes at the
    // very end, so an ordinary word that happens to hold brackets is untouched
    // (TODOS-CAD P1a-1).
    if (detail::is_call_text(raw)) return detail::parse_call(raw, depth);

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
            auto inner = classify(rest, depth);
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
            // `@100<45g`: a one-letter unit suffix on the ANGLE — g grad, d degree,
            // r radian — names the unit for this one coordinate, whatever the
            // session's `core.aci.birim` says. It is taken off before the
            // expression is read, so `@100<(40+5)g` works too. Any other trailing
            // letter is left for the expression parser to refuse by name, so a
            // typo is reported and never silently read as a suffix.
            std::optional<core::AngleUnit> unit;
            if (core::AngleUnit named{};
                b.size() > 1 && core::angle_unit_from_suffix(b.back(), named)) {
                unit = named;
                b.remove_suffix(1);
            }

            ExprParser pa(a);
            pa.allow_units = true; // the distance is a length: `@12.5m<45`
            const double d = pa.parse();
            ExprParser pb(b);
            const double ang = pb.parse();
            if (pa.failed) return err(ErrorCode::ParseError, "Kutupsal mesafe: " + pa.why);
            if (pb.failed) return err(ErrorCode::ParseError, "Kutupsal açı: " + pb.why);
            t.kind       = Token::Kind::Polar;
            t.a          = d;
            t.b          = ang;
            t.angle_unit = unit;
            return t;
        }

        if (split_pair(body, a, b)) {
            ExprParser pa(a);
            ExprParser pb(b);
            pa.allow_units = pb.allow_units = true; // `@1250cm,30`: metres, whatever was written
            const double dx                 = pa.parse();
            const double dy                 = pb.parse();
            if (pa.failed)
                return err(ErrorCode::ParseError, "Göreli dx: " + pa.why + "; yazılan '" +
                                                      std::string(a) + "'. Örnek: @50,30");
            if (pb.failed)
                return err(ErrorCode::ParseError, "Göreli dy: " + pb.why + "; yazılan '" +
                                                      std::string(b) + "'. Örnek: @50,30");
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
            ExprParser pb(b);
            pa.allow_units = pb.allow_units = true;
            const double x                  = pa.parse();
            const double y                  = pb.parse();
            // WHAT WAS TYPED and what would have worked: "sayı bekleniyordu (konum 0)" alone
            // left a new user guessing which of two numbers, and in what form (TODOS U-06).
            if (pa.failed)
                return err(ErrorCode::ParseError, "X koordinatı: " + pa.why + "; yazılan '" +
                                                      std::string(a) +
                                                      "'. Örnek: 485320.150,4310220.400");
            if (pb.failed)
                return err(ErrorCode::ParseError, "Y koordinatı: " + pb.why + "; yazılan '" +
                                                      std::string(b) +
                                                      "'. Örnek: 485320.150,4310220.400");
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
            p.allow_units  = true;
            const double v = p.parse();
            if (!p.failed) {
                t.kind = Token::Kind::Number;
                t.a    = v; // metres when a unit was written, as typed when not
                if (p.used_unit && p.dim == 1) {
                    t.is_length = true;
                    t.text      = std::string(raw); // worked out again in the parameter's unit
                }
                return t;
            }
        }
    }

    t.kind = Token::Kind::Word;
    t.word = std::string(raw);
    return t;
}

// ---- the filter predicate, CLAUDE.md 5.11 -----------------------------------
//
// A predicate is the value expression language (`expression.hpp`) asked for a yes or a no. It lives
// there since the field calculator (TODOS G-03) needed the same grammar to produce numbers and
// text: one language, so a filter typed at the prompt, in the table's filter bar and in the
// calculator mean the same thing, which is exactly the class of defect one parser exists to
// prevent.
core::Result<bool> evaluate_predicate(std::string_view expr, const FieldReader& field)
{
    // An empty filter matches everything. A user who clears the bar means "show
    // me all of it", not "show me nothing".
    bool blank = true;
    for (char c : expr)
        blank = blank && (c == ' ' || c == '\t');
    if (blank) return true;

    auto compiled = Expression::compile(expr);
    if (!compiled) return compiled.error();
    auto value = compiled.value().evaluate(field);
    if (!value)
        return err(ErrorCode::ParseError, "'" + std::string(expr) + "': " + value.error().message);
    return value.value().truthy();
}

core::Result<double> evaluate_expression(std::string_view expr)
{
    ExprParser p(expr);
    const double v = p.parse();
    if (p.failed)
        return err(ErrorCode::ParseError, "'" + std::string(expr) + "' ifadesi: " + p.why);
    return v;
}

core::Result<Quantity> evaluate_quantity(std::string_view expr, int target_exp)
{
    ExprParser p(expr);
    p.allow_units  = true;
    p.target_exp   = target_exp;
    const double v = p.parse();
    if (p.failed)
        return err(ErrorCode::ParseError, "'" + std::string(expr) + "' ifadesi: " + p.why);
    return Quantity{v, p.used_unit && p.dim == 1};
}

core::Result<double> evaluate_answer(std::string_view text, std::optional<int> length_exp)
{
    std::string t(text);
    while (!t.empty() && is_space(t.back()))
        t.pop_back();
    while (!t.empty() && is_space(t.front()))
        t.erase(t.begin());

    // THE DECIMAL COMMA, which a Turkish keyboard types and which only a prompt can take
    // (the line's grammar needs the comma for coordinates). When both separators are there
    // the one that comes LAST is the decimal one and the other groups thousands: `1.250,5`
    // and `1,250.5` are both 1250.5. Several commas and no dot is not a number.
    const std::size_t dot   = t.rfind('.');
    const std::size_t comma = t.rfind(',');
    if (comma != std::string::npos) {
        if (dot != std::string::npos && dot > comma) {
            std::erase(t, ',');
        } else if (dot != std::string::npos) {
            std::erase(t, '.');
            std::ranges::replace(t, ',', '.');
        } else if (std::ranges::count(t, ',') > 1) {
            return err(ErrorCode::ParseError, "'" + t + "' sayısında birden çok virgül var.");
        } else {
            std::ranges::replace(t, ',', '.');
        }
    }

    ExprParser p(t);
    p.allow_units  = true;
    p.target_exp   = length_exp.value_or(0);
    const double v = p.parse();
    if (p.failed)
        return err(ErrorCode::ParseError, "'" + std::string(text) + "' okunamadı: " + p.why);
    if (p.used_unit && p.dim == 1 && !length_exp)
        return err(ErrorCode::InvalidArgument, "Bu istem birimli sayı almıyor; '" +
                                                   std::string(text) +
                                                   "' yerine sayıyı birimsiz yazın.");
    return v;
}

core::Result<ParsedLine> parse_line(std::string_view line)
{
    ParsedLine out;

    /// One raw token as the scanner found it, plus what the QUOTES did to it.
    ///
    /// `quote_at` is where the first quoted run began INSIDE the assembled token,
    /// and it is the whole reason this struct exists. Losing it is what made
    /// `hedef="host=localhost dbname=x"` come apart: `classify` re-split the
    /// value at its own `=` because, by then, nothing remembered the user had
    /// quoted it. A quoted value is LITERAL — that is what quoting means here,
    /// in every shell, and in AutoCAD's command line.
    struct Raw
    {
        std::string text;
        std::size_t quote_at{std::string::npos};
    };

    std::vector<Raw> raw;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && is_space(line[i]))
            ++i;
        if (i >= line.size()) break;

        if (line[i] == '"') {
            ++i;
            std::string text;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\') {
                    i += append_escape(text, line, i);
                    continue;
                }
                text += line[i++];
            }
            if (i >= line.size())
                return err(ErrorCode::ParseError, "Komut satırında kapanmamış tırnak var.");
            ++i;
            raw.push_back(Raw{std::move(text), 0});
            continue;
        }

        // A bare token ends at whitespace, but a quoted run inside it — the value
        // of `ad="YOL KENARI"` — is absorbed whole, quotes stripped.
        std::string token;
        int depth            = 0;
        std::size_t quote_at = std::string::npos;

        while (i < line.size()) {
            const char c = line[i];

            if (c == '"') {
                ++i;
                if (quote_at == std::string::npos) quote_at = token.size();
                while (i < line.size() && line[i] != '"') {
                    if (line[i] == '\\') {
                        i += append_escape(token, line, i);
                        continue;
                    }
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
        raw.push_back(Raw{std::move(token), quote_at});
    }

    if (raw.empty()) return err(ErrorCode::ParseError, "Boş komut satırı.");

    // A UNIT WRITTEN AFTER A SPACE belongs to the number before it: `12.5 m` is `12.5m` and
    // `mesafe=1250 cm` is `mesafe=1250cm` (TODOS U-02). Only an unquoted number followed by an
    // unquoted unit word, and never the command word itself, so a name or a value that merely
    // is "m" keeps being one.
    for (std::size_t k = 1; k + 1 < raw.size();) {
        const auto ends_in_number = [](const std::string& text) {
            const std::size_t eq = text.rfind('=');
            const std::string_view tail =
                std::string_view(text).substr(eq == std::string::npos ? 0 : eq + 1);
            if (tail.empty() || !(tail.back() >= '0' && tail.back() <= '9')) return false;
            std::size_t dots = 0, digits = 0;
            for (std::size_t n = (tail.front() == '-' || tail.front() == '+') ? 1 : 0;
                 n < tail.size(); ++n) {
                if (tail[n] == '.')
                    ++dots;
                else if (tail[n] >= '0' && tail[n] <= '9')
                    ++digits;
                else
                    return false;
            }
            return dots <= 1 && digits > 0;
        };
        const auto is_unit = [](const std::string& text) {
            std::string low;
            for (const char c : text)
                low += static_cast<char>(c | 0x20);
            return text.size() <= 2 &&
                   (low == "mm" || low == "cm" || low == "dm" || low == "m" || low == "km");
        };
        if (raw[k].quote_at == std::string::npos && raw[k + 1].quote_at == std::string::npos &&
            ends_in_number(raw[k].text) && is_unit(raw[k + 1].text)) {
            raw[k].text += raw[k + 1].text;
            raw.erase(raw.begin() + static_cast<std::ptrdiff_t>(k) + 1);
            continue;
        }
        ++k;
    }

    out.command = raw.front().text;
    out.tokens.reserve(raw.size() - 1);

    for (std::size_t k = 1; k < raw.size(); ++k) {
        const std::string& text    = raw[k].text;
        const std::size_t quote_at = raw[k].quote_at;

        // Nothing was quoted: classify it as written.
        if (quote_at == std::string::npos) {
            auto t = detail::classify(text, 0);
            if (!t) return t.error();
            out.tokens.push_back(std::move(t.value()));
            continue;
        }

        // The whole token was quoted: it is text, exactly as typed.
        if (quote_at == 0) {
            Token t;
            t.kind = Token::Kind::Text;
            t.text = text;
            out.tokens.push_back(std::move(t));
            continue;
        }

        // `key="value"` — a bare key and a quoted value. The KEY still means what
        // a key means, and the VALUE is literal: it is not re-split at its own
        // `=`, not read as a coordinate pair because it holds a comma, and not
        // turned into a number because it looks like one. That is what a
        // connection string, a Windows path and a format string all need.
        const std::size_t eq = text.find('=');
        if (eq != std::string::npos && eq > 0 && eq < quote_at) {
            Token value;
            value.kind = Token::Kind::Text;
            value.text = text.substr(eq + 1);

            Token t;
            t.kind = Token::Kind::KeyValue;
            t.word = text.substr(0, eq);
            t.nested.push_back(std::move(value));
            out.tokens.push_back(std::move(t));
            continue;
        }

        // A quoted run somewhere else in a bare token — `abc"def"` and the like.
        // Rare, and the honest reading is still "the user meant this text".
        Token t;
        t.kind = Token::Kind::Text;
        t.text = text;
        out.tokens.push_back(std::move(t));
    }
    return out;
}

/// What the message says a coordinate may look like. One string, because the
/// three places that refuse a non-coordinate must not describe the grammar
/// three different ways (CLAUDE.md 5.10).
namespace {
const char* const kCoordinateForms =
    "Beklenen: koordinat (x,y | @dx,dy | @mesafe<açı | nokta fonksiyonu: orta, dik, semt, kes, "
    "ara, uzanti, xy, boyunca, n, son). Girilen: ";
} // namespace

bool is_coordinate(const Token& t)
{
    return t.kind == Token::Kind::Absolute || t.kind == Token::Kind::Relative ||
           t.kind == Token::Kind::Polar || t.kind == Token::Kind::Call;
}

core::Result<Point2> resolve_point(const Token& t, Point2 last, const ResolveContext& ctx)
{
    switch (t.kind) {
    case Token::Kind::Absolute: return Point2{core::mm_from_metres(t.a), core::mm_from_metres(t.b)};
    case Token::Kind::Relative:
        return Point2{last.x + core::mm_from_metres(t.a), last.y + core::mm_from_metres(t.b)};
    case Token::Kind::Polar: {
        // The suffix names the unit; a bare angle is in the session's. The RULE
        // is never spelled on the coordinate: which way an angle grows is the
        // session's decision (`core.aci.kural`), not the token's. The offset
        // itself comes from core — `sin_cos_udeg`, one rounding per axis — so a
        // typed polar point is the same millimetre on every platform (§7.3),
        // where the `std::cos`/`std::sin` this used to call were not.
        core::AngleConvention applied = ctx.convention;
        if (t.angle_unit) applied.unit = *t.angle_unit;
        return last + core::polar_offset(t.a, t.b, applied);
    }
    case Token::Kind::Call: return detail::resolve_call(t, last, ctx);
    default: return err(ErrorCode::InvalidArgument, kCoordinateForms + describe(t));
    }
}

core::Result<Point2> parse_point(std::string_view text, Point2 last, const ResolveContext& ctx)
{
    // A JSON string may carry blanks around the coordinate; a coordinate never
    // contains one, so they are not part of what is read.
    while (!text.empty() && is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && is_space(text.back()))
        text.remove_suffix(1);

    auto token = detail::classify(text, 0);
    if (!token) return token.error();
    if (!is_coordinate(token.value()))
        return err(ErrorCode::InvalidArgument, kCoordinateForms + describe(token.value()));
    return resolve_point(token.value(), last, ctx);
}

std::string describe(const Token& t)
{
    switch (t.kind) {
    case Token::Kind::Word: return "'" + t.word + "'";
    // A WHOLE NUMBER IS WRITTEN WHOLE: "Girilen: 10.000000" for a 10 the user typed read as a
    // different value than the one they gave (TODOS U-06).
    case Token::Kind::Number: return core::format_general(t.a, 15);
    case Token::Kind::Text: return "\"" + t.text + "\"";
    case Token::Kind::Absolute:
        return "point(" + std::to_string(t.a) + "," + std::to_string(t.b) + ")";
    case Token::Kind::Relative: return "@(" + std::to_string(t.a) + "," + std::to_string(t.b) + ")";
    case Token::Kind::Polar: {
        std::string out = "@" + std::to_string(t.a) + "<" + std::to_string(t.b);
        if (t.angle_unit) out += core::angle_unit_suffix(*t.angle_unit);
        return out;
    }
    case Token::Kind::KeyValue:
        return t.word + "=" + (t.nested.empty() ? std::string("?") : describe(t.nested.front()));
    case Token::Kind::Call: return detail::describe_call(t);
    }
    return "?";
}

} // namespace piricad::command
