// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: what the editor knows about `kentos.cad`.
//
// A HEADER OF ITS OWN because two things need these shapes and neither should
// pull the other in: the controller PRODUCES them from the registry, and the
// editor CONSUMES them for completion and the signature hint. Putting them on
// `Controller` would make the editor include the whole shell; putting them in
// the editor would make the controller include `QPlainTextEdit`.
//
// PROJECTED, NEVER WRITTEN. These carry exactly what `Registry` declares, with
// the type spelled by `script::python_type_name` — the same function the
// generated reference and the generated stub use. An editor that described a
// different surface from the one that runs would be worse than an editor with no
// hints at all (CLAUDE.md 5.10, 5.20).
#pragma once

#include <QString>
#include <QVector>

namespace kentos::app {

/// One keyword of one Python callable.
struct PythonArg
{
    QString name; ///< the English keyword, which is what the user types
    QString type; ///< `list[list[int]]`, `float`, `str`
    QString help; ///< the Turkish sentence the parameter declares
    QString unit; ///< what the number is measured in, empty when none
};

/// One `cad.<name>(...)` callable.
struct PythonCallable
{
    QString name;            ///< `line`, `circle_draw`, `label_length`
    QString command;         ///< `core.line` — what the journal will record
    QString turkish;         ///< `ÇİZGİ` — what the same act is called at the prompt
    QString summary;         ///< the one Turkish line the command declares
    QVector<PythonArg> args; ///< in declaration order, which is the order shown
};

} // namespace kentos::app
