// SPDX-License-Identifier: GPL-3.0-or-later
// SEMBOL — the symbol shelf.
//
// The command that makes the MPYY gösterim set BROWSABLE rather than merely
// applicable. `STİL kod=` has always been able to apply a catalogue row; what was
// missing was the way to find out which row you want out of four hundred and
// seventy-six, and to see the tree the regulation itself prints them in.
//
// READ-ONLY over the document. It loads a package into the session's shelf, walks
// the tree and searches it; nothing here writes an entity, which is why it takes
// no transaction and leaves no undo record. Applying a symbol is `STİL`'s job and
// stays there — one road to the style column, not two (CLAUDE.md 1.1).
//
// The tree comes from the package. MPYY EK-1 files its rows by annex and then by
// a section path such as SINIRLAR > İDARİ SINIRLAR, and that is exactly the tree
// a planner already navigates on paper. Nothing here invents a taxonomy (5.13).
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/json.hpp"
#include "piricad/core/style_library.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// Reads one catalogue package from disk.
///
/// Core may not do this (core.md P9) and neither may the shelf, so the command
/// layer does it — the same place `STİL` reads its package from, and for the same
/// reason.
core::Result<core::StyleCatalog> load_package(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return core::err(core::ErrorCode::NotFound, "Sembol paketi açılamadı: '" + path +
                                                        "'. Yol çalışma dizinine göre çözülür.");

    std::ostringstream buffer;
    buffer << in.rdbuf();

    auto parsed = core::Json::parse(buffer.str());
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "Sembol paketi okunamadı: '" + path + "': " + parsed.error().message);

    return core::StyleCatalog::from_json(parsed.value());
}

/// The separator between levels of a group path.
///
/// `>` is not a choice this file made: it is the notation MPYY itself prints in
/// its own source references — `SINIRLAR > İDARİ SINIRLAR`. Checked against the
/// package: no annex name, section name or gösterim label in it contains the
/// character, so no name can be split by accident. `/` was rejected for exactly
/// that reason — `HAVAALANI/HAVALİMANI KORUMA KUŞAĞI` is one name.
constexpr char kPathSeparator = '>';

/// The group path a caller asked for, outermost name first.
///
/// Surrounding spaces are trimmed on each level, so `SINIRLAR > İDARİ SINIRLAR`
/// and `SINIRLAR>İDARİ SINIRLAR` are the same path — a user reading the printed
/// annex will type the spaces.
std::vector<std::string> requested_path(const std::string& text)
{
    std::vector<std::string> path;
    std::size_t start = 0;

    while (start <= text.size()) {
        const std::size_t stop = text.find(kPathSeparator, start);
        std::string part =
            text.substr(start, stop == std::string::npos ? std::string::npos : stop - start);

        const std::size_t first = part.find_first_not_of(" \t");
        const std::size_t last  = part.find_last_not_of(" \t");
        if (first != std::string::npos) path.push_back(part.substr(first, last - first + 1));

        if (stop == std::string::npos) break;
        start = stop + 1;
    }
    return path;
}

/// One line describing an entry, in the order a person reads it.
std::string describe(const core::LibraryEntry& e)
{
    std::string line = "  " + e.id;
    if (!e.label.empty()) line += "  ·  " + e.label;
    if (e.deprecated) line += "  [yürürlükten kalkmış]";
    return line;
}

Task<void> run(Context& ctx)
{
    core::StyleLibrary& shelf = ctx.session().bus().style_library();

    // ---- load ----
    if (const Value package = ctx.argument("paket"); !package.empty()) {
        auto catalog = load_package(package.as_text());
        if (!catalog) {
            ctx.session().fail(catalog.error());
            co_return;
        }

        // The pictures the rows were published with, read from beside the package
        // and interned into the SHELF's own store. Without this a gallery
        // thumbnail shows a colour where the annex prints a hatch, which is the
        // one thing a person browsing a gösterim set is looking at.
        const std::filesystem::path dir = std::filesystem::path(package.as_text()).parent_path();

        auto resolve = [&](const std::string& file) -> core::ImageId {
            std::ifstream image(dir / file, std::ios::binary);
            if (!image) return core::kNoImage;

            // Read as chars and viewed as bytes: an istreambuf_iterator yields
            // `char`, and a vector<byte> cannot be built from one.
            const std::string raw((std::istreambuf_iterator<char>(image)),
                                  std::istreambuf_iterator<char>());
            if (raw.empty()) return core::kNoImage;

            const std::span<const std::byte> bytes{reinterpret_cast<const std::byte*>(raw.data()),
                                                   raw.size()};

            // A picture the package lists but cannot be read is not fatal: the row
            // keeps its colours and its place on the shelf, and the drawer opens.
            auto id = shelf.intern_image(bytes, file);
            return id ? id.value() : core::kNoImage;
        };

        const std::size_t added = shelf.add_catalog(catalog.value(), resolve, package.as_text());
        ctx.echo("Sembol paketi yüklendi: " + catalog.value().id() + " " +
                 catalog.value().package_version() + " — " + std::to_string(added) +
                 " gösterim, rafta toplam " + std::to_string(shelf.size()) + "."); // ui-label
        ctx.echo("Kaynak: " + catalog.value().source());
        co_return;
    }

    if (shelf.empty()) {
        ctx.echo(
            "Sembol rafı boş. 'SEMBOL paket=<yol>' ile bir gösterim paketi yükleyin."); // ui-label
        co_return;
    }

    // ---- one row in detail ----
    if (const Value code = ctx.argument("kod"); !code.empty()) {
        const core::LibraryEntry* entry = shelf.find(code.as_text());
        if (entry == nullptr) {
            // Hoisted so the gate annotation stays on the line it describes:
            // clang-format is free to rewrap an expression, and an annotation that
            // drifts onto another line stops being a claim about anything.
            const std::string unknown = "Rafta böyle bir gösterim yok: '"; // ui-label
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound,
                          unknown + code.as_text() + "'. 'SEMBOL ara=' ile arayabilirsiniz."));
            co_return;
        }

        ctx.echo(entry->id + "  ·  " + entry->label);
        std::string where;
        for (const std::string& g : entry->group)
            where += (where.empty() ? "" : " > ") + g;
        if (!where.empty()) ctx.echo("  Yer: " + where);
        if (!entry->source_ref.empty()) ctx.echo("  Kaynak: " + entry->source_ref);
        ctx.echo("  Sembol katmanı: " + std::to_string(entry->symbol.layers.size()));
        co_return;
    }

    // ---- search ----
    if (const Value needle = ctx.argument("ara"); !needle.empty()) {
        const auto hits = shelf.search(needle.as_text());
        if (hits.empty()) {
            ctx.echo("'" + needle.as_text() + "' için rafta eşleşme yok.");
            co_return;
        }

        ctx.echo("'" + needle.as_text() + "': " + std::to_string(hits.size()) +
                 " gösterim."); // ui-label
        // Bounded, and it SAYS it is bounded. A search that quietly showed the
        // first twenty of two hundred would read as though there were twenty.
        constexpr std::size_t kMaxShown = 20;
        for (std::size_t i = 0; i < hits.size() && i < kMaxShown; ++i)
            ctx.echo(describe(*hits[i]));
        if (hits.size() > kMaxShown)
            ctx.echo("  … ve " + std::to_string(hits.size() - kMaxShown) +
                     " tane daha. Aramayı daraltın.");
        co_return;
    }

    // ---- walk the tree ----
    const std::vector<std::string> path = requested_path(ctx.argument("grup").as_text());

    std::string where;
    for (const std::string& g : path)
        where += (where.empty() ? "" : " > ") + g;
    ctx.echo(where.empty()
                 ? "Sembol rafı — " + std::to_string(shelf.size()) + " gösterim:" // ui-label
                 : where + ":");

    const auto children = shelf.children(path);
    for (const std::string& child : children)
        ctx.echo("  [" + child + "]");

    const auto here = shelf.in_group(path);
    for (const core::LibraryEntry* e : here)
        ctx.echo(describe(*e));

    if (children.empty() && here.empty()) ctx.echo("  (boş — böyle bir grup yok)");
}

} // namespace

PIRICAD_COMMAND(symbol)
{
    return CommandSpec{
        .id       = "core.symbol",
        .names    = {"SEMBOL", "SEMBOLLER", "SYMBOL", "SMB"},
        .category = Category::Layer,
        .params =
            {
                Param::text("paket", Arity::optional(),
                            "Yüklenecek gösterim paketinin dosya yolu"), // ui-label
                Param::text("grup", Arity::optional(),
                            "Gezilecek grup yolu, düzeyler '>' ile ayrılır"),
                Param::text("ara", Arity::optional(), "Etikette, kimlikte ve grup yolunda arar"),
                Param::text("kod", Arity::optional(), "Tek bir gösterimin ayrıntısı"), // ui-label
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Gösterim rafını yükler, ağacında gezer ve içinde arar.", // ui-label
        .run     = &run,
    };
}

} // namespace piricad::command
