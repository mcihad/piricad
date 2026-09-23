// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the support matrix, MEASURED (TODOS F-01).
//
// WHY A PROGRAM AND NOT A TABLE. "Does TRIM work on an arc" has two answers in
// any codebase: the one a person believes and the one the code gives. A
// hand-written support table records the first and drifts from the second the
// day either changes. This builds one fresh document per cell, creates the
// entity through the same commands a user types, runs the edit through the same
// bus every client uses, and CLASSIFIES WHAT CAME OUT — so every cell of
// `docs/nesneler/destek-matrisi.md` is a measurement with its evidence beside it,
// and `scripts/ci-gate-kapsam.sh` regenerates and diffs it, which makes a
// behaviour change that flips a cell show up in the same commit as the change.
//
// THE FOUR VERDICTS, and what separates them is the whole value of the table:
//
//   `destekli`    the command ran and the result is what a CAD user expects of
//                 that kind — an arc trimmed is still an arc, a circle offset is
//                 a circle of the new radius, a move moved it exactly
//   `kısmi`       the command ran and the result is WRONG IN KIND: a curve came
//                 back as chords, an open line came back as a closed band, a
//                 measurement is off the analytic value
//   `yok`         the command refused; the refusal sentence is the evidence
//   `uygulanamaz` the operation has no meaning for the kind — extending a closed
//                 circle, filleting a lone point. DECLARED here with its reason,
//                 never inferred from a refusal, because "refused" and
//                 "meaningless" are the two answers a table must not confuse
//
// WHAT IS DELIBERATELY LEFT OUT. File interchange (DXF, DWG, GeoPackage) is its
// own item, I-01, with its own matrix: its answers depend on which format
// libraries a build has, and a table that differed between two correct builds
// could not be diffed by a gate. The one round trip measured here is the
// project file, which every build writes and reads.
//
// It is an executable at the top of the dependency graph (Article 3.2a): it links
// io, processing, the domains and ai so its registry is the application's, and it
// contains no Qt.
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace kentos;
using command::Args;
using command::Value;
using core::EntityKey;
using core::KindId;
using core::Mm;
using core::Point2;

// =============================================================================
// the rig: one fresh document per cell
// =============================================================================

/// A bus with every command the application registers and a file engine
/// attached — which is what a running program is, minus the window.
struct Rig
{
    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};

    Rig()
    {
        command::register_builtin_commands(reg);
        processing::register_processing_commands(reg);
        domain::geodesy::register_geodesy_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        domain::surface::register_surface_commands(reg);
        ai::register_ai_commands(reg);
        bus.on_echo = [](std::string_view) {};
    }
};

/// A command line, as a user types it. Coordinates are metres.
core::Status line(Rig& r, const std::string& text)
{
    auto result = r.bus.execute_line(text, command::Origin::Script);
    if (!result) return result.error();
    return core::ok();
}

/// A built invocation, which is how the edits are run: the arguments are
/// millimetre points and entity keys, and a JSON script sends exactly this.
///
/// THE WHOLE RESULT, not a status: whether the document changed and what the
/// command SAID are both evidence, and the second one is how a refusal that was
/// never returned as an error gets caught (see `edit_failed`).
core::Result<command::DispatchResult> invoke(Rig& r, const char* id, Args args)
{
    command::Invocation inv;
    inv.name   = id;
    inv.args   = std::move(args);
    inv.origin = command::Origin::Script;
    return r.bus.dispatch(inv);
}

Value ids(std::initializer_list<EntityKey> keys)
{
    Value::Ints out;
    for (const EntityKey k : keys)
        out.push_back(static_cast<std::int64_t>(core::raw(k)));
    return Value::ids(std::move(out));
}

/// Every drawn, standalone entity — not the members hidden inside a block
/// definition, which are not something a user edits directly.
std::vector<EntityKey> live_keys(const Rig& r)
{
    std::vector<EntityKey> out;
    const core::EntityTable& t = r.doc.entities();
    for (core::EntityId e = 0; e < t.size(); ++e)
        if (t.standalone(e)) out.push_back(t.key[e]);
    return out;
}

bool contains(const std::vector<EntityKey>& list, EntityKey k)
{
    return std::find(list.begin(), list.end(), k) != list.end();
}

/// The keys alive now that were not alive `before`, plus `keep` if it survived.
std::vector<EntityKey> results(const Rig& r, const std::vector<EntityKey>& before, EntityKey keep,
                               const std::vector<EntityKey>& scaffold = {})
{
    std::vector<EntityKey> out;
    for (const EntityKey k : live_keys(r)) {
        if (contains(scaffold, k)) continue;
        if (k == keep || !contains(before, k)) out.push_back(k);
    }
    return out;
}

// =============================================================================
// what an entity is, measured
// =============================================================================

/// The runs an entity is drawn with, and whether each closes on itself.
struct Outline
{
    std::vector<std::vector<Point2>> runs;
    std::vector<bool> closed;

    bool empty() const { return runs.empty(); }

    /// Every vertex of every run, in order.
    std::vector<Point2> points() const
    {
        std::vector<Point2> all;
        for (const auto& run : runs)
            all.insert(all.end(), run.begin(), run.end());
        return all;
    }

    bool all_closed() const
    {
        return !closed.empty() &&
               std::all_of(closed.begin(), closed.end(), [](bool c) { return c; });
    }
};

Outline outline_of(const Rig& r, core::EntityId e)
{
    Outline out;
    const core::EntityTable& t  = r.doc.entities();
    const core::RingGeometry& g = r.doc.geometry();
    const KindId kind           = t.kind[e];
    const std::uint32_t slot    = t.slot[e];

    core::EmitBuffer buffer;
    if (core::curve_outline(kind, g, slot, buffer)) {
        for (std::size_t i = 0; i < buffer.run_start.size(); ++i) {
            if (i < buffer.run_hole.size() && buffer.run_hole[i] != 0) continue;
            std::vector<Point2> run;
            for (std::uint32_t k = 0; k < buffer.run_count[i]; ++k) {
                const std::uint32_t at = buffer.run_start[i] + k;
                run.push_back(Point2{buffer.xs[at], buffer.ys[at]});
            }
            out.runs.push_back(std::move(run));
            out.closed.push_back(buffer.run_closed[i] != 0);
        }
        return out;
    }

    const core::RingSpan span = g.rings_of(slot);
    for (std::uint32_t ring = span.first; ring < span.first + span.count; ++ring) {
        if (g.ring_role[ring] == core::RingRole::Interior) continue;
        std::vector<Point2> run;
        for (std::uint32_t k = 0; k < g.ring_count[ring]; ++k)
            run.push_back(g.vertex(ring, k));
        out.runs.push_back(std::move(run));
        out.closed.push_back(g.ring_role[ring] != core::RingRole::Open);
    }
    return out;
}

/// The facts a cell is decided on.
struct Probe
{
    bool alive{false};
    KindId kind{core::kNoKind};
    core::Box2 box{};
    Mm perimeter{0};
    double area_m2{0.0};
    bool closed{false};
    /// Interior rings: what a face's holes are, and what an offset must keep.
    std::size_t holes{0};
    Outline outline;
};

Probe probe(const Rig& r, EntityKey key)
{
    Probe p;
    const core::EntityId e = r.doc.slot_of(key);
    if (e == core::kNoEntity || !r.doc.alive(e)) return p;
    p.alive     = true;
    p.kind      = r.doc.entities().kind[e];
    p.box       = r.doc.entity_extent(e);
    p.perimeter = r.doc.entity_perimeter(e);
    p.area_m2   = static_cast<double>(r.doc.entity_area(e)) / 1.0e6;
    p.outline   = outline_of(r, e);
    p.closed    = p.outline.all_closed();

    const core::RingGeometry& g = r.doc.geometry();
    const core::RingSpan span   = g.rings_of(r.doc.entities().slot[e]);
    for (std::uint32_t ring = span.first; ring < span.first + span.count; ++ring)
        if (g.ring_role[ring] == core::RingRole::Interior) ++p.holes;
    return p;
}

std::string kind_name(KindId id)
{
    if (const core::KindSpec* spec = core::builtin_kinds().find(id); spec != nullptr) {
        if (spec->names[0] != nullptr) return spec->names[0];
        return spec->stable_id;
    }
    return "bilinmeyen tür";
}

/// Metres to three decimals, which is a millimetre — the document's own grain.
std::string metres(Mm v)
{
    char buffer[64];
    (void)std::snprintf(buffer, sizeof buffer, "%.3f m", static_cast<double>(v) / 1000.0);
    return buffer;
}

std::string square_metres(double v)
{
    char buffer[64];
    (void)std::snprintf(buffer, sizeof buffer, "%.3f m²", v);
    return buffer;
}

/// A refusal sentence, trimmed to its first sentence so the table stays a
/// table. The full sentence is what the user sees; the table needs its gist.
std::string g_data_root; ///< where `tests/support/kapsam` lives

std::string gist(std::string message)
{
    // NO MACHINE'S PATH IN THE TABLE. A refusal that names a file names it
    // absolutely, and a table carrying one checkout's path would differ on every
    // other machine — which is a freshness gate that fails for no reason.
    if (!g_data_root.empty())
        for (auto at = message.find(g_data_root); at != std::string::npos;
             at      = message.find(g_data_root))
            message.replace(at, g_data_root.size(), "<depo>");

    for (const char* stop : {". ", ".\n"}) {
        if (const auto at = message.find(stop); at != std::string::npos) message.resize(at + 1);
    }
    for (char& c : message)
        if (c == '|' || c == '\n') c = ' ';
    if (message.size() > 150) message = message.substr(0, 147) + "…";
    return message;
}

bool near(Mm a, Mm b, Mm tolerance)
{
    return std::llabs(a - b) <= tolerance;
}

/// A tolerance for a quantity after a transform rounded every vertex to the
/// millimetre: a millimetre per vertex, or a thousandth of the value.
Mm slack(Mm value, std::size_t vertices)
{
    const Mm by_rounding = static_cast<Mm>(vertices) + 2;
    const Mm by_ratio    = static_cast<Mm>(std::llabs(value) / 1000);
    return std::max(by_rounding, by_ratio);
}

// =============================================================================
// the rows: one fixture per kind, made through the commands a user types
// =============================================================================

/// The shape of a fixture, which decides where an operation even applies.
enum class Shape {
    Open,      ///< a curve with two ends: a line, an arc, an open spline
    Closed,    ///< a curve with no ends: a circle, a full ellipse, a face
    Annotation ///< not a curve at all: a point, a text, a block, a dimension
};

struct Fixture
{
    const char* id;    ///< stable, for the anchor
    const char* title; ///< the row heading, Turkish
    Shape shape;
    bool has_corner; ///< whether ONE entity of it has a corner to fillet

    /// The analytic length, when there is one — what `ölç` is checked against.
    std::optional<double> length_m;
    /// The analytic area, when the kind encloses one.
    std::optional<double> area_m2;

    /// Builds it in `r` and answers its key.
    std::function<core::Result<EntityKey>(Rig&)> make;
};

/// Runs `lines`, then answers the entity they added — the one of kind `want`
/// when they added several.
///
/// SEVERAL IS A REAL CASE, not a harness nuisance: `LİDER` writes its caption as
/// a separate text entity beside the leader itself, so a fixture that insisted on
/// one new entity refused the leader row outright.
core::Result<EntityKey> build(Rig& r, std::initializer_list<std::string> lines,
                              KindId want = core::kNoKind)
{
    const std::vector<EntityKey> before = live_keys(r);
    for (const std::string& text : lines)
        if (auto st = line(r, text); !st) return st.error();
    std::vector<EntityKey> made;
    for (const EntityKey k : live_keys(r)) {
        if (contains(before, k)) continue;
        if (want != core::kNoKind && r.doc.entities().kind[r.doc.slot_of(k)] != want) continue;
        made.push_back(k);
    }
    if (made.size() != 1)
        return core::err(core::ErrorCode::InvalidArgument,
                         "fikstür bir nesne yerine " + std::to_string(made.size()) + " üretti");
    return made.front();
}

std::vector<Fixture> fixtures()
{
    constexpr double pi = std::numbers::pi;

    // Ramanujan's second approximation: relative error below 1e-10 at this
    // eccentricity, which is far inside the millimetre the table reports.
    const auto ellipse_perimeter = [](double a, double b) {
        const double h = ((a - b) * (a - b)) / ((a + b) * (a + b));
        return pi * (a + b) * (1.0 + 3.0 * h / (10.0 + std::sqrt(4.0 - 3.0 * h)));
    };

    return {
        {"cizgi", "Çizgi", Shape::Open, false, 50.0, std::nullopt,
         [](Rig& r) { return build(r, {"ÇİZGİ 0,0 50,0"}); }},

        {"coklucizgi", "Köşeli çoklu çizgi", Shape::Open, true, 90.0, std::nullopt,
         [](Rig& r) { return build(r, {"ÇOKLUÇİZGİ 0,0 50,0 50,40"}); }},

        {"yayli", "Yaylı çoklu çizgi (DXF şişkinliği)", Shape::Open, true, pi * 25.0 + 30.0,
         std::nullopt,
         [](Rig& r) {
             // THE ONLY ROAD THIS KIND HAS, which is itself a finding: no command
             // makes an arc polyline; it arrives from a DXF bulge and nowhere
             // else. So its row is built the way a user gets one.
             const std::string path = g_data_root + "/tests/support/kapsam/yayli-cizgi.dxf";
             return build(r, {"İÇEAKTAR dosya=\"" + path + "\""});
         }},

        {"alan", "Delikli alan", Shape::Closed, true, 360.0 + 80.0, 8000.0 - 400.0,
         [](Rig& r) -> core::Result<EntityKey> {
             const std::vector<EntityKey> before = live_keys(r);
             Args a;
             a.set("noktalar", Value::points({{0, 0},
                                              {100000, 0},
                                              {100000, 80000},
                                              {0, 80000},
                                              {40000, 30000},
                                              {60000, 30000},
                                              {60000, 50000},
                                              {40000, 50000}}));
             a.set("bolum", Value::ids({4, 4}));
             if (auto ran = invoke(r, "core.area", std::move(a)); !ran) return ran.error();
             for (const EntityKey k : live_keys(r))
                 if (!contains(before, k)) return k;
             return core::err(core::ErrorCode::InvalidArgument, "alan oluşmadı");
         }},

        {"cokparca", "Çok parçalı alan", Shape::Closed, true, 2.0 * (40.0 + 40.0) * 2.0,
         1600.0 + 1600.0,
         [](Rig& r) -> core::Result<EntityKey> {
             // TWO DISJOINT FACES MADE ONE by BİRLEŞTİR, which is the only road to
             // a multipart face: `ALAN`'s ring list reads its first ring as the
             // exterior and every later one as a hole.
             for (const char* at : {"ALAN 0,0 40,0 40,40 0,40", "ALAN 60,0 100,0 100,40 60,40"})
                 if (auto st = line(r, at); !st) return st.error();
             const std::vector<EntityKey> faces = live_keys(r);
             if (faces.size() != 2)
                 return core::err(core::ErrorCode::InvalidArgument, "iki alan oluşmadı");
             Args a;
             a.set("nesneler", ids({faces[0], faces[1]}));
             if (auto ran = invoke(r, "core.combine", std::move(a)); !ran) return ran.error();
             const std::vector<EntityKey> left = live_keys(r);
             if (left.size() != 1)
                 return core::err(core::ErrorCode::InvalidArgument,
                                  "BİRLEŞTİR iki alanı tek nesne yapmadı: " +
                                      std::to_string(left.size()) + " kaldı");
             return left.front();
         }},

        {"daire", "Daire", Shape::Closed, false, 2.0 * pi * 10.0, pi * 100.0,
         [](Rig& r) { return build(r, {"DAİRE 0,0 10,0"}); }},

        {"yay", "Yay", Shape::Open, false, pi * 50.0 / 2.0, std::nullopt,
         [](Rig& r) { return build(r, {"YAY 0,0 50,0 0,50"}); }},

        {"elips", "Elips", Shape::Closed, false, ellipse_perimeter(50.0, 20.0), pi * 50.0 * 20.0,
         [](Rig& r) { return build(r, {"ELİPS 0,0 50,0 0,20"}); }},

        {"spline", "Spline", Shape::Open, false, std::nullopt, std::nullopt,
         [](Rig& r) { return build(r, {"SPLINE noktalar=0,0 10,20 20,20 30,0 derece=3"}); }},

        {"tarama", "Tarama", Shape::Closed, true, 100.0, 600.0,
         [](Rig& r) {
             return build(r, {"TARAMA noktalar=0,0 30,0 30,20 0,20 desen=ANSI31 olcek=1000"});
         }},

        {"nokta", "Nokta", Shape::Annotation, false, std::nullopt, std::nullopt,
         [](Rig& r) { return build(r, {"NOKTA 10,10"}); }},

        {"yazi", "Yazı", Shape::Annotation, false, std::nullopt, std::nullopt,
         [](Rig& r) { return build(r, {"METİN noktalar=10,10 yazi=Deneme yukseklik=2500"}); }},

        {"blok", "Blok referansı", Shape::Annotation, false, std::nullopt, std::nullopt,
         [](Rig& r) -> core::Result<EntityKey> {
             auto member = build(r, {"ÇİZGİ 0,0 10,0"});
             if (!member) return member.error();
             if (auto st = line(r, "BLOK ad=KAPSAM taban=0,0 nesneler=" +
                                       std::to_string(core::raw(member.value())));
                 !st)
                 return st.error();
             return build(r, {"BLOKEKLE ad=KAPSAM nokta=20,20"});
         }},

        {"olcu", "Ölçü", Shape::Annotation, false, std::nullopt, std::nullopt,
         [](Rig& r) { return build(r, {"ÖLÇÜ tur=hizali birinci=0,0 ikinci=30,0 konum=15,5"}); }},

        {"lider", "Lider", Shape::Open, false, std::nullopt, std::nullopt,
         [](Rig& r) {
             return build(r, {"LİDER noktalar=0,0 5,5 10,5 metin=Not"}, core::kLeaderKind);
         }},
    };
}

// =============================================================================
// the columns: one operation each, with the check that decides its verdict
// =============================================================================

/// The four verdicts, and a fifth state that is not a verdict at all: the row's
/// fixture could not be BUILT, so nothing about the operations was measured.
/// Kept apart from `None` because "this kind cannot even be made through a
/// command" is one finding about the row, not seventeen about its cells.
enum class Verdict { Supported, Partial, None, NotApplicable, Unbuilt };

struct Cell
{
    Verdict verdict{Verdict::None};
    std::string evidence;
};

/// The prefix a silent refusal's evidence carries, so it can be counted and
/// found: the refusal a person sees and an automated client does not.
constexpr const char* kSilent = "SESSİZ RET —";

Cell supported(std::string why)
{
    return {Verdict::Supported, std::move(why)};
}

Cell partial(std::string why)
{
    return {Verdict::Partial, std::move(why)};
}

Cell refused(const core::Error& error)
{
    return {Verdict::None, gist(error.message)};
}

Cell refused(std::string why)
{
    return {Verdict::None, std::move(why)};
}

Cell meaningless(std::string why)
{
    return {Verdict::NotApplicable, std::move(why)};
}

/// Whether an edit failed, and how.
///
/// TWO WAYS TO FAIL, and the second is a finding in its own right. A command can
/// refuse by returning an error, which every client sees. Or it can write the
/// refusal to the transcript and end its body — and then the BUS REPORTS
/// SUCCESS: a person at the command line reads the sentence, but a script, an
/// agent, MCP and Python are told the edit happened. An edit that returned
/// success and changed nothing is exactly that, so it is recorded as a refusal
/// with the sentence it hid, marked so the matrix can count them.
std::optional<Cell> edit_failed(const core::Result<command::DispatchResult>& ran)
{
    if (!ran) return refused(ran.error());
    if (ran.value().mutated) return std::nullopt;

    std::string said;
    for (const std::string& l : ran.value().lines) {
        if (!said.empty()) said += " ";
        said += l;
    }
    if (said.empty()) said = "komut hiçbir şey yazmadı";
    return Cell{Verdict::None, std::string(kSilent) + " " + gist(said)};
}

struct Operation
{
    const char* id;
    const char* title;
    const char* command; ///< the command id the operation runs, for the surface table
    std::function<Cell(Rig&, const Fixture&, EntityKey)> run;
};

Point2 centre_of(const core::Box2& b)
{
    return Point2{(b.min_x + b.max_x) / 2, (b.min_y + b.max_y) / 2};
}

/// The family a kind belongs to for the question "did it keep its kind": a
/// trimmed circle becomes an arc and that is right; a trimmed arc that comes
/// back as a polyline is chords, and that is not.
int family(KindId k)
{
    if (k == core::kCircleKind || k == core::kArcKind) return 1;
    if (k == core::kPolylineKind) return 2;
    if (k == core::kArcPolylineKind) return 3;
    return 100 + static_cast<int>(k);
}

bool is_polyline(KindId k)
{
    return k == core::kPolylineKind;
}

bool true_curve(KindId k)
{
    return k == core::kCircleKind || k == core::kArcKind || k == core::kEllipseKind ||
           k == core::kSplineKind || k == core::kArcPolylineKind;
}

/// The verdict for an edit that must leave a curve a curve.
Cell curve_kept(const Probe& before, const std::vector<Probe>& after, const std::string& done)
{
    if (after.empty()) return refused("komut başarılı döndü ama nesne kalmadı");
    for (const Probe& p : after) {
        if (true_curve(before.kind) && is_polyline(p.kind))
            return partial(done + "; " + kind_name(before.kind) + " kirişlere (" +
                           kind_name(p.kind) + ") dönüştü");
    }
    std::string kinds;
    for (const Probe& p : after) {
        if (!kinds.empty()) kinds += ", ";
        kinds += kind_name(p.kind);
    }
    return supported(done + "; sonuç: " + kinds);
}

// ---- the point on the entity an operation aims at ---------------------------

/// The outline vertex nearest the fraction `t` of the outline's length.
Point2 along(const Outline& o, double t)
{
    const std::vector<Point2> pts = o.points();
    if (pts.empty()) return {};
    double total = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i)
        total += static_cast<double>(core::segment_length(pts[i - 1], pts[i]));
    double want = total * t;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double seg = static_cast<double>(core::segment_length(pts[i - 1], pts[i]));
        if (want <= seg) {
            const double f = seg > 0.0 ? want / seg : 0.0;
            return Point2{pts[i - 1].x + static_cast<Mm>(std::llround(
                                             f * static_cast<double>(pts[i].x - pts[i - 1].x))),
                          pts[i - 1].y + static_cast<Mm>(std::llround(
                                             f * static_cast<double>(pts[i].y - pts[i - 1].y)))};
        }
        want -= seg;
    }
    return pts.back();
}

/// The two ends of an open curve and the direction it leaves the last one.
struct End
{
    Point2 at{};
    double dx{1.0};
    double dy{0.0};
};

End last_end(const Outline& o)
{
    End end;
    if (o.runs.empty() || o.runs.back().size() < 2) return end;
    const std::vector<Point2>& run = o.runs.back();
    const Point2 a                 = run[run.size() - 2];
    const Point2 b                 = run.back();
    const double len = std::hypot(static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y));
    end.at           = b;
    if (len > 0.0) {
        end.dx = static_cast<double>(b.x - a.x) / len;
        end.dy = static_cast<double>(b.y - a.y) / len;
    }
    return end;
}

Point2 offset_point(Point2 p, double dx, double dy, double by)
{
    return Point2{p.x + static_cast<Mm>(std::llround(dx * by)),
                  p.y + static_cast<Mm>(std::llround(dy * by))};
}

/// A straight line through `through`, square to (dx, dy), `half` either side.
core::Result<EntityKey> cross_line(Rig& r, Point2 through, double dx, double dy, double half)
{
    const std::vector<EntityKey> before = live_keys(r);
    Args a;
    a.set("noktalar", Value::points({offset_point(through, -dy, dx, half),
                                     offset_point(through, dy, -dx, half)}));
    if (auto ran = invoke(r, "core.line", std::move(a)); !ran) return ran.error();
    for (const EntityKey k : live_keys(r))
        if (!contains(before, k)) return k;
    return core::err(core::ErrorCode::InvalidArgument, "sınır çizgisi oluşmadı");
}

// ---- the operations ---------------------------------------------------------

Cell op_select(Rig& r, const Fixture&, EntityKey key)
{
    const Probe before = probe(r, key);
    Args a;
    a.set("mod", Value::text("PENCERE"));
    a.set("noktalar", Value::points({Point2{before.box.min_x - 1000, before.box.min_y - 1000},
                                     Point2{before.box.max_x + 1000, before.box.max_y + 1000}}));
    if (auto ran = invoke(r, "core.select", std::move(a)); !ran) return refused(ran.error());
    if (r.bus.selection().contains(key)) return supported("pencere seçimi nesneyi aldı");
    return refused("pencere seçimi nesneyi almadı");
}

Cell op_snap(Rig& r, const Fixture&, EntityKey key)
{
    const Probe p          = probe(r, key);
    const core::EntityId e = r.doc.slot_of(key);
    // WHERE A USER WOULD AIM: every vertex, every grip, the middle of every
    // segment and the middle of the box. Without the segment middles a
    // two-segment polyline "had no midpoint snap", which was a gap in the aiming
    // and not in the snap engine.
    std::vector<Point2> aims = p.outline.points();
    for (const auto& run : p.outline.runs)
        for (std::size_t i = 1; i < run.size(); ++i)
            aims.push_back(Point2{(run[i - 1].x + run[i].x) / 2, (run[i - 1].y + run[i].y) / 2});
    for (const core::GripPoint& g : core::entity_grips(r.doc, e))
        aims.push_back(g.at);
    aims.push_back(centre_of(p.box));

    std::vector<std::string> fired;
    for (std::uint32_t bit = 1; bit != 0 && bit <= core::SnapObjectMask; bit <<= 1U) {
        if ((core::SnapObjectMask & bit) == 0) continue;
        bool hit = false;
        for (const Point2 aim : aims) {
            core::SnapQuery q;
            q.aim                    = aim;
            q.radius                 = 800;
            q.modes                  = bit;
            const core::SnapResult s = core::snap(r.doc, q);
            if (s.mode == bit && s.entity == e) {
                hit = true;
                break;
            }
        }
        if (hit) fired.emplace_back(core::snap_mode_label(bit));
    }
    if (fired.empty()) return refused("hiçbir yakalama kipi bu nesnede nokta vermedi");
    std::string list;
    for (const std::string& f : fired)
        list += (list.empty() ? "" : ", ") + f;
    return supported(list);
}

Cell op_grip(Rig& r, const Fixture&, EntityKey key)
{
    const Probe before                       = probe(r, key);
    const std::vector<core::GripPoint> grips = core::entity_grips(r.doc, r.doc.slot_of(key));
    if (grips.empty()) return refused("nesnenin tutamacı yok");

    const Point2 to = Point2{grips.front().at.x + 1000, grips.front().at.y + 500};
    Args a;
    a.set("nesne", ids({key}));
    a.set("kose", Value::integer(1));
    a.set("nokta", Value::point(to));
    if (auto bad = edit_failed(invoke(r, "core.vertex_move", std::move(a)))) return *bad;

    const Probe after = probe(r, key);
    if (!after.alive) return refused("tutamaç taşındıktan sonra nesne kayboldu");
    if (family(after.kind) != family(before.kind))
        return partial(std::to_string(grips.size()) + " tutamaç; ilki taşınınca " +
                       kind_name(before.kind) + " → " + kind_name(after.kind));
    return supported(std::to_string(grips.size()) + " tutamaç; ilki taşındı, tür korundu");
}

Cell op_move(Rig& r, const Fixture&, EntityKey key)
{
    const Probe before = probe(r, key);
    Args a;
    a.set("nesneler", ids({key}));
    a.set("baslangic", Value::point({0, 0}));
    a.set("bitis", Value::point({5000, 3000}));
    if (auto bad = edit_failed(invoke(r, "core.move", std::move(a)))) return *bad;

    const Probe after = probe(r, key);
    if (!after.alive) return refused("taşınan nesne kayboldu");
    const bool exact =
        after.box.min_x == before.box.min_x + 5000 && after.box.min_y == before.box.min_y + 3000 &&
        after.box.max_x == before.box.max_x + 5000 && after.box.max_y == before.box.max_y + 3000;
    if (after.kind != before.kind)
        return partial("tür değişti: " + kind_name(before.kind) + " → " + kind_name(after.kind));
    if (!exact) return partial("kapsam tam (5, 3) m kaymadı");
    return supported("kapsam tam (5, 3) m kaydı, tür ve ölçü korundu");
}

Cell op_rotate(Rig& r, const Fixture&, EntityKey key)
{
    const Probe before = probe(r, key);
    const Point2 c     = centre_of(before.box);
    Args a;
    a.set("nesneler", ids({key}));
    a.set("merkez", Value::point(c));
    a.set("aci", Value::number(90.0));
    if (auto bad = edit_failed(invoke(r, "core.rotate", std::move(a)))) return *bad;

    const Probe after = probe(r, key);
    if (!after.alive) return refused("döndürülen nesne kayboldu");
    if (after.kind != before.kind)
        return partial("tür değişti: " + kind_name(before.kind) + " → " + kind_name(after.kind));

    const std::size_t n  = before.outline.points().size();
    const Mm w0          = before.box.max_x - before.box.min_x;
    const Mm h0          = before.box.max_y - before.box.min_y;
    const Mm w1          = after.box.max_x - after.box.min_x;
    const Mm h1          = after.box.max_y - after.box.min_y;
    const bool swapped   = near(w1, h0, slack(h0, n)) && near(h1, w0, slack(w0, n));
    const bool same_size = near(after.perimeter, before.perimeter, slack(before.perimeter, n));
    if (!swapped) return partial("90° sonrası kapsam eni ve boyu yer değiştirmedi");
    if (!same_size)
        return partial("çevre korunmadı: " + metres(before.perimeter) + " → " +
                       metres(after.perimeter));
    return supported("90° döndü: en ve boy yer değiştirdi, çevre korundu");
}

Cell op_scale(Rig& r, const Fixture&, EntityKey key)
{
    const Probe before = probe(r, key);
    Args a;
    a.set("nesneler", ids({key}));
    a.set("merkez", Value::point(centre_of(before.box)));
    a.set("carpan", Value::number(2.0));
    if (auto bad = edit_failed(invoke(r, "core.scale", std::move(a)))) return *bad;

    const Probe after = probe(r, key);
    if (!after.alive) return refused("ölçeklenen nesne kayboldu");
    if (after.kind != before.kind)
        return partial("tür değişti: " + kind_name(before.kind) + " → " + kind_name(after.kind));

    const std::size_t n = before.outline.points().size();
    const Mm w0         = before.box.max_x - before.box.min_x;
    const Mm w1         = after.box.max_x - after.box.min_x;
    if (!near(w1, 2 * w0, slack(2 * w0, n))) return partial("kapsam iki katına çıkmadı");
    if (before.area_m2 > 0.0 &&
        std::abs(after.area_m2 - 4.0 * before.area_m2) > 1e-3 * after.area_m2)
        return partial("alan dört katına çıkmadı: " + square_metres(before.area_m2) + " → " +
                       square_metres(after.area_m2));
    if (before.area_m2 > 0.0) return supported("×2: kapsam iki, alan dört katına çıktı");
    return supported("×2: kapsam iki katına çıktı");
}

Cell op_mirror(Rig& r, const Fixture&, EntityKey key)
{
    const Probe before                = probe(r, key);
    const Point2 c                    = centre_of(before.box);
    const std::vector<EntityKey> keys = live_keys(r);
    Args a;
    a.set("nesneler", ids({key}));
    a.set("baslangic", Value::point({c.x, c.y - 10000}));
    a.set("bitis", Value::point({c.x, c.y + 10000}));
    if (auto bad = edit_failed(invoke(r, "core.mirror", std::move(a)))) return *bad;

    // THE ORIGINAL OR A COPY: some programs mirror in place and some leave the
    // source, and either is a working mirror as long as the image is right.
    //
    // THE TARGET AND WHAT IS NEW, not every live entity: `LİDER` keeps its
    // caption as a separate text beside it, and counting that text as an image
    // of the leader reported a leader that "became a polyline".
    std::vector<Probe> images;
    for (const EntityKey k : results(r, keys, key))
        images.push_back(probe(r, k));
    if (images.empty()) return refused("aynadan sonra nesne kalmadı");
    for (const Probe& p : images) {
        if (p.kind != before.kind)
            return partial("tür değişti: " + kind_name(before.kind) + " → " + kind_name(p.kind));
        const std::size_t n = before.outline.points().size();
        if (!near(p.perimeter, before.perimeter, slack(before.perimeter, n)))
            return partial("çevre korunmadı: " + metres(before.perimeter) + " → " +
                           metres(p.perimeter));
    }
    return supported("dikey eksende aynalandı, tür ve çevre korundu");
}

Cell op_trim(Rig& r, const Fixture& f, EntityKey key)
{
    if (f.shape == Shape::Annotation) return meaningless("kesilecek bir eğri değil");

    const Probe before = probe(r, key);
    const Point2 c     = centre_of(before.box);
    // A boundary straight through the middle, top to bottom, well past both
    // edges — every fixture is crossed by it at least once.
    auto boundary = cross_line(r, c, 1.0, 0.0,
                               static_cast<double>(before.box.max_y - before.box.min_y) + 20000.0);
    if (!boundary) return refused(boundary.error());

    // The piece to drop: the outline vertex furthest to the WEST, which is on
    // the curve and on one side of the boundary.
    Point2 pick = before.outline.points().front();
    for (const Point2 p : before.outline.points())
        if (p.x < pick.x) pick = p;

    const std::vector<EntityKey> keys = live_keys(r);
    Args a;
    a.set("nesne", ids({key}));
    a.set("sinir", ids({boundary.value()}));
    a.set("nokta", Value::point(pick));
    if (auto bad = edit_failed(invoke(r, "core.trim", std::move(a)))) return *bad;

    std::vector<Probe> after;
    for (const EntityKey k : results(r, keys, key, {boundary.value()}))
        after.push_back(probe(r, k));
    if (after.empty()) return refused("kesimden sonra nesne kalmadı");
    Mm total = 0;
    for (const Probe& p : after)
        total += p.perimeter;
    if (total >= before.perimeter)
        return partial("uzunluk azalmadı: " + metres(before.perimeter) + " → " + metres(total));
    return curve_kept(before, after,
                      "kesildi: " + metres(before.perimeter) + " → " + metres(total));
}

Cell op_extend(Rig& r, const Fixture& f, EntityKey key)
{
    if (f.shape == Shape::Closed) return meaningless("kapalı eğrinin ucu yok");
    if (f.shape == Shape::Annotation) return meaningless("ucu uzatılacak bir eğri değil");

    const Probe before = probe(r, key);
    const End end      = last_end(before.outline);
    // A boundary ten metres ahead of the end, square to the way it leaves.
    const Point2 ahead = offset_point(end.at, end.dx, end.dy, 10000.0);
    auto boundary      = cross_line(r, ahead, end.dx, end.dy, 40000.0);
    if (!boundary) return refused(boundary.error());

    const std::vector<EntityKey> keys = live_keys(r);
    Args a;
    a.set("nesne", ids({key}));
    a.set("sinir", ids({boundary.value()}));
    a.set("nokta", Value::point(end.at));
    if (auto bad = edit_failed(invoke(r, "core.extend", std::move(a)))) return *bad;

    std::vector<Probe> after;
    for (const EntityKey k : results(r, keys, key, {boundary.value()}))
        after.push_back(probe(r, k));
    if (after.empty()) return refused("uzatmadan sonra nesne kalmadı");
    if (after.front().perimeter <= before.perimeter)
        return partial("uzunluk artmadı: " + metres(before.perimeter) + " → " +
                       metres(after.front().perimeter));
    return curve_kept(before, after,
                      "uzadı: " + metres(before.perimeter) + " → " +
                          metres(after.front().perimeter));
}

Cell op_split(Rig& r, const Fixture& f, EntityKey key)
{
    if (f.shape == Shape::Annotation) return meaningless("bölünecek bir eğri değil");

    const Probe before                = probe(r, key);
    const std::vector<EntityKey> keys = live_keys(r);
    Args a;
    a.set("nesne", ids({key}));
    a.set("nokta", Value::point(along(before.outline, 0.5)));
    if (auto bad = edit_failed(invoke(r, "core.split", std::move(a)))) return *bad;

    std::vector<Probe> after;
    for (const EntityKey k : results(r, keys, key))
        after.push_back(probe(r, k));
    if (after.size() < 2 && f.shape == Shape::Open)
        return partial("tek parça kaldı: " + std::to_string(after.size()));
    Mm total = 0;
    for (const Probe& p : after)
        total += p.perimeter;
    const std::size_t n = before.outline.points().size();
    if (!near(total, before.perimeter, slack(before.perimeter, n)))
        return partial("parçaların toplamı kaynağa eşit değil: " + metres(before.perimeter) +
                       " → " + metres(total));
    return curve_kept(before, after,
                      std::to_string(after.size()) + " parça, toplam uzunluk korundu");
}

Cell op_break(Rig& r, const Fixture& f, EntityKey key)
{
    if (f.shape == Shape::Annotation) return meaningless("kırılacak bir eğri değil");

    const Probe before                = probe(r, key);
    const std::vector<EntityKey> keys = live_keys(r);
    Args a;
    a.set("nesne", ids({key}));
    a.set("birinci", Value::point(along(before.outline, 0.33)));
    a.set("ikinci", Value::point(along(before.outline, 0.66)));
    if (auto bad = edit_failed(invoke(r, "core.break", std::move(a)))) return *bad;

    std::vector<Probe> after;
    for (const EntityKey k : results(r, keys, key))
        after.push_back(probe(r, k));
    Mm total = 0;
    for (const Probe& p : after)
        total += p.perimeter;
    if (after.empty()) return refused("kırıldıktan sonra nesne kalmadı");
    if (total >= before.perimeter) return partial("aradaki parça çıkmadı");
    return curve_kept(before, after,
                      "aradaki parça çıktı: " + metres(before.perimeter) + " → " + metres(total));
}

Cell op_offset(Rig& r, const Fixture& f, EntityKey key)
{
    if (f.shape == Shape::Annotation) return meaningless("paraleli alınacak bir eğri değil");

    const Probe before                = probe(r, key);
    const std::vector<EntityKey> keys = live_keys(r);
    Args a;
    a.set("nesneler", ids({key}));
    a.set("mesafe", Value::integer(2000));
    if (auto bad = edit_failed(invoke(r, "core.offset", std::move(a)))) return *bad;

    std::vector<Probe> made;
    for (const EntityKey k : live_keys(r))
        if (!contains(keys, k)) made.push_back(probe(r, k));
    const bool source_kept = probe(r, key).alive;
    if (made.empty()) return refused("paralel üretilmedi");

    // THE QUESTION C-03 ASKS, measured: a CAD parallel of an open curve is an
    // open curve; a closed band around it is a buffer, which is a different
    // operation with a different name.
    const Probe& p = made.front();
    if (f.shape == Shape::Open && p.closed)
        return partial("açık eğriden kapalı bant (" + kind_name(p.kind) + ", " +
                       square_metres(p.area_m2) + ") üretildi — paralel değil tampon");
    if (true_curve(before.kind) && is_polyline(p.kind))
        return partial(kind_name(before.kind) + " paraleli kirişlerle (" + kind_name(p.kind) +
                       ") üretildi");
    if (!source_kept) return partial("paralel üretildi ama kaynak silindi");

    // A FACE'S HOLES ARE PART OF IT. Offsetting a holed parcel must give one
    // holed face; getting the exterior and the hole back as separate closed
    // lines has lost which ring is a hole of which — the topology.
    if (before.holes > 0) {
        std::size_t kept = 0;
        for (const Probe& q : made)
            kept += q.holes;
        if (kept != before.holes)
            return partial(std::to_string(before.holes) + " delikli alandan " +
                           std::to_string(made.size()) + " ayrı nesne (" + kind_name(p.kind) +
                           ") çıktı, " + std::to_string(kept) +
                           " delik kaldı — delik ilişkisi kayboldu");
    }
    if (f.shape == Shape::Closed && before.kind == core::kCircleKind &&
        p.kind == core::kCircleKind) {
        const double r0 = static_cast<double>(before.perimeter) / (2.0 * std::numbers::pi);
        const double r1 = static_cast<double>(p.perimeter) / (2.0 * std::numbers::pi);
        if (std::abs(std::abs(r1 - r0) - 2000.0) > 2.0)
            return partial("yarıçap 2 m değişmedi: " + metres(static_cast<Mm>(r0)) + " → " +
                           metres(static_cast<Mm>(r1)));
        return supported("yarıçap 2 m değişti, daire kaldı");
    }
    return supported(std::to_string(made.size()) + " paralel (" + kind_name(p.kind) +
                     "), kaynak korundu");
}

Cell corner_op(Rig& r, const Fixture& f, EntityKey key, const char* command,
               const char* amount_param, double amount, const char* done)
{
    if (!f.has_corner) return meaningless("tek nesnede köşe yok; iki nesne arası köşe C-06'da");

    const Probe before = probe(r, key);
    // The corner of every cornered fixture sits at (50, 0) or at the first
    // corner of a face; the grips list starts with the vertices.
    Point2 corner{50000, 0};
    if (f.shape == Shape::Closed) corner = before.outline.points().at(1);

    Args a;
    a.set("nesne", ids({key}));
    a.set("nokta", Value::point(corner));
    a.set(amount_param, Value::number(amount));
    const std::vector<EntityKey> keys = live_keys(r);
    if (auto bad = edit_failed(invoke(r, command, std::move(a)))) return *bad;

    std::vector<Probe> after;
    for (const EntityKey k : results(r, keys, key))
        after.push_back(probe(r, k));
    if (after.empty()) return refused("işlemden sonra nesne kalmadı");
    const Probe& p = after.front();
    if (std::string(command) == "core.fillet" && is_polyline(p.kind))
        return partial(std::string(done) + "; yuvarlatma yayı kirişlerle (" + kind_name(p.kind) +
                       ")");
    return supported(std::string(done) + "; sonuç: " + kind_name(p.kind));
}

Cell op_fillet(Rig& r, const Fixture& f, EntityKey key)
{
    return corner_op(r, f, key, "core.fillet", "yaricap", 5.0, "5 m yarıçapla yuvarlandı");
}

Cell op_chamfer(Rig& r, const Fixture& f, EntityKey key)
{
    return corner_op(r, f, key, "core.chamfer", "mesafe", 5.0, "5 m pah kırıldı");
}

Cell op_join(Rig& r, const Fixture& f, EntityKey key)
{
    if (f.shape != Shape::Open) return meaningless("uç uca eklenecek ucu yok");

    const Probe before = probe(r, key);
    // THE SECOND PIECE IS THE FIRST'S MIRROR across the normal at its end: it
    // passes through that end and leaves it in the same direction the first
    // arrived, so the two meet smoothly — the case a join is for.
    const End end                     = last_end(before.outline);
    const std::vector<EntityKey> keys = live_keys(r);
    {
        Args copy;
        copy.set("nesneler", ids({key}));
        copy.set("baslangic", Value::point({0, 0}));
        copy.set("bitis", Value::point({0, 0}));
        if (auto bad = edit_failed(invoke(r, "core.copy", std::move(copy)))) return *bad;
    }
    EntityKey twin = EntityKey::None;
    for (const EntityKey k : live_keys(r))
        if (!contains(keys, k)) twin = k;
    if (twin == EntityKey::None) return refused("ikinci parça oluşmadı");
    {
        Args mirror;
        mirror.set("nesneler", ids({twin}));
        mirror.set("baslangic", Value::point(offset_point(end.at, -end.dy, end.dx, 10000.0)));
        mirror.set("bitis", Value::point(offset_point(end.at, end.dy, -end.dx, 10000.0)));
        if (auto bad = edit_failed(invoke(r, "core.mirror", std::move(mirror)))) return *bad;
    }

    const std::vector<EntityKey> pieces = live_keys(r);
    Mm sum                              = 0;
    for (const EntityKey k : pieces)
        sum += probe(r, k).perimeter;

    Args a;
    a.set("nesne", ids({key, twin}));
    a.set("tolerans", Value::number(0.01));
    if (auto bad = edit_failed(invoke(r, "core.join", std::move(a)))) return *bad;

    const std::vector<EntityKey> left = live_keys(r);
    if (left.size() != 1) return partial(std::to_string(left.size()) + " parça kaldı, birleşmedi");
    const Probe p       = probe(r, left.front());
    const std::size_t n = before.outline.points().size() * 2;
    if (!near(p.perimeter, sum, slack(sum, n)))
        return partial("birleşik uzunluk parçaların toplamı değil: " + metres(sum) + " → " +
                       metres(p.perimeter));
    if (true_curve(before.kind) && is_polyline(p.kind))
        return partial("birleşti ama eğri kirişlere (" + kind_name(p.kind) + ") dönüştü");
    return supported("iki parça tek " + kind_name(p.kind) + " oldu, uzunluk korundu");
}

Cell op_measure(Rig& r, const Fixture& f, EntityKey key)
{
    Args a;
    a.set("nesneler", ids({key}));
    if (auto ran = invoke(r, "core.entity_info", std::move(a)); !ran) return refused(ran.error());

    const Probe p    = probe(r, key);
    std::string said = "çevre " + metres(p.perimeter);
    if (p.area_m2 > 0.0) said += ", alan " + square_metres(p.area_m2);

    // AGAINST THE ANALYTIC VALUE, and within a millimetre: a surveyed figure on
    // a signed document is either right or wrong, and a circle measured by its
    // chords is short by a sum a surveyor can see.
    if (f.length_m) {
        const Mm expected = static_cast<Mm>(std::llround(*f.length_m * 1000.0));
        if (!near(p.perimeter, expected, 1))
            return partial(said + " — analitik " + metres(expected));
    }
    if (f.area_m2 && std::abs(p.area_m2 - *f.area_m2) > 1e-3)
        return partial(said + " — analitik alan " + square_metres(*f.area_m2));
    if (!f.length_m && !f.area_m2) return supported(said + " (analitik değer tanımlı değil)");
    return supported(said + " — analitik değerle aynı");
}

Cell op_project(Rig& r, const Fixture&, EntityKey key, const std::string& scratch,
                const std::string& id)
{
    const Probe before     = probe(r, key);
    const std::string path = scratch + "/" + id + ".pcad";
    if (auto st = line(r, "FARKLIKAYDET \"" + path + "\""); !st) return refused(st.error());

    Rig reopened;
    if (auto st = line(reopened, "AÇ \"" + path + "\""); !st) return refused(st.error());
    const Probe after = probe(reopened, key);
    if (!after.alive) return refused("açılan dosyada nesne yok");
    if (after.kind != before.kind)
        return partial("tür değişti: " + kind_name(before.kind) + " → " + kind_name(after.kind));
    if (after.perimeter != before.perimeter || std::abs(after.area_m2 - before.area_m2) > 1e-9 ||
        after.box.min_x != before.box.min_x || after.box.max_y != before.box.max_y)
        return partial("geometri değişti");
    return supported("kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı");
}

std::vector<Operation> operations(const std::string& scratch)
{
    return {
        {"sec", "Seç", "core.select", op_select},
        {"yakala", "Yakala", "", op_snap},
        {"tutamac", "Tutamaç", "core.vertex_move", op_grip},
        {"tasi", "Taşı", "core.move", op_move},
        {"dondur", "Döndür", "core.rotate", op_rotate},
        {"olcekle", "Ölçekle", "core.scale", op_scale},
        {"aynala", "Aynala", "core.mirror", op_mirror},
        {"kes", "Kes (BUDA)", "core.trim", op_trim},
        {"uzat", "Uzat", "core.extend", op_extend},
        {"bol", "Böl", "core.split", op_split},
        {"kir", "Kır", "core.break", op_break},
        {"paralel", "Paralel (OFSET)", "core.offset", op_offset},
        {"yuvarla", "Yuvarla", "core.fillet", op_fillet},
        {"pah", "Pah", "core.chamfer", op_chamfer},
        {"ucuca", "Uç uca", "core.join", op_join},
        {"olc", "Ölç", "core.entity_info", op_measure},
        {"proje", "Proje dosyası", "core.saveas",
         [scratch](Rig& r, const Fixture& f, EntityKey key) {
             return op_project(r, f, key, scratch, f.id);
         }},
    };
}

// =============================================================================
// the document
// =============================================================================

const char* mark(Verdict v)
{
    switch (v) {
    case Verdict::Supported: return "✓";
    case Verdict::Partial: return "◐";
    case Verdict::None: return "✗";
    case Verdict::NotApplicable: return "—";
    case Verdict::Unbuilt: return "⊘";
    }
    return "?";
}

const char* word(Verdict v)
{
    switch (v) {
    case Verdict::Supported: return "destekli";
    case Verdict::Partial: return "kısmi";
    case Verdict::None: return "yok";
    case Verdict::NotApplicable: return "uygulanamaz";
    case Verdict::Unbuilt: return "ölçülemedi";
    }
    return "?";
}

std::string surface_table(const std::vector<Operation>& ops)
{
    Rig r;
    std::string out;
    out +=
        "| İşlem | Komut | Komut satırı ve arayüz | Yapay zekâ ve MCP | Python ve JSON betik |\n";
    out += "|---|---|---|---|---|\n";
    for (const Operation& op : ops) {
        if (op.command[0] == '\0') {
            out += std::string("| ") + op.title + " | — | ✓ imleçle | — okuma aracı yok | — |\n";
            continue;
        }
        const command::CommandSpec* spec = r.reg.by_id(op.command);
        if (spec == nullptr) {
            out += std::string("| ") + op.title + " | `" + op.command +
                   "` | ✗ kayıtlı değil | ✗ | ✗ |\n";
            continue;
        }
        const bool ai     = command::has_flag(spec->flags, command::Flags::AiAccessible);
        const bool script = command::has_flag(spec->flags, command::Flags::Scriptable);
        out +=
            std::string("| ") + op.title + " | `" + op.command + "` (`" + spec->names.front() +
            "`) | ✓ | " + (ai ? "✓" : "✗ bayrak yok") + " | " +
            (script ? "✓ `cad." + command::python_callable_name(*spec) + "`" : std::string("✗")) +
            " |\n";
    }
    return out;
}

int run(int argc, char** argv)
{
    if (argc < 4) {
        (void)std::fprintf(stderr,
                           "kullanım: kentos_kapsam <çıktı.md> <depo kökü> <geçici dizin>\n");
        return 2;
    }
    const std::string out_path = argv[1];
    g_data_root                = argv[2];
    const std::string scratch  = argv[3];
    std::filesystem::create_directories(scratch);

    const std::vector<Fixture> rows      = fixtures();
    const std::vector<Operation> columns = operations(scratch);

    std::vector<std::vector<Cell>> grid(rows.size(), std::vector<Cell>(columns.size()));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        for (std::size_t j = 0; j < columns.size(); ++j) {
            Rig r;
            auto key = rows[i].make(r);
            if (!key) {
                grid[i][j] = Cell{Verdict::Unbuilt, gist(key.error().message)};
                continue;
            }
            try {
                grid[i][j] = columns[j].run(r, rows[i], key.value());
            } catch (const std::exception& e) {
                grid[i][j] = refused(std::string("istisna: ") + e.what());
            }
        }
    }

    std::string doc;
    doc += "<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->\n";
    doc +=
        "<!-- Kaynak: kentos_kapsam. Her hücre bir komut GERÇEKTEN çalıştırılarak ölçülür. -->\n";
    doc += "<!-- Yeniden üret: make kapsam -->\n\n";
    doc += "# Destek Matrisi\n\n";
    doc += "Hangi düzenleme işleminin hangi nesne türünde ne yaptığını gösterir. Tablo elle\n";
    doc += "yazılmadı: her hücre için boş bir çizimde o tür, bir kullanıcının yazacağı komutla\n";
    doc += "oluşturulur, işlem aynı komut veri yolundan çalıştırılır ve **çıkan sonuç ölçülür**.\n";
    doc += "Bir davranış değiştiğinde bu sayfa da aynı değişiklikte değişir.\n\n";
    doc += "| İşaret | Anlamı |\n|---|---|\n";
    doc += "| ✓ destekli | İşlem çalıştı ve sonuç o türden beklenen şey: kesilen yay yay kalır, "
           "taşınan nesne tam kayar |\n";
    doc += "| ◐ kısmi | İşlem çalıştı ama sonuç **türce yanlış**: eğri kirişe döndü, açık çizgi "
           "kapalı banda döndü, ölçü analitik değerden sapıyor |\n";
    doc += "| ✗ yok | Komut reddetti; ret cümlesi kanıttır |\n";
    doc += "| — uygulanamaz | İşlemin o türde anlamı yok: kapalı dairenin ucu uzatılmaz, tek "
           "noktada köşe yoktur |\n";
    doc +=
        "| ⊘ ölçülemedi | Tür, bir kullanıcının yazabileceği hiçbir komutla oluşturulamadı |\n\n";
    doc += "Dosya alışverişi (DXF, DWG, GeoPackage) bu tabloda **yoktur**: sonucu derlemedeki\n";
    doc += "biçim kütüphanelerine bağlıdır ve ayrı bir matriste ölçülecektir. Burada ölçülen tek\n";
    doc += "gidiş-dönüş, her derlemenin yazıp okuduğu proje dosyasıdır.\n\n";

    doc += "## Özet\n\n| Tür |";
    for (const Operation& op : columns)
        doc += std::string(" ") + op.title + " |";
    doc += "\n|---|";
    for (std::size_t j = 0; j < columns.size(); ++j)
        doc += "---|";
    doc += "\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        doc += std::string("| [") + rows[i].title + "](#" + rows[i].id + ") |";
        for (std::size_t j = 0; j < columns.size(); ++j)
            doc += std::string(" ") + mark(grid[i][j].verdict) + " |";
        doc += "\n";
    }

    // ONE SLOT PER VERDICT, sized by the enum rather than by a number typed
    // here: the fifth state was added after the first four, and a four-slot
    // array indexed by it was an out-of-bounds write the hardened library
    // stopped with a trap.
    std::array<int, static_cast<std::size_t>(Verdict::Unbuilt) + 1> totals{};
    for (const auto& row : grid)
        for (const Cell& c : row)
            ++totals[static_cast<std::size_t>(c.verdict)];
    const int measured = totals[0] + totals[1] + totals[2] + totals[3];
    doc += "\n" + std::to_string(measured) + " hücre: " + std::to_string(totals[0]) +
           " destekli, " + std::to_string(totals[1]) + " kısmi, " + std::to_string(totals[2]) +
           " yok, " + std::to_string(totals[3]) + " uygulanamaz.";
    if (totals[4] > 0)
        doc += " ⊘ işaretli satırların türü hiçbir komutla oluşturulamadığı için ölçülemedi;\n"
               "nedeni o türün kanıt bölümündedir.";
    doc += "\n\n";

    // THE SILENT REFUSALS, gathered, because they are a defect of their own and
    // not a property of any one kind: the command refused, said so on the
    // transcript, and told the bus it succeeded. A person is informed; a script,
    // an agent, MCP and Python are misinformed (TODOS F-05, A-07).
    std::vector<std::string> silent;
    for (std::size_t i = 0; i < rows.size(); ++i)
        for (std::size_t j = 0; j < columns.size(); ++j)
            if (grid[i][j].evidence.rfind(kSilent, 0) == 0)
                silent.push_back(std::string("| ") + rows[i].title + " | " + columns[j].title +
                                 " | `" + columns[j].command + "` | " +
                                 grid[i][j].evidence.substr(std::string(kSilent).size() + 1) +
                                 " |");
    doc += "## Sessiz retler\n\n";
    if (silent.empty()) {
        doc += "Yok: reddeden her komut reddini hata olarak döndürüyor.\n\n";
    } else {
        doc +=
            std::to_string(silent.size()) +
            " hücrede komut işlemi **reddetti ama veri yoluna başarı bildirdi**: reddini\n"
            "transkripte bir cümle olarak yazdı ve gövdesini bitirdi. Komut satırındaki kişi\n"
            "cümleyi okur; bir betik, yapay zekâ, MCP ve Python ise işlemin yapıldığını sanır ve\n"
            "kalan adımlara devam eder. Ret bir hata olarak dönmelidir.\n\n";
        doc += "| Tür | İşlem | Komut | Transkripte yazılan |\n|---|---|---|---|\n";
        for (const std::string& row : silent)
            doc += row + "\n";
        doc += "\n";
    }

    doc += "## Kanıt\n\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        doc += std::string("### <a id=\"") + rows[i].id + "\"></a>" + rows[i].title + "\n\n";
        if (grid[i].front().verdict == Verdict::Unbuilt) {
            doc += "⊘ **Ölçülemedi: bu tür hiçbir komutla oluşturulamıyor.** Denenen yol: " +
                   grid[i].front().evidence + "\n\n";
            continue;
        }
        doc += "| İşlem | Durum | Kanıt |\n|---|---|---|\n";
        for (std::size_t j = 0; j < columns.size(); ++j)
            doc += std::string("| ") + columns[j].title + " | " + mark(grid[i][j].verdict) + " " +
                   word(grid[i][j].verdict) + " | " + grid[i][j].evidence + " |\n";
        doc += "\n";
    }

    doc += "## Yüzeyler\n\n";
    doc += "Bütün istemciler aynı komut veri yolunu kullanır, dolayısıyla bir işlemin **sonucu**\n";
    doc += "yüzeyden yüzeye değişmez — bunu GUI = komut satırı = betik eşitlik kanıtı sınar. "
           "Değişen\n";
    doc += "tek şey işleme **ulaşılıp ulaşılamadığıdır**, ve o da komutun kendi bayraklarından "
           "okunur.\n\n";
    doc += surface_table(columns);

    std::ofstream out(out_path, std::ios::out | std::ios::binary);
    if (!out) {
        (void)std::fprintf(stderr, "kentos_kapsam: '%s' yazılamadı\n", out_path.c_str());
        return 1;
    }
    out << doc;
    (void)std::fprintf(
        stdout,
        "kapsam: %zu tür × %zu işlem -> %s (%d destekli, %d kısmi, %d yok, %d uygulanamaz, "
        "%d ölçülemedi)\n",
        rows.size(), columns.size(), out_path.c_str(), totals[0], totals[1], totals[2], totals[3],
        totals[4]);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        (void)std::fprintf(stderr, "kentos_kapsam: %s\n", e.what());
        return 1;
    } catch (...) {
        (void)std::fprintf(stderr, "kentos_kapsam: bilinmeyen hata\n");
        return 1;
    }
}
