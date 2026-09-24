// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: where a command's input comes from.
//
// kentoscad.md §2.4, the most critical detail in the architecture: a command body
// must NOT be able to tell whether a value came from a mouse click, a typed
// coordinate, the next script argument, or an AI-produced value. The same command
// code runs in all four contexts.
#pragma once

#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/identity.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kentos::command {

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
    Arc,       ///< the arc a centre, a start and the cursor sweep out: YAY
    Ellipse,   ///< the ellipse a centre, a first-axis end (the chain) and the cursor's reach make:
               ///< ELİPS
    Curve,     ///< the smooth curve through the chain and the cursor: SPLINE
    Dimension, ///< the dimension the chain's picks and the cursor's line location make: ÖLÇÜ
    Block,     ///< the block definition `rubber_payload` names, placed at the cursor: BLOKEKLE
    Ghost, ///< the selected objects carried by the cursor's offset from the origin: TAŞI, KOPYALA
    AreaEdit, ///< a face with one edge or corner following the cursor to a wanted area: ALANDÜZENLE
    Polygon,  ///< the regular polygon a centre, the cursor and `rubber_payload`'s side count and
              ///< fit make: ÇOKGEN
    EdgeRectangle, ///< the rectangle the chain's edge and the cursor's depth make: DİKDÖRTGEN
                   ///< yontem=3n
    CircleBuild,   ///< the circle the chain's fixed points and the cursor make, by the
                   ///< construction `rubber_payload` names: DAİRE yontem=2n|3n|ttr
    ArcBuild,      ///< the arc the chain's fixed points and the cursor make, by the construction
                   ///< `rubber_payload` names: YAY yontem=3n|devam|bby
    ArcSweep,      ///< the arc from the chain's start round the origin through the angle the
                   ///< cursor sweeps, with the sweep written on it: YAY yontem=bma
    Fixed,      ///< the reference the run has already fixed, with NOTHING following the cursor: the
                ///< baseline of DİKAYAK, the station of ALIM, the line of ARANOKTA
    Candidates, ///< the answers this pick chooses between, marked; the one nearest the cursor is
                ///< the one it will take: KESİŞİMNOKTA yontem=mesafe
    Angle,  ///< the two arms of an angle and the sweep between them, with the reading written on
            ///< it: AÇIÖLÇ
    Corner, ///< the corner `rubber_payload` names, cut at the cursor's distance from it: PAH,
            ///< YUVARLA
    Grip,   ///< the object with the grip `rubber_payload` names at the cursor — or a new
            ///< corner there: KÖŞETAŞI, KÖŞEEKLE
    MeasureRun,    ///< the run measured so far and its next segment to the cursor, each segment's
                   ///< length on it and the total at the cursor: ÖLÇ
    MeasureRing,   ///< the face the corners so far and the cursor enclose, with its area and
                   ///< perimeter written in it: ALANÖLÇ yontem=nokta
    Parallel,      ///< the parallels of the objects `rubber_payload` names, on the side of each
                   ///< the cursor is on: OFSET
    Stretch,       ///< the objects the window in `rubber_payload` stretches, its grips carried by
                   ///< the cursor's offset from the origin: ESNET
    Break,         ///< the line `rubber_payload` names, with the piece between the origin and the
                   ///< cursor marked for removal: KIR
    Trim,          ///< the object under the cursor, with the piece it would lose (or the reach it
                   ///< would gain) against the edges `rubber_payload` names: BUDA, UZAT
    TrimFence,     ///< the fence `rubber_chain` holds, run on to the cursor, with every piece it
                   ///< would take (or every end it would carry on): BUDA, UZAT yontem=çit
    Split,         ///< the object `rubber_payload` names cut at the chain's points and the
                   ///< cursor, each piece it becomes drawn in turn: BÖL yontem=nokta
    PairCorner,    ///< the corner between the two objects `rubber_payload` names, at the size the
                   ///< cursor's distance from the origin shows: YUVARLA, PAH with two objects
    EdgeArc,       ///< the object `rubber_payload` names with one edge bent through the
                   ///< cursor: KENARTÜRÜ tur=yay
    Region,        ///< the region of the linework the cursor is inside, found as
                   ///< `rubber_payload`'s query finds it, its islands as holes and its area
                   ///< written in it: SINIR
    DimensionNext, ///< the next dimension of a run: from the chain's first point to the
                   ///< cursor, on the line through the chain's second, with
                   ///< `rubber_payload`'s figures and direction: ZİNCİRÖLÇÜ, BAZÖLÇÜ
};

struct Prompt
{
    std::string message;                         ///< Turkish, user-facing
    ParamKind kind{ParamKind::Point};            ///< what kind of value would satisfy it
    std::string param;                           ///< the declared parameter name being filled
    bool has_rubber_band{false};                 ///< whether a preview should be drawn
    Point2 rubber_origin{};                      ///< where that preview starts
    RubberShape rubber_shape{RubberShape::Line}; ///< what it draws between the two

    /// Whether the answer is AIMED FROM `rubber_origin`: dik mod, kutupsal
    /// izleme and the normal lock lay their rays through it, and the dynamic
    /// readout measures from it. True for every guide that draws from a point
    /// already given — a line's next corner, a move's end. False for a preview
    /// that shows what a click will do without measuring from anything: BUDA's
    /// pick of the piece to throw away, the base point of ESNET's move. Aimed
    /// from an origin that is not a base, dik mod bent the pick onto a ray
    /// through a window corner — or through the drawing's zero.
    bool rubber_base{true};

    /// The points this run has already fixed, oldest first, `rubber_origin` last.
    ///
    /// For `RubberShape::Fixed` this is the WHOLE of the preview: a command that
    /// fixes a reference and then asks for NUMBERS — a baseline and then tape
    /// readings off it, a station and then an angle — had nothing on screen while
    /// those numbers were typed, because the reference is not a document object
    /// and the cursor is not answering anything. The user was aiming at a
    /// baseline they could no longer see.
    ///
    /// For `RubberShape::Candidates` it is the answers the pick chooses between.
    ///
    /// A command that writes its geometry only once it is complete — ALAN cannot
    /// add a two-vertex face to the document, because no such face is valid — has
    /// nothing on screen to show the work so far, and `rubber_origin` alone shows
    /// only the newest segment. Every click then appeared to erase the one before
    /// it and the shape arrived all at once on completion. ÇİZGİ writes its run
    /// only when the run ends too, so a wrong corner can be taken back
    /// (`can_retract`), and it hands its run here like the others.
    std::vector<Point2> rubber_chain{};

    /// The words that would answer this prompt, when the set is known and
    /// SMALL: the blocks a drawing has, the patterns a catalogue holds, the
    /// layers, the styles.
    ///
    /// A prompt whose answer is a name is unanswerable by a mouse — and a name
    /// is what BLOK, BLOKEKLE, TARAMA and STİL each ask for first. Pressing
    /// their buttons put a question on the status line that only somebody who
    /// already knew the answer could give, which is what the user meant by "I
    /// could not run them". The command knows the set because it holds the
    /// document; the shell knows how to offer a set. So the command says, and
    /// the shell offers.
    ///
    /// EMPTY IS NOT "NO RESTRICTION". These are suggestions, not a word list:
    /// the validator still decides what is acceptable, because a new block's
    /// name is by definition not among the ones a drawing already has.
    std::vector<std::string> choices{};

    /// The kind payload of the thing about to be made, for a preview that needs
    /// more than points: the block reference BLOKEKLE will place (its block,
    /// scale and turn), the dimension ÖLÇÜ will lay out (its type and figures),
    /// the spline's degree. Decoded by the canvas with the kind's own decoder
    /// and drawn by the kind's own outline, so the preview is the future
    /// drawing. Empty for every other shape.
    std::vector<std::uint8_t> rubber_payload{};

    /// AN ANGLE THE MOUSE CAN GIVE. Set on the prompt for a swept angle — YAY
    /// `bma`'s — that a click answers with the sweep from `rubber_chain`'s start
    /// round `rubber_origin` to the click, in the session's unit and positive
    /// sense (`core::arc_sweep_toward`), the way every CAD lets an included
    /// angle be typed OR shown. The body is handed the number either way.
    bool pick_sweep{false};

    /// A NUMBER THE MOUSE CAN GIVE. Set on a prompt for a distance in metres —
    /// a chamfer, a fillet radius, an offset — that a click answers with its
    /// distance from `rubber_origin`, the way every CAD program lets a distance
    /// be typed OR shown. Without it a click at a number prompt was an error
    /// ("sayı bekliyor"), so the canvas could preview the result at the cursor
    /// and then refuse the click that pointed at it. The body is handed the
    /// number either way and cannot tell which it was (Article 1.2).
    bool pick_distance{false};

    /// THE NEWEST POINT MAY BE TAKEN BACK. Set on the prompt for the next corner
    /// of a run — ÇİZGİ, ÇOKLUÇİZGİ, ALAN, SPLINE — where a wrong click must not
    /// cost the whole drawing: ⌫, Ctrl+Z, or `U`, `G`, `geri` typed at the prompt
    /// retract that one point and ask again (`Session::retract`, TODOS C-02).
    /// `rubber_chain` then holds the run's corners, which the snap offers as the
    /// run's own endpoints (`command::pending_run`).
    bool can_retract{false};

    /// WHAT THE OBJECTS ARE, when a command acts on one kind only. A hatch lies
    /// on the edges of the parcel it fills and a dimension on the side it
    /// measures, so a click meant for either lands on two things at once — and
    /// the one nearer the bottom of the drawing, the parcel, won the tie. The
    /// canvas takes the one of this kind among what is under the cursor without
    /// asking which; `SEÇ mod=NOKTA sira=` is the line it sends, so a keyboard
    /// reaches the same object (CLAUDE.md 5.15). `kNoKind`: any.
    core::KindId pick_kind{core::kNoKind};
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

/// Live interaction: a value the client already knows is handed over, anything
/// else suspends the command so the UI can supply it. Used by the GUI client only.
///
/// THE PRESET IS WHAT MAKES A BUTTON AN EQUAL CLIENT (Article 1.2). A toolbar
/// button is not always the bare command: "Alan Seç" is `SEÇ mod=KUTU`, and the
/// mode is known before the user has clicked anything while the two corners are
/// not. Without this the GUI could start a command OR pass it arguments, never
/// both, so any tool whose command takes a keyword was reachable only by typing —
/// which is the mouse-only prohibition of CLAUDE.md 5.15 standing on its head.
///
/// The difference from `ArgInputSource` is the whole point: running out of preset
/// values means ASK THE USER, not stop. A script's queue ending is the end of the
/// command; a button's preset ending is where the clicking starts.
class InteractiveInputSource final : public InputSource
{
public:
    /// Nothing answered in advance: every parameter is asked for.
    InteractiveInputSource() = default;

    /// Starts with `args` already answered; every other parameter is asked for.
    /// `origin` is what the journal records: a button is `Gui`, a typed line that
    /// then prompts is `CommandLine`. Neither buys the source any privilege
    /// (Article 1.2).
    explicit InteractiveInputSource(Args args, Origin origin = Origin::Gui)
        : preset_(std::move(args)), origin_(origin)
    {}

    /// Where the run came from, as recorded in the journal.
    Origin origin() const override { return origin_; }

    std::optional<Value> take(const Param& param) override;

    /// An interactive source runs out only when the user cancels — ESC — because
    /// there is always another click available until then.
    bool exhausted() const override { return cancelled_; }

    const Args* preset() const override { return preset_.size() == 0 ? nullptr : &preset_; }

    void cancel() { cancelled_ = true; }

private:
    /// How many values of a given parameter the preset has already handed out;
    /// the same cursor `ArgInputSource` keeps, for the same reason.
    std::vector<std::pair<std::string, std::size_t>> cursor_;

    Args preset_;
    bool cancelled_{false};
    Origin origin_{Origin::Gui};
};

} // namespace kentos::command
