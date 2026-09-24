// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the pieces of the ribbon that are this program's own.
//
// THE RIBBON IS SARIBBON'S (`.claude/ui.md` R46): its tab bar, its panels, its
// buttons, its galleries and its contextual tabs. What lives here is what the
// library does not know about:
//
//   * a FAMILY of tools behind one split button;
//   * the LIVE BOXES — the layer the hand is on and the colours in hand — which
//     read the document and write through a command, the way AutoCAD's layer
//     and colour lists do (R47);
//   * the pictures the ribbon draws for things that are DATA rather than
//     glyphs: a hatch pattern from the catalogue, the nine text anchors;
//   * the rich tip every ribbon button shows;
//   * the corner of the tab row, with the command search and the user chip.
//
// None of it holds logic of its own: every button, box and gallery item runs a
// command line (CLAUDE.md Article 1.2), and the ribbon buys the GUI no privilege
// over any other client.
#pragma once

#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/core/identity.hpp"

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <QColor>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

class QAction;
class QMenu;
class SARibbonToolButton;

// SARibbon's element factory, which this program subclasses below.
#include "SARibbon.h"

namespace kentos::core {
/// The drawing `ribbon_context_of` reads an object's kind and caption from.
class Document;
} // namespace kentos::core

namespace kentos::app {

/// The dynamic property every tool action carries: the command word it
/// dispatches (or the whole line, for a method). Declared once, because the
/// button that runs a tool and everything that names it — the rich tip, the
/// registry's "already placed" walk, the probes — must never be able to
/// disagree about which command it is (CLAUDE.md 5.10).
inline constexpr const char* kToolCommandProperty = "piricad.command";

/// THE RIBBON'S BUTTONS ARE MADE HERE, so each one knows the keyboard — Down or
/// F4 on a button with a list opens it (`.claude/ui.md` R21) — and shows the
/// ribbon's rich tip. The factory makes every button SARibbon builds, so nothing
/// has to be installed on the buttons it happened to make first. Installed once,
/// before the window builds its ribbon.
class RibbonElementFactory : public SARibbonElementFactory
{
public:
    /// A ribbon button that opens its list from the keyboard and shows the rich
    /// tip (`ribbon.cpp`).
    SARibbonToolButton* createRibbonToolButton(QWidget* parent) override;

    /// Where the tip of a button carrying an action comes from: the window
    /// composes it from the action and the registry (`MainWindow::ribbonTip`).
    /// Empty falls back to the action's own plain tooltip.
    static void setTipSource(std::function<QString(const QAction*)> source);

    /// The rich tip for `action`, or empty when no source is set.
    static QString tipFor(const QAction* action);
};

/// THE BAR ITSELF, SARibbon's with one thing drawn differently: an editor tab
/// wears a thin cap of its colour over the folder tab rather than a block of it
/// behind the label. SARibbon fills the whole context rectangle, which behind
/// a transparent tab put the word on a slab of saturated colour; the cap says
/// "this tab is about what you selected" and leaves the word on the tab row's
/// own ground (`design.md` §1.2: saturated colour is for meaning, sparingly).
class RibbonBar : public SARibbonBar
{
    Q_OBJECT

public:
    /// Builds the bar; the window installs it before building its ribbon.
    explicit RibbonBar(QWidget* parent = nullptr);

protected:
    /// The cap over an editor tab, in `color`; `title` is not drawn — the tab
    /// row has no room above the tabs in the compact layout.
    void paintContextCategoryTab(QPainter& painter, const QString& title, const QRect& contextRect,
                                 const QColor& color) override;
};

/// `KENTOS CAD`, the application button, AS WIDE AS ITS NAME. SARibbon's compact
/// layout scales a button's hint to the title row's height at a 1 : 1,5 aspect
/// (`scaleSizeByHeight`), so a taller row made a wider button; the hint this
/// returns is the one that scaling maps back onto the label's own width.
class RibbonAppButton : public SARibbonApplicationButton
{
    Q_OBJECT

public:
    RibbonAppButton(const QString& text, QWidget* parent);

    QSize sizeHint() const override;
};

/// The two chips of the tab row's corner, painted in `ribbon.cpp`.
class SearchChip;
class UserChip;

/// The dynamic property a family member carries when the family's button should
/// wear a SHORT word of its own while that member is on its face: `Uzat` on the
/// Buda button, `Pah` on the Yuvarla button — a menu row may say "Uzat — çit
/// ile", a 26 px ribbon row may not.
inline constexpr const char* kRibbonShortLabel = "kentos.ribbon.short";

/// ONE SPLIT BUTTON FOR A FAMILY OF TOOLS — the Daire with its three other ways
/// of drawing a circle, the Buda with its fence and its carried edges.
///
/// The ribbon button carries `head()`: a click runs the member used last, the
/// arrow lists every member. The face follows the member last chosen — its
/// label, its icon, its tip — and the head reads as pressed while any member
/// runs, so a family whose variant is active never looks idle.
///
/// The head is a PROXY. It carries no command of its own (no `kToolCommand`):
/// the members do, so the tool probe, the reach probe and the registry's
/// "already placed" walk count each member once and the head not at all.
class RibbonFamily : public QObject
{
    Q_OBJECT

public:
    /// Builds the family over `members`, the first of them the face. Owned by
    /// `parent`; the members stay owned by whoever made them.
    RibbonFamily(const QList<QAction*>& members, QObject* parent);

    /// The action the ribbon button carries, with the members' menu on it.
    QAction* head() const noexcept { return head_; }

    /// Every member, in the order the menu lists them.
    const QList<QAction*>& members() const noexcept { return members_; }

    /// The member a click on the face runs.
    QAction* face() const noexcept { return face_; }

    /// Keeps the label the ribbon gave the button when a member becomes the
    /// face: the icon and the tip still follow the member, the word does not.
    /// For a family shown as ONE word on the `Giriş` tab — `Daire`, `Buda` —
    /// whose members are named by their method.
    void setFixedLabel(const QString& label);

private:
    /// Makes `member` the face and copies what the button shows from it.
    void adopt(QAction* member);

    /// Lights the head while any member runs.
    void syncChecked();

    QAction* head_{nullptr};
    QMenu* menu_{nullptr};
    QAction* face_{nullptr};
    QList<QAction*> members_;
    QString fixedLabel_;
};

/// One row of the layer list: what the layer box shows for a layer.
struct RibbonLayerRow
{
    QString name;       ///< as the user typed it
    QColor colour;      ///< the layer's stroke, as the scene draws it
    bool visible{true}; ///< drawn at all
    bool locked{false}; ///< drawn but not selectable
};

/// THE LAYER THE HAND IS ON, in the ribbon (`.claude/ui.md` R47).
///
/// AutoCAD's layer list, and it behaves like it: with nothing selected it reads
/// the ACTIVE layer and a pick makes another one active (`KATMAN ad=`); with a
/// selection it reads the selection's layer — "farklı katmanlar" when they
/// differ — and a pick MOVES the selection there (`KATMANAT`). Each row wears
/// the layer's colour and says when the layer is hidden or locked. The window
/// decides which of the two lines to run; the box only says what was picked.
class RibbonLayerBox : public ComboBox
{
    Q_OBJECT

public:
    /// Builds an empty box at the compact height.
    explicit RibbonLayerBox(QWidget* parent = nullptr);

    /// Fills the list. `shown` is the row the box reads; -1 prints `mixed`
    /// in its place, for a selection that spans several layers.
    void setRows(const QVector<RibbonLayerRow>& rows, int shown, const QString& mixed);

    void applyTheme(ThemeMode mode) override;

signals:
    /// The user chose the layer named `name` from the list.
    void layerPicked(const QString& name);

private:
    /// Draws each row's marks again, in the theme's inks.
    void repaintMarks();

    QVector<RibbonLayerRow> rows_;
    ThemeMode mode_{ThemeMode::Dark};
};

/// A COLOUR IN HAND, read like a list and opened like one: the swatch and the
/// colour's name, and the chevron. Opening it does not drop a list — it asks the
/// window for the colour menu (`menuRequested`), whose swatches are `RENK`'s
/// own words, so what the box offers is exactly what the command paints.
class RibbonColourBox : public ComboBox
{
    Q_OBJECT

public:
    /// Builds the box at the compact height.
    explicit RibbonColourBox(QWidget* parent = nullptr);

    /// What the box shows: `colour` (invalid for none) and the word under it.
    void setShown(const QColor& colour, const QString& label);

    /// The colour the box shows; invalid for none.
    QColor shownColour() const { return colour_; }

    /// Asks for the colour menu instead of dropping the list.
    void showPopup() override;

    void applyTheme(ThemeMode mode) override;

signals:
    /// The box was opened, by the mouse or the keyboard.
    void menuRequested();

private:
    void repaintSwatch();

    QColor colour_;
    QString label_;
    ThemeMode mode_{ThemeMode::Dark};
};

/// A HATCH PATTERN AS IT WILL LOOK: its line families from the catalogue, drawn
/// into a `size` swatch in `ink` over `ground`, at a scale chosen per pattern so
/// that its lines are a few pixels apart — the catalogue's own numbers, never a
/// picture of them (CLAUDE.md 5.13: the pattern is data).
QIcon hatch_swatch(const command::HatchPattern& pattern, const QColor& ink, const QColor& ground,
                   QSize size);

/// THE NINE TEXT ANCHORS as a 3 × 3 picture: three text lines set left, centre
/// or right by `column`, and a mark at the top, the middle or the baseline by
/// `row` — where the point sits on the text.
QIcon anchor_icon(int column, int row, const QColor& ink, const QColor& mark);

/// The editor tabs a selection brings up, in the order `RibbonLive::contexts`
/// holds them (`.claude/ui.md` R48).
enum class RibbonContext : std::uint8_t {
    Text,      ///< a caption: its height, spacing, anchor and words
    Dimension, ///< a drawn dimension: its style, decimals and unit
    Hatch,     ///< a hatch: its pattern, angle, scale and islands
    Area,      ///< a closed face: the survey and parcel work done on one
    Block,     ///< a placed block
};
/// How many editor tabs there are.
inline constexpr std::size_t kRibbonContextCount = 5;

/// The editor tab object `e` of `doc` belongs to, if any: what brings a tab
/// up for a selection and what a double click on the object opens.
std::optional<RibbonContext> ribbon_context_of(const core::Document& doc, core::EntityId e);

/// EVERYTHING ON THE RIBBON THAT READS THE DOCUMENT (`.claude/ui.md` R47, R48):
/// the boxes that say where the next object goes and what colour it wears, the
/// defaults the annotation commands fall back to, and the editor tabs with the
/// controls on them. Refreshed on every document, selection and setting change
/// (`MainWindow::refreshRibbon`), never written to directly: a control's change
/// is a command line, and the document it changes is what the next refresh reads.
struct RibbonLive
{
    RibbonLayerBox* layer{nullptr};   ///< Giriş's layer list
    RibbonColourBox* stroke{nullptr}; ///< the stroke colour in hand
    RibbonColourBox* fill{nullptr};   ///< the fill colour in hand

    ComboBox* textHeightDefault{nullptr}; ///< `AYAR metin_yüksekliği`
    ComboBox* dimStyleDefault{nullptr};   ///< `AYAR ölçü_stili`
    ComboBox* plotScale{nullptr};         ///< `AYAR plan_ölçeği`

    /// The editor tabs, in `RibbonContext` order.
    std::array<SARibbonContextCategory*, kRibbonContextCount> contexts{};
    /// Which of them the selection has up now.
    std::array<bool, kRibbonContextCount> showing{};
    /// What each tab edits its object with, and what a double click on such an
    /// object runs (TODOS C-17): the caption's, the dimension's and the
    /// hatch's own edit. Null where the object has none — an area or a block
    /// goes to the attribute panel instead.
    std::array<QAction*, kRibbonContextCount> editors{};
    /// The tab that was up before an editor tab raised itself, to go back to.
    QPointer<SARibbonCategory> before;

    ComboBox* textHeight{nullptr};
    ComboBox* textSpacing{nullptr};
    QList<QAction*> textAnchors; ///< nine, in `core::TextAnchor` order

    ComboBox* dimStyle{nullptr};
    ComboBox* dimPrecision{nullptr};
    ComboBox* dimUnit{nullptr};

    ComboBox* hatchAngle{nullptr};
    ComboBox* hatchScale{nullptr};
    QAction* hatchCross{nullptr};
    QList<QAction*> hatchIslands;  ///< normal, dis, yoksay
    QList<QAction*> hatchPatterns; ///< the editor's gallery, one per pattern
    QList<QAction*> drawPatterns;  ///< the Çizim tab's gallery, one per pattern

    /// The catalogues the galleries and lists were built from.
    std::vector<command::HatchPattern> patterns;
    std::vector<command::DimensionStyle> dimStyles;
};

/// The right end of the tab row: the command search and the user's initials —
/// what the old title bar carried at its right end (`design.md` §7). The plot
/// scale and the coordinate system went to the status strip.
class ShellCorner : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the corner.
    explicit ShellCorner(QWidget* parent = nullptr);

    /// The initials in the round chip at the right end.
    void setUserInitials(const QString& initials);

    void applyTheme(ThemeMode mode) override;

signals:
    /// The search chip was clicked.
    void searchRequested();

private:
    SearchChip* search_{nullptr};
    UserChip* user_{nullptr};
};

} // namespace kentos::app
