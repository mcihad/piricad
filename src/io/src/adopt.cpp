// SPDX-License-Identifier: GPL-3.0-or-later
#include "adopt.hpp"

#include "xref.hpp"

#include <utility>

namespace kentos::io {

command::Task<core::Result<ProjectReport>> adopt_project(command::Bus& bus, std::string path,
                                                         std::stop_token stop)
{
    // Read into a FRESH document and a FRESH settings store, and put them in
    // place only once the whole file has been read. A failed open therefore costs
    // the user nothing: the drawing on screen is untouched until the last byte is
    // in (io.md P11).
    core::Document loaded;
    core::Settings loaded_settings{core::builtin_settings(), core::SettingScopeMask::Project};

    // Every edit the reader makes goes through this transaction, so a failure
    // half-way unwinds cleanly rather than leaving a half-built document behind
    // (Article 5.9, io.md R17).
    command::Transaction tx(loaded, "Proje dosyası okuma");

    const std::string project = path;
    auto report               = co_await read_project(tx, std::move(path), loaded_settings, stop);
    if (!report) {
        tx.rollback();
        co_return report.error();
    }

    // Resolve the CRS the file named. The reader could not: it has no bus, and the
    // zone catalogue lives in a module /src/io may not reach (Article 3.2). Doing
    // it here means a drawing opened from disk knows its EPSG code exactly as one
    // typed by hand does, and `DIŞAAKTAR` works on it without the user restating
    // something the file already said.
    if (bus.on_crs_resolve && !loaded.crs().id().empty()) {
        core::Op discard;
        const core::Crs resolved = bus.on_crs_resolve(loaded.crs().id());
        if (auto st = loaded.set_crs(resolved, discard); !st)
            report.value().warnings.push_back(Warning{
                "io.crs_resolve", "Dosyadaki koordinat sistemi çözülemedi: " + st.error().message});
    }

    // THE EXTERNAL REFERENCES the drawing holds, read from their files into
    // the same read, before anything is on screen (TODOS C-14, model.md R45a).
    // One whose file cannot be read is a warning, never a failed open: a
    // missing base map must not lock a surveyor out of their own drawing.
    auto missing = co_await load_externals(tx, project, stop);
    report.value().warnings.insert(report.value().warnings.end(), missing.begin(), missing.end());

    bus.document()         = std::move(loaded);
    bus.project_settings() = std::move(loaded_settings);

    // Opening is not undoable and the stack's slots belong to a document that no
    // longer exists, so it goes — along with the active layer, the selection and
    // any batch a script had open around this call. `Bus::document_replaced`
    // holds that list, because `YENİ` needs exactly the same one and two copies
    // of it would be two places to forget an entry. This is the same thing every
    // CAD and GIS application the users know does on File > Open.
    bus.document_replaced();

    if (bus.on_document_changed) bus.on_document_changed();

    co_return std::move(report.value());
}

} // namespace kentos::io
