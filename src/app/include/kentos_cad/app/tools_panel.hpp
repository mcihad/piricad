// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the tools panel — the processing tools, as a tree and a card.
//
// The third tab beside the attributes and the history. The tree is the tool
// registry (processing.md): one branch per group, one row per tool, each with
// the mark its spec names, nothing typed here by hand. Picking a row shows the
// tool's CARD — what it applies to, the scope, its parameters as fields, the
// output layer — and the exact command line the run button will send, because
// the panel is a client of the bus like the command line is (CLAUDE.md 1.2):
// it composes a line and sends it, and the same line typed by hand does the
// same thing. The card is one widget, shown under the tree or, when the
// `core.islem.pencere` preference says so, in a window of its own.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/core/units.hpp"

#include <functional>
#include <vector>

#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QScrollArea;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QHBoxLayout;

namespace kentos::processing {
class ProcessingTool; ///< a tool of the registry, shown as a row and a card
struct ToolParam;     ///< one of its parameters, shown as a field
} // namespace kentos::processing

namespace kentos::app {

class Button;     ///< the run button (widgets.hpp)
class Controller; ///< the bus the panel sends its line to
class Field;      ///< one parameter's editor (fields.hpp)
class Segment;    ///< the scope chooser (widgets.hpp)

/// One tool's card: what it is, what it will be run with, and the line that
/// runs it. Lives under the tree or inside `ToolDialog`; the two never differ.
class ToolCard : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty card ("choose a tool").
    explicit ToolCard(Controller& controller, QWidget* parent = nullptr);

    /// Shows `tool`, or the empty state for null. Rebuilds every field from the spec.
    void setTool(const processing::ProcessingTool* tool);

    /// The tool shown, or null.
    const processing::ProcessingTool* tool() const noexcept { return shown_; }

    /// Where the viewport is, for the `gorunum` scope: the two corners go on the
    /// command line as `pencere=`, so a script can say the same.
    void setViewportProvider(std::function<core::Box2()> provider);

    /// Whether the card carries its own run button. Off inside the dialog,
    /// whose footer holds it.
    void setRunButtonVisible(bool on);

    /// Re-reads what the scope hint depends on: how many objects are selected.
    void refresh();

    /// The line the run button would send right now, or empty with no tool shown.
    QString commandLine() const;

    /// Sends the line, as the run button does.
    void run();

    void applyTheme(ThemeMode mode) override;

signals:
    /// The user pressed run: `line` is what to run, exactly as typed.
    void runRequested(const QString& line);

private:
    void rebuildPreview();

    Controller& controller_;
    std::function<core::Box2()> viewport_;
    const processing::ProcessingTool* shown_{nullptr};

    QLabel* title_{nullptr};
    QLabel* summary_{nullptr};
    QWidget* chips_{nullptr};
    QHBoxLayout* chipsLayout_{nullptr};
    Segment* scope_{nullptr};
    QLabel* scopeHint_{nullptr};
    QWidget* params_{nullptr};
    QVBoxLayout* paramsLayout_{nullptr};
    Field* layer_{nullptr};
    QLabel* preview_{nullptr};
    Button* run_{nullptr};

    /// One parameter's field, so the line can be composed from what is typed.
    struct Bound
    {
        const processing::ToolParam* param{nullptr};
        Field* field{nullptr};
    };

    std::vector<Bound> bound_;
    ThemeMode theme_{ThemeMode::Dark};
};

/// The card in a window of its own, with run and close in the footer.
class ToolDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Builds the window over `tool`.
    ToolDialog(Controller& controller, const processing::ProcessingTool* tool,
               std::function<core::Box2()> viewport, QWidget* parent = nullptr);

    /// The card inside, so the panel can point it at another tool.
    ToolCard* card() const noexcept { return card_; }

signals:
    /// The footer's run button was pressed: `line` is what to run.
    void runRequested(const QString& line);

private:
    ToolCard* card_{nullptr};
};

/// The tools panel.
class ToolsPanel : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the panel over the registry's tools.
    explicit ToolsPanel(Controller& controller, QWidget* parent = nullptr);

    /// Tells the panel where the viewport is; passed on to every card it makes.
    void setViewportProvider(std::function<core::Box2()> provider);

    /// Re-reads the selection count and the `core.islem.pencere` preference.
    void refresh();

    /// Shows the card of the tool whose command id or name is `id` — under the
    /// tree or in its window, as the preference says. False when no such tool.
    bool selectTool(const QString& id);

    /// The line the run button would send right now, or empty with no tool shown.
    QString commandLine() const;

    void applyTheme(ThemeMode mode) override;

signals:
    /// A card's run button was pressed: `line` is what to run, exactly as typed.
    void runRequested(const QString& line);

protected:
    /// Paints the panel ground, so the card's fields sit on it rather than on
    /// a white viewport.
    void paintEvent(QPaintEvent* event) override;

private:
    void rebuildTree(const QString& filter);
    void showTool(const processing::ProcessingTool* tool);
    bool opensInWindow() const;

    Controller& controller_;
    std::function<core::Box2()> viewport_;

    QLineEdit* search_{nullptr};
    QTreeWidget* tree_{nullptr};
    QScrollArea* scroll_{nullptr};
    ToolCard* card_{nullptr};
    QPointer<ToolDialog> dialog_;
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
