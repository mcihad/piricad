// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/attribute.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>

namespace piricad::core {
namespace {

constexpr std::size_t kBitsPerWord = 64;

bool is_text_shaped(AttrType t) noexcept
{
    return t == AttrType::Text || t == AttrType::CodeRef;
}

/// Folded into a content hash so an absent cell can never collide with a
/// present zero: "ada numarası kayıtlı değil" and "ada numarası 0" are different
/// facts, and a hash that conflates them hides a data-loss bug.
constexpr std::int64_t kAbsentMarker = -0x7FFF'FFFF'FFFF'FFFFLL;

} // namespace

const char* attr_type_name(AttrType t) noexcept
{
    switch (t) {
    case AttrType::Int64: return "Int64";
    case AttrType::Length: return "Uzunluk";
    case AttrType::Bool: return "Bool";
    case AttrType::Text: return "Text";
    case AttrType::CodeRef: return "CodeRef";
    }
    return "?";
}

AttrValue attr_absent(AttrType t)
{
    AttrValue v;
    v.type    = t;
    v.present = false;
    return v;
}

AttrValue attr_int64(std::int64_t n)
{
    AttrValue v;
    v.type    = AttrType::Int64;
    v.present = true;
    v.number  = n;
    return v;
}

AttrValue attr_mm(Mm n)
{
    AttrValue v;
    v.type    = AttrType::Length;
    v.present = true;
    v.number  = n;
    return v;
}

AttrValue attr_bool(bool b)
{
    AttrValue v;
    v.type    = AttrType::Bool;
    v.present = true;
    v.number  = b ? 1 : 0;
    return v;
}

AttrValue attr_text(std::string s)
{
    AttrValue v;
    v.type    = AttrType::Text;
    v.present = true;
    v.text    = std::move(s);
    return v;
}

AttrValue attr_code(std::string code)
{
    AttrValue v;
    v.type    = AttrType::CodeRef;
    v.present = true;
    v.text    = std::move(code);
    return v;
}

// ------------------------------------------------------------- Catalogue -----

Catalogue::Catalogue(std::string id, std::string package_version)
    : id_(std::move(id)), package_version_(std::move(package_version))
{}

void Catalogue::add_code(std::string code)
{
    const auto at = std::lower_bound(codes_.begin(), codes_.end(), code);
    if (at != codes_.end() && *at == code) return;
    codes_.insert(at, std::move(code));
}

bool Catalogue::contains(std::string_view code) const
{
    const auto at = std::lower_bound(codes_.begin(), codes_.end(), code);
    return at != codes_.end() && *at == code;
}

void CatalogueSet::add(Catalogue c)
{
    const auto at =
        std::lower_bound(items_.begin(), items_.end(), c.id(),
                         [](const Catalogue& e, const std::string& k) { return e.id() < k; });
    if (at != items_.end() && at->id() == c.id()) {
        *at = std::move(c);
        return;
    }
    items_.insert(at, std::move(c));
}

const Catalogue* CatalogueSet::find(std::string_view id) const
{
    const auto at =
        std::lower_bound(items_.begin(), items_.end(), id,
                         [](const Catalogue& e, std::string_view k) { return e.id() < k; });
    if (at == items_.end() || at->id() != id) return nullptr;
    return &*at;
}

// ------------------------------------------------------------ AttrColumn -----

AttrColumn::AttrColumn(AttrSpec spec) : spec_(std::move(spec)) {}

void AttrColumn::resize(std::size_t rows)
{
    if (is_text_shaped(spec_.type))
        codes_.resize(rows, 0);
    else
        numbers_.resize(rows, 0);

    const std::size_t words = (rows + kBitsPerWord - 1) / kBitsPerWord;
    present_.resize(words, 0);

    // Clear the bits past the new end, so shrinking and regrowing cannot bring a
    // cell back from the dead with its old presence bit still set.
    if (rows < rows_ && words > 0) {
        const std::size_t tail = rows % kBitsPerWord;
        if (tail != 0) present_[words - 1] &= (std::uint64_t{1} << tail) - 1;
    }
    rows_ = rows;
}

bool AttrColumn::present(std::size_t row) const noexcept
{
    if (row >= rows_) return false;
    return (present_[row / kBitsPerWord] >> (row % kBitsPerWord)) & 1u;
}

Result<std::uint32_t> AttrColumn::intern(const std::string& s)
{
    if (const auto it = intern_.find(s); it != intern_.end()) return it->second;

    if (pool_.size() >= 0xFFFFFFFFu)
        return err(ErrorCode::Internal, "'" + spec_.name_tr + "' sütununun metin havuzu doldu.");

    const auto id = static_cast<std::uint32_t>(pool_.size());
    pool_.push_back(s);
    intern_.emplace(s, id);
    return id;
}

Result<AttrValue> AttrColumn::set(std::size_t row, const AttrValue& v)
{
    if (row >= rows_)
        return err(ErrorCode::NotFound, "'" + spec_.name_tr + "' sütununda " + std::to_string(row) +
                                            ". satır yok (satır sayısı " + std::to_string(rows_) +
                                            ").");
    if (v.type != spec_.type)
        return err(ErrorCode::InvalidArgument,
                   "'" + spec_.name_tr + "' sütunu " + attr_type_name(spec_.type) +
                       " ister, verilen: " + attr_type_name(v.type) + ".");

    auto previous = get(row);
    if (!previous) return previous.error();

    const std::size_t word  = row / kBitsPerWord;
    const std::uint64_t bit = std::uint64_t{1} << (row % kBitsPerWord);

    if (!v.present) {
        present_[word] &= ~bit;
        return previous;
    }

    if (is_text_shaped(spec_.type)) {
        auto id = intern(v.text);
        if (!id) return id.error();
        codes_[row] = id.value();
    } else {
        // Bool is normalised on the way in, so two documents that recorded "true"
        // as 1 and as 7 hash identically.
        numbers_[row] = (spec_.type == AttrType::Bool) ? (v.number != 0 ? 1 : 0) : v.number;
    }
    present_[word] |= bit;
    return previous;
}

Result<AttrValue> AttrColumn::get(std::size_t row) const
{
    if (row >= rows_)
        return err(ErrorCode::NotFound, "'" + spec_.name_tr + "' sütununda " + std::to_string(row) +
                                            ". satır yok (satır sayısı " + std::to_string(rows_) +
                                            ").");

    AttrValue v;
    v.type    = spec_.type;
    v.present = present(row);
    if (!v.present) return v;

    if (is_text_shaped(spec_.type))
        v.text = pool_[codes_[row]];
    else
        v.number = numbers_[row];
    return v;
}

std::string_view AttrColumn::text(std::size_t row) const
{
    if (!is_text_shaped(spec_.type) || !present(row)) return {};
    return pool_[codes_[row]];
}

std::uint64_t AttrColumn::fold(std::uint64_t seed) const
{
    std::uint64_t h = fnv1a(spec_.id, seed);
    h               = fnv1a_int(static_cast<std::int64_t>(spec_.type), h);

    for (std::size_t row = 0; row < rows_; ++row) {
        if (!present(row)) {
            h = fnv1a_int(kAbsentMarker, h);
            continue;
        }
        if (is_text_shaped(spec_.type))
            h = fnv1a(pool_[codes_[row]], h);
        else
            h = fnv1a_int(numbers_[row], h);
    }
    return h;
}

// ------------------------------------------------------------- AttrTable -----

Result<AttrId> AttrTable::add(AttrSpec spec)
{
    if (spec.id.empty()) return err(ErrorCode::InvalidArgument, "Öznitelik kimliği boş olamaz.");
    if (find(spec.id) != kNoAttr)
        return err(ErrorCode::ValidationFailed, "Öznitelik zaten tanımlı: " + spec.id);
    if (spec.type == AttrType::CodeRef && spec.catalog.empty())
        return err(ErrorCode::InvalidArgument,
                   "'" + spec.id +
                       "' bir katalog kodu sütunu ama hangi kataloğu kullandığı yazılmamış.");
    if (columns_.size() >= kNoAttr)
        return err(ErrorCode::Internal,
                   "Bir tabloda en çok " + std::to_string(kNoAttr) + " öznitelik sütunu olabilir.");

    const auto id = static_cast<AttrId>(columns_.size());
    columns_.emplace_back(std::move(spec));
    columns_.back().resize(rows_);
    return id;
}

AttrId AttrTable::find(std::string_view id) const
{
    for (std::size_t i = 0; i < columns_.size(); ++i)
        if (columns_[i].spec().id == id) return static_cast<AttrId>(i);
    return kNoAttr;
}

const AttrColumn* AttrTable::column(AttrId col) const
{
    return col < columns_.size() ? &columns_[col] : nullptr;
}

AttrColumn* AttrTable::column(AttrId col)
{
    return col < columns_.size() ? &columns_[col] : nullptr;
}

void AttrTable::resize(std::size_t rows)
{
    for (auto& c : columns_)
        c.resize(rows);
    rows_ = rows;
}

Result<AttrValue> AttrTable::set(AttrId col, std::size_t row, const AttrValue& v)
{
    AttrColumn* c = column(col);
    if (c == nullptr)
        return err(ErrorCode::NotFound,
                   "Bilinmeyen öznitelik sütunu: " + std::to_string(col) + ".");
    return c->set(row, v);
}

Result<AttrValue> AttrTable::get(AttrId col, std::size_t row) const
{
    const AttrColumn* c = column(col);
    if (c == nullptr)
        return err(ErrorCode::NotFound,
                   "Bilinmeyen öznitelik sütunu: " + std::to_string(col) + ".");
    return c->get(row);
}

Status AttrTable::validate_required(std::size_t row) const
{
    if (row >= rows_)
        return err(ErrorCode::NotFound, std::to_string(row) + ". satır yok (satır sayısı " +
                                            std::to_string(rows_) + ").");

    for (const auto& c : columns_) {
        if (!c.spec().required || c.present(row)) continue;
        return err(ErrorCode::ValidationFailed, "'" + c.spec().name_tr +
                                                    "' özniteliği zorunlu ama " +
                                                    std::to_string(row) + ". satırda boş.");
    }
    return ok();
}

Status AttrTable::validate_codes(std::size_t row, const CatalogueSet& catalogues) const
{
    if (row >= rows_)
        return err(ErrorCode::NotFound, std::to_string(row) + ". satır yok (satır sayısı " +
                                            std::to_string(rows_) + ").");

    for (const auto& c : columns_) {
        if (c.spec().type != AttrType::CodeRef || !c.present(row)) continue;

        const Catalogue* cat = catalogues.find(c.spec().catalog);
        if (cat == nullptr)
            return err(ErrorCode::NotFound, "'" + c.spec().name_tr + "' özniteliğinin dayandığı " +
                                                c.spec().catalog + " kataloğu yüklü değil.");

        const std::string_view code = c.text(row);
        if (!cat->contains(code))
            return err(ErrorCode::ValidationFailed, "'" + std::string(code) + "' kodu " +
                                                        cat->id() + " kataloğunun " +
                                                        cat->package_version() + " sürümünde yok.");
    }
    return ok();
}

Status AttrTable::validate_row(std::size_t row, const CatalogueSet& catalogues) const
{
    if (auto st = validate_required(row); !st) return st;
    return validate_codes(row, catalogues);
}

std::uint64_t AttrTable::fold(std::uint64_t seed) const
{
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_), seed);
    for (const auto& c : columns_)
        h = c.fold(h);
    return h;
}

} // namespace piricad::core
