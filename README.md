# QtWatchAllVideos2

A tiny Qt desktop app that watches **all** the videos in a web page, one after
another, without any of the surrounding site chrome.

Paste a thread URL, click **Watch All Videos**, and it scrapes the page for
video links and `<video>` sources, then plays them back-to-back in a clean black
viewer with a minimal overlay of controls.

## Download

Prebuilt Windows, macOS and Linux binaries are published to GitHub Pages by CI
on every push to `main`:

**<https://snoutfirst.github.io/QtWatchAllVideos2/>**

Each archive is self-contained (Qt bundled with `windeployqt` / `macdeployqt` /
`linuxdeploy`), so nothing needs to be installed first.

## What it's for

Its primary intended usage is browsing **4chan `/gif/`** threads — where a
thread is basically a long list of `.webm` and `.gif` links and you just want to
sit back and let them play.

That said, the extraction is deliberately generic: **it will work on any site
that serves its videos as plain `<video>` / `<source>` elements or as links
ending in a video extension** (`.webm`, `.mp4`, `.gif`, `.mov`, `.mkv`, `.ogg`,
and friends, plus the usual legacy formats).

## What it will *not* do

It does **not** execute a site's player logic. Extraction is done on the static
HTML only, so it **won't work on sites that use fancy JavaScript players to load
their videos** — YouTube is the obvious example, but plenty of others are in the
same boat (anything with blob/MSE streaming, DRM, or an API-driven player).

## CAPTCHAs

It **can** get past a Cloudflare or CAPTCHA interstitial — but not on its own.
The real page loads in the embedded browser first, and you pass the challenge by
hand; the video scan then runs against the page you ended up on. There is no
solver, no cookie farming and no headless trickery: the human does the one bit
that needs a human, and everything after that is automatic.

## Features

- **Watch all videos in order** — every match is collected from the page
  (deduplicated) and played sequentially, advancing automatically when each one
  ends.
- **Play order** — Forward (default), **Shuffled**, or **Reversed**, chosen from
  the drop-down arrow next to the *Watch All Videos* button
  (<kbd>Ctrl</kbd>+<kbd>S</kbd> / <kbd>Ctrl</kbd>+<kbd>R</kbd>), or from the
  URL field's right-click menu.
- **Animated GIF support** — `.gif` files are downloaded and played as animated
  GIFs with proper scaling. A GIF that loops for more than a few seconds is
  treated as "finished" and auto-advances to the next video.
- **Resume where you left off** — recent history is saved **locally** (per
  thread URL, in a `QSettings` INI file under the platform's app-data
  directory), so re-opening a thread picks up at the last video you were
  watching. History entries older than a year are pruned automatically.
- **Minimal, out-of-the-way UI** — the controls panel and the mouse cursor hide
  themselves after 1.5 seconds of inactivity and come back as soon as you move
  the mouse.
- **Single click to pause, double click for fullscreen** — click anywhere on the
  video to pause/resume; double-click (or <kbd>F</kbd>) toggles fullscreen.
- **Keyboard shortcuts**
  - <kbd>Space</kbd> — play / pause
  - <kbd>N</kbd> or <kbd>Enter</kbd> — next video
  - <kbd>P</kbd> — previous video
  - <kbd>←</kbd> — jump back 5 seconds (stops at the start of the video)
  - <kbd>→</kbd> — jump forward 5 seconds (past the end, it moves on to the next
    video)
  - <kbd>M</kbd> — mute / unmute
  - <kbd>F</kbd> — toggle fullscreen
  - <kbd>Esc</kbd> — leave fullscreen
- **Previous / Next buttons** plus a `current / total` counter.
- **Crazy-clicking friendly** — a small race guard keeps an auto-advance and a
  panicked *Next* click from skipping a video. Spamming *Next* still works at
  full speed.
- **Interruptible downloads** — switching videos while a GIF is still
  downloading aborts the pending transfer instead of letting the old one pop up
  later.
- **Remembers your window** — size and position are restored on the next launch
  (maximized on first run).

## Building

The project is built with CMake (≥ 3.29) and needs **Qt 6** with the
**Widgets**, **Network**, **Multimedia**, **MultimediaWidgets** and
**WebEngineWidgets** modules:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Run it with `./build/src/app/QtWatchAllVideos2` (`build\src\app\Release\
QtWatchAllVideos2.exe` for a multi-config build on Windows, or
`build/src/app/QtWatchAllVideos2.app` on macOS).

Installing Qt:

- **Debian / Ubuntu** — `qt6-base-dev qt6-multimedia-dev qt6-webengine-dev`
- **Fedora** — `qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtwebengine-devel`
- **macOS** — `brew install qt` (Homebrew's `qt` formula includes QtWebEngine)
- **Windows** — the Qt online installer, or `aqtinstall`
  (`aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtmultimedia
  qtwebengine qtwebchannel qtpositioning`)

CMake presets for the common compiler/platform combinations live in
`CMakePresets.json`; the template's build options (`QtWatchAllVideos2_ENABLE_*`)
are documented in [README_building.md](README_building.md) and
[README_design.md](README_design.md).

## Continuous integration and releases

`.github/workflows/ci.yml` builds and tests Debug + Release on
`ubuntu-latest` (GCC and Clang), `macos-latest` (Apple Clang) and
`windows-latest` (MSVC), then packages each binary:

| Platform | Packaged by | Archive |
| --- | --- | --- |
| Linux | `linuxdeploy` + `linuxdeploy-plugin-qt` | `QtWatchAllVideos2-<sha>-linux-x86_64.AppImage` |
| macOS | `macdeployqt` | `QtWatchAllVideos2-<sha>-macos-<arch>.zip` |
| Windows | `windeployqt` | `QtWatchAllVideos2-<sha>-windows-x64.zip` |

`.github/workflows/pages.yml` then publishes the Release artifacts of every
push to `main` to GitHub Pages, together with a generated index page listing the
downloads. Tagged builds additionally attach the same archives to the GitHub
Release.

## A note on provenance

This project was mostly a copy/paste project from ChatGPT web — and the Qt code
still is; only the project scaffolding around it (CMake, CI, packaging) comes
from [`cpp-best-practices/cmake_template`](https://github.com/cpp-best-practices/cmake_template).
It works, and it's small enough to read, but don't expect the architecture to be
a textbook example.

## Patches welcome

Patches welcome — but please don't clutter the UI. The whole point is that it
stays out of your way.

## License

MIT — see [LICENSE.md](LICENSE.md).
