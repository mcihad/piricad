// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/parser.hpp"

#include "piricad/core/text.hpp"

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

// ---- the filter predicate, CLAUDE.md 5.11 -----------------------------------
//
// The SAME grammar as `evaluate_expression`, wrapped in comparison and boolean
// layers. It is here rather than in the attribute table for the reason 5.11
// gives: a second grammar would drift from this one, and then a filter typed at
// the prompt would mean something different from the same filter typed in the
// table — which is exactly the class of defect one parser exists to prevent.
namespace {

/// A value inside a predicate: a number, a string, or nothing at all.
///
/// NULL IS ITS OWN THING. A cell nobody filled is not an empty string and not a
/// zero; `IS NULL` is how a user asks for it and every comparison against it is
/// false, which is what SQL does and what every GIS user already expects.
struct Cell
{
    bool null{true};
    bool numeric{false};
    double number{0.0};
    std::string text;

    static Cell of_number(double v) { return Cell{false, true, v, {}}; }

    static Cell of_text(std::string v) { return Cell{false, false, 0.0, std::move(v)}; }
};

class PredicateParser
{
public:
    PredicateParser(std::string_view src, const FieldReader& field) : s_(src), field_(field) {}

    bool parse_or()
    {
        bool value = parse_and();
        while (keyword("OR")) {
            const bool rhs = parse_and();
            value          = value || rhs;
        }
        return value;
    }

    bool failed{false};
    std::string why;

private:
    void skip()
    {
        while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\t'))
            ++at_;
    }

    /// Consumes `word` when it is the next token, case-insensitively over ASCII.
    /// The keywords are ASCII by construction (AND, OR, NOT, IS, NULL), so
    /// `std::toupper` is not reached for Turkish text and 5.6 is not in play.
    bool keyword(const char* word)
    {
        skip();
        const std::size_t n = std::strlen(word);
        if (at_ + n > s_.size()) return false;
        for (std::size_t i = 0; i < n; ++i) {
            const char c = s_[at_ + i];
            const char u = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
            if (u != word[i]) return false;
        }
        // A keyword must not run into an identifier: `ORDER` is not `OR`.
        if (at_ + n < s_.size()) {
            const char after = s_[at_ + n];
            if ((after >= 'A' && after <= 'Z') || (after >= 'a' && after <= 'z') || after == '_')
                return false;
        }
        at_ += n;
        return true;
    }

    bool parse_and()
    {
        bool value = parse_not();
        while (keyword("AND")) {
            const bool rhs = parse_not();
            value          = value && rhs;
        }
        return value;
    }

    bool parse_not()
    {
        if (keyword("NOT")) return !parse_not();

        skip();
        if (at_ < s_.size() && s_[at_] == '(') {
            ++at_;
            const bool inner = parse_or();
            skip();
            if (at_ < s_.size() && s_[at_] == ')')
                ++at_;
            else
                fail("Kapanmayan parantez");
            return inner;
        }
        return parse_compare();
    }

    bool parse_compare()
    {
        const Cell lhs = parse_cell();
        skip();

        if (keyword("IS")) {
            const bool negate = keyword("NOT");
            if (!keyword("NULL")) {
                fail("'IS' sonrası beklenen: NULL veya NOT NULL");
                return false;
            }
            return negate ? !lhs.null : lhs.null;
        }

        static const char* const kOps[] = {"!=", "<>", "<=", ">=", "=", "<", ">"};
        const char* op                  = nullptr;
        for (const char* candidate : kOps) {
            const std::size_t n = std::strlen(candidate);
            if (s_.compare(at_, n, candidate) == 0) {
                op = candidate;
                at_ += n;
                break;
            }
        }
        if (op == nullptr) {
            fail("Beklenen bir karşılaştırma: = != < <= > >= veya IS NULL");
            return false;
        }

        const Cell rhs = parse_cell();

        // Any comparison against an unfilled cell is false, INCLUDING `!=`. A
        // measurement nobody took is not "different from 5"; it is unknown, and
        // saying otherwise would put every unsurveyed parcel into every filter.
        if (lhs.null || rhs.null) return false;

        const int order = compare(lhs, rhs);
        if (std::strcmp(op, "=") == 0) return order == 0;
        if (std::strcmp(op, "!=") == 0 || std::strcmp(op, "<>") == 0) return order != 0;
        if (std::strcmp(op, "<") == 0) return order < 0;
        if (std::strcmp(op, "<=") == 0) return order <= 0;
        if (std::strcmp(op, ">") == 0) return order > 0;
        return order >= 0;
    }

    static int compare(const Cell& a, const Cell& b)
    {
        if (a.numeric && b.numeric) return a.number < b.number ? -1 : (a.number > b.number ? 1 : 0);

        // Mixed types compare as TEXT, because a cell is text until somebody says
        // otherwise: an `ada_no` column holding `1284` in one row and `1284/A` in
        // the next is normal in cadastre, and refusing the comparison would make
        // the filter fail on the whole layer rather than on the odd row.
        const std::string left  = a.numeric ? format_number(a.number) : a.text;
        const std::string right = b.numeric ? format_number(b.number) : b.text;
        return left.compare(right) < 0 ? -1 : (left == right ? 0 : 1);
    }

    static std::string format_number(double v)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << v;
        return out.str();
    }

    Cell parse_cell()
    {
        skip();
        if (at_ >= s_.size()) {
            fail("İfade beklenmedik biçimde bitti");
            return {};
        }

        // "column" — the double quote is SQL's, and every GIS user has it.
        if (s_[at_] == '"') {
            const std::size_t close = s_.find('"', at_ + 1);
            if (close == std::string_view::npos) {
                fail("Kapanmayan sütun adı tırnağı");
                return {};
            }
            const std::string name(s_.substr(at_ + 1, close - at_ - 1));
            at_ = close + 1;

            if (!field_) return {};
            const std::optional<std::string> got = field_(name);
            if (!got) return {};

            // A cell that reads as a number IS a number, so `> 2000` works on a
            // text column of numbers — which is what an attribute table holds.
            const std::optional<double> numeric = as_number(*got);
            return numeric ? Cell::of_number(*numeric) : Cell::of_text(*got);
        }

        // 'string'
        if (s_[at_] == '\'') {
            const std::size_t close = s_.find('\'', at_ + 1);
            if (close == std::string_view::npos) {
                fail("Kapanmayan metin tırnağı");
                return {};
            }
            Cell cell = Cell::of_text(std::string(s_.substr(at_ + 1, close - at_ - 1)));
            at_       = close + 1;
            return cell;
        }

        // Anything else is arithmetic, read by the one expression parser.
        const std::size_t start = at_;
        int depth               = 0;
        while (at_ < s_.size()) {
            const char c = s_[at_];
            if (c == '(') ++depth;
            if (c == ')') {
                if (depth == 0) break;
                --depth;
            }
            if (depth == 0 && (c == '=' || c == '<' || c == '>' || c == '!')) break;
            if (depth == 0 && starts_keyword()) break;
            ++at_;
        }

        const std::string_view body = s_.substr(start, at_ - start);
        ExprParser inner(body);
        const double value = inner.parse();
        if (inner.failed) {
            fail("'" + std::string(body) + "': " + inner.why);
            return {};
        }
        return Cell::of_number(value);
    }

    /// Whether the cursor is at the start of a boolean keyword, so a bare term
    /// stops before `AND`, `OR` and `IS` rather than swallowing them.
    bool starts_keyword() const
    {
        if (at_ == 0 || (s_[at_ - 1] != ' ' && s_[at_ - 1] != '\t')) return false;
        for (const char* word : {"AND", "OR", "NOT", "IS"}) {
            const std::size_t n = std::strlen(word);
            if (at_ + n > s_.size()) continue;
            bool same = true;
            for (std::size_t i = 0; i < n && same; ++i) {
                const char c = s_[at_ + i];
                const char u = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
                same         = u == word[i];
            }
            if (same) return true;
        }
        return false;
    }

    static std::optional<double> as_number(const std::string& text)
    {
        if (text.empty()) return std::nullopt;

        std::istringstream in(text);
        in.imbue(std::locale::classic());
        double v = 0.0;
        in >> v;
        if (in.fail()) return std::nullopt;

        // Trailing anything means it was not a number: `1284/A` must stay text.
        char extra = 0;
        if (in >> extra) return std::nullopt;
        return v;
    }

    void fail(std::string message)
    {
        if (failed) return;
        failed = true;
        why    = std::move(message);
    }

    std::string_view s_;
    const FieldReader& field_;
    std::size_t at_{0};
};

} // namespace

core::Result<bool> evaluate_predicate(std::string_view expr, const FieldReader& field)
{
    // An empty filter matches everything. A user who clears the bar means "show
    // me all of it", not "show me nothing".
    bool blank = true;
    for (char c : expr)
        blank = blank && (c == ' ' || c == '\t');
    if (blank) return true;

    PredicateParser p(expr, field);
    const bool value = p.parse_or();
    if (p.failed) return err(ErrorCode::ParseError, "'" + std::string(expr) + "': " + p.why);
    return value;
}

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

    out.command = raw.front().text;
    out.tokens.reserve(raw.size() - 1);

    for (std::size_t k = 1; k < raw.size(); ++k) {
        const std::string& text    = raw[k].text;
        const std::size_t quote_at = raw[k].quote_at;

        // Nothing was quoted: classify it as written.
        if (quote_at == std::string::npos) {
            auto t = classify(text);
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
