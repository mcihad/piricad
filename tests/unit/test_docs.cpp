// SPDX-License-Identifier: GPL-3.0-or-later
//
// .claude/docs.md R10: "Her örnek OLDUĞU GİBİ çalışmalı."
//
// A manual whose examples do not run is worse than no manual, so this test reads
// the shipped Markdown pages, extracts the fenced blocks, and executes them
// through the same command bus a user would. There is no second copy of the
// examples anywhere — the pages are the source.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

namespace fs = std::filesystem;

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig()
    {
        register_builtin_commands(reg);

        // The manual and the golden scenarios may name ANY command the program
        // ships, and a domain module owns some of them: `/src/command` may not
        // depend on `/src/domain`, so its builtin list cannot mention OTURT
        // (Article 3.2). A harness that registers only the builtins reports a
        // real command as unknown.
        domain::geodesy::register_geodesy_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        domain::surface::register_surface_commands(reg);
        bus.on_echo = [](std::string_view) {}; // transcript output is not the subject
    }
};

std::vector<fs::path> markdown_pages()
{
    std::vector<fs::path> pages;
    const fs::path root{KENTOS_DOCS_DIR};
    if (!fs::exists(root)) return pages;

    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".md")
            pages.push_back(entry.path());

    std::sort(pages.begin(), pages.end());
    return pages;
}

std::string read_file(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

struct Block
{
    std::string language;
    std::string body;
    std::size_t line{0};
};

std::vector<Block> fenced_blocks(const std::string& text)
{
    std::vector<Block> blocks;
    std::istringstream in(text);
    std::string line;
    std::size_t lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;
        if (line.rfind("```", 0) != 0) continue;

        Block block;
        block.language = line.substr(3);
        block.line     = lineno + 1;

        while (std::getline(in, line)) {
            ++lineno;
            if (line.rfind("```", 0) == 0) break;
            block.body += line;
            block.body += '\n';
        }
        blocks.push_back(std::move(block));
    }
    return blocks;
}

/// A syntax skeleton is not an example to run. A skeleton is recognised by a
/// COMPLETE `<...>` placeholder — the closing bracket matters: `@100<abc` has no
/// closing bracket, so it is a broken polar coordinate and must be reported, not
/// quietly skipped.
bool is_illustration(const std::string& line)
{
    if (line.find("←") != std::string::npos) return true;
    if (line.find("...") != std::string::npos) return true;
    if (line.find("…") != std::string::npos) return true;

    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        if (line[i] != '<') continue;

        const auto close = line.find('>', i + 1);
        if (close == std::string::npos) continue;

        const unsigned char first = static_cast<unsigned char>(line[i + 1]);
        const bool alphabetic =
            first >= 0x80 || (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');
        if (alphabetic && close > i + 1) return true;
    }
    return false;
}

/// BETİK examples name files outside the repository, and a transcript line such
/// as "Betik tamamlandı: …" resolves to the same command, so the whole command is
/// left to the script pages' own tests.
bool is_out_of_scope(const CommandSpec& spec)
{
    if (spec.id == "core.script") return true;

    // File commands need a FileService, and this rig deliberately has none: a doc
    // check must not write to the working directory, and a manual page should be
    // able to say `STİLAKTAR KONUT konut.qml` — which is what a user types —
    // rather than dressing the path in angle brackets to hide from the checker.
    //
    // Their examples are not unchecked: /tests/unit/test_io.cpp runs the same
    // commands against a temp directory with the engine attached, which is where
    // a file command can actually be verified.
    return spec.category == Category::File;
}

/// A bare command name is a syntax skeleton when the command needs an argument —
/// `ÇİZGİ` alone cannot run non-interactively, while `YARDIM` alone can. The
/// registry decides, so this stays true as commands change.
bool needs_arguments(const CommandSpec& spec)
{
    for (const auto& p : spec.params)
        if (p.arity.min > 0) return true;
    return false;
}

std::string first_word(const std::string& line)
{
    const auto end = line.find_first_of(" \t");
    return end == std::string::npos ? line : line.substr(0, end);
}

std::string trim(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

TEST_CASE("DOKÜMAN: kılavuzda yazan her komut satırı çalışır")
{
    Rig rig;
    std::size_t checked = 0;

    for (const auto& page : markdown_pages()) {
        for (const auto& block : fenced_blocks(read_file(page))) {
            if (!block.language.empty()) continue; // ``` with no language = command lines

            std::istringstream lines(block.body);
            std::string raw;
            std::size_t offset = 0;

            while (std::getline(lines, raw)) {
                ++offset;
                const std::string line = trim(raw);
                if (line.empty() || is_illustration(line)) continue;

                // Only lines that start with a registered command name are input.
                const CommandSpec* spec = rig.reg.resolve(first_word(line));
                if (!spec || is_out_of_scope(*spec)) continue;
                if (line == first_word(line) && needs_arguments(*spec)) continue;

                auto result = rig.bus.execute_line(line, Origin::Test);
                if (!result) {
                    FAIL_WITH(line.c_str(), page.filename().string() + ":" +
                                                std::to_string(block.line + offset - 1) + " — " +
                                                result.error().message);
                }
                ++checked;
            }
        }
    }

    // A silent zero would mean the extractor broke, not that the manual is clean.
    CHECK(checked >= 20);
}

TEST_CASE("DOKÜMAN: kılavuzdaki her JSON betiği geçerli ve çalışır")
{
    std::size_t parsed   = 0;
    std::size_t executed = 0;

    for (const auto& page : markdown_pages()) {
        for (const auto& block : fenced_blocks(read_file(page))) {
            if (block.language != "json") continue;

            auto json = core::Json::parse(block.body);
            if (!json) {
                FAIL_WITH("JSON parse", page.filename().string() + ":" +
                                            std::to_string(block.line) + " — " +
                                            json.error().message);
                continue;
            }
            ++parsed;

            // A block is a script when it is an array of commands or an object
            // carrying one; anything else is a journal line or a schema excerpt.
            const bool is_script = (json.value().is_object() && (json.value().find("komutlar") ||
                                                                 json.value().find("commands"))) ||
                                   (json.value().is_array() && !json.value().as_array().empty() &&
                                    json.value().as_array().front().find("cmd"));
            if (!is_script) continue;

            // Same reason as is_out_of_scope above: this rig has no FileService,
            // so a script whose commands need one is checked by /tests/unit/
            // test_io.cpp against a temp directory instead. The block is still
            // PARSED here, so a malformed example is still caught.
            Rig probe;
            bool needs_files = false;
            for (const core::Json& step : json.value().is_array() ? json.value().as_array()
                                          : json.value().find("komutlar")
                                              ? json.value().find("komutlar")->as_array()
                                              : json.value().find("commands")->as_array()) {
                const core::Json* cmd = step.find("cmd");
                if (cmd == nullptr || !cmd->is_string()) continue;
                if (const CommandSpec* spec = probe.reg.resolve(cmd->as_string());
                    spec != nullptr && spec->category == Category::File)
                    needs_files = true;
            }
            if (needs_files) continue;

            Rig rig;
            script::JsonRunner runner(rig.bus, script::Sandbox::Project);

            auto run = runner.run_text(block.body);
            if (!run) {
                FAIL_WITH("betik", page.filename().string() + ":" + std::to_string(block.line) +
                                       " — " + run.error().message);
            }
            ++executed;
        }
    }

    CHECK(parsed >= 8);
    CHECK(executed >= 5);
}

TEST_CASE("DOKÜMAN: örnek betik dosyaları çalışır")
{
    // Every script the manual points at must run, or the link is a promise the
    // product does not keep.
    const fs::path journal_dir{KENTOS_JOURNAL_DIR};
    if (!fs::exists(journal_dir)) return;

    std::size_t ran = 0;
    for (const auto& entry : fs::directory_iterator(journal_dir)) {
        if (entry.path().extension() != ".json") continue;

        Rig rig;
        script::JsonRunner runner(rig.bus, script::Sandbox::Project);

        auto run = runner.run_file(entry.path().string());
        if (!run) {
            FAIL_WITH(entry.path().filename().string().c_str(), run.error().message);
        } else {
            CHECK(rig.doc.live_entity_count() > 0);
            CHECK(rig.undo.undo_depth() == 1); // bir betik = tek geri alma adımı
        }
        ++ran;
    }
    CHECK(ran >= 1);
}
