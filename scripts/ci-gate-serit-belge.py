#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""GATE: every `Sekme ▸ Panel [▸ Düğme]` path the manual names is a place on the ribbon.

WHAT IT GUARDS (`.claude/ui.md` R46, R55, and the Definition of Done). A manual that
tells the reader to press `Değiştir ▸ Köşe ▸ Pah` when the button moved to the arrow of
`Yuvarla` is a manual that sends people looking for something that is not there, and
the ribbon was reorganised once already in this project's life, and the manual names
some two hundred and fifty such paths. They cannot be kept right by memory.

THE RIBBON'S OWN MAP IS THE TRUTH. `PIRICAD_RIBBON_SHEET` prints one `[şerit] yol:`
line per panel — tab, panel, every button — read out of the real window, never out of
a table kept beside the code (CLAUDE.md 5.10). This runs the program offscreen once,
reads that map, and checks every path under /docs against it.

WHAT COUNTS AS A PATH. A tab name followed by ` ▸ `. A path is VALID when it is one
of the map's — `Tab ▸ Panel`, `Tab ▸ Panel ▸ Button` or `Tab ▸ Button` — or one of the
Araçlar panel's own tree (`Analiz ▸ Tampon bölge`, from docs/islem/README.md, whose
group names collide with a tab's). Valid paths are masked first, longest first, so
`Çizim ▸ Çizgi ve Eğri ▸ Yay` is not also read as the tab `Eğri` followed by `Yay`; what
is left of a ` ▸ ` after that is a path that does not exist. A suffix after a name is
fine (`Yay'a`, `Alan Üret'e`), a wrapped line is joined, link text is skipped
(`[Yazı ▸ Genişlik](…)` is a page's heading, not a button) and so are the generated
files, which carry no ribbon paths.
"""
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def binary():
    for aday in ("build/dev/bin/piricad", "build/release/bin/piricad", "build/debug/bin/piricad"):
        yol = os.path.join(ROOT, aday)
        if os.access(yol, os.X_OK):
            return yol
    return None


def ribbon_map(exe):
    """{tab: {panel: [button, ...]}} from the probe's `[şerit] yol:` lines."""
    with tempfile.TemporaryDirectory() as tmp:
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                   PIRICAD_DATA=os.path.join(ROOT, "data"), PIRICAD_RIBBON_SHEET=tmp)
        run = subprocess.run([exe], env=env, capture_output=True, text=True, timeout=300)
    tabs = {}
    for line in run.stdout.splitlines():
        m = re.match(r"\[şerit\] yol: (.+?) ▸ (.+?): ?(.*)$", line)
        if not m:
            continue
        tab, panel, items = m.groups()
        buttons = [re.sub(r"^[▣·] ", "", x.strip()) for x in items.split(", ") if x.strip()]
        tabs.setdefault(tab, {})[panel] = buttons
    return tabs


def valid_paths(tabs):
    out = set()
    for tab, panels in tabs.items():
        for panel, buttons in panels.items():
            out.add(f"{tab} ▸ {panel}")
            for b in buttons:
                for name in {b, b.rstrip("….")}:
                    out.add(f"{tab} ▸ {panel} ▸ {name}")
                    out.add(f"{tab} ▸ {name}")
    # The Araçlar panel's tree: `| Grup | Araç | ...` rows of the processing manual.
    readme = os.path.join(ROOT, "docs", "islem", "README.md")
    if os.path.exists(readme):
        for row in open(readme, encoding="utf-8"):
            cells = [c.strip().strip("*") for c in row.strip().strip("|").split("|")]
            if len(cells) >= 3 and cells[0] and cells[1] and not set(cells[0]) <= set("-: "):
                out.add(f"{cells[0]} ▸ {cells[1]}")
                out.add(f"{cells[0]} ▸ {cells[1].lower()}")
    return out


def paragraphs(text):
    """(first line number, text with whitespace collapsed) for every paragraph."""
    line, block, start = 1, [], 1
    for raw in text.split("\n"):
        if raw.strip():
            if not block:
                start = line
            block.append(raw)
        elif block:
            yield start, " ".join(" ".join(block).split())
            block = []
        line += 1
    if block:
        yield start, " ".join(" ".join(block).split())


def main():
    exe = binary()
    if exe is None:
        print("serit-belge: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)")
        return 0

    tabs = ribbon_map(exe)
    if sum(len(p) for p in tabs.values()) < 60:
        print("serit-belge: şerit sondası haritayı yazmadı (panel sayısı "
              f"{sum(len(p) for p in tabs.values())}) — şerit sondası çalışmıyor", file=sys.stderr)
        return 1

    valid = sorted(valid_paths(tabs), key=len, reverse=True)
    tab_rx = re.compile(r"(?<![\w\x00])(" + "|".join(re.escape(t) for t in
                                                      sorted(tabs, key=len, reverse=True)) + r") ▸ ")
    link_rx = re.compile(r"\[[^\]]*▸[^\]]*\]\([^)]*\)")

    bad = []
    checked = 0
    for dosya in sorted(glob.glob(os.path.join(ROOT, "docs", "**", "*.md"), recursive=True)):
        ad = os.path.basename(dosya)
        if ad == "referans.md" or ad.startswith("llms"):
            continue
        for first, para in paragraphs(open(dosya, encoding="utf-8").read()):
            para = link_rx.sub(lambda m: " " * len(m.group(0)), para)
            para = para.replace("**", "").replace("`", "")
            if " ▸ " not in para:
                continue
            for yol in valid:
                if yol in para:
                    para = para.replace(yol, "\x00" * len(yol))
            for m in tab_rx.finditer(para):
                checked += 1
                bad.append((os.path.relpath(dosya, ROOT), first, para[m.start():m.start() + 70]))

    if bad:
        print("serit-belge: kılavuzda şeritte olmayan yollar var (harita: "
              "PIRICAD_RIBBON_SHEET çıktısındaki `[şerit] yol:` satırları):", file=sys.stderr)
        for dosya, satir, parca in bad:
            print(f"  {dosya}:{satir}: {parca}", file=sys.stderr)
        return 1

    print(f"serit-belge: OK — kılavuzdaki her Sekme ▸ Panel ▸ Düğme yolu şeritte var "
          f"({len(tabs)} sekme, {sum(len(p) for p in tabs.values())} panel)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
