---
description: What Glimpse does, its main features and system requirements.
---

# Introduction

Glimpse is a screenshot and screen recording tool for Windows and Linux. A capture can be annotated, saved, copied or pinned to the screen; the text in it can be recognized, copied or translated, and the translation can be painted back over the picture. Glimpse is open source under the MIT License and free to use.

Glimpse lives in a small toolbar and in the notification area (tray), and every feature has a global hotkey.

![Selecting an area to capture](/images/en/capture.png)

## Main features

| Feature | Description | See |
| --- | --- | --- |
| Capturing | A window or control, a rectangle, a freehand region or the full screen, with an optional delay | [Capturing the Screen](./capture) |
| Scrolling capture | Scrolls and stitches long pages and documents (Windows, experimental) | [Scrolling Capture](./scrolling) |
| Annotating | Arrows, shapes, text, numbers, highlighting and blur; crop, rotate and resize | [Preview and Editor](./editor) |
| Pinning | Keeps a captured area on top of all windows | [Pinning to the Screen](./pin) |
| Text recognition | PaddleOCR, Windows OCR and Tesseract | [Text Recognition and Translation](./ocr) |
| Translation | Offline Firefox translation and local language models, and online services such as Google, DeepL, Claude and OpenAI-compatible APIs | [Translation Engines and Prompts](./translation) |
| Screen recording | Records to MP4, with mouse clicks and keys shown | [Screen Recording](./record) |
| Screen whiteboard | Draws over the live screen, a frozen one or a whiteboard | [Screen Whiteboard](./whiteboard) |
| More tools | QR codes, crosshair and color picker, screen ruler | [QR Codes, Crosshair and Ruler](./tools) |

## System requirements

- Windows 10 or Windows 11 (x64).
- Linux: `.deb` and `.rpm` packages for Debian 12, Ubuntu 22.04, Fedora 35, RHEL 9 and later, on X11 or Wayland. Some features are not available yet; see [Linux](./linux).
- Interface languages: English, Simplified Chinese and Japanese.

The models for text recognition and offline translation are not bundled. They are downloaded the first time they are needed, and work offline from then on.
