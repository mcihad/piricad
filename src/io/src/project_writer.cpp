// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: writing the native project file.
//
// The writer is deliberately dull. It walks the document's columns in slot order
// and streams them out; there is no reordering, no compaction and no cleverness,
// because every one of those would change which key belongs to which parcel and
// model.md R4 makes that question a legal one.
//
// WHAT TRAVELS, and why each thing does:
//
//   CRS + catalogue package version  model.md R35 — a document whose regulatory
//                                    basis is unknown must not open silently.
//   entity and layer KEYS            model.md R1/R4 — a key survives save, load,
//                                    reorder and compaction, and is never reused.
//   dead entity rows                 an erased entity keeps its row and its key.
//                                    Dropping the row would compact the key space
//                                    and hand a retired key to a different parcel.
//   the bbox columns                 model.md R6 — the cull block is mapped, not
//                                    recomputed, so the first frame after an open
//                                    costs what every other frame costs.
//   project-scope settings           model.md R39 — they are part of the document
//                                    and of its fingerprint. App and Session
//                                    scopes are per user and per run and MUST NOT
//                                    reach the file.
#include "piricad/io/project.hpp"

#include "piricad/core/text.hpp"
#include "piricad/io/format.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace piricad::io {
namespace {

using core::err;
using core::ErrorCode;

namespace fs = std::filesystem;

/// Deduplicating UTF-8 pool. Layer names repeat, catalogue references repeat, and
/// the empty string repeats once per layer that has no description — interning
/// them costs one hash lookup per write and keeps the pool honest.
class StringPool
{
public:
    StringPool()
    {
        // Index 0 is always the empty string, so "no description" and "no
        // catalogue reference" cost four bytes and no pool entry.
        intern(std::string_view{});
    }

    std::uint32_t intern(std::string_view s)
    {
        const std::string key(s);
        if (const auto it = index_.find(key); it != index_.end()) return it->second;

        const auto id = static_cast<std::uint32_t>(spans_.size());
        StringSpan span{static_cast<std::uint64_t>(bytes_.size()),
                        static_cast<std::uint64_t>(s.size())};
        bytes_.insert(bytes_.end(), s.begin(), s.end());
        spans_.push_back(span);
        index_.emplace(key, id);
        return id;
    }

    const std::vector<char>& bytes() const noexcept { return bytes_; }

    const std::vector<StringSpan>& spans() const noexcept { return spans_; }

private:
    std::vector<char> bytes_;
    std::vector<StringSpan> spans_;
    std::unordered_map<std::string, std::uint32_t> index_;
};

/// One block waiting to be written: either a view onto memory the document
/// already owns (the columns — never copied) or a buffer the writer built.
struct Pending
{
    std::uint32_t id{0};
    std::uint32_t elem_bytes{0};
    const void* data{nullptr};
    std::uint64_t bytes{0};
    std::uint64_t count{0};
};

template<class T> Pending column(BlockId id, const std::vector<T>& v)
{
    Pending p;
    p.id         = static_cast<std::uint32_t>(id);
    p.elem_bytes = static_cast<std::uint32_t>(sizeof(T));
    p.data       = v.data();
    p.count      = static_cast<std::uint64_t>(v.size());
    p.bytes      = p.count * p.elem_bytes;
    return p;
}

AppearanceRecord to_record(const core::Appearance& a)
{
    AppearanceRecord r{};
    r.rgba       = a.rgba;
    r.width_um   = a.width_um;
    r.dash       = a.dash;
    r.symbol     = a.symbol;
    r.fill_rgba  = a.fill_rgba;
    r.hatch      = a.hatch;
    r.z_order    = a.z_order;
    r.src_colour = static_cast<std::uint8_t>(a.src_colour);
    r.src_width  = static_cast<std::uint8_t>(a.src_width);
    r.src_dash   = static_cast<std::uint8_t>(a.src_dash);
    r.src_fill   = static_cast<std::uint8_t>(a.src_fill);
    return r;
}

/// The catalogue package version stamp (model.md R34, R35).
///
/// It is a project-scope setting, so it already travels in the settings block;
/// it is ALSO lifted into the document record because R35 calls it critical, and
/// a critical field that can only be found by scanning an optional block is a
/// field a future reader can miss.
std::string catalogue_stamp(const core::Settings& settings)
{
    const std::uint32_t idx = settings.catalogue().find("core.katalog.paket_surumu");
    if (idx == core::kNoSetting) return {};
    return std::string(settings.get("core.katalog.paket_surumu").as_text());
}

/// Writes to a sibling temporary and renames it into place.
///
/// A project file is somebody's day of work and quite often a legal document. A
/// crash, a full disk or a pulled cable half-way through a plain overwrite leaves
/// neither the old file nor the new one; a rename is atomic on every filesystem
/// this product targets, so the worst case becomes "the previous save survives".
class AtomicFile
{
public:
    core::Status open(const fs::path& target)
    {
        target_ = target;
        temp_   = target;
        temp_ += ".yeni";

        std::error_code ec;
        if (target.has_parent_path() && !fs::exists(target.parent_path(), ec))
            return err(ErrorCode::IoFailure,
                       "'" + target.parent_path().string() +
                           "' dizini yok. Önce dizini oluşturun ya da başka bir yol seçin.");

        out_.open(temp_, std::ios::binary | std::ios::trunc);
        if (!out_)
            return err(ErrorCode::IoFailure,
                       "'" + temp_.string() +
                           "' yazmak için açılamadı. Dizin izinlerini ve boş alanı denetleyin.");
        return core::ok();
    }

    void write(const void* data, std::uint64_t bytes)
    {
        if (bytes == 0) return;
        out_.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
    }

    void pad(std::uint64_t bytes)
    {
        static constexpr char kZero[kAlignment] = {};
        if (bytes != 0) out_.write(kZero, static_cast<std::streamsize>(bytes));
    }

    bool bad() const { return !out_; }

    /// Closes, flushes and renames. A failure anywhere here leaves the previous
    /// file untouched and removes the temporary, so a failed save never costs the
    /// user the save before it.
    core::Status commit()
    {
        out_.flush();
        const bool ok_stream = static_cast<bool>(out_);
        out_.close();

        std::error_code ec;
        if (!ok_stream) {
            fs::remove(temp_, ec);
            return err(ErrorCode::IoFailure,
                       "'" + target_.string() +
                           "' yazılırken hata oluştu; disk dolu olabilir. Önceki dosya "
                           "değiştirilmedi.");
        }

        fs::rename(temp_, target_, ec);
        if (ec) {
            fs::remove(temp_, ec);
            return err(ErrorCode::IoFailure, "'" + target_.string() + "' yerine konamadı: " +
                                                 ec.message() + ". Önceki dosya değiştirilmedi.");
        }
        return core::ok();
    }

    void abandon()
    {
        out_.close();
        std::error_code ec;
        fs::remove(temp_, ec);
    }

private:
    fs::path target_;
    fs::path temp_;
    std::ofstream out_;
};

} // namespace

bool is_project_path(const std::string& path)
{
    const std::string want = core::turkish_upper(kProjectExtension);
    if (path.size() < want.size()) return false;
    return core::turkish_upper(path.substr(path.size() - want.size())) == want;
}

core::Result<ProjectReport> save_project(const core::Document& doc, const core::Settings& settings,
                                         const std::string& path)
{
    if (path.empty())
        return err(ErrorCode::InvalidArgument,
                   "Kaydedilecek dosya adı boş. FARKLIKAYDET ile bir ad verin.");

    const core::EntityTable& ents = doc.entities();
    const core::RingGeometry& geo = doc.geometry();

    // ---- strings ----
    StringPool pool;
    const std::uint32_t crs_id     = pool.intern(doc.crs().id());
    const std::uint32_t catalog_id = pool.intern(catalogue_stamp(settings));

    // ---- layers ----
    std::vector<LayerRecord> layers;
    layers.reserve(doc.layers().size());
    for (const core::Layer& l : doc.layers()) {
        LayerRecord r{};
        r.key                = raw(l.key);
        r.name_string        = pool.intern(l.name);
        r.description_string = pool.intern(l.description);
        r.catalog_ref_string = pool.intern(l.catalog_ref);
        r.visible            = l.visible ? 1u : 0u;
        r.locked             = l.locked ? 1u : 0u;
        r.plottable          = l.plottable ? 1u : 0u;
        r.opacity            = l.opacity;
        r.min_scale          = l.min_scale;
        r.max_scale          = l.max_scale;
        r.appearance         = to_record(l.appearance);
        layers.push_back(r);
    }

    std::vector<std::uint32_t> layer_groups;
    layer_groups.reserve(doc.layers().size());
    for (const core::Layer& l : doc.layers())
        layer_groups.push_back(pool.intern(l.group));

    std::vector<core::StyleId> layer_styles;
    layer_styles.reserve(doc.layers().size());
    bool has_layer_styles = false;
    for (const core::Layer& l : doc.layers()) {
        layer_styles.push_back(l.style);
        has_layer_styles = has_layer_styles || l.style != core::kByLayerStyle;
    }

    // ---- styles ----
    std::vector<AppearanceRecord> styles;
    styles.reserve(doc.styles().size());
    for (const core::Appearance& a : doc.styles().entries())
        styles.push_back(to_record(a));

    // ---- symbols ----
    //
    // The stack behind each style id. Without this block a saved drawing kept its
    // colours and lost its symbology: a `gösterim` declared as a fill under a
    // boundary under a glyph came back as the boundary alone, and the document
    // fingerprint said so on the next open.
    std::vector<SymbolRecord> symbols;
    std::vector<SymbolLayerRecord> symbol_layers;
    std::vector<std::uint8_t> symbol_layer_flags;
    std::vector<std::uint32_t> symbol_layer_text;
    std::vector<std::int32_t> symbol_layer_phase;
    symbols.reserve(doc.styles().size());

    for (std::size_t i = 0; i < doc.styles().size(); ++i) {
        const core::Symbol& sym = doc.styles().symbol_at(static_cast<core::StyleId>(i));

        SymbolRecord r{};
        r.first_layer = static_cast<std::uint32_t>(symbol_layers.size());
        r.layer_count = static_cast<std::uint32_t>(sym.layers.size());
        r.min_scale   = sym.min_scale;
        r.max_scale   = sym.max_scale;
        symbols.push_back(r);

        for (const core::SymbolLayer& l : sym.layers) {
            SymbolLayerRecord sl{};
            sl.look            = to_record(l.look);
            sl.offset_value    = l.offset.value;
            sl.size_value      = l.size.value;
            sl.interval_value  = l.interval.value;
            sl.spacing_y_value = l.spacing_y.value;
            sl.angle_udeg      = l.angle_udeg;
            sl.offset_unit     = static_cast<std::uint8_t>(l.offset.unit);
            sl.size_unit       = static_cast<std::uint8_t>(l.size.unit);
            sl.interval_unit   = static_cast<std::uint8_t>(l.interval.unit);
            sl.spacing_y_unit  = static_cast<std::uint8_t>(l.spacing_y.unit);
            sl.type            = static_cast<std::uint8_t>(l.type);
            sl.shape           = static_cast<std::uint8_t>(l.shape);
            sl.placement       = static_cast<std::uint8_t>(l.placement);
            sl.cap             = static_cast<std::uint8_t>(l.cap);
            sl.join            = static_cast<std::uint8_t>(l.join);
            sl.opacity         = l.opacity;
            sl.image           = l.image;
            symbol_layers.push_back(sl);
            // Bit 0 drawn, bit 1 colour locked. Additive: a file written before
            // the lock existed has the bit clear, which reads back unlocked — and
            // unlocked is what that file meant.
            symbol_layer_flags.push_back(
                static_cast<std::uint8_t>((l.enabled ? 1u : 0u) | (l.colour_locked ? 2u : 0u)));
            symbol_layer_text.push_back(pool.intern(l.text));
        }
    }

    // ---- project settings (model.md R39) ----
    std::vector<SettingRecord> setting_rows;
    for (const std::string& id : settings.explicit_ids()) {
        const std::uint32_t idx = settings.catalogue().find(id);
        if (idx == core::kNoSetting) continue;
        const core::SettingSpec& spec = settings.catalogue().at(idx);
        if (spec.scope != core::SettingScope::Project) continue;

        const core::SettingValue v = settings.get(id);
        SettingRecord r{};
        r.id_string = pool.intern(spec.id);
        r.type      = static_cast<std::uint8_t>(v.type());
        r.scalar    = v.scalar();
        if (v.type() == core::SettingType::Text) {
            const std::string_view text = v.as_text();
            // The buffer is exactly core::kSettingTextCapacity; SettingValue::text
            // refuses anything longer, so this can only ever be a full copy.
            const std::size_t n = std::min(text.size(), sizeof(r.text));
            std::memcpy(r.text, text.data(), n);
        }
        setting_rows.push_back(r);
    }

    // ---- attributes (model.md R27-R29) ----
    //
    // Attributes and text fold into content_hash(), so a file that dropped them
    // would reopen as a DIFFERENT document — and the reader's own fingerprint
    // check would say so on every load. It did, which is how this gap was found.
    std::vector<AttrColumnRecord> attr_columns;
    std::vector<AttrCellRecord> attr_cells;
    {
        const core::AttrTable& table = doc.attributes();
        attr_columns.reserve(table.columns());

        for (core::AttrId c = 0; c < table.columns(); ++c) {
            const core::AttrColumn* col = table.column(c);
            const core::AttrSpec& spec  = col->spec();

            AttrColumnRecord r{};
            r.id_string      = pool.intern(spec.id);
            r.name_string    = pool.intern(spec.name_tr);
            r.summary_string = pool.intern(spec.summary_tr);
            r.catalog_string = pool.intern(spec.catalog);
            r.type           = static_cast<std::uint8_t>(spec.type);
            r.required       = spec.required ? 1u : 0u;
            attr_columns.push_back(r);

            // Only cells that carry a value. A cadastral layer is mostly empty
            // columns; a record per empty cell would be the biggest block in the
            // file and would say nothing.
            for (std::size_t row = 0; row < col->rows(); ++row) {
                if (!col->present(row)) continue;

                auto cell = table.get(c, row);
                if (!cell) continue;

                AttrCellRecord cr{};
                cr.column = c;
                cr.row    = static_cast<std::uint32_t>(row);
                cr.number = cell.value().number;
                if (cell.value().type == core::AttrType::Text ||
                    cell.value().type == core::AttrType::CodeRef)
                    cr.text_string = pool.intern(cell.value().text);
                attr_cells.push_back(cr);
            }
        }
    }

    // ---- text ----
    std::vector<TextRecord> text_rows;
    {
        const core::TextTable& texts = doc.texts();
        for (std::size_t row = 0; row < texts.slot_count(); ++row) {
            if (!texts.has(row)) continue;

            TextRecord r{};
            r.row            = static_cast<std::uint32_t>(row);
            r.content_string = pool.intern(std::string(texts.text(row)));
            r.height_mm      = texts.height(row);
            r.anchor         = static_cast<std::uint8_t>(texts.anchor(row));
            text_rows.push_back(r);
        }
    }

    // ---- the document record ----
    DocumentRecord dr{};
    dr.crs_string      = crs_id;
    dr.catalog_string  = catalog_id;
    dr.next_entity_key = doc.keys().peek_entity();
    dr.next_layer_key  = doc.keys().peek_layer();
    dr.entity_count    = static_cast<std::uint64_t>(ents.size());
    dr.layer_count     = static_cast<std::uint64_t>(layers.size());
    // ---- embedded pictures ----
    //
    // MPYY publishes its symbology as images, so a drawing that uses a gösterim
    // carries the picture rather than a path to it: a path breaks when the file
    // moves, when the data package is not installed, when the drawing is emailed
    // to the belediye that has to check it.
    //
    // Written from id 1: slot 0 is the "no image" sentinel and holds nothing.
    std::vector<ImageRecord> images;
    std::vector<std::byte> image_bytes;

    for (core::ImageId id = 1; id < static_cast<core::ImageId>(doc.images().size()); ++id) {
        const std::span<const std::byte> payload = doc.images().bytes(id);

        ImageRecord r{};
        r.offset = static_cast<std::uint64_t>(image_bytes.size());
        r.bytes  = static_cast<std::uint64_t>(payload.size());
        r.origin = pool.intern(std::string(doc.images().origin(id)));
        r.format = static_cast<std::uint8_t>(doc.images().format(id));
        images.push_back(r);

        image_bytes.insert(image_bytes.end(), payload.begin(), payload.end());
    }

    // The line types, written from id 1 for the same reason images are: slot 0 is
    // the solid sentinel and holds nothing.
    std::vector<DashRecord> dashes;
    for (core::DashId id = 1; id < static_cast<core::DashId>(doc.dashes().size()); ++id) {
        const core::DashPattern& p = doc.dashes().at(id);

        DashRecord r{};
        for (std::size_t i = 0; i < core::kMaxDashSegments; ++i)
            r.lengths[i] = p.lengths[i];
        r.origin = pool.intern(std::string(doc.dashes().origin(id)));
        r.count  = p.count;
        dashes.push_back(r);
    }

    dr.style_count        = static_cast<std::uint64_t>(styles.size());
    dr.symbol_layer_count = static_cast<std::uint64_t>(symbol_layers.size());
    dr.slot_count         = static_cast<std::uint64_t>(geo.slot_count());
    dr.ring_count         = static_cast<std::uint64_t>(geo.ring_count_total());
    dr.vertex_count       = static_cast<std::uint64_t>(geo.vertex_count());

    // ---- the block list, in id order so a hex dump reads like the spec ----
    std::vector<Pending> blocks;

    Pending p_strings;
    p_strings.id    = kBlkStringBytes;
    p_strings.data  = pool.bytes().data();
    p_strings.bytes = static_cast<std::uint64_t>(pool.bytes().size());
    p_strings.count = p_strings.bytes;
    blocks.push_back(p_strings);

    blocks.push_back(column(kBlkStringSpans, pool.spans()));

    Pending p_doc;
    p_doc.id         = kBlkDocument;
    p_doc.elem_bytes = static_cast<std::uint32_t>(sizeof(DocumentRecord));
    p_doc.data       = &dr;
    p_doc.count      = 1;
    p_doc.bytes      = sizeof(DocumentRecord);
    blocks.push_back(p_doc);

    blocks.push_back(column(kBlkLayers, layers));
    blocks.push_back(column(kBlkLayerGroups, layer_groups));
    if (has_layer_styles) blocks.push_back(column(kBlkLayerStyles, layer_styles));
    blocks.push_back(column(kBlkStyles, styles));
    blocks.push_back(column(kBlkSymbols, symbols));
    blocks.push_back(column(kBlkSymbolLayers, symbol_layers));
    blocks.push_back(column(kBlkSymbolLayerFlags, symbol_layer_flags));
    blocks.push_back(column(kBlkSymbolLayerText, symbol_layer_text));

    // Written only when something asks for it, so a drawing that uses no phase is
    // byte for byte what it was before the phase existed.
    if (std::any_of(symbol_layer_phase.begin(), symbol_layer_phase.end(),
                    [](std::int32_t v) { return v != 0; }))
        blocks.push_back(column(kBlkSymbolLayerPhase, symbol_layer_phase));
    blocks.push_back(column(kBlkImages, images));
    blocks.push_back(column(kBlkImageBytes, image_bytes));
    if (!dashes.empty()) blocks.push_back(column(kBlkDashes, dashes));

    blocks.push_back(column(kBlkEntityMinX, ents.min_x));
    blocks.push_back(column(kBlkEntityMinY, ents.min_y));
    blocks.push_back(column(kBlkEntityMaxX, ents.max_x));
    blocks.push_back(column(kBlkEntityMaxY, ents.max_y));
    blocks.push_back(column(kBlkEntityFlags, ents.flags));
    blocks.push_back(column(kBlkEntityLayer, ents.layer));
    blocks.push_back(column(kBlkEntityStyle, ents.style));
    blocks.push_back(column(kBlkEntityKind, ents.kind));
    blocks.push_back(column(kBlkEntitySlot, ents.slot));
    blocks.push_back(column(kBlkEntityKey, ents.key));

    blocks.push_back(column(kBlkRingStart, geo.ring_start));
    blocks.push_back(column(kBlkRingCount, geo.ring_count));
    blocks.push_back(column(kBlkRingPart, geo.ring_part));
    blocks.push_back(column(kBlkRingRole, geo.ring_role));
    blocks.push_back(column(kBlkSlotFirstRing, geo.first_ring));
    blocks.push_back(column(kBlkSlotRingTotal, geo.ring_total));
    blocks.push_back(column(kBlkVertexX, geo.xs));
    blocks.push_back(column(kBlkVertexY, geo.ys));

    blocks.push_back(column(kBlkSettings, setting_rows));
    blocks.push_back(column(kBlkAttrSchema, attr_columns));
    blocks.push_back(column(kBlkAttrCells, attr_cells));
    blocks.push_back(column(kBlkTexts, text_rows));

    // An empty column carries no information a reader needs and its absence is
    // the encoding of "zero of these" (BlockView::column accepts that), so an
    // empty document is a 64-byte header, a document record and a directory.
    std::erase_if(blocks, [](const Pending& b) { return b.bytes == 0 && b.id != kBlkDocument; });

    // ---- offsets ----
    std::uint64_t cursor = sizeof(FileHeader);
    std::vector<BlockEntry> directory;
    directory.reserve(blocks.size());
    for (const Pending& b : blocks) {
        cursor = align_up(cursor);
        directory.push_back(BlockEntry{b.id, b.elem_bytes, cursor, b.bytes, b.count});
        cursor += b.bytes;
    }
    const std::uint64_t directory_offset = align_up(cursor);
    const std::uint64_t total            = directory_offset + directory.size() * sizeof(BlockEntry);

    if (total > kMaxFileSize)
        return err(ErrorCode::Internal, "Belge dosya biçiminin üst sınırından büyük (" +
                                            std::to_string(total) +
                                            " bayt). Belgeyi bölerek kaydedin.");

    // ---- the header ----
    FileHeader header{};
    std::memcpy(header.magic, kMagic, sizeof(kMagic));
    header.format_version     = kFormatVersion;
    header.min_reader_version = kMinReaderVersion;
    header.header_bytes       = static_cast<std::uint32_t>(sizeof(FileHeader));
    header.block_count        = static_cast<std::uint32_t>(directory.size());
    header.directory_offset   = directory_offset;
    header.file_bytes         = total;
    header.content_hash       = doc.content_hash();
    header.settings_hash      = settings.fold(core::fnv1a(std::string_view{}));

    // ---- out ----
    AtomicFile file;
    if (auto st = file.open(fs::path(path)); !st) return st.error();

    file.write(&header, sizeof(header));

    std::uint64_t written = sizeof(FileHeader);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        file.pad(directory[i].offset - written);
        file.write(blocks[i].data, blocks[i].bytes);
        written = directory[i].offset + blocks[i].bytes;
        if (file.bad()) break;
    }
    file.pad(directory_offset - written);
    if (!directory.empty()) file.write(directory.data(), directory.size() * sizeof(BlockEntry));

    if (file.bad()) {
        file.abandon();
        return err(ErrorCode::IoFailure,
                   "'" + path +
                       "' yazılırken hata oluştu; disk dolu olabilir. Önceki dosya "
                       "değiştirilmedi.");
    }
    if (auto st = file.commit(); !st) return st.error();

    ProjectReport report;
    report.entities             = static_cast<std::uint64_t>(doc.live_entity_count());
    report.layers               = dr.layer_count;
    report.vertices             = dr.vertex_count;
    report.bytes                = total;
    report.format_version       = kFormatVersion;
    report.stored_content_hash  = header.content_hash;
    report.stored_settings_hash = header.settings_hash;
    return report;
}

} // namespace piricad::io
