// SPDX-License-Identifier: GPL-3.0-or-later
//
// libFuzzer harness for the native project format (.pcad).
//
// .claude/io.md R19 and CLAUDE.md 6.7: every parser ships its harness and its
// seed corpus in the same change as the format. File reading is the largest
// attack surface this product has (kentoscad.md §13), and the native reader is the
// one parser KentOSCad wrote itself — so it is the one nobody else is fuzzing.
//
// WHAT COUNTS AS A CRASH. Nothing here asserts that a random buffer is a valid
// project; almost none are. The property under test is narrower and much
// stronger: for ANY byte sequence, the reader either returns a `Result` error or
// produces a well-formed `Document`, and it never reads outside the mapping,
// never allocates on an unchecked count and never leaves a half-built document
// behind. Under ASan and UBSan, any violation of that is a crash and therefore a
// P0 (io.md Enforcement).
//
// Build:
//   cmake --preset dev -DKENTOS_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
//   ./build/dev/bin/kentos_fuzz_proje tests/fuzz/tohum/proje -max_total_time=300
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
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

/// One temporary path for the whole run.
///
/// The reader takes a PATH because io.md R5 requires it to memory-map the file,
/// and a mapping needs a file descriptor. Adding an in-memory entry point purely
/// for the fuzzer would be a test-only hook in /src, which test.md P12 bans — so
/// the harness pays for one write per iteration instead. On tmpfs that is a few
/// microseconds and the reader still dominates the profile.
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
                       ("kentoscad-fuzz-proje-" + std::to_string(pid) + ".pcad");
        return p.string();
    }();
    return path;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    // A cap, not a correctness rule: the interesting bugs are in the header, the
    // directory and the cross-column indices, and all of them fit in far less
    // than this. Without it the fuzzer spends its budget writing megabytes.
    if (size > (1u << 20)) return 0;

    {
        std::ofstream out(scratch_path(), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }

    // A whole application per iteration, deliberately: the fuzzer then exercises
    // the real path a user takes — the AÇ command, on the bus, through the file
    // service — rather than a reader called in isolation. It also proves the
    // document is left clean after a rejected open, on every single input.
    kentos::core::Document doc;
    kentos::command::Registry registry;
    kentos::command::Journal journal;
    kentos::command::UndoStack undo;
    kentos::command::Bus bus{doc, registry, journal, undo};
    kentos::io::FileService files{bus};

    kentos::command::register_builtin_commands(registry);
    bus.on_echo = [](std::string_view) {};

    auto opened = bus.execute_line("AÇ \"" + scratch_path() + "\"", kentos::command::Origin::Test);
    if (!opened) {
        // A rejected file must leave nothing behind (io.md P11). If it ever does,
        // that is the bug, and it is worth aborting the run for.
        if (doc.live_entity_count() != 0 || doc.layers().size() != 1) __builtin_trap();
        return 0;
    }

    // Accepted: the document must be self-consistent. Every one of these walks a
    // column the file supplied, so a missed bounds check shows up here rather than
    // three frames later in the renderer.
    (void)doc.content_hash();
    (void)doc.extent();
    for (kentos::core::EntityId e = 0; e < doc.entities().size(); ++e) {
        (void)doc.entity_area(e);
        (void)doc.entity_perimeter(e);
        if (doc.entities().layer[e] >= doc.layers().size()) __builtin_trap();
        if (!doc.styles().contains(doc.entities().style[e])) __builtin_trap();
    }
    return 0;
}
