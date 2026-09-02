// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: where a command's input comes from.
//
// piricad.md §2.4, the most critical detail in the architecture: a command body
// must NOT be able to tell whether a value came from a mouse click, a typed
// coordinate, the next script argument, or an AI-produced value. The same command
// code runs in all four contexts.
#pragma once

#include "piricad/command/spec.hpp"
#include "piricad/command/value.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace piricad::command {

/// Which client started a command.
///
/// RECORDED, never branched on. Article 1.2 makes every client equal, so a
/// command body that asked this question would be the privilege the architecture
/// exists to prevent; the journal keeps it because an audit record should say who
/// did something, not because the code behaves differently.
enum class Origin : std::uint8_t {
    Gui, ///< toolbar button / menu — no privileges over any other client
    CommandLine,
    Script,
    Ai,
    Batch,
    Test,
};

/// Stable machine name, for the journal and for tests.
const char* origin_name(Origin o);

/// A request the running command has made and is suspended on.
/// What the preview between the last point and the cursor looks like.
///
/// A guide is not decoration: it is the only thing that tells a user WHAT the
/// next click will make before they make it. A rectangle previewed as a single
/// line says nothing about the shape being drawn, and the user finds out what
/// they built after they have built it.
enum class RubberShape : std::uint8_t {
    Line,      ///< the segment about to be drawn: ÇİZGİ
    Rectangle, ///< the face two opposite corners enclose: DİKDÖRTGEN
    Ring,      ///< the closed face the points so far would enclose: ALAN
    Circle,    ///< the circle a centre and a rim point make: DAİRE
    Arc        ///< the arc a centre, a start and the cursor sweep out: YAY
};

struct Prompt
{
    std::string message;                         ///< Turkish, user-facing
    ParamKind kind{ParamKind::Point};            ///< what kind of value would satisfy it
    std::string param;                           ///< the declared parameter name being filled
    bool has_rubber_band{false};                 ///< whether a preview should be drawn
    Point2 rubber_origin{};                      ///< where that preview starts
    RubberShape rubber_shape{RubberShape::Line}; ///< what it draws between the two

    /// The points this run has already fixed, oldest first, `rubber_origin` last.
    ///
    /// A command that writes its geometry only once it is complete — ALAN cannot
    /// add a two-vertex face to the document, because no such face is valid — has
    /// nothing on screen to show the work so far, and `rubber_origin` alone shows
    /// only the newest segment. Every click then appeared to erase the one before
    /// it and the shape arrived all at once on completion. A command that commits
    /// as it goes (ÇİZGİ) leaves this empty: its segments are already in the
    /// document, and drawing them twice is what a preview must not do.
    std::vector<Point2> rubber_chain{};
};

/// Supplies values to a running command. Implementations: queued arguments
/// (script / CLI / AI / batch) and live user interaction (GUI).
class InputSource
{
public:
    /// Virtual: a source is owned polymorphically by the session.
    virtual ~InputSource() = default;

    /// Which client this source speaks for. Recorded in the journal; never used to
    /// decide behaviour.
    virtual Origin origin() const = 0;

    /// Returns the next value for `param` if one is already available.
    /// std::nullopt means "ask the user" — the command suspends.
    virtual std::optional<Value> take(const Param& param) = 0;

    /// True once the source is exhausted; a command loop uses this to terminate
    /// exactly where an interactive user would press ESC.
    virtual bool exhausted() const = 0;

    /// Arguments supplied up front, when the client had them. Never reveals which
    /// client that was — a command body may read values, never their provenance.
    virtual const Args* preset() const { return nullptr; }
};

/// Pre-supplied arguments: the script, CLI, AI and batch clients all use this.
class ArgInputSource final : public InputSource
{
public:
    ArgInputSource(Args args, Origin o) : args_(std::move(args)), origin_(o) {}

    Origin origin() const override { return origin_; }

    std::optional<Value> take(const Param& param) override;

    bool exhausted() const override { return exhausted_; }

    const Args* preset() const override { return &args_; }

    const Args& args() const noexcept { return args_; }

private:
    /// How many values of a given parameter have already been handed out. A
    /// point-list argument is drained one point per co_await, so the identical
    /// command loop terminates where an interactive user would press ESC.
    std::vector<std::pair<std::string, std::size_t>> cursor_;

    Args args_;
    Origin origin_;
    bool exhausted_{false};
};

/// Live interaction: every take() returns nullopt, so the command suspends and the
/// UI resumes it by supplying a value. Used by the GUI client only.
class InteractiveInputSource final : public InputSource
{
public:
    /// A live user at a mouse and keyboard. Recorded in the journal; it buys this
    /// source no privilege (Article 1.2).
    Origin origin() const override { return Origin::Gui; }

    std::optional<Value> take(const Param&) override { return std::nullopt; }

    /// An interactive source runs out only when the user cancels — ESC — because
    /// there is always another click available until then.
    bool exhausted() const override { return cancelled_; }

    void cancel() { cancelled_ = true; }

private:
    bool cancelled_{false};
};

} // namespace piricad::command
