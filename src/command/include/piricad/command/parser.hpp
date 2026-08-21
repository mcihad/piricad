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
//   keyword argument              katman=SINIR   mesafe=-3.0
//   text                          "yol kenarı"
#pragma once

#include "piricad/command/value.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace piricad::command {

struct Token
{
    enum class Kind : std::uint8_t {
        Word,     ///< bare identifier (command name, keyword, layer name)
        Number,   ///< 42  /  -3.5  /  (100*3)
        Text,     ///< "quoted string"
        Absolute, ///< x,y in metres
        Relative, ///< @dx,dy in metres, from the last point
        Polar,    ///< @distance<angle, metres and degrees
        KeyValue, ///< key=<nested token>
    };

    Kind kind{Kind::Word};
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

/// Converts a coordinate token to an absolute point, resolving @ forms against
/// `last`. Returns an error for a non-coordinate token.
core::Result<core::Point2> resolve_point(const Token& t, core::Point2 last);

/// True when the token can become a point.
bool is_coordinate(const Token& t);

/// Human-readable token rendering, for error messages and the transcript.
std::string describe(const Token& t);

} // namespace piricad::command
