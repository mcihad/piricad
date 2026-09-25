// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: what steps WOULD do, with the drawing untouched (TODOS F-05).
//
// ONE PREVIEW FOR EVERY CLIENT. A suggestion card, a dry-run of a script, the
// command line asking "what would this do" — each wanted the same answer and
// none had it: a card could show the lines it would run and highlight their
// inputs, but not what they would leave. The answer is not a second, simulated
// implementation of every command, which would drift from the real one by the
// second release. It is the real one: the steps run through `Bus::dispatch`,
// inside one batch, and the batch is then aborted — rolled back and cut back to
// the tail it began at (`Document::truncate_to`), which restores the rows, the
// keys and the revision — so the drawing, the undo and redo stacks, the journal
// and the selection come out exactly as they went in, and a suggestion
// composed against the drawing is still composed against it.
//
// NOTHING LEAVES THE MACHINE. A step whose effect reaches past the document —
// it writes a file, prints, writes a database, changes an application or
// session setting, replaces the drawing or moves the view — is NOT run; it is
// listed with the reason, and the steps after it run against the drawing as it
// stands. What the steps would change is counted (`ChangeSummary`) and the
// objects they would leave are handed back as outlines, for the canvas to draw
// dashed.
#pragma once

#include "kentos_cad/command/changes.hpp"
#include "kentos_cad/command/ghost.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {

/// One object as the previewed steps would leave it.
struct PreviewShape
{
    /// Its key when it exists already and would change; `None` for a new one,
    /// whose key is not handed out by a preview (model.md R4a).
    core::EntityKey key{core::EntityKey::None};
    bool created{false};        ///< a new object rather than a changed one
    std::string layer;          ///< the layer it would stand on
    std::vector<GhostRun> runs; ///< its outline, in document millimetres
    std::string text;           ///< the words it would say, when it is a caption
};

/// A step the preview did not run, and why.
struct PreviewSkip
{
    std::size_t step{0};    ///< its position in the steps, from 0
    std::string command_id; ///< what it is
    std::string reason;     ///< why it was not run, a sentence fragment
};

/// What steps would do, found out by running them and taking them back.
struct Preview
{
    std::size_t steps{0};                 ///< how many steps were asked about
    std::size_t ran{0};                   ///< how many were run
    ChangeSummary changes;                ///< what they would change, counted
    std::vector<PreviewShape> shapes;     ///< the new and changed objects, up to the bound
    std::size_t shapes_left_out{0};       ///< the ones past the bound, counted
    std::vector<core::EntityKey> erased;  ///< the existing objects they would erase
    std::vector<PreviewSkip> skipped;     ///< the steps not run, and why
    std::vector<std::string> lines;       ///< what the steps said as they ran
    std::vector<std::string> warnings;    ///< what they could not honour
    std::optional<std::size_t> failed_at; ///< the step that would fail, from 0
    core::Error error{};                  ///< why it would, when one would
};

/// How many objects a preview hands back as outlines, and how many vertices in
/// all: a preview of ten thousand buffers is counted in full and drawn in part.
inline constexpr std::size_t kPreviewShapeLimit  = 2000;
inline constexpr std::size_t kPreviewVertexLimit = 200000;

/// The preview as a person reads it: what would change or where it would stop,
/// and what was not run. One or more lines, no trailing newline.
std::string describe_preview(const Preview& p);

/// The preview as a client reads it: the counts (`changes_json`), the failure,
/// the skipped steps, the erased keys and — when `with_shapes` — the outlines.
core::Json preview_json(const Preview& p, bool with_shapes);

class Context;

/// A command's answer with a preview: `describe_preview` echoed a line at a
/// time and `preview_json` reported — what ÖNİZLE and `BETİK … onizle=evet`
/// both say, from one place.
void answer_preview(Context& ctx, const Preview& p, bool with_shapes);

} // namespace kentos::command
