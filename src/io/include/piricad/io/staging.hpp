// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: a file written beside its target, put in place only when
// whole (TODOS F-05).
//
// AN EXPORT THAT FAILS HALF WAY MUST LEAVE WHAT WAS THERE. GDAL's writer and
// libdxfrw write where they are pointed, and an export pointed at its target
// left a half-written file behind a cancel or a full disk — or, worse, deleted
// the good file that stood there first, so the user had neither. Every file
// this program writes for a user is written into a STAGING DIRECTORY beside its
// target — the same directory, so moving it into place is a rename and never a
// copy — under the target's own name, and moved there only after it closed
// without error. Anything else removes the staging directory and leaves the
// target exactly as it was.
//
// THE TARGET'S OWN NAME, NOT A DISGUISED ONE. A GML names its schema file, a
// GeoPackage its table after the file, a world file its picture by sharing a
// stem: a file written under a staging NAME would carry that name inside it
// after the move. Written as `<dir>/.kentos-<n>/<name>`, every file of the set
// already has the name it will have.
//
// A SET OF FILES MOVES TOGETHER. A DXF and its `.prj`, a picture and its world
// file, the pages of a multi-page sheet: each is written into the one staging
// directory and each moves beside the target. The moves are renames, one after
// the other, so the window in which a set can be half moved is the time of a few
// renames — and a failure inside it is reported file by file, what was moved
// and what was not, rather than called a success.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace kentos::io {

/// What moving a staged set into place did, file by file.
struct Placed
{
    std::vector<std::string> written; ///< the target paths now holding the new files
    std::vector<std::string> failed;  ///< the target paths that could not be replaced, with why
};

/// One file, or one set of files, being written for `target`.
///
/// Not copyable: two guards over one staging directory would remove it twice.
class Staging
{
public:
    /// Makes a staging directory beside `target` (`.kentos-<n>`, a name no
    /// user file has). When the directory cannot be made the writer's own open
    /// fails on `path()` and says so, and `commit` refuses.
    explicit Staging(std::filesystem::path target);

    /// Removes the staging directory and everything in it that was not moved.
    ~Staging();

    Staging(const Staging&)            = delete;
    Staging& operator=(const Staging&) = delete;
    Staging(Staging&&)                 = delete;
    Staging& operator=(Staging&&)      = delete;

    /// Where the writer writes the target: the staging directory, the target's name.
    [[nodiscard]] std::filesystem::path path() const;

    /// Where the set's other files are written, each under the name it will have.
    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return dir_; }

    /// Where the file ends up.
    [[nodiscard]] const std::filesystem::path& target() const noexcept { return target_; }

    /// Moves every file of the staging directory beside the target, the target
    /// itself last, each over the file of the same name. A failure is reported in
    /// `Placed::failed` for that file and the others still move: a set half in
    /// place is said to be, never hidden. An error when nothing was staged.
    [[nodiscard]] core::Result<Placed> commit();

private:
    std::filesystem::path target_;
    std::filesystem::path dir_;
};

/// `Staging::commit` as one answer: nothing when every file of the set is in
/// place; otherwise an error that names what moved and what did not — the
/// report an external effect owes when it could not be atomic (TODOS F-05).
[[nodiscard]] core::Status place_staged(Staging& staged);

/// Whether `name` is a staging directory's name — what a directory a user
/// lists should never be left holding.
[[nodiscard]] bool is_staging_name(const std::string& name) noexcept;

} // namespace kentos::io
