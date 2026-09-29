// SPDX-License-Identifier: GPL-3.0-or-later
// core.double_line — ÇİFTÇİZGİ. An axis drawn point by point, and the two
// parallels that run beside it (netcad_plan.md N-11; Netcad's Paralel Çizgi,
// wiki 217385136).
//
// A road, a canal, a railway: drawn as a centre line and read as a strip. OFSET
// makes a parallel after the fact, one side and one distance at a time; this
// makes both sides as the axis is drawn, each at its own width, with every
// corner resolved the way OFSET resolves it. It is the SAME computation —
// `core::run_parallel`, the body `core::entity_parallel` runs for an open line —
// so neither command has a parallel of its own to disagree with the other.
//
// RIGHT IS POSITIVE, AS EVERYWHERE HERE: `sol` and `sag` are the two sides of
// the axis looking along the way it is drawn, and each is a LENGTH. Zero draws
// nothing on that side; a negative one is refused, because the side is named,
// not signed.
//
// THE WIDTHS ARE ASKED BEFORE THE POINTS, as ÇOKGEN asks its side count first:
// with both known, every point of the axis shows the double line it will make.
// The canvas draws `core::double_line` under the cursor (`RubberShape::
// DoubleLine`, through `command::ghost_outline`) — the call this body commits
// with — so the guide IS the result. A run given its widths as arguments asks
// for the points only.
//
// WHAT IS DRAWN, in this order, so the keys are predictable: the axis (unless
// `eksen=cizme`), the left parallel, the right parallel, then the two end caps
// (`uclar=kapali`). Each parallel is its own object on its own layer
// (`katman_sol`, `katman_sag`, created if absent) and, when the axis is drawn,
// records that it was made from it (core/lineage.hpp).
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The corner a word names; anything but the two named is the sharp one, which
/// is also what the bus lets through for an omitted `kose`.
core::JoinStyle join_from(const std::string& word)
{
    if (core::turkish_key_equals(word, "yuvarlak")) return core::JoinStyle::Round;
    if (core::turkish_key_equals(word, "pah")) return core::JoinStyle::Bevel;
    return core::JoinStyle::Miter;
}

/// A length as a surveyor reads one: metres, three decimals, the Turkish comma.
std::string metres(core::Mm v)
{
    return core::metres_fixed(v, 3, ',') + " m";
}

std::int64_t key_of(const core::Document& doc, core::EntityId e)
{
    return static_cast<std::int64_t>(core::raw(doc.key_of(e)));
}

/// The layer one side goes to: the layer named for it — created if absent, as
/// drawing on a new layer creates it — or the active one when none was named.
core::LayerId side_layer(Context& ctx, const std::string& name)
{
    if (name.empty()) return ctx.active_layer();
    return ctx.transaction().ensure_layer(name);
}

/// Records that `made` came from `axis` when there is one.
bool derive_from(Context& ctx, core::EntityId made, std::optional<core::EntityId> axis)
{
    if (!axis) return true;
    const core::EntityId sources[] = {*axis};
    if (const auto st = ctx.derive(made, std::span<const core::EntityId>(sources)); !st) {
        ctx.refuse(st.error());
        return false;
    }
    return true;
}

/// Writes every piece of one side on `layer`; their keys are appended to
/// `keys`. False, having refused, when the document refuses one.
bool write_side(Context& ctx, const std::vector<core::ParallelPiece>& pieces, core::LayerId layer,
                std::optional<core::EntityId> axis, std::vector<std::int64_t>& keys)
{
    for (const core::ParallelPiece& piece : pieces) {
        auto added = add_parallel_piece(ctx, layer, piece);
        if (!added) {
            ctx.refuse(added.error());
            return false;
        }
        if (!derive_from(ctx, added.value(), axis)) return false;
        keys.push_back(key_of(ctx.document(), added.value()));
    }
    return true;
}

core::Json keys_json(const std::vector<std::int64_t>& keys)
{
    core::Json out = core::Json::array({});
    for (const std::int64_t key : keys)
        out.push(core::Json::integer(key));
    return out;
}

/// One side's words in the answer: its width, and how many pieces it broke into
/// when that is not one.
std::string side_said(const char* side, core::Mm width, std::size_t pieces)
{
    std::string text = std::string(side) + " paralel " + metres(width);
    if (pieces == 0)
        text += " (bu genişlikte bir şey kalmadı)";
    else if (pieces > 1)
        text += " (" + std::to_string(pieces) + " parça)";
    return text;
}

Task<void> run(Context& ctx)
{
    // 1. THE TWO WIDTHS, in metres, asked first (see the note at the top).
    auto left_m = co_await ctx.number("sol", "Sol genişlik (m) — 0: sol yan çizilmez");
    if (!left_m) co_return; // ESC before anything was drawn
    auto right_m = co_await ctx.number("sag", "Sağ genişlik (m) — 0: sağ yan çizilmez");
    if (!right_m) co_return;

    core::DoubleLineSpec spec;
    spec.left            = core::mm_from_metres(*left_m);
    spec.right           = core::mm_from_metres(*right_m);
    spec.join            = join_from(ctx.argument("kose").as_text());
    spec.close_ends      = core::turkish_key_equals(ctx.argument("uclar").as_text(), "kapali");
    const bool draw_axis = !core::turkish_key_equals(ctx.argument("eksen").as_text(), "cizme");

    // REFUSED NOW, not after the axis is drawn: a width that cannot make a
    // double line is no reason to ask for the points. `core::double_line` says
    // the same again at the end, for a run that got past this.
    if (const auto st = core::check_double_line(spec); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    // 2. THE AXIS, point by point, as ÇOKLUÇİZGİ takes a run — the newest point
    // can be taken back, and taking back the first asks for it again — with the
    // double line the run would make so far under the cursor.
    const std::vector<std::uint8_t> guide = core::encode_double_line_preview(spec);
    std::vector<core::Point2> points;
    while (points.empty()) {
        auto p1 = co_await ctx.point("noktalar", "Eksenin ilk noktası");
        if (!p1) co_return;
        points.push_back(*p1);

        for (;;) {
            auto next =
                co_await ctx.point("noktalar", "Eksenin sonraki noktası — ⌫: son noktayı geri al",
                                   PointOptions{.rubber_band    = true,
                                                .rubber_origin  = points.back(),
                                                .rubber_shape   = RubberShape::DoubleLine,
                                                .rubber_chain   = points,
                                                .rubber_payload = guide,
                                                .can_retract    = true});
            if (next) {
                points.push_back(*next);
                continue;
            }
            if (!ctx.took_back()) break;
            points.pop_back();
            if (points.empty()) break;
        }
    }

    if (points.size() < 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Bir eksen en az iki nokta ister; " +
                                                         std::to_string(points.size()) +
                                                         " nokta verildi.");
        co_return;
    }

    // 3. THE DOUBLE LINE, from the function the guide drew it with.
    auto made = core::double_line(points, spec);
    if (!made) {
        ctx.refuse(made.error());
        co_return;
    }

    // 4. WRITTEN: the axis, the left, the right, the caps.
    const std::string left_name  = ctx.argument("katman_sol").as_text();
    const std::string right_name = ctx.argument("katman_sag").as_text();
    const core::LayerId left_layer =
        spec.left > 0 ? side_layer(ctx, left_name) : ctx.active_layer();
    const core::LayerId right_layer =
        spec.right > 0 ? side_layer(ctx, right_name) : ctx.active_layer();
    if (left_layer == core::kNoLayer || right_layer == core::kNoLayer) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Katman oluşturulamadı: " +
                       (left_layer == core::kNoLayer ? left_name : right_name));
        co_return;
    }

    core::Json report = core::Json::object({});
    std::vector<std::string> said;

    std::optional<core::EntityId> axis;
    if (draw_axis) {
        auto drawn = ctx.transaction().add_polyline(ctx.active_layer(), points);
        if (!drawn) {
            ctx.refuse(drawn.error());
            co_return;
        }
        axis = drawn.value();
        report.set("eksen", core::Json::integer(key_of(ctx.document(), drawn.value())));
        said.emplace_back("eksen");
    }

    std::vector<std::int64_t> left_keys;
    std::vector<std::int64_t> right_keys;
    if (spec.left > 0) {
        if (!write_side(ctx, made.value().left, left_layer, axis, left_keys)) co_return;
        report.set("sol", keys_json(left_keys));
        said.push_back(side_said("sol", spec.left, made.value().left.size()));
    }
    if (spec.right > 0) {
        if (!write_side(ctx, made.value().right, right_layer, axis, right_keys)) co_return;
        report.set("sag", keys_json(right_keys));
        said.push_back(side_said("sağ", spec.right, made.value().right.size()));
    }

    std::vector<std::int64_t> cap_keys;
    for (const std::array<core::Point2, 2>& cap : made.value().caps) {
        auto drawn = ctx.transaction().add_polyline(ctx.active_layer(), cap);
        if (!drawn) {
            ctx.refuse(drawn.error());
            co_return;
        }
        if (!derive_from(ctx, drawn.value(), axis)) co_return;
        cap_keys.push_back(key_of(ctx.document(), drawn.value()));
    }
    if (!cap_keys.empty()) {
        report.set("uclar", keys_json(cap_keys));
        said.emplace_back("iki uç çizgisi");
    }

    // WHAT WAS ASKED, in the words a replay reads back. The widths are recorded
    // as the metres that were meant, whichever way they arrived.
    ctx.record("noktalar", Value::points(points));
    ctx.record("sol", Value::number(*left_m));
    ctx.record("sag", Value::number(*right_m));
    for (const char* word : {"kose", "eksen", "uclar", "katman_sol", "katman_sag"})
        if (!ctx.argument(word).empty()) ctx.record(word, ctx.argument(word));

    std::string line = "Çift çizgi çizildi: ";
    for (std::size_t i = 0; i < said.size(); ++i)
        line += (i > 0 ? ", " : "") + said[i];
    ctx.echo(line + ".");
    ctx.report(std::move(report));
}

} // namespace

KENTOS_COMMAND(double_line)
{
    return CommandSpec{
        .id       = "core.double_line",
        .names    = {"ÇİFTÇİZGİ", "CIFTCIZGI", "DOUBLELINE", "ÇFÇ", "CFC"},
        .title    = "Çift Çizgi",
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::at_least(2),
                              "Eksenin köşe noktaları; en az iki nokta. Paraleller eksenin "
                              "çizildiği yöne bakarak sol ve sağ yanına çizilir")
                    .en("points"),
                Param::number("sol", Arity::exactly(1),
                              "Sol paralelin eksene uzaklığı, metre; 0 verilirse sol yan çizilmez. "
                              "Sol, eksenin çizildiği yöne bakarken soldur")
                    .measured_in("m")
                    .en("left"),
                Param::number("sag", Arity::exactly(1),
                              "Sağ paralelin eksene uzaklığı, metre; 0 verilirse sağ yan çizilmez. "
                              "Sağ, eksenin çizildiği yöne bakarken sağdır")
                    .measured_in("m")
                    .en("right"),
                Param::choice("kose", Arity::optional(), {"keskin", "yuvarlak", "pah"},
                              "Eksenin kırıklarında dış köşenin biçimi: keskin (öntanımlı) "
                              "kesişimde birleşir, yuvarlak gerçek bir yay olur, pah düz kesilir. "
                              "İç köşe her zaman kesişimde birleşir")
                    .en("corner"),
                Param::choice("eksen", Arity::optional(), {"ciz", "cizme"},
                              "ciz (öntanımlı): eksenin kendisi de çizilir · cizme: yalnız "
                              "paraleller çizilir")
                    .en("axis"),
                Param::choice("uclar", Arity::optional(), {"acik", "kapali"},
                              "acik (öntanımlı): uçlar açık kalır · kapali: eksenin iki ucu birer "
                              "çizgiyle kapatılır")
                    .en("ends"),
                Param::text("katman_sol", Arity::optional(),
                            "Sol paralelin katmanı; yoksa oluşturulur. Verilmezse katman= ya da "
                            "etkin katman")
                    .en("left_layer"),
                Param::text("katman_sag", Arity::optional(),
                            "Sağ paralelin katmanı; yoksa oluşturulur. Verilmezse katman= ya da "
                            "etkin katman")
                    .en("right_layer"),
                Param::draw_layer(),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir eksenin sol ve sağında verilen genişliklerde paralel çizgiler çizer; "
                   "köşeler keskin, yuvarlak ya da pahlı, uçlar açık ya da kapalı.",
        .run     = &run,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
