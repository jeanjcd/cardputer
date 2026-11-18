# SPDX-License-Identifier: GPL-3.0-or-later
"""Check relative links in the project's Markdown files.

Scans README*.md and docs/**/*.md (relative to the project folder), and for
every inline link or image whose target is a relative path:
  - the target file or folder must exist;
  - if the target is a Markdown file with a #fragment (or the link is a
    same-file #fragment), a heading with that GitHub-style anchor must exist.
External links (http, https, mailto) are ignored. Fenced code blocks and
inline code are skipped. Exit code 1 if anything is broken.

Usage: python tools/check_links.py
"""
import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parent.parent
LINK = re.compile(r"!?\[(?:[^\[\]]|\[[^\]]*\])*\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
FENCE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE = re.compile(r"(`+)(?:(?!\1).)+?\1")


def strip_code(text):
    """Return the lines of text with fenced blocks and inline code blanked."""
    out, in_fence = [], False
    for line in text.splitlines():
        if FENCE.match(line):
            in_fence = not in_fence
            out.append("")
            continue
        out.append("" if in_fence else INLINE_CODE.sub("", line))
    return out


def slug(heading):
    """GitHub-style anchor for a heading's text."""
    h = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", heading)  # links -> text
    h = h.replace("`", "").replace("*", "")
    h = h.strip().lower()
    h = re.sub(r"[^\w\- ]", "", h)
    return h.replace(" ", "-")


_anchor_cache = {}


def anchors(path):
    if path not in _anchor_cache:
        seen, result, in_fence = {}, set(), False
        for line in path.read_text(encoding="utf-8").splitlines():
            if FENCE.match(line):
                in_fence = not in_fence
                continue
            m = None if in_fence else re.match(r"^#{1,6}\s+(.*?)\s*#*\s*$", line)
            if not m:
                continue
            base = slug(m.group(1))
            n = seen.get(base, 0)
            seen[base] = n + 1
            result.add(base if n == 0 else f"{base}-{n}")
        _anchor_cache[path] = result
    return _anchor_cache[path]


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    files = sorted(ROOT.glob("README*.md")) + sorted((ROOT / "docs").rglob("*.md"))
    errors, checked = [], 0
    for md in files:
        for lineno, line in enumerate(strip_code(md.read_text(encoding="utf-8")), 1):
            for m in LINK.finditer(line):
                target = m.group(1)
                if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target):
                    continue  # http:, https:, mailto:, ...
                checked += 1
                path_part, _, frag = target.partition("#")
                dest = (md.parent / unquote(path_part)).resolve() if path_part else md
                where = f"{md.relative_to(ROOT).as_posix()}:{lineno}: {target}"
                if not dest.exists():
                    errors.append(f"{where} -> target does not exist")
                    continue
                if frag and dest.is_file() and dest.suffix == ".md":
                    if unquote(frag).lower() not in anchors(dest):
                        errors.append(f"{where} -> no heading #{frag} in {dest.name}")
    for e in errors:
        print(e)
    print(f"{len(files)} files, {checked} relative links checked, {len(errors)} broken")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
