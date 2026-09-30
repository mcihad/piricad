// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the About window.
//
// What the program is, which build this is and whose work it is built on: the
// name and its mark, the facts a bug report needs (version, Qt, the canvas's
// backend, the platform, how many commands the registry holds), the
// third-party components exactly as `/NOTICE` records them, and the licence
// text. One button copies the facts for a report; the other closes.
//
// THE COMPONENTS ARE READ FROM NOTICE, embedded at build time, and never from a
// list in C++: NOTICE is the one record of what is linked (CLAUDE.md 5.12), and
// a second copy here would be a second answer to that question.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QString>

class QStackedWidget;

namespace kentos::app {

/// The component set's segmented switch the three pages hang from (`widgets.hpp`).
class Segment;

/// The facts the window prints about this build, gathered by the shell.
struct AboutFacts
{
    QString version;  ///< the program's own, `KENTOS_VERSION`
    QString qt;       ///< the Qt the program is running on
    QString backend;  ///< the canvas's drawing backend, as the status strip names it
    QString platform; ///< the operating system and the processor architecture
    int commands{0};  ///< how many commands the registry holds
    int tools{0};     ///< how many of them are processing tools
    QString dataRoot; ///< where the catalogues were read from
};

/// The About window.
class AboutDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Builds the window over `facts`.
    AboutDialog(const AboutFacts& facts, QWidget* parent);

    void applyTheme(ThemeMode mode) override;

    /// The facts as the lines the copy button puts on the clipboard.
    QString factsText() const;

private:
    AboutFacts facts_;
    Segment* pages_{nullptr};
    QStackedWidget* stack_{nullptr};
    QWidget* mark_{nullptr};
};

} // namespace kentos::app
