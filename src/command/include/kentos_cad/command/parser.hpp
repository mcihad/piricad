// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: THE grammar.
//
// kentoscad.md §3, implementation note: the command-line parser and the script
// engine's parser must be the SAME grammar. Two parsers guarantee behavioural
// drift. There is exactly one of these in the project.
//
// Grammar (§3):
//   command                       ÇİZGİ | LINE | Ç | L
//   absolute point                485320.150,4310220.400          (metres)
//   relative point                @50,30
//   polar point                   @100<45   @100<45g  @100<30d  @100<0.7r
//                                 metres, then the angle: bare, it is read in the
//                                 session's unit and rule — `core.aci.birim` and
//                                 `core.aci.kural`, SEMT + GRAD by default, so
//                                 `@100<45` is 45 grad clockwise from north — or
//                                 in the unit its g/d/r suffix names. The rule is
//                                 never written on the coordinate (TODOS-CAD P0).
//   inline expression             @(100*3),0
//   keyword argument              `katman`=SINIR   mesafe=-3.0
//   text                          "yol kenarı"
#pragma once

#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::command {

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
        Polar,    ///< @distance<angle[g|d|r], metres and an angle (see `angle_unit`)
        KeyValue, ///< key=<nested token>
    };

    Kind kind{Kind::Word};     ///< which of the fields below carry meaning
    std::string word;          ///< Word text, or the key of a KeyValue
    double a{0.0};             ///< Number value, x, dx, or distance
    double b{0.0};             ///< y, dy, or the angle exactly as written
    std::string text;          ///< Text payload
    std::vector<Token> nested; ///< KeyValue payload (exactly one element)

    /// The unit a Polar angle's suffix named — `@100<45g` grad, `d` degree, `r`
    /// radian — or empty when the angle was written bare and the session's
    /// `core.aci.birim` decides. Carried on the token rather than applied here,
    /// because the tokeniser has no convention in hand: `resolve_point` does.
    std::optional<core::AngleUnit> angle_unit;
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
///
/// A polar token is resolved under `convention` — the unit a bare angle is in and
/// which way it grows — with the token's own suffix overriding the unit. The
/// convention is a PARAMETER: the bus reads it from the session settings once per
/// line and hands it down, so the parser holds no global and the script engine,
/// the command line and a typed answer cannot resolve the same text two ways
/// (CLAUDE.md 5.11, TODOS-CAD P0-2).
core::Result<core::Point2> resolve_point(const Token& t, core::Point2 last,
                                         core::AngleConvention convention);

/// Reads ONE coordinate written as text — `485320,4310220`, `@50,30`, `@100<45g`,
/// exactly what the command line accepts — and resolves it against `last`.
///
/// The same grammar, entered from a string rather than a line: it is how a JSON
/// script or any other client that carries a coordinate as text gets it read by
/// `classify` and `resolve_point` and nothing else (CLAUDE.md 5.11). Surrounding
/// blanks are ignored; anything that is not a coordinate is refused with the
/// message a typed line would get.
core::Result<core::Point2> parse_point(std::string_view text, core::Point2 last,
                                       core::AngleConvention convention);

/// True when the token can become a point.
bool is_coordinate(const Token& t);

/// Human-readable token rendering, for error messages and the transcript.
std::string describe(const Token& t);

} // namespace kentos::command
