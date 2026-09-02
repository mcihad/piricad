#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: every public declaration is documented, and every comment is in English.
#
# WHY THIS GATE EXISTS, stated plainly because the rule is unusual.
#
# This codebase is read by people and by models, and a reader of a header has
# nothing but the header. A signature says WHAT a function takes; it never says
# why the shape is that shape, what it refuses, which rule in /.claude it obeys,
# or what went wrong the last time somebody assumed otherwise. That reasoning is
# the expensive part of the work and it is invisible in the type system. A model
# asked to change this code six months from now will reconstruct it from comments
# or it will guess, and guessing in a cadastral program produces a wrong parcel.
#
# So: every class, struct, enum, free function, member function, type alias and
# data member declared in a PUBLIC header is documented, or the build stops.
#
# WHAT COUNTS AS DOCUMENTED
#
#   1. A `///` or `/** */` comment immediately above the declaration.
#   2. A doc comment introducing a contiguous RUN of declarations. Eight related
#      error tokens under one paragraph is better documentation than eight copies
#      of one sentence, and the code already does this.
#   3. A trailing `///<` on the same line — the right shape for a struct field
#      whose explanation is six words.
#
# WHAT IS NOT DOCUMENTATION
#
#   A comment that restates the signature. `/// Returns the name.` above `name()`
#   teaches nothing the reader did not already have, and is worse than silence
#   because it looks like the question was answered. No shell script can judge
#   that, so it is a review rule rather than a check. What the gate CAN refuse is
#   the empty case, and it does.
#
# LANGUAGE (CLAUDE.md 11.9)
#
#   /docs is Turkish and tells a user how to do their work. Code comments are for
#   contributors and are ENGLISH. Turkish appears in a comment only as a QUOTE of
#   something that is Turkish in the product — a command name, a UI label, an
#   error string, a catalogue field — and then it is either ALL CAPS (`TERCİH`,
#   `ÇİZGİ`) or inside backticks or quotes. `easting (sağa değer)` is flagged;
#   ``easting (`sağa değer`)`` is not, and the difference is that the second one
#   is visibly a quotation.
#
# SCOPE: public headers under /src/*/include for coverage; every .cpp and .hpp
# under /src and /tests for language. A .cpp file's internals are not required to
# carry a doc comment on every local lambda — that would produce noise which hides
# the comments that matter.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 - "$root" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])

# ------------------------------------------------------------- coverage ----

headers = sorted(root.glob('src/*/include/**/*.hpp'))
headers += sorted(root.glob('src/*/*/include/**/*.hpp'))

# Deliberately conservative: it would rather miss an exotic construct than block a
# build on a false positive, because a gate that cries wolf gets switched off and
# then nothing is checked at all.
DECLARATION = re.compile(
    r'^\s{0,8}'
    r'(?:\[\[[^\]]*\]\]\s*)?'
    r'(?:template\s*<[^>]*>\s*)?'
    r'(?:(?:inline|static|constexpr|consteval|explicit|virtual|friend|mutable)\s+)*'
    r'(?:(?P<kw>class|struct|enum\s+class|enum|using)\s+)?'
    r'(?P<rest>[A-Za-z_][^;{]*?)'
    r'\s*(?P<tail>[({=;])'
)

SKIP_PREFIX = (
    'namespace', 'using namespace', 'extern "C"', 'public:', 'private:',
    'protected:', 'return', 'friend bool operator', 'friend auto operator',
    'static_assert', '#', 'operator', 'default:', 'case ',
    # Qt's own macros are not declarations. `Q_OBJECT` never matched because it
    # carries no `(`, but `Q_INTERFACES(...)` and `Q_PROPERTY(...)` do — and
    # asking a contributor to write a doc comment above a macro that means
    # "moc, generate the usual" teaches nothing and is noise above every class.
    'Q_OBJECT', 'Q_INTERFACES', 'Q_PROPERTY', 'Q_ENUM', 'Q_FLAG', 'Q_GADGET',
    'Q_DECLARE_', 'Q_INVOKABLE', 'Q_SIGNALS', 'Q_SLOTS',
)


def has_doc(lines, index):
    """True when `index` is covered by a doc comment, directly or by its group."""
    if '///<' in lines[index]:
        return True

    j = index - 1
    siblings = 0
    while j >= 0:
        text = lines[j].strip()

        if text == '':
            j -= 1
            continue
        if text.startswith(('///', '//', '*', '/*')):
            return True

        # A sibling declaration: keep walking, but not forever. A run longer than
        # this is not a documented group, it is an undocumented block with one
        # lucky comment somewhere above it.
        # A `template<...>` line belongs to the declaration below it, not to the
        # run: the doc comment sits above the template head.
        if text.startswith('template<') or text.startswith('template <'):
            j -= 1
            continue

        # So does a return type clang-format put on its own line, for exactly the
        # same reason. `core::Result<command::DispatchResult>` above the name it
        # returns is half of one declaration, not a neighbour of it, and the doc
        # comment sits above the pair. Recognised by being an unfinished
        # statement that opens nothing: no parameter list, no terminator, no
        # brace, no label.
        if '(' not in text and not text.endswith((';', ',', '{', '}', ')', ':')):
            j -= 1
            continue

        if siblings < 24 and text.endswith((';', ',', '}')):
            siblings += 1
            j -= 1
            continue

        return False
    return False


checked = 0
undocumented = []

for header in headers:
    lines = header.read_text(encoding='utf-8').split('\n')
    in_private = False
    private_depth = None
    brace = 0
    body_depth = None    # brace depth at which the current function body started
    body_pending = False # a definition was seen; its body opens at the next brace

    for i, raw in enumerate(lines):
        text = raw.strip()
        opened = raw.count('{') - raw.count('}')

        # Statements inside a function body are not declarations. Without this the
        # gate demanded a doc comment on every `if` inside every accessor, which is
        # noise that would have taught the reader to ignore the gate.
        if body_depth is not None:
            brace += opened
            if brace <= body_depth:
                body_depth = None
            continue

        # A definition's brace may be on its own line, which is this project's
        # style for anything longer than one statement.
        if body_pending and '{' in raw:
            body_depth = brace
            brace += opened
            body_pending = False
            if brace <= body_depth:
                body_depth = None
            continue

        if text.startswith('private:'):
            in_private, private_depth = True, brace
        elif text.startswith(('public:', 'protected:')):
            in_private, private_depth = False, None

        brace += opened
        if in_private and private_depth is not None and brace < private_depth:
            in_private, private_depth = False, None

        if in_private or not text or text[0] in '/*}#':
            continue
        if any(text.startswith(p) for p in SKIP_PREFIX):
            continue
        # `using core::Point2;` imports a name; it declares nothing of ours.
        if re.match(r'^using\s+[A-Za-z_]\w*::', text):
            continue
        # An enumerator inside an enum body, which the enum's own comment covers.
        if re.match(r'^[A-Za-z_]\w*\s*(=\s*[^;,]*)?,\s*(//.*)?$', text):
            continue

        match = DECLARATION.match(raw)
        if not match:
            continue
        if match.group('kw') is None and '(' not in text and '=' not in text and \
           not text.endswith(';'):
            continue

        checked += 1
        if not has_doc(lines, i):
            undocumented.append((header.relative_to(root), i + 1, text[:88]))

        # A type body holds declarations and is walked into; a function body holds
        # statements and is skipped.
        if match.group('kw') in ('class', 'struct', 'enum', 'enum class', 'using'):
            continue

        if opened > 0:
            body_depth = brace - opened
        elif '(' in text and not text.endswith(';'):
            # A definition whose brace is on the next line. `(` rather than the
            # regex's `tail`, because for `operator=(...)` the first punctuation
            # the pattern reaches is the `=` of the operator's own name.
            body_pending = True

# ------------------------------------------------------------- language ----

sources = sorted(root.glob('src/**/*.hpp')) + sorted(root.glob('src/**/*.cpp'))
sources += sorted(root.glob('tests/**/*.hpp')) + sorted(root.glob('tests/**/*.cpp'))

# Letters that exist in Turkish and in no language this project writes comments in.
# ç, ö and ü are deliberately absent: they are ordinary in German and French, and
# flagging them would fire on borrowed words rather than on Turkish prose.
TURKISH = re.compile(r'[ğıİĞşŞ]')

# A word is a permitted quotation when it is ALL CAPS — every command name in this
# product is — because a lower-case Turkish word in an English sentence is prose.
ALL_CAPS = re.compile(r'^[A-ZÇĞİÖŞÜ0-9_]+$')

# Box-drawing characters mean this line is a DIAGRAM, not prose. The interface
# sketch at the top of main_window.hpp labels the real widgets, which are Turkish
# because the product is; redrawing it in English would make it a picture of a
# program that does not exist.
DIAGRAM = re.compile(r'[│┌└┐┘─├┤┬┴┼]')

turkish_prose = []

for source in sources:
    quoting = False  # a quotation left open by the previous line

    for i, raw in enumerate(source.read_text(encoding='utf-8').split('\n'), 1):
        text = raw.strip()
        if not text.startswith(('//', '///', '*', '/*')):
            quoting = False
            continue

        if DIAGRAM.search(text):
            continue

        # A quotation may WRAP, with either mark. A backticked term split across
        # two lines is one quotation and so is a block quote from kentoscad.md, and
        # demanding that prose never wrap to keep a checker happy would be the
        # checker dictating the writing.
        body = text.lstrip('/* ')
        opened_here = quoting
        marks = body.count('`') + body.count('"')

        prose = body
        if opened_here:
            # Everything up to the closing mark belongs to the previous line.
            closes = [i for i in (prose.find('`'), prose.find('"')) if i >= 0]
            prose = prose[min(closes) + 1:] if closes else ''

        prose = re.sub(r'`[^`]*`', ' ', prose)
        prose = re.sub(r'"[^"]*"', ' ', prose)
        prose = re.sub(r"'[^']*'", ' ', prose)

        quoting = (marks - (1 if opened_here else 0)) % 2 == 1

        for word in re.findall(r'[^\s.,;:()\[\]/*<>=+-]+', prose):
            if TURKISH.search(word) and not ALL_CAPS.match(word):
                turkish_prose.append((source.relative_to(root), i, text[:88]))
                break

# ------------------------------------------------------------------ report ----

for path, line, text in undocumented:
    print(f'comments: undocumented public declaration -> {path}:{line}  {text}',
          file=sys.stderr)

for path, line, text in turkish_prose:
    print(f'comments: Turkish prose in a comment (CLAUDE.md 11.9) -> {path}:{line}  {text}',
          file=sys.stderr)

if undocumented or turkish_prose:
    print(f'comments: {len(undocumented)} undocumented of {checked} public declarations, '
          f'{len(turkish_prose)} comments not in English', file=sys.stderr)
    sys.exit(1)

print(f'comments: OK — {checked} public declarations documented, every comment in English')
PY
