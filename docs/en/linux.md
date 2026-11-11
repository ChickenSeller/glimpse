---
description: What differs on Linux (X11 and Wayland), and what to know there.
---

# Linux

Glimpse comes as `.deb` and `.rpm` packages; see [Download and Installation](./install#linux).

## Differences

| Feature | Windows | X11 | Wayland |
| --- | --- | --- | --- |
| Rectangle, freehand and full screen captures | ✓ | ✓ | ✓ |
| Picking a window, the pointer in captures | ✓ | Planned | — |
| Scrolling capture | ✓ | Planned | Planned |
| Global hotkeys | ✓ | GNOME | GNOME |
| Clicks and keys in recordings | ✓ | Planned | — |
| PaddleOCR, Tesseract | ✓ | ✓ | ✓ |
| Windows OCR | ✓ | — | — |
| Local model translation | ✓ | ✓ | ✓ |
| Firefox translation | ✓ | Planned | Planned |
| Updating from inside the app | ✓ | Tells about new versions | Tells about new versions |

Features that are not available are shown disabled.

## Hotkeys on GNOME

On GNOME, Glimpse's hotkeys become custom keyboard shortcuts in GNOME's settings. Pressed while Glimpse is not running, they start it and run their action. Other desktops have no global hotkeys yet; use the toolbar or the tray menu.

## Wayland

- On a Wayland session Glimpse runs through XWayland (unless `QT_QPA_PLATFORM` is set), as Wayland windows cannot place themselves or stay on top.
- Screenshots go through the desktop portal (xdg-desktop-portal). The system asks for permission at the first capture, once. If it was refused, Glimpse offers **Ask Again**.
- Screenshots cannot include the pointer; recordings show it.
