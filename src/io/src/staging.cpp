// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/staging.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <system_error>

namespace kentos::io {
namespace {

namespace fs = std::filesystem;

/// What every staging directory's name begins with.
constexpr const char* kStagingPrefix = ".kentos-";

/// The directory `target` is in, or the working directory for a bare name.
fs::path directory_of(const fs::path& target)
{
    return target.has_parent_path() ? target.parent_path() : fs::path(".");
}

/// A number no other staging of this process has used, and that another
/// process is unlikely to be using in the same directory at the same moment.
std::string unique_part()
{
    static std::atomic<std::uint64_t> counter{0};
    const auto ticks =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::to_string(ticks % 1000000007ULL) + "-" + std::to_string(++counter);
}

/// Why a move was refused, in the words of the person at the desk rather than
/// the C library's English. A reason not named here is passed on as the system
/// said it — never dropped.
std::string why_not(const std::error_code& ec)
{
    if (ec == std::errc::is_a_directory || ec == std::errc::directory_not_empty)
        return "yerinde aynı adlı bir klasör var";
    if (ec == std::errc::device_or_resource_busy || ec == std::errc::text_file_busy)
        return "dosya başka bir programda açık";
    if (ec == std::errc::permission_denied || ec == std::errc::operation_not_permitted)
        return "izin yok ya da dosya başka bir programda açık";
    if (ec == std::errc::no_space_on_device) return "disk dolu";
    if (ec == std::errc::read_only_file_system) return "disk salt okunur";
    return ec.message();
}

} // namespace

Staging::Staging(std::filesystem::path target) : target_(std::move(target))
{
    dir_ = directory_of(target_) / (std::string(kStagingPrefix) + unique_part());
    std::error_code ec;
    fs::create_directory(dir_, ec);
}

Staging::~Staging()
{
    // WHAT WAS NOT MOVED INTO PLACE GOES: a cancelled export, a failed one, or
    // a file of the set whose rename was refused. The target is left as it was.
    std::error_code ec;
    fs::remove_all(dir_, ec);
}

std::filesystem::path Staging::path() const
{
    return dir_ / target_.filename();
}

core::Result<Placed> Staging::commit()
{
    std::vector<fs::path> staged;
    std::error_code ec;
    for (fs::directory_iterator it(dir_, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) staged.push_back(it->path());
    if (staged.empty())
        return core::err(core::ErrorCode::IoFailure,
                         "'" + target_.string() + "' için hiçbir dosya hazırlanmadı.");

    // BY NAME, THE TARGET LAST: the report reads the same on every filesystem
    // (a directory lists in whatever order it likes), and the file the user
    // named is replaced only after its companions are — a program that opens it
    // the moment it changes finds its `.prj` or world file already new.
    const fs::path main = path();
    std::ranges::sort(staged, [&](const fs::path& a, const fs::path& b) {
        const bool a_main = a == main;
        const bool b_main = b == main;
        if (a_main != b_main) return b_main;
        return a.filename().string() < b.filename().string();
    });

    Placed out;
    const fs::path beside = directory_of(target_);
    for (const fs::path& one : staged) {
        const fs::path into = beside / one.filename();
        std::error_code moved;
        fs::rename(one, into, moved);
        if (moved)
            out.failed.push_back(into.string() + " (" + why_not(moved) + ")");
        else
            out.written.push_back(into.string());
    }
    return out;
}

core::Status place_staged(Staging& staged)
{
    auto placed = staged.commit();
    if (!placed) return placed.error();
    if (placed.value().failed.empty()) return core::ok();
    std::string written;
    for (const std::string& one : placed.value().written)
        written += (written.empty() ? "" : ", ") + one;
    std::string failed;
    for (const std::string& one : placed.value().failed)
        failed += (failed.empty() ? "" : ", ") + one;
    return core::err(core::ErrorCode::IoFailure,
                     "'" + staged.target().string() + "' yerine tam konamadı. Yerine konan: " +
                         (written.empty() ? std::string("hiçbiri") : written) +
                         ". Konamayan: " + failed +
                         ". Hedefteki dosya takımı eski ve yeni dosyaların karışımı olabilir; "
                         "dosyaları kullanan programı kapatıp dışa aktarmayı yineleyin.");
}

bool is_staging_name(const std::string& name) noexcept
{
    return name.starts_with(kStagingPrefix);
}

} // namespace kentos::io
