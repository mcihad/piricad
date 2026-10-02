// SPDX-License-Identifier: GPL-3.0-or-later
//
// libFuzzer harness for the DWG import path (LibreDWG + the PiriCAD reader).
//
// .claude/io.md R19 and test.md R9 name DWG as a parser that must be fuzzed, and
// this directory's README listed it as "still to land" until DWG reading became
// part of every sanctioned preset (`canvas` in CMakePresets.json). It is the
// riskiest reader this program has, for two reasons that add up: a DWG is the file
// an engineer RECEIVES from somebody else — a municipality, a contractor, an email
// — and its decoder is LibreDWG, a C library that reverse-engineers a closed
// format. A crash found here is either ours (the seam in src/io/src/dwg.cpp: the
// object walk, the conversion to `Mm`, the layer census, the rollback) or the
// library's, and either way a drawing somebody sent is what triggers it.
//
// Built only when PIRICAD_WITH_DWG is ON; without it the target is not defined,
// because a harness that exercises nothing is worse than an absent one.
//
// THE INVARIANTS are the DXF harness's, and for the same reason (io.md R17): a
// failed import leaves the document with EXACTLY the pre-import fingerprint, and a
// successful one leaves entities whose layers exist. A DWG may also come back
// "read with warnings", which is a success — what it must never do is hang or
// crash, so the smoke run passes `-timeout=10`: libFuzzer's default is twenty
// minutes per input, which turns a decoder that loops on a lying section count
// into a silent stall.
//
// libFuzzer's default `-max_len` is 4096 bytes, shorter than most seeds. The
// commands below pass a length the corpus fits in.
//
// Build:
//   cmake --preset dev -DPIRICAD_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
//   ./build/dev/bin/piricad_fuzz_dwg tests/fuzz/tohum/dwg -max_len=131072 -timeout=10
#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/io/service.hpp"

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
                       ("piricad-fuzz-dwg-" + std::to_string(pid) + ".dwg");
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

    piricad::core::Document doc;
    piricad::command::Registry registry;
    piricad::command::Journal journal;
    piricad::command::UndoStack undo;
    piricad::command::Bus bus{doc, registry, journal, undo};
    piricad::io::FileService files{bus};

    piricad::command::register_builtin_commands(registry);
    bus.on_echo = [](std::string_view) {};
    (void)bus.execute_line("AYAR core.crs.id EPSG:5254", piricad::command::Origin::Test);

    const std::uint64_t before = doc.content_hash();

    auto imported =
        bus.execute_line("İÇEAKTAR \"" + scratch_path() + "\"", piricad::command::Origin::Test);
    if (!imported) {
        // io.md R17/P11: a failed import rolls back to EXACTLY the pre-import
        // document. Not "close enough" — the same fingerprint.
        if (doc.content_hash() != before) __builtin_trap();
        return 0;
    }

    (void)doc.content_hash();
    for (piricad::core::EntityId e = 0; e < doc.entities().size(); ++e) {
        (void)doc.entity_area(e);
        if (doc.entities().layer[e] >= doc.layers().size()) __builtin_trap();
    }
    return 0;
}
