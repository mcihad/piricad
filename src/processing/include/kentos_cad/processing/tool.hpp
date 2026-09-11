// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — processing: what an ANALYSIS TOOL is.
//
// A processing tool is a piece of work applied to many objects at once — label
// every line with its length, number every corner of a parcel, buffer, simplify,
// dissolve. QGIS calls the family "Processing"; here it is the same idea inside
// the constitution: a tool IS a command (Article 1), its parameters are declared
// once and validated by the bus, and it runs from the panel, the command line, a
// script and the AI by the same road. What a tool adds to a plain command is
// SHAPE: it says which geometry classes it applies to, it takes its objects
// from a scope (the selection, the viewport, the whole project), it runs its
// work on a worker thread with a progress figure and a stop, and it writes its
// result as new objects on a layer of the user's choosing.
//
// The interface is deliberately narrow. A tool never sees the `Document`: it is
// handed a SNAPSHOT of the objects in scope (`ToolInput`), computes into a
// Qt-free, document-free `ToolOutput`, and the runner (`run.cpp`) writes that
// output through the command's one transaction on the bus thread. That split is
// what makes the worker thread safe — nothing it touches is shared — and what
// makes a tool testable without a bus at all.
#pragma once

#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {
class Document; ///< read by `classify` and the runner, never by a tool
} // namespace kentos::core

namespace kentos::command {
class Context; ///< the interactive phase's way to ask for points
} // namespace kentos::command

namespace kentos::processing {

/// The geometry classes a tool can be applied to, as a bit set. A class is
/// what the user SEES — a line, a face, a point — not a kind id: an open
/// polyline and an arc polyline are both lines to a labelling tool.
enum class Applies : std::uint8_t {
    None   = 0,
    Points = 1U << 0, ///< `core.point`
    Lines  = 1U << 1, ///< an open polyline
    Faces  = 1U << 2, ///< a closed polyline (an exterior ring) or a hatch
    Curves = 1U << 3, ///< circle, arc, ellipse, spline, arc polyline
    Texts  = 1U << 4, ///< a caption-carrying object
};

/// Bitwise OR of two class sets.
constexpr Applies operator|(Applies a, Applies b) noexcept
{
    return static_cast<Applies>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/// Whether `set` contains `one`.
constexpr bool applies_to(Applies set, Applies one) noexcept
{
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(one)) != 0;
}

/// The Turkish name of one class, for a chip on the panel and a message.
const char* applies_name(Applies one) noexcept;

/// One parameter a tool declares. It becomes a `command::Param` on the
/// generated command (validated by the bus) and a field on the panel; the
/// `choices` and the range are what the tool validates itself, because the
/// bus knows only the value's kind.
struct ToolParam
{
    std::string name;                                  ///< ASCII, the CLI keyword
    command::ParamKind kind{command::ParamKind::Text}; ///< what shape of value
    std::string help;                                  ///< one Turkish line
    std::string fallback;             ///< the value when not given, as text; empty = none
    std::vector<std::string> choices; ///< for a Text parameter: the only words accepted
    std::int64_t low{0};              ///< for an Integer/Number: the range, when `bounded`
    std::int64_t high{0};             ///< upper bound of that range
    bool bounded{false};              ///< whether `low..high` is enforced

    /// A free-text parameter.
    static ToolParam text(std::string name, std::string help, std::string fallback = {});
    /// One of a fixed set of words.
    static ToolParam choice(std::string name, std::string help, std::vector<std::string> choices,
                            std::string fallback);
    /// A whole number, optionally bounded.
    static ToolParam integer(std::string name, std::string help, std::int64_t fallback,
                             std::int64_t low = 0, std::int64_t high = 0);
    /// A whole number the user may leave out: no default, so a tool can tell
    /// "not given" from zero (`Args::has`).
    static ToolParam integer_optional(std::string name, std::string help, std::int64_t low = 0,
                                      std::int64_t high = 0);
    /// A real number; an empty `fallback` means the user must give it.
    static ToolParam number(std::string name, std::string help, std::string fallback = {});
    /// A coordinate.
    static ToolParam point(std::string name, std::string help);
    /// A yes/no switch.
    static ToolParam boolean(std::string name, std::string help, bool fallback);
};

/// What a tool produces.
enum class OutputShape : std::uint8_t {
    NewEntities, ///< new objects on the output layer; the inputs are untouched
    InPlace,     ///< the inputs' own geometry replaced, identity kept (`ToolOutput::replacements`)
    Report,      ///< text on the transcript only; the drawing does not change
};

/// Everything the panel, the docs and the generated command need to know about
/// a tool: its identity, where it sits in the tree, what it applies to, its
/// parameters and its output.
struct ToolSpec
{
    std::string id; ///< stable, namespaced: `islem.uzunluk_yaz`
    std::vector<std::string>
        names;           ///< Turkish, ASCII-folded, English, abbreviation — the command's names
    std::string title;   ///< the tree row: `Çizgi uzunluğu yaz`
    std::string summary; ///< one Turkish sentence
    std::string group;   ///< the tree branch: `Etiketleme`
    /// The mark the tree row wears, by NAME, so the Qt-free module never
    /// names a glyph: `cetvel` (a ruler), `koordinat`, `yazi`, `alan`, `cizgi`,
    /// `nokta`, `sigma`, `islev`. The panel maps a name it does not know to
    /// the generic tool mark.
    std::string icon;
    Applies applies{
        Applies::None}; ///< the classes it takes; others in scope are skipped and counted
    std::vector<ToolParam> params; ///< the tool's own parameters, after the four every tool shares
    OutputShape output{OutputShape::NewEntities}; ///< what the tool produces
    std::string output_suffix; ///< the default output layer name is `<source layer>_<suffix>`;
                               ///< empty = the active layer

    /// The command this tool is reachable as. The four SHARED parameters come
    /// first: `nesneler` (ids), `kapsam` (secili / gorunum / proje), `pencere`
    /// (two corners, for gorunum) and `katman` (the output layer), then the
    /// tool's own. `run` is the one generic body in run.cpp.
    command::CommandSpec to_command_spec() const;
};

/// One object of the input, copied out of the document on the bus thread so
/// the worker reads nothing shared.
struct InputEntity
{
    /// One ring of it: the points, without a repeated closing vertex, and the role.
    struct Ring
    {
        std::vector<core::Point2> points;          ///< the vertices, no repeated closing point
        core::RingRole role{core::RingRole::Open}; ///< open, exterior or interior
    };

    core::EntityId slot{core::kNoEntity}; ///< the entity's slot at snapshot time
    std::int64_t key{0};                  ///< its persistent key, what the journal records
    core::KindId kind{0};                 ///< its kind
    Applies cls{Applies::None};           ///< the class it was admitted as
    core::LayerId layer{0};               ///< its layer
    std::string layer_name;               ///< that layer's name, for the default output layer
    std::vector<Ring> rings;              ///< its rings, in R11 order
    std::string text;                     ///< its caption, or empty
    core::Mm text_height{0};              ///< the caption's height
};

/// What a tool is handed.
struct ToolInput
{
    std::vector<InputEntity> entities; ///< the objects in scope that the tool applies to
    command::Args args;                ///< every tool parameter, defaults applied and validated
    core::DrawingUnit unit{core::DrawingUnit::Metre}; ///< the project's drawing unit
    std::int64_t plan_scale{1000}; ///< the plan scale's denominator, for paper sizes
};

/// What a tool produces. The runner turns it into objects on the output layer.
struct ToolOutput
{
    /// A caption: text centred at `centre`, reading along `(dir_x, dir_y)`.
    struct Caption
    {
        core::Point2 centre{}; ///< where the text is centred
        double dir_x{1.0};     ///< the reading direction, unit length: x
        double dir_y{0.0};     ///< and y
        std::string text;      ///< what it says
        core::Mm height{2500}; ///< ground millimetres
    };

    /// A run of points, open or closed.
    struct Polyline
    {
        std::vector<core::Point2> points; ///< the vertices, no repeated closing point
        bool closed{false};               ///< a face (exterior ring) rather than a line
    };

    /// One input object's new geometry, for `OutputShape::InPlace`: the same
    /// object (its key) with these rings instead of its own.
    struct Replacement
    {
        std::int64_t key{0};                  ///< which object
        std::vector<InputEntity::Ring> rings; ///< its new rings, R11 order
    };

    std::vector<Caption> captions;         ///< text objects to create
    std::vector<Polyline> polylines;       ///< line and face objects to create
    std::vector<Replacement> replacements; ///< geometry to put in place of an input's
    std::vector<std::string> notes;        ///< what the tool wants said on the transcript
    std::size_t touched{0};                ///< how many input objects produced something
};

/// The worker's view of the run: the stop it must honour and the figure it
/// reports. `at(done, total)` is cheap enough to call per object.
struct Progress
{
    std::stop_token stop;                          ///< requested by Durdur or Esc
    std::atomic<std::uint32_t>* permille{nullptr}; ///< 0..1000, read by the status strip

    /// Whether the user asked for the run to stop. A tool checks this between
    /// objects and returns `Cancelled` when it is set.
    bool cancelled() const noexcept { return stop.stop_requested(); }

    /// Reports `done` of `total` objects handled.
    void at(std::size_t done, std::size_t total) const noexcept;
};

/// The interface every tool implements. Stateless: one instance serves every
/// run, on any thread.
class ProcessingTool
{
public:
    ProcessingTool()          = default;
    virtual ~ProcessingTool() = default;

    ProcessingTool(const ProcessingTool&)            = delete;
    ProcessingTool& operator=(const ProcessingTool&) = delete;

    /// What the tool is.
    virtual const ToolSpec& spec() const noexcept = 0;

    /// The work. Reads `input`, writes `output`, honours `progress.stop`. Runs on
    /// a worker thread when the session can host one; must touch nothing but its
    /// arguments. Returns `Cancelled` when stopped, an error for a parameter the
    /// bus could not judge (a bad choice, a range), `ok()` otherwise.
    virtual core::Status run(const ToolInput& input, ToolOutput& output,
                             const Progress& progress) const = 0;

    /// The INTERACTIVE phase, when a tool has one: runs on the bus thread after
    /// the scope is known and before the worker, and may ask the user for
    /// points (`ctx.point`) and write what it learns into `input.args`. It
    /// MUST NOT touch the document or echo; the runner does both. A tool with
    /// nothing to ask leaves the default, which asks nothing. Where the hand
    /// went is recorded with the other arguments, so a replay needs no hand.
    virtual command::Task<core::Status> interact(command::Context& ctx, ToolInput& input) const;
};

/// Classifies one live entity of the document into the class a tool sees, or
/// `None` for a kind no tool takes (a dimension, a leader, a block reference).
Applies classify(const core::Document& doc, core::EntityId e);

/// The error a tool returns when it was stopped.
core::Error cancelled();

} // namespace kentos::processing
