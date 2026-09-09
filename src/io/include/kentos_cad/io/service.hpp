// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: the file engine, plugged into the command bus.
//
// WHY THIS SEAM EXISTS, stated once so nobody has to rediscover it.
//
// Constitution Article 3.2 makes `io -> command` one-way: /src/io may include
// /src/command, and /src/command may never include /src/io. But io.md R4 also
// requires every import and export to be a command registered in `Registry`, and
// the registry, the CLI help, the AI schema and `kentos_docgen` all live in
// /src/command. A command whose factory lived in /src/io would be invisible to
// docgen, and `docs/komutlar/referans.md` is generated from the registry
// (CLAUDE.md 5.18) — so the reference would silently lose the file commands.
//
// The resolution is the one /src/script already uses for `BETİK`: the CommandSpec
// and the command body live in /src/command/src/commands/file.cpp, and the actual
// work arrives through a `std::function` on the `Bus` that THIS module installs.
// `Bus::on_run_script` is the precedent, `Bus::on_file_request` is the same shape,
// and `Category::File` was reserved in `spec.hpp` from the beginning.
//
// A headless client that never installs a FileService gets a clear
// "Dosya motoru bağlı değil." from the commands rather than a crash — the same
// answer BETİK gives without a script engine.
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/vector.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace kentos::io {

/// What one READ-ONLY look at an import file found: which layers it holds, how
/// many entities each would produce, and what the reader wants to say about it.
///
/// It exists for the import wizard, whose second page cannot honestly ask "which
/// of these layers do you want" until something has actually read the file. The
/// same numbers are what the wizard prints beside each tick box.
struct ImportProbe
{
    std::string driver;                                        ///< DXF, DWG, GPKG…
    std::string crs;                                           ///< what the file declared
    std::uint64_t entities{0};                                 ///< across every layer
    std::vector<std::pair<std::string, std::uint64_t>> layers; ///< name, entity count
    std::vector<std::string> notes;                            ///< the reader's own words
    std::vector<VectorField> fields;                           ///< every attribute field, per layer
};

/// Reads `path` into `scratch` — a document the caller owns and the user has never
/// seen — and reports what came out.
///
/// THIS IS NOT A SHORTCUT PAST THE COMMAND BUS. Article 5.9 forbids mutating THE
/// document outside a command; `scratch` is a throwaway the caller allocated for
/// the purpose, holds no user work, is never journalled and is never rendered as
/// the drawing. The real import still goes through `İÇEAKTAR` afterwards, with the
/// layer names the user ticked — which is why the wizard's OK button produces a
/// journal line identical to the one a script would write.
///
/// Synchronous on purpose: the readers it drives suspend only for their own
/// streaming, never for user input, and a modal wizard has nothing else to do
/// while it waits.
core::Result<ImportProbe> probe_import(core::Document& scratch, const std::string& path,
                                       const std::string& project_crs, std::stop_token stop);

/// Owns the file engine for exactly one `Bus`, and therefore for exactly one
/// document. Installing the hook in the constructor and clearing it in the
/// destructor is what keeps a service from outliving the bus it points at.
///
/// Deliberately not a singleton and not a process-wide static: two documents open
/// in one process must not share a current-file path (core.md P8, and the same
/// mistake `Bus::project_settings()` was moved off a static to avoid).
class FileService
{
public:
    /// Installs `Bus::on_file_request` and clears it on destruction. That hook is
    /// the seam that lets /src/command own the file COMMANDS while /src/io owns
    /// the file WORK, without command including io (Article 3.2).
    explicit FileService(command::Bus& bus);
    ~FileService();

    /// Non-copyable: it owns the bus hook, and two services would fight over it.
    FileService(const FileService&)            = delete;
    FileService& operator=(const FileService&) = delete;

    /// The project file the document currently belongs to, or empty when it has
    /// never been saved. NOT document state (model.md R43): it is not hashed, not
    /// journalled and not undoable.
    const std::string& current_path() const noexcept { return current_path_; }

    /// The document revision the file on disk was written from.
    ///
    /// WHY HERE AND NOT IN THE DOCUMENT. "Has this been saved" is a fact about
    /// the FILE, not about the drawing: two documents open in one process are
    /// saved separately, and `Document::revision()` is a content counter that
    /// knows nothing about disks (model.md R43). Zero means "never written", and
    /// an empty drawing at revision zero is correctly not dirty.
    std::uint64_t saved_revision() const noexcept { return saved_revision_; }

    /// Asks every running read to stop. io.md R15: a cancelled read returns
    /// within 100 ms.
    void request_stop();

private:
    /// The request is taken BY VALUE: a coroutine does not copy its reference
    /// parameters into its frame, and this one is called through a `std::function`
    /// whose argument is a temporary at every GUI call site.
    command::Task<core::Result<std::string>> handle(command::FileRequest request);

private:
    /// Writes one layer's symbology as a QGIS QML style file. Not a coroutine:
    /// it touches one layer and a few dozen lines of XML, so there is nothing to
    /// suspend for.
    core::Result<std::string> export_style(std::string path, std::string layer_name);

    command::Task<core::Result<std::string>> open(std::string path);
    core::Result<std::string> save(const std::string& path, bool save_as);

    // Named `import_into` / `export_out` rather than `import` / `export`: those
    // two words are module directives in C++20 and CLAUDE.md 5.2 bans modules
    // outright, so neither should appear as an identifier at the head of a line
    // where a reader — or a grep — could mistake it for one.
    command::Task<core::Result<std::string>> import_into(command::Transaction* tx, std::string path,
                                                         std::string format,
                                                         std::vector<std::string> only,
                                                         std::vector<std::string> fields);
    command::Task<core::Result<std::string>> export_out(std::string path, std::string format);

    /// Reads a surveyed point list and puts one point entity per row in the
    /// drawing, with `nokta_no`, `kot` and `kod` as attributes.
    ///
    /// `swapped_axes` says the file's columns run `no X Y` instead of the Turkish
    /// `no Y X`; see `io/point_list.hpp` for why that is stated and never guessed.
    command::Task<core::Result<std::string>> import_points(command::Transaction* tx,
                                                           std::string path, bool swapped_axes);

    /// Writes every point entity in the drawing back out in the same shape.
    core::Result<std::string> export_points(std::string path, bool swapped_axes,
                                            std::vector<std::uint64_t> entities);

    command::Bus& bus_;
    std::string current_path_;
    std::uint64_t saved_revision_{0};
    std::stop_source stop_;
};

} // namespace kentos::io
