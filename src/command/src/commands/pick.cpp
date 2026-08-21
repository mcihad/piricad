// SPDX-License-Identifier: GPL-3.0-or-later
// core.select — SEÇ.
//
// Selecting is a COMMAND even though it mutates nothing, and that is not a
// formality. Constitution Article 1.2 makes the mouse one client among equals: if
// a rubber-band drag reached the selection directly, a script and the AI would
// have no way to say "the parcels inside this box", and `secimi_al()` in
// piricad.md §5.1 would have nothing to read. So the drag builds the same
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
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/pick.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

using core::EntityKey;

/// What the user asked for. Canonical Turkish names; the aliases are folded onto
/// these once, here, so no other file repeats the list.
enum class Mode : std::uint8_t { Report, All, Clear, Objects, Window, Crossing, Box, Point };

/// What to do with what was found.
enum class Op : std::uint8_t { Replace, Add, Remove, Toggle };

bool matches(const std::string& folded, std::initializer_list<const char*> names)
{
    for (const char* n : names)
        if (core::turkish_iequals(folded, n)) return true;
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
    } else if (matches(typed, {"PENCERE", "WINDOW", "W"})) {
        out = Mode::Window;
    } else if (matches(typed, {"KESEN", "CROSSING", "C"})) {
        out = Mode::Crossing;
    } else if (matches(typed, {"KUTU", "BOX", "B"})) {
        out = Mode::Box;
    } else if (matches(typed, {"NOKTA", "POINT", "P"})) {
        out = Mode::Point;
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
    case Mode::Window: return "PENCERE";
    case Mode::Crossing: return "KESEN";
    case Mode::Box: return "KUTU";
    case Mode::Point: return "NOKTA";
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
            ctx.echo("Beklenen mod: TÜMÜ | TEMİZLE | NESNE | PENCERE | KESEN | KUTU | NOKTA. "
                     "Girilen: '" +
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

    const auto need_points = [&](std::size_t n) -> bool {
        const Value v = ctx.argument("noktalar");
        if (v.as_points().size() >= n) return true;
        ctx.echo(std::string("'") + mode_name(mode) + "' " + std::to_string(n) +
                 " nokta bekliyor. Girilen: " + std::to_string(v.as_points().size()) + " nokta.");
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

    case Mode::Point: {
        if (!need_points(1)) co_return;
        const core::Point2 aim = ctx.argument("noktalar").as_points().front();

        // Pixels by default (core.secim.tolerans), metres when a client states
        // one. A script has no screen, so without `tolerans` it picks what lies
        // exactly under the point — honest, and never a silent guess.
        core::Mm radius = bus.aid_settings().pick_radius;
        if (const Value t = ctx.argument("tolerans"); !t.empty())
            radius = core::mm_from_metres(t.as_number());

        const core::EntityId hit = core::pick_nearest(doc, aim, radius);
        if (hit != core::kNoEntity) picked.push_back(doc.key_of(hit));
        break;
    }

    case Mode::Window:
    case Mode::Crossing:
    case Mode::Box: {
        if (!need_points(2)) co_return;
        const Value corners  = ctx.argument("noktalar");
        const core::Point2 a = corners.as_points()[0];
        const core::Point2 b = corners.as_points()[1];

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
    // every client's run reads the same however it aimed (piricad.md §2.2).
    ctx.record("mod", Value::text(mode_name(mode)));
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

PIRICAD_COMMAND(select)
{
    return CommandSpec{
        .id       = "core.select",
        .names    = {"SEÇ", "SEC", "SELECT", "S"},
        .category = Category::Modify,
        .params =
            {
                Param::text("mod", Arity::optional(),
                            "TÜMÜ | TEMİZLE | NESNE | PENCERE | KESEN | KUTU | NOKTA"),
                Param::points("noktalar", Arity{0, 2},
                              "Kutu köşeleri (iki nokta) veya tek tıklama noktası"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "NESNE modunda nesne kimlikleri"},
                Param::text("islem", Arity::optional(), "DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE"),
                Param::number("tolerans", Arity::optional(),
                              "NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı"),
            },
        // R43: a selection is not document state, so it is not undoable and not
        // journalled as a mutation. ReadOnly is how that is said to the bus — the
        // same flag core.mode, core.preference and core.zoom carry.
        .undo  = UndoPolicy::None,
        .flags = Flags::Scriptable | Flags::ReadOnly,
        // Deliberately NOT AiAccessible. A suggestion engine that could change
        // what the engineer has highlighted could change what the next SİL
        // removes without ever emitting SİL itself (.claude/ai.md, §5.1).
        .summary = "Nesneleri seçer: tümü, kimlikle, pencere, kesen kutu veya tek nokta.",
        .run     = &run_select,
    };
}

} // namespace piricad::command
