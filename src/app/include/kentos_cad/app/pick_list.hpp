// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: which of the things under the cursor did you mean?
//
// WHY IT EXISTS. A click on a cadastral sheet lands on a parcel, on the boundary
// that closes it and on the ada boundary drawn over that, all at once.
// `core::pick_nearest` answers with one of the three — correctly, by distance and
// then by slot — and the user has no way to say they meant one of the other two.
// On a plan sheet that is most clicks.
//
// WHAT IT IS NOT. It is not a second selection engine. The window READS the
// document to describe the candidates and then sends `SEÇ`, exactly the line the
// command line would send, so the choice is a command and not a gesture (Article
// 1.2, 5.9). The candidate list itself comes from `core::pick_all`, which
// `SEÇ mod=NOKTA sira=` also reads — so a script can reach past the top object
// without a mouse (CLAUDE.md 5.15).
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/core/identity.hpp"

#include <vector>

#include <QString>

class QTableWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The chooser. Modal, keyboard-first, and it closes on Esc.
class PickList : public DialogFrame
{
    Q_OBJECT

public:
    /// Builds the list over the entities `candidates` names, nearest first.
    ///
    /// `candidates` are SLOTS, because that is what `core::pick_all` returns and
    /// what the document is read through; the key each row sends to `SEÇ` is
    /// resolved here, once, at construction (model.md R1/R5).
    PickList(Controller& controller, const std::vector<core::EntityId>& candidates,
             QWidget* parent = nullptr);

    void applyTheme(ThemeMode mode) override;

    /// The row the user settled on, or `None` when they did not. Read after
    /// `exec()` returns; an ACCESSOR and not a signal because the window is modal
    /// and the caller is standing right there.
    core::EntityKey picked() const noexcept { return picked_; }

signals:
    /// The row the user is on. Emitted while they move through the list, not only
    /// when they confirm: the point of the window is to SEE which one is meant,
    /// and the canvas behind it is where that is visible.
    void highlighted(core::EntityKey key);

private:
    /// Fills the table from the document. Called once; the window is modal, so
    /// nothing can change under it.
    void build(const std::vector<core::EntityId>& candidates);

    /// The key on the current row, or `None` when there is no current row.
    core::EntityKey currentKey() const;

    Controller& controller_;
    QTableWidget* table_{nullptr};
    core::EntityKey picked_{core::EntityKey::None};
    std::vector<core::EntityKey> keys_;
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
