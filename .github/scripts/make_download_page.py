#!/usr/bin/env python3
"""Build the GitHub Pages index for the packaged QtWatchAllVideos2 binaries.

Usage: make_download_page.py <artifact-dir> <site-dir> <commit-sha> <ref-name>

The archives themselves are far too large for GitHub Pages (git rejects any file
over 100 MB, and these are 150-230 MB with Qt bundled), so they live as assets of
the rolling "ci-latest" GitHub Release and this page only links to them. That
keeps the published site a few kilobytes.

Artifact names look like

    QtWatchAllVideos2-1a2b3c4-linux-x86_64.AppImage
    QtWatchAllVideos2-1a2b3c4-macos-arm64.zip
    QtWatchAllVideos2-1a2b3c4-windows-x64.zip

(the "-<sha>" part is stripped before the release upload, so the download URLs
are stable across builds).
"""

from __future__ import annotations

import html
import os
import sys
from pathlib import Path

# GitHub Actions sets GITHUB_REPOSITORY; the literal is only a fallback for a
# manual run, so renaming the repository does not need a code change.
REPO = os.environ.get("GITHUB_REPOSITORY") or "SnoutFirst/QtWatchAllVideos"
RELEASE_TAG = "ci-latest"

PLATFORM_ORDER = {"windows": 0, "macos": 1, "linux": 2}
PLATFORM_LABEL = {"windows": "Windows", "macos": "macOS", "linux": "Linux"}
PLATFORM_NOTE = {
    "windows": "Unpack the .zip anywhere and run QtWatchAllVideos2.exe.",
    "macos": "Unpack the .zip and drag QtWatchAllVideos2.app wherever you like. "
    "It is unsigned, so the first launch needs right-click \u2192 Open "
    "(or <code>xattr -dr com.apple.quarantine QtWatchAllVideos2.app</code>).",
    "linux": "Make the AppImage executable and run it; nothing needs to be installed.",
}

PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>QtWatchAllVideos2 downloads</title>
<style>
  :root {{ color-scheme: dark; }}
  body {{
    margin: 0 auto; padding: 2rem 1.25rem 4rem; max-width: 46rem;
    background: #111; color: #e8e8e8;
    font: 16px/1.55 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  }}
  h1 {{ font-size: 1.7rem; margin: 0 0 .25rem; letter-spacing: -.01em; }}
  p.lead {{ color: #a8a8a8; margin-top: 0; }}
  table {{ width: 100%; border-collapse: collapse; margin: 1.75rem 0 1rem; }}
  th, td {{ text-align: left; padding: .7rem .5rem; border-bottom: 1px solid #2a2a2a;
            vertical-align: top; }}
  th {{ font-size: .78rem; text-transform: uppercase; letter-spacing: .06em; color: #9a9a9a; }}
  a {{ color: #7fc8ff; }}
  .file {{ font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: .85rem;
           color: #cfcfcf; word-break: break-all; }}
  .note {{ color: #9a9a9a; font-size: .88rem; }}
  footer {{ margin-top: 2.5rem; color: #7d7d7d; font-size: .85rem; }}
  code {{ background: #1c1c1c; padding: .1rem .3rem; border-radius: 4px; }}
</style>
</head>
<body>
<h1>QtWatchAllVideos2</h1>
<p class="lead">Watch all the videos in a web page, back-to-back, without the site
chrome around them. <a href="https://github.com/{repo}">Source code</a>.</p>

{body}

<footer>
  Built from <code>{ref}</code> at commit <code>{sha}</code> by GitHub Actions.
  The archives are attached to the
  <a href="https://github.com/{repo}/releases/tag/{tag}">rolling CI release</a>
  because GitHub Pages refuses files over 100&nbsp;MB. Older builds are kept as
  workflow artifacts, and tagged versions also get a GitHub Release each.
</footer>
</body>
</html>
"""


def human_size(num: int) -> str:
    value = float(num)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if value < 1024 or unit == "GiB":
            return f"{value:.0f} {unit}" if unit == "B" else f"{value:.1f} {unit}"
        value /= 1024
    return f"{value:.1f} GiB"


def split_name(name: str) -> tuple[str, str]:
    """Return (platform, arch) for an artifact name, or ("", "") if unknown."""
    stem = name.rsplit(".", 1)[0]
    parts = stem.split("-")
    for index, part in enumerate(parts):
        if part in PLATFORM_ORDER:
            arch = parts[index + 1] if index + 1 < len(parts) else ""
            return part, arch
    return "", ""


def main(argv: list[str]) -> int:
    if len(argv) != 5:
        print(__doc__, file=sys.stderr)
        return 2

    artifact_dir, site_dir = Path(argv[1]), Path(argv[2])
    sha, ref = argv[3], argv[4]

    files = sorted(p for p in artifact_dir.rglob("*") if p.is_file())
    if not files:
        print(f"no artifacts found in {artifact_dir}", file=sys.stderr)
        return 1

    rows = []
    for path in files:
        platform, arch = split_name(path.name)
        if not platform:
            print(f"skipping unrecognised artifact {path.name}", file=sys.stderr)
            continue
        rows.append((PLATFORM_ORDER[platform], platform, arch, path))

    if not rows:
        print("no recognised artifacts to publish", file=sys.stderr)
        return 1

    rows.sort(key=lambda row: row[0])
    download_base = f"https://github.com/{REPO}/releases/download/{RELEASE_TAG}"
    table_rows = "\n".join(
        "  <tr><td><strong>{label}</strong><br><span class=\"note\">{arch}</span></td>"
        "<td><a class=\"file\" href=\"{url}\">{name}</a><br>"
        "<span class=\"note\">{size}</span></td>"
        "<td class=\"note\">{note}</td></tr>".format(
            label=PLATFORM_LABEL[platform],
            arch=html.escape(arch),
            url=html.escape(f"{download_base}/{path.name}"),
            name=html.escape(path.name),
            size=human_size(path.stat().st_size),
            note=PLATFORM_NOTE[platform],
        )
        for _, platform, arch, path in rows
    )

    body = (
        "<table>\n"
        "  <tr><th>Platform</th><th>Download</th><th>Notes</th></tr>\n"
        f"{table_rows}\n"
        "</table>"
    )

    site_dir.mkdir(parents=True, exist_ok=True)
    (site_dir / "index.html").write_text(
        PAGE.format(body=body, ref=html.escape(ref), sha=html.escape(sha[:7]), repo=REPO, tag=RELEASE_TAG),
        encoding="utf-8",
    )
    print(f"wrote {site_dir / 'index.html'} with {len(rows)} download(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
