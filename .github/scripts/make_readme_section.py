#!/usr/bin/env python3
"""Render the GitHub Pages section of README.md as an HTML fragment.

Usage: make_readme_section.py <README.md> <out.html>

README.md is the single source of truth for the prose on the published site.
Everything between the two marker comments

    <!-- github-pages:start -->
    <!-- github-pages:end -->

is rendered to HTML here and handed to make_download_page.py, which wraps it in
the page shell (title, styling, download table, footer). Both delimiters are
required, exactly once, start before end, and the section must not be empty: a
missing or mangled marker is a build failure, not a quietly shorter page.

Markdown handling is deliberately small and *safe*: README content is data,
never trusted markup. Every character is HTML-escaped and only the constructs
below are interpreted, so an accidental `<script>` in the README shows up as
text instead of running on the site.

    headings, fenced code blocks, paragraphs, horizontal rules,
    unordered lists (nested), code spans, **strong**, *em*,
    [links](url), <autolinks>, ![images](src)

The single exception to "no raw HTML" is INLINE_TAGS: a fixed allowlist of
tags that are passed through as markup. It exists because the README styles its
keyboard shortcuts with <kbd>…</kbd> and escaped tags would be unreadable.

Relative links and images cannot resolve on the published site (only the
generated index.html is published, no sources are), so they are rewritten to
the repository on GitHub. Set GITHUB_REPOSITORY to override the fallback.
"""

from __future__ import annotations

import html
import os
import re
import sys
from pathlib import Path

START = "<!-- github-pages:start -->"
END = "<!-- github-pages:end -->"

REPO = os.environ.get("GITHUB_REPOSITORY") or "SnoutFirst/QtWatchAllVideos"

# Inline tags copied verbatim into the output (their *contents* stay escaped).
INLINE_TAGS = ("kbd", "br")

_ESCAPES = {"&": "&amp;", "<": "&lt;", ">": "&gt;"}


class SectionError(Exception):
    """The README does not delimit its GitHub Pages section correctly."""


def _escape(text: str) -> str:
    return "".join(_ESCAPES.get(char, char) for char in text)


def _marker_lines(text: str, marker: str) -> list[int]:
    return [number for number, line in enumerate(text.splitlines(), 1) if marker in line]


# --- inline rendering -------------------------------------------------------


def _resolve(url: str, *, image: bool = False) -> str:
    """Point a relative README URL at GitHub, pass absolute ones through."""
    if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", url) or url.startswith(("#", "/")):
        return url
    path = url.lstrip("./") or url
    if image:
        return f"https://raw.githubusercontent.com/{REPO}/HEAD/{path}"
    return f"https://github.com/{REPO}/blob/HEAD/{path}"


def render_inline(text: str) -> str:
    """Convert one run of Markdown inline syntax to escaped HTML."""
    stash: list[str] = []

    def keep(fragment: str) -> str:
        stash.append(fragment)
        return f"\x00{len(stash) - 1}\x00"

    # Code spans first: their content is literal, so nothing else may touch it.
    def code(match: re.Match[str]) -> str:
        return keep(f"<code>{_escape(match.group(1))}</code>")

    text = re.sub(r"`([^`\n]+)`", code, text)

    # Autolinks and the pass-through allowlist, both before escaping.
    def autolink(match: re.Match[str]) -> str:
        url = match.group(1)
        return keep(f'<a href="{html.escape(url, quote=True)}">{_escape(url)}</a>')

    text = re.sub(r"<((?:https?|ftp)://[^>\s]+)>", autolink, text)
    text = re.sub(
        r"</?(?:%s)\s*/?>" % "|".join(INLINE_TAGS), lambda m: keep(m.group(0)), text
    )

    text = _escape(text)

    # Images before links: "![alt](src)" also looks like "[alt](src)".
    def image(match: re.Match[str]) -> str:
        url = _resolve(html.unescape(match.group(2)), image=True)
        return keep(
            f'<img src="{html.escape(url, quote=True)}" '
            f'alt="{html.escape(html.unescape(match.group(1)), quote=True)}" loading="lazy">'
        )

    def link(match: re.Match[str]) -> str:
        url = _resolve(html.unescape(match.group(2)))
        return keep(
            f'<a href="{html.escape(url, quote=True)}">{match.group(1)}</a>'
        )

    text = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)", image, text)
    text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", link, text)

    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"\*([^*]+)\*", r"<em>\1</em>", text)

    # Restore in reverse so nested stashes (a link label holding a code span)
    # resolve in one pass.
    while "\x00" in text:
        text = re.sub(r"\x00(\d+)\x00", lambda m: stash[int(m.group(1))], text)
    return text


# --- block rendering --------------------------------------------------------


class Item:
    __slots__ = ("text", "children")

    def __init__(self, text: str) -> None:
        self.text = [text]
        self.children: list[Item] = []


LIST_MARKER = re.compile(r"^(\s*)[-*+]\s+(.*)$")


def _render_list(lines: list[str]) -> str:
    roots: list[Item] = []
    stack: list[tuple[int, Item]] = []

    for line in lines:
        marker = LIST_MARKER.match(line)
        if marker:
            indent, body = len(marker.group(1)), marker.group(2)
            while stack and stack[-1][0] >= indent:
                stack.pop()
            item = Item(body)
            if stack:
                stack[-1][1].children.append(item)
            else:
                roots.append(item)
            stack.append((indent, item))
        elif stack:
            # Continuation of the item above ("  the rest of the sentence").
            stack[-1][1].text.append(line.strip())

    def render(items: list[Item], depth: int) -> list[str]:
        pad = "  " * depth
        out = [f"{pad}<ul>"]
        for item in items:
            body = render_inline(" ".join(item.text))
            if item.children:
                out.append(f"{pad}  <li>{body}")
                out.extend(render(item.children, depth + 2))
                out.append(f"{pad}  </li>")
            else:
                out.append(f"{pad}  <li>{body}</li>")
        out.append(f"{pad}</ul>")
        return out

    return "\n".join(render(roots, 0))


def render_markdown(markdown: str) -> str:
    lines = markdown.splitlines()
    out: list[str] = []
    paragraph: list[str] = []
    lists: list[str] = []
    fence: list[str] | None = None

    def flush_paragraph() -> None:
        if paragraph:
            out.append(f"<p>{render_inline(' '.join(paragraph))}</p>")
            paragraph.clear()

    def flush_list() -> None:
        if lists:
            out.append(_render_list(lists))
            lists.clear()

    def flush() -> None:
        flush_paragraph()
        flush_list()

    for line in lines:
        if fence is not None:
            if line.strip().startswith("```"):
                out.append("<pre><code>" + _escape("\n".join(fence)) + "</code></pre>")
                fence = None
            else:
                fence.append(line)
            continue

        stripped = line.strip()
        if stripped.startswith("```"):
            flush()
            fence = []
            continue
        if not stripped:
            flush()
            continue

        heading = re.match(r"^(#{1,6})\s+(.*?)\s*#*$", stripped)
        if heading:
            flush()
            level = len(heading.group(1))
            out.append(f"<h{level}>{render_inline(heading.group(2))}</h{level}>")
            continue
        if re.match(r"^(?:-{3,}|\*{3,}|_{3,})$", stripped):
            flush()
            out.append("<hr>")
            continue
        if LIST_MARKER.match(line):
            flush_paragraph()
            lists.append(line)
            continue
        if lists:
            lists.append(line)
            continue
        paragraph.append(stripped)

    if fence is not None:
        raise SectionError("unclosed ``` code fence in the github-pages section")
    flush()
    return "\n".join(out)


# --- extraction -------------------------------------------------------------


def extract_section(text: str) -> str:
    starts = _marker_lines(text, START)
    ends = _marker_lines(text, END)

    if len(starts) != 1:
        raise SectionError(
            f"expected exactly one {START} line, found {len(starts)}"
            + (f" (lines {', '.join(map(str, starts))})" if starts else "")
        )
    if len(ends) != 1:
        raise SectionError(
            f"expected exactly one {END} line, found {len(ends)}"
            + (f" (lines {', '.join(map(str, ends))})" if ends else "")
        )
    if starts[0] > ends[0]:
        raise SectionError(
            f"{END} (line {ends[0]}) comes before {START} (line {starts[0]})"
        )

    section = "\n".join(text.splitlines()[starts[0] : ends[0] - 1])
    if not section.strip():
        raise SectionError(
            f"the section between line {starts[0]} and line {ends[0]} is empty"
        )
    return section


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2

    source, destination = Path(argv[1]), Path(argv[2])
    try:
        section = extract_section(source.read_text(encoding="utf-8"))
        fragment = render_markdown(section) + "\n"
    except (OSError, SectionError) as error:
        print(f"{source}: {error}", file=sys.stderr)
        return 1

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(fragment, encoding="utf-8")
    print(f"wrote {destination} ({len(section.splitlines())} README lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
