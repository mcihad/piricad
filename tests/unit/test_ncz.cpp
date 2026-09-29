// SPDX-License-Identifier: GPL-3.0-or-later
// (discovery draft — replaced before commit)
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/io/service.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>

using namespace kentos;
using namespace kentos::command;
namespace fs = std::filesystem;

namespace {
struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};
    std::string transcript;
    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { transcript.append(s).append("\n"); };
    }
};
} // namespace

TEST_CASE("NCZ-DISCOVER")
{
    std::vector<std::string> files;
    for (const auto& e : fs::directory_iterator(fs::path(KENTOS_FUZZ_DIR) / "tohum" / "ncz")) files.push_back(e.path().string());
    std::sort(files.begin(), files.end());
    if (const char* real = std::getenv("KENTOS_TEST_NCZ")) files.push_back(real);
    for (const auto& f : files) {
        Rig rig;
        const auto t0 = std::chrono::steady_clock::now();
        auto r = rig.bus.execute_line("İÇEAKTAR \"" + f + "\"", Origin::Test);
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("=== %s  ok=%d  %.1f ms  live=%zu layers=%zu\n", f.c_str(), (int)r.ok(), ms, rig.doc.live_entity_count(), rig.doc.layers().size());
        if (!r) std::printf("ERROR: %s\n", r.error().message.c_str());
        std::printf("%s\n", rig.transcript.c_str());
        std::map<int, int> kinds;
        for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e) if (rig.doc.alive(e)) ++kinds[rig.doc.entities().kind[e]];
        for (auto [k, n] : kinds) std::printf("  kind %d: %d\n", k, n);
    }
}
