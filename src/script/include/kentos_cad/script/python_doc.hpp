// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — script: the Python surface, written out as documents.
//
// TWO PROJECTIONS OF ONE REGISTRY, and the thing to notice is that the RUN-TIME
// surface (`python_api.cpp`) and these documents are built from the same source
// by different code paths. That is deliberate and it is also the risk: two
// projections can drift. They are kept together by a test that walks the
// registry and by `scripts/ci-gate-docs.sh`, which regenerates and diffs — a
// description of a surface that can be regenerated separately from the surface
// is a description that will eventually be wrong (CLAUDE.md 5.20, 6.14).
//
// COMPILED IN EVERY CONFIGURATION, including one with no Python at all. A
// generated document whose freshness depended on a build option would pass the
// gate on one machine and fail it on another, which is a freshness check that
// has never run.
#pragma once

#include <string>

namespace kentos::command {
/// The command registry; declared rather than included, because this header is
/// about two documents and only their signatures need the type.
class Registry;
} // namespace kentos::command

namespace kentos::script {

/// `docs/python/referans.md` — every command's Python signature, its English
/// keywords and the Turkish help each one carries.
std::string python_reference(const command::Registry& reg);

/// `docs/python/kentos_cad.pyi` — the same surface as a PEP 484 stub, so an
/// editor outside this program can complete a script and a type checker can read
/// it. It declares; it never runs.
std::string python_stub(const command::Registry& reg);

} // namespace kentos::script
