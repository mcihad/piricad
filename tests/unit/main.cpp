// SPDX-License-Identifier: GPL-3.0-or-later
// The unit-test entry point.
//
// `DOCTEST_CONFIG_IMPLEMENT` rather than `..._WITH_MAIN` because this binary has
// one job doctest's generated main does not do: print, after the summary, which
// cases could not run in this build. See `piricad_test.hpp` for why that report
// exists at all.
#define DOCTEST_CONFIG_IMPLEMENT
#include "piricad_test.hpp"

#include <cstdio>

namespace {

/// Records the name of the case doctest is currently running.
///
/// A LISTENER, not a reporter: a listener runs ALONGSIDE the console reporter and
/// prints nothing, where registering a second reporter would replace the output
/// the developer is reading. Every method but one is empty on purpose — this
/// observes one fact and has no opinion about anything else.
struct RunningCaseListener : doctest::IReporter
{
    explicit RunningCaseListener(const doctest::ContextOptions&) {}

    void test_case_start(const doctest::TestCaseData& in) override
    {
        ::piricad_test::running_case() = in.m_name;
    }

    void report_query(const doctest::QueryData&) override {}

    void test_run_start() override {}

    void test_run_end(const doctest::TestRunStats&) override {}

    void test_case_reenter(const doctest::TestCaseData&) override {}

    void test_case_end(const doctest::CurrentTestCaseStats&) override {}

    void test_case_exception(const doctest::TestCaseException&) override {}

    void subcase_start(const doctest::SubcaseSignature&) override {}

    void subcase_end() override {}

    void log_assert(const doctest::AssertData&) override {}

    void log_message(const doctest::MessageData&) override {}

    void test_case_skipped(const doctest::TestCaseData&) override {}
};

} // namespace

REGISTER_LISTENER("piricad-running-case", 1, RunningCaseListener);

namespace piricad_test {

void report_pending()
{
    if (pending_cases().empty()) return;

    // stdout, next to doctest's summary, because a reader who pipes the run
    // through `tail` must still see it. Missing coverage that scrolls past is
    // missing coverage nobody knows about.
    std::fprintf(stdout, "\n[beklemede] %zu vaka bu yapıda çalıştırılamadı:\n",
                 pending_cases().size());
    for (const auto& c : pending_cases())
        std::fprintf(stdout, "  BEKL  %s\n        %s\n", c.test.c_str(), c.why.c_str());
}

} // namespace piricad_test

int main(int argc, char** argv)
{
    doctest::Context context(argc, argv);
    const int failed = context.run();

    // `--exit`, `--list-test-cases` and friends: doctest has answered a query
    // rather than run the suite, and there is no coverage report to make.
    if (context.shouldExit()) return failed;

    piricad_test::report_pending();
    return failed;
}
