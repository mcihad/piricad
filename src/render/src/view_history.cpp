// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/view_history.hpp"

namespace piricad::render {

namespace {

/// Keeps a stack at `ViewHistory::kDepth`, the oldest step going first.
void push_capped(std::deque<ViewState>& stack, const ViewState& state)
{
    stack.push_back(state);
    if (stack.size() > ViewHistory::kDepth) stack.pop_front();
}

} // namespace

void ViewHistory::moved(const ViewState& from, const ViewState& to, Move how, std::int64_t at_ms)
{
    const bool folds =
        how == Move::Wheel && wheeling_ && at_ms >= wheeled_ && at_ms - wheeled_ <= kFoldMs;
    wheeling_ = how == Move::Wheel;
    wheeled_  = at_ms;
    if (folds || from == to) return;

    push_capped(back_, from);
    forward_.clear();
}

std::optional<ViewState> ViewHistory::back(const ViewState& now)
{
    wheeling_ = false;
    if (back_.empty()) return std::nullopt;
    const ViewState target = back_.back();
    back_.pop_back();
    push_capped(forward_, now);
    return target;
}

std::optional<ViewState> ViewHistory::forward(const ViewState& now)
{
    wheeling_ = false;
    if (forward_.empty()) return std::nullopt;
    const ViewState target = forward_.back();
    forward_.pop_back();
    push_capped(back_, now);
    return target;
}

void ViewHistory::clear()
{
    back_.clear();
    forward_.clear();
    wheeling_ = false;
}

} // namespace piricad::render
