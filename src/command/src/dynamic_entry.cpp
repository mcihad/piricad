// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/dynamic_entry.hpp"

#include "piricad/core/geometry.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace piricad::command {
namespace {

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

/// A whole number of units over a power of ten, written with `places` decimals and no sign
/// games: integer arithmetic, so no locale and no rounding of a rounding.
std::string fixed(std::int64_t scaled, int places)
{
    std::int64_t unit = 1;
    for (int i = 0; i < places; ++i)
        unit *= 10;
    const bool negative  = scaled < 0;
    const std::int64_t a = negative ? -scaled : scaled;
    char out[48];
    (void)std::snprintf(out, sizeof out, "%s%lld.%0*lld", negative ? "-" : "",
                        static_cast<long long>(a / unit), places, static_cast<long long>(a % unit));
    return out;
}

/// `12.500000000` as `12.5`: the figure a person would have typed, which is what the transcript
/// shows and what two equal tokens compare as. A whole number keeps no point.
std::string without_zeros(std::string number)
{
    if (number.find('.') == std::string::npos) return number;
    while (!number.empty() && number.back() == '0')
        number.pop_back();
    if (!number.empty() && number.back() == '.') number.pop_back();
    return number;
}

/// Metres as the Turkish figure a ruler shows: millimetre precision, no trailing zeroes, the
/// comma for the decimal point.
std::string metres_text(double metres)
{
    const auto mm   = static_cast<std::int64_t>(std::llround(metres * 1000.0));
    std::string out = fixed(mm, 3);
    while (!out.empty() && out.back() == '0')
        out.pop_back();
    if (!out.empty() && out.back() == '.')
        out.pop_back();
    else if (const auto dot = out.find('.'); dot != std::string::npos)
        out[dot] = ',';
    return out + " m";
}

const char* unit_word(core::AngleUnit unit)
{
    switch (unit) {
    case core::AngleUnit::Grad: return "grad";
    case core::AngleUnit::Degree: return "°";
    case core::AngleUnit::Radian: return "rad";
    }
    return "grad";
}

/// What an angle that was typed says about itself: its own unit letter when it has one, the
/// session's unit when it does not.
std::string typed_angle_text(std::string_view text, const core::AngleConvention& convention)
{
    std::string out(trim(text));
    core::AngleUnit own{};
    if (!out.empty() && core::angle_unit_from_suffix(out.back(), own)) {
        out.pop_back();
        return out + " " + unit_word(own);
    }
    return out + " " + unit_word(convention.unit);
}

/// Micro-degrees as `D.dddddd` degrees, which the polar token's `d` suffix reads back exactly.
std::string degrees_token(double turns)
{
    const auto udeg =
        static_cast<std::int64_t>(std::llround(turns * static_cast<double>(core::kUDegFullCircle)));
    return without_zeros(fixed(udeg, 6)) + "d";
}

} // namespace

bool takes_dynamic_entry(const Prompt& prompt) noexcept
{
    if (prompt.kind != ParamKind::Point || !prompt.has_rubber_band || !prompt.rubber_base)
        return false;
    switch (prompt.rubber_shape) {
    case RubberShape::AreaEdit:
    case RubberShape::Fixed:
    case RubberShape::Candidates:
    case RubberShape::Angle:
    case RubberShape::MeasureRun:
    case RubberShape::MeasureRing:
    case RubberShape::Parallel:
    case RubberShape::Corner:
    case RubberShape::Break:
    case RubberShape::TrimFence:
    case RubberShape::ArcSweep:
    case RubberShape::PairCorner:
    case RubberShape::EdgeArc:
    case RubberShape::Dimension:
    case RubberShape::DimensionNext: return false;
    default: return true;
    }
}

void DynamicEntry::clear() noexcept
{
    active_ = DynField::Length;
    locked_ = {false, false};
    token_  = {};
    text_   = {};
}

bool DynamicEntry::is_bare_length(std::string_view text)
{
    text = trim(text);
    if (text.empty() || text.find(',') != std::string_view::npos) return false;
    if (text.front() == '@') return false;
    const auto q = evaluate_quantity(text, 0);
    return q.ok() && std::isfinite(q.value().value);
}

core::Result<std::string> DynamicEntry::token_of(DynField field, std::string_view text,
                                                 const ResolveContext& ctx) const
{
    text = trim(text);
    if (text.empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         field == DynField::Length ? "Uzunluk boş." : "Açı boş.");

    if (field == DynField::Length) {
        // THE PROMPT'S GRAMMAR for one number — a unit (`1250 cm`), an expression, the decimal
        // comma — in metres. A length is a distance away: zero and below are not one.
        auto metres = evaluate_answer(text, 0);
        if (!metres) return metres.error();
        if (!std::isfinite(metres.value()) || metres.value() <= 0.0 || metres.value() > 1.0e9)
            return core::err(core::ErrorCode::InvalidArgument,
                             "Uzunluk sıfırdan büyük, makul bir değer olmalı.");
        return without_zeros(
            fixed(static_cast<std::int64_t>(std::llround(metres.value() * 1.0e9)), 9));
    }

    // AN ANGLE IS WHATEVER THE POLAR TOKEN TAKES — `45`, `45d`, `(40+5)g` — and is checked by
    // reading it there, so a figure this class accepts and the grammar refuses cannot exist.
    const std::string probe = "@1<" + std::string(text);
    auto read               = parse_point(probe, core::Point2{}, ctx);
    if (!read) return read.error();
    return std::string(text);
}

core::Result<std::string> DynamicEntry::tab(std::string_view line, const ResolveContext& ctx)
{
    line                    = trim(line);
    const std::size_t here  = index(active_);
    const DynField next     = active_ == DynField::Length ? DynField::Angle : DynField::Length;
    const std::size_t there = index(next);

    if (!line.empty()) {
        auto token = token_of(active_, line, ctx);
        if (!token) return token.error();
        locked_[here] = true;
        token_[here]  = token.value();
        text_[here]   = std::string(line);
        active_       = next;
        return std::string{};
    }

    // NOTHING TYPED: Tab walks to the other field, and a field that was locked is taken up again
    // for editing — its figure comes back into the line and the lock is released.
    active_ = next;
    if (!locked_[there]) return std::string{};
    locked_[there]   = false;
    std::string back = std::move(text_[there]);
    text_[there].clear();
    token_[there].clear();
    return back;
}

core::Result<std::string> DynamicEntry::compose(core::Point2 origin, core::Point2 cursor,
                                                std::string_view line,
                                                const ResolveContext& ctx) const
{
    line = trim(line);

    // THE FIELD EACH OF THE TWO TAKES ITS FIGURE FROM: a lock, else the text in the line when
    // it is the active field's, else the cursor.
    const auto figure = [&](DynField field) -> core::Result<std::string> {
        const std::size_t i = index(field);
        if (locked_[i]) return token_[i];
        if (field == active_ && !line.empty()) return token_of(field, line, ctx);
        if (field == DynField::Length) {
            const core::Mm length = core::segment_length(origin, cursor);
            return without_zeros(
                fixed(length * 1'000'000, 9)); ///< millimetres to nanometres: exact
        }
        return degrees_token(core::direction_turns(origin, cursor, ctx.convention.rule));
    };

    auto length = figure(DynField::Length);
    if (!length) return length.error();
    auto angle = figure(DynField::Angle);
    if (!angle) return angle.error();
    return "@" + length.value() + "<" + angle.value();
}

bool DynamicEntry::needs_cursor(std::string_view line) const noexcept
{
    line           = trim(line);
    const auto own = [&](DynField f) {
        return locked_[index(f)] || (f == active_ && !line.empty());
    };
    return !(own(DynField::Length) && own(DynField::Angle));
}

core::Result<core::Point2> DynamicEntry::resolve(core::Point2 origin, core::Point2 cursor,
                                                 std::string_view line,
                                                 const ResolveContext& ctx) const
{
    auto typed = compose(origin, cursor, line, ctx);
    if (!typed) return typed.error();
    return parse_point(typed.value(), origin, ctx);
}

std::array<DynamicEntry::Shown, 2>
DynamicEntry::shown(core::Point2 origin, core::Point2 cursor, std::string_view line,
                    const core::AngleConvention& convention) const
{
    line = trim(line);
    ResolveContext ctx;
    ctx.convention = convention;

    std::array<Shown, 2> out{};
    for (const DynField field : {DynField::Length, DynField::Angle}) {
        const std::size_t i = index(field);
        Shown& s            = out[i];
        s.locked            = locked_[i];
        s.active            = field == active_;
        s.typing            = !locked_[i] && field == active_ && !line.empty();

        const bool from_text = s.locked || s.typing;
        if (from_text) {
            const std::string_view typed = s.locked ? std::string_view(text_[i]) : line;
            if (s.typing && !token_of(field, typed, ctx)) s.invalid = true;
            if (field == DynField::Length) {
                auto metres = evaluate_answer(trim(typed), 0);
                s.text      = metres ? metres_text(metres.value()) : std::string(typed);
            } else {
                s.text = typed_angle_text(typed, convention);
            }
        } else if (field == DynField::Length) {
            s.text =
                metres_text(static_cast<double>(core::segment_length(origin, cursor)) / 1000.0);
        } else {
            s.text = origin == cursor
                         ? std::string("—")
                         : core::angle_text(core::direction_turns(origin, cursor, convention.rule),
                                            convention.unit);
        }
    }
    return out;
}

} // namespace piricad::command
