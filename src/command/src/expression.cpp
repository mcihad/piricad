// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/expression.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace piricad::command {

using core::err;
using core::ErrorCode;

// ------------------------------------------------------------------ values --

ExprValue ExprValue::of_number(double v)
{
    ExprValue out;
    out.kind   = Kind::Number;
    out.number = v;
    return out;
}

ExprValue ExprValue::of_text(std::string v)
{
    ExprValue out;
    out.kind = Kind::Text;
    out.text = std::move(v);
    return out;
}

ExprValue ExprValue::of_bool(bool v)
{
    ExprValue out;
    out.kind    = Kind::Bool;
    out.boolean = v;
    return out;
}

namespace {

/// A whole number is written whole, anything else with all the digits a double has and no more.
std::string number_text(double v)
{
    if (v == std::floor(v) && std::abs(v) < 1e15) return std::to_string(static_cast<long long>(v));
    return core::format_general(v, 15);
}

/// The number a cell's text is, when the whole of it is one.
std::optional<double> whole_number(std::string_view text)
{
    if (text.empty()) return std::nullopt;
    const auto value = core::parse_decimal(text);
    if (!value || !std::isfinite(*value)) return std::nullopt;
    return value;
}

} // namespace

ExprValue ExprValue::of_cell(const std::optional<std::string>& cell)
{
    if (!cell) return null();
    if (const auto number = whole_number(*cell); number) {
        ExprValue out = of_number(*number);
        out.text      = *cell;
        out.from_cell = true;
        return out;
    }
    return of_text(*cell);
}

std::string ExprValue::as_text() const
{
    switch (kind) {
    case Kind::Null: return {};
    case Kind::Number: return from_cell ? text : number_text(number);
    case Kind::Text: return text;
    case Kind::Bool: return boolean ? "evet" : "hayır";
    }
    return {};
}

bool ExprValue::truthy() const noexcept
{
    switch (kind) {
    case Kind::Null: return false;
    case Kind::Number: return number != 0.0;
    case Kind::Text: return !text.empty();
    case Kind::Bool: return boolean;
    }
    return false;
}

// ------------------------------------------------------------------- tree ---

struct Expression::Node
{
    enum class Op : std::uint8_t {
        Literal,
        Column,
        Neg,
        Not,
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        Pow,
        Concat,
        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,
        And,
        Or,
        IsNull,
        IsNotNull,
        Call,
        Case
    };
    enum class Fn : std::uint8_t {
        Coalesce,
        If,
        NullIf,
        Round,
        Abs,
        Floor,
        Ceil,
        Sqrt,
        Pow,
        Min,
        Max,
        Upper,
        Lower,
        Trim,
        Length,
        Left,
        Right,
        Substr,
        Replace,
        Lpad,
        Rpad,
        ToText,
        ToNumber,
        Contains,
        StartsWith,
        EndsWith
    };

    Op op{Op::Literal};
    Fn fn{Fn::Coalesce};
    ExprValue literal;
    std::string name; ///< Column: the column; Call: the function as written
    std::vector<std::shared_ptr<const Node>> kids;
    bool has_else{false}; ///< Case: the last kid is the ELSE
};

namespace {

using Node    = Expression::Node;
using NodePtr = std::shared_ptr<const Node>;

// ------------------------------------------------------------------ lexer ---

enum class Tok : std::uint8_t { End, Number, String, Column, Dollar, Ident, Sym };

struct Token
{
    Tok kind{Tok::End};
    std::string text; ///< the contents (String, Column, Ident, Dollar) or the symbol
    double number{0.0};
    std::size_t at{0};
};

bool ident_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
}

bool ident_part(unsigned char c)
{
    return ident_start(c) || (c >= '0' && c <= '9');
}

struct Lexer
{
    std::string_view s;
    std::size_t i{0};
    std::string error;

    explicit Lexer(std::string_view text) : s(text) {}

    void fail(std::string why, std::size_t at)
    {
        if (error.empty()) error = why + " (konum " + std::to_string(at) + ")";
    }

    std::vector<Token> run()
    {
        std::vector<Token> out;
        while (true) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
                ++i;
            Token t;
            t.at = i;
            if (i >= s.size()) {
                out.push_back(t);
                return out;
            }
            const auto c = static_cast<unsigned char>(s[i]);

            if ((c >= '0' && c <= '9') ||
                (c == '.' && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '9')) {
                std::size_t j = i;
                while (j < s.size() && ((s[j] >= '0' && s[j] <= '9') || s[j] == '.'))
                    ++j;
                if (j < s.size() && (s[j] == 'e' || s[j] == 'E')) {
                    std::size_t k = j + 1;
                    if (k < s.size() && (s[k] == '+' || s[k] == '-')) ++k;
                    if (k < s.size() && s[k] >= '0' && s[k] <= '9') {
                        while (k < s.size() && s[k] >= '0' && s[k] <= '9')
                            ++k;
                        j = k;
                    }
                }
                const auto value = core::parse_decimal(s.substr(i, j - i));
                if (!value) {
                    fail("'" + std::string(s.substr(i, j - i)) + "' geçerli bir sayı değil", i);
                    return out;
                }
                // `12abc` is no number followed by a word; it is a typing mistake.
                if (j < s.size() && ident_start(static_cast<unsigned char>(s[j]))) {
                    fail("sayının hemen ardından harf gelemez", j);
                    return out;
                }
                t.kind   = Tok::Number;
                t.number = *value;
                i        = j;
            } else if (c == '\'' || c == '"') {
                // 'text' and "column", the same quote doubled to write itself (SQL's rule).
                const char quote = static_cast<char>(c);
                std::string body;
                std::size_t j = i + 1;
                bool closed   = false;
                while (j < s.size()) {
                    if (s[j] == quote) {
                        if (j + 1 < s.size() && s[j + 1] == quote) {
                            body += quote;
                            j += 2;
                            continue;
                        }
                        closed = true;
                        ++j;
                        break;
                    }
                    body += s[j++];
                }
                if (!closed) {
                    fail(quote == '"' ? "Kapanmayan sütun adı tırnağı" : "Kapanmayan metin tırnağı",
                         i);
                    return out;
                }
                t.kind = quote == '"' ? Tok::Column : Tok::String;
                t.text = std::move(body);
                i      = j;
            } else if (c == '$') {
                std::size_t j = i + 1;
                while (j < s.size() && ident_part(static_cast<unsigned char>(s[j])))
                    ++j;
                if (j == i + 1) {
                    fail("'$' ardından bir ad gelmeli ($alan, $uzunluk, $x …)", i);
                    return out;
                }
                t.kind = Tok::Dollar;
                t.text = std::string(s.substr(i, j - i));
                i      = j;
            } else if (ident_start(c)) {
                std::size_t j = i;
                while (j < s.size() && ident_part(static_cast<unsigned char>(s[j])))
                    ++j;
                t.kind = Tok::Ident;
                t.text = std::string(s.substr(i, j - i));
                i      = j;
            } else {
                static const char* const kTwo[] = {"||", "!=", "<>", "<=", ">="};
                t.kind                          = Tok::Sym;
                bool two                        = false;
                for (const char* candidate : kTwo)
                    if (s.compare(i, 2, candidate) == 0) {
                        t.text = candidate;
                        i += 2;
                        two = true;
                        break;
                    }
                if (!two) {
                    if (std::strchr("+-*/%^=<>(),", static_cast<char>(c)) == nullptr) {
                        fail(std::string("beklenmeyen '") + static_cast<char>(c) + "' karakteri",
                             i);
                        return out;
                    }
                    t.text = std::string(1, static_cast<char>(c));
                    ++i;
                }
            }
            out.push_back(std::move(t));
        }
    }
};

std::string upper_ascii(std::string_view w)
{
    std::string out(w);
    for (char& c : out)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    return out;
}

// ----------------------------------------------------------------- parser ---

constexpr int kMaxDepth = 120;

struct Parser
{
    std::vector<Token> toks;
    std::size_t p{0};
    int depth{0};
    std::string error;
    std::vector<std::string> columns;

    const Token& peek() const { return toks[p]; }

    void fail(std::string why)
    {
        if (error.empty()) error = why + " (konum " + std::to_string(peek().at) + ")";
    }

    bool sym(const char* s) const { return peek().kind == Tok::Sym && peek().text == s; }

    bool word(const char* w) const
    {
        return peek().kind == Tok::Ident && upper_ascii(peek().text) == w;
    }

    bool take_sym(const char* s)
    {
        if (!sym(s)) return false;
        ++p;
        return true;
    }

    bool take_word(const char* w)
    {
        if (!word(w)) return false;
        ++p;
        return true;
    }

    static NodePtr make(Node::Op op, std::vector<NodePtr> kids)
    {
        auto n  = std::make_shared<Node>();
        n->op   = op;
        n->kids = std::move(kids);
        return n;
    }

    struct Depth
    {
        Parser& self;

        explicit Depth(Parser& s) : self(s) { ++self.depth; }

        ~Depth() { --self.depth; }
    };

    NodePtr parse_all()
    {
        NodePtr root = parse_or();
        if (!error.empty()) return nullptr;
        if (peek().kind != Tok::End) {
            fail("beklenmeyen '" + (peek().kind == Tok::Sym ? peek().text : peek().text) +
                 "'; bir işleç ya da bitiş bekleniyordu");
            return nullptr;
        }
        return root;
    }

    NodePtr parse_or()
    {
        const Depth d(*this);
        if (depth > kMaxDepth) {
            fail("ifade çok iç içe");
            return nullptr;
        }
        NodePtr left = parse_and();
        while (error.empty() && take_word("OR")) {
            NodePtr right = parse_and();
            if (!error.empty()) return nullptr;
            left = make(Node::Op::Or, {left, right});
        }
        return left;
    }

    NodePtr parse_and()
    {
        NodePtr left = parse_not();
        while (error.empty() && take_word("AND")) {
            NodePtr right = parse_not();
            if (!error.empty()) return nullptr;
            left = make(Node::Op::And, {left, right});
        }
        return left;
    }

    NodePtr parse_not()
    {
        if (take_word("NOT")) {
            const Depth d(*this);
            if (depth > kMaxDepth) {
                fail("ifade çok iç içe");
                return nullptr;
            }
            NodePtr inner = parse_not();
            return error.empty() ? make(Node::Op::Not, {inner}) : nullptr;
        }
        return parse_compare();
    }

    NodePtr parse_compare()
    {
        NodePtr left = parse_concat();
        if (!error.empty()) return nullptr;

        if (take_word("IS")) {
            const bool negate = take_word("NOT");
            if (!take_word("NULL")) {
                fail("'IS' sonrası beklenen: NULL veya NOT NULL");
                return nullptr;
            }
            return make(negate ? Node::Op::IsNotNull : Node::Op::IsNull, {left});
        }

        struct Cmp
        {
            const char* sym;
            Node::Op op;
        };

        static const Cmp kCmp[] = {{"!=", Node::Op::Ne}, {"<>", Node::Op::Ne}, {"<=", Node::Op::Le},
                                   {">=", Node::Op::Ge}, {"=", Node::Op::Eq},  {"<", Node::Op::Lt},
                                   {">", Node::Op::Gt}};
        for (const Cmp& c : kCmp)
            if (take_sym(c.sym)) {
                NodePtr right = parse_concat();
                if (!error.empty()) return nullptr;
                return make(c.op, {left, right});
            }
        return left;
    }

    NodePtr parse_concat()
    {
        NodePtr left = parse_additive();
        while (error.empty() && take_sym("||")) {
            NodePtr right = parse_additive();
            if (!error.empty()) return nullptr;
            left = make(Node::Op::Concat, {left, right});
        }
        return left;
    }

    NodePtr parse_additive()
    {
        NodePtr left = parse_term();
        while (error.empty()) {
            Node::Op op;
            if (take_sym("+"))
                op = Node::Op::Add;
            else if (take_sym("-"))
                op = Node::Op::Sub;
            else
                break;
            NodePtr right = parse_term();
            if (!error.empty()) return nullptr;
            left = make(op, {left, right});
        }
        return left;
    }

    NodePtr parse_term()
    {
        NodePtr left = parse_unary();
        while (error.empty()) {
            Node::Op op;
            if (take_sym("*"))
                op = Node::Op::Mul;
            else if (take_sym("/"))
                op = Node::Op::Div;
            else if (take_sym("%"))
                op = Node::Op::Mod;
            else
                break;
            NodePtr right = parse_unary();
            if (!error.empty()) return nullptr;
            left = make(op, {left, right});
        }
        return left;
    }

    NodePtr parse_unary()
    {
        const Depth d(*this);
        if (depth > kMaxDepth) {
            fail("ifade çok iç içe");
            return nullptr;
        }
        if (take_sym("-")) {
            NodePtr inner = parse_unary();
            return error.empty() ? make(Node::Op::Neg, {inner}) : nullptr;
        }
        if (take_sym("+")) return parse_unary();
        return parse_power();
    }

    NodePtr parse_power()
    {
        NodePtr base = parse_primary();
        if (!error.empty()) return nullptr;
        if (take_sym("^")) {
            NodePtr exponent = parse_unary(); // right-associative
            if (!error.empty()) return nullptr;
            return make(Node::Op::Pow, {base, exponent});
        }
        return base;
    }

    NodePtr literal(ExprValue v)
    {
        auto n     = std::make_shared<Node>();
        n->op      = Node::Op::Literal;
        n->literal = std::move(v);
        return n;
    }

    NodePtr column(std::string name)
    {
        if (std::find(columns.begin(), columns.end(), name) == columns.end())
            columns.push_back(name);
        auto n  = std::make_shared<Node>();
        n->op   = Node::Op::Column;
        n->name = std::move(name);
        return n;
    }

    struct FnInfo
    {
        const char* name;
        Node::Fn fn;
        int min_args;
        int max_args; ///< -1: any number
        const char* usage;
        const char* help; ///< one Turkish sentence for the calculator's function list
    };

    /// THE ONE TABLE of the language's functions: the parser reads names and arities from it, and
    /// the calculator window and the manual list what it holds (`expression_functions`) — a
    /// function added here is in all three or in none.
    static const std::vector<FnInfo>& function_table()
    {
        static const std::vector<FnInfo> kFns = {
            {"coalesce", Node::Fn::Coalesce, 1, -1, "coalesce(a, b, …)",
             "İlk boş olmayan değer; hiçbiri yoksa boş."},
            {"if", Node::Fn::If, 3, 3, "if(koşul, evetse, hayırsa)",
             "Koşula göre iki değerden biri."},
            {"nullif", Node::Fn::NullIf, 2, 2, "nullif(a, b)", "a, b'ye eşitse boş; değilse a."},
            {"round", Node::Fn::Round, 1, 2, "round(x, basamak)",
             "Sayıyı verilen ondalık basamağa yuvarlar."},
            {"abs", Node::Fn::Abs, 1, 1, "abs(x)", "Mutlak değer."},
            {"floor", Node::Fn::Floor, 1, 1, "floor(x)", "Aşağıya yuvarlanmış tam sayı."},
            {"ceil", Node::Fn::Ceil, 1, 1, "ceil(x)", "Yukarıya yuvarlanmış tam sayı."},
            {"sqrt", Node::Fn::Sqrt, 1, 1, "sqrt(x)", "Karekök; negatif sayıda hata."},
            {"pow", Node::Fn::Pow, 2, 2, "pow(x, üs)", "x'in üssü."},
            {"min", Node::Fn::Min, 1, -1, "min(a, b, …)", "En küçük değer; boşlar atlanır."},
            {"max", Node::Fn::Max, 1, -1, "max(a, b, …)", "En büyük değer; boşlar atlanır."},
            {"upper", Node::Fn::Upper, 1, 1, "upper(metin)",
             "Türkçe kurallarıyla büyük harf (i → İ)."},
            {"lower", Node::Fn::Lower, 1, 1, "lower(metin)",
             "Türkçe kurallarıyla küçük harf (I → ı)."},
            {"trim", Node::Fn::Trim, 1, 1, "trim(metin)", "Baştaki ve sondaki boşlukları atar."},
            {"length", Node::Fn::Length, 1, 1, "length(metin)", "Karakter sayısı."},
            {"left", Node::Fn::Left, 2, 2, "left(metin, n)", "Metnin ilk n karakteri."},
            {"right", Node::Fn::Right, 2, 2, "right(metin, n)", "Metnin son n karakteri."},
            {"substr", Node::Fn::Substr, 2, 3, "substr(metin, başlangıç, uzunluk)",
             "Metnin bir parçası; başlangıç 1'den sayılır."},
            {"replace", Node::Fn::Replace, 3, 3, "replace(metin, ara, yerine)",
             "Bulunan her parçayı değiştirir."},
            {"lpad", Node::Fn::Lpad, 3, 3, "lpad(metin, uzunluk, dolgu)",
             "Solu dolgu karakteriyle doldurur."},
            {"rpad", Node::Fn::Rpad, 3, 3, "rpad(metin, uzunluk, dolgu)",
             "Sağı dolgu karakteriyle doldurur."},
            {"to_text", Node::Fn::ToText, 1, 1, "to_text(x)", "Değeri metne çevirir."},
            {"to_number", Node::Fn::ToNumber, 1, 1, "to_number(metin)",
             "Metni sayıya çevirir; olmazsa hata."},
            {"contains", Node::Fn::Contains, 2, 2, "contains(metin, parça)",
             "Metin parçayı içeriyor mu."},
            {"starts_with", Node::Fn::StartsWith, 2, 2, "starts_with(metin, önek)",
             "Metin bu önekle başlıyor mu."},
            {"ends_with", Node::Fn::EndsWith, 2, 2, "ends_with(metin, sonek)",
             "Metin bu sonekle bitiyor mu."},
        };
        return kFns;
    }

    static const FnInfo* function_named(const std::string& lower)
    {
        for (const FnInfo& f : function_table())
            if (lower == f.name) return &f;
        return nullptr;
    }

    NodePtr parse_primary()
    {
        const Token t = peek();
        switch (t.kind) {
        case Tok::Number: ++p; return literal(ExprValue::of_number(t.number));
        case Tok::String: ++p; return literal(ExprValue::of_text(t.text));
        case Tok::Column:
        case Tok::Dollar: ++p; return column(t.text);
        case Tok::Sym:
            if (t.text == "(") {
                ++p;
                NodePtr inner = parse_or();
                if (!error.empty()) return nullptr;
                if (!take_sym(")")) {
                    fail("Kapanmayan parantez");
                    return nullptr;
                }
                return inner;
            }
            fail("beklenmeyen '" + t.text + "'; bir değer bekleniyordu");
            return nullptr;
        case Tok::End: fail("İfade beklenmedik biçimde bitti"); return nullptr;
        case Tok::Ident: break;
        }

        const std::string up = upper_ascii(t.text);
        if (up == "NULL") {
            ++p;
            return literal(ExprValue::null());
        }
        if (up == "TRUE") {
            ++p;
            return literal(ExprValue::of_bool(true));
        }
        if (up == "FALSE") {
            ++p;
            return literal(ExprValue::of_bool(false));
        }
        if (up == "CASE") {
            ++p;
            return parse_case();
        }

        // A word followed by "(" is a function; a bare word is nothing the language knows, and the
        // message says what the two kinds of quote are for — the commonest mistake there is.
        if (p + 1 < toks.size() && toks[p + 1].kind == Tok::Sym && toks[p + 1].text == "(") {
            std::string lower;
            for (char c : t.text)
                lower += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
            const FnInfo* info = function_named(lower);
            if (info == nullptr) {
                fail("bilinmeyen işlev '" + t.text + "'");
                return nullptr;
            }
            p += 2;
            std::vector<NodePtr> args;
            if (!sym(")")) {
                while (true) {
                    NodePtr arg = parse_or();
                    if (!error.empty()) return nullptr;
                    args.push_back(arg);
                    if (take_sym(",")) continue;
                    break;
                }
            }
            if (!take_sym(")")) {
                fail("'" + t.text + "' işlevinin parantezi kapanmadı");
                return nullptr;
            }
            const int n = static_cast<int>(args.size());
            if (n < info->min_args || (info->max_args >= 0 && n > info->max_args)) {
                fail("'" + t.text + "' " +
                     (info->max_args < 0 ? "en az " + std::to_string(info->min_args)
                      : info->min_args == info->max_args
                          ? std::to_string(info->min_args)
                          : std::to_string(info->min_args) + "–" + std::to_string(info->max_args)) +
                     " değer ister, " + std::to_string(n) + " verildi");
                return nullptr;
            }
            auto node  = std::make_shared<Node>();
            node->op   = Node::Op::Call;
            node->fn   = info->fn;
            node->name = t.text;
            node->kids = std::move(args);
            return node;
        }
        fail("bilinmeyen sözcük '" + t.text +
             "': metin tek tırnak içinde ('Konut'), sütun adı çift tırnak içinde (\"alan_m2\") "
             "yazılır");
        return nullptr;
    }

    NodePtr parse_case()
    {
        auto node = std::make_shared<Node>();
        node->op  = Node::Op::Case;
        if (!word("WHEN")) {
            fail("CASE sonrası WHEN bekleniyordu");
            return nullptr;
        }
        while (take_word("WHEN")) {
            NodePtr cond = parse_or();
            if (!error.empty()) return nullptr;
            if (!take_word("THEN")) {
                fail("WHEN koşulundan sonra THEN bekleniyordu");
                return nullptr;
            }
            NodePtr then = parse_or();
            if (!error.empty()) return nullptr;
            node->kids.push_back(cond);
            node->kids.push_back(then);
        }
        if (take_word("ELSE")) {
            NodePtr other = parse_or();
            if (!error.empty()) return nullptr;
            node->kids.push_back(other);
            node->has_else = true;
        }
        if (!take_word("END")) {
            fail("CASE ifadesi END ile bitmeli");
            return nullptr;
        }
        return node;
    }
};

// -------------------------------------------------------------- evaluation ---

struct Eval
{
    const FieldReader& field;
    std::string error;

    void fail(std::string why)
    {
        if (error.empty()) error = std::move(why);
    }
};

std::size_t utf8_length(std::string_view s)
{
    std::size_t n = 0;
    for (const char c : s)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

/// The byte offset of code point `index` (0-based) in `s`; the size when past the end.
std::size_t utf8_offset(std::string_view s, std::size_t index)
{
    std::size_t seen = 0;
    for (std::size_t i = 0; i < s.size(); ++i)
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (seen == index) return i;
            ++seen;
        }
    return s.size();
}

std::string utf8_slice(std::string_view s, std::size_t first, std::size_t count)
{
    const std::size_t from = utf8_offset(s, first);
    const std::size_t to   = utf8_offset(s, first + count);
    return std::string(s.substr(from, to - from));
}

/// The value as a number, or why it is not one.
std::optional<double> as_number(const ExprValue& v, Eval& ev, const char* what)
{
    switch (v.kind) {
    case ExprValue::Kind::Number: return v.number;
    case ExprValue::Kind::Text: {
        if (const auto n = whole_number(v.text); n) return n;
        ev.fail(std::string(what) + ": '" + v.text + "' bir sayı değil");
        return std::nullopt;
    }
    case ExprValue::Kind::Bool:
        ev.fail(std::string(what) + ": mantıksal değerle sayı işlemi yapılamaz");
        return std::nullopt;
    case ExprValue::Kind::Null: return std::nullopt;
    }
    return std::nullopt;
}

int compare_values(const ExprValue& a, const ExprValue& b)
{
    if (a.kind == ExprValue::Kind::Number && b.kind == ExprValue::Kind::Number)
        return a.number < b.number ? -1 : (a.number > b.number ? 1 : 0);
    // Not both numbers: compare as TEXT, because a cell is text until somebody says otherwise —
    // `1284/A` sorts after `1284` and neither is an error.
    const std::string left  = a.as_text();
    const std::string right = b.as_text();
    const int order         = left.compare(right);
    return order < 0 ? -1 : (order > 0 ? 1 : 0);
}

ExprValue evaluate_node(const Node& n, Eval& ev);

ExprValue evaluate_call(const Node& n, Eval& ev)
{
    using Fn       = Node::Fn;
    const auto arg = [&](std::size_t i) { return evaluate_node(*n.kids[i], ev); };

    // The lazy ones first: only the branch taken is evaluated.
    if (n.fn == Fn::If) {
        const ExprValue c = arg(0);
        if (!ev.error.empty()) return {};
        return c.truthy() ? arg(1) : arg(2);
    }
    if (n.fn == Fn::Coalesce) {
        for (std::size_t i = 0; i < n.kids.size(); ++i) {
            ExprValue v = arg(i);
            if (!ev.error.empty()) return {};
            if (!v.is_null()) return v;
        }
        return {};
    }

    std::vector<ExprValue> a;
    a.reserve(n.kids.size());
    for (std::size_t i = 0; i < n.kids.size(); ++i) {
        a.push_back(arg(i));
        if (!ev.error.empty()) return {};
    }
    const auto num = [&](std::size_t i) { return as_number(a[i], ev, n.name.c_str()); };

    switch (n.fn) {
    case Fn::NullIf:
        return a[0].is_null() || a[1].is_null() || compare_values(a[0], a[1]) != 0 ? a[0]
                                                                                   : ExprValue{};
    case Fn::Round: {
        if (a[0].is_null()) return {};
        const auto x = num(0);
        if (!x) return {};
        double digits = 0;
        if (a.size() == 2) {
            const auto d = num(1);
            if (!d) return {};
            digits = *d;
        }
        if (digits < 0 || digits > 12 || digits != std::floor(digits)) {
            ev.fail("round: basamak sayısı 0–12 arasında bir tam sayı olmalı");
            return {};
        }
        const double scale = std::pow(10.0, digits);
        return ExprValue::of_number(std::round(*x * scale) / scale);
    }
    case Fn::Abs:
    case Fn::Floor:
    case Fn::Ceil:
    case Fn::Sqrt: {
        if (a[0].is_null()) return {};
        const auto x = num(0);
        if (!x) return {};
        if (n.fn == Fn::Abs) return ExprValue::of_number(std::abs(*x));
        if (n.fn == Fn::Floor) return ExprValue::of_number(std::floor(*x));
        if (n.fn == Fn::Ceil) return ExprValue::of_number(std::ceil(*x));
        if (*x < 0) {
            ev.fail("sqrt: negatif sayının karekökü alınamaz");
            return {};
        }
        return ExprValue::of_number(std::sqrt(*x));
    }
    case Fn::Pow: {
        if (a[0].is_null() || a[1].is_null()) return {};
        const auto x = num(0);
        const auto y = num(1);
        if (!x || !y) return {};
        const double r = std::pow(*x, *y);
        if (!std::isfinite(r)) {
            ev.fail("pow: sonuç tanımsız");
            return {};
        }
        return ExprValue::of_number(r);
    }
    case Fn::Min:
    case Fn::Max: {
        std::optional<double> best;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i].is_null()) return {};
            const auto x = num(i);
            if (!x) return {};
            if (!best || (n.fn == Fn::Min ? *x < *best : *x > *best)) best = x;
        }
        return ExprValue::of_number(*best);
    }
    case Fn::Upper:
    case Fn::Lower:
    case Fn::Trim: {
        if (a[0].is_null()) return {};
        const std::string s = a[0].as_text();
        if (n.fn == Fn::Upper) return ExprValue::of_text(core::turkish_upper(s));
        if (n.fn == Fn::Lower) return ExprValue::of_text(core::turkish_lower(s));
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return ExprValue::of_text({});
        const auto last = s.find_last_not_of(" \t\r\n");
        return ExprValue::of_text(s.substr(first, last - first + 1));
    }
    case Fn::Length:
        return a[0].is_null()
                   ? ExprValue{}
                   : ExprValue::of_number(static_cast<double>(utf8_length(a[0].as_text())));
    case Fn::Left:
    case Fn::Right: {
        if (a[0].is_null() || a[1].is_null()) return {};
        const auto k = num(1);
        if (!k) return {};
        const std::string s = a[0].as_text();
        const auto len      = static_cast<std::size_t>(utf8_length(s));
        const auto take =
            static_cast<std::size_t>(std::max(0.0, std::min(*k, static_cast<double>(len))));
        return ExprValue::of_text(utf8_slice(s, n.fn == Fn::Left ? 0 : len - take, take));
    }
    case Fn::Substr: {
        if (a[0].is_null() || a[1].is_null()) return {};
        const auto start = num(1);
        if (!start) return {};
        const std::string s = a[0].as_text();
        const auto len      = static_cast<double>(utf8_length(s));
        // 1-based, as SQL has it; a start before the first letter starts at the first.
        const double from = std::max(1.0, *start);
        double count      = len;
        if (a.size() == 3) {
            if (a[2].is_null()) return {};
            const auto c = num(2);
            if (!c) return {};
            count = std::max(0.0, *c);
        }
        if (from > len) return ExprValue::of_text({});
        return ExprValue::of_text(
            utf8_slice(s, static_cast<std::size_t>(from - 1),
                       static_cast<std::size_t>(std::min(count, len - (from - 1)))));
    }
    case Fn::Replace: {
        if (a[0].is_null() || a[1].is_null() || a[2].is_null()) return {};
        std::string s         = a[0].as_text();
        const std::string old = a[1].as_text();
        const std::string neu = a[2].as_text();
        if (old.empty()) return ExprValue::of_text(std::move(s));
        std::string out;
        std::size_t at = 0;
        while (true) {
            const std::size_t hit = s.find(old, at);
            if (hit == std::string::npos) {
                out.append(s, at, std::string::npos);
                break;
            }
            out.append(s, at, hit - at);
            out += neu;
            at = hit + old.size();
        }
        return ExprValue::of_text(std::move(out));
    }
    case Fn::Lpad:
    case Fn::Rpad: {
        if (a[0].is_null() || a[1].is_null() || a[2].is_null()) return {};
        const auto width = num(1);
        if (!width) return {};
        const std::string fill = a[2].as_text();
        std::string s          = a[0].as_text();
        if (fill.empty() || utf8_length(fill) != 1) {
            ev.fail(n.name + ": dolgu tek bir karakter olmalı");
            return {};
        }
        if (*width < 0 || *width > 4096) {
            ev.fail(n.name + ": genişlik 0–4096 arasında olmalı");
            return {};
        }
        const auto have = utf8_length(s);
        const auto want = static_cast<std::size_t>(*width);
        if (have >= want) return ExprValue::of_text(utf8_slice(s, 0, want));
        std::string pad;
        for (std::size_t i = have; i < want; ++i)
            pad += fill;
        return ExprValue::of_text(n.fn == Fn::Lpad ? pad + s : s + pad);
    }
    case Fn::ToText: return a[0].is_null() ? ExprValue{} : ExprValue::of_text(a[0].as_text());
    case Fn::ToNumber: {
        if (a[0].is_null()) return {};
        if (a[0].kind == ExprValue::Kind::Number) return a[0];
        const auto v = whole_number(a[0].as_text());
        return v ? ExprValue::of_number(*v)
                 : ExprValue{}; // not a number is NULL, and `coalesce` answers
    }
    case Fn::Contains:
    case Fn::StartsWith:
    case Fn::EndsWith: {
        if (a[0].is_null() || a[1].is_null()) return ExprValue::of_bool(false);
        const std::string s   = a[0].as_text();
        const std::string sub = a[1].as_text();
        if (n.fn == Fn::Contains) return ExprValue::of_bool(s.find(sub) != std::string::npos);
        if (n.fn == Fn::StartsWith)
            return ExprValue::of_bool(s.compare(0, sub.size(), sub) == 0 && s.size() >= sub.size());
        return ExprValue::of_bool(s.size() >= sub.size() &&
                                  s.compare(s.size() - sub.size(), sub.size(), sub) == 0);
    }
    case Fn::If:
    case Fn::Coalesce: break;
    }
    return {};
}

ExprValue evaluate_node(const Node& n, Eval& ev)
{
    using Op = Node::Op;
    switch (n.op) {
    case Op::Literal: return n.literal;
    case Op::Column: return ExprValue::of_cell(ev.field ? ev.field(n.name) : std::nullopt);
    case Op::Neg: {
        const ExprValue v = evaluate_node(*n.kids[0], ev);
        if (!ev.error.empty() || v.is_null()) return {};
        const auto x = as_number(v, ev, "eksi işareti");
        return x ? ExprValue::of_number(-*x) : ExprValue{};
    }
    case Op::Not: {
        const ExprValue v = evaluate_node(*n.kids[0], ev);
        return ev.error.empty() ? ExprValue::of_bool(!v.truthy()) : ExprValue{};
    }
    case Op::And: {
        const ExprValue l = evaluate_node(*n.kids[0], ev);
        if (!ev.error.empty()) return {};
        if (!l.truthy()) return ExprValue::of_bool(false);
        const ExprValue r = evaluate_node(*n.kids[1], ev);
        return ev.error.empty() ? ExprValue::of_bool(r.truthy()) : ExprValue{};
    }
    case Op::Or: {
        const ExprValue l = evaluate_node(*n.kids[0], ev);
        if (!ev.error.empty()) return {};
        if (l.truthy()) return ExprValue::of_bool(true);
        const ExprValue r = evaluate_node(*n.kids[1], ev);
        return ev.error.empty() ? ExprValue::of_bool(r.truthy()) : ExprValue{};
    }
    case Op::IsNull:
    case Op::IsNotNull: {
        const ExprValue v = evaluate_node(*n.kids[0], ev);
        if (!ev.error.empty()) return {};
        return ExprValue::of_bool(n.op == Op::IsNull ? v.is_null() : !v.is_null());
    }
    case Op::Call: return evaluate_call(n, ev);
    case Op::Case: {
        const std::size_t pairs = (n.kids.size() - (n.has_else ? 1 : 0)) / 2;
        for (std::size_t i = 0; i < pairs; ++i) {
            const ExprValue c = evaluate_node(*n.kids[2 * i], ev);
            if (!ev.error.empty()) return {};
            if (c.truthy()) return evaluate_node(*n.kids[2 * i + 1], ev);
        }
        return n.has_else ? evaluate_node(*n.kids.back(), ev) : ExprValue{};
    }
    default: break;
    }

    // Everything below has two operands.
    const ExprValue l = evaluate_node(*n.kids[0], ev);
    if (!ev.error.empty()) return {};
    const ExprValue r = evaluate_node(*n.kids[1], ev);
    if (!ev.error.empty()) return {};

    switch (n.op) {
    case Op::Eq:
    case Op::Ne:
    case Op::Lt:
    case Op::Le:
    case Op::Gt:
    case Op::Ge: {
        // Any comparison against an unfilled cell is false, INCLUDING `!=`. A measurement nobody
        // took is not "different from 5"; it is unknown.
        if (l.is_null() || r.is_null()) return ExprValue::of_bool(false);
        const int order = compare_values(l, r);
        switch (n.op) {
        case Op::Eq: return ExprValue::of_bool(order == 0);
        case Op::Ne: return ExprValue::of_bool(order != 0);
        case Op::Lt: return ExprValue::of_bool(order < 0);
        case Op::Le: return ExprValue::of_bool(order <= 0);
        case Op::Gt: return ExprValue::of_bool(order > 0);
        default: return ExprValue::of_bool(order >= 0);
        }
    }
    case Op::Concat:
        if (l.is_null() || r.is_null()) return {};
        return ExprValue::of_text(l.as_text() + r.as_text());
    default: break;
    }

    if (l.is_null() || r.is_null()) return {};
    const auto x = as_number(l, ev, "işlem");
    const auto y = as_number(r, ev, "işlem");
    if (!x || !y) return {};
    double out = 0.0;
    switch (n.op) {
    case Op::Add: out = *x + *y; break;
    case Op::Sub: out = *x - *y; break;
    case Op::Mul: out = *x * *y; break;
    case Op::Div:
        if (*y == 0.0) {
            ev.fail("sıfıra bölme");
            return {};
        }
        out = *x / *y;
        break;
    case Op::Mod:
        if (*y == 0.0) {
            ev.fail("sıfıra göre mod");
            return {};
        }
        out = std::fmod(*x, *y);
        break;
    case Op::Pow: out = std::pow(*x, *y); break;
    default: break;
    }
    if (!std::isfinite(out)) {
        ev.fail("sonuç tanımsız ya da sonsuz");
        return {};
    }
    return ExprValue::of_number(out);
}

} // namespace

core::Result<Expression> Expression::compile(std::string_view source)
{
    Lexer lexer(source);
    Parser parser;
    parser.toks = lexer.run();
    if (!lexer.error.empty())
        return err(ErrorCode::ParseError, "'" + std::string(source) + "': " + lexer.error);
    NodePtr root = parser.parse_all();
    if (!parser.error.empty() || root == nullptr)
        return err(ErrorCode::ParseError, "'" + std::string(source) + "': " + parser.error);

    Expression out;
    out.root_    = std::move(root);
    out.source_  = std::string(source);
    out.columns_ = std::move(parser.columns);
    return out;
}

core::Result<ExprValue> Expression::evaluate(const FieldReader& field) const
{
    Eval ev{field, {}};
    ExprValue v = evaluate_node(*root_, ev);
    if (!ev.error.empty()) return err(ErrorCode::ValidationFailed, ev.error);
    return v;
}

std::vector<FunctionHelp> expression_functions()
{
    std::vector<FunctionHelp> out;
    for (const Parser::FnInfo& f : Parser::function_table())
        out.push_back({f.name, f.usage, f.help});
    return out;
}

} // namespace piricad::command
