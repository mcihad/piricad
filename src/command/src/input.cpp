// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/input.hpp"

namespace kentos::command {

const char* origin_name(Origin o)
{
    switch (o) {
    case Origin::Gui: return "gui";
    case Origin::CommandLine: return "cli";
    case Origin::Script: return "script";
    case Origin::Ai: return "ai";
    case Origin::Batch: return "batch";
    case Origin::Test: return "test";
    }
    return "?";
}

namespace {

/// Hands out the next value of `param` from `args`, or nothing when `args` has no
/// more of it.
///
/// SHARED BY BOTH SOURCES, and the sharing is the point: a script draining its
/// queue and a toolbar button spending its preset must read an argument the same
/// way, or the two clients would disagree about what `noktalar=0,0 10,10` means.
/// What they may differ on is what happens when it runs out, so that decision is
/// the caller's — `ran_out` reports it rather than acting on it.
std::optional<Value> next_of(const Args& args,
                             std::vector<std::pair<std::string, std::size_t>>& cursor,
                             const Param& param, bool& ran_out)
{
    ran_out = false;

    const Value* v = args.find(param.name);
    if (v == nullptr || v->empty()) {
        ran_out = true;
        return std::nullopt;
    }

    std::size_t* pos = nullptr;
    for (auto& [name, index] : cursor) {
        if (name == param.name) {
            pos = &index;
            break;
        }
    }
    if (pos == nullptr) {
        cursor.emplace_back(param.name, 0);
        pos = &cursor.back().second;
    }

    // A list-valued argument is drained one element per request, so a command
    // loop reads it exactly as it reads successive mouse clicks (kentoscad.md §2.4).
    if (v->kind() == Value::Kind::PointList || v->kind() == Value::Kind::Point) {
        const auto& pts = v->as_points();
        if (*pos >= pts.size()) {
            ran_out = true;
            return std::nullopt;
        }
        return Value::point(pts[(*pos)++]);
    }

    if (v->kind() == Value::Kind::IdList) {
        const auto& ids = v->as_ids();
        if (param.kind == ParamKind::Selection) {
            if (*pos > 0) {
                ran_out = true;
                return std::nullopt;
            }
            ++(*pos);
            return *v;
        }
        if (*pos >= ids.size()) {
            ran_out = true;
            return std::nullopt;
        }
        return Value::integer(ids[(*pos)++]);
    }

    if (*pos > 0) {
        ran_out = true;
        return std::nullopt;
    }
    ++(*pos);
    return *v;
}

} // namespace

std::optional<Value> ArgInputSource::take(const Param& param)
{
    // A QUEUE THAT RUNS OUT ENDS THE COMMAND. A script has no user to ask, so this
    // is exactly where an interactive run would have had ESC pressed.
    bool ran_out           = false;
    std::optional<Value> v = next_of(args_, cursor_, param, ran_out);
    if (ran_out) exhausted_ = true;
    return v;
}

std::optional<Value> InteractiveInputSource::take(const Param& param)
{
    // A PRESET THAT RUNS OUT HANDS OVER TO THE USER — the opposite of the line
    // above, and the reason the two sources cannot share a `take`. `SEÇ mod=KUTU`
    // started from a button answers `mod` from here and then waits for the two
    // corners to be clicked; treating the end of the preset as exhaustion would
    // finish the command before the user had touched the canvas.
    bool ran_out = false;
    if (preset_.size() != 0) {
        std::optional<Value> v = next_of(preset_, cursor_, param, ran_out);
        if (v) return v;
    }
    return std::nullopt;
}

} // namespace kentos::command
