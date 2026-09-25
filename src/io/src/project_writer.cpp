// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: writing the native project file.
//
// The writer is deliberately dull. It walks the document's columns in row order
// and streams them out; there is no reordering of rows and no cleverness, because
// either would change which key belongs to which parcel and model.md R4 makes
// that question a legal one.
//
// ONE THING IS LAID OUT RATHER THAN DUMPED: the slot-indexed half. File slot r
// is the geometry, caption, attribute cells and foreign data of ROW r, as that
// row holds them now. The document keeps a slot per geometry VERSION — an edit
// appends one and leaves the old for undo — and writing those as they stood put
// history in the file: the reader, which rightly reads slot r as row r, gave a
// moved parcel's old cells to whichever object was drawn next, and refused a
// file outright when a moved line's XDATA sat on a slot no object held
// (format 3). A drawing never edited has one slot per row, in row order, and its
// columns are still streamed straight from the document.
//
// AND ONE KIND OF ROW IS NOT WRITTEN: the members an external reference loaded
// (model.md R45a, format 4). Their source is the reference's own file, read
// again on every open; the file keeps the reference's name and path and leaves
// a gap in the key sequence where its members stood.
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
#include "kentos_cad/io/project.hpp"

#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/io/format.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace kentos::io {
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

/// The slot-indexed columns of the file, laid out one slot per written row
/// (see the header comment). Filled only when the document's own columns are
/// not already in that shape; `raw` says they are and nothing was copied.
struct RowSlots
{
    bool raw{true};
    std::vector<std::uint32_t> slot;       ///< per row: its file slot, which is the row
    std::vector<std::uint32_t> first_ring; ///< per file slot
    std::vector<std::uint32_t> ring_total;
    std::vector<std::uint32_t> ring_start; ///< per ring
    std::vector<std::uint32_t> ring_count;
    std::vector<std::uint16_t> ring_part;
    std::vector<core::RingRole> ring_role;
    std::vector<core::Mm> xs; ///< per vertex
    std::vector<core::Mm> ys;
    std::vector<std::uint32_t> payload_ref; ///< per file slot; empty when no row has one
    std::vector<std::uint64_t> payload_start;
    std::vector<std::uint32_t> payload_bytes;
    std::vector<std::uint8_t> payload;
};

/// `v` without the rows `external` marks, or nothing when it marks none — the
/// entity columns of a drawing holding an external reference.
template<class T>
std::vector<T> without_external(const std::vector<T>& v, const std::vector<bool>& external)
{
    std::vector<T> out;
    if (external.empty()) return out;
    out.reserve(v.size());
    for (std::size_t e = 0; e < v.size(); ++e)
        if (!external[e]) out.push_back(v[e]);
    return out;
}

/// An external reference's path as the file stores it: relative to the
/// directory the project file is written into when both sit under one root,
/// with forward slashes on every platform — so a project folder can move, or
/// travel to another machine, with its references beside it. As it is when no
/// relative form exists (another drive).
std::string stored_path(const std::string& path, const fs::path& project_file)
{
    std::error_code ec;
    const fs::path absolute = fs::path(path).lexically_normal();
    const fs::path base     = fs::absolute(project_file, ec).parent_path().lexically_normal();
    if (!absolute.is_absolute() || absolute.root_name() != base.root_name())
        return absolute.generic_string();
    const fs::path relative = absolute.lexically_relative(base);
    return relative.empty() ? absolute.generic_string() : relative.generic_string();
}

RowSlots lay_out_by_row(const core::RingGeometry& geo, const std::vector<std::uint32_t>& slots)
{
    RowSlots out;
    out.raw = slots.size() == geo.slot_count();
    for (std::size_t r = 0; out.raw && r < slots.size(); ++r)
        out.raw = slots[r] == r;
    if (out.raw) return out;

    bool any_payload = false;
    for (const std::uint32_t s : slots)
        any_payload = any_payload || !geo.payload_of(s).empty();

    out.slot.reserve(slots.size());
    out.first_ring.reserve(slots.size());
    out.ring_total.reserve(slots.size());
    if (any_payload) out.payload_ref.reserve(slots.size());
    for (std::size_t r = 0; r < slots.size(); ++r) {
        const std::uint32_t s = slots[r];
        out.slot.push_back(static_cast<std::uint32_t>(r));
        out.first_ring.push_back(static_cast<std::uint32_t>(out.ring_start.size()));
        out.ring_total.push_back(geo.ring_total[s]);
        for (std::uint32_t k = 0; k < geo.ring_total[s]; ++k) {
            const std::uint32_t ring = geo.first_ring[s] + k;
            const std::uint32_t from = geo.ring_start[ring];
            const std::uint32_t n    = geo.ring_count[ring];
            out.ring_start.push_back(static_cast<std::uint32_t>(out.xs.size()));
            out.ring_count.push_back(n);
            out.ring_part.push_back(geo.ring_part[ring]);
            out.ring_role.push_back(geo.ring_role[ring]);
            out.xs.insert(out.xs.end(), geo.xs.begin() + from, geo.xs.begin() + from + n);
            out.ys.insert(out.ys.end(), geo.ys.begin() + from, geo.ys.begin() + from + n);
        }
        if (!any_payload) continue;
        const std::span<const std::uint8_t> bytes = geo.payload_of(s);
        if (bytes.empty()) {
            out.payload_ref.push_back(core::kNoPayload);
            continue;
        }
        out.payload_ref.push_back(static_cast<std::uint32_t>(out.payload_start.size()));
        out.payload_start.push_back(static_cast<std::uint64_t>(out.payload.size()));
        out.payload_bytes.push_back(static_cast<std::uint32_t>(bytes.size()));
        out.payload.insert(out.payload.end(), bytes.begin(), bytes.end());
    }
    return out;
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

    // The slot each written row holds now; file slot r is row r's.
    const std::vector<std::uint32_t> row_slot = doc.row_slots();
    const RowSlots laid                       = lay_out_by_row(geo, row_slot);

    // The rows an external reference loaded, which the file does not hold; the
    // entity columns are written without them when there are any.
    const std::vector<bool> external = doc.external_rows();
    const auto is_external           = [&external](core::EntityId e) {
        return !external.empty() && e < external.size() && external[e];
    };

    // A CLIPPED REFERENCE is written in a payload layout an older reader
    // refuses (format.hpp `kMinReaderVersionClip`). Every written row counts,
    // live or not: a row the file holds is a row a reader decodes.
    bool any_clip = false;
    for (core::EntityId e = 0; e < ents.size() && !any_clip; ++e) {
        if (is_external(e) || ents.kind[e] != core::kBlockReferenceKind) continue;
        auto ref = core::block_reference_of(geo, ents.slot[e]);
        any_clip = ref && !ref.value().clip.empty();
    }
    const auto ext_min_x = without_external(ents.min_x, external);
    const auto ext_min_y = without_external(ents.min_y, external);
    const auto ext_max_x = without_external(ents.max_x, external);
    const auto ext_max_y = without_external(ents.max_y, external);
    const auto ext_flags = without_external(ents.flags, external);
    const auto ext_layer = without_external(ents.layer, external);
    const auto ext_style = without_external(ents.style, external);
    const auto ext_kind  = without_external(ents.kind, external);
    const auto ext_key   = without_external(ents.key, external);

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
    std::vector<std::uint16_t> bind_count;
    std::vector<std::uint32_t> bind_field;
    std::vector<std::uint8_t> bind_what;
    std::vector<std::uint8_t> bind_type;
    bool any_field = false;
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
            bind_count.push_back(static_cast<std::uint16_t>(l.bindings.size()));
            for (const core::SymbolBinding& b : l.bindings) {
                bind_field.push_back(pool.intern(b.field));
                bind_what.push_back(static_cast<std::uint8_t>(b.what));
                bind_type.push_back(static_cast<std::uint8_t>(b.type));
            }
            any_field = any_field || !l.bindings.empty();
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
    std::vector<std::uint32_t> attr_column_layer;
    std::vector<AttrCellRecord> attr_cells;
    {
        const core::AttrTable& table = doc.attributes();
        attr_columns.reserve(table.columns());
        attr_column_layer.reserve(table.columns());

        for (std::size_t i = 0; i < table.columns(); ++i) {
            const auto c                = static_cast<core::AttrId>(i);
            const core::AttrColumn* col = table.column(c);
            const core::AttrSpec& spec  = col->spec();

            AttrColumnRecord r{};
            r.id_string      = pool.intern(spec.id);
            r.name_string    = pool.intern(spec.name_tr);
            r.summary_string = pool.intern(spec.summary_tr);
            r.catalog_string = pool.intern(spec.catalog);
            r.type           = static_cast<std::uint8_t>(spec.type);
            r.required       = spec.required ? 1u : 0u;
            r.scale          = spec.scale;
            attr_columns.push_back(r);
            attr_column_layer.push_back(pool.intern(spec.layer));

            // Only cells that carry a value. A cadastral layer is mostly empty
            // columns; a record per empty cell would be the biggest block in the
            // file and would say nothing. By row, each at the slot it holds now.
            for (std::size_t row = 0; row < row_slot.size(); ++row) {
                const std::uint32_t slot = row_slot[row];
                if (!col->present(slot)) continue;

                auto cell = table.get(c, slot);
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
        for (std::size_t row = 0; row < row_slot.size(); ++row) {
            const std::uint32_t slot = row_slot[row];
            if (!texts.has(slot)) continue;

            TextRecord r{};
            r.row                       = static_cast<std::uint32_t>(row);
            r.content_string            = pool.intern(texts.text(slot));
            r.height_mm                 = texts.height(slot);
            r.anchor                    = static_cast<std::uint8_t>(texts.anchor(slot));
            const core::TextLines lines = texts.lines(slot);
            r.flags                     = lines.wrap ? 1 : 0;
            r.spacing                   = lines.spacing;
            text_rows.push_back(r);
        }
    }

    // ---- foreign data (model.md R26a) ----
    //
    // By row, each at the slot it holds now. The pool is written as the table
    // holds it when every record belongs to a row, and rebuilt from the written
    // records when one does not — history's bytes stay behind with history.
    std::vector<ForeignRecord> foreign_rows;
    std::vector<std::uint8_t> foreign_pool;
    bool foreign_pool_rebuilt = false;
    {
        const core::ForeignTable& foreign                   = doc.foreign();
        const std::vector<core::ForeignTable::Record>& recs = foreign.records();
        if (!recs.empty()) {
            std::vector<std::pair<std::uint32_t, const core::ForeignTable::Record*>> written;
            for (std::size_t row = 0; row < row_slot.size(); ++row) {
                auto at = std::ranges::lower_bound(recs, row_slot[row], {},
                                                   &core::ForeignTable::Record::slot);
                for (; at != recs.end() && at->slot == row_slot[row]; ++at)
                    written.emplace_back(static_cast<std::uint32_t>(row), &*at);
            }
            foreign_pool_rebuilt = written.size() != recs.size();
            for (const auto& [row, rec] : written) {
                ForeignRecord r{};
                r.slot       = row;
                r.tag_string = pool.intern(foreign.tags()[rec->tag]);
                r.offset     = rec->start;
                r.bytes      = rec->bytes;
                if (foreign_pool_rebuilt) {
                    r.offset = static_cast<std::uint64_t>(foreign_pool.size());
                    foreign_pool.insert(foreign_pool.end(), foreign.pool().data() + rec->start,
                                        foreign.pool().data() + rec->start + rec->bytes);
                }
                foreign_rows.push_back(r);
            }
        }
    }

    // ---- attachments (core/attach.hpp): live dependents of live sources ----
    std::vector<AttachRecord> attach_rows;
    {
        const core::AttachTable& attachments = doc.attachments();
        for (const core::EntityId e : attachments.attached()) {
            if (!doc.alive(e) || is_external(e)) continue;
            const core::Attachment* a = attachments.get(e);
            const core::EntityId src  = doc.slot_of(a->source);
            if (src == core::kNoEntity || !doc.alive(src) || is_external(src)) continue;
            AttachRecord r{};
            r.dependent_key = core::raw(doc.key_of(e));
            r.source_key    = core::raw(a->source);
            r.gap_mm        = a->gap;
            r.along_mm      = a->along;
            r.across_mm     = a->across;
            r.index         = a->index;
            r.format_string = a->format.empty() ? 0u : pool.intern(a->format);
            r.ring          = a->ring;
            r.anchor        = static_cast<std::uint8_t>(a->anchor);
            r.side          = static_cast<std::uint8_t>(a->side);
            r.derive        = static_cast<std::uint8_t>(a->derive);
            r.unit          = a->unit;
            r.precision     = a->precision;
            r.separator     = static_cast<std::uint8_t>(a->separator);
            attach_rows.push_back(r);
        }
    }

    // ---- dimension links (core/dimension_link.hpp): live dimensions only ----
    std::vector<DimLinkRecord> dimlink_rows;
    {
        const core::DimLinkTable& links = doc.dimension_links();
        for (const core::EntityId dim : links.linked()) {
            if (!doc.alive(dim) || is_external(dim)) continue;
            for (const core::DimLink& l : *links.get(dim)) {
                DimLinkRecord r{};
                r.dimension_key = core::raw(doc.key_of(dim));
                r.source_key    = core::raw(l.source);
                r.index         = l.index;
                r.ring          = l.ring;
                r.point         = l.point;
                r.anchor        = static_cast<std::uint8_t>(l.anchor);
                r.broken        = l.broken ? 1 : 0;
                dimlink_rows.push_back(r);
            }
        }
    }

    // ---- hatch links (core/hatch_link.hpp): live hatches only ----
    std::vector<HatchLinkRecord> hatchlink_rows;
    {
        const core::HatchLinkTable& links = doc.hatch_links();
        for (const core::EntityId hatch : links.linked()) {
            if (!doc.alive(hatch) || is_external(hatch)) continue;
            for (const core::HatchSource& s : *links.get(hatch)) {
                HatchLinkRecord r{};
                r.hatch_key  = core::raw(doc.key_of(hatch));
                r.source_key = core::raw(s.source);
                r.broken     = s.broken ? 1 : 0;
                hatchlink_rows.push_back(r);
            }
        }
    }

    // ---- lineage (core/lineage.hpp), a source by its key ----
    //
    // DEAD ROWS TOO: a parcel two ifraz ago is gone from the sheet and still
    // the link between today's parcel and the one it all came from. Its row is
    // written dead with its key, so its history can be written beside it.
    std::vector<LineageRecord> lineage_rows;
    {
        const core::LineageTable& table = doc.lineage();
        for (const core::EntityId made : table.derived()) {
            if (is_external(made)) continue;
            const core::Lineage& origin = *table.get(made);
            const std::uint32_t op      = pool.intern(origin.operation);
            for (const core::EntityKey source : origin.sources) {
                LineageRecord r{};
                r.made_key         = core::raw(doc.key_of(made));
                r.source_key       = core::raw(source);
                r.operation_string = op;
                lineage_rows.push_back(r);
            }
        }
    }

    // ---- block definitions (model.md R45) ----
    std::vector<BlockRecord> block_rows;
    std::vector<std::uint64_t> block_members;
    std::vector<std::uint32_t> block_uses;
    bool any_external = false;
    for (const core::BlockDef& def : doc.blocks().all()) {
        BlockRecord r{};
        r.name_string  = pool.intern(def.name);
        r.desc_string  = def.description.empty() ? 0u : pool.intern(def.description);
        r.base_x       = def.base.x;
        r.base_y       = def.base.y;
        r.first_member = static_cast<std::uint32_t>(block_members.size());
        r.first_use    = static_cast<std::uint32_t>(block_uses.size());
        r.flags        = def.flags;
        // AN EXTERNAL REFERENCE KEEPS ITS NAME AND ITS PATH, and nothing its
        // file supplies: neither the members nor the uses they imply, which
        // come back with them when the file is read.
        if (def.external()) {
            any_external = true;
            if (!def.path.empty()) r.path_string = pool.intern(stored_path(def.path, path));
            block_rows.push_back(r);
            continue;
        }
        r.member_count = static_cast<std::uint32_t>(def.members.size());
        for (const core::EntityKey k : def.members)
            block_members.push_back(core::raw(k));
        r.use_count = static_cast<std::uint32_t>(def.uses.size());
        for (const core::BlockId u : def.uses)
            block_uses.push_back(u);
        block_rows.push_back(r);
    }

    // ---- the document record ----
    DocumentRecord dr{};
    dr.crs_string      = crs_id;
    dr.catalog_string  = catalog_id;
    dr.next_entity_key = doc.keys().peek_entity();
    dr.next_layer_key  = doc.keys().peek_layer();
    dr.entity_count = static_cast<std::uint64_t>(external.empty() ? ents.size() : ext_key.size());
    dr.layer_count  = static_cast<std::uint64_t>(layers.size());
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
    dr.slot_count = static_cast<std::uint64_t>(laid.raw ? geo.slot_count() : laid.slot.size());
    dr.ring_count =
        static_cast<std::uint64_t>(laid.raw ? geo.ring_count_total() : laid.ring_start.size());
    dr.vertex_count = static_cast<std::uint64_t>(laid.raw ? geo.vertex_count() : laid.xs.size());

    // The sheet layouts, on the same bargain as the guides above: written only
    // when the drawing has one, so a file without a pafta is byte for byte what
    // it was before layouts existed. The pages, the items and the two name runs
    // per item go in three blocks beside the layout row, exactly as a block
    // definition's members do (`kBlkBlockMembers`).
    std::vector<LayoutRecord> layout_rows;
    std::vector<LayoutPageRecord> page_rows;
    std::vector<LayoutItemRecord> item_rows;
    std::vector<std::uint32_t> name_rows;
    {

        for (const core::Layout& l : doc.layouts().all()) {
            LayoutRecord row{};
            row.name      = pool.intern(l.name);
            row.paper     = pool.intern(l.paper);
            row.dpi       = l.dpi;
            row.margin_um = l.margin;
            row.landscape = l.landscape ? 1u : 0u;
            // 0 MEANS "not an atlas", which is what a file written before this
            // field says in its reserved bytes.
            row.atlas_layer       = pool.intern(l.atlas.coverage_layer);
            row.atlas_sort        = pool.intern(l.atlas.sort_by);
            row.atlas_single_file = l.atlas.single_file ? 1u : 0u;
            row.atlas_margin_pct  = l.atlas.margin_percent;
            row.first_page        = static_cast<std::uint32_t>(page_rows.size());
            row.page_count        = static_cast<std::uint32_t>(l.pages.size());
            row.first_item        = static_cast<std::uint32_t>(item_rows.size());
            row.item_count        = static_cast<std::uint32_t>(l.items.size());

            for (const core::LayoutPage& page : l.pages)
                page_rows.push_back(LayoutPageRecord{page.w, page.h});

            for (std::size_t i = 0; i < l.items.size(); ++i) {
                const core::LayoutItem& item = l.items[i];
                LayoutItemRecord out{};
                out.id                  = pool.intern(item.id);
                out.text                = pool.intern(item.text);
                out.x_um                = item.frame.x;
                out.y_um                = item.frame.y;
                out.w_um                = item.frame.w;
                out.h_um                = item.frame.h;
                out.z                   = item.z;
                out.rotation_udeg       = item.rotation_udeg;
                out.page                = l.page_of(i);
                out.frame_width_um      = item.frame_width;
                out.frame_colour        = item.frame_colour;
                out.background_colour   = item.background_colour;
                out.text_height_um      = item.text_height;
                out.text_colour         = item.text_colour;
                out.extent_min_x        = item.extent.min_x;
                out.extent_min_y        = item.extent.min_y;
                out.extent_max_x        = item.extent.max_x;
                out.extent_max_y        = item.extent.max_y;
                out.scale               = item.scale;
                out.grid_interval_mm    = item.grid_interval;
                out.grid_width_um       = item.grid_width;
                out.grid_colour         = item.grid_colour;
                out.grid_text_height_um = item.grid_text_height;
                out.style               = item.style;
                out.row_limit           = item.row_limit;
                out.kind                = static_cast<std::uint8_t>(item.kind);
                out.locked              = item.locked ? 1u : 0u;
                out.frame_visible       = item.frame_visible ? 1u : 0u;
                out.background          = item.background ? 1u : 0u;
                out.align_h             = item.align_h;
                out.align_v             = item.align_v;
                out.grid                = static_cast<std::uint8_t>(item.grid);
                out.grid_labels         = static_cast<std::uint8_t>(item.grid_labels);
                out.shape               = static_cast<std::uint8_t>(item.shape);
                // 0 MEANS "follow the first map", which is what an empty name means
                // and what a file written before this field says.
                out.linked_map = pool.intern(item.linked_map);

                out.first_layer = static_cast<std::uint32_t>(name_rows.size());
                out.layer_count = static_cast<std::uint32_t>(item.layers.size());
                for (const std::string& layer : item.layers)
                    name_rows.push_back(pool.intern(layer));
                out.first_column = static_cast<std::uint32_t>(name_rows.size());
                out.column_count = static_cast<std::uint32_t>(item.columns.size());
                for (const std::string& col : item.columns)
                    name_rows.push_back(pool.intern(col));

                item_rows.push_back(out);
            }
            layout_rows.push_back(row);
        }
    }

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

    // AND THE SAME BARGAIN FOR THE PARAMETERS. A drawing whose symbols declare
    // none writes no block at all, so its file is byte for byte the one it was
    // before parameters existed — which is what keeps `tests/golden` honest about
    // what a change actually changed.
    if (any_field) {
        blocks.push_back(column(kBlkSymbolLayerBindCount, bind_count));
        blocks.push_back(column(kBlkSymbolLayerBindField, bind_field));
        blocks.push_back(column(kBlkSymbolLayerBindWhat, bind_what));
        blocks.push_back(column(kBlkSymbolLayerBindType, bind_type));
    }
    blocks.push_back(column(kBlkImages, images));
    blocks.push_back(column(kBlkImageBytes, image_bytes));
    if (!dashes.empty()) blocks.push_back(column(kBlkDashes, dashes));

    // The guides. Written only when there are some, so a drawing with none costs
    // no bytes and its file stays byte-identical to one written before guides
    // existed — which is what keeps the golden fixtures valid.
    std::vector<std::uint8_t> guide_axes;
    std::vector<std::int64_t> guide_coords;
    std::vector<std::int64_t> guide_angles;
    std::vector<std::int64_t> guide_through_x;
    std::vector<std::int64_t> guide_through_y;
    std::vector<std::uint8_t> guide_rays;
    for (std::size_t i = 0; i < doc.guides().size(); ++i) {
        guide_axes.push_back(static_cast<std::uint8_t>(doc.guides().axis(i)));
        guide_coords.push_back(static_cast<std::int64_t>(doc.guides().coordinate(i)));
        guide_angles.push_back(doc.guides().angle(i));
        guide_through_x.push_back(static_cast<std::int64_t>(doc.guides().through(i).x));
        guide_through_y.push_back(static_cast<std::int64_t>(doc.guides().through(i).y));
        guide_rays.push_back(doc.guides().ray(i) ? 1u : 0u);
    }
    if (!guide_axes.empty()) {
        blocks.push_back(column(kBlkGuideAxis, guide_axes));
        blocks.push_back(column(kBlkGuideCoord, guide_coords));
        // THE FOUR ANGLED COLUMNS ONLY WHEN ONE IS ANGLED, so a drawing of
        // ordinary ruler guides writes exactly the bytes the build before this
        // one wrote — which is what keeps the stored golden fixtures valid.
        if (doc.guides().any_angled()) {
            blocks.push_back(column(kBlkGuideAngle, guide_angles));
            blocks.push_back(column(kBlkGuideThroughX, guide_through_x));
            blocks.push_back(column(kBlkGuideThroughY, guide_through_y));
            blocks.push_back(column(kBlkGuideRay, guide_rays));
        }
    }

    // The four layout blocks. The rows were built above, before the string pool
    // was snapshotted, because interning a name after that point would move the
    // bytes the pool block already points at.
    if (!layout_rows.empty()) {
        blocks.push_back(column(kBlkLayouts, layout_rows));
        blocks.push_back(column(kBlkLayoutPages, page_rows));
        if (!item_rows.empty()) blocks.push_back(column(kBlkLayoutItems, item_rows));
        if (!name_rows.empty()) blocks.push_back(column(kBlkLayoutNames, name_rows));
    }

    const bool all_rows = external.empty();
    blocks.push_back(column(kBlkEntityMinX, all_rows ? ents.min_x : ext_min_x));
    blocks.push_back(column(kBlkEntityMinY, all_rows ? ents.min_y : ext_min_y));
    blocks.push_back(column(kBlkEntityMaxX, all_rows ? ents.max_x : ext_max_x));
    blocks.push_back(column(kBlkEntityMaxY, all_rows ? ents.max_y : ext_max_y));
    blocks.push_back(column(kBlkEntityFlags, all_rows ? ents.flags : ext_flags));
    blocks.push_back(column(kBlkEntityLayer, all_rows ? ents.layer : ext_layer));
    blocks.push_back(column(kBlkEntityStyle, all_rows ? ents.style : ext_style));
    blocks.push_back(column(kBlkEntityKind, all_rows ? ents.kind : ext_kind));
    blocks.push_back(column(kBlkEntitySlot, laid.raw ? ents.slot : laid.slot));
    blocks.push_back(column(kBlkEntityKey, all_rows ? ents.key : ext_key));

    blocks.push_back(column(kBlkRingStart, laid.raw ? geo.ring_start : laid.ring_start));
    blocks.push_back(column(kBlkRingCount, laid.raw ? geo.ring_count : laid.ring_count));
    blocks.push_back(column(kBlkRingPart, laid.raw ? geo.ring_part : laid.ring_part));
    blocks.push_back(column(kBlkRingRole, laid.raw ? geo.ring_role : laid.ring_role));
    blocks.push_back(column(kBlkSlotFirstRing, laid.raw ? geo.first_ring : laid.first_ring));
    blocks.push_back(column(kBlkSlotRingTotal, laid.raw ? geo.ring_total : laid.ring_total));
    blocks.push_back(column(kBlkVertexX, laid.raw ? geo.xs : laid.xs));
    blocks.push_back(column(kBlkVertexY, laid.raw ? geo.ys : laid.ys));

    // The kind payload, only when a slot carries one (model.md R9a): a drawing of
    // parcels, circles and captions writes none of these, so its file is byte
    // for byte the one it was before payloads existed — which keeps the golden
    // fixtures honest about what a change actually changed. Once one slot has a
    // payload the reference column covers every slot (RingGeometry::append).
    if (laid.raw ? geo.has_payload() : !laid.payload_ref.empty()) {
        blocks.push_back(column(kBlkKindPayload, laid.raw ? geo.payload : laid.payload));
        blocks.push_back(column(kBlkSlotPayloadRef, laid.raw ? geo.payload_ref : laid.payload_ref));
        blocks.push_back(
            column(kBlkPayloadStart, laid.raw ? geo.payload_start : laid.payload_start));
        blocks.push_back(
            column(kBlkPayloadBytes, laid.raw ? geo.payload_bytes : laid.payload_bytes));
    }

    blocks.push_back(column(kBlkSettings, setting_rows));
    blocks.push_back(column(kBlkAttrSchema, attr_columns));
    blocks.push_back(column(kBlkAttrColumnLayer, attr_column_layer));
    blocks.push_back(column(kBlkAttrCells, attr_cells));
    blocks.push_back(column(kBlkTexts, text_rows));

    // Foreign data and block definitions: both absent from a drawing that has
    // none, so its file is byte for byte what it was before they existed.
    if (!foreign_rows.empty()) {
        blocks.push_back(
            column(kBlkForeignBytes, foreign_pool_rebuilt ? foreign_pool : doc.foreign().pool()));
        blocks.push_back(column(kBlkForeignRecords, foreign_rows));
    }
    if (!block_rows.empty()) {
        blocks.push_back(column(kBlkBlocks, block_rows));
        blocks.push_back(column(kBlkBlockMembers, block_members));
        blocks.push_back(column(kBlkBlockUses, block_uses));
    }
    if (!attach_rows.empty()) blocks.push_back(column(kBlkAttachments, attach_rows));
    if (!dimlink_rows.empty()) blocks.push_back(column(kBlkDimensionLinks, dimlink_rows));
    if (!hatchlink_rows.empty()) blocks.push_back(column(kBlkHatchLinks, hatchlink_rows));
    if (!lineage_rows.empty()) blocks.push_back(column(kBlkLineage, lineage_rows));

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
    header.format_version = kFormatVersion;
    // THE FILE SAYS WHAT IT NEEDS, per drawing rather than per build: the
    // highest of what it holds asks — a clipped reference, an external
    // reference, an angled guide (format.hpp `kMinReaderVersion…`).
    header.min_reader_version = any_clip                    ? kMinReaderVersionClip
                                : any_external              ? kMinReaderVersionExternal
                                : doc.guides().any_angled() ? kMinReaderVersionAngledGuide
                                                            : kMinReaderVersion;
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
    // The drawing's own objects: an external reference's are its file's.
    std::uint64_t live = doc.live_entity_count();
    for (std::size_t e = 0; e < external.size(); ++e)
        if (external[e] && doc.alive(static_cast<core::EntityId>(e))) --live;
    report.entities             = live;
    report.layers               = dr.layer_count;
    report.vertices             = dr.vertex_count;
    report.bytes                = total;
    report.format_version       = kFormatVersion;
    report.stored_content_hash  = header.content_hash;
    report.stored_settings_hash = header.settings_hash;
    return report;
}

} // namespace kentos::io
