// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the value expression language of a row (TODOS G-03).
//
// A row of an attribute table is asked three kinds of question: "is it one of these" (a filter),
// "what is the value of this field for it" (the field calculator, a label, a table column) and
// "which text does it print". They are ONE language, because a person who has learnt `"alan_m2" >
// 2000` for the filter bar expects `"alan_m2" * 0.4` and `"ada_no" || '/' || "parsel_no"` to read
// the same way — and because CLAUDE.md 5.11 allows exactly one grammar in this product.
// `evaluate_predicate` (parser.hpp) is this language asked for a yes or a no.
//
//     "alan_m2" > 2000 AND "plan_fonksiyon" = 'Konut'            a filter
//     round("alan_m2" * 0.4, 2)                                     a number
//     "ada_no" || '/' || lpad("parsel_no", 3, '0')                 a text
//     CASE WHEN "alan_m2" > 3000 THEN 'Büyük' ELSE 'Küçük' END    a choice
//
// WRITTEN ONCE, RUN MANY TIMES. `compile` reads the text once into a tree; `evaluate` walks the
// tree for one row. A million-row layer filtered or calculated used to mean a million parses of the
// same text.
//
// NULL IS NOT ZERO AND NOT EMPTY (the model's R-rule, and the one a surveyor trips on): a cell
// nobody filled is NULL; `0` and `''` are values. Arithmetic and `||` with a NULL are NULL, a
// comparison with a NULL is false, `IS NULL` asks, and `coalesce` answers.
//
// No function here depends on the clock, the machine or the locale: the same expression over the
// same row gives the same value everywhere, which is what lets a calculation be journalled and
// replayed (Article 1.4, 6.4).
#pragma once

#include "piricad/command/parser.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::command {

/// A value the language computes.
struct ExprValue
{
    /// The four kinds of value the language has.
    enum class Kind : std::uint8_t { Null, Number, Text, Bool };

    Kind kind{Kind::Null}; ///< which of the four the value is
    double number{0.0};    ///< Number
    std::string text;      ///< Text; and, for a Number read from a cell, the cell's own text
    bool boolean{false};   ///< Bool
    bool from_cell{
        false}; ///< a Number that was a cell: `text` is what the cell said, digit for digit

    /// The NULL value: a cell nobody filled.
    static ExprValue null() { return {}; }

    /// A Number.
    static ExprValue of_number(double v);

    /// A Text.
    static ExprValue of_text(std::string v);

    /// A Bool.
    static ExprValue of_bool(bool v);

    /// A cell's text as a value: nothing is NULL, text that is wholly a number is a Number (which
    /// remembers the text, so `0012` concatenates as `0012`), anything else is Text.
    static ExprValue of_cell(const std::optional<std::string>& cell);

    bool is_null() const noexcept { return kind == Kind::Null; }

    /// The value as the text a cell or a `||` would carry; empty for NULL (callers ask `is_null`
    /// first).
    std::string as_text() const;

    /// Whether the value counts as "yes": a Bool as it is, a Number when not zero, Text when not
    /// empty.
    bool truthy() const noexcept;
};

/// One expression, compiled.
class Expression
{
public:
    /// Reads `source` once. Fails with a message that names the place in the text.
    static core::Result<Expression> compile(std::string_view source);

    /// The value for one row. A column the row does not have is NULL; an operation that cannot be
    /// done (a text that is no number, a division by zero) fails with the reason.
    core::Result<ExprValue> evaluate(const FieldReader& field) const;

    /// The text it was compiled from.
    const std::string& source() const noexcept { return source_; }

    /// Every column the expression names (`"alan_m2"`, `$alan`), in order of first appearance.
    const std::vector<std::string>& columns() const noexcept { return columns_; }

    struct Node;

private:
    std::shared_ptr<const Node> root_;
    std::string source_;
    std::vector<std::string> columns_;
};

/// One function of the language, for a list a person reads (the calculator window, the manual).
struct FunctionHelp
{
    std::string name;  ///< as typed: `round`
    std::string usage; ///< the call with its parameters: `round(x, basamak)`
    std::string help;  ///< one Turkish sentence
};

/// Every function the language has, in the order the parser's own table holds them. The same table
/// the parser reads names and arities from, so a list built from this can never name a function
/// that does not exist or miss one that does.
std::vector<FunctionHelp> expression_functions();

/// The `$` words a row answers (`$fid`, `$alan`, …), with a Turkish sentence each: values of the
/// row itself that no schema declares. `usage` is the word as typed.
std::vector<FunctionHelp> expression_pseudo_columns();

} // namespace piricad::command
