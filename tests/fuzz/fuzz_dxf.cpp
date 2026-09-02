// SPDX-License-Identifier: GPL-3.0-or-later
//
// libFuzzer harness for the DXF import path (GDAL/OGR + the KentOSCad wrapper).
//
// .claude/io.md R19 names DXF as one of the parsers that must be fuzzed. GDAL's
// own DXF parser is fuzzed upstream by OSS-Fuzz; what is NOT fuzzed anywhere else
// is the seam this project added — the geometry conversion to `Mm`, the ring
// role assignment, the CRS sidecar reader and the transaction rollback. A crash
// found here is ours until proven otherwise.
//
// Built only when KENTOS_WITH_GDAL is ON; without it the target is not defined,
// because a harness that exercises nothing is worse than an absent one.
//
// Build:
//   cmake --preset dev -DKENTOS_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
//   ./build/dev/bin/kentos_fuzz_dxf tests/fuzz/tohum/dxf -max_total_time=300
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/io/service.hpp"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

const std::string& scratch_path()
{
    static const std::string path = [] {
    // The pid is in the name because libFuzzer's `-jobs=N` forks: two workers
    // sharing one scratch file would fuzz each other's input and report a
    // crash nobody can reproduce.
#ifdef _WIN32
        const auto pid = ::_getpid();
#else
        const auto pid = ::getpid();
#endif
        const auto p = std::filesystem::temp_directory_path() /
                       ("kentoscad-fuzz-dxf-" + std::to_string(pid) + ".dxf");
        return p.string();
    }();
    return path;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size > (1u << 20)) return 0;

    {
        std::ofstream out(scratch_path(), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }

    kentos::core::Document doc;
    kentos::command::Registry registry;
    kentos::command::Journal journal;
    kentos::command::UndoStack undo;
    kentos::command::Bus bus{doc, registry, journal, undo};
    kentos::io::FileService files{bus};

    kentos::command::register_builtin_commands(registry);
    bus.on_echo = [](std::string_view) {};
    (void)bus.execute_line("AYAR core.crs.id EPSG:5254", kentos::command::Origin::Test);

    const std::uint64_t before = doc.content_hash();

    auto imported =
        bus.execute_line("İÇEAKTAR \"" + scratch_path() + "\"", kentos::command::Origin::Test);
    if (!imported) {
        // io.md R17/P11: a failed import rolls back to EXACTLY the pre-import
        // document. Not "close enough" — the same fingerprint.
        if (doc.content_hash() != before) __builtin_trap();
        return 0;
    }

    (void)doc.content_hash();
    for (kentos::core::EntityId e = 0; e < doc.entities().size(); ++e) {
        (void)doc.entity_area(e);
        if (doc.entities().layer[e] >= doc.layers().size()) __builtin_trap();
    }
    return 0;
}
