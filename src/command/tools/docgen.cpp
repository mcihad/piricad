// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — generates the command reference from the command registry.
//
// CLAUDE.md 5.10 forbids a second, hand-maintained command list. The user manual
// still needs a complete reference table, so it is GENERATED from `Registry`
// here and written into /docs. Editing the output by hand is a defect; the gate
// scripts/ci-gate-docs.sh regenerates it and fails on any difference.
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/text.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

using namespace kentos::command;

std::string slug(const std::string& id)
{
    // "core.line" -> "cizgi" is not derivable, so the slug comes from the id's
    // last segment: "core.line" -> "line". Page names follow it exactly.
    const auto dot = id.rfind('.');
    return dot == std::string::npos ? id : id.substr(dot + 1);
}

std::string arity_text(const Param& p)
{
    if (p.arity.max == 0xFFFFFFFFu) return "en az " + std::to_string(p.arity.min);
    if (p.arity.min == 0 && p.arity.max == 1) return "isteğe bağlı";
    if (p.arity.min == p.arity.max) return std::to_string(p.arity.min);
    return std::to_string(p.arity.min) + "–" + std::to_string(p.arity.max);
}

std::string flags_text(Flags f)
{
    std::string out;
    const auto add = [&out](const char* s) {
        if (!out.empty()) out += ", ";
        out += s;
    };
    if (has_flag(f, Flags::Interactive)) add("etkileşimli");
    if (has_flag(f, Flags::Scriptable)) add("betiklenebilir");
    if (has_flag(f, Flags::AiAccessible)) add("AI erişimli");
    if (has_flag(f, Flags::Transparent)) add("şeffaf");
    if (has_flag(f, Flags::ReadOnly)) add("salt okunur");
    return out.empty() ? "—" : out;
}

std::string undo_text(UndoPolicy u)
{
    switch (u) {
    case UndoPolicy::SingleTransaction: return "tek işlem";
    case UndoPolicy::None: return "geri alınmaz";
    case UndoPolicy::Custom: return "komuta özel";
    }
    return "?";
}

std::string build(const Registry& reg)
{
    std::string out;

    out += "<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->\n";
    out += "<!-- Kaynak: kentos::command::Registry.  Yeniden üret: make reference -->\n";
    out += "<!-- Bir komutun burada görünmesi için tek yapılması gereken onu kaydetmektir; -->\n";
    out += "<!-- projede elle tutulan ikinci bir komut listesi yoktur (CLAUDE.md 5.10). -->\n\n";

    out += "# Komut Referansı\n\n";
    out += "Bu tablo komut kaydından üretilir. Her komutun ayrıntılı kullanım sayfası\n";
    out += "`docs/komutlar/` altındadır ve tablodan bağlanır.\n\n";

    out += "| Komut | Adlar | Kategori | Geri alma | Özellikler | Açıklama |\n";
    out += "|---|---|---|---|---|---|\n";

    for (const auto& spec : reg.all()) {
        out += "| [`" + spec.id + "`](" + slug(spec.id) + ".md) | ";
        for (std::size_t i = 0; i < spec.names.size(); ++i) {
            if (i) out += ", ";
            out += "`" + spec.names[i] + "`";
        }
        out += " | ";
        out += category_name(spec.category);
        out += " | " + undo_text(spec.undo);
        out += " | " + flags_text(spec.flags);
        out += " | " + spec.summary + " |\n";
    }

    out += "\n## Parametreler\n\n";

    for (const auto& spec : reg.all()) {
        out += "### `" + spec.id + "` — " + spec.names.front() + "\n\n";
        out += spec.summary + "\n\n";

        if (spec.params.empty()) {
            out += "Parametre almaz.\n\n";
        } else {
            out += "| Parametre | Tip | Adet | Açıklama |\n|---|---|---|---|\n";
            for (const auto& p : spec.params) {
                out += "| `" + p.name + "` | " + param_kind_name(p.kind) + " | " + arity_text(p) +
                       " | " + (p.help.empty() ? "—" : p.help) + " |\n";
            }
            out += "\n";
        }
        out += "Ayrıntılı kullanım: [" + spec.names.front() + "](" + slug(spec.id) + ".md)\n\n";
    }

    out += "## AI araç kataloğu\n\n";
    out += "AI'ın görebildiği komutlar `Flags::AiAccessible` bayrağından üretilir.\n";
    out += "Elle tutulan ikinci bir araç şeması yoktur (kentoscad.md §2.3, §5.1).\n\n";
    out += "```json\n";
    out += reg.ai_tool_schema().dump_pretty(2);
    out += "\n```\n";

    return out;
}

} // namespace

/// The page slug of a kind: its Turkish name, folded to ASCII and lowered.
/// `ÇOKLUÇİZGİ` becomes `coklucizgi`, which is the file under docs/nesneler/.
std::string kind_slug(const kentos::core::KindSpec& spec)
{
    std::string folded = kentos::core::turkish_fold_key(spec.names[0]);
    for (char& c : folded)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return folded;
}

/// The kinds table (model.md R25): generated from `builtin_kinds()` the way the
/// command table is generated from the registry, so there is no hand-kept list
/// of entity kinds anywhere in /docs.
std::string build_kinds()
{
    std::string out;
    out += "<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->\n";
    out += "<!-- Kaynak: kentos::core::builtin_kinds().  Yeniden üret: make reference -->\n";
    out += "<!-- Bir nesne türünün burada görünmesi için tek yapılması gereken onu kaydetmektir; "
           "-->\n";
    out += "<!-- projede elle tutulan ikinci bir tür listesi yoktur (model.md R25). -->\n\n";

    out += "# Nesne Türleri Referansı\n\n";
    out += "Bu tablo çekirdeğin tür kaydından üretilir. Her türün ayrıntılı sayfası\n";
    out += "`docs/nesneler/` altındadır ve tablodan bağlanır. Kimlik dosyaya yazılan sayıdır ve\n";
    out += "bir kez verildikten sonra asla başka anlama gelmez (model.md R26).\n\n";

    out += "| Tür | Kimlik | Adlar | Açıklama |\n";
    out += "|---|---|---|---|\n";
    for (const kentos::core::KindSpec& spec : kentos::core::builtin_kinds().all()) {
        out += "| [`" + std::string(spec.stable_id) + "`](" + kind_slug(spec) + ".md) | " +
               std::to_string(spec.id) + " | ";
        bool first = true;
        for (const char* n : spec.names) {
            if (n == nullptr || *n == '\0') continue;
            if (!first) out += ", ";
            first = false;
            out += "`" + std::string(n) + "`";
        }
        out += " | " + std::string(spec.summary_tr) + " |\n";
    }
    return out;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        (void)std::fprintf(stderr,
                           "kullanım: kentos_docgen <komut-referans.md> [<nesne-referans.md>]\n");
        return 2;
    }

    Registry reg;
    register_builtin_commands(reg);

    std::ofstream out(argv[1], std::ios::out | std::ios::binary);
    if (!out) {
        (void)std::fprintf(stderr, "docgen: '%s' yazılamadı\n", argv[1]);
        return 1;
    }

    out << build(reg);
    (void)std::fprintf(stdout, "docgen: %zu komut -> %s\n", reg.size(), argv[1]);

    if (argc >= 3) {
        std::ofstream kinds(argv[2], std::ios::out | std::ios::binary);
        if (!kinds) {
            (void)std::fprintf(stderr, "docgen: '%s' yazılamadı\n", argv[2]);
            return 1;
        }
        kinds << build_kinds();
        (void)std::fprintf(stdout, "docgen: %zu nesne türü -> %s\n",
                           kentos::core::builtin_kinds().size(), argv[2]);
    }
    return 0;
}
