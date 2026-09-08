// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the PostGIS window.
//
// WHAT THIS WINDOW IS ALLOWED TO DO: collect arguments and show answers. Every
// button on it builds a `VERİTABANI …` line and runs it through the controller,
// which is the same bus a script and the AI use (Article 1.1, 1.2). There is no
// path from this dialog to `PostgisStore`, and there must not be: a feature only
// the mouse can reach is forbidden by 5.15, and a database write that skipped the
// bus would skip the journal with it.
//
// So this file holds no SQL, no connection and no libpqxx header. It holds four
// line edits, a port field, two lists and the sentences the store sent back.
//
// THE PASSWORD IS NOT REMEMBERED. Host, port, database and user are application
// settings and come back next time; the password field starts empty every time
// and is never written anywhere. libpq reads `~/.pgpass` and `PGPASSWORD`, which
// are the mechanisms that already exist for this and are already protected by
// file permissions — see `docs/komutlar/veritabani.md`.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QString>

/// The Qt widgets this dialog holds, declared rather than included: a header that
/// pulls in the widget classes it stores pointers to makes every translation unit
/// including it wait for them.
class QLabel;
class QLineEdit;
class QListWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The component set; see widgets.hpp and fields.hpp.
class Button;
class Field;

/// Connects to a PostGIS database and moves layers and projects in and out.
class DatabaseDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens the window on the connection the application settings remember.
    /// Opening it connects to nothing: the user presses the connect button.
    explicit DatabaseDialog(Controller& controller, QWidget* parent = nullptr);

private:
    // ---- building ----
    QWidget* buildConnection();
    QWidget* buildContents();

    // ---- acting. Each of these ends in exactly one command line. ----
    void connectToServer();
    void disconnectFromServer();
    void writeLayer();
    void saveProject();
    void openProject();
    void dropProject();

    /// Runs one `VERİTABANI` line and returns what the transcript said about it.
    ///
    /// The dialog reads the ECHO rather than a return value, because the echo is
    /// what every other client of this command sees. If the window could learn
    /// something the command line cannot say, the two would have drifted apart.
    QString run(const QString& line);

    /// Re-reads the table and project lists from the server.
    void refresh();

    /// Enables what a connection makes possible and disables the rest.
    void updateEnabled();

    /// The libpq connection string built from the four fields, or the raw string
    /// the user typed into the host box when it already looks like one.
    QString conninfo() const;

    /// Remembers host, port, database and user — never the password. Written
    /// through `TERCİH`, so the settings file is reached the same way a script
    /// would reach it.
    void rememberConnection();

    /// The port as typed, or libpq's own default when the box is empty — which
    /// is what the box's placeholder promises.
    QString port() const;

    Controller& controller_;

    QLineEdit* host_{nullptr};
    Field* port_{nullptr};
    QLineEdit* database_{nullptr};
    QLineEdit* user_{nullptr};
    QLineEdit* password_{nullptr};
    Button* connect_{nullptr};
    Button* disconnect_{nullptr};
    Button* refresh_{nullptr};
    QLabel* status_{nullptr};

    QListWidget* tables_{nullptr};
    QListWidget* projects_{nullptr};

    Button* writeLayer_{nullptr};
    Button* saveProject_{nullptr};
    Button* openProject_{nullptr};
    Button* dropProject_{nullptr};

    bool connected_{false};
};

} // namespace kentos::app
