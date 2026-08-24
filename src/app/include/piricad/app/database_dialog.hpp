// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the PostGIS window.
//
// WHAT THIS WINDOW IS ALLOWED TO DO: collect arguments and show answers. Every
// button on it builds a `VERİTABANI …` line and runs it through the controller,
// which is the same bus a script and the AI use (Article 1.1, 1.2). There is no
// path from this dialog to `PostgisStore`, and there must not be: a feature only
// the mouse can reach is forbidden by 5.15, and a database write that skipped the
// bus would skip the journal with it.
//
// So this file holds no SQL, no connection and no libpqxx header. It holds four
// line edits, two lists and the sentences the store sent back.
//
// THE PASSWORD IS NOT REMEMBERED. Host, port, database and user are application
// settings and come back next time; the password field starts empty every time
// and is never written anywhere. libpq reads `~/.pgpass` and `PGPASSWORD`, which
// are the mechanisms that already exist for this and are already protected by
// file permissions — see `docs/komutlar/veritabani.md`.
#pragma once

#include <QDialog>
#include <QString>

/// The Qt widgets this dialog holds, declared rather than included: a header that
/// pulls in the widget classes it stores pointers to makes every translation unit
/// including it wait for them.
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Connects to a PostGIS database and moves layers and projects in and out.
class DatabaseDialog : public QDialog
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

    Controller& controller_;

    QLineEdit* host_{nullptr};
    QSpinBox* port_{nullptr};
    QLineEdit* database_{nullptr};
    QLineEdit* user_{nullptr};
    QLineEdit* password_{nullptr};
    QPushButton* connect_{nullptr};
    QPushButton* disconnect_{nullptr};
    QPushButton* refresh_{nullptr};
    QLabel* status_{nullptr};

    QListWidget* tables_{nullptr};
    QListWidget* projects_{nullptr};

    QPushButton* writeLayer_{nullptr};
    QPushButton* saveProject_{nullptr};
    QPushButton* openProject_{nullptr};
    QPushButton* dropProject_{nullptr};

    bool connected_{false};
};

} // namespace piricad::app
