// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the list of a drawing's paftas.
//
// WHY A LIST WINDOW EXISTS AT ALL. The designer edits ONE sheet. Everything
// about the SET of them — how many there are, which to open, renaming one,
// copying the A3 you spent an afternoon on into an A2, throwing one away — has
// no home in a window that is already showing a page. QGIS learned this and
// calls it the Layout Manager; this is the same thing under the name a surveyor
// uses.
//
// EVERY BUTTON IS A COMMAND, like everywhere else: `PAFTA islem=ekle`, `=sil`,
// `=ad`. Copying is the one that is two lines rather than one, and it is honest
// about that — there is no `islem=kopyala`, so the window does what a user would
// do by hand and says so in the journal (Article 1.2).
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QString>

class QListWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;
/// The component set's button; see widgets.hpp.
class Button;

/// Lists the drawing's paftas and opens, renames, copies or removes one.
class LayoutManager : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on the drawing's current sheets, with nothing selected.
    explicit LayoutManager(Controller& controller, QWidget* parent = nullptr);

    /// Repaints in `mode`'s tokens.
    void applyTheme(ThemeMode mode) override;

signals:
    /// The user asked to open `layout` in the designer. The shell opens it, so
    /// this window does not have to know what a designer is.
    void openRequested(const QString& layout);

private:
    QWidget* buildBody();

    /// Refills the list from the document and re-enables what a selection allows.
    void refresh();

    /// The selected sheet's name, or empty.
    QString selected() const;

    Controller& controller_;
    QListWidget* list_{nullptr};
    Button* open_{nullptr};
    Button* rename_{nullptr};
    Button* copy_{nullptr};
    Button* remove_{nullptr};
};

} // namespace kentos::app
