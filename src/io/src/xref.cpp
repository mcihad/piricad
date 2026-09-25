// SPDX-License-Identifier: GPL-3.0-or-later
#include "xref.hpp"

#include "kentos_cad/command/external_ref.hpp"
#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/io/dwg.hpp"
#include "kentos_cad/io/dxf.hpp"
#include "kentos_cad/io/options.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace kentos::io {
namespace {

using core::err;
using core::ErrorCode;
namespace fs = std::filesystem;

/// Whether two paths name the same file, compared as the filesystem sees them
/// when both exist and as normalised text otherwise.
bool same_file(const std::string& a, const std::string& b)
{
    std::error_code ec;
    if (fs::exists(a, ec) && fs::exists(b, ec)) return fs::equivalent(a, b, ec);
    return fs::path(a).lexically_normal() == fs::path(b).lexically_normal();
}

} // namespace

bool looks_like_dxf(const std::string& path)
{
    if (path.size() < 4) return false;
    return core::turkish_upper(path.substr(path.size() - 4)) == ".DXF";
}

bool looks_like_dwg(const std::string& path)
{
    if (path.size() < 4) return false;
    return core::turkish_upper(path.substr(path.size() - 4)) == ".DWG";
}

std::string crs_for_reading(const core::Crs& crs)
{
    if (crs.resolved()) return "EPSG:" + std::to_string(crs.epsg());
    return crs.id();
}

command::Task<core::Status> read_drawing(core::Document& scratch, std::string path, std::string crs,
                                         std::stop_token stop)
{
    command::Transaction into(scratch, "cizim-oku");
    ImportOptions reading;
    reading.project_crs = std::move(crs);
    if (looks_like_dxf(path)) {
        if (!dxf_backend_available())
            co_return err(ErrorCode::Unsupported, "Bu yapıda DXF okuyucu yok.");
        auto read = co_await import_dxf(into, path, reading, stop);
        if (!read) co_return read.error();
    } else if (looks_like_dwg(path)) {
        if (!dwg_backend_available())
            co_return err(ErrorCode::Unsupported,
                          "Bu yapıda DWG okuyucu yok; dosyayı DXF olarak kaydedin.");
        auto read = co_await import_dwg(into, path, reading, stop);
        if (!read) co_return read.error();
    } else {
        core::Settings ignored{core::builtin_settings(), core::SettingScopeMask::Project};
        auto read = co_await read_project(into, path, ignored, stop);
        if (!read) co_return read.error();
    }
    co_return core::ok();
}

std::string locate_external(const std::string& path, const std::string& project)
{
    std::error_code ec;
    if (path.empty()) return {};
    if (fs::is_regular_file(path, ec)) return fs::path(path).lexically_normal().string();
    if (project.empty()) return {};
    const fs::path beside =
        fs::absolute(project, ec).parent_path() / fs::path(std::string(core::file_name_of(path)));
    if (fs::is_regular_file(beside, ec)) return beside.lexically_normal().string();
    return {};
}

command::Task<core::Result<XrefLoad>> load_external(command::Transaction& tx, core::BlockId block,
                                                    std::string project, std::stop_token stop)
{
    core::Document& doc = tx.document();
    if (block >= doc.blocks().size())
        co_return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(block));
    // COPIED: the table grows while the file's blocks come in, and a reference
    // into it would not survive that.
    const core::BlockDef def = doc.blocks().at(block);
    if ((def.flags & core::kBlockExternal) == 0)
        co_return err(ErrorCode::InvalidArgument, "'" + def.name + "' bir dış referans değil.");

    XrefLoad out;
    out.found_at = locate_external(def.path, project);
    if (out.found_at.empty())
        co_return err(
            ErrorCode::NotFound,
            "'" + def.path + "' bulunamadı" +
                (project.empty() ? std::string() : ", proje dosyasının klasöründe de yok") + ".");
    out.moved = out.found_at != fs::path(def.path).lexically_normal().string();
    if (!project.empty() && same_file(out.found_at, project))
        co_return err(ErrorCode::InvalidArgument,
                      "Çizim kendi dosyasına dış referans olamaz: '" + out.found_at + "'.");

    // THE FILE, READ WHERE NOTHING SEES IT, then brought across in one step.
    core::Document scratch;
    if (auto read = co_await read_drawing(scratch, out.found_at, crs_for_reading(doc.crs()), stop);
        !read)
        co_return read.error();
    // THE SAME SYSTEM OR NOT AT ALL: a reference drawn in another system lands
    // kilometres away and looks right (model.md R36), so it is refused until
    // references are reprojected rather than drawn where it would mislead.
    const std::string theirs = scratch.crs().id();
    const std::string ours   = doc.crs().id();
    if (!looks_like_dxf(out.found_at) && !looks_like_dwg(out.found_at) && !theirs.empty() &&
        !ours.empty() && !core::turkish_iequals(theirs, ours))
        co_return err(ErrorCode::ValidationFailed,
                      "'" + out.found_at + "' " + theirs + " koordinat sisteminde, çizim " + ours +
                          " sisteminde. Farklı sistemdeki bir dosya bu sürümde dış referans olarak "
                          "yüklenmez; dosyayı çizimin sistemine dönüştürün.");

    const std::size_t mark = tx.size();
    auto emptied           = command::empty_external(tx, block);
    if (!emptied) {
        tx.rollback_to(mark);
        co_return emptied.error();
    }
    command::Transaction::AdoptOptions options;
    options.into   = block;
    options.prefix = command::external_prefix(def);
    auto adopted   = tx.adopt_from(scratch, options);
    if (!adopted) {
        tx.rollback_to(mark);
        co_return adopted.error();
    }
    if (out.moved)
        if (auto st = tx.set_block_external(block, out.found_at, def.flags); !st) {
            tx.rollback_to(mark);
            co_return st.error();
        }
    if (auto refreshed = tx.refresh_block_references(block); !refreshed) {
        tx.rollback_to(mark);
        co_return refreshed.error();
    }
    out.entities = adopted.value().entities;
    out.layers   = adopted.value().layers;
    out.notes    = adopted.value().notes;
    co_return out;
}

command::Task<std::vector<Warning>> load_externals(command::Transaction& tx, std::string project,
                                                   std::stop_token stop)
{
    std::vector<Warning> warnings;
    const core::Document& doc = tx.document();
    // By id, and to the size the table had: a reference's file may define
    // blocks, which are appended while it loads.
    const std::size_t count = doc.blocks().size();
    for (core::BlockId b = 0; b < count; ++b) {
        const std::uint8_t flags = doc.blocks().at(b).flags;
        if ((flags & core::kBlockExternal) == 0 ||
            (flags & (core::kBlockUnloaded | core::kBlockDetached)) != 0)
            continue;
        const std::string name = doc.blocks().at(b).name;
        auto loaded            = co_await load_external(tx, b, project, stop);
        if (!loaded) {
            warnings.push_back(Warning{
                "io.xref", "'" + name + "' dış referansı yüklenemedi: " + loaded.error().message +
                               " Çizim açıldı; referans boş çizilir. Dosyanın yeni yerini "
                               "DIŞREFERANS islem=yol ad=" +
                               name + " dosya=<yol> ile gösterin."});
            continue;
        }
        if (loaded.value().moved)
            warnings.push_back(Warning{"io.xref_moved", "'" + name +
                                                            "' dış referansı kayıtlı yerinde "
                                                            "yoktu, proje klasöründe bulundu: " +
                                                            loaded.value().found_at + "."});
    }
    co_return warnings;
}

} // namespace kentos::io
