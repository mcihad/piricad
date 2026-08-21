// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — a 60-line test harness.
//
// doctest is the project's chosen framework (piricad.md §9.11) for its compile
// time; it is not installed on this machine, and Phase 0 takes no dependency
// beyond Qt (CLAUDE.md Article 8). Swapping this for doctest is a mechanical
// rename of the three macros below.
#pragma once

#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace microtest {

struct Case
{
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& cases()
{
    static std::vector<Case> c;
    return c;
}

inline int& failures()
{
    static int f = 0;
    return f;
}

inline std::string& current()
{
    static std::string c;
    return c;
}

struct Registrar
{
    Registrar(std::string name, std::function<void()> fn)
    {
        cases().push_back(Case{std::move(name), std::move(fn)});
    }
};

inline void report(const char* file, int line, const char* expr, const std::string& detail)
{
    ++failures();
    std::fprintf(stderr, "  FAIL  %s\n        %s:%d\n        %s\n", current().c_str(), file, line,
                 expr);
    if (!detail.empty()) std::fprintf(stderr, "        %s\n", detail.c_str());
}

inline int run(const char* suite)
{
    std::fprintf(stdout, "== %s : %zu case(s) ==\n", suite, cases().size());
    int failed_cases = 0;

    for (auto& c : cases()) {
        current()        = c.name;
        const int before = failures();

        // A case that throws is a FAILED case, not a dead run. Result::value() is
        // std::get<0> on a variant, so it throws bad_variant_access on an error
        // path — and without this the first real regression in such a path kills
        // the whole suite with no per-case report, i.e. the report disappears
        // exactly when the implementation is most broken.
        try {
            c.fn();
        } catch (const std::exception& e) {
            report("(case)", 0, "case bir istisna ile sonlandı", e.what());
        } catch (...) {
            report("(case)", 0, "case bilinmeyen bir istisna ile sonlandı", {});
        }

        const bool ok = failures() == before;
        if (!ok) ++failed_cases;
        std::fprintf(stdout, "  %s  %s\n", ok ? "ok  " : "FAIL", c.name.c_str());
    }

    std::fprintf(stdout, "== %d/%zu passed, %d assertion failure(s) ==\n",
                 static_cast<int>(cases().size()) - failed_cases, cases().size(), failures());
    return failures() == 0 ? 0 : 1;
}

} // namespace microtest

#define PIRICAD_CONCAT_(a, b) a##b
#define PIRICAD_CONCAT(a, b) PIRICAD_CONCAT_(a, b)

#define TEST_CASE(name)                                                                            \
    static void PIRICAD_CONCAT(pt_case_, __LINE__)();                                              \
    static ::microtest::Registrar PIRICAD_CONCAT(pt_reg_, __LINE__)(                               \
        name, &PIRICAD_CONCAT(pt_case_, __LINE__));                                                \
    static void PIRICAD_CONCAT(pt_case_, __LINE__)()

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) ::microtest::report(__FILE__, __LINE__, #expr, {});                           \
    } while (false)

/// A precondition whose failure would be dereferenced afterwards. Reports and
/// RETURNS from the case, because a case is a void function and a soft CHECK
/// followed by `*t.at(slot)` or `.value()` is a crash waiting for the first real
/// regression.
#define REQUIRE(expr)                                                                              \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            ::microtest::report(__FILE__, __LINE__, "REQUIRE " #expr, {});                         \
            return;                                                                                \
        }                                                                                          \
    } while (false)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        const auto pt_a = (a);                                                                     \
        const auto pt_b = (b);                                                                     \
        if (!(pt_a == pt_b))                                                                       \
            ::microtest::report(__FILE__, __LINE__, #a " == " #b,                                  \
                                ::microtest::describe(pt_a, pt_b));                                \
    } while (false)

namespace microtest {

template<class A, class B> std::string describe(const A& a, const B& b)
{
    std::string out = "left  = ";
    if constexpr (requires { std::to_string(a); })
        out += std::to_string(a);
    else if constexpr (requires { std::string(a); })
        out += std::string(a);
    else
        out += "(unprintable)";
    out += "\n        right = ";
    if constexpr (requires { std::to_string(b); })
        out += std::to_string(b);
    else if constexpr (requires { std::string(b); })
        out += std::string(b);
    else
        out += "(unprintable)";
    return out;
}

} // namespace microtest
