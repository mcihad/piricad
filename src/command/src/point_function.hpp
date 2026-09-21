// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command, private: the point functions of THE grammar.
//
// `orta(A,B)`, `dik(A,B,30,-5)`, `kes(A,r1,B,r2,yon=sol)`, `n(1284)`, `son` —
// the constructions a surveyor does on paper before typing a coordinate, written
// where a coordinate is written (TODOS-CAD P1a). They are part of
// `kentos_cad/command/parser.hpp` and nothing else: CLAUDE.md 5.11 allows one
// grammar, and this file is a chapter of it, not a second one.
//
// WHY THIS IS A SEPARATE TRANSLATION UNIT AND A PRIVATE HEADER. The table of
// functions, the matcher that reads an argument list against a signature, and the
// geometry each construction runs are together longer than the rest of the
// grammar put together, and none of it is anybody's business outside
// `parser.cpp`. The two files call into each other — `classify` builds a call,
// a call's arguments are classified — so the seam is declared here rather than
// guessed at, and it stays out of the public header where a client could reach
// for it.
//
// THE ARGUMENT SEPARATOR IS ALSO THE COORDINATE SEPARATOR. `orta(0,0,10,10)` is
// two points, not four numbers, and `dik(0,0,100,0,30,-5)` is two points and two
// distances. There is no way to count arguments without knowing what the function
// expects, so the list is matched AGAINST THE SIGNATURE: a point argument spends
// two comma-separated fields when it is written as bare coordinates and one when
// it is written `@100<50`, `son` or as a nested call. Where one name has several
// shapes — `kes` has three — every shape is tried and exactly one must fit, so a
// reading is never chosen in silence.
#pragma once

#include "kentos_cad/command/parser.hpp"

#include <string>
#include <string_view>

namespace kentos::command::detail {

/// How deep one point function may be written inside another.
///
/// A limit rather than a taste: `orta(orta(orta(…)))` recurses through
/// `classify` once per level, and a fuzzer reaches a stack overflow long before
/// a surveyor reaches four. Sixteen is past anything a person writes and far
/// short of the stack.
inline constexpr int kMaxCallDepth = 16;

/// Classifies one already-unquoted token — the classifier inside `parser.cpp`.
///
/// Declared here so the argument matcher reads a field with THE grammar rather
/// than a private copy of it: a point function's argument is an ordinary
/// coordinate, number or expression and is read by exactly the code that reads
/// one anywhere else. `depth` is the call nesting the field was found at.
core::Result<Token> classify(std::string_view raw, int depth);

/// True when `text` is `name(…)` for a name this grammar knows.
///
/// A name it does NOT know stays an ordinary word, because a layer, a file or a
/// caption may legitimately contain parentheses and turning those into parse
/// errors would break text that works today. What the user gets instead is the
/// coordinate error with their text quoted in it.
bool is_call_text(std::string_view text);

/// Builds the `Kind::Call` token for `text`, which `is_call_text` has accepted.
///
/// This is where a form is chosen — once, at parse time — and where an argument
/// list that fits no form is refused by name, with the shapes the function does
/// take (command.md R19).
core::Result<Token> parse_call(std::string_view text, int depth);

/// Runs the construction a `Kind::Call` token holds and returns its one point.
///
/// `last` is the point before, which `son` answers and which a `@` argument is
/// measured from; `ctx` carries the angle convention and the numbered-point
/// lookup. Every failure — parallel directions, circles that do not meet, a
/// point number that is not in the drawing — comes back as an `Error` naming
/// what was expected and what arrived.
core::Result<core::Point2> resolve_call(const Token& t, core::Point2 last,
                                        const ResolveContext& ctx);

/// A `Kind::Call` token written back out the way it was typed, for an error
/// message or the transcript: `orta(point(0.0,0.0),@(50.0,30.0))`.
std::string describe_call(const Token& t);

} // namespace kentos::command::detail
