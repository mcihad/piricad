// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/io/database.hpp"

#include "adopt.hpp"

#include "piricad/io/project.hpp"

#if PIRICAD_HAVE_POSTGIS
#include "piricad/io/postgis.hpp"
#endif

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace piricad::io {
namespace {

using core::err;
using core::ErrorCode;

namespace fs = std::filesystem;

#if PIRICAD_HAVE_POSTGIS

/// A private directory under the system temp path, removed when it goes away.
///
/// WHY A TEMPORARY FILE IS IN THIS PATH AT ALL. A project is stored in the
/// database as the bytes of its `.pcad` file, and this program has exactly one
/// writer and one reader for that format — one thing to fuzz, one thing to
/// version, one thing whose round trip is checked by the fingerprint on every
/// open. Both are path-based: the writer renames a temporary into place, and the
/// reader mmaps, because io.md R5 wants the geometry blocks usable without
/// parsing. Giving the database its own in-memory writer and reader would be a
/// SECOND implementation of the format, and the moment the two drift, a project
/// saved to the database stops being a project.
///
/// So the bytes travel through a file, and the cost is one extra local write and
/// read either way — noise beside sending the same bytes over a network. This
/// goes away when `MappedFile` learns to adopt a buffer instead of a mapping, at
/// which point the reader takes bytes directly and the writer gains a sink.
///
/// The DIRECTORY is what makes it safe: created fresh and owned by this process,
/// so nothing in a shared `/tmp` can substitute a symlink for the file we are
/// about to write.
class ScratchDir
{
public:
    /// Creates it. `valid()` is false when the system temp directory is not
    /// usable, which a caller must check rather than assume.
    explicit ScratchDir(std::string_view tag)
    {
        std::error_code ec;
        const fs::path base = fs::temp_directory_path(ec);
        if (ec) return;

        // A name unique to this attempt. `create_directory` returning false means
        // it already existed, and we take a different name rather than reuse
        // somebody else's directory.
        for (int attempt = 0; attempt < 64; ++attempt) {
            const fs::path candidate =
                base / ("piricad-" + std::string(tag) + "-" + std::to_string(counter_++));
            if (fs::create_directory(candidate, ec) && !ec) {
                path_ = candidate;
                return;
            }
        }
    }

    ~ScratchDir()
    {
        if (path_.empty()) return;
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    ScratchDir(const ScratchDir&)            = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;

    bool valid() const noexcept { return !path_.empty(); }

    /// The path a project's bytes should travel through.
    std::string file() const { return (path_ / "proje.pcad").string(); }

private:
    fs::path path_;

    /// Distinguishes two scratch directories made in the same millisecond. Not a
    /// security measure — the directory creation is — just a way to stop the loop
    /// above from retrying the same name.
    static inline unsigned counter_ = 0;
};

/// Reads a whole file into memory.
core::Result<std::vector<std::byte>> read_all(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return err(ErrorCode::IoFailure, "'" + path + "' okunamadı.");

    const std::streamoff size = in.tellg();
    if (size <= 0) return err(ErrorCode::IoFailure, "'" + path + "' boş.");
    in.seekg(0);

    std::vector<std::byte> out(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(out.data()), size);
    if (!in) return err(ErrorCode::IoFailure, "'" + path + "' tamamı okunamadı.");
    return out;
}

/// Writes a blob out whole.
core::Status write_all(const std::string& path, const std::vector<std::byte>& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return err(ErrorCode::IoFailure, "'" + path + "' yazmak için açılamadı.");

    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) return err(ErrorCode::IoFailure, "'" + path + "' yazılamadı.");
    return core::ok();
}

/// A byte count as a person reads it.
std::string human_bytes(std::int64_t bytes)
{
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

#endif // PIRICAD_HAVE_POSTGIS

} // namespace

// ------------------------------------------------------------------ Impl ----

struct DatabaseService::Impl
{
#if PIRICAD_HAVE_POSTGIS
    std::unique_ptr<PostgisStore> store;
#endif
};

DatabaseService::DatabaseService(command::Bus& bus) : bus_(bus), impl_(std::make_unique<Impl>())
{
    bus_.on_database_request = [this](const command::DatabaseRequest& request)
        -> command::Task<core::Result<std::string>> { return this->handle(request); };
}

DatabaseService::~DatabaseService()
{
    bus_.on_database_request = nullptr;
}

bool DatabaseService::available() noexcept
{
#if PIRICAD_HAVE_POSTGIS
    return true;
#else
    return false;
#endif
}

bool DatabaseService::connected() const noexcept
{
#if PIRICAD_HAVE_POSTGIS
    return impl_->store != nullptr;
#else
    return false;
#endif
}

#if !PIRICAD_HAVE_POSTGIS

command::Task<core::Result<std::string>> DatabaseService::handle(command::DatabaseRequest)
{
    // A build without the dependency says so PLAINLY and in the same shape as
    // every other missing engine in this program. It does not pretend, and it does
    // not crash (build.md: an optional dependency is invisible to its callers
    // except in the message it gives when asked to work).
    co_return err(ErrorCode::Unsupported,
                  "Bu PiriCAD yapısı PostgreSQL desteği olmadan derlenmiş. "
                  "Kaynaktan derliyorsanız PIRICAD_WITH_POSTGIS=ON ile yapılandırın.");
}

#else

command::Task<core::Result<std::string>> DatabaseService::handle(command::DatabaseRequest request)
{
    using Verb = command::DatabaseRequest::Verb;

    // ---- baglan ----
    if (request.verb == Verb::Connect) {
        auto opened = PostgisStore::open(request.target);
        if (!opened) co_return opened.error();

        impl_->store = std::move(opened.value());

        // The connection string is NOT kept as typed: it may carry a password and
        // this string reaches the GUI, where a screenshot travels further than a
        // journal does. The SAME redactor the command layer journals through, so
        // the two cannot drift into covering different forms.
        target_ = command::redact_conninfo(request.target);

        // The server's own `version()` is a long sentence; its first two words are
        // the part a user checks.
        std::string server = impl_->store->server_version();
        if (const std::size_t comma = server.find(' ', server.find(' ') + 1);
            comma != std::string::npos)
            server.resize(comma);

        co_return "Bağlanıldı: " + server + ", PostGIS " + impl_->store->postgis_version() + ".";
    }

    // Everything below needs a connection, and saying so once is better than six
    // copies of the same sentence.
    if (request.verb != Verb::Disconnect && impl_->store == nullptr)
        co_return err(ErrorCode::Unsupported,
                      "Veritabanına bağlı değilsiniz. Önce: VERİTABANI baglan hedef=\"...\"");

    switch (request.verb) {
    // ---- kes ----
    case Verb::Disconnect: {
        if (impl_->store == nullptr) co_return std::string("Zaten bağlı değilsiniz.");
        impl_->store.reset();
        target_.clear();
        co_return std::string("Veritabanı bağlantısı kapatıldı.");
    }

    // ---- tablolar ----
    case Verb::Tables: {
        auto found = impl_->store->tables();
        if (!found) co_return found.error();

        if (found.value().empty())
            co_return std::string("Bu veritabanında hiç mekansal tablo yok.");

        std::string out = std::to_string(found.value().size()) + " mekansal tablo:";
        for (const PostgisTable& t : found.value())
            out += "\n  " + t.schema + "." + t.name + "  (" + t.geometry +
                   ", EPSG:" + std::to_string(t.srid) + ", ~" + std::to_string(t.rows) + " satır)";
        co_return out;
    }

    // ---- katmanyaz ----
    case Verb::WriteLayer: {
        const core::LayerId layer = bus_.document().find_layer(request.layer);
        if (layer == core::kNoLayer)
            co_return err(ErrorCode::NotFound, "Katman bulunamadı: '" + request.layer + "'.");

        // The SRID has to be a real one. A spatial table labelled with the wrong
        // SRID moves every coordinate in it by kilometres and does so silently,
        // because the numbers still look like Turkish coordinates. Refusing here
        // is the whole point of model.md R36.
        if (!bus_.document().crs().resolved())
            co_return err(ErrorCode::ValidationFailed,
                          "Çizimin koordinat sistemi çözülmemiş, tabloya SRID yazılamaz. "
                          "Önce: AYAR koordinat_sistemi deger=TUREF/TM30");

        auto written = impl_->store->write_layer(bus_.document(), layer, request.target);
        if (!written) co_return written.error();

        co_return "'" + request.layer + "' katmanı yazıldı: " + std::to_string(written.value()) +
            " satır, EPSG:" + std::to_string(bus_.document().crs().epsg()) + ".";
    }

    // ---- projekaydet ----
    case Verb::SaveProject: {
        ScratchDir scratch("kaydet");
        if (!scratch.valid())
            co_return err(ErrorCode::IoFailure,
                          "Geçici dizin oluşturulamadı; sistem geçici dizinini denetleyin.");

        auto report = save_project(bus_.document(), bus_.project_settings(), scratch.file());
        if (!report) co_return report.error();

        auto bytes = read_all(scratch.file());
        if (!bytes) co_return bytes.error();

        if (auto st = impl_->store->write_project(request.target, bytes.value()); !st)
            co_return st.error();

        co_return "Proje veritabanına kaydedildi: '" + request.target + "'  (" +
            std::to_string(report.value().entities) + " nesne, " +
            human_bytes(static_cast<std::int64_t>(bytes.value().size())) + ").";
    }

    // ---- projeac ----
    case Verb::OpenProject: {
        auto bytes = impl_->store->read_project(request.target);
        if (!bytes) co_return bytes.error();

        ScratchDir scratch("ac");
        if (!scratch.valid())
            co_return err(ErrorCode::IoFailure,
                          "Geçici dizin oluşturulamadı; sistem geçici dizinini denetleyin.");

        if (auto st = write_all(scratch.file(), bytes.value()); !st) co_return st.error();

        // The SAME swap `AÇ` performs, from the same function: read into a fresh
        // document, resolve the CRS, replace only when the last byte is in. A
        // failed read leaves the drawing on screen untouched.
        //
        // No stop token: this reads a local temporary that is already in the page
        // cache, so there is nothing worth cancelling. The download above is the
        // slow part and it has already finished.
        auto report = co_await adopt_project(bus_, scratch.file(), std::stop_token{});
        if (!report) co_return report.error();

        const ProjectReport& r = report.value();
        co_return "Veritabanından açıldı: '" + request.target + "'  (" +
            std::to_string(r.entities) + " nesne, " + std::to_string(r.layers) + " katman, " +
            std::to_string(r.vertices) + " nokta).";
    }

    // ---- projeler ----
    case Verb::Projects: {
        auto found = impl_->store->projects();
        if (!found) co_return found.error();

        if (found.value().empty()) co_return std::string("Bu veritabanında kayıtlı proje yok.");

        std::string out = std::to_string(found.value().size()) + " kayıtlı proje:";
        for (const PostgisProject& p : found.value())
            out += "\n  " + p.name + "  (" + human_bytes(p.bytes) + ", " + p.updated + ")";
        co_return out;
    }

    // ---- projesil ----
    case Verb::DropProject: {
        auto gone = impl_->store->delete_project(request.target);
        if (!gone) co_return gone.error();

        // "There was nothing by that name" is not an error — a script that tidies
        // up before it runs must not abort because the tidying was unnecessary —
        // but it is not silence either.
        if (gone.value() == 0)
            co_return "Veritabanında böyle bir proje yoktu: '" + request.target + "'.";
        co_return "Proje veritabanından silindi: '" + request.target + "'.";
    }

    case Verb::Connect: break; // handled above
    }

    co_return err(ErrorCode::Internal, "Bilinmeyen veritabanı işlemi.");
}

#endif // PIRICAD_HAVE_POSTGIS

} // namespace piricad::io
