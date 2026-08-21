// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: the native project format, on the wire.
//
// This header is the format specification in code. The user-facing description
// of the same bytes is /docs/veri/proje-dosyasi.md, and the two must agree.
//
// .claude/io.md R5, R7, R8, R10:
//
//   R5  columnar (SoA blocks mirroring the Document store), 8-byte aligned,
//       offset-addressed with u64, usable by mmap with zero parsing of the
//       geometry blocks.
//   R7  every coordinate is `Mm` — int64 fixed-point millimetres. The file
//       contains no floating-point geometry, and no floating-point ANYTHING
//       (model.md R21/P8).
//   R8  magic + u32 format_version + u32 min_reader_version inside the first
//       32 bytes.
//   R10 an unknown block is skipped by its declared length and is never fatal;
//       adding an optional block never raises min_reader_version.
//
// LAYOUT
//
//   [ 0                 ) FileHeader, 64 bytes
//   [ 64                ) block payloads, each 8-byte aligned, in id order
//   [ directory_offset  ) BlockEntry[block_count], 32 bytes each
//
// The directory sits at the END so the writer can stream the payloads without
// knowing their sizes in advance, and the reader still finds it in one seek.
//
// BYTE ORDER. Every field is little-endian, which is also the native order of
// all three supported platforms (x86-64 Linux, x86-64 Windows, arm64 macOS), so
// a column can be handed to the caller as a `std::span` over the mapping with no
// byte swapping and no copy — which is what R5 means by "zero parsing". A
// big-endian host is refused at compile time rather than silently mis-reading.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace piricad::io {

static_assert(std::endian::native == std::endian::little,
              "The native project format is little-endian and is mapped, not parsed "
              "(.claude/io.md R5). A big-endian port must add an explicit byte-swapping "
              "read path; it must NOT reinterpret the mapping in place.");

// ------------------------------------------------------------ identity -----

/// Eight bytes. The trailing 0x1A is the DOS end-of-file character: it stops a
/// `type project.pcad` from spewing the whole file at a terminal, which is the
/// same trick PNG uses for the same reason.
inline constexpr char kMagic[8] = {'P', 'I', 'R', 'I', 'C', 'A', 'D', '\x1A'};

/// The extension the writer produces and the file dialogs filter on.
inline constexpr const char* kProjectExtension = ".pcad";

/// The version this build WRITES.
///
/// Bumped when the meaning of an existing block changes. Adding a new optional
/// block is NOT a version bump, because R10 makes an unknown block skippable.
inline constexpr std::uint32_t kFormatVersion = 1;

/// The lowest reader version that can still make sense of what this build wrote.
///
/// A file is refused when its `min_reader_version` exceeds `kFormatVersion`:
/// that is the file saying "you would misread me". It stays at 1 for as long as
/// every added block is optional (R10).
inline constexpr std::uint32_t kMinReaderVersion = 1;

/// Stable error tokens. `core::Error` carries an `ErrorCode` enum rather than the
/// string code io.md R9 writes, so the token is placed at the FRONT of the
/// message: it stays greppable, testable and stable across translations while the
/// rest of the sentence stays Turkish and actionable.
inline constexpr const char* kErrTooNew    = "io.format_too_new";
inline constexpr const char* kErrNotPiri   = "io.not_a_project";
inline constexpr const char* kErrTruncated = "io.truncated";
inline constexpr const char* kErrBlock     = "io.bad_block";
inline constexpr const char* kErrConsist   = "io.inconsistent";
inline constexpr const char* kErrKey       = "io.key_mismatch";
inline constexpr const char* kErrKind      = "io.unknown_kind";
inline constexpr const char* kErrNoDriver  = "io.no_driver";

// -------------------------------------------------------------- header -----

/// 64 bytes, at offset 0. Magic and both version fields land inside the first 32
/// bytes exactly as R8 requires.
///
/// Every count and offset in this record is a HINT FROM AN UNTRUSTED FILE and is
/// bounds-checked against the real file size before it is used (R18, P6).
struct FileHeader
{
    char magic[8];                    ///<  0  kMagic
    std::uint32_t format_version;     ///<  8  the version that wrote this file
    std::uint32_t min_reader_version; ///< 12  refuse if this exceeds kFormatVersion
    std::uint32_t header_bytes;       ///< 16  sizeof(FileHeader); lets the header grow
    std::uint32_t block_count;        ///< 20  directory entries
    std::uint64_t directory_offset;   ///< 24  absolute, 8-byte aligned
    std::uint64_t file_bytes;         ///< 32  declared total size; must equal the real one
    std::uint64_t content_hash;       ///< 40  Document::content_hash() when written
    std::uint64_t settings_hash;      ///< 48  project-scope Settings::fold() when written
    std::uint64_t reserved;           ///< 56  zero
};

static_assert(sizeof(FileHeader) == 64, "the header is a wire record, not a C++ struct");
static_assert(offsetof(FileHeader, min_reader_version) + sizeof(std::uint32_t) <= 32,
              "io.md R8: magic and both version fields live in the first 32 bytes");

// ----------------------------------------------------------- directory -----

/// One 32-byte directory entry. `elem_bytes == 0` marks an opaque byte blob
/// (the string pool); otherwise `bytes == count * elem_bytes` is enforced, which
/// is what makes a column mappable as a typed span without further arithmetic.
struct BlockEntry
{
    std::uint32_t id;         ///<  0  BlockId
    std::uint32_t elem_bytes; ///<  4  stride, or 0 for an opaque blob
    std::uint64_t offset;     ///<  8  absolute, 8-byte aligned
    std::uint64_t bytes;      ///< 16  payload length
    std::uint64_t count;      ///< 24  element count
};

static_assert(sizeof(BlockEntry) == 32, "the directory is a wire record");

/// Block identities. STABLE FOREVER: an id that reached a written file is never
/// re-meant, exactly as an entity kind id is never re-meant (model.md R26).
enum BlockId : std::uint32_t {
    // ---- strings -----------------------------------------------------------
    kBlkStringBytes = 0x0001, ///< u8[]  UTF-8 pool, not NUL-terminated
    kBlkStringSpans = 0x0002, ///< StringSpan[]

    // ---- document-level ----------------------------------------------------
    kBlkDocument = 0x0010, ///< exactly one DocumentRecord
    kBlkLayers   = 0x0020, ///< LayerRecord[]
    kBlkStyles   = 0x0030, ///< AppearanceRecord[]

    // ---- entity table, one block per column (model.md R6 cull block first) --
    kBlkEntityMinX  = 0x0040, ///< i64[]
    kBlkEntityMinY  = 0x0041, ///< i64[]
    kBlkEntityMaxX  = 0x0042, ///< i64[]
    kBlkEntityMaxY  = 0x0043, ///< i64[]
    kBlkEntityFlags = 0x0044, ///< u8[]
    kBlkEntityLayer = 0x0045, ///< u32[]
    kBlkEntityStyle = 0x0046, ///< u32[]
    kBlkEntityKind  = 0x0047, ///< u16[]
    kBlkEntitySlot  = 0x0048, ///< u32[]
    kBlkEntityKey   = 0x0049, ///< u64[] persistent keys (model.md R1, R4)

    // ---- ring geometry -----------------------------------------------------
    kBlkRingStart     = 0x0050, ///< u32[]
    kBlkRingCount     = 0x0051, ///< u32[]
    kBlkRingPart      = 0x0052, ///< u16[]
    kBlkRingRole      = 0x0053, ///< u8[]
    kBlkSlotFirstRing = 0x0054, ///< u32[]
    kBlkSlotRingTotal = 0x0055, ///< u32[]
    kBlkVertexX       = 0x0060, ///< i64[]  Mm, never a double (R7, P9)
    kBlkVertexY       = 0x0061, ///< i64[]

    // ---- settings ----------------------------------------------------------
    /// PROJECT-scope settings only. model.md R39: they travel with the document
    /// and are part of its fingerprint; App and Session scopes are per user and
    /// per run and MUST NOT reach the file.
    kBlkSettings = 0x0070, ///< SettingRecord[]

    // ---- reserved. Declared here so the ids can never be re-meant. ----------
    /// Payload of an entity kind this build does not understand, kept so that
    /// model.md R26 ("preserved, non-editable, byte-identical round trip") can be
    /// honoured without a format change. Phase 1.
    kBlkKindPayload = 0x0080,
    /// Precomputed Douglas–Peucker LOD levels in quadtree tiles (io.md R6).
    /// Phase 1: no simplifier exists yet. See CLAUDE.md Article 8.
    kBlkLodTiles = 0x0090,
    /// Bulk-loaded STR R-tree (io.md R6). Phase 1: `core::Document` owns its
    /// index privately and has no way to accept a prebuilt one, so writing it
    /// today would produce a block nothing could install.
    kBlkRtree = 0x0091,
};

// ------------------------------------------------------- block records -----

struct StringSpan
{
    std::uint64_t offset; ///< into kBlkStringBytes
    std::uint64_t bytes;
};

static_assert(sizeof(StringSpan) == 16, "wire record");

/// The interned appearance, field by field. Never a memcpy of `core::Appearance`:
/// its padding bytes are indeterminate, and a file whose bytes depend on padding
/// is a file that differs between compilers (model.md, core.md R9).
struct AppearanceRecord
{
    std::uint32_t rgba;
    std::int32_t width_um; ///< PAPER micrometres, never pixels (model.md R20/P9)
    std::uint16_t dash;
    std::uint16_t symbol;
    std::uint32_t fill_rgba;
    std::uint16_t hatch;
    std::int16_t z_order;
    std::uint8_t src_colour;
    std::uint8_t src_width;
    std::uint8_t src_dash;
    std::uint8_t src_fill;
    std::uint8_t reserved[8];
};

static_assert(sizeof(AppearanceRecord) == 32, "wire record");

/// model.md R32, every stored field of a layer record. `folded` is deliberately
/// absent: it is `turkish_upper(name)` and is recomputed on load, so a file can
/// never disagree with the running build's folding table (io.md P16).
struct LayerRecord
{
    std::uint64_t key; ///< LayerKey — persistent, never a slot (model.md R1/P4)
    std::uint32_t name_string;
    std::uint32_t description_string;
    std::uint32_t catalog_ref_string;
    std::uint8_t visible;
    std::uint8_t locked;
    std::uint8_t plottable;
    std::uint8_t opacity;
    std::uint32_t min_scale;
    std::uint32_t max_scale;
    AppearanceRecord appearance;
};

static_assert(sizeof(LayerRecord) == 64, "wire record");

/// One project-scope setting. Fixed width, so the block is a mappable column and
/// so a Text setting cannot make the record length depend on its content.
struct SettingRecord
{
    std::uint32_t id_string;
    std::uint8_t type; ///< core::SettingType
    std::uint8_t reserved[3];
    std::int64_t scalar; ///< Bool / Int / Length / Enum
    char text[48];       ///< core::kSettingTextCapacity
};

static_assert(sizeof(SettingRecord) == 64, "wire record");

/// The one document-level record. Counts here are cross-checked against the
/// directory: a column whose length disagrees with this record is a corrupt file,
/// not a hint to be trusted (R18, P6).
struct DocumentRecord
{
    std::uint32_t crs_string;     ///< core::Crs::id(), never empty (io.md R20)
    std::uint32_t catalog_string; ///< catalogue package version (model.md R34/R35)
    std::uint64_t next_entity_key;
    std::uint64_t next_layer_key;
    std::uint64_t entity_count;
    std::uint64_t layer_count;
    std::uint64_t style_count;
    std::uint64_t slot_count;
    std::uint64_t ring_count;
    std::uint64_t vertex_count;
    std::uint64_t reserved;
};

static_assert(sizeof(DocumentRecord) == 80, "wire record");

// --------------------------------------------------------------- limits ----

/// Absolute ceilings applied before the file-size bound, so a header that claims
/// four billion blocks is rejected on the first arithmetic rather than after it.
inline constexpr std::uint32_t kMaxBlocks   = 4096;
inline constexpr std::uint64_t kMaxFileSize = std::uint64_t{1} << 42; ///< 4 TiB

/// Payload alignment. Every block starts here, which is what lets a column be
/// handed out as a typed span over the mapping (R5).
inline constexpr std::uint64_t kAlignment = 8;

constexpr std::uint64_t align_up(std::uint64_t v) noexcept
{
    return (v + (kAlignment - 1)) & ~(kAlignment - 1);
}

} // namespace piricad::io
