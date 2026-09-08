// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: the native project format, on the wire.
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

namespace kentos::io {

static_assert(std::endian::native == std::endian::little,
              "The native project format is little-endian and is mapped, not parsed "
              "(.claude/io.md R5). A big-endian port must add an explicit byte-swapping "
              "read path; it must NOT reinterpret the mapping in place.");

// ------------------------------------------------------------ identity -----

/// Eight bytes. The trailing 0x1A is the DOS end-of-file character: it stops a
/// `type project.pcad` from spewing the whole file at a terminal, which is the
/// same trick PNG uses for the same reason.
// THE SIGNATURE DOES NOT FOLLOW THE PRODUCT'S NAME, and must not. It is on disk
// in every file written so far; changing it would make each of them unreadable by
// the build that renamed it, which is a data loss dressed up as a rename. The
// program was called PiriCAD when the format shipped and the bytes remember that.
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

    /// The symbol behind each style id, and its stack of layers.
    ///
    /// Both OPTIONAL: a file written before symbols existed, or by a drawing where
    /// every style is a plain colour and width, carries neither and every style
    /// reads back as a single default layer. That is exactly what such a drawing
    /// meant, so an old file keeps its fingerprint.
    kBlkSymbols      = 0x0031, ///< SymbolRecord[], one per style id
    kBlkSymbolLayers = 0x0032, ///< SymbolLayerRecord[]

    /// The pictures the drawing carries, and their bytes.
    ///
    /// Both OPTIONAL and both absent from every file written before raster
    /// symbology existed. The COUNT comes from the block directory rather than
    /// from the document record, which has no reserved field left; the directory
    /// entry is the same authority that bounds the block, and `BlockView::column`
    /// already refuses a length that disagrees with count × stride.
    /// Where each layer sits in the layer tree, one string index per layer.
    ///
    /// A separate OPTIONAL block rather than a field on `LayerRecord`, because
    /// that record is exactly 64 bytes with nothing spare and growing it would
    /// make every file written so far unreadable — `BlockView::column` refuses a
    /// block whose element size disagrees with the type.
    kBlkLayerGroups = 0x0021, ///< u32[], index into kBlkStringSpans

    /// The full default symbol for each layer. OPTIONAL: absent from older files
    /// means every layer falls back to its Appearance, which is what they stored.
    kBlkLayerStyles = 0x0022, ///< StyleId[], one row per layer

    /// Whether each symbol layer is drawn, one byte per symbol layer.
    ///
    /// A separate OPTIONAL block for the same reason the layer tree is one:
    /// `SymbolLayerRecord` is exactly 64 bytes with nothing spare. A file written
    /// before the flag existed has no such block and every layer reads back
    /// enabled, which is what that file meant.
    kBlkSymbolLayerFlags = 0x0035, ///< u8[], bit 0 = drawn, bit 1 = colour locked

    /// What each `TextMarker` symbol layer writes, one string index per layer.
    ///
    /// Optional, like the flags beside it: a file written before fixed text
    /// existed has no such block and every layer reads back with none.
    kBlkSymbolLayerText = 0x0036, ///< u32[], index into kBlkStringSpans

    /// How far along the line each symbol layer's first marker sits, in the unit
    /// that layer's other measures are read in. `SymbolLayerRecord` is full to the
    /// byte, so this is its own OPTIONAL block: a file written before the phase
    /// existed has none and every layer reads back at zero, which is what that
    /// file meant (io.md R10).
    kBlkSymbolLayerPhase = 0x0038, ///< i32[], one per symbol layer

    /// The parameters each symbol layer takes from the OBJECT: which column, which
    /// property, and what the column holds.
    ///
    /// FOUR OPTIONAL BLOCKS, written only when some layer declares a parameter —
    /// the same bargain as the phase above them (io.md R10, and the reason adding
    /// a block is not a version bump). A file written before symbol parameters
    /// existed has none of them, and every layer reads back as a plain caption,
    /// which is what that file meant.
    ///
    /// A LIST PER LAYER, so `Count` says how many each one has and the other three
    /// run back to back across every layer in order — the same (first, count)
    /// shape the ring geometry uses, with the first derived by running total
    /// rather than stored, because a parameter list is read whole or not at all.
    kBlkSymbolLayerBindCount = 0x003B, ///< u16[], one per symbol layer
    kBlkSymbolLayerBindField = 0x003C, ///< u32[], index into kBlkStringSpans
    kBlkSymbolLayerBindWhat  = 0x003D, ///< u8[],  core::SymbolProperty
    kBlkSymbolLayerBindType  = 0x003E, ///< u8[],  core::AttrType

    kBlkImages     = 0x0033, ///< ImageRecord[]
    kBlkImageBytes = 0x0034, ///< u8[], the payloads back to back

    /// The line types the drawing carries; `AppearanceRecord::dash` indexes them.
    /// An OPTIONAL block, so a file written before line types existed reads with
    /// an empty store and every stroke in it stays solid — which is what it was
    /// (io.md R10, and the reason adding a block is not a version bump).
    kBlkDashes = 0x0037, ///< DashRecord[]

    /// The drafting guides (`core/guide.hpp`), as two parallel columns.
    ///
    /// OPTIONAL, like every block added after the format shipped: a file written
    /// before guides existed has neither and reads back with none, which is what
    /// that file meant (io.md R10). Two columns rather than a record, because a
    /// guide is exactly an axis and a coordinate and a struct would waste six
    /// bytes of padding on every one.
    ///
    /// The COUNT comes from the directory, as it does for images: `DocumentRecord`
    /// has no reserved field left, and `BlockView::column` already refuses a
    /// length that disagrees with count × stride.
    kBlkGuideAxis  = 0x0039, ///< u8[],  0 horizontal, 1 vertical
    kBlkGuideCoord = 0x003A, ///< i64[], Mm — northing for horizontal, easting for vertical

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

    // ---- attributes (model.md R27-R29) --------------------------------------
    /// The declared schema: one record per column, in declaration order. The
    /// order IS the AttrId, so it is written and read as a sequence rather than
    /// keyed — an id that moved would repoint every cell in the file.
    kBlkAttrSchema = 0x0072, ///< AttrColumnRecord[]
    /// Cells, one record per non-absent value. Absent cells are NOT written: a
    /// cadastral layer is mostly empty columns, and a record per empty cell would
    /// be the largest block in the file for no information.
    kBlkAttrCells = 0x0073, ///< AttrCellRecord[]

    /// The layer each column is scoped to, one string index per column, in the
    /// same order as `kBlkAttrSchema`. Index 0 — the empty string — means the
    /// column belongs to the whole project.
    ///
    /// ITS OWN BLOCK RATHER THAN A FIELD, and this is what the optional-block
    /// mechanism is for (io.md R10). `AttrColumnRecord` has five reserved bytes
    /// left, which is one short of a properly aligned `u32`, and reordering the
    /// record would repoint every schema row in every file already written. A
    /// file with no such block reads as every column being the project's —
    /// exactly what those files meant, because that was the only kind there was.
    kBlkAttrColumnLayer = 0x0075, ///< u32[], index into kBlkStringSpans

    // ---- text (a caption is geometry-adjacent, not an attribute) ------------
    /// One record per slot that carries text. Same reasoning as the cells above.
    kBlkTexts = 0x0074, ///< TextRecord[]

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

/// Where one interned string lives inside the string-bytes block.
///
/// Offset and length rather than a NUL terminator, because the block is MAPPED
/// and a reader must be able to hand out a `string_view` into it without scanning
/// for an end that a corrupt file may not contain.
struct StringSpan
{
    std::uint64_t offset; ///< into kBlkStringBytes
    std::uint64_t bytes;  ///< length in bytes, not characters: the pool is UTF-8
};

static_assert(sizeof(StringSpan) == 16, "wire record");

/// The interned appearance, field by field. Never a memcpy of `core::Appearance`:
/// its padding bytes are indeterminate, and a file whose bytes depend on padding
/// is a file that differs between compilers (model.md, core.md R9).
struct AppearanceRecord
{
    std::uint32_t rgba;      ///< stroke colour, 0xAARRGGBB
    std::int32_t width_um;   ///< PAPER micrometres, never pixels (model.md R20/P9)
    std::uint16_t dash;      ///< index into the dash table, which lives in /data
    std::uint16_t symbol;    ///< index into the symbol atlas, also /data
    std::uint32_t fill_rgba; ///< 0 means no fill, which is not the same as clear
    std::uint16_t hatch;     ///< index into the hatch table
    std::int16_t z_order;    ///< draw order, which MPYY prescribes for plan sheets

    /// Where each property gets its value: explicit, ByLayer or ByBlock
    /// (`core::Source`). Stored as bytes rather than as the enum so the file does
    /// not depend on the enum's underlying type.
    std::uint8_t src_colour;
    std::uint8_t src_width;
    std::uint8_t src_dash;
    std::uint8_t src_fill;

    /// Padding to a round size, zero-filled on write. Reserved bytes are how a
    /// later version adds a field without moving every record that follows.
    std::uint8_t reserved[8];
};

static_assert(sizeof(AppearanceRecord) == 32, "wire record");

/// One interned symbol: where its layers live and the scales it draws at.
struct SymbolRecord
{
    std::uint32_t first_layer; ///< index into kBlkSymbolLayers
    std::uint32_t layer_count; ///< 0 means this style is a bare appearance
    std::uint32_t min_scale;   ///< 1:N denominator; 0 = unbounded
    std::uint32_t max_scale;   ///< 1:N denominator; 0 = unbounded
};

static_assert(sizeof(SymbolRecord) == 16, "wire record");

/// One embedded picture: where its bytes are, what they encode, and where the
/// picture was published.
///
/// The bytes are in a separate block rather than inline, for the reason every
/// column in this format is separate: a reader that wants the record table does
/// not have to walk megabytes of JPEG to find the next record.
/// One line type: its segment lengths and where it was published.
///
/// Fixed width rather than a length-prefixed run, because `core::kMaxDashSegments`
/// caps a pattern at eight and a fixed record needs no second block to index into
/// — the same trade `AppearanceRecord` makes for its own small fields.
struct DashRecord
{
    std::uint16_t lengths[8]; ///< hundredths of a stroke width, mark first
    std::uint32_t origin;     ///< index into kBlkStringSpans — provenance (model.md R35)
    std::uint8_t count;       ///< meaningful entries in `lengths`; always even

    /// Padding to a round size, zero-filled on write.
    std::uint8_t reserved[3];
};

struct ImageRecord
{
    std::uint64_t offset; ///< into kBlkImageBytes
    std::uint64_t bytes;  ///< payload length; bounded against the block, not this number
    std::uint32_t origin; ///< index into kBlkStringSpans — provenance (model.md R35)
    std::uint8_t format;  ///< core::ImageFormat

    /// Padding to a round size, zero-filled on write.
    std::uint8_t reserved[3];
};

static_assert(sizeof(ImageRecord) == 24, "wire record");
static_assert(sizeof(DashRecord) == 24, "wire record");

/// One layer of a symbol, field by field.
///
/// Laid out four-byte fields first and single bytes after, so the record has NO
/// padding on any platform. A record whose size depended on the compiler's
/// padding choices is a file that differs between compilers (core.md R9), and
/// with a `static_assert` on the size that is checked rather than hoped for.
///
/// Each measure is a value and a UNIT, stored apart: two millimetres on paper and
/// two millimetres on the ground are different symbols, and at 1/1000 they differ
/// by a factor of a thousand.
struct SymbolLayerRecord
{
    AppearanceRecord look;        ///< the colours and widths this layer draws with
    std::int32_t offset_value;    ///< perpendicular offset from the geometry
    std::int32_t size_value;      ///< marker diameter, or hash tick length
    std::int32_t interval_value;  ///< spacing along a line, or the first pattern axis
    std::int32_t spacing_y_value; ///< the second pattern axis; 0 means square
    std::int32_t angle_udeg;      ///< pattern angle or glyph rotation, micro-degrees

    std::uint8_t offset_unit;    ///< core::Unit for offset_value
    std::uint8_t size_unit;      ///< core::Unit for size_value
    std::uint8_t interval_unit;  ///< core::Unit for interval_value
    std::uint8_t spacing_y_unit; ///< core::Unit for spacing_y_value
    std::uint8_t type;           ///< core::SymbolLayerType
    std::uint8_t shape;          ///< core::MarkerShape
    std::uint8_t placement;      ///< core::MarkerPlacement
    std::uint8_t cap;            ///< core::LineCap
    std::uint8_t join;           ///< core::LineJoin
    std::uint8_t opacity;        ///< 0 transparent to 255 opaque

    /// The picture a raster type draws, as an index into kBlkImages. Zero for
    /// every other type. This WAS the two reserved bytes, and it reads as zero in
    /// every file written before raster symbology existed — which is the right
    /// answer for those files.
    std::uint16_t image;
};

static_assert(sizeof(SymbolLayerRecord) == 64, "wire record");

/// model.md R32, every stored field of a layer record. `folded` is deliberately
/// absent: it is `turkish_upper(name)` and is recomputed on load, so a file can
/// never disagree with the running build's folding table (io.md P16).
struct LayerRecord
{
    std::uint64_t key;                ///< LayerKey — persistent, never a slot (R1/P4)
    std::uint32_t name_string;        ///< index into kBlkStringSpans
    std::uint32_t description_string; ///< index into kBlkStringSpans; may be empty
    std::uint32_t catalog_ref_string; ///< which catalogue row this layer follows (R34)
    std::uint8_t visible;             ///< drawn at all
    std::uint8_t locked;              ///< editable
    std::uint8_t plottable;           ///< printed; a guide layer is visible and not plotted
    std::uint8_t opacity;             ///< 0 transparent to 255 opaque
    std::uint32_t min_scale;          ///< 1:N denominator; 0 = unbounded
    std::uint32_t max_scale;          ///< 1:N denominator; 0 = unbounded
    AppearanceRecord appearance;      ///< the layer default the ByLayer cascade resolves to
};

static_assert(sizeof(LayerRecord) == 64, "wire record");

/// One project-scope setting. Fixed width, so the block is a mappable column and
/// so a Text setting cannot make the record length depend on its content.
struct SettingRecord
{
    std::uint32_t id_string;  ///< the setting's stable id, into the string pool
    std::uint8_t type;        ///< core::SettingType
    std::uint8_t reserved[3]; ///< alignment, zero-filled
    std::int64_t scalar;      ///< Bool / Int / Length / Enum
    char text[48];            ///< core::kSettingTextCapacity, NOT NUL-terminated
};

static_assert(sizeof(SettingRecord) == 64, "wire record");

/// One declared attribute column. The catalogue name is present even for a
/// non-CodeRef column so that a package reference survives a round trip through a
/// build that does not hold that catalogue (model.md R34).
struct AttrColumnRecord
{
    std::uint32_t id_string;      ///< column id, into the string pool
    std::uint32_t name_string;    ///< Turkish display name
    std::uint32_t summary_string; ///< one-line description
    std::uint32_t catalog_string; ///< catalogue id, or 0
    std::uint8_t type;            ///< core::AttrType
    std::uint8_t required;        ///< 1 when every row must carry a value

    /// Digits after the point, for a `Decimal` column; zero for every other type.
    ///
    /// TAKEN FROM THE RESERVED BYTES, which is what they were left for: the
    /// record keeps its size, its offsets and its block id, and a file written
    /// before this field existed reads back as scale 0 — which is exactly right,
    /// because every type that file could hold has no fraction (io.md R10).
    std::uint8_t scale;
    std::uint8_t reserved[5]; ///< alignment, zero-filled
};

static_assert(sizeof(AttrColumnRecord) == 24, "wire record");

/// One cell. `column` is the index into the schema block, `row` is the entity
/// SLOT — the same index geometry uses, so a reader does not have to resolve a
/// key to place a value.
struct AttrCellRecord
{
    std::uint32_t column;      ///< index into the schema block, in file order
    std::uint32_t row;         ///< entity slot, the same index geometry uses
    std::int64_t number;       ///< Int64 / Length / Bool
    std::uint32_t text_string; ///< Text / CodeRef, into the pool; 0 otherwise
    std::uint32_t reserved;    ///< alignment, zero-filled
};

static_assert(sizeof(AttrCellRecord) == 24, "wire record");

/// One caption. Height is ground millimetres, like every other length in the
/// file; the baseline lives in the ordinary geometry blocks.
struct TextRecord
{
    std::uint32_t row;            ///< entity slot
    std::uint32_t content_string; ///< the caption itself, into the string pool
    std::int64_t height_mm;       ///< GROUND millimetres, like every other length
    std::uint8_t anchor;          ///< core::TextAnchor
    std::uint8_t reserved[7];     ///< alignment, zero-filled
};

static_assert(sizeof(TextRecord) == 24, "wire record");

/// The one document-level record. Counts here are cross-checked against the
/// directory: a column whose length disagrees with this record is a corrupt file,
/// not a hint to be trusted (R18, P6).
struct DocumentRecord
{
    std::uint32_t crs_string;     ///< core::Crs::id(), never empty (io.md R20)
    std::uint32_t catalog_string; ///< catalogue package version (model.md R34/R35)

    /// Where the key allocators stood when the file was written. Restored on load
    /// so a reopened drawing cannot hand a NEW entity a key a dead one already
    /// used — which would make the journal's history ambiguous (model.md R4).
    std::uint64_t next_entity_key;
    std::uint64_t next_layer_key;

    /// Row counts, cross-checked against the directory on load. A column whose
    /// length disagrees with these is a corrupt file rather than a hint.
    std::uint64_t entity_count;
    std::uint64_t layer_count;
    std::uint64_t style_count;
    std::uint64_t slot_count;
    std::uint64_t ring_count;
    std::uint64_t vertex_count;

    /// Rows in kBlkSymbolLayers. There is no separate symbol count: a symbol
    /// belongs to exactly one style id, so kBlkSymbols has `style_count` rows.
    ///
    /// This field WAS the reserved u64, and it reads as zero in every file written
    /// before symbols were persisted — which is the right answer for those files,
    /// because they hold no stacks.
    std::uint64_t symbol_layer_count;
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

} // namespace kentos::io
