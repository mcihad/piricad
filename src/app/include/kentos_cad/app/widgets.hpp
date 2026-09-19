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
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QMenu;
class QScrollArea;
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

/// The one drop-down list — `bileşen_standardı.png`'s `Açılır liste`.
///
/// A `QComboBox` under Fusion draws its own box and its own arrow, and under a
/// stylesheet either keeps the arrow and loses the box or the other way round;
/// every combo in the program looked like a different program's. This one paints
/// itself: the standard's input ground and border, the value in sans, a drawn
/// chevron that re-tints with the theme, the accent border on focus, and the
/// `state` the field standard names (changed, invalid, derived, read-only). Its
/// popup is the shell's own list, 26 px rows on the panel ground.
///
/// It IS a `QComboBox`, so `addItem`, `currentText`, `currentData` and every
/// signal are Qt's — a caller replaces the type name and nothing else.
class ComboBox : public QComboBox, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty list at the standard's regular height.
    explicit ComboBox(QWidget* parent = nullptr);

    /// One of the standard's three heights.
    void setControlSize(ControlSize size);

    /// Drawn without its box — the value and the chevron only — when a `Field`
    /// or a table cell owns it and paints the frame itself. Releases the fixed
    /// height too: the owner's height is then the height.
    void setBare(bool on);

    void applyTheme(ThemeMode mode) override;

    /// The text's width plus the chevron; the caller widens it as it likes.
    QSize sizeHint() const override;

protected:
    /// Draws the box, the value (with its icon, when the item has one) and the
    /// chevron; the focus ring for keyboard focus only.
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    ControlSize size_{ControlSize::Regular};
    ThemeMode theme_{ThemeMode::Dark};
    bool bare_{false};
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

    /// Adds the one action button at the right end. A banner offers at most one.
    /// NOT `addAction`: that is `QWidget`'s, takes a `QAction*`, and a banner
    /// hiding it would make `banner->addAction(someQAction)` stop compiling.
    Button* addButton(const QString& text);

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
// Conversation — the five parts a chat is drawn from
// =============================================================================

/// Who a bubble is from. The UI's own four, deliberately NOT `ai::Role`:
/// `widgets.hpp` is the component set and may not know what an AI is, exactly as
/// it does not know what a parcel is. The chat panel maps one to the other in one
/// function, and the living standard can draw all four without linking the model.
enum class Speaker : std::uint8_t {
    Person,     ///< the engineer at the workstation — accent wash, right-aligned head
    Model,      ///< the model's answer, which is ALWAYS labelled `ÖNERİ` (ai.md R17)
    ToolResult, ///< what a tool reported, in mono, folded away by default
    Notice,     ///< the program speaking for itself: a refusal, a cancellation
};

/// Three dots that rise in turn: what the program shows while it is waiting on a
/// model that has not said anything yet.
///
/// THE ONLY ANIMATION BESIDE `ProgressStrip`, and it exists for the same reason:
/// a stream can be silent for several seconds before the first token — a reasoning
/// model is silent for thirty — and a panel that showed nothing in that time reads
/// as a panel that is broken. It reports no fraction, because there is none. When
/// the profile shows thinking text the dots give way to the text; when it does
/// not, they are all the user gets — which is the interactive "thinking" mark
/// the brief asked for in place of reasoning text.
class ThinkingDot : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds stopped dots; they start when the widget is shown.
    explicit ThinkingDot(QWidget* parent = nullptr);

    /// Starts or stops the dots. Stopped, the widget draws nothing.
    void setActive(bool on);

    bool isActive() const noexcept { return active_; }

    /// The word beside the dots, `Düşünüyor` by default. A caller replaces it
    /// with what is actually happening: `Bağlanıyor`, `Araç çalışıyor`.
    void setLabel(const QString& text);

    /// Shows `s` seconds beside the label once a turn has run long enough to be
    /// worth timing. Negative hides it.
    void setElapsedSeconds(int seconds);

    void applyTheme(ThemeMode mode) override;
    QSize sizeHint() const override;

protected:
    /// Draws the three dots and the word beside them, and nothing when stopped.
    void paintEvent(QPaintEvent* event) override;

    /// Being shown IS being active, for the reason `ProgressStrip` says: a
    /// widget its owner reveals while waiting and hides when done must not also
    /// need to be told to start, or the two disagree.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QTimer* clock_{nullptr};
    QString label_;
    int phase_{0};
    int elapsed_{-1};
    bool active_{false};
    ThemeMode theme_{ThemeMode::Dark};
};

/// One turn in the transcript: a head with the speaker and its badge, the words
/// under it, and — for a model turn — a fold holding the reasoning.
///
/// TEXT ARRIVES IN PIECES, WHICH IS WHY `appendText` EXISTS. A streamed answer
/// grows a few characters at a time and `setText` on every fragment would
/// re-layout the whole bubble sixty times a second; `appendText` adds to the
/// document instead. The bubble reports `wantsScroll()` so the transcript can
/// decide whether to follow — a person who scrolled up to read is not dragged
/// back down by an answer still arriving.
class MessageBubble : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty bubble for `speaker`. A `Model` bubble carries the `ÖNERİ`
    /// badge from the moment it is built, before a single character has arrived:
    /// the label is not a verdict on the content, it is what the content IS
    /// (ai.md R17, P4).
    explicit MessageBubble(Speaker speaker, QWidget* parent = nullptr);

    Speaker speaker() const noexcept { return speaker_; }

    /// Replaces the words.
    void setText(const QString& text);

    /// Adds to them, for a stream.
    void appendText(const QString& text);

    /// The words as they stand.
    QString text() const;

    /// Adds reasoning text to the fold, creating it on the first call. Only a
    /// `Model` bubble has one, and only when the profile asked for the thinking
    /// to be shown (`core.ai.dusunme_goster`).
    void appendReasoning(const QString& text);

    /// Opens or closes the reasoning fold. Closed is the default: the answer is
    /// what the reader came for.
    void setReasoningOpen(bool open);

    bool hasReasoning() const noexcept;

    /// Shows the moving dots under the words while the turn is in flight, and the
    /// elapsed time beside them once it has run for more than a second.
    void setWaiting(bool waiting);

    /// A one-line note under the words, in `tone`: the turn was cancelled, the
    /// provider failed, the steps were filed as a suggestion.
    void setNote(const QString& text, Tone tone = Tone::Neutral);

    /// Puts a widget at the foot of the bubble — the suggestion card goes here,
    /// so a proposal is read inside the turn that made it rather than in a modal
    /// somewhere else. Takes ownership.
    void setFooter(QWidget* footer);

    void applyTheme(ThemeMode mode) override;

protected:
    /// Draws the wash and the rounded outline; every word in it is a child label.
    void paintEvent(QPaintEvent* event) override;

private:
    Speaker speaker_;
    QLabel* who_{nullptr};
    Badge* mark_{nullptr};
    QLabel* body_{nullptr};
    Button* foldButton_{nullptr};
    QLabel* fold_{nullptr};
    ThinkingDot* dots_{nullptr};
    QLabel* note_{nullptr};
    QWidget* footer_{nullptr};
    QVBoxLayout* column_{nullptr};
    QString reasoning_;
    ThemeMode theme_{ThemeMode::Dark};
};

/// The scrolling column of bubbles.
///
/// IT FOLLOWS THE END ONLY WHILE THE READER IS AT THE END. A transcript that
/// always scrolled down would yank the view away from somebody reading an earlier
/// answer every time a token arrived; one that never scrolled would hide the
/// answer being written. So `append` and `bumped()` scroll only when the view was
/// already within `kFollowSlack` pixels of the bottom, which is the same rule a
/// terminal uses.
class Transcript : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty transcript showing its placeholder.
    explicit Transcript(QWidget* parent = nullptr);

    /// Adds a bubble at the end and takes ownership of it.
    void append(QWidget* bubble);

    /// The last bubble, or null when the transcript is empty.
    QWidget* last() const noexcept;

    /// How many bubbles are in it.
    int count() const;

    /// Removes and destroys every bubble. What "yeni sohbet" does.
    void clear();

    /// Tells the transcript that the last bubble grew, so it can follow the end
    /// if the reader is there.
    void bumped();

    /// Whether the view is at the end, within the follow slack.
    bool atEnd() const;

    /// Scrolls to the end regardless — what the send button does, because a
    /// person who just pressed Enter is asking to see the answer.
    void toEnd();

    /// Scrolls to the beginning. What the living standard uses, so the sheet
    /// shows every bubble rather than the tail of a scroll.
    void toStart();

    /// The placeholder shown while there is nothing to read: a sentence naming
    /// what the panel is for, replaced by the first bubble.
    void setPlaceholder(const QString& text);

    void applyTheme(ThemeMode mode) override;

private:
    QScrollArea* scroll_{nullptr};
    QWidget* column_{nullptr};
    QVBoxLayout* stack_{nullptr};
    QLabel* placeholder_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

/// One attached file, before it is sent: a kind glyph, the name, the size, and an
/// × that takes it off again.
///
/// SIZE IS PART OF THE LABEL, not a tooltip. An attachment is charged to the
/// context window and a 4 MB screenshot is most of a small one, so the number a
/// user needs in order to decide is on the chip.
class AttachmentChip : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// `name` is the file's own name and `bytes` its size; `media` is its media
    /// type, which picks the glyph.
    AttachmentChip(const QString& name, qint64 bytes, const QString& media,
                   QWidget* parent = nullptr);

    const QString& fileName() const noexcept { return name_; }

    qint64 byteCount() const noexcept { return bytes_; }

    /// Hides the × — what a chip in a message already sent looks like, since an
    /// attachment cannot be taken out of a turn the model has read.
    void setRemovable(bool on);

    void applyTheme(ThemeMode mode) override;
    QSize sizeHint() const override;

signals:
    /// The × was pressed. The owner drops the attachment and deletes the chip.
    void removeRequested();

protected:
    /// Draws the ground, the kind glyph, the elided name and the size; the × is
    /// a child button.
    void paintEvent(QPaintEvent* event) override;

private:
    QString name_;
    QString shown_;
    qint64 bytes_;
    Glyph glyph_;
    Button* drop_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

/// How much of the model's context the conversation is using: a bar, the two
/// numbers, and — the part that matters — WHICH KIND OF NUMBER it is.
///
/// AN ESTIMATE AND A MEASUREMENT ARE NOT THE SAME READING. During a turn the only
/// count available is this program's own, four bytes to the token; when the turn
/// ends the provider reports what it actually charged. The meter therefore says
/// `tahmin` or `ölçüldü` beside the numbers, and never shows a bar at all when
/// the window is unknown — which is the ordinary case for a cloud endpoint,
/// because no vendor's model listing reports it (`ai/provider.hpp`,
/// `ContextSource`). A meter that invented a denominator would be inventing the
/// one number the user is deciding on.
class ContextMeter : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a meter reading zero of an unknown window.
    explicit ContextMeter(QWidget* parent = nullptr);

    /// `used` tokens of `window` (0 = unknown), `measured` saying whether the
    /// provider reported the number or this program estimated it.
    void setUsage(qint64 used, qint64 window, bool measured);

    /// The line the meter is showing, for a tooltip and for the probe.
    QString caption() const;

    void applyTheme(ThemeMode mode) override;
    QSize sizeHint() const override;

protected:
    /// Draws the line, and the bar only when the window is known.
    void paintEvent(QPaintEvent* event) override;

private:
    qint64 used_{0};
    qint64 window_{0};
    bool measured_{false};
    ThemeMode theme_{ThemeMode::Dark};
};

/// `12.4 b` / `128 b` — a token count in Turkish, as the meter prints it.
/// `b` is `bin`, and a count under a thousand is printed whole.
QString formatTokenCount(qint64 tokens);

/// `248 KB`, `1.4 MB` — a file size in Turkish, as an attachment chip prints it.
QString formatByteCount(qint64 bytes);

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
