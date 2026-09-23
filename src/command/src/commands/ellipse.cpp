// SPDX-License-Identifier: GPL-3.0-or-later
// core.ellipse_draw — ELİPS. Centre and the two axis endpoints.
//
// STORED AS ITS DEFINITION, like DAİRE and YAY: five numbers, from which the
// whole shape follows. The many-sided run is only what gets drawn, and a drawing
// that stored the run would lose the shape's identity the moment somebody zoomed
// in (core/ellipse.hpp).
//
// THE SECOND AXIS IS TAKEN PERPENDICULAR to the first, at the distance the user
// pointed at. Letting them place it freely would let them draw a sheared shape
// that is not an ellipse at all — and the record would then hold two axes that no
// ellipse has, which nothing downstream could draw.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/text.hpp"
#include <optional>
#include <string>

#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <span>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    // TWO WAYS TO FIX ONE ELLIPSE, and they are not the same gesture
    // (TODOS-CAD P2-6):
    //   merkez — the centre, then one end of the first axis. What a plan gives
    //            when the centre is the known thing: a manhole, a roundabout.
    //   eksen  — the two ENDS of the first axis, the centre being their
    //            midpoint. What a measured ellipse gives, and AutoCAD's own
    //            default, because the two ends are what a tape reaches.
    std::string how = "merkez";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const bool by_axis = core::turkish_key_equals(how, "eksen");

    std::optional<core::Point2> centre;
    std::optional<core::Point2> major;

    if (by_axis) {
        auto from = co_await ctx.point("birinci", "Birinci eksenin bir ucu");
        if (!from) co_return;
        auto to = co_await ctx.point("ikinci_uc", "Birinci eksenin öteki ucu",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *from,
                                                  .rubber_shape  = RubberShape::Line});
        if (!to) co_return;
        if (*from == *to) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Eksenin iki ucu aynı nokta; elipsin ekseni sıfır "
                                         "olamaz."));
            co_return;
        }
        centre = core::Point2{(from->x + to->x) / 2, (from->y + to->y) / 2};
        major  = *to;

        // NOTHING EXTRA IS RECORDED HERE: `birinci` and `ikinci_uc` were AWAITED,
        // so they are in the record already, and `yontem` was bound. What this
        // branch has to do is make sure the tail below does not overwrite them —
        // see the note there.
    } else {
        centre = co_await ctx.point("merkez", "Elipsin merkezi");
        if (!centre) co_return; // ESC before anything was drawn

        major = co_await ctx.point("birinci", "Birinci eksenin ucu",
                                   PointOptions{.rubber_band   = true,
                                                .rubber_origin = *centre,
                                                .rubber_shape  = RubberShape::Line});
        if (!major) co_return;

        if (major->x == centre->x && major->y == centre->y) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Birinci eksenin ucu merkezle aynı yerde; elipsin ekseni sıfır olamaz.");
            co_return;
        }
    }

    auto reach = co_await ctx.point("ikinci", "İkinci eksenin uzaklığı",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Ellipse,
                                                 .rubber_chain  = {*major}});
    if (!reach) co_return;

    // The first axis as a vector, and the perpendicular to it.
    const auto ax      = static_cast<double>(major->x - centre->x);
    const auto ay      = static_cast<double>(major->y - centre->y);
    const double a_len = std::sqrt(ax * ax + ay * ay);

    // How far the third click reached, measured ACROSS the first axis: the
    // component perpendicular to it. Clicking along the first axis therefore
    // gives a second axis of zero, and the command says so rather than drawing a
    // line and calling it an ellipse.
    const auto rx       = static_cast<double>(reach->x - centre->x);
    const auto ry       = static_cast<double>(reach->y - centre->y);
    const double across = (rx * -ay + ry * ax) / a_len;

    const double b = across < 0.0 ? -across : across;
    if (b < 1.0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "İkinci eksen sıfır: üçüncü nokta birinci eksenin üzerinde. "
                   "Eksene dik bir yer seçin.");
        co_return;
    }

    // The perpendicular unit vector, scaled to that distance.
    const core::Point2 minor{centre->x + core::mm_round(-ay / a_len * b),
                             centre->y + core::mm_round(ax / a_len * b)};

    // A PARTIAL ellipse when both angles are given: the sweep, counter-clockwise
    // from the first axis in the ellipse's own parameter (core/ellipse.hpp), is
    // the kind's payload; a whole one carries none.
    const Value from = ctx.argument("baslangic");
    const Value to   = ctx.argument("bitis");
    if (from.empty() != to.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kısmi elips için baslangic= ve bitis= birlikte verilir (derece).");
        co_return;
    }
    std::vector<std::uint8_t> payload;
    if (!from.empty()) {
        const auto norm = [](double deg) {
            auto udeg =
                static_cast<std::int64_t>(std::llround(deg * 1000000.0)) % core::kUDegFullCircle;
            if (udeg < 0) udeg += core::kUDegFullCircle;
            return udeg;
        };
        const core::EllipseArc arc{norm(from.as_number()), norm(to.as_number())};
        if (arc.start_udeg == arc.end_udeg) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Başlangıç ve bitiş açısı aynı; tam elips için ikisini de vermeyin.");
            co_return;
        }
        payload = core::encode_ellipse_arc(arc);
    }

    const core::Point2 def[3]{*centre, *major, minor};
    const core::RingGeometry::RingInput ring{std::span<const core::Point2>(def, 3),
                                             core::RingRole::Open, 0};
    auto created = ctx.transaction().add_kind(
        ctx.active_layer(), core::kEllipseKind,
        std::span<const core::RingGeometry::RingInput>(&ring, 1), payload);
    if (!created) {
        ctx.refuse(created.error());
        co_return; // the bus rolls the transaction back
    }
    if (!from.empty()) {
        ctx.record("baslangic", from);
        ctx.record("bitis", to);
    }

    // RECORDED IN THE METHOD THAT WAS ASKED, and this is a fix a journal replay
    // found. The tail used to record `merkez` and `birinci` unconditionally, and
    // under `yontem=eksen` that overwrote the FIRST axis end with the MAJOR end —
    // which is the second one. The line then went out saying `birinci` and
    // `ikinci_uc` were the same point, and replaying it refused with "Eksenin iki
    // ucu aynı nokta". A record that cannot be replayed is not a record
    // (Article 1.4).
    //
    // Under `eksen` the two ends are already in the record, awaited under their
    // own names; under the centre method the resolved centre and major end are
    // what this run derived and what a replay must be handed. `ikinci` means the
    // same thing in both, so it is recorded once here.
    if (!by_axis) {
        ctx.record("merkez", Value::point(*centre));
        ctx.record("birinci", Value::point(*major));
    }
    ctx.record("ikinci", Value::point(*reach));
}

} // namespace

KENTOS_COMMAND(ellipse_draw)
{
    return CommandSpec{
        .id       = "core.ellipse_draw",
        .names    = {"ELİPS", "ELIPS", "ELLIPSE", "EL"},
        .title    = "Elips",
        .category = Category::Draw,
        .params =
            {
                Param::points("merkez", Arity::optional(), "Elipsin merkezi").en("center"),
                Param::points("birinci", Arity::optional(),
                              "merkez: birinci eksenin ucu · eksen: birinci eksenin bir ucu")
                    .en("first"),
                Param::points("ikinci", Arity::optional(),
                              "İkinci eksenin uzaklığı; eksene dik ölçülür")
                    .en("second"),
                Param::choice("yontem", Arity::optional(), {"merkez", "eksen"},
                              "merkez: merkez + eksen ucu · eksen: eksenin iki ucu")
                    .en("method"),
                Param::points("ikinci_uc", Arity::optional(), "eksen: birinci eksenin öteki ucu")
                    .en("second_end"),
                Param::number("baslangic", Arity::optional(),
                              "Kısmi elips: başlangıç açısı, derece, birinci eksenden saat "
                              "yönünün tersine")
                    .en("start"),
                Param::number("bitis", Arity::optional(),
                              "Kısmi elips: bitiş açısı, derece; baslangic ile birlikte")
                    .en("end"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.",
        .run     = &run,
    };
}

} // namespace kentos::command
