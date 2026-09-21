// SPDX-License-Identifier: GPL-3.0-or-later
// core.select — SEÇ.
//
// Selecting is a COMMAND even though it mutates nothing, and that is not a
// formality. Constitution Article 1.2 makes the mouse one client among equals: if
// a rubber-band drag reached the selection directly, a script and the AI would
// have no way to say "the parcels inside this box", and `secimi_al()` in
// kentoscad.md §5.1 would have nothing to read. So the drag builds the same
// invocation the command line builds, and both take the bus.
//
// It carries `UndoPolicy::None` and `Flags::ReadOnly`, exactly as `core.mode` and
// `core.preference` do, because model.md R43 puts selection outside document
// state: it never touches `content_hash()`, it is not undoable, and it is not
// journalled as a document mutation. `GERİAL` undoes what you drew, never what
// you had highlighted.
//
// Identity is `EntityKey` throughout (R44). A key survives a save, a reorder and
// a reload; a dense slot does not, and a selection that silently shifted by one
// after a compaction would delete the neighbouring parcel.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

using core::EntityKey;

/// What the user asked for. Canonical Turkish names; the aliases are folded onto
/// these once, here, so no other file repeats the list.
enum class Mode : std::uint8_t {
    Report,
    All,
    Clear,
    Objects,
    Layer,
    Window,
    Crossing,
    Box,
    Point,

    /// ÇOKGEN / ÇOKGENKESEN — a polygon instead of a box. A parcel block is not
    /// rectangular and neither is a road corridor, so a box either misses what
    /// the user meant or takes the neighbours with it.
    Polygon,
    PolygonCrossing,

    /// ÇİT — a line drawn THROUGH the drawing; what it crosses is what it takes.
    /// A run of kerb stones along a road, without the buildings behind them.
    Fence,

    /// ÖNCEKİ — the selection before this one. One step deep on purpose: a stack
    /// of selections is a stack nobody can keep in their head.
    Previous,

    /// SON — the most recently created entity. What a drafter means by "that
    /// one" after drawing it.
    Last,
};

/// What to do with what was found.
enum class Op : std::uint8_t { Replace, Add, Remove, Toggle };

bool matches(const std::string& folded, std::initializer_list<const char*> names)
{
    for (const char* n : names)
        if (core::turkish_key_equals(folded, n)) return true;
    return false;
}

bool parse_mode(const std::string& typed, Mode& out)
{
    if (matches(typed, {"TÜMÜ", "TUMU", "ALL", "HEPSİ", "HEPSI"})) {
        out = Mode::All;
    } else if (matches(typed, {"TEMİZLE", "TEMIZLE", "CLEAR", "NONE", "HİÇ", "HIC"})) {
        out = Mode::Clear;
    } else if (matches(typed, {"NESNE", "NESNELER", "OBJECT", "LAST"})) {
        out = Mode::Objects;
    } else if (matches(typed, {"KATMAN", "LAYER", "K"})) {
        out = Mode::Layer;
    } else if (matches(typed, {"PENCERE", "WINDOW", "W"})) {
        out = Mode::Window;
    } else if (matches(typed, {"KESEN", "CROSSING", "C"})) {
        out = Mode::Crossing;
    } else if (matches(typed, {"KUTU", "BOX", "B"})) {
        out = Mode::Box;
    } else if (matches(typed, {"NOKTA", "POINT", "P"})) {
        out = Mode::Point;
    } else if (matches(typed, {"ÇOKGEN", "COKGEN", "WPOLYGON", "WP"})) {
        out = Mode::Polygon;
    } else if (matches(typed, {"ÇOKGENKESEN", "COKGENKESEN", "CPOLYGON", "CP"})) {
        out = Mode::PolygonCrossing;
    } else if (matches(typed, {"ÇİT", "CIT", "FENCE", "F"})) {
        out = Mode::Fence;
    } else if (matches(typed, {"ÖNCEKİ", "ONCEKI", "PREVIOUS", "PR"})) {
        out = Mode::Previous;
    } else if (matches(typed, {"SON", "LAST", "L"})) {
        out = Mode::Last;
    } else {
        return false;
    }
    return true;
}

const char* mode_name(Mode m)
{
    switch (m) {
    case Mode::Report: return "DURUM";
    case Mode::All: return "TÜMÜ";
    case Mode::Clear: return "TEMİZLE";
    case Mode::Objects: return "NESNE";
    case Mode::Layer: return "KATMAN";
    case Mode::Window: return "PENCERE";
    case Mode::Crossing: return "KESEN";
    case Mode::Box: return "KUTU";
    case Mode::Point: return "NOKTA";
    case Mode::Polygon: return "ÇOKGEN";
    case Mode::PolygonCrossing: return "ÇOKGENKESEN";
    case Mode::Fence: return "ÇİT";
    case Mode::Previous: return "ÖNCEKİ";
    case Mode::Last: return "SON";
    }
    return "DURUM";
}

bool parse_op(const std::string& typed, Op& out)
{
    if (matches(typed, {"DEĞİŞTİR", "DEGISTIR", "REPLACE", "YENİ", "YENI"})) {
        out = Op::Replace;
    } else if (matches(typed, {"EKLE", "ADD", "+"})) {
        out = Op::Add;
    } else if (matches(typed, {"ÇIKAR", "CIKAR", "REMOVE", "-"})) {
        out = Op::Remove;
    } else if (matches(typed, {"TERSİNE", "TERSINE", "TOGGLE"})) {
        out = Op::Toggle;
    } else {
        return false;
    }
    return true;
}

const char* op_name(Op o)
{
    switch (o) {
    case Op::Replace: return "DEĞİŞTİR";
    case Op::Add: return "EKLE";
    case Op::Remove: return "ÇIKAR";
    case Op::Toggle: return "TERSİNE";
    }
    return "DEĞİŞTİR";
}

/// Keys, in ascending order, of a run of slots the picker returned. This is the
/// slot-to-key translation model.md R2 places at the command bus boundary, and it
/// is the only place in this file where a dense id exists at all.
std::vector<EntityKey> keys_of(const core::Document& doc, const std::vector<core::EntityId>& slots)
{
    std::vector<EntityKey> keys;
    keys.reserve(slots.size());
    for (core::EntityId s : slots)
        keys.push_back(doc.key_of(s));
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

std::string key_list(const std::vector<EntityKey>& keys, std::size_t limit = 12)
{
    std::string out;
    for (std::size_t i = 0; i < keys.size() && i < limit; ++i) {
        if (i) out += ", ";
        out += std::to_string(core::raw(keys[i]));
    }
    if (keys.size() > limit) out += ", … (+" + std::to_string(keys.size() - limit) + ")";
    return out;
}

void report(Context& ctx, const Selection& selection)
{
    if (selection.empty()) {
        ctx.echo("Seçimde nesne yok.");
        return;
    }
    ctx.echo(std::to_string(selection.size()) + " nesne seçili: " + key_list(selection.keys()));
}

Task<void> run_select(Context& ctx)
{
    Bus& bus                  = ctx.session().bus();
    Selection& selection      = bus.selection();
    const core::Document& doc = bus.document();

    // ---- mode ----
    Mode mode = Mode::Report;
    if (const Value v = ctx.argument("mod"); !v.empty()) {
        if (!parse_mode(v.as_text(), mode)) {
            ctx.echo("Beklenen mod: TÜMÜ | TEMİZLE | NESNE | KATMAN | PENCERE | KESEN | KUTU | "
                     "NOKTA | ÇOKGEN | ÇOKGENKESEN | ÇİT | ÖNCEKİ | SON. Girilen: '" +
                     v.as_text() + "'");
            co_return;
        }
    } else if (!ctx.argument("nesneler").empty()) {
        mode = Mode::Objects; // `SEÇ nesneler=12` needs no ceremony
    } else if (!ctx.argument("noktalar").empty()) {
        mode = ctx.argument("noktalar").as_points().size() >= 2 ? Mode::Box : Mode::Point;
    }

    if (mode == Mode::Report) {
        report(ctx, selection);
        co_return;
    }

    // ---- operation ----
    Op op = Op::Replace;
    if (const Value v = ctx.argument("islem"); !v.empty()) {
        if (!parse_op(v.as_text(), op)) {
            ctx.echo("Beklenen işlem: DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE. Girilen: '" + v.as_text() +
                     "'");
            co_return;
        }
    }

    // ---- what was picked ----
    std::vector<EntityKey> picked;
    std::vector<core::EntityId> slots;

    // ASKED FOR WHEN THEY ARE NOT GIVEN. `SEÇ PENCERE` could always take two
    // corners as arguments and could never ask for them, so the tool column's
    // "Alan Seç" button had nothing to send and shipped disabled. A command that
    // can be typed with its arguments must also be able to collect them, or the
    // GUI is a client with less reach than the command line (Article 1.2).
    std::vector<core::Point2> supplied = ctx.argument("noktalar").as_points();

    if (mode == Mode::Window || mode == Mode::Crossing || mode == Mode::Box) {
        if (supplied.empty()) {
            auto first = co_await ctx.point("noktalar", "Seçim kutusunun ilk köşesi");
            if (!first) co_return; // ESC before anything was picked
            supplied.push_back(*first);
        }
        if (supplied.size() < 2) {
            auto second = co_await ctx.point("noktalar", "Karşı köşe",
                                             PointOptions{.rubber_band   = true,
                                                          .rubber_origin = supplied.front(),
                                                          .rubber_shape  = RubberShape::Rectangle});
            if (!second) co_return;
            supplied.push_back(*second);
        }
    } else if (mode == Mode::Polygon || mode == Mode::PolygonCrossing || mode == Mode::Fence) {
        // A RUN OF POINTS, ENDED BY THE USER. A polygon and a fence have no fixed
        // number of corners, so the loop runs until ESC or the right button —
        // exactly as `ALAN` collects a boundary. The guide shows the shape so
        // far, which is the only thing that says what the next click will take.
        const bool closed = mode != Mode::Fence;

        // ONLY WHEN THE CALLER DID NOT SAY. A typed line, a script and a journal
        // give the whole run up front, and asking anyway drains the same points
        // a second time — which is not a longer polygon but a self-overlapping
        // one, and `ring_contains` then answers that nothing is inside it. The
        // box branch above keeps the same rule for the same reason.
        // ASKED FOR ONLY WHEN NOTHING CAME UP FRONT, and then until the run ends.
        //
        // The condition used to be `while (supplied.empty())`, which stopped the
        // loop after the FIRST point: a fence or a selection polygon could never
        // collect more than one point from the mouse, and the command then
        // refused with "Çit en az iki nokta ister" — a mode reachable by mouse
        // that cannot be used by one (CLAUDE.md 5.15). The intent behind that
        // condition was right and its spelling was not: a script that gave the
        // whole run must not be asked anything, because asking drains the same
        // points a second time and a doubled polygon overlaps itself.
        const bool ask_for_them = supplied.empty();
        while (ask_for_them) {
            PointOptions options;
            if (!supplied.empty()) {
                options.rubber_band   = true;
                options.rubber_origin = supplied.back();
                options.rubber_shape  = closed ? RubberShape::Ring : RubberShape::Line;
                options.rubber_chain  = supplied;
            }
            auto next = co_await ctx.point(
                "noktalar",
                supplied.empty() ? (closed ? "Seçim çokgeninin ilk köşesi" : "Çitin ilk noktası")
                                 : (closed ? "Sonraki köşe" : "Çitin sonraki noktası"),
                options);
            if (!next) break;
            supplied.push_back(*next);
        }
        if (supplied.size() < (closed ? 3u : 2u)) {
            ctx.echo(closed ? "Seçim çokgeni en az üç köşe ister." : "Çit en az iki nokta ister.");
            co_return;
        }
    } else if (mode == Mode::Point && supplied.empty()) {
        auto aim = co_await ctx.point("noktalar", "Seçilecek nesnenin üzerinde bir nokta");
        if (!aim) co_return;
        supplied.push_back(*aim);
    }

    const auto need_points = [&](std::size_t n) -> bool {
        if (supplied.size() >= n) return true;
        ctx.echo(std::string("'") + mode_name(mode) + "' " + std::to_string(n) +
                 " nokta bekliyor. Girilen: " + std::to_string(supplied.size()) + " nokta.");
        return false;
    };

    switch (mode) {
    case Mode::Report: co_return;

    case Mode::Clear: break;

    case Mode::All: {
        const core::EntityTable& entities = doc.entities();
        for (core::EntityId e = 0; e < entities.size(); ++e)
            if (entities.visible(e)) picked.push_back(doc.key_of(e));
        break;
    }

    case Mode::Polygon:
    case Mode::PolygonCrossing: {
        core::pick_in_polygon(
            doc, supplied,
            mode == Mode::Polygon ? core::PickMode::Window : core::PickMode::Crossing, slots);
        picked = keys_of(doc, slots);
        break;
    }

    case Mode::Fence: {
        core::pick_along_fence(doc, supplied, slots);
        picked = keys_of(doc, slots);
        break;
    }

    case Mode::Previous: {
        // THE KEYS, NOT THE SLOTS. A key survives an erase and a reload; a slot
        // is an index into this document's table (model.md R1). An entity that
        // has since been deleted is dropped rather than resurrected.
        for (EntityKey k : bus.previous_selection().keys()) {
            // ALIVE, NOT MERELY KNOWN. A deleted entity keeps its slot mapping
            // — that is what lets an undo bring it back — so `slot_of` alone
            // would resurrect a selection of things the user has just erased,
            // and the next SİL would report deleting what is already gone.
            const core::EntityId slot = doc.slot_of(k);
            if (slot != core::kNoEntity && doc.alive(slot)) picked.push_back(k);
        }
        break;
    }

    case Mode::Last: {
        // THE MOST RECENTLY CREATED LIVE ENTITY, which is what "that one" means
        // after drawing something. Walked from the top because a slot is handed
        // out in creation order and the newest is the highest live one.
        const core::EntityTable& entities = doc.entities();
        // COUNTED IN THE ID'S OWN TYPE. `size()` returns a `std::size_t` and an
        // `EntityId` is narrower, so the loop variable is made from the bound
        // rather than assigned it — the narrowing is real and belongs where it
        // is written.
        for (auto e = static_cast<core::EntityId>(entities.size()); e-- > 0;)
            if (doc.alive(e) && entities.visible(e)) {
                picked.push_back(doc.key_of(e));
                break;
            }
        break;
    }

    case Mode::Objects: {
        const Value v = ctx.argument("nesneler");
        if (v.empty()) {
            ctx.echo("'NESNE' en az bir nesne kimliği bekliyor. Örnek: SEÇ NESNE nesneler=1");
            co_return;
        }
        for (std::int64_t raw : v.as_ids()) {
            if (raw <= 0) {
                ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                         ". Kimlikler 1'den başlar.");
                co_return;
            }
            const auto key = static_cast<EntityKey>(static_cast<std::uint64_t>(raw));
            if (doc.slot_of(key) == core::kNoEntity) {
                ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
                co_return;
            }
            picked.push_back(key);
        }
        break;
    }

    case Mode::Layer: {
        const Value v = ctx.argument("katman");
        if (v.empty()) {
            ctx.echo("'KATMAN' bir katman adı bekliyor. Örnek: SEÇ mod=KATMAN katman=\"PARSEL\"");
            co_return;
        }

        const std::string wanted       = v.as_text();
        const core::LayerId layer_slot = doc.find_layer(wanted);
        if (layer_slot == core::kNoLayer) {
            ctx.echo("Katman bulunamadı: " + wanted);
            co_return;
        }

        // Hidden entities are skipped for the same reason TÜMÜ skips them: what
        // the user cannot see, the user did not mean to select. Selecting a
        // hidden layer's contents this way is therefore a no-op rather than a
        // silent grab — turn the eye back on first.
        const core::EntityTable& entities = doc.entities();
        for (core::EntityId e = 0; e < entities.size(); ++e)
            if (entities.visible(e) && entities.layer[e] == layer_slot)
                picked.push_back(doc.key_of(e));
        break;
    }

    case Mode::Point: {
        if (!need_points(1)) co_return;
        const core::Point2 aim = supplied.front();

        // Pixels by default (core.secim.tolerans), metres when a client states
        // one. A script has no screen, so without `tolerans` it picks what lies
        // exactly under the point — honest, and never a silent guess.
        core::Mm radius = bus.aid_settings().pick_radius;
        if (const Value t = ctx.argument("tolerans"); !t.empty())
            radius = core::mm_from_metres(t.as_number());

        // WHICH ONE OF THEM, and this is what makes the shell's chooser a command
        // rather than a gesture. A click on a cadastral sheet lands on a parcel,
        // its boundary and the ada boundary at once; `sira=1` is the nearest and
        // is what a bare NOKTA has always meant, `sira=2` is the next one down.
        // Without it, reaching past the top object would be a thing only a mouse
        // could do (CLAUDE.md 5.15) and a script could never repeat.
        std::size_t want = 1;
        if (const Value n = ctx.argument("sira"); !n.empty()) {
            const double asked = n.as_number();
            if (asked < 1.0) {
                ctx.echo("'sira' 1'den küçük olamaz; 1 en yakın nesnedir.");
                co_return;
            }
            want = static_cast<std::size_t>(asked);
        }

        if (want == 1) {
            const core::EntityId hit = core::pick_nearest(doc, aim, radius);
            if (hit != core::kNoEntity) picked.push_back(doc.key_of(hit));
            break;
        }

        std::vector<core::EntityId> under;
        core::pick_all(doc, aim, radius, under);
        if (want > under.size()) {
            ctx.echo("O noktada " + std::to_string(under.size()) + " nesne var; " +
                     std::to_string(want) + ". istendi.");
            co_return;
        }
        picked.push_back(doc.key_of(under[want - 1]));
        break;
    }

    case Mode::Window:
    case Mode::Crossing:
    case Mode::Box: {
        if (!need_points(2)) co_return;
        const core::Point2 a = supplied[0];
        const core::Point2 b = supplied[1];

        core::Box2 box{};
        box.extend(a);
        box.extend(b);

        // The CAD convention, and it is muscle memory rather than a preference:
        // dragging left to right takes what is wholly inside, right to left takes
        // whatever the box touches.
        core::PickMode how = core::PickMode::Window;
        if (mode == Mode::Crossing) how = core::PickMode::Crossing;
        if (mode == Mode::Box) how = b.x < a.x ? core::PickMode::Crossing : core::PickMode::Window;

        slots.clear();
        core::pick_in_box(doc, box, how, slots);
        picked = keys_of(doc, slots);
        break;
    }
    }

    std::sort(picked.begin(), picked.end());
    picked.erase(std::unique(picked.begin(), picked.end()), picked.end());

    // ---- apply ----
    const std::size_t before = selection.size();

    // REMEMBERED BEFORE IT IS REPLACED, and only when this run actually changes
    // it: `SEÇ ÖNCEKİ` twice has to go back and forth rather than forget what it
    // was going back to, and a reporting run must not overwrite the way back.
    if (mode != Mode::Previous) bus.remember_selection();

    if (mode == Mode::Clear) {
        selection.clear();
    } else {
        if (op == Op::Replace) selection.clear();
        for (EntityKey k : picked) {
            switch (op) {
            case Op::Replace:
            case Op::Add: selection.add(k); break;
            case Op::Remove: selection.remove(k); break;
            case Op::Toggle: selection.toggle(k); break;
            }
        }
    }

    if (bus.on_selection_changed) bus.on_selection_changed();

    // The RESOLVED selection is recorded, not the gesture that produced it, so
    // every client's run reads the same however it aimed (kentoscad.md §2.2).
    ctx.record("mod", Value::text(mode_name(mode)));
    // The layer NAME is recorded, not the slot it resolved to: a slot is an
    // index into this document's table and means nothing in a replay against
    // another one (model.md R2, R43).
    if (mode == Mode::Layer) ctx.record("katman", ctx.argument("katman"));
    if (mode == Mode::Point)
        if (const Value n = ctx.argument("sira"); !n.empty()) ctx.record("sira", n);
    // The gesture too, so a replay of a WINDOW pick re-runs the same box rather
    // than only restoring the keys it happened to find. The resolved selection is
    // recorded below and remains what a client reads back.
    if (!supplied.empty()) ctx.record("noktalar", Value::points(supplied));
    if (op != Op::Replace) ctx.record("islem", Value::text(op_name(op)));
    if (!selection.empty()) {
        Value::Ints ids;
        ids.reserve(selection.keys().size());
        for (EntityKey k : selection.keys())
            ids.push_back(static_cast<std::int64_t>(core::raw(k)));
        ctx.record("nesneler", Value::ids(std::move(ids)));
    }

    if (mode == Mode::Clear) {
        ctx.echo("Seçim temizlendi (" + std::to_string(before) + " nesne bırakıldı).");
        co_return;
    }

    if (picked.empty()) {
        ctx.echo("Bu aramada nesne bulunamadı. Seçimde " + std::to_string(selection.size()) +
                 " nesne var.");
        co_return;
    }

    ctx.echo(std::to_string(picked.size()) + " nesne bulundu (" + op_name(op) + "). Seçimde " +
             std::to_string(selection.size()) + " nesne var: " + key_list(selection.keys()));
}

} // namespace

KENTOS_COMMAND(select)
{
    return CommandSpec{
        .id       = "core.select",
        .names    = {"SEÇ", "SEC", "SELECT", "S"},
        .title    = "Seç",
        .category = Category::Modify,
        .params =
            {
                Param::text("mod", Arity::optional(),
                            "TÜMÜ | TEMİZLE | NESNE | KATMAN | PENCERE | KESEN | KUTU | NOKTA | "
                            "ÇOKGEN | ÇOKGENKESEN | ÇİT | ÖNCEKİ | SON"),
                Param::points("noktalar", Arity{0, 0xFFFFFFFFu},
                              "Kutu köşeleri (iki nokta), çokgen/çit köşeleri ya da tek tıklama "
                              "noktası"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "NESNE modunda nesne kimlikleri"},
                Param::text("katman", Arity::optional(), "KATMAN modunda katman adı"),
                Param::text("islem", Arity::optional(), "DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE"),
                Param::number("tolerans", Arity::optional(),
                              "NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı"),
                Param::number("sira", Arity::optional(),
                              "NOKTA modunda kaçıncı nesne: 1 en yakını, 2 altındaki"),
            },
        // R43: a selection is not document state, so it is not undoable and not
        // journalled as a mutation. ReadOnly is how that is said to the bus — the
        // same flag core.mode, core.preference and core.zoom carry.
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        // Deliberately NOT AiAccessible. A suggestion engine that could change
        // what the engineer has highlighted could change what the next SİL
        // removes without ever emitting SİL itself (.claude/ai.md, §5.1).
        .summary = "Nesneleri seçer: tümü, kimlikle, katman, pencere, kesen kutu, çokgen, çit, "
                   "önceki seçim, son nesne ya da tek nokta.",
        .run = &run_select,
        // NOT A QUERY, THOUGH IT WRITES NOTHING. A changed highlight changes
        // what the next SİL deletes, so it is treated as an edit to the thing
        // the next edit will act on — which is also why it is closed to agents.
        .effect = Effect::Query | Effect::DocumentEdit,
    };
}

} // namespace kentos::command
