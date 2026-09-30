// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: a caption's words filled from the object it names.
//
// THE SUBSTITUTION LIVES IN `core` (core/text_fields.hpp) since the picture
// fills fields too — a block's member caption naming `{no}` is drawn with its
// reference's number (TODOS C-13). ETİKET, a caption that follows its object
// and the processing tools keep this spelling of it.
#pragma once

#include "piricad/core/text_fields.hpp"

namespace piricad::command {

using core::FieldFormat;
using core::fill_fields;
using core::has_fields;

} // namespace piricad::command
