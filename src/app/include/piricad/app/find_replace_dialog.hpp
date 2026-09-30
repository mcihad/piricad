// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: find a word in the drawing's captions, see, then change.
//
// THE DIALOG IS A FACE ON BULDEĞİŞTİR (TODOS C-12). It never reads a caption
// itself and never writes one: "Tümünü Bul" sends the command without a
// replacement (it selects what it finds), "Önizle" sends it with `uygula=hayır`
// and fills its table from the command's structured answer (`Context::report`),
// "Tümünü Değiştir" sends the same line with `uygula=evet`. So the preview a hand
// sees here is the preview a script is told, row for row, and the change is one
// journal line and one undo step (Article 1.2, 5.9).
//
// THE PREVIEW IS THE GATE. "Tümünü Değiştir" is enabled only while the fields
// say what the table shows: change a word, a layer or a box and it goes grey
// until Önizle is pressed again. A change nobody saw is not a change this
// window makes.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QString>

#include <cstdint>

class QLabel;
class QTableWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The component set; see widgets.hpp and fields.hpp.
class Button;
class CheckBox;
class Field;

/// Modeless: the canvas stays in reach, and a row picked in the table is the
/// caption shown there.
class FindReplaceDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Builds the window over `controller`, which runs every line it sends.
    explicit FindReplaceDialog(Controller& controller, QWidget* parent = nullptr);

    /// What a button asks of the command.
    enum class Step : std::uint8_t {
        Find,    ///< find and select, no replacement
        Preview, ///< what the replacement would write, nothing written
        Apply,   ///< write what the preview showed
    };

    /// Repaints the window and its controls in `mode`.
    void applyTheme(ThemeMode mode) override;

    /// Fills the fields and clicks the button of `step`, as a hand would — for
    /// the probe that proves the dialog end to end. A disabled button stays
    /// unclicked, exactly as it would under the mouse.
    void runForProbe(const QString& find, const QString& replace, Step step);

    /// How many rows the table shows.
    int previewRows() const;

    /// Whether "Tümünü Değiştir" can be pressed now.
    bool applyEnabled() const;

private:
    /// The command line the fields say for `step`.
    QString commandLine(Step step) const;

    /// Runs the line and puts its answer in the table and the summary.
    void run(Step step);

    /// Greys "Tümünü Değiştir" out unless the fields still say what was previewed.
    void refreshApply();

    Controller& controller_;
    Field* find_{nullptr};
    Field* replace_{nullptr};
    Field* layer_{nullptr};
    CheckBox* matchCase_{nullptr};
    CheckBox* wholeWord_{nullptr};
    QLabel* summary_{nullptr};
    QTableWidget* table_{nullptr};
    Button* findAll_{nullptr};
    Button* preview_{nullptr};
    Button* apply_{nullptr};
    QString previewed_; ///< the preview line the table shows, empty when none
};

} // namespace kentos::app
