// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — generates the command reference from the command registry.
//
// CLAUDE.md 5.10 forbids a second, hand-maintained command list. The user manual
// still needs a complete reference table, so it is GENERATED from `Registry`
// here and written into /docs. Editing the output by hand is a defect; the gate
// scripts/ci-gate-docs.sh regenerates it and fails on any difference.
#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/llmstxt.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/python_doc.hpp"

#include <cstdio>
#include <exception>
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
    if (has_flag(f, Flags::LongRunning)) add("uzun iş");
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

    out += "| Komut | Adı | Adlar | Kategori | Geri alma | Özellikler | Açıklama |\n";
    out += "|---|---|---|---|---|---|---|\n";

    for (const auto& spec : reg.all()) {
        out += "| [`" + spec.id + "`](" + slug(spec.id) + ".md) | ";
        // The human label beside the id. A menu, a panel and a page all name the
        // command this way; the reference is the one place a reader can see the
        // label and the word to type side by side (`CommandSpec::title`).
        out += (spec.title.empty() ? "—" : spec.title) + " | ";
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
        out += "### `" + spec.id + "` — " + spec.names.front() +
               (spec.title.empty() ? "" : " (" + spec.title + ")") + "\n\n";
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

    // THE CATALOGUE AS AN AGENT RECEIVES IT, from `ai::build_catalog` and from
    // nowhere else. It used to be a second projection living on `Registry`; two
    // projections of one registry are two answers to one question
    // (.claude/ai.md P7), so that one is gone and this is the survivor.
    out += "## AI araç kataloğu\n\n";
    out += "AI'ın görebildiği komutlar `Flags::AiAccessible` bayrağından üretilir.\n";
    out += "Elle tutulan ikinci bir araç şeması yoktur (kentoscad.md §2.3, §5.1).\n\n";
    out += "```json\n";
    out += kentos::ai::build_catalog(reg).to_tools_list().dump_pretty(2);
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

/// The generator proper. `main` below is the boundary that turns a thrown
/// exception into an exit code, because a generator that aborts mid-write leaves
/// a half-written reference that `ci-gate-docs.sh` would then diff against.
int run(int argc, char** argv)
{
    if (argc < 2) {
        (void)std::fprintf(stderr,
                           "kullanım: kentos_docgen <komut-referans.md> [<nesne-referans.md>] "
                           "[<llms.txt>] [<llms-full.txt>]\n");
        return 2;
    }

    // EVERY REGISTRY THE PROGRAM HAS, and the domain ones were missing: the
    // generator saw 73 of the 82 commands, so the nine commands that the geodesy,
    // cadastre and surface modules register — parcel division and merging,
    // topology, fitting, reprojection, stake-out, contours and earthwork — were
    // absent from the reference a user reads and from the catalogue an agent
    // reads. A generator that walks an incomplete registry documents an
    // incomplete program. `docgen` is an executable at the top of the dependency
    // graph, so it may link the domain modules the command library must not
    // (Article 3.2).
    Registry reg;
    register_builtin_commands(reg);
    kentos::processing::register_processing_commands(reg);
    kentos::domain::geodesy::register_geodesy_commands(reg);
    kentos::domain::cadastre::register_cadastre_commands(reg);
    kentos::domain::surface::register_surface_commands(reg);
    // AND THE AI LAYER'S OWN COMMANDS: the five read tools plus ÖNERİ and
    // MCPSUNUCU. They are commands like any others (Article 1.2), so they belong
    // in the reference a user reads and in the catalogue an agent reads — and
    // `llms.txt` would be describing a surface that lacked its own read tools
    // without them.
    kentos::ai::register_ai_commands(reg);

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

    // THE TWO DOCUMENTS A MODEL READS. Generated from the same registry as the
    // reference and in the same run, because a tool surface and its description
    // that can be regenerated separately are a surface and a description that
    // will disagree (CLAUDE.md 5.20, 6.14).
    if (argc >= 4) {
        std::ofstream llms(argv[3], std::ios::out | std::ios::binary);
        if (!llms) {
            (void)std::fprintf(stderr, "docgen: '%s' yazılamadı\n", argv[3]);
            return 1;
        }
        llms << kentos::ai::llms_txt(reg);
        (void)std::fprintf(stdout, "docgen: llms.txt -> %s\n", argv[3]);
    }
    if (argc >= 5) {
        std::ofstream full(argv[4], std::ios::out | std::ios::binary);
        if (!full) {
            (void)std::fprintf(stderr, "docgen: '%s' yazılamadı\n", argv[4]);
            return 1;
        }
        full << kentos::ai::llms_full_txt(reg);
        (void)std::fprintf(stdout, "docgen: llms-full.txt -> %s\n", argv[4]);
    }

    // THE PYTHON SURFACE, AS A REFERENCE AND AS A TYPE STUB. Same registry, same
    // run, for the reason the two documents above give: a surface and its
    // description that can be regenerated separately are a surface and a
    // description that will disagree (CLAUDE.md 6.14, and this is Article 6.15's
    // half of it).
    //
    // Written in EVERY configuration, including one built without Python.
    // `kentos_script` compiles `python_doc.cpp` unconditionally, so the freshness
    // check does not depend on a build option — a gate that passes on one machine
    // and fails on another has not checked anything.
    if (argc >= 6) {
        std::ofstream py(argv[5], std::ios::out | std::ios::binary);
        if (!py) {
            (void)std::fprintf(stderr, "docgen: '%s' yazılamadı\n", argv[5]);
            return 1;
        }
        py << kentos::script::python_reference(reg);
        (void)std::fprintf(stdout, "docgen: Python referansı -> %s\n", argv[5]);
    }
    if (argc >= 7) {
        std::ofstream stub(argv[6], std::ios::out | std::ios::binary);
        if (!stub) {
            (void)std::fprintf(stderr, "docgen: '%s' yazılamadı\n", argv[6]);
            return 1;
        }
        stub << kentos::script::python_stub(reg);
        (void)std::fprintf(stdout, "docgen: Python tip taslağı -> %s\n", argv[6]);
    }
    return 0;
}

int main(int argc, char** argv)
{
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        (void)std::fprintf(stderr, "docgen: %s\n", e.what());
        return 1;
    } catch (...) {
        (void)std::fprintf(stderr, "docgen: bilinmeyen hata\n");
        return 1;
    }
}
