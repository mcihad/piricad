// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: THE grammar.
//
// piricad.md §3, implementation note: the command-line parser and the script
// engine's parser must be the SAME grammar. Two parsers guarantee behavioural
// drift. There is exactly one of these in the project.
//
// Grammar (§3):
//   command                       ÇİZGİ | LINE | Ç | L
//   absolute point                485320.150,4310220.400          (metres)
//   relative point                @50,30
//   polar point                   @100<45                          (metres, degrees CCW)
//   inline expression             @(100*3),0
//   keyword argument              `katman`=SINIR   mesafe=-3.0
//   text                          "yol kenarı"
#pragma once

#include "piricad/command/value.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::command {

/// One lexical unit of a command line.
///
/// There is exactly ONE parser in this product (CLAUDE.md 5.11), and this is its
/// output: the command line and the script engine both go through it, so a
/// coordinate typed by a surveyor and a coordinate in a JSON file are read by the
/// same code and cannot disagree.
struct Token
{
    /// What the token turned out to be. Coordinates are their own kinds rather
    /// than a pair of numbers, because `@50,30` means something the numbers alone
    /// do not: it is relative to the previous point.
    enum class Kind : std::uint8_t {
        Word,     ///< bare identifier (command name, keyword, layer name)
        Number,   ///< 42  /  -3.5  /  (100*3)
        Text,     ///< "quoted string"
        Absolute, ///< x,y in metres
        Relative, ///< @dx,dy in metres, from the last point
        Polar,    ///< @distance<angle, metres and degrees
        KeyValue, ///< key=<nested token>
    };

    Kind kind{Kind::Word};     ///< which of the fields below carry meaning
    std::string word;          ///< Word text, or the key of a KeyValue
    double a{0.0};             ///< Number value, x, dx, or distance
    double b{0.0};             ///< y, dy, or angle in degrees
    std::string text;          ///< Text payload
    std::vector<Token> nested; ///< KeyValue payload (exactly one element)
};

struct ParsedLine
{
    std::string command;       ///< first word, as typed
    std::vector<Token> tokens; ///< everything after the command
};

/// Parses one command line. The same function backs the CLI widget, the script
/// engine and macro playback.
core::Result<ParsedLine> parse_line(std::string_view line);

/// Evaluates an arithmetic expression: + - * / % ^, parentheses, unary minus.
/// Locale-independent. Backs the CLI's `@(100*3),0` form (§3).
core::Result<double> evaluate_expression(std::string_view expr);

/// One row of whatever is being filtered, as the predicate sees it.
///
/// A function rather than a table, because the caller knows where a column lives
/// and the parser must not: the attribute table reads an `AttrColumn`, a future
/// PostGIS filter would read a result set, and neither belongs in the grammar.
/// The returned string is the cell's TEXT; a cell with no value returns
/// `std::nullopt`, which is how `NULL` stays different from an empty string.
using FieldReader = std::function<std::optional<std::string>(std::string_view column)>;

/// Evaluates a filter predicate against one row.
///
/// THE SAME GRAMMAR, EXTENDED — not a second one. CLAUDE.md 5.11 allows exactly
/// one parser in this product, and an attribute filter is an expression like any
/// other, so it lives here beside `evaluate_expression` and every client gets it:
/// the table's filter bar, `SEÇ ifade=…` from the command line, a script, and the
/// AI (Article 1.2).
///
///     "alan_m2" > 2000 AND "plan_fonksiyon" = 'Konut'
///     "beyan" IS NULL OR NOT "nitelik" = 'Tarla'
///
/// Grammar, in precedence order:
///   or        := and { OR and }
///   and       := not { AND not }
///   not       := [ NOT ] compare
///   compare   := sum [ ( = | != | <> | < | <= | > | >= | IS NULL | IS NOT NULL ) sum ]
///   sum       := the arithmetic `evaluate_expression` already reads, plus
///                "column" references and 'string' literals
///
/// A column name is written in DOUBLE quotes and a string in SINGLE quotes,
/// which is SQL's convention and the one every GIS user already has. Comparing a
/// number to a string compares them as text, because a cell is text until
/// somebody says otherwise and refusing would make a filter fail on a column
/// that happens to hold `1284` in one row and `1284/A` in the next.
core::Result<bool> evaluate_predicate(std::string_view expr, const FieldReader& field);

/// Converts a coordinate token to an absolute point, resolving @ forms against
/// `last`. Returns an error for a non-coordinate token.
core::Result<core::Point2> resolve_point(const Token& t, core::Point2 last);

/// True when the token can become a point.
bool is_coordinate(const Token& t);

/// Human-readable token rendering, for error messages and the transcript.
std::string describe(const Token& t);

} // namespace piricad::command
