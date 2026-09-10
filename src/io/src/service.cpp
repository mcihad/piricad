// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/service.hpp"

#include "kentos_cad/io/dwg.hpp"
#include "kentos_cad/io/dxf.hpp"

#include "kentos_cad/core/text.hpp"

#include "kentos_cad/io/point_list.hpp"

#include "adopt.hpp"
#include "qgis_style.hpp"

#include "kentos_cad/io/project.hpp"
#include "kentos_cad/io/vector.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace kentos::io {
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

/// The unit a drawing-format file is read in when it names none, and written in
/// always: the project setting `core.cizim.birim`, which had no consumer before
/// this. Project scope is right (model.md R40): the unit changes every byte of an
/// exported DXF.
core::DrawingUnit effective_unit(const command::Bus& bus)
{
    return core::drawing_unit_from_setting(bus.setting("core.cizim.birim").as_enum());
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

/// Whether the path names a DWG, whatever case it was typed in.
/// `.dxf` by extension, case-folded. The libdxfrw road when the build has it.
bool looks_like_dxf(const std::string& path)
{
    if (path.size() < 4) return false;
    const std::string tail = core::turkish_upper(path.substr(path.size() - 4));
    return tail == ".DXF";
}

bool looks_like_dwg(const std::string& path)
{
    if (path.size() < 4) return false;
    return core::turkish_upper(path.substr(path.size() - 4)) == ".DWG";
}

} // namespace

core::Result<ImportProbe> probe_import(core::Document& scratch, const std::string& path,
                                       const ImportOptions& options, std::stop_token stop)
{
    // The label is never shown: this transaction's inverse Ops are dropped with
    // the scratch document. It is here because Article 5.9 admits exactly one
    // route to geometry and a probe does not get a second one.
    command::Transaction tx(scratch, "İçe aktarma ön okuması");

    ImportProbe out;

    // DRIVEN, NOT AWAITED. `Task<T>` is lazy and the readers below suspend only
    // on their own streaming — never on user input, which is the one thing that
    // would need a bus underneath. So resuming until `done()` runs them to
    // completion, and doing that here keeps coroutine driving inside /src/io
    // rather than spreading it into the dialog code.
    const auto drive = [](auto task) {
        while (!task.done())
            task.resume();
        return std::move(task.result());
    };

    // A probe reads EVERY layer and NO attribute column: the wizard's pages are
    // built from what the file has, and the user has not chosen yet.
    ImportOptions everything = options;
    everything.only.clear();
    everything.fields.clear();

    if (dxf_backend_available() &&
        (looks_like_dxf(path) || core::turkish_iequals(options.driver, "DXF"))) {
        auto read = drive(import_dxf(tx, path, everything, std::move(stop)));
        if (!read) return read.error();

        const DxfReport& d = read.value();
        out.driver         = "DXF " + d.version;
        out.crs            = d.crs;
        out.entities       = d.entities;
        out.diagnostics    = d.diagnostics;
        out.layers.reserve(d.layer_names.size());
        for (const auto& [name, made] : d.layer_names)
            out.layers.emplace_back(name, static_cast<std::uint64_t>(made));
        return out;
    }

    if (looks_like_dwg(path)) {
        auto read = drive(import_dwg(tx, path, everything, std::move(stop)));
        if (!read) return read.error();

        const DwgReport& d = read.value();
        out.driver         = "DWG " + d.version;
        out.crs            = options.project_crs;
        out.entities       = d.entities;
        out.diagnostics    = d.diagnostics;
        out.layers.reserve(d.layer_names.size());
        for (const auto& [name, made] : d.layer_names)
            out.layers.emplace_back(name, static_cast<std::uint64_t>(made));
        return out;
    }

    auto read = drive(import_vector(tx, path, everything, std::move(stop)));
    if (!read) return read.error();

    const VectorReport& r = read.value();
    out.driver            = r.driver;
    out.crs               = r.crs;
    out.fields            = r.fields;
    out.entities          = r.entities;
    out.diagnostics       = r.diagnostics;
    out.layers            = r.layer_names;
    return out;
}

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
    if (current_job_ != nullptr) current_job_->stop.request_stop();
    stop_ = std::stop_source{}; // ready for the next operation
}

core::Result<ImportOutcome> read_into_scratch(command::Transaction& tx, const std::string& path,
                                              const ImportOptions& options, std::stop_token stop)
{
    // DRIVEN, NOT AWAITED — see `probe_import` for why that is safe here.
    const auto drive = [](auto task) {
        while (!task.done())
            task.resume();
        return std::move(task.result());
    };

    ImportOutcome out;
    // DXF GOES TO LIBDXFRW when the build has it (io.md R13): the GDAL driver
    // flattens every curve and drops the blocks and the XDATA before this program
    // sees them. `bicim=DXF` on a file with another extension takes the same road.
    if (dxf_backend_available() &&
        (looks_like_dxf(path) || core::turkish_iequals(options.driver, "DXF"))) {
        auto read = drive(import_dxf(tx, path, options, std::move(stop)));
        if (!read) return read.error();
        const DxfReport& d = read.value();
        out.driver         = "DXF " + d.version;
        out.crs            = d.crs;
        out.entities       = d.entities;
        out.layers         = d.layers;
        out.layer_names    = d.layer_names;
        out.diagnostics    = d.diagnostics;
        return out;
    }
    if (looks_like_dwg(path)) {
        auto read = drive(import_dwg(tx, path, options, std::move(stop)));
        if (!read) return read.error();
        const DwgReport& d = read.value();
        out.driver         = "DWG " + d.version;
        out.crs            = options.project_crs;
        out.entities       = d.entities;
        out.layers         = d.layers;
        out.layer_names    = d.layer_names;
        out.diagnostics    = d.diagnostics;
        return out;
    }

    auto read = drive(import_vector(tx, path, options, std::move(stop)));
    if (!read) return read.error();
    const VectorReport& r = read.value();
    out.driver            = r.driver;
    out.crs               = r.crs;
    out.entities          = r.entities;
    out.layers            = r.layers;
    out.layer_names       = r.layer_names;
    out.diagnostics       = r.diagnostics;
    return out;
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
        co_return co_await import_into(request.tx, request.session, std::move(request.path),
                                       std::move(request.format), std::move(request.layers),
                                       std::move(request.fields));

    case command::FileRequest::Verb::Export:
        co_return co_await export_out(std::move(request.path), std::move(request.format),
                                      request.version);

    case command::FileRequest::Verb::ExportStyle:
        co_return export_style(std::move(request.path), std::move(request.layer));

    case command::FileRequest::Verb::ImportPoints:
        co_return co_await import_points(request.tx, std::move(request.path), request.swapped_axes);

    case command::FileRequest::Verb::ExportPoints:
        co_return export_points(std::move(request.path), request.swapped_axes,
                                std::move(request.entities));
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

    // WHAT IS ON DISK, AS OF NOW. A drawing just read from a file is not dirty,
    // however many revisions the read itself took to build it.
    saved_revision_ = bus_.document().revision();

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

    current_path_   = path;
    saved_revision_ = bus_.document().revision();

    const ProjectReport& r = report.value();
    return std::string(save_as ? "Farklı kaydedildi: " : "Kaydedildi: ") + path + "  (" +
           std::to_string(r.entities) + " nesne, " + std::to_string(r.bytes) + " bayt)";
}

// -------------------------------------------------------------- İÇEAKTAR ----

command::Task<core::Result<std::string>>
FileService::import_into(command::Transaction* tx, command::Session* session, std::string path,
                         std::string format, std::vector<std::string> only,
                         std::vector<std::string> fields)
{
    if (!tx)
        co_return err(ErrorCode::Internal,
                      "İçe aktarma bir işlem (transaction) olmadan istendi; bu bir program "
                      "hatasıdır.");

    if (is_project_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' bir KentOSCad proje dosyası. Proje dosyası açılır, içe aktarılmaz: "
                          "AÇ komutunu kullanın.");

    ImportOptions options;
    options.driver       = std::move(format);
    options.project_crs  = effective_crs(bus_);
    options.only         = std::move(only);
    options.fields       = std::move(fields);
    options.drawing_unit = effective_unit(bus_);

    // PHASE ONE: the read, into a document of its own. Everything below `job.work`
    // may run on a host thread while this frame sits suspended; the scratch
    // document, the outcome and the job live in the frame, which is heap memory
    // that outlives the suspension. The scratch document is touched by exactly
    // one thread at a time — the worker while it runs, this one afterwards.
    auto scratch                        = std::make_unique<core::Document>();
    core::Result<ImportOutcome> outcome = err(ErrorCode::Internal, "Okuma başlamadı.");
    command::Job job;
    job.label = "İçe aktarılıyor: " + std::filesystem::path(path).filename().string();
    job.work  = [&scratch, &outcome, &path, &options](const command::JobControl& control) {
        command::Transaction reading(*scratch, "İçe aktarma okuması");
        outcome = read_into_scratch(reading, path, options, control.stop);
        // The inverse Ops are dropped with the scratch document; nothing here is
        // journalled or undoable (Article 5.9 admits one route to geometry and a
        // read does not get a second one).
        if (!outcome) reading.rollback();
    };
    current_job_ = &job;
    if (session != nullptr)
        co_await command::run_job(*session, job);
    else
        job.work(command::JobControl{job.stop.get_token()});
    current_job_ = nullptr;

    if (!outcome) {
        if (job.stop.stop_requested())
            co_return err(ErrorCode::Cancelled, "İçe aktarma durduruldu; çizim değişmedi.");
        co_return outcome.error();
    }

    // PHASE TWO: into the real document, on this thread, inside the command's one
    // transaction (io.md R17). A failure here rolls the whole import back.
    auto adopted = tx->adopt_from(*scratch);
    if (!adopted) co_return adopted.error();

    // WHAT WAS LEFT BEHIND, IN FRONT OF THE USER. A skipped feature that only
    // reached a counter is a silent loss, which io.md P11 forbids; the transcript
    // carries the count with the first reason, the unit that was used and every
    // type that was read, degraded or left out.
    const ImportOutcome& r = outcome.value();
    std::string said       = "İçe aktarıldı: " + std::to_string(r.entities) + " nesne, " +
                       std::to_string(r.layers) + " katman (" + r.driver + ", " + r.crs + ")" +
                       r.diagnostics.transcript();
    for (const std::string& n : adopted.value().notes)
        said += "\n  not: " + n;
    co_return said;
}

// ------------------------------------------------------------- DIŞAAKTAR ----

command::Task<core::Result<std::string>> FileService::export_out(std::string path,
                                                                 std::string format, int version)
{
    if (is_project_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' bir KentOSCad proje dosyası uzantısı taşıyor. Proje kaydetmek için "
                          "FARKLIKAYDET kullanın.");

    const std::string target = path;
    ExportOptions options;
    options.driver = std::move(format);
    options.crs    = effective_crs(bus_);
    options.unit   = effective_unit(bus_);

    // DXF GOES TO LIBDXFRW when the build has it: a circle is written as a
    // CIRCLE, not as the polygon the GDAL driver would make of it (io.md R13).
    if (dxf_backend_available() &&
        (looks_like_dxf(target) || core::turkish_iequals(options.driver, "DXF"))) {
        const auto ver = dxf_version_from_year(version == 0 ? 2007 : version);
        if (!ver)
            co_return err(ErrorCode::InvalidArgument,
                          "'" + std::to_string(version) +
                              "' bir DXF sürümü değil. Seçenekler: 2000, 2004, 2007, 2010, 2013, "
                              "2018.");
        auto written = co_await export_dxf(bus_.document(), target, std::move(options), *ver,
                                           stop_.get_token());
        if (!written) co_return written.error();
        const DxfReport& d = written.value();
        co_return "Dışa aktarıldı: " + target + "  (" + std::to_string(d.entities) + " nesne, " +
            std::to_string(d.layers) + " katman, DXF " + d.version + ", " + d.crs + ")" +
            d.diagnostics.transcript();
    }
    if (version != 0)
        co_return err(
            ErrorCode::InvalidArgument,
            "surum= yalnız DXF için anlamlıdır" +
                std::string(dxf_backend_available() ? "." : " ve bu yapıda libdxfrw kapalı."));

    auto report = co_await export_vector(bus_.document(), std::move(path), std::move(options),
                                         stop_.get_token());
    if (!report) co_return report.error();

    const VectorReport& r = report.value();
    co_return "Dışa aktarıldı: " + target + "  (" + std::to_string(r.features) + " öğe, " +
        std::to_string(r.layers) + " katman, " + r.driver + ")" + join_notes(r.notes);
}

// ------------------------------------------------------------ point lists ----

command::Task<core::Result<std::string>>
FileService::import_points(command::Transaction* tx, std::string path, bool swapped_axes)
{
    if (tx == nullptr)
        co_return err(ErrorCode::Internal, "Nokta okuma bir işlem içinde çalışmalı.");

    const PointOrder order =
        swapped_axes ? PointOrder::NumberNorthingEasting : PointOrder::NumberEastingNorthing;

    auto read = read_point_list(path, order);
    if (!read) co_return read.error();

    // THE THREE COLUMNS A POINT LIST CARRIES, declared once and reused if they
    // are already there. `declare_attribute` is not undoable by design (a schema
    // is not a drawing edit), so re-importing into the same document adds no
    // second column.
    const core::AttrTable& table = tx->document().attributes();
    const auto column            = [&](const char* id, const char* label,
                            core::AttrType type) -> core::Result<core::AttrId> {
        if (const core::AttrId found = table.find(id); found != core::kNoAttr) return found;

        core::AttrSpec spec;
        spec.id      = id;
        spec.name_tr = label;
        spec.type    = type;
        return tx->declare_attribute(std::move(spec));
    };

    auto no = column("nokta_no", "nokta no", core::AttrType::Text);
    if (!no) co_return no.error();
    auto kot = column("kot", "kot", core::AttrType::Length);
    if (!kot) co_return kot.error();
    auto code = column("kod", "kod", core::AttrType::Text);
    if (!code) co_return code.error();

    const command::LayerId layer = bus_.active_layer();
    std::size_t made             = 0;

    for (const SurveyPoint& p : read.value()) {
        auto created = tx->add_point(layer, p.at);
        if (!created) co_return created.error();

        if (!p.number.empty())
            if (auto st = tx->set_attribute(no.value(), created.value(), core::attr_text(p.number));
                !st)
                co_return st.error();

        if (p.has_height)
            if (auto st = tx->set_attribute(kot.value(), created.value(), core::attr_mm(p.height));
                !st)
                co_return st.error();

        if (!p.code.empty())
            if (auto st = tx->set_attribute(code.value(), created.value(), core::attr_text(p.code));
                !st)
                co_return st.error();
        ++made;
    }

    co_return std::to_string(made) + " nokta okundu: " + path +
        (swapped_axes ? "  (sütunlar X, Y sırasında)" : "");
}

core::Result<std::string> FileService::export_points(std::string path, bool swapped_axes,
                                                     std::vector<std::uint64_t> entities)
{
    const core::Document& doc    = bus_.document();
    const core::AttrTable& table = doc.attributes();
    const core::AttrId no        = table.find("nokta_no");
    const core::AttrId kot       = table.find("kot");
    const core::AttrId code      = table.find("kod");

    std::vector<SurveyPoint> points;

    // THE CORNERS OF NAMED OBJECTS: one row per vertex of every ring, numbered
    // `key.n` so a row can be traced back to its parcel, coded with the layer's
    // name so a list of forty parcels still reads. What a stake-out list is.
    if (!entities.empty()) {
        for (const std::uint64_t raw : entities) {
            const auto key            = static_cast<core::EntityKey>(raw);
            const core::EntityId slot = doc.slot_of(key);
            if (slot == core::kNoEntity || !doc.alive(slot))
                return err(ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));

            const core::Layer* layer  = doc.layer(doc.entities().layer[slot]);
            const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
            std::size_t n             = 0;
            for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                const auto xs = doc.geometry().ring_xs(r);
                const auto ys = doc.geometry().ring_ys(r);
                for (std::size_t i = 0; i < xs.size(); ++i) {
                    SurveyPoint p;
                    p.number = std::to_string(raw) + "." + std::to_string(++n);
                    p.at     = core::Point2{xs[i], ys[i]};
                    if (layer != nullptr) p.code = layer->name;
                    points.push_back(std::move(p));
                }
            }
        }
        if (points.empty())
            return err(ErrorCode::NotFound, "Verilen nesnelerin yazılacak köşesi yok.");

        const PointOrder order =
            swapped_axes ? PointOrder::NumberNorthingEasting : PointOrder::NumberEastingNorthing;
        if (auto st = write_point_list(path, points, order); !st) return st.error();
        return std::to_string(points.size()) + " köşe yazıldı: " + path;
    }

    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.entities().kind[e] != core::kPointKind) continue;

        SurveyPoint p;
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
        if (span.count == 0) continue;
        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        if (xs.empty()) continue;
        p.at = core::Point2{xs[0], ys[0]};

        // The point's own number when the drawing carries one, and its permanent
        // key when it does not — a list whose rows have no name is a list nobody
        // can take back to the field.
        if (no != core::kNoAttr) {
            if (auto had = doc.attribute(no, e); had && had.value().present)
                p.number = had.value().text;
        }
        if (p.number.empty())
            p.number = std::to_string(static_cast<std::uint64_t>(core::raw(doc.entities().key[e])));

        if (kot != core::kNoAttr) {
            if (auto had = doc.attribute(kot, e); had && had.value().present) {
                p.height     = static_cast<core::Mm>(had.value().number);
                p.has_height = true;
            }
        }
        if (code != core::kNoAttr) {
            if (auto had = doc.attribute(code, e); had && had.value().present)
                p.code = had.value().text;
        }
        points.push_back(std::move(p));
    }

    if (points.empty())
        return err(ErrorCode::NotFound,
                   "Çizimde nokta yok. NOKTA komutuyla çizin ya da bir liste okuyun.");

    const PointOrder order =
        swapped_axes ? PointOrder::NumberNorthingEasting : PointOrder::NumberEastingNorthing;
    if (auto st = write_point_list(path, points, order); !st) return st.error();

    return std::to_string(points.size()) + " nokta yazıldı: " + path;
}

} // namespace kentos::io
