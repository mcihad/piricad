// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: is this process a probe?
//
// ONE LIST. Two lists of probe variables lived in two files — `main.cpp` (which moves
// the profile out of the way) and `main_window.cpp` (which keeps the shell from writing
// preferences) — and drifted: a run named by one and not the other left the person's own
// profile written to. A probe runs the REAL shell, and the shell saves its window
// geometry, its dock layout, its preferences and what it remembers about the person on
// the way out; a test that changes the machine it ran on is not a test.
#pragma once

namespace piricad::app {

/// Whether any of the developer-tooling environment variables that drive a probe, a
/// picture or a measurement is set. Such a run never touches the person's own profile.
bool probe_environment();

} // namespace piricad::app
