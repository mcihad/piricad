// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: which commands a person reaches for, kept per machine.
//
// What the command search (`Ctrl+K`) opens on before a word is typed (TODOS U-01):
// the commands starred, then the commands run last. The lists and the rules that
// maintain them are the command layer's (`command::note_use`, `toggle_member`,
// `worth_remembering`, tested without Qt); this is only the part that remembers
// them between sessions.
//
// USER STATE, NOT DOCUMENT STATE, and kept the way the window layout is kept: raw
// `QSettings` keys, per machine, no `SettingSpec`, no command reads them. A drawing
// that travels to another office does not carry its author's favourite tools.
#pragma once

#include <QObject>

#include <string>
#include <vector>

namespace piricad::app {

class CommandUsage : public QObject
{
    Q_OBJECT

public:
    /// How many recent commands are kept: a short list is the point, and eight is
    /// what fits above the first category without pushing the reference off screen.
    static constexpr std::size_t kRecentLimit = 8;

    /// `persistent` false keeps everything in memory: a probe run must not write
    /// the developer's real preferences (`MainWindow::isProbeRun`).
    explicit CommandUsage(bool persistent, QObject* parent = nullptr);

    /// Most recent first, each id once.
    const std::vector<std::string>& recents() const noexcept { return recent_; }

    /// In the order they were starred.
    const std::vector<std::string>& favourites() const noexcept { return favourite_; }

    bool isFavourite(const std::string& id) const;

    /// A command a person ran has finished.
    void note(const std::string& id);

    /// Stars the command, or takes the star off; true when it is starred afterwards.
    bool toggleFavourite(const std::string& id);

    /// Moves a starred command `by` places up (negative) or down the list, which is the
    /// order the palette shows them in. True when the order changed.
    bool moveFavourite(const std::string& id, int by);

signals:
    /// Either list changed.
    void changed();

private:
    void load();
    void save() const;

    bool persistent_;
    std::vector<std::string> recent_;
    std::vector<std::string> favourite_;
};

} // namespace piricad::app
