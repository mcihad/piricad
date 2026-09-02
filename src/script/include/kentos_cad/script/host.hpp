// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — script: what every script host shares.
//
// A host is a LANGUAGE, not an architecture. The JSON runner, the Lua runner and
// the Python module of §4.2 differ in how a script is written and in nothing
// else: all three dispatch through the same `Bus`, parse command text with the
// same `Parser`, run inside one batch and report the same way. What is common
// lives here so that the third one cannot quietly answer differently from the
// first two (CLAUDE.md 5.10 makes this objection about lists; it is the same
// objection about report shapes).
#pragma once

#include "kentos_cad/script/sandbox.hpp"

#include <cstddef>
#include <string>

namespace kentos::script {

/// What a finished run has to say for itself.
struct RunReport
{
    std::size_t commands{0}; ///< how many commands ran
    std::size_t ops{0};      ///< primitive edits across all of them
    std::string label;       ///< the script's own name, for the undo entry
};

} // namespace kentos::script
