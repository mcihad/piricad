// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the application menu, what the `KentOS CAD` button opens.
//
// AUTOCAD'S APPLICATION MENU, not a list (`design.md` §7, `.claude/ui.md` R50).
// Three regions under a search chip:
//
//   ┌ Komut ara…                                             Ctrl+K ┐
//   ├──────────────────────────┬────────────────────────────────────┤
//   │ ▣ Yeni                   │ Son kullanılan belgeler            │
//   │   Boş bir çizim açar     │ ada-112.pcad          2 dk önce    │
//   │ ▣ Aç…                    │   ~/Projeler/Sivas                 │
//   │ ▣ Yazdır               › │ …                                  │
//   ├──────────────────────────┴────────────────────────────────────┤
//   │ Komut Listesi   Hakkında                     Ayarlar   Çıkış   │
//   └────────────────────────────────────────────────────────────────┘
//
// The left column is the file verbs, each with its picture, its name and one
// line of what it does. The pane on the right shows the documents opened last —
// and, while the pointer or the keyboard is on a verb that has choices of its
// own (`Yazdır`'s profiles, the drawing's layouts), those choices. The foot holds
// the program's settings and the way out.
//
// IT RUNS NOTHING ITSELF. Every row triggers the action it was handed, the same
// action the ribbon, the quick access row and the shortcuts trigger, and a
// recent document is opened by the line `AÇ` is (CLAUDE.md Article 1.2).
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <functional>

#include <QList>
#include <QString>
#include <QVector>
#include <QWidget>

class QAction;
class QVBoxLayout;

namespace kentos::app {

/// The painted row the menu is built from (`app_menu.cpp`) and the component
/// set's button its foot uses (`widgets.hpp`).
class AppMenuRow;
class Button;

/// The application menu. A popup: it closes when a row is chosen, when Esc is
/// pressed and when a click lands outside it.
class ApplicationMenu : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// One verb of the left column.
    struct Entry
    {
        QAction* action{nullptr}; ///< what the row triggers; its text and picture are the row's
        QString detail;           ///< one line under the name: what it does
        /// The pane's rows while this verb is the current one; unset for a verb
        /// that has none, which leaves the recent documents in the pane.
        std::function<QList<QAction*>()> choices;
        bool groupStart{false}; ///< a rule above it: the start of a group
    };

    /// One document of the pane.
    struct Recent
    {
        QString path;   ///< what `AÇ` is given
        QString name;   ///< the file name
        QString folder; ///< where it is, shortened
        QString when;   ///< how long ago it was last opened or saved
    };

    /// Builds the empty menu; `setEntries` and `setFooter` fill it.
    explicit ApplicationMenu(QWidget* parent);

    /// The left column, top to bottom.
    void setEntries(const QVector<Entry>& entries);

    /// The documents the pane lists, most recent first.
    void setRecent(const QVector<Recent>& recent);

    /// The foot: the command list, the about box, the settings and quit.
    void setFooter(QAction* help, QAction* about, QAction* settings, QAction* quit);

    /// Opens the menu under `anchor`, its left edge on the anchor's, with the
    /// keyboard on the first verb.
    void popupUnder(QWidget* anchor);

    void applyTheme(ThemeMode mode) override;

signals:
    /// The search chip was pressed: the palette should open.
    void searchRequested();

    /// A document of the pane was chosen.
    void recentChosen(const QString& path);

protected:
    /// Paints the three grounds — head and foot, the column, the pane — and
    /// the rules between them; the rows paint themselves.
    void paintEvent(QPaintEvent* event) override;
    /// Walks it like a menu: up and down a column, right into the pane, left
    /// back to the verbs, Esc out.
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// Makes verb `index` the current one and shows its choices, or the
    /// documents when it has none.
    void setCurrent(int index);

    /// Fills the pane with the documents.
    void showRecent();

    /// Fills the pane with `choices` under `title`.
    void showChoices(const QString& title, const QList<QAction*>& choices);

    /// Empties the pane.
    void clearPane();

    /// Closes the menu and triggers `action`.
    void choose(QAction* action);

    QVector<Entry> entries_;
    QVector<Recent> recent_;
    QList<AppMenuRow*> rows_;
    QList<AppMenuRow*> paneRows_;
    int current_{-1};
    QVBoxLayout* column_{nullptr};
    QVBoxLayout* pane_{nullptr};
    QWidget* search_{nullptr};
    QList<Button*> footer_;
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
