// SPDX-License-Identifier: GPL-3.0-or-later
#include "adopt.hpp"

#include <utility>

namespace piricad::io {

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

    auto report = co_await read_project(tx, std::move(path), loaded_settings, std::move(stop));
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

    bus.document()         = std::move(loaded);
    bus.project_settings() = std::move(loaded_settings);

    // Opening is not undoable and the stack's slots belong to a document that no
    // longer exists, so it goes. This is the same thing every CAD and GIS
    // application the users know does on File > Open.
    bus.undo_stack().clear();
    bus.set_active_layer(0);

    if (bus.on_document_changed) bus.on_document_changed();

    co_return std::move(report.value());
}

} // namespace piricad::io
