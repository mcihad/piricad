// SPDX-License-Identifier: GPL-3.0-or-later
//
// libFuzzer harness for the Netcad NCZ reader (io/ncz.hpp).
//
// An NCZ is a binary a user was sent by someone else, and this reader is
// hand-written (no library reads the format): the block walk, the offsets it
// trusts, the compressed curves, the smart objects' property lists, the RTF of a
// plan note, the attribute tables, and the mapping into the document — all of
// it is ours, and every byte of it is reached from a file. io.md R19 and CLAUDE.md
// 6.7: the format ships with its harness and its seeds (tohum/ncz/, written by
// scripts/ncz-tohum.py).
//
// Always built with the fuzz targets: the reader needs no library. Only a map
// sheet's frame uses GDAL's PROJ (ncz_sheets.hpp), and without it the sheet keeps
// its box — the path is fuzzed either way.
//
// Build:
//   cmake --preset dev -DPIRICAD_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
//   ./build/dev/bin/piricad_fuzz_ncz build/dev/fuzz-corpus/ncz tests/fuzz/tohum/ncz \
//       -max_total_time=300
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
                       ("piricad-fuzz-ncz-" + std::to_string(pid) + ".ncz");
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

    const std::uint64_t before = doc.content_hash();

    // Every column as well, so the attribute half of the mapping is reached.
    auto imported = bus.execute_line("İÇEAKTAR \"" + scratch_path() + "\" alanlar=*",
                                     piricad::command::Origin::Test);
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
