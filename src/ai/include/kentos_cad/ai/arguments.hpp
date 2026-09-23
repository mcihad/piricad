// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: a tool call's JSON arguments, compiled into a command's `Args`.
//
// ONE COMPILER FOR EVERY AGENT ROAD. The MCP server and the in-app chat used to
// compile arguments two ways: the server resolved a handle into the points it
// names, the chat wrote the handle's TEXT into the argument — where a command
// expecting points found a word. So the chat could draw nothing and could not
// act on a selection, and the server could draw only at points the drawing
// already had, because nothing minted a point handle at all. Both roads now call
// `compile_arguments`, and what one accepts the other accepts.
//
// WHERE A POSITION COMES FROM (CLAUDE.md 5.8). A point argument is one of:
//
//   "@0123456789abcdef.2"                        a handle a read tool minted
//   {"taban": "@…", "dogu": 10000, "kuzey": 0}   that point moved by a DIMENSION
//
// The second is the CAD user's `@10,0`: a base the drawing supplied and an offset
// in millimetres, east and north. An offset is a dimension like OFSET's distance
// or a circle's radius — the model has always stated those — and it is recorded in
// the audit record beside the handle it was measured from, so the question "why
// is this point here" still has an answer that is not "the model said so". A
// bare number where a position belongs is still refused, before any `Args`
// exists (ai.md R10).
#pragma once

#include "kentos_cad/ai/handles.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::ai {

/// What compiling one call's arguments produced.
struct CompiledArguments
{
    command::Args args;                     ///< what the command takes
    std::vector<std::string> handles;       ///< every handle the arguments named
    std::vector<std::string> constructions; ///< `@….0 + doğu 10000, kuzey 0 mm`, for the audit
    std::string refusal;                    ///< non-empty when it could not be compiled
    bool coordinate_literal{false};         ///< a number arrived where a position belongs
    bool protocol_fault{false};             ///< the refusal is the client's schema mistake
};

/// The largest offset a relative point may carry, in millimetres, east or north:
/// a thousand kilometres, which is past the edge of any Turkish projection zone.
/// An offset beyond it is refused rather than added, because a sum that leaves
/// int64 is not a coordinate and one that leaves the country is not a drawing.
inline constexpr std::int64_t kMaxRelativeOffset = 1'000'000'000'000;

/// Compiles `arguments` (a JSON object) for `spec`, resolving every handle
/// against `handles` at the document's current `revision`. Refuses, with a
/// sentence naming the parameter, an undeclared name, a value of the wrong
/// kind, a coordinate written as a number, a handle that is unknown or stale.
CompiledArguments compile_arguments(const command::CommandSpec& spec, const core::Json& arguments,
                                    const HandleStore& handles, std::uint64_t revision);

/// The command line a person reads on the suggestion card — and could type.
///
/// THE SAME WORDS THEY WOULD TYPE, because the GUI, the command line, a script
/// and the agent roads are equal clients (Article 1.2) and a suggestion the
/// engineer cannot read is a suggestion they cannot be responsible for.
std::string render_line(const command::CommandSpec& spec, const command::Args& args);

} // namespace kentos::ai
