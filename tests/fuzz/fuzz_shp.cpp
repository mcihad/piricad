// SPDX-License-Identifier: GPL-3.0-or-later
//
// libFuzzer harness for the Shapefile import path (GDAL/OGR + the KentOSCad
// wrapper).
//
// `.claude/io.md` R19 and the allow-list in `cmake/KentOSCadGdalDrivers.cmake`
// both require a driver to arrive with a harness. GDAL's own shapefile parser is
// fuzzed upstream by OSS-Fuzz; what is not fuzzed anywhere else is the seam this
// project added — the geometry conversion to `Mm`, the ring role assignment, the
// `.prj` sidecar reader and the transaction rollback. A crash found here is ours
// until proven otherwise.
//
// A SHAPEFILE IS FOUR FILES, and the fuzzer produces one buffer. Handing GDAL a
// lone `.shp` means it refuses at open and nothing past the opener is ever
// reached, so the harness writes the companions itself: a `.shx` whose single
// record spans the whole fuzzed file, and an empty `.dbf`. Both are minimal and
// both are DELIBERATELY consistent with a well-formed file rather than with the
// fuzzed one — an index that disagrees with the geometry it points at is exactly
// the input worth reaching the parser.
//
// Build:
//   cmake --preset dev -DKENTOS_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
//   ./build/dev/bin/kentos_fuzz_shp tests/fuzz/tohum/shp -max_total_time=300
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/io/service.hpp"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

/// The scratch set's base path, without an extension.
///
/// The pid is in the name because libFuzzer's `-jobs=N` forks: two workers
/// sharing one scratch file would fuzz each other's input and report a crash
/// nobody can reproduce.
const std::string& scratch_base()
{
    static const std::string path = [] {
#ifdef _WIN32
        const auto pid = ::_getpid();
#else
        const auto pid = ::getpid();
#endif
        const auto p =
            std::filesystem::temp_directory_path() / ("kentoscad-fuzz-shp-" + std::to_string(pid));
        return p.string();
    }();
    return path;
}

void put_be32(std::ofstream& out, std::uint32_t v)
{
    const std::array<char, 4> bytes{
        static_cast<char>((v >> 24) & 0xFF), static_cast<char>((v >> 16) & 0xFF),
        static_cast<char>((v >> 8) & 0xFF), static_cast<char>(v & 0xFF)};
    out.write(bytes.data(), 4);
}

/// A 100-byte shapefile index whose one record covers the whole `.shp`.
///
/// Big-endian, 16-bit words, exactly as the specification writes it: file code
/// 9994, the file length in WORDS at offset 24, version 1000 and the shape type
/// little-endian from offset 28.
void write_shx(const std::string& base, std::size_t shp_bytes)
{
    std::ofstream out(base + ".shx", std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return;

    put_be32(out, 9994);
    for (int i = 0; i < 5; ++i)
        put_be32(out, 0);
    put_be32(out, (100 + 8) / 2); // this index is one record long

    const std::array<char, 72> tail{}; // version, shape type, bbox: zeroed
    out.write(tail.data(), static_cast<std::streamsize>(tail.size()));

    put_be32(out, 100 / 2);                                   // record offset, in words
    put_be32(out, static_cast<std::uint32_t>(shp_bytes / 2)); // and its length
}

/// A dBASE III header with no fields and no rows.
void write_dbf(const std::string& base)
{
    std::ofstream out(base + ".dbf", std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return;

    std::array<char, 33> header{};
    header[0]  = 0x03; // dBASE III, no memo
    header[8]  = 33;   // header length, little-endian
    header[10] = 1;    // record length: the deletion flag alone
    header[32] = 0x0D; // the field descriptor terminator
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size > (1u << 20)) return 0;

    const std::string& base = scratch_base();
    {
        std::ofstream out(base + ".shp", std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    write_shx(base, size);
    write_dbf(base);

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
        bus.execute_line("İÇEAKTAR \"" + base + ".shp\"", kentos::command::Origin::Test);
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
