# Glimpse

A screenshot and screen recording tool for the desktop, in the spirit of
FastStone Capture. Built with Qt 6 for Windows and Linux (X11 and Wayland).

The interface is available in English, Simplified Chinese and Japanese.

Homepage: <https://pages.yanlei.org/kaguya/glimpse/>

## Features

- **Capture**: active window, rectangular region, freehand region or full
  screen, with an optional delay. The area outside a freehand region can be
  filled with a color or left transparent.
- **Pin to the screen**: keep a captured region floating on top of all
  windows, right where it was taken; drag, zoom and fade it.
- **Annotate**: arrows, shapes, text, highlighting, blur and more, in the
  built-in editor (kImageAnnotator).
- **Save and share**: save, copy to the clipboard, or *Save and Copy Path*,
  which copies the saved file's full path (handy for pasting into a chat or
  a coding assistant).
- **Screen recording** to MP4: highlights the pointer and mouse clicks, and
  can show the keys pressed, or a full on-screen keyboard, at the bottom of the
  video.
- **Text recognition (OCR)** with Windows OCR, Tesseract or PaddleOCR.
- **Screenshot translation**: recognize the text in a region and translate it.
  - Offline: Firefox's translation engine (Bergamot), or a local language
    model through llama.cpp (Hy-MT2, Qwen3), which uses the GPU through Vulkan
    and falls back to the CPU.
  - Online: Google Translate (free), DeepL, Google Cloud Translation,
    Microsoft Translator or Claude, with your own API key where one is needed.
- **QR code and barcode** reading (ZXing-C++).
- **Screen color picker** and **screen crosshair**.
- Global hotkeys and a small floating toolbar.

Models for OCR and offline translation are downloaded on first use, not
bundled. Where Hugging Face or GitHub are hard to reach, Glimpse also fetches
them from a mirror (by default <https://pages.yanlei.org/glimpse-mirror>,
tried first); the order and the mirror can be changed in Settings > General.
On Windows, Glimpse checks the homepage for updates at start; Settings >
General chooses whether it updates automatically, only to a required
version, asks first, or never checks. The homepage's web server reads the
releases live from GitLab and sets the required version (see
`deploy/nginx/`).

To run a mirror, list the files with `glimpse --list-downloads list.tsv` and
fill a web server's directory with `python tools/mirror-downloads.py list.tsv
<directory>`.

### Default hotkeys

| Hotkey | Action |
|---|---|
| Ctrl+Alt+1 | Window |
| Ctrl+Alt+2 | Region |
| Ctrl+Alt+3 | Full screen |
| Ctrl+Alt+P | Pin region to screen |
| Ctrl+Alt+4 | QR code |
| Ctrl+Alt+5 | Text recognition (OCR) |
| Ctrl+Alt+6 | Freehand region |
| Ctrl+Alt+7 | Color picker |
| Ctrl+Alt+8 | Crosshair |
| Ctrl+Alt+V | Screen recording |
| Ctrl+Alt+T | Translate |

All hotkeys can be changed in the settings.

### Platform support

Windows has every feature. On Linux, the following are not available yet and
are shown disabled: window capture, the pointer in captures, global hotkeys,
and the mouse and keyboard overlays in recordings. Wayland also cannot offer
window picking or the pointer at all. Windows OCR is Windows only, and the
Firefox translation engine is built on Windows only so far.

## Building

Requirements:

- Qt 6.8 or later (Widgets, Svg, Concurrent, Network, Multimedia,
  LinguistTools; DBus on Linux)
- CMake 3.24 or later and a C++20 compiler (MinGW-w64 or MSVC on Windows,
  GCC or Clang on Linux)
- Git and Visual Studio 2022 on Windows for the Firefox translation engine,
  which is built with MSVC; without them the build skips it

Other dependencies (ZXing-C++, Tesseract, Leptonica, ONNX Runtime, llama.cpp,
kImageAnnotator) are taken from the system when CMake finds them and are
otherwise downloaded at pinned, checksum-verified versions.

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt dir>
cmake --build build/release --parallel
```

Optional engines can be left out:

| Option | Default | Builds |
|---|---|---|
| `GLIMPSE_WITH_TESSERACT` | `ON` | Tesseract OCR |
| `GLIMPSE_WITH_PADDLEOCR` | `ON` | PaddleOCR (ONNX Runtime) |
| `GLIMPSE_WITH_LLAMA` | `ON` | Local translation with llama.cpp |
| `GLIMPSE_WITH_BERGAMOT` | `ON` | Firefox translation engine (Windows) |

### Packaging

```sh
# A ready-to-run folder with Qt and all libraries deployed beside the program
cmake --install build/release --component glimpse --prefix <dir>

# The same as a zip archive
cmake --build build/release --target package
```

## License

Glimpse is released under the [MIT License](LICENSE). It uses third-party
components under their own licenses (LGPL-3.0 for Qt, MPL-2.0 for the
Firefox translation engine, and others); see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the `licenses/` folder.
