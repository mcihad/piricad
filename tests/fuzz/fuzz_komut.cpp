// SPDX-License-Identifier: GPL-3.0-or-later
//
// libFuzzer harness for THE grammar — piricad/command/parser.hpp.
//
// test.md R9 names `command/parser.hpp` among the parsers that must have a target
// here, and CLAUDE.md 6.7 ships the harness with the change that touched the
// grammar: the polar coordinate gained a unit suffix and a convention parameter
// (TODOS-CAD P0), so the line lexer, the coordinate classifier and the resolver
// all changed shape at once.
//
// WHAT COUNTS AS A CRASH. Almost no random byte sequence is a command line, and
// nothing here asserts that one is. The property is narrower and stronger: for
// ANY input, every entry point of the grammar either returns a `Result` error or a
// well-formed value — it never reads past the end of the text, never recurses
// without bound on nested parentheses, never divides by a zero it did not check
// and never turns an out-of-range angle into undefined behaviour in the
// micro-degree conversion. Under ASan and UBSan any violation is a crash.
//
// FOUR ENTRY POINTS, because the grammar has four: a line (`parse_line`, then
// every coordinate token resolved under all six angle conventions and chained the
// way `bind_tokens` chains them), a bare expression (`evaluate_expression`), a
// filter predicate (`evaluate_predicate`) and a single coordinate written as text
// (`parse_point`, the script path).
//
// POINT FUNCTIONS go through the same four (TODOS-CAD P1a): `orta(…)`,
// `kes(…)` and the rest are read by `classify` and resolved by `resolve_point`,
// so every byte sequence tried here is also tried as an argument list — nested
// calls, a signature that fits nothing, two shapes of `kes`, an angle with a
// suffix inside a call. `n()` is given a lookup that answers for a handful of
// numbers, so both the found and the missing road are walked; the harness also
// runs each input once with NO lookup, which is the headless case where `n()`
// must refuse rather than reach for a document that is not there.
//
// Build:
//   cmake --preset dev -DPIRICAD_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
//   ./build/dev/bin/piricad_fuzz_komut build/dev/fuzz-corpus/komut tests/fuzz/tohum/komut \
//       -max_total_time=300
#include "piricad/command/parser.hpp"
#include "piricad/core/angle.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    // A cap, not a correctness rule: a command line a person types is a few
    // hundred bytes, and the interesting bugs are in how tokens end, nest and
    // quote — all of which fit in far less than this.
    if (size > (1u << 16)) return 0;

    const std::string_view text(reinterpret_cast<const char*>(data), size);

    using piricad::core::AngleConvention;
    using piricad::core::AngleRule;
    using piricad::core::AngleUnit;
    using piricad::core::Point2;

    // A stand-in drawing for `n(1284)`: three numbered points and nothing else,
    // so a hit and a miss are both one call away. The real one walks the
    // document (`Bus::numbered_point`); what the grammar sees is this shape.
    const auto context = [](AngleConvention convention) {
        piricad::command::ResolveContext ctx;
        ctx.convention  = convention;
        ctx.named_point = [](std::int64_t number) -> std::optional<Point2> {
            switch (number) {
            case 1: return Point2{0, 0};
            case 1284: return Point2{485320150, 4310220400};
            case -7: return Point2{-1, -1};
            default: return std::nullopt;
            }
        };
        // AND FOR `nesne(k)`, THREE PATHS: a 20 m arc, a straight 100 m segment
        // and a closed 10 m square — a curve, a line and a ring to walk along —
        // and a refusal for every other key.
        ctx.object_path = [](std::int64_t key) -> piricad::core::Result<piricad::core::CurvePath> {
            piricad::core::CurvePath path;
            if (key == 1) {
                path.pieces.push_back(piricad::core::arc_piece(
                    Point2{0, 0}, 20000, Point2{20000, 0}, Point2{-20000, 0}, true));
            } else if (key == 2) {
                piricad::core::PathPiece line;
                line.from = Point2{0, 0};
                line.to   = Point2{100000, 0};
                path.pieces.push_back(line);
            } else if (key == 3) {
                const Point2 corners[] = {{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
                for (std::size_t i = 0; i < 4; ++i) {
                    piricad::core::PathPiece side;
                    side.from = corners[i];
                    side.to   = corners[(i + 1) % 4];
                    path.pieces.push_back(side);
                }
                path.closed = true;
            } else {
                return piricad::core::err(piricad::core::ErrorCode::NotFound, "yok");
            }
            return path;
        };
        return ctx;
    };

    // ---- 1. a line, and every coordinate on it under every convention ----
    if (auto parsed = piricad::command::parse_line(text)) {
        Point2 last{};
        for (const piricad::command::Token& token : parsed.value().tokens) {
            (void)piricad::command::describe(token);
            if (!piricad::command::is_coordinate(token)) continue;
            for (int unit = 0; unit < 3; ++unit)
                for (int rule = 0; rule < 2; ++rule) {
                    const AngleConvention convention{static_cast<AngleUnit>(unit),
                                                     static_cast<AngleRule>(rule)};
                    if (auto p = piricad::command::resolve_point(token, last, context(convention)))
                        last = p.value();
                }
        }
    }

    // ---- 2. the expression grammar on the whole input ----
    (void)piricad::command::evaluate_expression(text);

    // ---- 3. the predicate grammar, against a row that has one NULL cell ----
    const piricad::command::FieldReader row =
        [](std::string_view column) -> std::optional<std::string> {
        if (column == "beyan") return std::nullopt;
        return std::string("1284");
    };
    (void)piricad::command::evaluate_predicate(text, row);

    // ---- 4. one coordinate as text, the script path ----
    (void)piricad::command::parse_point(text, Point2{}, context(AngleConvention{}));

    // ---- 5. the same text with NO document behind it ----
    //
    // The headless case: `ResolveContext::named_point` is empty, `n()` must say
    // so and every other function must be unaffected.
    (void)piricad::command::parse_point(text, Point2{}, piricad::command::ResolveContext{});

    return 0;
}
