// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: where the view has been (`render::ViewHistory`).
//
// The rule of what counts as one step back is the feature: a history that kept
// every wheel notch would take ten ÖNCEKİ to undo one gesture, and one that
// kept a move that went nowhere would make ÖNCEKİ do nothing. The canvas only
// reports its moves; this is where the rule is held, without a window.
#include "kentos_test.hpp"

#include "kentos_cad/render/view_history.hpp"

#include <vector>

using namespace kentos;
using render::ViewHistory;
using render::ViewState;

namespace {

/// A view at `step` metres east, at a fixed scale — every one different.
ViewState at(int step)
{
    return ViewState{core::Point2{static_cast<core::Mm>(step) * 1000, 0}, 10.0};
}

} // namespace

TEST_CASE(
    "GÖRÜNÜM GEÇMİŞİ: otuz bir değişiklikten sonra ÖNCEKİ otuz adım geri, SONRAKİ ileri gider")
{
    ViewHistory h;
    // Thirty-one moves, far apart in time: thirty-one steps, of which the
    // oldest falls off the end.
    for (int i = 0; i < 31; ++i)
        h.moved(at(i), at(i + 1), ViewHistory::Move::Step, i * 10'000);
    CHECK(h.behind() == ViewHistory::kDepth);
    CHECK(h.ahead() == 0);

    ViewState now = at(31);
    for (int i = 30; i >= 1; --i) {
        const auto back = h.back(now);
        REQUIRE(back.has_value());
        CHECK(*back == at(i));
        now = *back;
    }
    // The thirty-first step back is the one the depth let go.
    CHECK_FALSE(h.back(now).has_value());
    CHECK(h.ahead() == ViewHistory::kDepth);

    // And forward again, all the way to where the moves ended.
    for (int i = 2; i <= 31; ++i) {
        const auto ahead = h.forward(now);
        REQUIRE(ahead.has_value());
        CHECK(*ahead == at(i));
        now = *ahead;
    }
    CHECK_FALSE(h.forward(now).has_value());
    CHECK(h.behind() == ViewHistory::kDepth);
}

TEST_CASE("GÖRÜNÜM GEÇMİŞİ: bir tekerlek dizisi tek adımdır; araya giren hamle diziyi bitirir")
{
    ViewHistory h;
    // Five notches, 100 ms apart: one look, one step back to where it began.
    for (int i = 0; i < 5; ++i)
        h.moved(at(i), at(i + 1), ViewHistory::Move::Wheel, 1'000 + i * 100);
    CHECK(h.behind() == 1);

    // A pause longer than the fold starts a second step.
    h.moved(at(5), at(6), ViewHistory::Move::Wheel, 1'400 + ViewHistory::kFoldMs + 1);
    CHECK(h.behind() == 2);

    // Any other move ends the burst, so the next notch is a step of its own
    // even if it comes at once.
    h.moved(at(6), at(7), ViewHistory::Move::Step, 3'000);
    h.moved(at(7), at(8), ViewHistory::Move::Wheel, 3'010);
    CHECK(h.behind() == 4);

    // Going back lands where each burst began.
    ViewState now                         = at(8);
    const std::vector<ViewState> expected = {at(7), at(6), at(5), at(0)};
    for (const ViewState& want : expected) {
        const auto back = h.back(now);
        REQUIRE(back.has_value());
        CHECK(*back == want);
        now = *back;
    }
}

TEST_CASE("GÖRÜNÜM GEÇMİŞİ: yerinden oynamayan hamle adım değildir; yeni hamle ileriyi siler")
{
    ViewHistory h;
    // KAPSAM on a view already at the extents: nothing to come back to.
    h.moved(at(0), at(0), ViewHistory::Move::Step, 0);
    CHECK(h.behind() == 0);

    h.moved(at(0), at(1), ViewHistory::Move::Step, 10);
    h.moved(at(1), at(2), ViewHistory::Move::Step, 20);
    REQUIRE(h.back(at(2)).has_value());
    CHECK(h.ahead() == 1);

    // A new move after a step back is a new branch: the old way forward goes.
    h.moved(at(1), at(9), ViewHistory::Move::Step, 30);
    CHECK(h.ahead() == 0);
    CHECK_FALSE(h.forward(at(9)).has_value());
    CHECK(h.behind() == 2);

    // And a new drawing forgets both ways.
    h.clear();
    CHECK(h.behind() == 0);
    CHECK(h.ahead() == 0);
}
