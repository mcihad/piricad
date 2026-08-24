// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/io/service.hpp"

#include "adopt.hpp"
#include "qgis_style.hpp"

#include "piricad/io/project.hpp"
#include "piricad/io/vector.hpp"

#include <fstream>
#include <string>
#include <utility>

namespace piricad::io {
namespace {

using core::err;
using core::ErrorCode;

std::string join_warnings(const std::vector<Warning>& warnings)
{
    std::string out;
    for (const Warning& w : warnings) {
        out += "\n  uyarı: ";
        out += w.message;
    }
    return out;
}

std::string join_notes(const std::vector<std::string>& notes)
{
    std::string out;
    for (const std::string& n : notes) {
        out += "\n  not: ";
        out += n;
    }
    return out;
}

/// The coordinate system a user can actually set.
///
/// A NOTE ON A DUPLICATION THIS MODULE DID NOT CREATE. model.md R36/R37 put the
/// CRS on the document, and `core::Document::crs()` is where `content_hash()`
/// folds it from. But the only CRS a client can write today is the project-scope
/// setting `core.crs.id`: `Transaction::set_crs` exists and no command calls it,
/// so `Document::crs()` is stuck at its "TUREF/TM30" default in every drawing the
/// application can produce.
///
/// The project file carries BOTH faithfully, so nothing is lost either way. For
/// an EXPORT there has to be one answer, and it is the setting — that is the
/// value the user set, the value model.md R40 calls part of the exported legal
/// document, and the value AYAR journals. When the two are unified this function
/// becomes `bus.document().crs().id()` and nothing else changes.
/// What a driver should be told the coordinates are in.
///
/// ONE source: the document. The project setting used to be consulted first and
/// the document second, which meant the two could disagree — and they did, because
/// nothing wrote the document's copy. `AYAR koordinat_sistemi` now sets the
/// document through a transaction, so the setting is the interface and the
/// document is the truth (model.md R36, R37).
///
/// A resolved CRS reports its EPSG code, which is what GDAL wants; an unresolved
/// one falls back to its id, so a build with no geodesy module still exports
/// whatever the user typed rather than nothing.
std::string effective_crs(const command::Bus& bus)
{
    const core::Crs& crs = bus.document().crs();
    if (crs.resolved()) return "EPSG:" + std::to_string(crs.epsg());
    return crs.id();
}

} // namespace

FileService::FileService(command::Bus& bus) : bus_(bus)
{
    // `request` is captured BY VALUE into the coroutine below for the reason
    // `project.hpp` gives: a coroutine does not copy its reference parameters.
    bus_.on_file_request =
        [this](const command::FileRequest& request) -> command::Task<core::Result<std::string>> {
        return this->handle(request);
    };

    bus_.on_current_file = [this] { return current_path_; };
}

FileService::~FileService()
{
    // Clearing beats leaving a dangling `this` behind: a Bus that outlives its
    // file service must report "no file engine", not call into freed memory.
    bus_.on_file_request = nullptr;
    bus_.on_current_file = nullptr;
    stop_.request_stop();
}

void FileService::request_stop()
{
    stop_.request_stop();
    stop_ = std::stop_source{}; // ready for the next operation
}

command::Task<core::Result<std::string>> FileService::handle(command::FileRequest request)
{
    switch (request.verb) {
    case command::FileRequest::Verb::Open: co_return co_await open(std::move(request.path));

    case command::FileRequest::Verb::Save:
    case command::FileRequest::Verb::SaveAs: {
        const bool as = request.verb == command::FileRequest::Verb::SaveAs;
        co_return save(request.path, as);
    }

    case command::FileRequest::Verb::Import:
        co_return co_await import_into(request.tx, std::move(request.path),
                                       std::move(request.format));

    case command::FileRequest::Verb::Export:
        co_return co_await export_out(std::move(request.path), std::move(request.format));

    case command::FileRequest::Verb::ExportStyle:
        co_return export_style(std::move(request.path), std::move(request.layer));
    }
    co_return err(ErrorCode::Internal, "Bilinmeyen dosya işlemi.");
}

// ------------------------------------------------------------- QML STİLİ ----

core::Result<std::string> FileService::export_style(std::string path, std::string layer_name)
{
    const core::Document& doc = bus_.document();

    const core::LayerId l = doc.find_layer(layer_name);
    if (l == core::kNoLayer)
        return err(ErrorCode::NotFound,
                   "Katman bulunamadı: '" + layer_name + "'. Önce KATMAN komutuyla oluşturun.");

    const core::Layer& layer = *doc.layer(l);

    // Which symbol describes this layer? The one its entities actually carry, if
    // they agree; the layer default otherwise. Exporting the layer default while
    // every parcel on it carries something else would hand QGIS a style that
    // matches nothing on screen here.
    core::StyleId chosen = core::kByLayerStyle;
    bool agreed          = true;
    bool area            = false;
    std::size_t counted  = 0;

    const auto& entities = doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e) || entities.layer[e] != l) continue;
        ++counted;

        const core::RingSpan span = doc.geometry().rings_of(entities.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            if (doc.geometry().ring_role[r] != core::RingRole::Open) area = true;

        if (counted == 1)
            chosen = entities.style[e];
        else if (entities.style[e] != chosen)
            agreed = false;
    }

    // A layer whose entities carry no explicit style exports its own default —
    // which is exactly what the screen shows for them.
    const core::Symbol effective = chosen == core::kByLayerStyle
                                       ? core::Symbol::of(layer.appearance)
                                       : doc.styles().symbol_at(chosen);

    const std::string body = build_qml(layer, effective, area);

    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out)
        return err(ErrorCode::IoFailure,
                   "'" + path + "' yazılamadı. Dizin izinlerini ve boş alanı denetleyin.");
    out << body;
    if (!out) return err(ErrorCode::IoFailure, "'" + path + "' yazılırken hata oluştu.");

    std::string note = "'" + layer.name + "' stili QML olarak yazıldı: " + path;
    if (!agreed)
        note += ". Uyarı: bu katmandaki nesneler tek bir stil taşımıyor; QML tek sembol "
                "biçiminde yazıldı ve ilk nesnenin stilini taşıyor. Kategorize dışa aktarım "
                "Faz 1'de gelecek.";
    return note;
}

// -------------------------------------------------------------------- AÇ ----

command::Task<core::Result<std::string>> FileService::open(std::string path)
{
    // The read, the CRS resolution and the swap all live in `adopt_project`,
    // because `VERİTABANI projeac` does exactly the same thing from a different
    // source and a second copy of that sequence is a second place for it to go
    // wrong. What is left here is what is specific to a FILE: remembering which
    // one the document now belongs to, and saying so.
    auto report = co_await adopt_project(bus_, path, stop_.get_token());
    if (!report) co_return report.error();

    current_path_ = std::move(path);

    const ProjectReport& r = report.value();
    co_return "Açıldı: " + current_path_ + "  (" + std::to_string(r.entities) + " nesne, " +
        std::to_string(r.layers) + " katman, " + std::to_string(r.vertices) + " nokta, biçim " +
        std::to_string(r.format_version) + ")" + join_warnings(r.warnings);
}

// -------------------------------------------- KAYDET / FARKLIKAYDET ---------

core::Result<std::string> FileService::save(const std::string& path, bool save_as)
{
    if (path.empty())
        return err(ErrorCode::InvalidArgument,
                   "Bu çizim henüz bir dosyaya bağlı değil. FARKLIKAYDET ile bir ad verin.");

    auto report = save_project(bus_.document(), bus_.project_settings(), path);
    if (!report) return report.error();

    current_path_ = path;

    const ProjectReport& r = report.value();
    return std::string(save_as ? "Farklı kaydedildi: " : "Kaydedildi: ") + path + "  (" +
           std::to_string(r.entities) + " nesne, " + std::to_string(r.bytes) + " bayt)";
}

// -------------------------------------------------------------- İÇEAKTAR ----

command::Task<core::Result<std::string>>
FileService::import_into(command::Transaction* tx, std::string path, std::string format)
{
    if (!tx)
        co_return err(ErrorCode::Internal,
                      "İçe aktarma bir işlem (transaction) olmadan istendi; bu bir program "
                      "hatasıdır.");

    if (is_project_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' bir PiriCAD proje dosyası. Proje dosyası açılır, içe aktarılmaz: "
                          "AÇ komutunu kullanın.");

    auto report = co_await import_vector(*tx, std::move(path), std::move(format),
                                         effective_crs(bus_), stop_.get_token());
    if (!report) co_return report.error();

    const VectorReport& r = report.value();
    co_return "İçe aktarıldı: " + std::to_string(r.entities) + " nesne, " +
        std::to_string(r.layers) + " katman (" + r.driver + ", " + r.crs + ")" +
        join_notes(r.notes);
}

// ------------------------------------------------------------- DIŞAAKTAR ----

command::Task<core::Result<std::string>> FileService::export_out(std::string path,
                                                                 std::string format)
{
    if (is_project_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' bir PiriCAD proje dosyası uzantısı taşıyor. Proje kaydetmek için "
                          "FARKLIKAYDET kullanın.");

    const std::string target = path;
    auto report = co_await export_vector(bus_.document(), std::move(path), std::move(format),
                                         effective_crs(bus_), stop_.get_token());
    if (!report) co_return report.error();

    const VectorReport& r = report.value();
    co_return "Dışa aktarıldı: " + target + "  (" + std::to_string(r.features) + " öğe, " +
        std::to_string(r.layers) + " katman, " + r.driver + ")" + join_notes(r.notes);
}

} // namespace piricad::io
