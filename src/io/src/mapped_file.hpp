// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io (internal): a read-only memory mapping of a file.
//
// .claude/io.md R5 requires the native project format to be "usable by mmap with
// zero parsing of geometry blocks". That is only true if something actually maps
// the file, so this is that something.
//
// ARTICLE 2.7 / 5.16 — WHY THIS IS HAND-ROLLED, and what would replace it.
//
// The library-first rule says never to reimplement a solved problem. What is
// below is not an algorithm; it is two operating-system calls (`mmap` on POSIX,
// `CreateFileMapping` + `MapViewOfFile` on Windows) behind an RAII handle, and
// every candidate library — mio, Boost.Interprocess, llfio — is a wrapper around
// exactly these calls. mio (MIT, header-only, cross-platform) is the right
// dependency to take, and taking it means the full Article 9 ceremony: read the
// LICENSE, pin it in /vcpkg.json against the baseline, gate it behind
// PIRICAD_WITH_MIO, record it in /NOTICE and regenerate the SBOM. That is a
// deliberate, reviewable change; it is not something to slip in alongside a file
// format. Until it happens this stays sixty lines with no allocation, no state
// and no third-party header, and the swap to mio is a one-file replacement
// because nothing outside this header knows how the bytes arrived.
//
// This header is NOT public: /src/io ships no operating-system type across its
// module boundary (io.md R1, R2).
#pragma once

#include "piricad/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace piricad::io {

/// A read-only mapping of a whole file. Move-only; the mapping lives exactly as
/// long as the object, so a span handed out from `bytes()` is valid for that long
/// and not one instruction longer.
class MappedFile
{
public:
    MappedFile() = default;
    ~MappedFile();

    MappedFile(MappedFile&& o) noexcept;
    MappedFile& operator=(MappedFile&& o) noexcept;

    MappedFile(const MappedFile&)            = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    /// Maps `path` read-only.
    ///
    /// An empty file is rejected here rather than mapped: `mmap` with length 0 is
    /// an error on POSIX and a zero-size mapping is an error on Windows, and a
    /// caller that got an empty span back would report "bad magic" for what is
    /// really "the file is empty".
    static core::Result<MappedFile> open(const std::string& path);

    std::span<const std::byte> bytes() const noexcept
    {
        return {static_cast<const std::byte*>(data_), size_};
    }

    std::size_t size() const noexcept { return size_; }

    bool valid() const noexcept { return data_ != nullptr; }

private:
    void close() noexcept;

    const void* data_{nullptr};
    std::size_t size_{0};

    // Kept as an integer rather than a HANDLE/void* so this header stays free of
    // <windows.h>. -1 is "none" on both platforms.
    std::intptr_t handle_{-1};
    std::intptr_t mapping_{-1}; ///< Windows only; unused on POSIX
};

} // namespace piricad::io
