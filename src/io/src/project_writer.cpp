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

    // ---- styles ----
    std::vector<AppearanceRecord> styles;
    styles.reserve(doc.styles().size());
    for (const core::Appearance& a : doc.styles().entries())
        styles.push_back(to_record(a));

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

    // ---- the document record ----
    DocumentRecord dr{};
    dr.crs_string      = crs_id;
    dr.catalog_string  = catalog_id;
    dr.next_entity_key = doc.keys().peek_entity();
    dr.next_layer_key  = doc.keys().peek_layer();
    dr.entity_count    = static_cast<std::uint64_t>(ents.size());
    dr.layer_count     = static_cast<std::uint64_t>(layers.size());
    dr.style_count     = static_cast<std::uint64_t>(styles.size());
    dr.slot_count      = static_cast<std::uint64_t>(geo.slot_count());
    dr.ring_count      = static_cast<std::uint64_t>(geo.ring_count_total());
    dr.vertex_count    = static_cast<std::uint64_t>(geo.vertex_count());

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
    blocks.push_back(column(kBlkStyles, styles));

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
