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
#include "kentos_cad/command/select_modes.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>
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

    /// İÇEREN — the closed objects whose face holds a point, smallest first: the
    /// parcel, then the ada it lies in, then the mahalle (`core::pick_containing`).
    /// A click INSIDE, where NOKTA's is a click ON: the parcel is found by its
    /// interior and the ada round it by `sira=2`, with no edge to aim at.
    Containing,

    /// DAİRE — what lies wholly inside a circle, a centre and a point on its rim:
    /// the trees within twenty metres of a well, the manholes round a junction.
    Circle,

    /// DIŞINDA — what lies wholly outside a box: the rest of the sheet, when the
    /// part being kept is the easy thing to draw a box round.
    Outside,

    /// GEÇEN — the lines through a point: every boundary meeting at a corner
    /// stone, where NOKTA takes one and İÇEREN takes the faces round it.
    Through,

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

/// One mode, once: its canonical name, every spelling `parse_mode` takes, and
/// what the Seçim prompt tab shows for it (`select_modes`). THE list — the
/// parser, the recorded name and the tab all read it (CLAUDE.md 5.10).
struct ModeRow
{
    Mode mode;
    const char* name;                   ///< canonical, what the record says
    std::array<const char*, 6> aliases; ///< every spelling, the name among them
    const char* label;                  ///< the tab's button; null when it has none
    int points;                         ///< clicks: 0 none, n exactly n, -1 a run
    bool adds;                          ///< the tab adds what it finds (`islem=EKLE`)
    const char* summary;                ///< the button's tip, after the line it writes
};

constexpr std::array<ModeRow, 17> kModes{{
    {Mode::Window,
     "PENCERE",
     {"PENCERE", "WINDOW", "W"},
     "Pencere",
     2,
     true,
     "tamamen içinde kalanlar; iki köşe tıklayın"},
    {Mode::Crossing,
     "KESEN",
     {"KESEN", "CROSSING", "C"},
     "Kesen",
     2,
     true,
     "kutuya değen her şey; iki köşe tıklayın"},
    {Mode::Polygon,
     "ÇOKGEN",
     {"ÇOKGENPENCERE", "COKGENPENCERE", "ÇOKGEN", "COKGEN", "WPOLYGON", "WP"},
     "Çokgen",
     -1,
     true,
     "çokgenin tamamen içindekiler; köşeleri tıklayın, Enter bitirir"},
    {Mode::PolygonCrossing,
     "ÇOKGENKESEN",
     {"ÇOKGENKESEN", "COKGENKESEN", "CPOLYGON", "CP"},
     "Çokgen Kesen",
     -1,
     true,
     "çokgenin değdiği her şey; köşeleri tıklayın, Enter bitirir"},
    {Mode::Fence,
     "ÇİT",
     {"ÇİT", "CIT", "FENCE", "F"},
     "Çit",
     -1,
     true,
     "hattın kestiği her şey; noktaları tıklayın, Enter bitirir"},
    {Mode::Circle,
     "DAİRE",
     {"DAİRE", "DAIRE", "CIRCLE"},
     "Daire",
     2,
     true,
     "dairenin tamamen içindekiler; merkezi, sonra çevreyi tıklayın"},
    {Mode::Outside,
     "DIŞINDA",
     {"DIŞINDA", "DISINDA", "OUTSIDE"},
     "Dışında",
     2,
     true,
     "kutuya hiç değmeyenler; iki köşe tıklayın"},
    {Mode::Containing,
     "İÇEREN",
     {"İÇEREN", "ICEREN", "CONTAINING"},
     "İçeren",
     1,
     true,
     "noktayı içeren en küçük alan; alanın içine tıklayın"},
    {Mode::Through,
     "GEÇEN",
     {"GEÇEN", "GECEN", "THROUGH"},
     "Geçen",
     1,
     true,
     "noktadan geçen çizgiler; noktaya tıklayın"},
    {Mode::All,
     "TÜMÜ",
     {"TÜMÜ", "TUMU", "ALL", "HEPSİ", "HEPSI"},
     "Tümü",
     0,
     true,
     "görünür bütün nesneler"},
    {Mode::Previous,
     "ÖNCEKİ",
     {"ÖNCEKİ", "ONCEKI", "PREVIOUS", "PR"},
     "Önceki",
     0,
     false,
     "bundan önceki seçim"},
    {Mode::Last, "SON", {"SON", "LAST", "L"}, "Son", 0, true, "en son çizilen nesne"},
    {Mode::Clear,
     "TEMİZLE",
     {"TEMİZLE", "TEMIZLE", "CLEAR", "NONE", "HİÇ", "HIC"},
     "Temizle",
     0,
     false,
     "seçimi boşaltır"},
    // TYPED OR POINTED AT, not started from the tab: an id, a layer name, and
    // the click and the drag the canvas already makes.
    {Mode::Objects, "NESNE", {"NESNE", "NESNELER", "OBJECT"}, nullptr, 0, false, ""},
    {Mode::Layer, "KATMAN", {"KATMAN", "LAYER", "K"}, nullptr, 0, false, ""},
    {Mode::Box, "KUTU", {"KUTU", "BOX", "B"}, nullptr, 2, false, ""},
    {Mode::Point, "NOKTA", {"NOKTA", "POINT", "P"}, nullptr, 1, false, ""},
}};

/// The mode `typed` names, folded the Turkish way. `LAST` is SON's alone: it
/// used to sit in NESNE's list too, which came first, so `SEÇ LAST` asked for
/// object ids instead of taking the newest object.
bool parse_mode(const std::string& typed, Mode& out)
{
    for (const ModeRow& row : kModes)
        for (const char* alias : row.aliases)
            if (alias != nullptr && core::turkish_key_equals(typed, alias)) {
                out = row.mode;
                return true;
            }
    return false;
}

const char* mode_name(Mode m)
{
    for (const ModeRow& row : kModes)
        if (row.mode == m) return row.name;
    return "DURUM"; ///< the implied mode: no mode given, the selection reported
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

/// An area in square metres to two decimals, divided in integers (Article 2.4).
std::string square_metres(core::Mm2 v)
{
    const bool negative     = v < 0;
    const auto abs_mm2      = static_cast<std::uint64_t>(negative ? -v : v);
    const std::uint64_t cm2 = (abs_mm2 + 5000) / 10000;
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return (negative ? "-" : "") + std::to_string(cm2 / 100) + "," + frac + " m²";
}

/// Which row of a list the caller asked for: 1 when `sira` is absent, a
/// refusal naming what the first row is when it is below one.
bool wanted_row(Context& ctx, const char* first_is, std::size_t& want)
{
    want = 1;
    if (const Value n = ctx.argument("sira"); !n.empty()) {
        const double asked = n.as_number();
        if (asked < 1.0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       std::string("'sira' 1'den küçük olamaz; 1 ") + first_is + ".");
            return false;
        }
        want = static_cast<std::size_t>(asked);
    }
    return true;
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
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Beklenen mod: TÜMÜ | TEMİZLE | NESNE | KATMAN | PENCERE | KESEN | KUTU | "
                       "NOKTA | İÇEREN | GEÇEN | DAİRE | DIŞINDA | ÇOKGEN | ÇOKGENKESEN | ÇİT | "
                       "ÖNCEKİ | SON. Girilen: '" +
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
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Beklenen işlem: DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE. Girilen: '" +
                           v.as_text() + "'");
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

    if (mode == Mode::Window || mode == Mode::Crossing || mode == Mode::Box ||
        mode == Mode::Outside) {
        if (supplied.empty()) {
            auto first = co_await ctx.point("noktalar", mode == Mode::Outside
                                                            ? "Dışı seçilecek kutunun ilk köşesi"
                                                            : "Seçim kutusunun ilk köşesi");
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
            ctx.refuse(core::ErrorCode::InvalidArgument, closed
                                                             ? "Seçim çokgeni en az üç köşe ister."
                                                             : "Çit en az iki nokta ister.");
            co_return;
        }
    } else if (mode == Mode::Point && supplied.empty()) {
        auto aim = co_await ctx.point("noktalar", "Seçilecek nesnenin üzerinde bir nokta");
        if (!aim) co_return;
        supplied.push_back(*aim);
    } else if (mode == Mode::Circle) {
        if (supplied.empty()) {
            auto centre = co_await ctx.point("noktalar", "Seçim dairesinin merkezi");
            if (!centre) co_return;
            supplied.push_back(*centre);
        }
        if (supplied.size() < 2) {
            auto rim = co_await ctx.point("noktalar", "Dairenin çevresi üzerinde bir nokta",
                                          PointOptions{.rubber_band   = true,
                                                       .rubber_origin = supplied.front(),
                                                       .rubber_shape  = RubberShape::Circle});
            if (!rim) co_return;
            supplied.push_back(*rim);
        }
    } else if (mode == Mode::Through && supplied.empty()) {
        // SNAPPED, unlike the `İÇEREN` click: "through this corner" is said by
        // landing on the corner, and the end-point snap is how a hand lands.
        auto aim = co_await ctx.point("noktalar", "Nesnelerin geçtiği nokta");
        if (!aim) co_return;
        supplied.push_back(*aim);
    } else if (mode == Mode::Containing && supplied.empty()) {
        // NOT SNAPPED, for SINIR's reason (`Prompt::aids`): a click inside a
        // parcel near its edge would land ON the edge, and on the edge the
        // point is in both parcels or in neither.
        auto aim = co_await ctx.point("noktalar", "Seçilecek alanın içine tıklayın",
                                      PointOptions{.aids = false});
        if (!aim) co_return;
        supplied.push_back(*aim);
    }

    const auto need_points = [&](std::size_t n) -> bool {
        if (supplied.size() >= n) return true;
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string("'") + mode_name(mode) + "' " + std::to_string(n) +
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
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "'NESNE' en az bir nesne kimliği bekliyor. Örnek: SEÇ NESNE nesneler=1");
            co_return;
        }
        for (std::int64_t raw : v.as_ids()) {
            if (raw <= 0) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Geçersiz nesne kimliği: " + std::to_string(raw) +
                               ". Kimlikler 1'den başlar.");
                co_return;
            }
            const auto key = static_cast<EntityKey>(static_cast<std::uint64_t>(raw));
            if (doc.slot_of(key) == core::kNoEntity) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
                co_return;
            }
            picked.push_back(key);
        }
        break;
    }

    case Mode::Layer: {
        const Value v = ctx.argument("katman");
        if (v.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "'KATMAN' bir katman adı bekliyor. Örnek: SEÇ mod=KATMAN katman=\"PARSEL\"");
            co_return;
        }

        const std::string wanted       = v.as_text();
        const core::LayerId layer_slot = doc.find_layer(wanted);
        if (layer_slot == core::kNoLayer) {
            ctx.refuse(core::ErrorCode::NotFound, "Katman bulunamadı: " + wanted);
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
        if (!wanted_row(ctx, "en yakın nesnedir", want)) co_return;

        if (want == 1) {
            const core::EntityId hit = core::pick_nearest(doc, aim, radius);
            if (hit != core::kNoEntity) picked.push_back(doc.key_of(hit));
            break;
        }

        std::vector<core::EntityId> under;
        core::pick_all(doc, aim, radius, under);
        if (want > under.size()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "O noktada " + std::to_string(under.size()) + " nesne var; " +
                           std::to_string(want) + ". istendi.");
            co_return;
        }
        picked.push_back(doc.key_of(under[want - 1]));
        break;
    }

    case Mode::Containing: {
        if (!need_points(1)) co_return;
        std::size_t want = 1;
        if (!wanted_row(ctx, "en küçük alandır", want)) co_return;

        std::vector<core::EntityId> holding;
        core::pick_containing(doc, supplied.front(), holding);
        if (holding.empty()) break;

        // THE LIST, SMALLEST FIRST, so the row `sira=2` names is read off the
        // transcript rather than guessed: the parcel, the ada, the mahalle.
        std::string rows;
        for (std::size_t i = 0; i < holding.size() && i < 12; ++i)
            rows += (i ? "; " : "") + std::to_string(i + 1) + ". nesne " +
                    std::to_string(core::raw(doc.key_of(holding[i]))) + " (" +
                    square_metres(doc.entity_area(holding[i])) + ")";
        if (holding.size() > 12) rows += "; …";
        ctx.echo("Noktayı içeren " + std::to_string(holding.size()) +
                 " kapalı nesne, küçükten büyüğe: " + rows);

        if (want > holding.size()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "O noktayı " + std::to_string(holding.size()) + " kapalı nesne içeriyor; " +
                           std::to_string(want) + ". istendi.");
            co_return;
        }
        picked.push_back(doc.key_of(holding[want - 1]));
        break;
    }

    case Mode::Circle: {
        if (!need_points(2)) co_return;
        const core::Mm radius = core::segment_length(supplied[0], supplied[1]);
        if (radius <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Seçim dairesinin yarıçapı sıfır: çevre noktası merkezle aynı.");
            co_return;
        }
        core::pick_in_circle(doc, supplied[0], radius, slots);
        picked = keys_of(doc, slots);
        break;
    }

    case Mode::Outside: {
        if (!need_points(2)) co_return;
        core::Box2 box{};
        box.extend(supplied[0]);
        box.extend(supplied[1]);
        core::pick_outside_box(doc, box, slots);
        picked = keys_of(doc, slots);
        break;
    }

    case Mode::Through: {
        if (!need_points(1)) co_return;
        // THE PICK RADIUS, as NOKTA's: pixels by default, metres when stated,
        // and exactly the point for a client with no screen.
        core::Mm radius = bus.aid_settings().pick_radius;
        if (const Value t = ctx.argument("tolerans"); !t.empty())
            radius = core::mm_from_metres(t.as_number());
        core::pick_through(doc, supplied.front(), radius, slots);
        picked = keys_of(doc, slots);
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

    // ---- the kind filter, across EVERY mode ----
    //
    // `tur=` narrows whatever the gesture picked to one kind: a window over a
    // sheet catches the parcels, the captions, the dimensions and the road
    // centreline together, and "the areas in that window" is a thing a surveyor
    // asks constantly. `katman=` is a MODE because a layer names a set on its
    // own; a kind names no set, it narrows one — so this is a filter and it
    // composes with all of them, `ÇİT` and `ÖNCEKİ` included.
    //
    // THE WORD IS THE KIND TABLE'S OWN. `find_name` folds Turkish and accepts
    // every alias a kind declares (`ÇOKLUÇİZGİ`, `cokluçizgi`, `POLYLINE`), so
    // there is no second list of kinds here and a kind a plugin registers is
    // selectable the day it is added (CLAUDE.md 5.10).
    if (const Value v = ctx.argument("tur"); !v.empty()) {
        const std::string wanted        = v.as_text();
        const core::KindSpec* kind_spec = core::builtin_kinds().find_name(wanted);
        if (kind_spec == nullptr) {
            std::string known;
            for (const core::KindSpec& one : core::builtin_kinds().all())
                if (one.names[0] != nullptr)
                    known += (known.empty() ? "" : ", ") + std::string(one.names[0]);
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound,
                          "Tanınmayan nesne türü: '" + wanted + "'. Türler: " + known + "."));
            co_return;
        }

        const core::EntityTable& entities = doc.entities();
        std::vector<EntityKey> kept;
        kept.reserve(picked.size());
        for (const EntityKey k : picked) {
            const core::EntityId slot = doc.slot_of(k);
            if (slot == core::kNoEntity || !doc.alive(slot)) continue;
            if (entities.kind[slot] == kind_spec->id) kept.push_back(k);
        }
        picked = std::move(kept);
    }

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
    // THE KIND'S OWN PRIMARY NAME, not the alias that was typed: `POLYLINE` and
    // `ÇOKLUÇİZGİ` name one kind and the record says which kind, once.
    if (const Value v = ctx.argument("tur"); !v.empty())
        if (const core::KindSpec* k = core::builtin_kinds().find_name(v.as_text()); k != nullptr)
            ctx.record("tur", Value::text(k->names[0]));
    if (mode == Mode::Point || mode == Mode::Containing)
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

std::span<const SelectModeInfo> select_modes()
{
    // BUILT ONCE FROM THE TABLE: the rows a hand starts from the tab, in order.
    static const std::vector<SelectModeInfo> offered = [] {
        std::vector<SelectModeInfo> out;
        for (const ModeRow& row : kModes)
            if (row.label != nullptr)
                out.push_back(
                    SelectModeInfo{row.name, row.label, row.points, row.adds, row.summary});
        return out;
    }();
    return offered;
}

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
                            "İÇEREN | GEÇEN | DAİRE | DIŞINDA | ÇOKGEN | ÇOKGENKESEN | ÇİT | "
                            "ÖNCEKİ | SON")
                    .en("mode"),
                Param::points("noktalar", Arity{0, 0xFFFFFFFFu},
                              "Kutu köşeleri (iki nokta; DIŞINDA da), çokgen/çit köşeleri, "
                              "DAİRE'de merkez ve çevre noktası ya da tek tıklama noktası (NOKTA, "
                              "İÇEREN, GEÇEN)")
                    .en("points"),
                Param::text("tur", Arity::optional(),
                            "Yalnız bu türdeki nesneler: ÇOKLUÇİZGİ, DAİRE, YAY, NOKTA, ELİPS…")
                    .en("type"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "NESNE modunda nesne kimlikleri"}
                    .en("objects"),
                Param::text("katman", Arity::optional(), "KATMAN modunda katman adı").en("layer"),
                Param::text("islem", Arity::optional(), "DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE")
                    .en("action"),
                Param::number("tolerans", Arity::optional(),
                              "NOKTA ve GEÇEN modlarında arama yarıçapı, metre; yoksa seçim "
                              "toleransı")
                    .en("tolerance"),
                Param::number("sira", Arity::optional(),
                              "Kaçıncı nesne: NOKTA'da 1 en yakını, 2 altındaki; İÇEREN'de 1 "
                              "en küçük alan, 2 onu içeren")
                    .en("order"),
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
                   "daire, kutunun dışı, önceki seçim, son nesne, tek nokta, bir noktayı içeren "
                   "alan ya da noktadan geçen çizgiler.",
        .run = &run_select,
        // NOT A QUERY, THOUGH IT WRITES NOTHING. A changed highlight changes
        // what the next SİL deletes, so it is treated as an edit to the thing
        // the next edit will act on — which is also why it is closed to agents.
        .effect = Effect::Query | Effect::DocumentEdit,
    };
}

} // namespace kentos::command
