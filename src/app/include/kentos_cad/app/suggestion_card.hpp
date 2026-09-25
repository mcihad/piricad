// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the card a person reads before anything is drawn, and THE ONE
// PLACE IN THE PROGRAM WHERE A DECISION IS MADE.
//
// WHY THIS FILE IS SPECIAL. `ai::Gate::apply` demands an `ai::Approval`, and an
// `Approval` has no public constructor, no aggregate initialisation and exactly
// one factory: `ai::Gate::approve`. This file is the only caller of that factory
// in the whole program — `scripts/ci-gate-ai.sh` fails the build if a second one
// appears, and names this path as the sanctioned one. Nothing in the MCP server,
// the chat loop, a provider dialect, a script or a command line can reach it.
// That is CLAUDE.md 5.7 and `.claude/ai.md` R3/P1/P15 made structural rather
// than promised: there is no trust mode to add, because there is nowhere to add
// it.
//
// WHAT IT SHOWS. The steps as COMMAND LINES, which is the form an engineer can
// read, check and retype, plus who asked, what model composed it, and which
// handles its coordinates came from — because a coordinate in this program comes
// from a recorded tool-call result and from nowhere else (5.8, R9/R10), and the
// card is where that provenance is put in front of the person who signs.
//
// AND THE BUTTON SAYS `Uygula`. Never "Onayla", never "Onaylandı", never a tick:
// ai.md P4 blocks the words that would make a machine's output look signed, and
// the engineer's seal on a cadastral document is not a thing this program may
// appear to give (§5.2.4).
#pragma once

#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/core/result.hpp"

#include <QString>
#include <QWidget>

#include <cstdint>

namespace kentos::app {

/// The plan store, the gate and the audit log; see ai_service.hpp.
class AiService;
/// The component set's button; see widgets.hpp.
class Button;

/// One pending plan, with the two buttons that answer it.
class SuggestionCard : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the card for `planId`. A plan that is not pending is shown in its
    /// settled state with no buttons — which is what a card looks like after the
    /// transcript is scrolled back to.
    SuggestionCard(AiService& service, const QString& planId, QWidget* parent = nullptr);

    const QString& planId() const noexcept { return plan_; }

    /// Whether this card is still waiting for an answer.
    bool pending() const noexcept { return pending_; }

    /// Presses `Uygula` as a person would. FOR THE PROBE AND THE TESTS ONLY, and
    /// it is deliberately not a way round the rule: it goes through the same
    /// `decide()` this widget's own button does, so what it proves is what a
    /// click would do (`KENTOS_MCP_PROBE`, `tests/unit/test_ai_tools.cpp`).
    core::Status probeApply();

    /// Presses `Reddet` as a person would — the same `decide()` the button
    /// calls. FOR THE PROBE ONLY, like `probeApply`.
    core::Status probeReject();

    /// What the card says applying it would do (TODOS F-05), or empty when it
    /// says nothing — for the probe.
    QString previewTextForProbe() const;

    void applyTheme(ThemeMode mode) override;

protected:
    /// Draws the DASHED outline that says "not yet part of the drawing".
    void paintEvent(QPaintEvent* event) override;

signals:
    /// The plan was applied or rejected. The transcript follows this to write the
    /// note under the bubble.
    void settled(const QString& planId, bool applied);

private:
    /// Mints the approval and hands it to the service. THE ONLY CALLER of
    /// `ai::Gate::approve` in the program.
    core::Status decide(bool apply);

    /// Who the audit record will name: `core.ai.sorumlu` when it is set, and the
    /// operating system's user name when it is not.
    QString operatorName() const;

    /// Replaces the buttons with the outcome, so a settled card cannot be
    /// answered twice.
    void showOutcome(bool applied, const QString& trouble);

    AiService& service_;
    QString plan_;
    QWidget* actions_{nullptr};
    Button* apply_{nullptr};
    Button* reject_{nullptr};
    QLabel* outcome_{nullptr};
    QLabel* preview_{nullptr}; ///< what applying it would do (TODOS F-05), while it waits
    Badge* mark_{nullptr};
    bool pending_{true};

    /// The fingerprint of the plan THIS CARD DREW (`Plan::content_fingerprint`).
    ///
    /// Carried into the approval so the decision is bound to the lines the person
    /// read, not to whatever the plan holds when the button is pressed. A client
    /// may append to its own pending suggestion between the two moments, and a
    /// card showing two lines must never apply three (TODOS S-04).
    std::uint64_t shown_content_{0};
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
