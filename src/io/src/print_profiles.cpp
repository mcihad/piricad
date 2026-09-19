// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/print_profiles.hpp"

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <sstream>

namespace kentos::io {
namespace {

constexpr std::int64_t kMinDpi = 72;
constexpr std::int64_t kMaxDpi = 4800;

std::string canonical_paper(std::string_view paper)
{
    return core::canonical_paper(paper);
}

} // namespace

// THE TABLE MOVED TO `core` and these two forward to it. They stay because the
// print profiles, the settings page and the print command all say `io::` today
// and a rename across four files buys nothing; what mattered was that there is
// now ONE table rather than a second copy in the command layer (CLAUDE.md 5.10).
std::optional<std::pair<std::int64_t, std::int64_t>> paper_size_mm(std::string_view paper)
{
    return core::paper_size_mm(paper);
}

std::span<const char* const> paper_names()
{
    return core::paper_names();
}

std::string describe_print_profile(const PrintProfile& p)
{
    // THE SHEET'S OWN MEASUREMENTS, after the turn: a landscape A3 is 420×297,
    // and printing the stored portrait pair beside the word "yatay" told the
    // reader the sheet was 297 wide when it is 420.
    return p.name + " — " + p.paper + " " + std::to_string(p.sheet_width_mm()) + "×" +
           std::to_string(p.sheet_height_mm()) + " mm, " + (p.landscape ? "yatay" : "dikey") +
           ", " + std::to_string(p.dpi) + " dpi, kenar " + std::to_string(p.margin_mm) + " mm";
}

// ------------------------------------------------------------ PrintProfiles ----

PrintProfiles PrintProfiles::builtin()
{
    PrintProfiles out;
    const auto add = [&out](const char* name, const char* paper, bool landscape) {
        PrintProfile p;
        p.name          = name;
        p.paper         = paper;
        p.landscape     = landscape;
        const auto size = paper_size_mm(paper);
        p.width_mm      = size->first;
        p.height_mm     = size->second;
        out.profiles_.push_back(std::move(p));
    };
    add("A4 Dikey", "A4", false);
    add("A4 Yatay", "A4", true);
    add("A3 Yatay", "A3", true);
    add("A2 Yatay", "A2", true);
    add("A1 Yatay", "A1", true);
    add("A0 Yatay", "A0", true);
    out.default_ = "A4 Dikey";
    return out;
}

const PrintProfile* PrintProfiles::find(std::string_view name) const
{
    for (const PrintProfile& p : profiles_)
        if (core::turkish_key_equals(p.name, name)) return &p;
    return nullptr;
}

const PrintProfile* PrintProfiles::fallback() const
{
    if (const PrintProfile* p = find(default_); p != nullptr) return p;
    return profiles_.empty() ? nullptr : &profiles_.front();
}

core::Status PrintProfiles::upsert(PrintProfile p)
{
    if (p.name.empty())
        return core::err(core::ErrorCode::InvalidArgument, "Profil adı boş olamaz.");
    p.paper = canonical_paper(p.paper);
    if (p.paper == "ozel") {
        if (p.width_mm <= 0 || p.height_mm <= 0)
            return core::err(core::ErrorCode::InvalidArgument,
                             "ozel kâğıt için genislik ve yukseklik milimetre olarak verilmeli "
                             "(sıfırdan büyük).");
    } else {
        const auto size = paper_size_mm(p.paper);
        if (!size) {
            std::string names;
            for (const char* n : core::paper_names())
                names += (names.empty() ? "" : ", ") + std::string(n);
            return core::err(core::ErrorCode::InvalidArgument,
                             "Tanınmayan kâğıt: '" + p.paper + "'. Kâğıtlar: " + names + ".");
        }
        p.width_mm  = size->first;
        p.height_mm = size->second;
    }
    if (p.dpi < kMinDpi || p.dpi > kMaxDpi)
        return core::err(core::ErrorCode::InvalidArgument,
                         "dpi " + std::to_string(kMinDpi) + " ile " + std::to_string(kMaxDpi) +
                             " arasında olmalı; verilen " + std::to_string(p.dpi) + ".");
    if (p.margin_mm < 0 || p.printable_width_mm() <= 0 || p.printable_height_mm() <= 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Kenar boşluğu kâğıtta yazdırılacak yer bırakmıyor: kenar " +
                             std::to_string(p.margin_mm) + " mm, kâğıt " +
                             std::to_string(p.width_mm) + "×" + std::to_string(p.height_mm) +
                             " mm.");

    for (PrintProfile& have : profiles_)
        if (core::turkish_key_equals(have.name, p.name)) {
            p.name = have.name; // keep the spelling the user first gave
            have   = std::move(p);
            return core::ok();
        }
    profiles_.push_back(std::move(p));
    if (default_.empty()) default_ = profiles_.back().name;
    return core::ok();
}

core::Status PrintProfiles::remove(std::string_view name)
{
    const auto at = std::find_if(profiles_.begin(), profiles_.end(), [&](const PrintProfile& p) {
        return core::turkish_key_equals(p.name, name);
    });
    if (at == profiles_.end())
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir yazdırma profili yok: '" + std::string(name) + "'.");
    if (profiles_.size() == 1)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Son profil silinemez; önce başka bir profil ekleyin.");
    const bool was_default = core::turkish_key_equals(at->name, default_);
    profiles_.erase(at);
    if (was_default) default_ = profiles_.front().name;
    return core::ok();
}

core::Status PrintProfiles::set_default(std::string_view name)
{
    const PrintProfile* p = find(name);
    if (p == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir yazdırma profili yok: '" + std::string(name) + "'.");
    default_ = p->name;
    return core::ok();
}

core::Result<PrintProfile> PrintProfiles::resolve(const command::PrintRequest& request) const
{
    const PrintProfile* base = request.profile.empty() ? fallback() : find(request.profile);
    if (base == nullptr) {
        if (request.profile.empty())
            return core::err(core::ErrorCode::NotFound,
                             "Hiç yazdırma profili yok; YAZDIRMAPROFİLİ ekle ile bir profil "
                             "tanımlayın.");
        return core::err(core::ErrorCode::NotFound, "Böyle bir yazdırma profili yok: '" +
                                                        request.profile + "'. Profiller:\n" +
                                                        listing());
    }
    PrintProfile p = *base;
    if (!request.paper.empty()) {
        p.paper = canonical_paper(request.paper);
        if (p.paper != "ozel") {
            const auto size = paper_size_mm(p.paper);
            if (!size)
                return core::err(core::ErrorCode::InvalidArgument,
                                 "Tanınmayan kâğıt: '" + request.paper + "'.");
            p.width_mm  = size->first;
            p.height_mm = size->second;
        }
    }
    if (request.width_mm > 0) p.width_mm = request.width_mm;
    if (request.height_mm > 0) p.height_mm = request.height_mm;
    if (request.landscape >= 0) p.landscape = request.landscape == 1;
    if (request.dpi > 0) p.dpi = request.dpi;
    if (request.margin_mm >= 0) p.margin_mm = request.margin_mm;

    // The same floor a stored profile has to clear, so an override cannot make
    // a sheet the store would have refused.
    PrintProfiles check;
    if (auto st = check.upsert(p); !st) return st.error();
    return check.profiles_.front();
}

std::string PrintProfiles::listing() const
{
    if (profiles_.empty()) return "Hiç yazdırma profili yok.";
    std::string out;
    for (const PrintProfile& p : profiles_) {
        out += core::turkish_key_equals(p.name, default_) ? "* " : "  ";
        out += describe_print_profile(p);
        out += '\n';
    }
    out += "(* varsayılan)";
    return out;
}

std::string PrintProfiles::to_json() const
{
    core::Json root = core::Json::object({});
    root.set("varsayilan", core::Json::string(default_));
    core::Json list = core::Json::array({});
    for (const PrintProfile& p : profiles_) {
        core::Json one = core::Json::object({});
        one.set("ad", core::Json::string(p.name));
        one.set("kagit", core::Json::string(p.paper));
        one.set("genislik", core::Json::integer(p.width_mm));
        one.set("yukseklik", core::Json::integer(p.height_mm));
        one.set("yon", core::Json::string(p.landscape ? "yatay" : "dikey"));
        one.set("dpi", core::Json::integer(p.dpi));
        one.set("kenar", core::Json::integer(p.margin_mm));
        list.push(std::move(one));
    }
    root.set("profiller", std::move(list));
    return root.dump_pretty(2);
}

core::Result<PrintProfiles> PrintProfiles::from_json(std::string_view text)
{
    auto parsed = core::Json::parse(text);
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "Yazdırma profilleri dosyası okunamadı: " + parsed.error().message);
    const core::Json& root = parsed.value();
    if (!root.is_object())
        return core::err(core::ErrorCode::ParseError,
                         "Yazdırma profilleri dosyası bir JSON nesnesi değil.");

    PrintProfiles out;
    if (const core::Json* list = root.find("profiller"); list != nullptr && list->is_array())
        for (const core::Json& one : list->as_array()) {
            PrintProfile p;
            if (const core::Json* v = one.find("ad")) p.name = v->as_string();
            if (const core::Json* v = one.find("kagit")) p.paper = v->as_string();
            if (const core::Json* v = one.find("genislik")) p.width_mm = v->as_int();
            if (const core::Json* v = one.find("yukseklik")) p.height_mm = v->as_int();
            if (const core::Json* v = one.find("yon"))
                p.landscape = core::turkish_key_equals(v->as_string(), "yatay");
            if (const core::Json* v = one.find("dpi")) p.dpi = v->as_int(300);
            if (const core::Json* v = one.find("kenar")) p.margin_mm = v->as_int(10);
            if (auto st = out.upsert(std::move(p)); !st)
                return core::err(core::ErrorCode::ParseError,
                                 "Yazdırma profilleri dosyasında geçersiz profil: " +
                                     st.error().message);
        }
    if (const core::Json* d = root.find("varsayilan"); d != nullptr && d->is_string())
        if (out.find(d->as_string()) != nullptr) out.default_ = out.find(d->as_string())->name;
    if (out.default_.empty() && !out.profiles_.empty()) out.default_ = out.profiles_.front().name;
    return out;
}

core::Result<PrintProfiles> PrintProfiles::load(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return builtin();
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.empty()) return builtin();
    return from_json(text);
}

core::Status PrintProfiles::save(const std::string& path) const
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return core::err(core::ErrorCode::IoFailure,
                         "Yazdırma profilleri yazılamadı: " + path + " açılamıyor.");
    out << to_json() << '\n';
    if (!out)
        return core::err(core::ErrorCode::IoFailure, "Yazdırma profilleri yazılamadı: " + path);
    return core::ok();
}

} // namespace kentos::io
