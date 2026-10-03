// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/probe_env.hpp"

#include <QtGlobal>

namespace piricad::app {

bool probe_environment()
{
    for (const char* probe : {
             "PIRICAD_PRINT_PROBE",     "PIRICAD_LAYOUT_PROBE",  "PIRICAD_SHOT_DIR",
             "PIRICAD_DESIGNER_PROBE",  "PIRICAD_HELP_PROBE",    "PIRICAD_MENU_PROBE",
             "PIRICAD_REACH_PROBE",     "PIRICAD_ANSWER_PROBE",  "PIRICAD_FLYOUT_PROBE",
             "PIRICAD_REALMOUSE_PROBE", "PIRICAD_STRIP_PROBE",   "PIRICAD_WIDGETS_PROBE",
             "PIRICAD_DIALOG_PROBE",    "PIRICAD_HAND_PROBE",    "PIRICAD_LAYER_PROBE",
             "PIRICAD_PICK_PROBE",      "PIRICAD_TABLE_PROBE",   "PIRICAD_SCHEMA_PROBE",
             "PIRICAD_CHAT_PROBE",      "PIRICAD_TOOL_PROBE",    "PIRICAD_NORMAL_PROBE",
             "PIRICAD_FAMILY_PROBE",    "PIRICAD_BUDGET_PROBE",  "PIRICAD_CLIP_PROBE",
             "PIRICAD_PROBE_LINE",      "PIRICAD_OSCLICK_PROBE", "PIRICAD_ACCESS_PROBE",
             "PIRICAD_PYTHON_PROBE",    "PIRICAD_FIT_PROBE",     "PIRICAD_RIBBON_SHEET",
             "PIRICAD_TOOL_DRIVE",      "PIRICAD_REPEAT_PROBE",  "PIRICAD_VIEW_PROBE",
             "PIRICAD_OFFER_PROBE",     "PIRICAD_PROMPT_PROBE",  "PIRICAD_WINDOW_SHOT",
             "PIRICAD_THEME_PROBE",     "PIRICAD_FRAME_DUMP",    "PIRICAD_MCP_PROBE",
             "PIRICAD_EDIT_PROBE",
         })
        if (qEnvironmentVariableIsSet(probe)) return true;
    return false;
}

} // namespace piricad::app
