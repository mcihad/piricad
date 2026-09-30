// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the external references panel (TODOS C-14).
//
// Every external reference the drawing holds, with the state it is in —
// loaded, put aside, its file missing, its file CHANGED since it was read —
// and the steps a user takes on one: read it again, put it aside or bring it
// back, point it at another file, make it the drawing's own, take it off.
// Each step is a `DIŞREFERANS` line through the controller, like every other
// client's (Article 1.2); the panel only reads the document and says what it
// saw. What it adds that no command can is the WATCH: a source file saved
// elsewhere while the drawing is open is noticed, said, and offered for reload.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QWidget>

class QFileSystemWatcher;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace kentos::app {

class Banner;     ///< widgets.hpp
class Button;     ///< widgets.hpp
class Controller; ///< controller.hpp

/// Paints one external reference as two lines: the eye, the name and its
/// state, the counts; under them, where its file is.
class XrefRowDelegate : public QStyledItemDelegate, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// A delegate for the panel's list.
    explicit XrefRowDelegate(QObject* parent = nullptr);

    /// Draws one row from its roles (name, state, counts, file, seen).
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    /// Two lines tall, as wide as the list.
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Whether `x` falls on the eye of a row.
    static bool onEye(int x);

    void applyTheme(ThemeMode mode) override { theme_ = mode; }

private:
    ThemeMode theme_ = ThemeMode::Dark;
};

/// The list, its steps and the watch over the files it lists.
class XrefPanel : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the panel over a controller, which outlives it.
    explicit XrefPanel(Controller& controller, QWidget* parent = nullptr);

    /// Reads the document again: the rows, their states, the files watched.
    void refresh();

    /// A step finished: a `core.xref` report names whose file was read again,
    /// and those are no longer changed.
    void onCommandFinished(const QString& id, const QString& report);

    void applyTheme(ThemeMode mode) override;

    /// For `KENTOS_REALMOUSE_PROBE`: each row as `name|STATE|counts`.
    QStringList probeRows() const;

    /// For the probe: selects `name`'s row and presses the step button whose
    /// text is `step` with a REAL mouse event. False when either is missing or
    /// the button is disabled.
    bool probePress(const QString& name, const QString& step);

    /// For the probe: whether the change banner shows; with `press`, a REAL
    /// click on its button.
    bool probeBanner(bool press);

    /// For the probe: a REAL click on `name`'s eye.
    bool probeEye(const QString& name);

    /// The step `Yol…` takes once a file is chosen — split from the window
    /// that chooses it, so a probe can take the same road without the window.
    void repathTo(const QString& name, const QString& file);

signals:
    /// The panel's own `+`: attach a file. The shell owns the file window.
    void attachRequested();

    /// A sentence for the status bar: a source changed.
    void notice(const QString& text);

private:
    /// The name of the selected row, or empty.
    QString selectedName() const;
    /// Enables the steps for the selected row and names the toggle.
    void syncButtons();
    /// A source file changed on disk.
    void onFileChanged(const QString& path);
    /// Shows or hides the change banner for what `changed_` holds.
    void syncBanner();
    /// Gives the banner the height its wrapped sentence needs at this width.
    void fitBanner();
    /// Runs `DIŞREFERANS islem=<op> ad="<name>"`.
    void runStep(const QString& op, const QString& name);
    /// Puts a reference's layers out while any shows, or brings them all up.
    void toggleSeen(const QString& name);
    /// The layers a reference's file brought: `NAME|…`.
    QStringList layersOf(const QString& name) const;
    /// A row's context menu: the same steps, in words.
    void showMenu(const QPoint& at);
    /// Presses `button` with a real mouse event.
    static bool pressByHand(Button* button);

    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    Controller& controller_;
    Banner* banner_{nullptr};
    QTreeWidget* tree_{nullptr};
    XrefRowDelegate* delegate_{nullptr};
    QLabel* empty_{nullptr};
    QWidget* steps_{nullptr}; ///< the row of step buttons, hidden while nothing is listed
    Button* reload_{nullptr};
    Button* unload_{nullptr};
    Button* repath_{nullptr};
    Button* bind_{nullptr};
    Button* detach_{nullptr};
    QFileSystemWatcher* watcher_{nullptr};

    /// Names whose file changed on disk since it was last read here.
    QSet<QString> changed_;
    /// The file the drawing belongs to, so a different drawing clears `changed_`.
    QString project_;
    /// Name → path, as last read, for matching a changed file to its rows.
    QHash<QString, QString> paths_;
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
