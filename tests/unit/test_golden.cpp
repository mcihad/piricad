// SPDX-License-Identifier: GPL-3.0-or-later
//
// kentoscad.md §7.3 / §10.5: golden output must be identical, bit for bit, on
// Linux, Windows and macOS in the same CI run. A cadastral area is a legal figure
// and may not depend on the machine that produced it.
//
// Each scenario is replayed and the resulting document is rendered as a
// deterministic text dump, which is diffed against the stored fixture. The dump
// is readable on purpose: when a platform disagrees, the diff must say which
// vertex moved, not merely that a hash changed.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

namespace fs = std::filesystem;

std::string read_file(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

/// Hex, fixed width: a decimal rendering of the same number would differ in
/// leading zeros between formatting implementations.
std::string hex64(std::uint64_t v)
{
    static const char* digits = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = digits[v & 0xF];
        v >>= 4;
    }
    return out;
}

/// The document as text. Millimetres are printed as integers, so nothing here
/// passes through a floating-point formatter — the dump cannot introduce a
/// difference the document does not have.
///
/// SPLIT FROM THE JOURNAL because a REPLAY writes its own journal: the origins
/// differ and so do the timestamps, and what a replay has to reproduce is the
/// drawing. The fixture on disk holds both halves.
std::string dump_document(const core::Document& doc)
{
    std::string out;

    out += "crs " + doc.crs().id() + "\n";
    out += "katman-sayisi " + std::to_string(doc.layers().size()) + "\n";
    out += "nesne-sayisi " + std::to_string(doc.live_entity_count()) + "\n";

    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& l = doc.layers()[i];
        out += "katman " + l.name;
        out += " gorunur=" + std::string(l.visible ? "1" : "0");
        out += " kilitli=" + std::string(l.locked ? "1" : "0");
        out += " renk=" + hex64(l.appearance.rgba).substr(8);
        out += " kalinlik_um=" + std::to_string(l.appearance.width_um);
        out += " nesne=" + std::to_string(doc.layer_entity_count(static_cast<core::LayerId>(i)));
        out += "\n";
    }

    const auto& entities = doc.entities();
    const auto& geometry = doc.geometry();

    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e)) continue;

        const core::RingSpan span = geometry.rings_of(entities.slot[e]);
        out += "nesne " + std::to_string(e) + " katman=" + doc.layers()[entities.layer[e]].name +
               " stil=" + std::to_string(entities.style[e]) +
               " halka=" + std::to_string(span.count);
        // The kind and the payload are content (model.md R9a): written only when
        // they say something, so a polyline's line reads as it always did.
        if (entities.kind[e] != core::kPolylineKind)
            out += " tur=" + std::to_string(entities.kind[e]);
        if (const auto bytes = geometry.payload_of(entities.slot[e]); !bytes.empty()) {
            out += " yuk=";
            for (const std::uint8_t b : bytes) {
                constexpr char kHex[] = "0123456789abcdef";
                out += kHex[b >> 4];
                out += kHex[b & 0x0F];
            }
        }
        out += "\n";

        // Ring role, part and vertex run are all part of the identity of the
        // geometry: a parcel and the same outline digitised as a polyline are
        // different documents (model.md R11).
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            out += "  halka rol=" + std::to_string(static_cast<int>(geometry.ring_role[r])) +
                   " parca=" + std::to_string(geometry.ring_part[r]) +
                   " tepe=" + std::to_string(geometry.ring_count[r]) + "\n";

            const auto xs = geometry.ring_xs(r);
            const auto ys = geometry.ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                out += "    " + std::to_string(xs[v]) + " " + std::to_string(ys[v]) + "\n";
        }

        out += "  alan_mm2 " + std::to_string(doc.entity_area(e)) + "  cevre_mm " +
               std::to_string(doc.entity_perimeter(e)) + "\n";
    }

    const core::Box2 box = doc.extent();
    if (box.empty()) {
        out += "kapsam bos\n";
    } else {
        out += "kapsam " + std::to_string(box.min_x) + " " + std::to_string(box.min_y) + " " +
               std::to_string(box.max_x) + " " + std::to_string(box.max_y) + "\n";
    }

    out += "icerik-ozeti " + hex64(doc.content_hash()) + "\n";
    return out;
}

/// The document half plus the journal — the whole fixture.
std::string dump(const core::Document& doc, const Journal& journal)
{
    std::string out = dump_document(doc);

    out += "gunluk\n";
    std::istringstream lines(journal.canonical());
    std::string line;
    while (std::getline(lines, line))
        out += "  " + line + "\n";

    return out;
}

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig()
    {
        register_builtin_commands(reg);

        // The manual and the golden scenarios may name ANY command the program
        // ships, and a domain module owns some of them: `/src/command` may not
        // depend on `/src/domain`, so its builtin list cannot mention OTURT
        // (Article 3.2). A harness that registers only the builtins reports a
        // real command as unknown.
        domain::geodesy::register_geodesy_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        domain::surface::register_surface_commands(reg);
        bus.on_echo = [](std::string_view) {};
    }
};

/// Runs one scenario into `rig` and says what went wrong, or nothing.
///
/// Split out of `replay` so a caller that needs the RIG — its journal, not just
/// its dump — does not have to run the scenario a second way.
bool run_into(Rig& rig, const fs::path& scenario, std::string& error);

/// Runs one scenario and returns its dump, or an empty string on failure.
std::string replay(const fs::path& scenario, std::string& error)
{
    Rig rig;

    if (scenario.extension() == ".json") {
        script::JsonRunner runner(rig.bus, script::Sandbox::Project);
        if (auto r = runner.run_file(scenario.string()); !r) {
            error = r.error().message;
            return {};
        }
    } else {
        std::istringstream lines(read_file(scenario));
        std::string line;
        std::size_t lineno = 0;

        while (std::getline(lines, line)) {
            ++lineno;
            const auto begin = line.find_first_not_of(" \t\r");
            if (begin == std::string::npos || line[begin] == '#') continue;

            if (auto r = rig.bus.execute_line(line, Origin::Test); !r) {
                error = "satır " + std::to_string(lineno) + ": " + r.error().message;
                return {};
            }
        }
    }
    return dump(rig.doc, rig.journal);
}

bool run_into(Rig& rig, const fs::path& scenario, std::string& error)
{
    if (scenario.extension() == ".json") {
        script::JsonRunner runner(rig.bus, script::Sandbox::Project);
        if (auto r = runner.run_file(scenario.string()); !r) {
            error = r.error().message;
            return false;
        }
        return true;
    }

    std::istringstream lines(read_file(scenario));
    std::string line;
    std::size_t lineno = 0;
    while (std::getline(lines, line)) {
        ++lineno;
        const auto begin = line.find_first_not_of(" \t\r");
        if (begin == std::string::npos || line[begin] == '#') continue;
        if (auto r = rig.bus.execute_line(line, Origin::Test); !r) {
            error = "satır " + std::to_string(lineno) + ": " + r.error().message;
            return false;
        }
    }
    return true;
}

std::vector<fs::path> scenarios()
{
    std::vector<fs::path> out;
    const fs::path dir{KENTOS_GOLDEN_DIR "/senaryolar"};
    if (!fs::exists(dir)) return out;

    for (const auto& entry : fs::directory_iterator(dir)) {
        const auto ext = entry.path().extension();
        if (ext == ".json" || ext == ".txt") out.push_back(entry.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

TEST_CASE("GOLDEN: her senaryo kayıtlı çıktısıyla bit-birebir eşleşir")
{
    const bool update = std::getenv("KENTOS_GOLDEN_UPDATE") != nullptr;
    const fs::path expected_dir{KENTOS_GOLDEN_DIR "/beklenen"};
    fs::create_directories(expected_dir);

    const auto files = scenarios();
    CHECK(!files.empty());

    for (const auto& scenario : files) {
        std::string error;
        const std::string actual = replay(scenario, error);

        if (actual.empty()) {
            FAIL_WITH(scenario.filename().string().c_str(), "senaryo çalışmadı: " + error);
            continue;
        }

        const fs::path expected_path = expected_dir / (scenario.stem().string() + ".txt");

        if (update) {
            std::ofstream out(expected_path, std::ios::binary);
            out << actual;
            continue;
        }

        if (!fs::exists(expected_path)) {
            FAIL_WITH(scenario.filename().string().c_str(),
                      "kayıtlı çıktı yok: " + expected_path.string() +
                          "  (KENTOS_GOLDEN_UPDATE=1 ile üretin)");
            continue;
        }

        const std::string expected = read_file(expected_path);
        if (actual == expected) continue;

        // Report the first differing line, because "hash farklı" is not a
        // diagnosis — the operator needs to know which vertex moved.
        std::istringstream a(actual), b(expected);
        std::string la, lb;
        std::size_t n      = 0;
        std::string detail = "beklenen ile ayrışıyor";

        while (true) {
            // Both streams must be advanced every round: `||` would short-circuit
            // and leave one side empty, which reports every difference as line 1.
            const bool has_a = static_cast<bool>(std::getline(a, la));
            const bool has_b = static_cast<bool>(std::getline(b, lb));
            if (!has_a && !has_b) break;

            ++n;
            if (!has_a) la = "(satır yok)";
            if (!has_b) lb = "(satır yok)";
            if (la != lb) {
                detail = expected_path.filename().string() + ":" + std::to_string(n) +
                         "\n        beklenen: " + lb + "\n        gelen   : " + la;
                break;
            }
        }
        FAIL_WITH(scenario.filename().string().c_str(), detail);
    }
}

TEST_CASE("GOLDEN: aynı senaryo iki kez çalıştırıldığında aynı çıktıyı verir")
{
    // Determinism within one process is the floor: if a scenario does not agree
    // with itself, comparing it across platforms is meaningless.
    for (const auto& scenario : scenarios()) {
        std::string e1, e2;
        const std::string first  = replay(scenario, e1);
        const std::string second = replay(scenario, e2);
        CHECK(first == second);
    }
}

TEST_CASE("GOLDEN: her senaryonun GÜNLÜĞÜ aynı belgeyi yeniden kurar")
{
    // ARTICLE 6.4's replay clause, over every command any scenario uses rather
    // than one case per command. A journal is the record this program promises:
    // §2.2 says an invocation round-trips losslessly, and the proof of that is
    // that replaying the record rebuilds the drawing it came from.
    //
    // THE SECOND DOCUMENT IS BUILT FROM THE RECORD ALONE — the resolved arguments
    // the bus wrote, with no scenario text in sight. A command that recorded the
    // question instead of the answer (a snap that resolved differently the second
    // time, a `Value` that lost its fraction) shows up here and nowhere else: the
    // fractional-sides defect `Value::from_json` carried was exactly this shape.
    for (const auto& scenario : scenarios()) {
        Rig first;
        std::string error;
        if (!run_into(first, scenario, error)) {
            FAIL_WITH(scenario.filename().string().c_str(), "senaryo çalışmadı: " + error);
            continue;
        }

        // A SCENARIO THAT UNDOES CANNOT BE REPLAYED FROM ITS JOURNAL, and that is
        // the design rather than a gap. `GERİAL` is `ReadOnly`, so `Bus::finish`
        // writes no journal line for it: the journal records what the DOCUMENT was
        // asked to do, and undo is not such a thing — it is a move on a stack
        // whose meaning is the stack's, not the drawing's. Replaying a journal
        // that had an undo taken out of it would re-apply the edit the user
        // undid.
        //
        // Named by scanning the scenario for a line the registry resolves to
        // `core.undo` or `core.redo`, so the exemption cannot be claimed by a
        // scenario that does not use one, and a new scenario that does needs no
        // edit here. Reported out loud, because a silent skip is an exemption
        // nobody audits.
        if (scenario.extension() != ".json") {
            bool undoes = false;
            std::istringstream lines(read_file(scenario));
            std::string line;
            while (std::getline(lines, line)) {
                const auto begin = line.find_first_not_of(" \t\r");
                if (begin == std::string::npos || line[begin] == '#') continue;
                const auto end = line.find_first_of(" \t\r", begin);
                const std::string word =
                    line.substr(begin, end == std::string::npos ? end : end - begin);
                const command::CommandSpec* spec = first.reg.resolve(word);
                if (spec != nullptr && (spec->id == "core.undo" || spec->id == "core.redo")) {
                    undoes = true;
                    break;
                }
            }
            if (undoes) {
                MESSAGE("günlük oynatmadan muaf (GERİAL/YİNELE içeriyor): "
                        << scenario.filename().string());
                continue;
            }
        }

        Rig again;
        bool replayed = true;
        for (const auto& entry : first.journal.entries()) {
            // `Origin::Batch` is the replay origin: the line is not being typed
            // again, it is being re-applied.
            auto r = again.bus.dispatch(
                command::Invocation{entry.command_id, entry.args, Origin::Batch});
            if (!r) {
                FAIL_WITH(scenario.filename().string().c_str(),
                          "günlük oynatılamadı (" + entry.command_id + "): " + r.error().message);
                replayed = false;
                break;
            }
        }
        if (!replayed) continue;

        // THE DOCUMENT, not the journal: a replay writes its own journal (the
        // origins differ, and so do the timestamps), and what has to match is the
        // drawing.
        const std::string want = dump_document(first.doc);
        const std::string got  = dump_document(again.doc);
        if (want != got) {
            // The FIRST differing line, because "başka bir belge" is not a
            // diagnosis: the operator needs to know which vertex moved.
            std::istringstream a(want);
            std::istringstream b(got);
            std::string la, lb, report;
            std::size_t n     = 0;
            std::size_t shown = 0;
            while (std::getline(a, la) && shown < 8) {
                ++n;
                if (!std::getline(b, lb)) lb = "(satır yok)";
                if (la == lb) continue;
                report += "\n  satır " + std::to_string(n) + " bekleniyor: " + la + "\n  satır " +
                          std::to_string(n) + " gelen:      " + lb;
                ++shown;
            }
            FAIL_WITH(scenario.filename().string().c_str(),
                      "günlük başka bir belge kurdu" + report);
        }
    }
}

TEST_CASE("GOLDEN: her günlük satırı JSON'a gidip geri dönüyor")
{
    // §2.2, over every command any scenario uses: an invocation is DATA, fully
    // expressible as `Value` and round-tripping losslessly through JSON. This is
    // the property the journal rests on — a parameter whose shape does not
    // survive `to_json` → `from_json` is a parameter whose replay is a different
    // command, which is how a traverse came back with its millimetres cut off.
    std::size_t lines = 0;
    for (const auto& scenario : scenarios()) {
        Rig rig;
        std::string error;
        if (!run_into(rig, scenario, error)) continue;

        for (const auto& entry : rig.journal.entries()) {
            const core::Json as_json = entry.args.to_json();
            auto back                = command::Args::from_json(as_json);
            if (!back) {
                FAIL_WITH(entry.command_id.c_str(), back.error().message);
                continue;
            }
            // COMPARED AS JSON AGAIN, because that is what the file holds: two
            // `Args` that print the same line are the same record.
            CHECK_MESSAGE(back.value().to_json().dump() == as_json.dump(), entry.command_id);
            ++lines;
        }
    }
    CHECK(lines > 0);
}
