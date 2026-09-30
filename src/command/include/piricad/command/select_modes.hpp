// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: SEÇ's modes, as a list a shell builds buttons from.
//
// The Seçim prompt tab (`.claude/ui.md` R48a) shows one button per mode a hand
// can start while a command asks for objects. The buttons are read from THE
// table `core.select` parses (commands/pick.cpp), so a mode added there is a
// button the day it is added and the tab never lists a mode SEÇ does not know
// (CLAUDE.md 5.10).
#pragma once

#include <span>
#include <string_view>

namespace kentos::command {

/// One mode of `SEÇ` the Seçim tab offers.
struct SelectModeInfo
{
    std::string_view word;    ///< the canonical mode word a line carries: `ÇİT`
    std::string_view label;   ///< the button's name: `Çit`
    int points{0};            ///< clicks it takes: 0 none, n exactly n, -1 a run Enter ends
    bool adds{false};         ///< whether the tab adds what it finds (`islem=EKLE`)
    std::string_view summary; ///< one line, Turkish: what it takes and how
};

/// Every mode the Seçim tab offers, in the order it shows them.
std::span<const SelectModeInfo> select_modes();

} // namespace kentos::command
