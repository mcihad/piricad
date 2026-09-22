// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — script: what the Python host and its generated surface share.
//
// A PRIVATE HEADER, under `src/` rather than `include/`: nothing outside this
// module includes it, and it names pybind11 types, which `python_runner.hpp`
// deliberately does not (CLAUDE.md Article 9).
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/result.hpp"

#include <pybind11/pybind11.h>

#include <cstddef>
#include <functional>
#include <string>

namespace kentos::script::detail {

/// What a generated callable needs from the running host.
struct Host
{
    command::Bus& bus;

    /// The count `RunReport::commands` reports. A generated callable bumps it
    /// exactly where `cad.run` does, because from the bus's side they are the
    /// same act.
    std::size_t& commands;

    /// Raises a Python exception AND records the bus error's code.
    ///
    /// A `std::function` and not a direct call, because the code that owns the
    /// pending error is the runner's `Impl` and this header must not know its
    /// shape. It never returns: the implementation throws.
    std::function<void(core::ErrorCode, std::string)> fail;
};

/// Adds one callable per registered command to the `cad` module.
///
/// PROJECTED, NEVER WRITTEN. The catalogue is `Registry::all()`, so a command
/// registered today is a Python function today and there is no second list to
/// forget (CLAUDE.md 5.10, 5.20). `ai::build_catalog` does the same projection
/// for the agent surface, from the same registry, and neither is checked in.
void bind_commands(Host& host, pybind11::object& cad);

/// Registers `cad.Point` and `cad.Box`.
///
/// THE CORE TYPES THEMSELVES, bound rather than mirrored: `cad.Point` IS
/// `core::Point2`, so a value that crosses into Python and back is the same
/// fixed-point pair it started as. A parallel Python-side class would be a second
/// definition of a coordinate, and the first rounding between them would be a
/// parcel in the wrong place (CLAUDE.md 2.4).
void bind_types(pybind11::object& cad);

/// Registers `cad.viewport`.
void bind_viewport(Host& host, pybind11::object& cad);

} // namespace kentos::script::detail
