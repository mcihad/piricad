// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: reading the native project file.
//
// THE READER'S CONTRACT, in three sentences.
//
//   1. Every number in the file is hostile until `BlockView` has bounded it, and
//      every cross-column index is bounded again here (io.md R18, P6).
//   2. Every edit goes through the caller's `command::Transaction`, so a failure
//      anywhere leaves the document exactly as it was — there is no partial load
//      and no orphan layer (io.md R17, P11; Article 5.9).
//   3. Nothing is repaired silently. A value that had to be adjusted comes back
//      as a `Warning` the command shows the user (io.md P13, model.md R42).
//
// WHY DEAD ROWS ARE REPLAYED. An erased entity keeps its row, its slot and its
// key; only its `FlagAlive` bit goes out. The reader recreates the row and then
// erases it again, which looks wasteful and is not: model.md R4 forbids reusing a
// retired key, so the key sequence has to be reproduced hole for hole. Skipping
// the dead rows would compact the sequence and hand parcel 47's retired key to
// parcel 48 — and "which parcel was this?" is a legal question (§12).
//
// WHAT THIS BUILD CANNOT PUT BACK. Six `core::Layer` fields — description,
// plottable, min_scale, max_scale, opacity and catalog_ref — have no primitive
// mutator, so no command can set them and no `Transaction` can restore them. The
// writer stores them all; the reader reports a warning when one is not at its
// default rather than dropping it in silence. Nothing in the running build can
// produce such a file today, so the warning is a tripwire for the day the
// mutators land, not a routine event.
#include "piricad/io/project.hpp"

#include "piricad/core/text.hpp"
#include "piricad/io/format.hpp"

#include "block_view.hpp"
#include "mapped_file.hpp"

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::io {
namespace {

using core::err;
using core::ErrorCode;

/// How often the reader looks at the stop token. io.md R15 asks for at least
/// every 4 MB or 64 K entities and a return within 100 ms; a parcel is tens of
/// vertices, so the entity count is the binding one.
constexpr std::size_t kCancelStride = 4096;

core::Error cancelled()
{
    return core::Error{ErrorCode::Cancelled, "Dosya okuma iptal edildi; belge değişmedi."};
}

/// Bounds-checked access to the string pool. An id or a span that points outside
/// the pool is a corrupt file, never a truncated string.
class Strings
{
public:
    Strings(std::span<const std::byte> pool, std::span<const StringSpan> spans)
        : pool_(pool), spans_(spans)
    {}

    core::Result<std::string> at(std::uint32_t id, const char* what) const
    {
        if (id >= spans_.size())
            return err(ErrorCode::ParseError, std::string(kErrConsist) + ": " + what +
                                                  " metin dizini " + std::to_string(id) +
                                                  ", havuzda " + std::to_string(spans_.size()) +
                                                  " metin var. Dosya bozuk.");

        const StringSpan s = spans_[id];
        if (s.offset > pool_.size() || s.bytes > pool_.size() - s.offset)
            return err(ErrorCode::ParseError, std::string(kErrConsist) + ": " + what +
                                                  " metni havuzun dışına taşıyor. Dosya bozuk.");

        return std::string(reinterpret_cast<const char*>(pool_.data() + s.offset),
                           static_cast<std::size_t>(s.bytes));
    }

private:
    std::span<const std::byte> pool_;
    std::span<const StringSpan> spans_;
};

core::Appearance from_record(const AppearanceRecord& r)
{
    core::Appearance a{};
    a.rgba      = r.rgba;
    a.width_um  = r.width_um;
    a.dash      = r.dash;
    a.symbol    = r.symbol;
    a.fill_rgba = r.fill_rgba;
    a.hatch     = r.hatch;
    a.z_order   = r.z_order;

    // A `Source` this build does not know is clamped to ByLayer rather than
    // reinterpreted: an out-of-range enum is UB the moment anything switches on
    // it, and ByLayer is the answer that loses the least (model.md R42).
    const auto src = [](std::uint8_t v) {
        return v <= static_cast<std::uint8_t>(core::Source::ByBlock) ? static_cast<core::Source>(v)
                                                                     : core::Source::ByLayer;
    };
    a.src_colour = src(r.src_colour);
    a.src_width  = src(r.src_width);
    a.src_dash   = src(r.src_dash);
    a.src_fill   = src(r.src_fill);
    return a;
}

core::SymbolLayer from_record(const SymbolLayerRecord& r)
{
    core::SymbolLayer l;
    l.look       = from_record(r.look);
    l.offset     = core::Measure{r.offset_value, static_cast<core::Unit>(r.offset_unit)};
    l.size       = core::Measure{r.size_value, static_cast<core::Unit>(r.size_unit)};
    l.interval   = core::Measure{r.interval_value, static_cast<core::Unit>(r.interval_unit)};
    l.spacing_y  = core::Measure{r.spacing_y_value, static_cast<core::Unit>(r.spacing_y_unit)};
    l.angle_udeg = r.angle_udeg;
    l.type       = static_cast<core::SymbolLayerType>(r.type);
    l.shape      = static_cast<core::MarkerShape>(r.shape);
    l.placement  = static_cast<core::MarkerPlacement>(r.placement);
    l.cap        = static_cast<core::LineCap>(r.cap);
    l.join       = static_cast<core::LineJoin>(r.join);
    l.opacity    = r.opacity;
    l.image      = r.image;
    return l;
}

/// Everything the reader needs from the file, already bounded.
struct Columns
{
    std::span<const core::Mm> min_x, min_y, max_x, max_y;
    std::span<const std::uint8_t> flags;
    std::span<const std::uint32_t> layer, style, slot;
    std::span<const std::uint16_t> kind;
    std::span<const std::uint64_t> key;

    std::span<const std::uint32_t> ring_start, ring_count;
    std::span<const std::uint16_t> ring_part;
    std::span<const std::uint8_t> ring_role;
    std::span<const std::uint32_t> first_ring, ring_total;
    std::span<const core::Mm> xs, ys;
};

core::Status load_columns(const BlockView& view, const DocumentRecord& dr, Columns& c)
{
#define PIRICAD_COLUMN(field, id, type, count, what)                                               \
    do {                                                                                           \
        auto r = view.column<type>(id, count, what);                                               \
        if (!r) return r.error();                                                                  \
        c.field = r.value();                                                                       \
    } while (false)

    PIRICAD_COLUMN(min_x, kBlkEntityMinX, core::Mm, dr.entity_count, "en küçük X");
    PIRICAD_COLUMN(min_y, kBlkEntityMinY, core::Mm, dr.entity_count, "en küçük Y");
    PIRICAD_COLUMN(max_x, kBlkEntityMaxX, core::Mm, dr.entity_count, "en büyük X");
    PIRICAD_COLUMN(max_y, kBlkEntityMaxY, core::Mm, dr.entity_count, "en büyük Y");
    PIRICAD_COLUMN(flags, kBlkEntityFlags, std::uint8_t, dr.entity_count, "nesne bayrakları");
    PIRICAD_COLUMN(layer, kBlkEntityLayer, std::uint32_t, dr.entity_count, "nesne katmanı");
    PIRICAD_COLUMN(style, kBlkEntityStyle, std::uint32_t, dr.entity_count, "nesne stili");
    PIRICAD_COLUMN(kind, kBlkEntityKind, std::uint16_t, dr.entity_count, "nesne türü");
    PIRICAD_COLUMN(slot, kBlkEntitySlot, std::uint32_t, dr.entity_count, "nesne geometri yuvası");
    PIRICAD_COLUMN(key, kBlkEntityKey, std::uint64_t, dr.entity_count, "nesne anahtarları");

    PIRICAD_COLUMN(ring_start, kBlkRingStart, std::uint32_t, dr.ring_count, "halka başlangıcı");
    PIRICAD_COLUMN(ring_count, kBlkRingCount, std::uint32_t, dr.ring_count, "halka uzunluğu");
    PIRICAD_COLUMN(ring_part, kBlkRingPart, std::uint16_t, dr.ring_count, "halka parçası");
    PIRICAD_COLUMN(ring_role, kBlkRingRole, std::uint8_t, dr.ring_count, "halka rolü");
    PIRICAD_COLUMN(first_ring, kBlkSlotFirstRing, std::uint32_t, dr.slot_count,
                   "yuvanın ilk halkası");
    PIRICAD_COLUMN(ring_total, kBlkSlotRingTotal, std::uint32_t, dr.slot_count,
                   "yuvanın halka sayısı");
    PIRICAD_COLUMN(xs, kBlkVertexX, core::Mm, dr.vertex_count, "tepe noktası X");
    PIRICAD_COLUMN(ys, kBlkVertexY, core::Mm, dr.vertex_count, "tepe noktası Y");

#undef PIRICAD_COLUMN
    return core::ok();
}

/// Cross-column consistency. Every index below points from one untrusted column
/// into another, and every one of them is a memory read if it is not checked
/// here — this is the function that decides whether a malicious .pcad is a
/// rejected file or a segfault.
core::Status validate_indices(const DocumentRecord& dr, const Columns& c)
{
    for (std::uint64_t r = 0; r < dr.ring_count; ++r) {
        const std::uint64_t start = c.ring_start[r];
        const std::uint64_t count = c.ring_count[r];
        if (start > dr.vertex_count || count > dr.vertex_count - start)
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(r + 1) + ". halka [" +
                           std::to_string(start) + ", +" + std::to_string(count) +
                           ") tepe noktası dizisinin dışına taşıyor (" +
                           std::to_string(dr.vertex_count) + " nokta). Dosya bozuk.");
        if (c.ring_role[r] > static_cast<std::uint8_t>(core::RingRole::Interior))
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(r + 1) +
                           ". halka tanımsız bir rol taşıyor: " + std::to_string(c.ring_role[r]) +
                           ". Geçerli roller: açık (0), dış (1), iç (2).");
    }

    for (std::uint64_t s = 0; s < dr.slot_count; ++s) {
        const std::uint64_t first = c.first_ring[s];
        const std::uint64_t total = c.ring_total[s];
        if (first > dr.ring_count || total > dr.ring_count - first)
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(s + 1) +
                           ". geometri yuvasının halkaları halka dizisinin dışına taşıyor. "
                           "Dosya bozuk.");
        if (total == 0)
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(s + 1) +
                           ". geometri yuvasında hiç halka yok. Bir geometri en az bir halka "
                           "ister.");
    }

    std::uint64_t previous_key = 0;
    for (std::uint64_t e = 0; e < dr.entity_count; ++e) {
        if (c.slot[e] >= dr.slot_count)
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(e + 1) +
                           ". nesne geometri yuvası " + std::to_string(c.slot[e]) + ", dosyada " +
                           std::to_string(dr.slot_count) + " yuva var. Dosya bozuk.");
        if (c.layer[e] >= dr.layer_count)
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(e + 1) + ". nesne katman " +
                           std::to_string(c.layer[e]) + "'e ait, dosyada " +
                           std::to_string(dr.layer_count) + " katman var. Dosya bozuk.");
        if (c.style[e] >= dr.style_count)
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(e + 1) + ". nesne stil " +
                           std::to_string(c.style[e]) + "'e bakıyor, dosyada " +
                           std::to_string(dr.style_count) + " stil var. Dosya bozuk.");

        // model.md R26 wants an unknown kind preserved and non-editable. This
        // build has no payload block to carry one, so a file that names a kind it
        // does not know is REFUSED rather than opened with its geometry silently
        // reinterpreted. Refusing loses nothing; opening it would lose the entity
        // and P11 forbids that.
        //
        // 0 is accepted and MEANS `core.polyline`. Every file written before the
        // kind column carried anything wrote a zero into it — the column existed,
        // nothing filled it — and those drawings are polylines. Refusing them now
        // would be refusing every project saved by an earlier build.
        if (c.kind[e] != 0 && c.kind[e] != core::kPolylineKind &&
            c.kind[e] != core::kCircleKind && c.kind[e] != core::kArcKind &&
            c.kind[e] != core::kPointKind)
            return err(ErrorCode::Unsupported,
                       std::string(kErrKind) + ": " + std::to_string(e + 1) + ". nesne " +
                           std::to_string(c.kind[e]) +
                           " numaralı nesne türünde; bu yapı bu türü tanımıyor. "
                           "Dosyayı yazan PiriCAD sürümüne yükseltin.");

        // model.md R3: a key above 2^63-1 becomes negative the moment it is
        // journalled, so it can never be allowed in from a file.
        if (c.key[e] > core::kMaxKey)
            return err(ErrorCode::ParseError,
                       std::string(kErrKey) + ": " + std::to_string(e + 1) +
                           ". nesnenin anahtarı temsil edilebilir aralığın dışında. Dosya bozuk.");
        if (c.key[e] <= previous_key)
            return err(ErrorCode::ParseError,
                       std::string(kErrKey) + ": nesne anahtarları artan sırada değil (" +
                           std::to_string(previous_key) + " sonrası " + std::to_string(c.key[e]) +
                           "). Anahtar sırası belgenin kimlik düzenidir; dosya bozuk.");
        previous_key = c.key[e];
    }

    return core::ok();
}

/// The load itself. Split out of the coroutine so the control flow reads as
/// ordinary code: there is nothing here to suspend on, and R15's `Task` signature
/// is about the CONTRACT the caller sees, not about faking a suspension point.
core::Result<ProjectReport> load(command::Transaction& tx, const std::string& path,
                                 core::Settings& settings, const std::stop_token& stop)
{
    core::Document& doc = tx.document();

    if (doc.entities().size() != 0 || doc.layers().size() != 1 || doc.styles().size() != 1)
        return err(ErrorCode::InvalidArgument,
                   "Proje dosyası yalnız boş bir belgeye okunabilir. Açmak için AÇ komutunu "
                   "kullanın; var olan bir çizime eklemek için İÇEAKTAR kullanın.");

    auto mapped = MappedFile::open(path);
    if (!mapped) return mapped.error();

    auto parsed = BlockView::parse(mapped.value().bytes(), path);
    if (!parsed) return parsed.error();
    const BlockView& view = parsed.value();

    auto record = view.document_record();
    if (!record) return record.error();
    const DocumentRecord dr = record.value();

    auto spans =
        view.column<StringSpan>(kBlkStringSpans, view.count_of(kBlkStringSpans), "metin dizini");
    if (!spans) return spans.error();
    const Strings strings(view.blob(kBlkStringBytes), spans.value());

    Columns cols{};
    if (auto st = load_columns(view, dr, cols); !st) return st.error();
    if (auto st = validate_indices(dr, cols); !st) return st.error();

    ProjectReport report;
    report.format_version       = view.header().format_version;
    report.bytes                = view.header().file_bytes;
    report.stored_content_hash  = view.header().content_hash;
    report.stored_settings_hash = view.header().settings_hash;

    if (view.unknown_blocks() != 0)
        report.warnings.push_back(
            Warning{"io.unknown_block",
                    "Dosyada bu sürümün tanımadığı " + std::to_string(view.unknown_blocks()) +
                        " veri bloğu var; içerikleri korunmadı. Dosyayı yazan PiriCAD sürümüyle "
                        "açarsanız tamamını görürsünüz."});

    // ---- CRS. io.md R20: never a silent assumption. ----
    auto crs = strings.at(dr.crs_string, "koordinat sistemi");
    if (!crs) return crs.error();
    if (crs.value().empty())
        return err(ErrorCode::ValidationFailed,
                   std::string(kErrConsist) +
                       ": dosya bir koordinat sistemi bildirmiyor. Etiketsiz koordinat kabul "
                       "edilmez; dosyayı yazan programda projeksiyonu ayarlayıp yeniden "
                       "kaydedin.");
    // Set by ID only. The reader has no bus and therefore no way to resolve, so
    // the metadata is filled in by `FileService::open` once the load succeeds —
    // the layer that does hold the seam.
    if (auto st = tx.set_crs(core::Crs(crs.value())); !st) return st.error();

    // ---- layers ----
    auto layer_rows = view.column<LayerRecord>(kBlkLayers, dr.layer_count, "katmanlar");
    if (!layer_rows) return layer_rows.error();

    // Where each layer sits in the tree. OPTIONAL: a file written before the tree
    // existed has no such block and every layer reads back at the root, which is
    // what that file meant.
    std::span<const std::uint32_t> group_rows;
    if (view.has(kBlkLayerGroups) && dr.layer_count > 0) {
        auto rows = view.column<std::uint32_t>(kBlkLayerGroups, dr.layer_count, "katman gruplari");
        if (!rows) return rows.error();
        group_rows = rows.value();
    }

    std::span<const core::StyleId> layer_style_rows;
    if (view.has(kBlkLayerStyles) && dr.layer_count > 0) {
        auto rows = view.column<core::StyleId>(kBlkLayerStyles, dr.layer_count, "katman stilleri");
        if (!rows) return rows.error();
        layer_style_rows = rows.value();
    }

    const core::Layer kDefaults{};
    std::vector<bool> locked(static_cast<std::size_t>(dr.layer_count), false);

    for (std::uint64_t i = 0; i < dr.layer_count; ++i) {
        const LayerRecord& r = layer_rows.value()[static_cast<std::size_t>(i)];

        auto name = strings.at(r.name_string, "katman adı");
        if (!name) return name.error();

        const core::LayerId slot = tx.ensure_layer(name.value());
        if (slot == core::kNoLayer)
            return err(ErrorCode::ValidationFailed,
                       std::string(kErrConsist) + ": '" + name.value() +
                           "' katmanı oluşturulamadı. Dosyada aynı ada gelen iki katman olabilir.");
        if (slot != static_cast<core::LayerId>(i))
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": '" + name.value() + "' katmanı " +
                           std::to_string(slot) + ". sıraya düştü, dosyada " + std::to_string(i) +
                           ". sırada. Katman sırası nesnelerin katman sütununu adresler; dosya "
                           "bozuk.");
        if (raw(doc.layer_key_of(slot)) != r.key)
            return err(ErrorCode::ParseError,
                       std::string(kErrKey) + ": '" + name.value() + "' katmanının anahtarı " +
                           std::to_string(raw(doc.layer_key_of(slot))) + " oldu, dosyada " +
                           std::to_string(r.key) +
                           ". Katman anahtarı kalıcıdır ve yeniden üretilemez; dosya bozuk.");

        const core::Layer* live = doc.layer(slot);
        if (!live) return err(ErrorCode::Internal, "Katman kaydı okunamadı: " + name.value());

        // Visibility BEFORE the entities, because a new entity mirrors its
        // layer's hidden bit at creation (model.md R7).
        if (live->visible != (r.visible != 0))
            if (auto st = tx.set_layer_visible(slot, r.visible != 0); !st) return st.error();

        const core::Appearance appearance = from_record(r.appearance);
        if (!(live->appearance == appearance))
            if (auto st = tx.set_layer_appearance(slot, appearance); !st) return st.error();

        if (!group_rows.empty()) {
            auto group = strings.at(group_rows[static_cast<std::size_t>(i)], "katman grubu");
            if (!group) return group.error();
            if (live->group != group.value())
                if (auto st = tx.set_layer_group(slot, group.value()); !st) return st.error();
        }

        // Locking is deferred: a locked layer refuses new geometry, and the
        // entities on it have not been created yet.
        locked[static_cast<std::size_t>(i)] = r.locked != 0;

        // The six fields no mutator can reach. See the file header.
        const auto unsupported = [&](const char* field) {
            report.warnings.push_back(
                Warning{"io.field_unsupported",
                        "'" + name.value() + "' katmanının '" + field +
                            "' alanı dosyada varsayılandan farklı, ama bu sürümde geri "
                            "yüklenemiyor; varsayılanla açıldı."});
        };
        auto description = strings.at(r.description_string, "katman açıklaması");
        if (!description) return description.error();
        auto catalog_ref = strings.at(r.catalog_ref_string, "katman katalog künyesi");
        if (!catalog_ref) return catalog_ref.error();

        if (description.value() != kDefaults.description) unsupported("açıklama");
        if (!catalog_ref.value().empty()) unsupported("katalog künyesi");
        if ((r.plottable != 0) != kDefaults.plottable) unsupported("çizdirilebilir");
        if (r.min_scale != kDefaults.min_scale) unsupported("en küçük ölçek");
        if (r.max_scale != kDefaults.max_scale) unsupported("en büyük ölçek");
        if (r.opacity != kDefaults.opacity) unsupported("saydamlık");
    }

    // ---- styles ----
    auto style_rows = view.column<AppearanceRecord>(kBlkStyles, dr.style_count, "stiller");
    if (!style_rows) return style_rows.error();

    // The stack behind each style, when the file carries one. OPTIONAL: a file
    // written before symbols were persisted has neither block and every style is
    // a plain colour and width — which is what such a drawing meant.
    std::span<const SymbolRecord> symbol_rows;
    std::span<const SymbolLayerRecord> symbol_layer_rows;

    if (view.has(kBlkSymbols) && dr.style_count > 0) {
        auto rows = view.column<SymbolRecord>(kBlkSymbols, dr.style_count, "semboller");
        if (!rows) return rows.error();
        symbol_rows = rows.value();
    }
    if (view.has(kBlkSymbolLayers) && dr.symbol_layer_count > 0) {
        auto rows = view.column<SymbolLayerRecord>(kBlkSymbolLayers, dr.symbol_layer_count,
                                                   "sembol katmanlari");
        if (!rows) return rows.error();
        symbol_layer_rows = rows.value();
    }

    // Whether each symbol layer is drawn. OPTIONAL: absent means every layer is.
    std::span<const std::uint8_t> symbol_layer_flags;
    if (view.has(kBlkSymbolLayerFlags) && dr.symbol_layer_count > 0) {
        auto rows = view.column<std::uint8_t>(kBlkSymbolLayerFlags, dr.symbol_layer_count,
                                              "sembol katmani bayraklari");
        if (!rows) return rows.error();
        symbol_layer_flags = rows.value();
    }

    std::span<const std::int32_t> symbol_layer_phase;
    if (view.has(kBlkSymbolLayerPhase) && dr.symbol_layer_count > 0) {
        auto rows = view.column<std::int32_t>(kBlkSymbolLayerPhase, dr.symbol_layer_count,
                                              "sembol katmani fazlari");
        if (!rows) return rows.error();
        symbol_layer_phase = rows.value();
    }

    std::span<const std::uint32_t> symbol_layer_text;
    if (view.has(kBlkSymbolLayerText) && dr.symbol_layer_count > 0) {
        auto rows = view.column<std::uint32_t>(kBlkSymbolLayerText, dr.symbol_layer_count,
                                               "sembol katmani yazilari");
        if (!rows) return rows.error();
        symbol_layer_text = rows.value();
    }

    for (std::uint64_t i = 0; i < dr.style_count; ++i) {
        const core::Appearance a = from_record(style_rows.value()[static_cast<std::size_t>(i)]);

        core::Symbol sym;
        if (!symbol_rows.empty()) {
            const SymbolRecord& r = symbol_rows[static_cast<std::size_t>(i)];
            // Bounded against the block that is ACTUALLY THERE, not against the
            // count the header claims. A truncated file has a header saying a
            // hundred symbol layers and a block holding none, and checking the
            // claim rather than the block is how a bounds check that reads like
            // one lets a hostile file walk off the end. The truncation test in
            // /tests/unit caught exactly that, on the first run.
            if (static_cast<std::uint64_t>(r.first_layer) + r.layer_count >
                symbol_layer_rows.size())
                return err(ErrorCode::ParseError,
                           std::string(kErrConsist) + ": " + std::to_string(i) +
                               ". sembol, katman tablosunun dışını gösteriyor.");

            sym.min_scale = r.min_scale;
            sym.max_scale = r.max_scale;
            for (std::uint32_t k = 0; k < r.layer_count; ++k) {
                core::SymbolLayer layer = from_record(symbol_layer_rows[r.first_layer + k]);
                if (r.first_layer + k < symbol_layer_phase.size())
                    layer.phase =
                        core::Measure{symbol_layer_phase[r.first_layer + k], layer.interval.unit};

                if (r.first_layer + k < symbol_layer_flags.size()) {
                    const std::uint8_t flags = symbol_layer_flags[r.first_layer + k];
                    layer.enabled            = (flags & 1u) != 0;
                    layer.colour_locked      = (flags & 2u) != 0;
                }
                if (r.first_layer + k < symbol_layer_text.size()) {
                    auto text =
                        strings.at(symbol_layer_text[r.first_layer + k], "sembol katmani yazisi");
                    if (!text) return text.error();
                    layer.text = std::move(text.value());
                }
                sym.layers.push_back(layer);
            }
        }

        // Interned through the SAME road a command takes, so a stack that is one
        // plain layer collapses back onto its bare appearance exactly as it did
        // when it was written — which is what keeps the ids in step.
        const core::StyleId id = sym.layers.empty() ? tx.intern_style(a) : tx.intern_symbol(sym);
        if (id != static_cast<core::StyleId>(i))
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(i) + ". stil kaydı " +
                           std::to_string(id) +
                           " kimliğine düştü. Stil tablosu tekilleştirilmiştir; dosyada "
                           "yinelenen stil var ve nesnelerin stil sütunu yanlış yere bakardı.");
    }

    // Styles are interned only now, so resolve layer defaults after the table is
    // complete. Older files have no block and retain their Appearance fallback.
    for (std::size_t i = 0; i < layer_style_rows.size(); ++i) {
        const core::StyleId style = layer_style_rows[i];
        if (style != core::kByLayerStyle && !doc.styles().contains(style))
            return err(ErrorCode::ParseError, std::string(kErrConsist) + ": " + std::to_string(i) +
                                                  ". katman, olmayan stil kimliği " +
                                                  std::to_string(style) + " kullanıyor.");
        if (style != core::kByLayerStyle) {
            if (auto st = tx.set_layer_style(static_cast<core::LayerId>(i), style); !st)
                return st.error();
        }
    }

    // ---- embedded pictures ----
    //
    // Read BEFORE the entities so a symbol layer that names one finds it there.
    // The count comes from the directory rather than from the document record,
    // which has no reserved field left; `column` already refuses a block whose
    // length disagrees with count × stride.
    if (view.has(kBlkImages)) {
        auto rows = view.column<ImageRecord>(kBlkImages, view.count_of(kBlkImages), "gorseller");
        if (!rows) return rows.error();

        auto payload = view.column<std::byte>(kBlkImageBytes, view.count_of(kBlkImageBytes),
                                              "gorsel baytlari");
        if (!payload) return payload.error();

        for (std::uint64_t i = 0; i < rows.value().size(); ++i) {
            const ImageRecord& r = rows.value()[static_cast<std::size_t>(i)];

            // Bounded against the block that is ACTUALLY THERE. A truncated file
            // has a record claiming a megabyte and a block holding nothing, and
            // checking the claim rather than the block is how a bounds check that
            // reads like one lets a hostile file walk off the end.
            if (r.offset > payload.value().size() || r.bytes > payload.value().size() - r.offset)
                return err(ErrorCode::ParseError,
                           std::string(kErrConsist) + ": " + std::to_string(i) +
                               ". görsel, bayt tablosunun dışını gösteriyor.");

            auto origin = strings.at(r.origin, "gorsel kaynagi");
            if (!origin) return origin.error();

            auto id = tx.intern_image(payload.value().subspan(static_cast<std::size_t>(r.offset),
                                                              static_cast<std::size_t>(r.bytes)),
                                      origin.value());
            if (!id) return id.error();

            // Slot 0 is the sentinel, so record i is id i+1. A mismatch means the
            // file holds two identical pictures under different ids, and every
            // symbol layer pointing at the second one would silently draw the
            // first.
            if (id.value() != static_cast<core::ImageId>(i + 1))
                return err(ErrorCode::ParseError,
                           std::string(kErrConsist) + ": " + std::to_string(i) + ". görsel kaydı " +
                               std::to_string(id.value()) +
                               " kimliğine düştü. Görsel tablosu tekilleştirilmiştir; "
                               "dosyada yinelenen görsel var.");
        }
    }

    // ---- line types ----
    //
    // OPTIONAL, and its absence is not a defect: every file written before line
    // types existed has no such block and every stroke in it was solid, which is
    // exactly what an empty store draws (io.md R10).
    if (view.has(kBlkDashes)) {
        auto rows = view.column<DashRecord>(kBlkDashes, view.count_of(kBlkDashes), "cizgi tipleri");
        if (!rows) return rows.error();

        for (std::uint64_t i = 0; i < rows.value().size(); ++i) {
            const DashRecord& r = rows.value()[static_cast<std::size_t>(i)];

            core::DashPattern pattern;
            pattern.count = r.count;
            for (std::size_t k = 0; k < core::kMaxDashSegments; ++k)
                pattern.lengths[k] = r.lengths[k];

            auto origin = strings.at(r.origin, "cizgi tipi kaynagi");
            if (!origin) return origin.error();

            // `intern_dash` is where the shape rules live — even count, no zero
            // segment, within the cap — so a hostile or corrupt file is refused
            // by the same check a catalogue is, and this loop restates none of it.
            auto id = tx.intern_dash(pattern, origin.value());
            if (!id) return id.error();

            // Slot 0 is the sentinel, so record i is id i+1. A mismatch means the
            // file holds one pattern twice and every stroke pointing at the second
            // would silently draw the first.
            if (id.value() != static_cast<core::DashId>(i + 1))
                return err(ErrorCode::ParseError,
                           std::string(kErrConsist) + ": " + std::to_string(i) +
                               ". çizgi tipi kaydı " + std::to_string(id.value()) +
                               " kimliğine düştü. Çizgi tipi tablosu tekilleştirilmiştir; "
                               "dosyada yinelenen desen var.");
        }
    }

    // ---- the drafting guides ----
    //
    // OPTIONAL, and both columns or neither: a file with one of them is corrupt
    // rather than old, and saying so is better than silently drawing half the
    // guides at coordinate zero.
    if (view.has(kBlkGuideAxis) || view.has(kBlkGuideCoord)) {
        const std::uint64_t n = view.count_of(kBlkGuideAxis);
        if (n != view.count_of(kBlkGuideCoord))
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) +
                           ": kılavuz eksen ve koordinat sütunları farklı uzunlukta (" +
                           std::to_string(n) + " / " + std::to_string(view.count_of(kBlkGuideCoord)) +
                           ").");

        auto axes = view.column<std::uint8_t>(kBlkGuideAxis, n, "kilavuz ekseni");
        if (!axes) return axes.error();
        auto coords = view.column<std::int64_t>(kBlkGuideCoord, n, "kilavuz koordinati");
        if (!coords) return coords.error();

        std::vector<core::GuideAxis> parsed_axes;
        std::vector<core::Mm> parsed_coords;
        parsed_axes.reserve(static_cast<std::size_t>(n));
        parsed_coords.reserve(static_cast<std::size_t>(n));

        for (std::uint64_t i = 0; i < n; ++i) {
            const std::uint8_t raw_axis = axes.value()[static_cast<std::size_t>(i)];
            if (raw_axis > 1)
                return err(ErrorCode::ParseError,
                           std::string(kErrConsist) + ": " + std::to_string(i) +
                               ". kılavuzun ekseni tanınmıyor (" + std::to_string(raw_axis) +
                               "). Beklenen: 0 yatay, 1 düşey.");

            parsed_axes.push_back(static_cast<core::GuideAxis>(raw_axis));
            parsed_coords.push_back(
                static_cast<core::Mm>(coords.value()[static_cast<std::size_t>(i)]));
        }
        // Through the transaction, like the dashes above: the reader builds the
        // document the way every other client does, so a partly-read file rolls
        // back whole rather than leaving half a guide list behind.
        for (std::size_t g = 0; g < parsed_axes.size(); ++g)
            if (auto st = tx.add_guide(parsed_axes[g], parsed_coords[g]); !st) return st.error();
    }

    // ---- entities ----
    std::vector<core::Point2> points;
    std::vector<core::RingGeometry::RingInput> rings;

    for (std::uint64_t e = 0; e < dr.entity_count; ++e) {
        if ((e % kCancelStride) == 0 && stop.stop_requested()) return cancelled();

        const std::uint32_t slot  = cols.slot[static_cast<std::size_t>(e)];
        const std::uint32_t first = cols.first_ring[slot];
        const std::uint32_t total = cols.ring_total[slot];

        rings.clear();
        points.clear();

        // One flat point buffer per entity, with the ring inputs pointing into
        // it, so a parcel with holes costs one allocation and not one per ring.
        //
        // The running total is checked against the vertex count on EVERY step,
        // not once at the end. Ring ranges are bounded individually but nothing
        // stops a hostile file from pointing a thousand rings at the same range:
        // the sum then exceeds anything the file actually holds, and left
        // unchecked it wraps `std::size_t` and under-allocates the buffer the
        // loop below is about to write into. Checking as we go keeps the sum
        // under 2·vertex_count, so it cannot wrap before it is caught.
        std::size_t needed = 0;
        for (std::uint32_t r = 0; r < total; ++r) {
            needed += cols.ring_count[first + r];
            if (needed > dr.vertex_count)
                return err(ErrorCode::ParseError,
                           std::string(kErrConsist) + ": " + std::to_string(e + 1) +
                               ". nesnenin halkaları dosyadaki toplam tepe noktası sayısından (" +
                               std::to_string(dr.vertex_count) +
                               ") fazlasını istiyor; halka aralıkları çakışıyor. Dosya bozuk.");
        }
        points.resize(needed);

        std::size_t cursor = 0;
        for (std::uint32_t r = 0; r < total; ++r) {
            const std::uint32_t ring  = first + r;
            const std::uint32_t start = cols.ring_start[ring];
            const std::uint32_t count = cols.ring_count[ring];

            for (std::uint32_t v = 0; v < count; ++v)
                points[cursor + v] = core::Point2{cols.xs[start + v], cols.ys[start + v]};

            rings.push_back(core::RingGeometry::RingInput{
                std::span<const core::Point2>(points.data() + cursor, count),
                static_cast<core::RingRole>(cols.ring_role[ring]), cols.ring_part[ring]});
            cursor += count;
        }

        const auto layer_slot = static_cast<core::LayerId>(cols.layer[static_cast<std::size_t>(e)]);

        // THE KIND COLUMN DECIDES, not the shape of the rings. A circle is stored
        // as one Open ring of two vertices, which is exactly what a two-point line
        // looks like; the column is the only thing that tells them apart, and
        // guessing from the geometry would silently turn every saved circle into a
        // line pointing east (see `core.circle`).
        //
        // A single Open ring is otherwise a polyline and anything else is a face.
        // Both land in Document::add_area, which owns every geometric rule
        // (model.md R9–R12) — the reader restates none of them.
        core::Result<core::EntityId> added = core::err(core::ErrorCode::Internal, "");
        if (cols.kind[static_cast<std::size_t>(e)] == core::kCircleKind) {
            const auto pts = rings.front().points;
            if (total != 1 || pts.size() != 2)
                return err(ErrorCode::ParseError,
                           std::string(kErrKind) + ": " + std::to_string(e + 1) +
                               ". nesne daire olarak işaretli ama merkez ve yarıçapı taşıyan iki "
                               "tepe noktası yok. Dosya bozuk.");
            added = tx.add_circle(layer_slot, pts[0], pts[1].x - pts[0].x);
        } else if (cols.kind[static_cast<std::size_t>(e)] == core::kArcKind) {
            const auto pts = rings.front().points;
            if (total != 1 || pts.size() != 4)
                return err(ErrorCode::ParseError,
                           std::string(kErrKind) + ": " + std::to_string(e + 1) +
                               ". nesne yay olarak işaretli ama merkezi, yarıçapı ve iki ucunu "
                               "taşıyan dört tepe noktası yok. Dosya bozuk.");
            added = tx.add_arc(layer_slot, pts[0], pts[1].x - pts[0].x, pts[2], pts[3]);
        } else if (cols.kind[static_cast<std::size_t>(e)] == core::kPointKind) {
            const auto pts = rings.front().points;
            if (total != 1 || pts.size() != 1)
                return err(ErrorCode::ParseError,
                           std::string(kErrKind) + ": " + std::to_string(e + 1) +
                               ". nesne nokta olarak işaretli ama tek bir tepe noktası yok. "
                               "Dosya bozuk.");
            added = tx.add_point(layer_slot, pts[0]);
        } else {
            added = (total == 1 && rings.front().role == core::RingRole::Open)
                        ? tx.add_polyline(layer_slot, rings.front().points)
                        : tx.add_area(layer_slot, rings);
        }
        if (!added)
            return err(added.error().code,
                       std::to_string(e + 1) + ". nesne okunamadı: " + added.error().message);

        const core::EntityId id = added.value();
        if (id != static_cast<core::EntityId>(e))
            return err(ErrorCode::ParseError,
                       std::string(kErrConsist) + ": " + std::to_string(e + 1) + ". nesne " +
                           std::to_string(id) +
                           ". yuvaya düştü. Nesne sırası anahtar sırasıdır; dosya bozuk.");
        if (raw(doc.key_of(id)) != cols.key[static_cast<std::size_t>(e)])
            return err(ErrorCode::ParseError,
                       std::string(kErrKey) + ": " + std::to_string(e + 1) +
                           ". nesnenin anahtarı " + std::to_string(raw(doc.key_of(id))) +
                           " oldu, dosyada " +
                           std::to_string(cols.key[static_cast<std::size_t>(e)]) +
                           ". Nesne anahtarı kalıcıdır ve yeniden üretilemez; dosya bozuk.");

        const std::uint32_t style_id = cols.style[static_cast<std::size_t>(e)];
        if (style_id != core::kByLayerStyle)
            if (auto st = tx.set_entity_style(id, style_id); !st) return st.error();

        const std::uint8_t flags = cols.flags[static_cast<std::size_t>(e)];
        if ((flags & core::FlagHidden) != 0)
            if (auto st = tx.set_entity_hidden(id, true); !st) return st.error();
        if ((flags & core::FlagAlive) == 0)
            if (auto st = tx.erase_entity(id); !st) return st.error();
    }

    // ---- deferred layer locks ----
    for (std::uint64_t i = 0; i < dr.layer_count; ++i) {
        if (!locked[static_cast<std::size_t>(i)]) continue;
        if (auto st = tx.set_layer_locked(static_cast<core::LayerId>(i), true); !st)
            return st.error();
    }

    // ---- project settings (model.md R39, R42) ----
    auto setting_rows =
        view.column<SettingRecord>(kBlkSettings, view.count_of(kBlkSettings), "proje ayarları");
    if (!setting_rows) return setting_rows.error();

    for (const SettingRecord& r : setting_rows.value()) {
        auto id = strings.at(r.id_string, "ayar kimliği");
        if (!id) return id.error();

        const std::uint32_t index = settings.catalogue().find(id.value());
        if (index == core::kNoSetting) {
            report.warnings.push_back(
                Warning{"io.setting_unknown", "'" + id.value() +
                                                  "' ayarı bu sürümde tanımlı değil; yok "
                                                  "sayıldı. Dosyayı yazan sürümde tanımlı."});
            continue;
        }
        const core::SettingSpec& spec = settings.catalogue().at(index);
        if (spec.scope != core::SettingScope::Project) {
            report.warnings.push_back(
                Warning{"io.setting_scope",
                        "'" + spec.id + "' ayarı dosyada duruyor ama " +
                            core::setting_scope_label(spec.scope) +
                            " kapsamında; yok sayıldı. Dosyaya yalnız proje ayarları girer."});
            continue;
        }
        if (static_cast<core::SettingType>(r.type) != spec.type) {
            report.warnings.push_back(
                Warning{"io.setting_type", "'" + spec.id +
                                               "' ayarının türü bu sürümde değişmiş; dosyadaki "
                                               "değer yok sayıldı ve varsayılan kullanıldı."});
            continue;
        }

        core::SettingValue value{};
        switch (spec.type) {
        case core::SettingType::Bool: value = core::SettingValue::boolean(r.scalar != 0); break;
        case core::SettingType::Int: value = core::SettingValue::integer(r.scalar); break;
        case core::SettingType::Length: value = core::SettingValue::length(r.scalar); break;
        case core::SettingType::Enum:
            value = core::SettingValue::enumerated(static_cast<std::uint16_t>(r.scalar));
            break;
        case core::SettingType::Text: {
            // The buffer is fixed width and NOT NUL-terminated by contract, so the
            // length is the first NUL or the whole buffer — never strlen past it.
            const auto* nul = static_cast<const char*>(std::memchr(r.text, '\0', sizeof(r.text)));
            const std::size_t n = nul ? static_cast<std::size_t>(nul - r.text) : sizeof(r.text);
            auto text           = core::SettingValue::text(std::string_view(r.text, n));
            if (!text) {
                report.warnings.push_back(
                    Warning{"io.setting_text", "'" + spec.id + "' ayarı: " + text.error().message});
                continue;
            }
            value = text.value();
            break;
        }
        }

        settings.clear_warnings();
        auto applied = settings.set(spec.id, value);
        if (!applied) {
            report.warnings.push_back(
                Warning{"io.setting_rejected",
                        "'" + spec.id + "' ayarı yüklenemedi: " + applied.error().message});
            continue;
        }
        for (const core::SettingWarning& w : settings.warnings())
            report.warnings.push_back(Warning{"io.setting_clamped", w.message});
    }

    // ---- attributes (model.md R27-R29) ----
    //
    // The schema first, in file order, because the order IS the AttrId every cell
    // refers to. A column this build cannot construct is a warning and not a
    // failure: the cells that point at it are then dropped by index, which is
    // lossy and said out loud, rather than shifting every later column by one.
    {
        auto schema = view.column<AttrColumnRecord>(kBlkAttrSchema, view.count_of(kBlkAttrSchema),
                                                    "öznitelik şeması");
        if (!schema) return schema.error();

        std::vector<core::AttrId> columns;
        columns.reserve(schema.value().size());

        for (const AttrColumnRecord& r : schema.value()) {
            auto id = strings.at(r.id_string, "öznitelik kimliği");
            if (!id) return id.error();
            auto name = strings.at(r.name_string, "öznitelik adı");
            if (!name) return name.error();
            auto summary = strings.at(r.summary_string, "öznitelik açıklaması");
            if (!summary) return summary.error();
            auto catalog = strings.at(r.catalog_string, "öznitelik kataloğu");
            if (!catalog) return catalog.error();

            if (r.type > static_cast<std::uint8_t>(core::AttrType::CodeRef)) {
                report.warnings.push_back(
                    Warning{"io.attr_type", "'" + id.value() +
                                                "' özniteliğinin türü bu sürümde tanınmıyor; "
                                                "sütun ve hücreleri yüklenmedi."});
                columns.push_back(core::kNoAttr);
                continue;
            }

            core::AttrSpec spec;
            spec.id         = id.value();
            spec.name_tr    = name.value();
            spec.summary_tr = summary.value();
            spec.catalog    = catalog.value();
            spec.type       = static_cast<core::AttrType>(r.type);
            spec.required   = r.required != 0;

            auto made = tx.declare_attribute(std::move(spec));
            if (!made) {
                report.warnings.push_back(Warning{
                    "io.attr_column",
                    "'" + id.value() + "' özniteliği yüklenemedi: " + made.error().message});
                columns.push_back(core::kNoAttr);
                continue;
            }
            columns.push_back(made.value());
        }

        auto cells = view.column<AttrCellRecord>(kBlkAttrCells, view.count_of(kBlkAttrCells),
                                                 "öznitelik hücreleri");
        if (!cells) return cells.error();

        for (const AttrCellRecord& c : cells.value()) {
            if (c.column >= columns.size() || columns[c.column] == core::kNoAttr) continue;
            if (c.row >= doc.entities().size()) {
                report.warnings.push_back(Warning{"io.attr_row",
                                                  "Dosyadaki bir öznitelik hücresi var olmayan bir "
                                                  "nesneye işaret ediyor; yok sayıldı."});
                continue;
            }

            const core::AttrColumn* col = doc.attributes().column(columns[c.column]);
            if (col == nullptr) continue;

            core::AttrValue v{};
            v.type    = col->type();
            v.present = true;
            v.number  = c.number;
            if (v.type == core::AttrType::Text || v.type == core::AttrType::CodeRef) {
                auto text = strings.at(c.text_string, "öznitelik değeri");
                if (!text) return text.error();
                v.text = text.value();
            }

            // The row IS the entity slot, and slots are dense and in order here,
            // so the slot is its own entity id at load time.
            if (auto st =
                    tx.set_attribute(columns[c.column], static_cast<core::EntityId>(c.row), v);
                !st)
                report.warnings.push_back(Warning{
                    "io.attr_cell", "Bir öznitelik değeri yüklenemedi: " + st.error().message});
        }
    }

    // ---- text ----
    {
        auto rows = view.column<TextRecord>(kBlkTexts, view.count_of(kBlkTexts), "metinler");
        if (!rows) return rows.error();

        for (const TextRecord& r : rows.value()) {
            if (r.row >= doc.entities().size()) {
                report.warnings.push_back(
                    Warning{"io.text_row", "Dosyadaki bir metin var olmayan bir nesneye işaret "
                                           "ediyor; yok sayıldı."});
                continue;
            }
            auto content = strings.at(r.content_string, "metin içeriği");
            if (!content) return content.error();

            if (auto st = tx.set_text(static_cast<core::EntityId>(r.row), content.value(),
                                      r.height_mm, static_cast<core::TextAnchor>(r.anchor));
                !st)
                report.warnings.push_back(
                    Warning{"io.text", "Bir metin yüklenemedi: " + st.error().message});
        }
    }

    // ---- the allocator must not hand out a key the file already used ----
    if (doc.keys().peek_entity() != dr.next_entity_key ||
        doc.keys().peek_layer() != dr.next_layer_key)
        report.warnings.push_back(Warning{
            "io.key_counter", "Anahtar sayacı dosyadakinden farklı yerde durdu (nesne " +
                                  std::to_string(doc.keys().peek_entity()) + "/" +
                                  std::to_string(dr.next_entity_key) + ", katman " +
                                  std::to_string(doc.keys().peek_layer()) + "/" +
                                  std::to_string(dr.next_layer_key) +
                                  "). Belge doğru yüklendi; yeni nesneler farklı anahtar alacak."});

    // ---- what the writer said this document was ----
    if (doc.content_hash() != view.header().content_hash)
        report.warnings.push_back(Warning{
            "io.hash_mismatch", "Dosyanın içerik parmak izi tutmuyor: kaydedilirken " +
                                    std::to_string(view.header().content_hash) + ", okununca " +
                                    std::to_string(doc.content_hash()) +
                                    ". Belge açıldı; kaydetmeden önce çiziminizi gözden geçirin."});

    // The bounding boxes in the file are the cull block, mapped rather than
    // recomputed (model.md R6). They are a HINT like every other number in the
    // file, so they are compared with what the geometry actually says and never
    // trusted over it.
    for (std::uint64_t e = 0; e < dr.entity_count; ++e) {
        const auto id = static_cast<core::EntityId>(e);
        const core::Box2 box{
            cols.min_x[static_cast<std::size_t>(e)], cols.min_y[static_cast<std::size_t>(e)],
            cols.max_x[static_cast<std::size_t>(e)], cols.max_y[static_cast<std::size_t>(e)]};
        if (doc.entities().box_of(id) == box) continue;
        report.warnings.push_back(
            Warning{"io.bbox_mismatch",
                    std::to_string(e + 1) +
                        ". nesnenin dosyadaki sınırlayıcı kutusu geometrisiyle uyuşmuyor; "
                        "geometriden yeniden hesaplandı."});
        break; // one warning is the message; ten thousand is noise
    }

    report.entities = static_cast<std::uint64_t>(doc.live_entity_count());
    report.layers   = dr.layer_count;
    report.vertices = dr.vertex_count;
    return report;
}

} // namespace

command::Task<core::Result<ProjectReport>> read_project(command::Transaction& tx, std::string path,
                                                        core::Settings& settings,
                                                        std::stop_token stop)
{
    co_return load(tx, path, settings, stop);
}

} // namespace piricad::io
