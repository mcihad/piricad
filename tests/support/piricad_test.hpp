// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the unit-test harness: doctest, plus the one thing doctest has no
// concept of.
//
// piricad.md §9.11 named doctest as the framework and Phase 0 shipped a 190-line
// stand-in instead, on the grounds that the dependency was not available. It is
// available; CLAUDE.md Article 2.7 and 5.16 say the mature library is used, so the
// stand-in is gone and every assertion macro below comes from doctest.
//
// WHAT THIS HEADER STILL ADDS, and why it is not upstream's job:
//
//   PENDING(reason) — a case that CANNOT RUN in this build, because an optional
//   dependency behind a PIRICAD_WITH_* option is off. `.claude/data.md`
//   (Enforcement) and CLAUDE.md Article 8.2 require such a case to "report as
//   PENDING, never as passing": printing `ok` beside a case that asserted nothing
//   is exactly the false report those rules forbid.
//
//   doctest's own skip mechanism cannot express this. `doctest::skip()` is a
//   COMPILE-TIME decorator, and whether GDAL is present is a run-time fact this
//   binary discovers by calling `io::vector_backend_available()`. Its skipped
//   cases are also silent by default, whereas the whole point here is to say out
//   loud which coverage this run did not have.
//
//   Pending is NOT a failure and does not change the exit code — an absent
//   optional dependency is not a broken build (test.md R8b). It is a line in the
//   report that a reader cannot miss.
#pragma once

// doctest must be reached through THIS header, never included directly, because
// the option below has to be set before its first inclusion in every translation
// unit; a TU that got the default would silently disagree with the rest.
#ifdef DOCTEST_VERSION
#error "doctest was included before piricad_test.hpp; include this header instead"
#endif

// Without this, doctest stringifies `const char*` through its generic pointer
// StringMaker and a failure message reads `0x5d309079d730` instead of the text
// the test was reporting. It is not hypothetical: it is what the first run after
// this migration printed. Every `const char*` in this suite is a string.
#define DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

namespace piricad_test {

/// One case that could not run, and the reason it could not.
struct PendingCase
{
    std::string test; ///< the doctest case name, captured by the listener below
    std::string why;  ///< shown verbatim in the report; written for a human
};

/// Every case that reported PENDING during this run, in the order they ran.
inline std::vector<PendingCase>& pending_cases()
{
    static std::vector<PendingCase> v;
    return v;
}

/// The name of the case doctest is currently running.
///
/// Filled by the listener registered in `unit/main.cpp`, which is doctest's
/// PUBLIC extension point — reading `doctest::detail::g_cs` would work today and
/// break on an upstream refactor of internals we have no claim on.
inline std::string& running_case()
{
    static std::string s;
    return s;
}

/// Records the running case as pending. Called only through PENDING below.
inline void mark_pending(std::string why)
{
    pending_cases().push_back(PendingCase{running_case(), std::move(why)});
}

/// Prints the pending report, or nothing when every case ran.
///
/// Deliberately after doctest's own summary, so the last thing on the terminal is
/// the coverage this run did not have rather than a green total that hides it.
void report_pending();

} // namespace piricad_test

/// Reports the running case as pending and leaves it.
///
/// The `return` is the load-bearing half: a pending case must not go on to assert
/// anything, because whatever it would assert is exactly what this build cannot
/// answer.
#define PENDING(why)                                                                               \
    do {                                                                                           \
        ::piricad_test::mark_pending(why);                                                         \
        return;                                                                                    \
    } while (false)

/// Reports a non-fatal failure whose message has two parts: WHAT went wrong, and
/// the DETAIL that says why.
///
/// The pair is the shape almost every failure in this suite actually has — a
/// command name and the error message it returned, a fixture name and the
/// mismatch, a golden scenario and the first differing byte. doctest's own
/// `FAIL_CHECK` streams its arguments together with nothing between them, which
/// runs the two halves into one unreadable line; this puts them on separate lines
/// and pins the location to the call site rather than to this header.
#define FAIL_WITH(what, detail) ADD_FAIL_CHECK_AT(__FILE__, __LINE__, what, "\n        ", detail)
