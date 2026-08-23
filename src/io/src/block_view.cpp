// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_view.hpp"

#include <algorithm>

namespace piricad::io {
namespace {

using core::err;
using core::ErrorCode;

/// `a + b` without wrapping. Every offset arithmetic below goes through this:
/// an offset of 2^64-8 plus a length of 16 wraps to 8, which would place a block
/// "inside" the file while pointing far outside the mapping. That is the classic
/// way a bounds check is bypassed, and it costs one comparison to close.
bool add_overflows(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
{
    if (a > UINT64_MAX - b) return true;
    out = a + b;
    return false;
}

std::string known_block_name(std::uint32_t id)
{
    switch (id) {
    case kBlkStringBytes: return "metin havuzu";
    case kBlkStringSpans: return "metin dizini";
    case kBlkDocument: return "belge kaydı";
    case kBlkLayers: return "katmanlar";
    case kBlkStyles: return "stiller";
    case kBlkSymbols: return "semboller";
    case kBlkLayerGroups: return "katman gruplari";
    case kBlkSymbolLayerFlags: return "sembol katmani bayraklari";
    case kBlkImages: return "gorseller";
    case kBlkImageBytes: return "gorsel baytlari";
    case kBlkSymbolLayers: return "sembol katmanlari";
    case kBlkEntityMinX:
    case kBlkEntityMinY:
    case kBlkEntityMaxX:
    case kBlkEntityMaxY: return "sınırlayıcı kutu sütunu";
    case kBlkEntityFlags: return "nesne bayrakları";
    case kBlkEntityLayer: return "nesne katmanı";
    case kBlkEntityStyle: return "nesne stili";
    case kBlkEntityKind: return "nesne türü";
    case kBlkEntitySlot: return "nesne geometri yuvası";
    case kBlkEntityKey: return "nesne anahtarları";
    case kBlkRingStart:
    case kBlkRingCount:
    case kBlkRingPart:
    case kBlkRingRole: return "halka sütunu";
    case kBlkSlotFirstRing:
    case kBlkSlotRingTotal: return "yuva-halka sütunu";
    case kBlkVertexX:
    case kBlkVertexY: return "tepe noktası sütunu";
    case kBlkSettings: return "proje ayarları";
    case kBlkAttrSchema: return "öznitelik şeması";
    case kBlkAttrCells: return "öznitelik hücreleri";
    case kBlkTexts: return "metinler";
    default: return {};
    }
}

} // namespace

core::Result<BlockView> BlockView::parse(std::span<const std::byte> bytes, const std::string& path)
{
    BlockView view;
    view.bytes_ = bytes;

    const std::uint64_t file_size = bytes.size();

    if (file_size < sizeof(FileHeader))
        return err(ErrorCode::ParseError, std::string(kErrTruncated) + ": '" + path + "' " +
                                              std::to_string(file_size) +
                                              " bayt; bir PiriCAD proje dosyasının başlığı bile " +
                                              std::to_string(sizeof(FileHeader)) + " bayttır.");

    view.header_ = read_record<FileHeader>(bytes, 0);

    if (std::memcmp(view.header_.magic, kMagic, sizeof(kMagic)) != 0)
        return err(ErrorCode::ParseError,
                   std::string(kErrNotPiri) + ": '" + path +
                       "' bir PiriCAD proje dosyası değil. Dış biçimler için İÇEAKTAR "
                       "komutunu kullanın.");

    // io.md R9. The file states the lowest reader that can still make sense of
    // it; if that is beyond us we refuse by NAME, never by crashing and never by
    // loading half of it.
    if (view.header_.min_reader_version > kFormatVersion)
        return err(ErrorCode::Unsupported,
                   std::string(kErrTooNew) + ": '" + path + "' en az " +
                       std::to_string(view.header_.min_reader_version) +
                       ". sürüm biçim okuyucusu istiyor; bu yapı " +
                       std::to_string(kFormatVersion) +
                       ". sürümü okuyor. Dosyayı yazan PiriCAD sürümüne yükseltin.");

    if (view.header_.header_bytes < sizeof(FileHeader) || view.header_.header_bytes > file_size)
        return err(ErrorCode::ParseError,
                   std::string(kErrBlock) + ": '" + path + "' başlık uzunluğu " +
                       std::to_string(view.header_.header_bytes) +
                       " olarak bildirilmiş; dosyaya sığmıyor. Dosya bozuk.");

    if (view.header_.file_bytes != file_size)
        return err(ErrorCode::ParseError,
                   std::string(kErrTruncated) + ": '" + path + "' " +
                       std::to_string(view.header_.file_bytes) + " bayt olduğunu bildiriyor, ama " +
                       std::to_string(file_size) +
                       " bayt. Dosya yarım kopyalanmış ya da kesilmiş olabilir; yedeğinden "
                       "geri alın.");

    if (view.header_.block_count > kMaxBlocks)
        return err(ErrorCode::ParseError, std::string(kErrBlock) + ": '" + path + "' " +
                                              std::to_string(view.header_.block_count) +
                                              " blok bildiriyor; üst sınır " +
                                              std::to_string(kMaxBlocks) + ". Dosya bozuk.");

    if ((view.header_.directory_offset % kAlignment) != 0)
        return err(ErrorCode::ParseError, std::string(kErrBlock) + ": '" + path +
                                              "' dizin başlangıcı " +
                                              std::to_string(view.header_.directory_offset) +
                                              ", 8 baytlık hizaya oturmuyor. Dosya bozuk.");

    std::uint64_t dir_end = 0;
    if (add_overflows(view.header_.directory_offset,
                      std::uint64_t{view.header_.block_count} * sizeof(BlockEntry), dir_end) ||
        dir_end > file_size)
        return err(ErrorCode::ParseError,
                   std::string(kErrTruncated) + ": '" + path +
                       "' blok dizini dosyanın dışına taşıyor. Dosya bozuk ya da kesilmiş.");

    // ---- the directory ----
    view.entries_.reserve(view.header_.block_count);
    for (std::uint32_t i = 0; i < view.header_.block_count; ++i) {
        const BlockEntry e = read_record<BlockEntry>(
            bytes, view.header_.directory_offset + std::uint64_t{i} * sizeof(BlockEntry));

        const std::string label = known_block_name(e.id).empty()
                                      ? ("blok 0x" + std::to_string(e.id))
                                      : known_block_name(e.id);

        if ((e.offset % kAlignment) != 0)
            return err(ErrorCode::ParseError, std::string(kErrBlock) + ": " + label +
                                                  " başlangıcı 8 baytlık hizaya "
                                                  "oturmuyor. Dosya bozuk.");

        std::uint64_t end = 0;
        if (add_overflows(e.offset, e.bytes, end) || end > file_size)
            return err(ErrorCode::ParseError,
                       std::string(kErrTruncated) + ": " + label + " [" + std::to_string(e.offset) +
                           ", " + std::to_string(e.offset) + "+" + std::to_string(e.bytes) +
                           ") dosyanın dışına taşıyor (" + std::to_string(file_size) +
                           " bayt). Dosya bozuk ya da kesilmiş.");

        if (e.elem_bytes != 0) {
            std::uint64_t declared = 0;
            if (e.count != 0 && e.elem_bytes > UINT64_MAX / e.count)
                return err(ErrorCode::ParseError, std::string(kErrBlock) + ": " + label +
                                                      " öğe sayısı × öğe boyu taşıyor. "
                                                      "Dosya bozuk.");
            declared = e.count * e.elem_bytes;
            if (declared != e.bytes)
                return err(ErrorCode::ParseError,
                           std::string(kErrBlock) + ": " + label + " uzunluğu " +
                               std::to_string(e.bytes) + " bayt, ama " + std::to_string(e.count) +
                               " × " + std::to_string(e.elem_bytes) + " = " +
                               std::to_string(declared) + " olmalı. Dosya bozuk.");
        }

        // A block that starts inside the header would overlap the version fields;
        // one that starts inside the directory would overlap its own entry.
        if (e.bytes != 0 && e.offset < view.header_.header_bytes)
            return err(ErrorCode::ParseError, std::string(kErrBlock) + ": " + label +
                                                  " başlığın üzerine biniyor. Dosya bozuk.");

        for (const auto& seen : view.entries_) {
            if (seen.id == e.id)
                return err(ErrorCode::ParseError, std::string(kErrBlock) + ": " + label +
                                                      " dizinde iki kez geçiyor. Dosya bozuk.");
            if (e.bytes == 0 || seen.bytes == 0) continue;
            const bool disjoint =
                e.offset >= seen.offset + seen.bytes || seen.offset >= e.offset + e.bytes;
            if (!disjoint)
                return err(ErrorCode::ParseError, std::string(kErrBlock) + ": " + label +
                                                      " başka bir blokla çakışıyor. Dosya bozuk.");
        }

        if (known_block_name(e.id).empty()) ++view.unknown_;
        view.entries_.push_back(e);
    }

    return view;
}

core::Result<DocumentRecord> BlockView::document_record() const
{
    const BlockEntry* e = find(kBlkDocument);
    if (!e)
        return err(ErrorCode::ParseError,
                   std::string(kErrConsist) +
                       ": belge kaydı yok. Her PiriCAD proje dosyası tam bir belge kaydı taşır; "
                       "bu dosya bozuk.");
    if (e->elem_bytes != sizeof(DocumentRecord) || e->count != 1)
        return err(ErrorCode::ParseError,
                   std::string(kErrBlock) + ": belge kaydı " + std::to_string(e->count) + " × " +
                       std::to_string(e->elem_bytes) + " bayt, beklenen 1 × " +
                       std::to_string(sizeof(DocumentRecord)) +
                       ". Dosya bozuk ya da başka bir "
                       "sürümden.");
    return read_record<DocumentRecord>(bytes_, e->offset);
}

} // namespace piricad::io
