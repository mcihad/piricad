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

/// How deep references inside references are followed.
constexpr std::size_t kMaxNesting = 8;

/// Carries `scratch` from `from` to `to` with `DÖNÜŞTÜR`, run on a bus of its
/// own over `registry` — the program's one reprojection, not a second copy of
/// it. A layer the file locked would refuse the move, so the locks are lifted
/// for it and put back after: the scratch is nobody's work, and what it
/// brings keeps the locks its file set.
core::Status reproject_scratch(core::Document& scratch, command::Registry& registry,
                               const std::string& from, const std::string& to)
{
    std::vector<core::LayerId> locked;
    {
        command::Transaction lift(scratch, "kilit-ac");
        for (std::size_t l = 0; l < scratch.layer_table().size(); ++l) {
            const core::Layer* layer = scratch.layer_table().at(static_cast<core::LayerId>(l));
            if (layer == nullptr || !layer->locked) continue;
            if (auto st = lift.set_layer_locked(static_cast<core::LayerId>(l), false); !st)
                return st;
            locked.push_back(static_cast<core::LayerId>(l));
        }
    }
    command::Journal journal;
    command::UndoStack undo;
    command::Bus side{scratch, registry, journal, undo};
    auto moved = side.execute_line("DÖNÜŞTÜR kaynak=\"" + from + "\" hedef=\"" + to + "\"",
                                   command::Origin::Batch);
    {
        command::Transaction put_back(scratch, "kilit-kapat");
        for (const core::LayerId l : locked)
            (void)put_back.set_layer_locked(l, true);
    }
    if (!moved) return moved.error();
    return core::ok();
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
                                                    std::string project, std::stop_token stop,
                                                    command::Bus* host,
                                                    std::vector<std::string> chain)
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
    // A FILE ALREADY BEING READ on the way down is a loop — A references B,
    // B references A — and reading it again would never end.
    for (const std::string& above : chain)
        if (same_file(out.found_at, above))
            co_return err(ErrorCode::ValidationFailed,
                          "Dış referans döngüsü: '" + out.found_at +
                              "' kendisini içeren bir dosyanın içinden yeniden isteniyor; bu "
                              "halka okunmadı.");
    if (chain.size() >= kMaxNesting)
        co_return err(ErrorCode::ValidationFailed,
                      "Dış referanslar " + std::to_string(kMaxNesting) + " kattan derin iç içe; '" +
                          out.found_at + "' okunmadı.");

    // THE FILE, READ WHERE NOTHING SEES IT, then brought across in one step.
    core::Document scratch;
    if (auto read = co_await read_drawing(scratch, out.found_at, crs_for_reading(doc.crs()), stop);
        !read)
        co_return read.error();
    // THE DRAWING'S SYSTEM, ALWAYS: a reference drawn in another one lands
    // kilometres away and looks right (model.md R36). The two are compared by
    // their EPSG codes where both resolve — `TUREF/TM30` is `EPSG:5254` — and
    // by name where not; a file in another system is carried into the
    // drawing's, and refused where nothing here can carry it.
    // A DXF or a DWG is read IN the drawing's system (`read_drawing`'s `crs`,
    // io.md R20), so only a project file, which carries its own, is compared.
    const std::string theirs_id = scratch.crs().id();
    const core::Crs& ours       = doc.crs();
    const bool carries_its_own  = !looks_like_dxf(out.found_at) && !looks_like_dwg(out.found_at);
    if (carries_its_own && !theirs_id.empty() && !ours.id().empty()) {
        const core::Crs theirs = host != nullptr && host->on_crs_resolve
                                     ? host->on_crs_resolve(theirs_id)
                                     : core::Crs(theirs_id);
        const bool same        = theirs.resolved() && ours.resolved()
                                     ? theirs.epsg() == ours.epsg()
                                     : core::turkish_iequals(theirs_id, ours.id());
        if (!same) {
            if (host == nullptr || host->registry().by_id("core.reproject") == nullptr)
                co_return err(ErrorCode::ValidationFailed,
                              "'" + out.found_at + "' " + theirs_id +
                                  " koordinat sisteminde, çizim " + ours.id() +
                                  " sisteminde, ve bu yapı koordinat dönüştüremiyor; dosyayı "
                                  "çizimin sistemine dönüştürün.");
            if (auto st = reproject_scratch(scratch, host->registry(), crs_for_reading(theirs),
                                            crs_for_reading(ours));
                !st)
                co_return err(
                    st.error().code,
                    "'" + out.found_at + "' " + theirs_id +
                        " sisteminden çizimin sistemine dönüştürülemedi: " + st.error().message);
            out.reprojected_from = theirs_id;
        }
    }

    // ITS OWN REFERENCES, read into it now that it stands in the drawing's
    // system, so each is carried from wherever it was drawn straight to where
    // the drawing needs it. They come across below as its dependents.
    chain.push_back(out.found_at);
    {
        command::Transaction inner(scratch, "ic-ice-dis-referans");
        const std::vector<Warning> nested =
            co_await load_externals(inner, out.found_at, stop, host, chain);
        for (const Warning& w : nested)
            out.notes.push_back(w.message);
    }

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
    // Appended, not assigned: the notes of the references read into it on
    // the way down are already here.
    out.notes.insert(out.notes.end(), adopted.value().notes.begin(), adopted.value().notes.end());
    co_return out;
}

command::Task<std::vector<Warning>> load_externals(command::Transaction& tx, std::string project,
                                                   std::stop_token stop, command::Bus* host,
                                                   std::vector<std::string> chain)
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
        auto loaded            = co_await load_external(tx, b, project, stop, host, chain);
        if (!loaded) {
            warnings.push_back(Warning{
                "io.xref", "'" + name + "' dış referansı yüklenemedi: " + loaded.error().message +
                               " Çizim açıldı; referans boş çizilir. Dosyanın yeni yerini "
                               "DIŞREFERANS islem=yol ad=" +
                               name + " dosya=<yol> ile gösterin."});
            continue;
        }
        // WHAT A REFERENCE SAID ON THE WAY DOWN is said here too — a loop two
        // files below, a nested file missing — or it would stop at the file
        // that met it and nobody would hear it.
        for (const std::string& note : loaded.value().notes)
            warnings.push_back(Warning{"io.xref_note", "'" + name + "': " + note});
        if (loaded.value().moved)
            warnings.push_back(Warning{"io.xref_moved", "'" + name +
                                                            "' dış referansı kayıtlı yerinde "
                                                            "yoktu, proje klasöründe bulundu: " +
                                                            loaded.value().found_at + "."});
    }
    co_return warnings;
}

} // namespace kentos::io
