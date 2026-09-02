// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: where the shipped data packages are.
//
// THE CATALOGUES SHIP WITH THE PROGRAM. MPYY's gösterimler, the TM3 dilim table
// and every other package under `/data` are what make this a Turkish planning
// program rather than a drawing editor, and a build that installs the binary
// alone is a build that starts with an empty shelf on every machine but the one
// it was compiled on (CLAUDE.md 3.5, §12).
//
// The default of `core.stil.kutuphane` is a RELATIVE path — `data/catalogs/...` —
// and a relative path is resolved against the working directory, which is the one
// place it is guaranteed not to be. This header is where that gets an answer.
//
// WHY NOT A COMPILE-TIME PATH. A package built on one machine is installed on
// another, moved, or run from a portable directory; a path baked in at build time
// is right exactly once. The search below asks the running program where it is.
#pragma once

#include <string>

namespace kentos::app {

/// The directory the shipped `/data` tree lives in, or empty when none was found.
///
/// Searched in this order, first hit wins:
///
///   1. `$KENTOS_DATA` — an explicit answer always beats a guess, and it is what
///      a packager, a test rig and a developer running two trees all need.
///   2. `<exe>/../share/piricad/data` — the installed layout.
///   3. `<exe>/data` — a portable directory, the binary and its data side by side.
///   4. The source tree it was configured from, so a developer build works with
///      no environment set at all.
///
/// A directory is only accepted when it actually holds `catalogs`: an empty or
/// half-installed tree is a miss, not a hit, because reporting success and then
/// finding nothing is how a fresh machine ends up with a silently empty shelf.
std::string data_root();

/// `relative` resolved against `data_root()`, or `relative` unchanged when it is
/// already absolute or no data tree was found.
///
/// Unchanged rather than empty on a miss: the caller then reports a path that does
/// not exist, which says what is wrong, instead of reporting nothing at all.
std::string data_path(const std::string& relative);

} // namespace kentos::app
