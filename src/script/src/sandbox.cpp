// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/script/sandbox.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

#include <array>

namespace kentos::script {
namespace {

/// The identity hash as fixed-width hex.
///
/// Written out here rather than with `std::format("{:016x}")`: the journal is
/// compared byte for byte (CLAUDE.md 6.4) and a locale-aware formatter is one
/// more thing between a number and its bytes.
std::string hex64(std::uint64_t v)
{
    static constexpr std::array<char, 16> kDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = kDigits[v & 0xFu];
        v >>= 4;
    }
    return out;
}

} // namespace

std::string_view sandbox_name(Sandbox level) noexcept
{
    switch (level) {
    case Sandbox::Safe: return "güvenli";
    case Sandbox::Project: return "proje";
    case Sandbox::Full: return "tam";
    }
    return "güvenli";
}

core::Result<Sandbox> sandbox_from_name(std::string_view name)
{
    // The SHARED folding, never `std::tolower` (CLAUDE.md 5.6): `guvenli` and
    // `GÜVENLİ` are the same word and the dotted/dotless i is what an ASCII
    // classifier gets wrong on exactly this alphabet.
    const std::string key = core::turkish_fold_key(name);

    if (key == core::turkish_fold_key("güvenli") || key == core::turkish_fold_key("safe"))
        return Sandbox::Safe;
    if (key == core::turkish_fold_key("proje") || key == core::turkish_fold_key("project"))
        return Sandbox::Project;
    if (key == core::turkish_fold_key("tam") || key == core::turkish_fold_key("full"))
        return Sandbox::Full;

    return core::err(core::ErrorCode::InvalidArgument, "Bilinmeyen kum havuzu seviyesi: '" +
                                                           std::string(name) +
                                                           "'. Beklenen: güvenli, proje, tam.");
}

std::uint64_t script_identity(std::string_view text) noexcept
{
    return core::fnv1a(text);
}

void journal_run(command::Bus& bus, std::string_view host, std::string_view label, Sandbox level,
                 std::uint64_t identity, bool consented)
{
    // Field order is a decision, not an accident: `core::JsonObject` keeps
    // insertion order and golden fixtures record the exact bytes.
    core::Json record;
    record.set("ne", core::Json::string("betik"));
    record.set("konak", core::Json::string(std::string(host)));
    record.set("ad", core::Json::string(std::string(label)));
    record.set("kum_havuzu", core::Json::string(std::string(sandbox_name(level))));
    record.set("kimlik", core::Json::string(hex64(identity)));

    // Only `tam` has anything to consent TO, and R12 wants the decision recorded
    // beside the identity it was given for. Writing `onay:false` for a `güvenli`
    // run would read as a refusal rather than as a question nobody asked.
    if (level == Sandbox::Full) record.set("onay", core::Json::boolean(consented));

    bus.journal().append_meta(record);
}

} // namespace kentos::script
