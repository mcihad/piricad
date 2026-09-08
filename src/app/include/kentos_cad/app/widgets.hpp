// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the component set, and the only place a control is made.
//
// WHY THIS FILE EXISTS. `Screenshots/bileşen_standardı.png` is the component
// standard: six kinds of button in one hierarchy, an input with seven states
// that each say a different thing, and the selection controls — box, radio,
// switch, segment, slider, chip. Before this file the program had most of those
// as STYLESHEET RULES and none of them as a THING: every dialog wrote
// `new QPushButton(...)` and then remembered, or did not, to set the object name
// that gave it a role, the height that made it 30 px, the icon, the accessible
// name. Twenty-nine buttons were made that way and they agreed with each other
// only where somebody had checked.
//
// A component here is the rule made into a type. `Button(ButtonRole::Danger, …)`
// cannot forget to be red-edged, cannot be 27 px tall, and cannot be made
// without saying what it is for — which is also what makes it teachable: a
// reader who knows six roles knows every button in the program.
//
// WHAT IS NOT HERE. The inline editors (`fields.hpp`) predate this file and are
// part of the same set; the dialog frame, the section list and the pill switch
// (`dialog_chrome.hpp`) likewise. This header adds what the standard shows and
// the shell lacked, and `scripts/ci-gate-bilesenler.sh` holds the line: a raw
// `QPushButton`, `QCheckBox`, `QRadioButton`, `QSlider`, `QSpinBox`,
// `QProgressBar` or `QGroupBox` constructed anywhere else in `/src/app` fails
// the build.
//
// EVERYTHING PAINTS FROM `tokens.hpp`. No colour is written here; a component
// that paints itself implements `Themed` and re-reads the tokens when told, and
// a component drawn by the stylesheet carries the object name the one sheet in
// `theme.cpp` styles. Either way there is one palette (design.md §2, §12).
#pragma once

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/theme.hpp"

#include <cstdint>
#include <optional>

#include <QAbstractButton>
#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QMenu;
class QSlider;
class QTimer;
class QVBoxLayout;

namespace kentos::app {

// =============================================================================
// Sizes and tones — the two vocabularies every component shares
// =============================================================================

/// The three heights the standard allows, and no fourth.
///
/// `bileşen_standardı.png` is titled "Yükseklik 24 / 30 / 36 px": a compact
/// control for a dense row, the regular one for a form, a large one for the
/// single action that closes a dialog. A control 27 px tall is not a size, it is
/// a mistake, and the enum is what makes it unwritable.
enum class ControlSize : std::uint8_t {
    Compact = 24,
    Regular = 30,
    Large   = 36,
};

/// What a badge, a banner or a caption is SAYING — never what colour it is.
///
/// design.md §1.2 gives each saturated colour one meaning and this enum is that
/// meaning: `Accent` is the program's own derivation (HESAP), `Warn` is something
/// the user changed or must attend to (KAYDEDİLMEDİ, BOŞ), `Danger` is a value
/// that is wrong or an action that cannot be undone (ZORUNLU). `Neutral` says a
/// fact with no urgency (SALT OKUNUR). A caller picks the meaning; the tokens
/// pick the colour, in both themes.
enum class Tone : std::uint8_t {
    Neutral,
    Accent,
    Warn,
    Danger,
    Ok,
};

/// The ink and the wash a tone paints with, from the current tokens. One place,
/// so the inspector's painted badge and the `Badge` widget cannot disagree.
struct ToneColours
{
    QColor ink;  ///< text, outline, glyph
    QColor wash; ///< the ground behind them, translucent
};

/// The ink and the wash a tone paints with in `mode`.
ToneColours toneColours(Tone tone, ThemeMode mode);

// =============================================================================
// Buttons — six kinds, one hierarchy
// =============================================================================

/// What a button is FOR. The standard's own six, in its own order.
///
/// ONE PRIMARY PER SCREEN is the rule that matters more than any measurement,
/// and the standard prints it in its margin: a screen with two primaries has
/// told the user nothing about which one to press.
enum class ButtonRole : std::uint8_t {
    Primary,   ///< the single action that closes the dialog — accent fill
    Secondary, ///< a non-destructive second action — outline
    Ghost,     ///< low priority; toolbars and inline actions — no chrome
    Danger,    ///< cannot be undone — danger outline, and it always confirms
    Mode,      ///< carries an on/off state — reads as pressed while on
    Icon,      ///< 32×32, a 16 px glyph, and a tooltip that is its whole label
};

/// One button. Built with its role and it cannot lose it.
class Button : public QPushButton, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// A labelled button. `glyph` is optional and sits before the text, 16 px.
    Button(ButtonRole role, const QString& text, std::optional<Glyph> glyph = std::nullopt,
           QWidget* parent = nullptr);

    /// An icon-only button. The tooltip IS its label, so it is required, and it
    /// also becomes the accessible name a screen reader speaks (ui.md R22).
    Button(Glyph glyph, const QString& tooltip, QWidget* parent = nullptr);

    ButtonRole role() const noexcept { return role_; }

    /// The height, from the standard's three. Regular unless told otherwise.
    void setControlSize(ControlSize size);

    /// Attaches a menu and draws the `▾` that says so. A button that opens a
    /// list and does not say it is a list nobody opens.
    void setMenuArrow(QMenu* menu);

    void applyTheme(ThemeMode mode) override;

protected:
    /// Draws the keyboard focus ring after Qt has drawn the button.
    ///
    /// §11: "1 px accent outer frame + 2 px offset; not shown on a mouse click".
    /// A stylesheet cannot tell a Tab from a click, so the reason is remembered
    /// in `focusInEvent` and the ring is painted here only for the keyboard.
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    /// Repaints the glyph in the tone the role and the state call for.
    void refreshIcon();

    ButtonRole role_;
    std::optional<Glyph> glyph_;
    ControlSize size_{ControlSize::Regular};
    bool keyboardFocus_{false};
    ThemeMode theme_{ThemeMode::Dark};
};

// =============================================================================
// Selection — box, radio, segment, slider, chip
// =============================================================================

/// A tick box. Painted, so the tick is a SHAPE and not only a colour (§13), and
/// so the indeterminate state has a dash the platform style would not give it.
///
/// Drop-in for the `QCheckBox` calls it replaces: `toggled`, `setChecked`,
/// `isChecked`, and `setTristate` / `setCheckState` for the three-way case.
class CheckBox : public QAbstractButton, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a box with `text` at its right. Three states through `setCheckState`;
    /// a click cycles the two a person can mean.
    explicit CheckBox(const QString& text, QWidget* parent = nullptr);

    /// A box that can also mean "some of them": a group whose members disagree.
    void setTristate(bool on) { tristate_ = on; }

    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const noexcept;

    void applyTheme(ThemeMode mode) override;
    QSize sizeHint() const override;

protected:
    /// Draws the box, the tick or the bar in it, the text, and the focus ring.
    void paintEvent(QPaintEvent* event) override;
    /// Repaints so the ring appears — for keyboard focus only (design.md §13).
    void focusInEvent(QFocusEvent* event) override;
    /// Repaints so the ring goes.
    void focusOutEvent(QFocusEvent* event) override;
    /// Unchecked to checked and back. The partial state is set only in code,
    /// because a click is a decision and "some of them" is not one.
    void nextCheckState() override;

private:
    bool tristate_{false};
    bool partial_{false};
    bool keyboardFocus_{false};
    ThemeMode theme_{ThemeMode::Dark};
};

/// One of several. Exclusive within its parent, the way `QRadioButton` is.
class RadioButton : public QAbstractButton, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a radio with `text` at its right; exclusive among its siblings, as Qt's is.
    explicit RadioButton(const QString& text, QWidget* parent = nullptr);

    /// Repaints in `mode`'s tokens.
    void applyTheme(ThemeMode mode) override;
    /// The ring, the gap and the text, on the standard's 20 px row.
    QSize sizeHint() const override;

protected:
    /// Draws the ring, the dot when chosen, the text, and the focus ring.
    void paintEvent(QPaintEvent* event) override;
    /// Repaints so the ring appears — for keyboard focus only.
    void focusInEvent(QFocusEvent* event) override;
    /// Repaints so the ring goes.
    void focusOutEvent(QFocusEvent* event) override;

private:
    bool keyboardFocus_{false};
    ThemeMode theme_{ThemeMode::Dark};
};

/// A row of mutually exclusive options that reads as ONE control: `mm | harita |
/// px`, `Tablo | Form`, `Milimetre | Harita birimi | Piksel`.
///
/// It replaced three places that each built the same thing from loose push
/// buttons and a `QButtonGroup` — and each rounded a different corner.
class Segment : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty control. `addOption` fills it; the first option added is lit.
    explicit Segment(QWidget* parent = nullptr);

    /// Appends an option and returns its index. Options are added once, at
    /// construction; a segment whose options change is a combo box in disguise.
    int addOption(const QString& text, const QString& tooltip = QString());

    /// Lights option `index` and reports it. `-1` lights nothing: the answer
    /// when the things the control describes disagree (a symbol whose layers
    /// are in different units), and a state the standard's picture cannot show.
    void setCurrent(int index);

    int current() const noexcept { return current_; }

    int count() const noexcept { return static_cast<int>(buttons_.size()); }

    void setControlSize(ControlSize size);

    void applyTheme(ThemeMode mode) override;

signals:
    /// A different option was chosen, by click or by keyboard.
    void currentChanged(int index);

private:
    QHBoxLayout* row_{nullptr};
    QButtonGroup* group_{nullptr};
    QVector<QPushButton*> buttons_;
    int current_{-1};
    ControlSize size_{ControlSize::Compact};
    ThemeMode theme_{ThemeMode::Dark};
};

/// A slider with its value printed beside it, in mono, because a slider alone
/// cannot be READ — only compared — and a figure in this program is always read.
class Slider : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a 0–100 slider with its readout at the right.
    explicit Slider(QWidget* parent = nullptr);

    /// The bounds of the value, inclusive.
    void setRange(int low, int high);
    /// Moves the handle to `value`, clamped to the range; reports as a drag would.
    void setValue(int value);
    /// Where the handle is.
    int value() const;

    /// Printed around the figure: `setAffixes("%", "")` reads `%62`,
    /// `setAffixes("", " m")` reads `12 m`.
    void setAffixes(const QString& prefix, const QString& suffix);

    void applyTheme(ThemeMode mode) override;

signals:
    /// The handle moved — by drag, by key, or by `setValue`.
    void valueChanged(int value);

private:
    void refreshReadout();

    QSlider* slider_{nullptr};
    QLabel* readout_{nullptr};
    QString prefix_;
    QString suffix_;
    ThemeMode theme_{ThemeMode::Dark};
};

/// A tag: a pill with a 1 px outline in its own colour. `Konut`, `Ticaret`,
/// `Yeşil alan` — the colour is the category's, handed in, never a token.
///
/// Checkable, so a row of chips is a filter; and `overflow(n)` makes the `+2`
/// that stands for the ones there was no room to show.
class Chip : public QAbstractButton, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a pill reading `text`, inked and washed from `colour`. `setCheckable(true)`
    /// makes it a filter; left as it is, it only shows.
    Chip(const QString& text, const QColor& colour, QWidget* parent = nullptr);

    /// The `+N` chip: neutral, not checkable, and it says how many are hidden.
    static Chip* overflow(int hidden, QWidget* parent = nullptr);

    void setColour(const QColor& colour);

    void applyTheme(ThemeMode mode) override;
    QSize sizeHint() const override;

protected:
    /// Draws the pill — filled when checked, outlined when not — and the focus ring.
    void paintEvent(QPaintEvent* event) override;
    /// Repaints so the ring appears — for keyboard focus only.
    void focusInEvent(QFocusEvent* event) override;
    /// Repaints so the ring goes.
    void focusOutEvent(QFocusEvent* event) override;

private:
    QColor colour_;
    bool neutral_{false};
    bool keyboardFocus_{false};
    ThemeMode theme_{ThemeMode::Dark};
};

// =============================================================================
// Annotation — badge, banner, progress
// =============================================================================

/// A micro label that says what state a value is in: `HESAP`, `BOŞ`, `ZORUNLU`,
/// `KAYDEDİLMEDİ`, `SALT OKUNUR`, `FOCUS`.
///
/// Uppercase by the caller, in Turkish, never by `toUpper()` (design.md §3:
/// "text-transform kullanılmaz, metin doğrudan büyük yazılır").
class Badge : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a 14 px badge reading `text` in `tone`.
    Badge(const QString& text, Tone tone, QWidget* parent = nullptr);

    /// Changes the text; the badge resizes to it.
    void setText(const QString& text);
    /// Changes the tone; the badge repaints in it.
    void setTone(Tone tone);

    /// Repaints in `mode`'s tokens.
    void applyTheme(ThemeMode mode) override;
    /// `widthFor(text)` by the standard's 14 px.
    QSize sizeHint() const override;

    /// The drawing, as a free function, so a widget that paints its own rows —
    /// the object inspector — draws the SAME badge this widget does. Two badge
    /// painters is two badges that drift apart by a pixel and a shade.
    static void paint(QPainter& painter, const QRect& box, const QString& text, Tone tone,
                      ThemeMode mode);

    /// How wide `text` needs, at the badge's own type.
    static int widthFor(const QString& text);

protected:
    /// Draws the wash, the outline and the small-caps text through `paint()`.
    void paintEvent(QPaintEvent* event) override;

private:
    QString text_;
    Tone tone_;
    ThemeMode theme_{ThemeMode::Dark};
};

/// A full-width strip with a glyph, a title, a sentence and one optional
/// action: `Geçerlilik denetimi: 2 uyarı … [Ayrıntı]`.
///
/// The tone is the whole statement: `Warn` asks for care, `Danger` says
/// something is wrong, `Info` only informs. A banner is not a dialog — it never
/// blocks, and it sits where the thing it is about sits.
class Banner : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a banner in `tone`: a coloured left edge, the tone's glyph, `title` in
    /// weight 600 and `text` under it.
    Banner(Tone tone, const QString& title, const QString& text, QWidget* parent = nullptr);

    /// Adds the one action at the right end. A banner offers at most one.
    Button* addAction(const QString& text);

    void setTitle(const QString& title);
    void setText(const QString& text);

    void applyTheme(ThemeMode mode) override;

protected:
    /// Draws the wash, the coloured edge and the glyph; the words are child labels.
    void paintEvent(QPaintEvent* event) override;

private:
    Tone tone_;
    QLabel* glyph_{nullptr};
    QLabel* title_{nullptr};
    QLabel* text_{nullptr};
    QHBoxLayout* row_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

/// The 2 px accent strip §11 puts at the top of a heading while something loads.
///
/// INDETERMINATE ON PURPOSE. The readers stream and do not report a fraction,
/// and a bar that filled itself on a timer would be a lie told at the exact
/// moment the user is deciding whether to wait. Content stays in place under it.
class ProgressStrip : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a stopped strip: 2 px high, as wide as its owner makes it.
    explicit ProgressStrip(QWidget* parent = nullptr);

    /// Starts or stops the moving segment. Stopped, the strip is invisible.
    void setActive(bool on);

    bool isActive() const noexcept { return active_; }

    void applyTheme(ThemeMode mode) override;
    QSize sizeHint() const override;

protected:
    /// Draws the moving segment while active, and nothing at all when stopped.
    void paintEvent(QPaintEvent* event) override;

    /// Being shown IS being active. A strip lives inside a box its owner shows
    /// while something loads and hides when it stops, so the two never
    /// disagree: a strip that is visible is moving, and one that is hidden has
    /// stopped its clock.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QTimer* clock_{nullptr};
    bool active_{false};
    int phase_{0};
    ThemeMode theme_{ThemeMode::Dark};
};

// =============================================================================
// Form — the grammar a page of inputs is written in
// =============================================================================

/// A labelled input: the name above, the control below, a caption at the right
/// end of the name, and one line under the control that helps or complains.
///
/// `form_örnek.png` is written entirely in these. The label carries the
/// required mark (`*`, accent) and the caption carries the state a badge would
/// (`HESAP`, `SALT OKUNUR`); the line underneath is dim when it helps ("Ölçü
/// alanından 2.64 m² sapma — tolerans içinde.") and danger when it complains
/// ("Zorunlu alan — imar durumu belgesi üretilemez."), and only one of the two
/// is shown at a time because a field is not both fine and wrong.
class FormRow : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Takes ownership of `editor` and places it under `label`.
    FormRow(const QString& label, QWidget* editor, QWidget* parent = nullptr);

    QWidget* editor() const noexcept { return editor_; }

    /// Marks the field required: an accent `*` after the label.
    void setRequired(bool on);

    /// The micro caption at the right end of the label row, or empty for none.
    void setCaption(const QString& text, Tone tone = Tone::Neutral);

    /// The dim line under the control. Clears any error.
    void setHelp(const QString& text);

    /// The danger line under the control. Clears any help, and marks the editor
    /// invalid through the `state` property the stylesheet reads.
    void setError(const QString& text);

    void applyTheme(ThemeMode mode) override;

private:
    void refreshLabel();

    QString label_;
    bool required_{false};
    QLabel* name_{nullptr};
    Badge* caption_{nullptr};
    QWidget* editor_{nullptr};
    QLabel* note_{nullptr};
    QHBoxLayout* head_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

/// A heading that opens a group of rows: small caps, a rule to the right edge,
/// and an optional note at the far end (`TAKBİS'ten çekildi · 14.03.2019`).
class FormSection : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the heading: `title` in small caps, a rule to the right edge, `note` at
    /// the far end of it.
    FormSection(const QString& title, const QString& note = QString(), QWidget* parent = nullptr);

    /// Replaces the note at the far end.
    void setNote(const QString& note);

    /// Repaints in `mode`'s tokens.
    void applyTheme(ThemeMode mode) override;
    /// One heading row, at the standard's section height.
    QSize sizeHint() const override;

protected:
    /// Draws the title, the rule and the note.
    void paintEvent(QPaintEvent* event) override;

private:
    QString title_;
    QString note_;
    ThemeMode theme_{ThemeMode::Dark};
};

// =============================================================================
// The living standard
// =============================================================================

/// Builds a window showing every component in every state, laid out as
/// `bileşen_standardı.png` is.
///
/// It is the standard made runnable: `KENTOS_WIDGETS_PROBE` opens it, prints an
/// inventory the gate reads, and photographs it when given a directory — which
/// is how the picture in `docs/baslangic/bilesenler.md` is regenerated
/// (docs.md R15) and how a change to a component is seen before it is shipped.
QWidget* buildComponentSheet(ThemeMode mode, QWidget* parent = nullptr);

/// One line per component on the sheet — kind, state, height — for the gate.
QStringList componentSheetInventory(QWidget* sheet);

} // namespace kentos::app
