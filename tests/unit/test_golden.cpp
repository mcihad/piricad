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

#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
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
std::string dump(const core::Document& doc, const Journal& journal)
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
               " halka=" + std::to_string(span.count) + "\n";

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
