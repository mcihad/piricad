// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/attribute.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <chrono>

namespace kentos::core {
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
    case AttrType::Decimal: return "Ondalik";
    case AttrType::Date: return "Tarih";
    }
    return "?";
}

std::optional<AttrType> attr_type_from_name(std::string_view word)
{
    // The words a user types, not the machine names `attr_type_name` prints: a
    // schema file says `Int64` and a person says `tam_sayi`, and this side of the
    // pair is the person's.
    if (turkish_iequals(word, "tam_sayi") || turkish_iequals(word, "tam_sayı"))
        return AttrType::Int64;
    if (turkish_iequals(word, "uzunluk")) return AttrType::Length;
    if (turkish_iequals(word, "evet_hayir") || turkish_iequals(word, "evet_hayır"))
        return AttrType::Bool;
    if (turkish_iequals(word, "metin")) return AttrType::Text;
    if (turkish_iequals(word, "kod")) return AttrType::CodeRef;
    if (turkish_iequals(word, "ondalik") || turkish_iequals(word, "ondalık"))
        return AttrType::Decimal;
    if (turkish_iequals(word, "tarih")) return AttrType::Date;
    return std::nullopt;
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

AttrValue attr_decimal(std::int64_t scaled, std::uint8_t scale)
{
    AttrValue v;
    v.type    = AttrType::Decimal;
    v.present = true;
    v.number  = scaled;
    v.scale   = scale > kMaxScale ? kMaxScale : scale;
    return v;
}

AttrValue attr_date(std::int64_t days)
{
    AttrValue v;
    v.type    = AttrType::Date;
    v.present = true;
    v.number  = days;
    return v;
}

std::string date_to_text(std::int64_t days)
{
    // `std::chrono`, not a hand-rolled civil calendar. The conversion is famously
    // easy to get wrong at the March boundary and in the years before 1970, the
    // standard library has carried a correct, constexpr, allocation-free version
    // since C++20, and Article 2.7 says to use it (CLAUDE.md 5.16).
    const std::chrono::year_month_day date{
        std::chrono::sys_days{std::chrono::days{static_cast<int>(days)}}};
    if (!date.ok()) return {};

    // Built digit by digit rather than with a formatter, because `std::format`
    // and the streams both consult a locale and this string is compared byte for
    // byte in golden fixtures (R9).
    const auto pad = [](int value, std::size_t width) {
        std::string out = std::to_string(value);
        if (out.size() < width) out.insert(0, width - out.size(), '0');
        return out;
    };
    return pad(static_cast<int>(date.year()), 4) + "-" +
           pad(static_cast<int>(static_cast<unsigned>(date.month())), 2) + "-" +
           pad(static_cast<int>(static_cast<unsigned>(date.day())), 2);
}

std::optional<std::int64_t> date_from_text(std::string_view text)
{
    // `YYYY-AA-GG`, and that shape exactly. A parser that also took `12/03/2026`
    // would have to decide whether that is March or December, and the answer
    // differs by country — which is precisely the ambiguity ISO 8601 exists to
    // end. The panel may SHOW a date any way it likes; this is what it stores.
    int part[3]{};
    std::size_t at    = 0;
    std::size_t field = 0;
    for (; field < 3; ++field) {
        const std::size_t start = at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            part[field] = part[field] * 10 + (text[at] - '0');
            ++at;
        }
        if (at == start) return std::nullopt;
        if (field < 2) {
            if (at >= text.size() || (text[at] != '-' && text[at] != '.')) return std::nullopt;
            ++at;
        }
    }
    if (at != text.size()) return std::nullopt;

    const std::chrono::year_month_day date{std::chrono::year{part[0]},
                                           std::chrono::month{static_cast<unsigned>(part[1])},
                                           std::chrono::day{static_cast<unsigned>(part[2])}};
    if (!date.ok()) return std::nullopt;
    return std::chrono::sys_days{date}.time_since_epoch().count();
}

std::string decimal_to_text(std::int64_t scaled, std::uint8_t scale, DecimalMark mark)
{
    const bool negative          = scaled < 0;
    const std::int64_t magnitude = negative ? -scaled : scaled;

    std::int64_t divisor = 1;
    for (std::uint8_t i = 0; i < scale; ++i)
        divisor *= 10;

    std::string out = std::to_string(magnitude / divisor);
    if (scale > 0) {
        // EVERY DECLARED DIGIT, trailing zeros included. A `Length` trims them
        // because a metre reading is a measurement; a TAKS does not, because
        // `0.40` and `0.4` are the same number and only one of them is what the
        // plan note says.
        std::string digits = std::to_string(magnitude % divisor);
        digits.insert(0, static_cast<std::size_t>(scale) - digits.size(), '0');
        out += (mark == DecimalMark::Comma ? "," : ".") + digits;
    }
    return negative ? "-" + out : out;
}

std::optional<std::int64_t> decimal_from_text(std::string_view text, std::uint8_t scale)
{
    // BOTH MARKS ACCEPTED. A user typing `0,40` into a Turkish panel and a script
    // writing `0.40` mean the same number, and which one this build prefers on
    // output is not something either of them should have to know.
    std::size_t at = 0;
    bool negative  = false;
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
        negative = text[at] == '-';
        ++at;
    }

    std::int64_t whole = 0;
    std::size_t digits = 0;
    for (; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at, ++digits)
        whole = whole * 10 + (text[at] - '0');

    std::int64_t fraction = 0;
    std::size_t taken     = 0;
    if (at < text.size() && (text[at] == ',' || text[at] == '.')) {
        ++at;
        for (; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at) {
            // PAST THE DECLARED PRECISION IS AN ERROR, not a rounding. A column
            // declared with two digits that quietly turned `0.405` into `0.40`
            // would be a document saying something the user did not.
            if (taken == scale) return std::nullopt;
            fraction = fraction * 10 + (text[at] - '0');
            ++taken;
        }
    }
    if (digits == 0 && taken == 0) return std::nullopt;
    if (at != text.size()) return std::nullopt;

    for (std::size_t i = taken; i < scale; ++i)
        fraction *= 10;

    std::int64_t divisor = 1;
    for (std::uint8_t i = 0; i < scale; ++i)
        divisor *= 10;

    const std::int64_t scaled = (whole * divisor) + fraction;
    return negative ? -scaled : scaled;
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
    v.scale   = spec_.scale;
    v.present = present(row);
    if (!v.present) return v;

    if (is_text_shaped(spec_.type))
        v.text = pool_[codes_[row]];
    else
        v.number = numbers_[row];
    return v;
}

std::string attr_display(const AttrValue& value, DecimalMark mark)
{
    if (!value.present) return {};

    switch (value.type) {
    case AttrType::Text:
    case AttrType::CodeRef: return value.text;
    case AttrType::Bool: return value.number != 0 ? "evet" : "hayır";
    case AttrType::Length: {
        // Millimetres to metres with three decimals, built from integers so no
        // locale can put a comma where a golden fixture expects a point (R9).
        const bool negative          = value.number < 0;
        const std::int64_t magnitude = negative ? -value.number : value.number;

        std::string out             = std::to_string(magnitude / 1000);
        const std::int64_t fraction = magnitude % 1000;
        if (fraction != 0) {
            std::string digits = std::to_string(fraction);
            digits.insert(0, static_cast<std::size_t>(3 - digits.size()), '0');
            while (!digits.empty() && digits.back() == '0')
                digits.pop_back();
            out += (mark == DecimalMark::Comma ? "," : ".") + digits;
        }
        return negative ? "-" + out : out;
    }
    case AttrType::Int64: return std::to_string(value.number);
    case AttrType::Decimal: return decimal_to_text(value.number, value.scale, mark);
    case AttrType::Date: return date_to_text(value.number);
    }
    return {};
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
        if (is_text_shaped(spec_.type)) {
            // The LENGTH first, then the bytes. fnv1a mixes no length and no
            // terminator, so folding adjacent rows' bytes straight into the same
            // chain made {"Bostan", "lı"} and {"Bostanlı", ""} — two different
            // mahalle assignments over two parcels — produce one fingerprint. A
            // content hash that cannot tell those apart cannot tell a save that
            // shifted a column boundary from a save that changed nothing.
            const std::string& text = pool_[codes_[row]];
            h                       = fnv1a_int(static_cast<std::int64_t>(text.size()), h);
            h                       = fnv1a(text, h);
        } else {
            h = fnv1a_int(numbers_[row], h);
        }
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

Status AttrColumn::amend(const AttrSpec& next)
{
    if (next.id != spec_.id)
        return err(ErrorCode::InvalidArgument,
                   "Bir sütunun kimliği değiştirilemez: '" + spec_.id + "' -> '" + next.id +
                       "'. Kimlik, sütuna atıfta bulunan her sembolün ve her kuralın "
                       "adlandırdığı şeydir.");
    if (next.type != spec_.type)
        return err(ErrorCode::InvalidArgument,
                   "'" + spec_.id + "' sütununun türü değiştirilemez (" +
                       attr_type_name(spec_.type) + " -> " + attr_type_name(next.type) +
                       "). Saklanan sayıların ANLAMI odur; başka bir tür istiyorsanız sütunu "
                       "silip yeniden tanımlayın.");
    if (next.type == AttrType::CodeRef && next.catalog.empty())
        return err(ErrorCode::InvalidArgument,
                   "'" + spec_.id +
                       "' bir katalog kodu sütunu ama hangi kataloğu kullandığı yazılmamış.");

    if (next.type == AttrType::Decimal && next.scale != spec_.scale) {
        if (next.scale > kMaxScale)
            return err(ErrorCode::InvalidArgument,
                       "En çok " + std::to_string(kMaxScale) + " ondalık basamak olabilir.");

        // GAINING DIGITS IS EXACT, LOSING THEM IS REFUSED. 0.40 at two digits is
        // 40 and at three is 400 — the same number, so the cells are rescaled and
        // nothing the user typed changes under them. Going the other way would
        // throw away a digit somebody entered on purpose, and a schema edit is
        // not the place to discover that a value has quietly moved.
        if (next.scale < spec_.scale)
            return err(ErrorCode::InvalidArgument,
                       "'" + spec_.id + "' sütununun basamak sayısı azaltılamaz (" +
                           std::to_string(spec_.scale) + " -> " + std::to_string(next.scale) +
                           "): girilmiş değerlerin son basamağı atılırdı.");

        std::int64_t factor = 1;
        for (std::uint8_t i = spec_.scale; i < next.scale; ++i)
            factor *= 10;
        for (std::size_t row = 0; row < rows_; ++row)
            if (present(row)) numbers_[row] *= factor;
    }

    spec_ = next;
    return ok();
}

bool AttrTable::remove(AttrId col)
{
    if (col >= columns_.size()) return false;
    columns_.erase(columns_.begin() + static_cast<std::ptrdiff_t>(col));
    return true;
}

Status AttrTable::amend(AttrId col, const AttrSpec& next)
{
    if (col >= columns_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen öznitelik sütunu: " + std::to_string(col));
    return columns_[col].amend(next);
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
    // A document with no columns folds to the seed UNCHANGED, and that is a
    // decision rather than an optimisation. A drawing that carries no attributes
    // is the same drawing it was before attributes existed, so every .pcad file
    // and every golden fixture written before this table was attached must keep
    // its fingerprint. Mixing a row count into an empty table would have changed
    // the hash of every drawing in the world to record the absence of something.
    //
    // Row count IS mixed once a column exists, because then "three rows, all
    // absent" and "two rows, all absent" are different documents.
    if (columns_.empty()) return seed;

    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_), seed);
    for (const auto& c : columns_)
        h = c.fold(h);
    return h;
}

} // namespace kentos::core
