---
description: The toolbar's buttons, docking it and changing it, the tray menu, and every hotkey.
---

# Toolbar, Tray and Hotkeys

## The toolbar

![The toolbar](/images/en/toolbar.png)

From left to right, the buttons are:

| Button | Action | See |
| --- | --- | --- |
| Drag handle | Drag to move the toolbar | |
| Window | Click a window or control, or drag a rectangle | [Capturing the Screen](./capture) |
| Scrolling | Scrolling capture (Windows, experimental) | [Scrolling Capture](./scrolling) |
| Freehand | Capture along any outline | [Capturing the Screen](./capture#freehand-region) |
| Pin | Capture an area and pin it to the screen | [Pinning to the Screen](./pin) |
| Record | Record the screen; press again to stop | [Screen Recording](./record) |
| QR Code | Read the QR codes and barcodes in an area | [QR codes](./tools#qr-codes) |
| Text | Recognize the text in an area, to copy or translate it | [Text Recognition and Translation](./ocr) |
| Crosshair | A full-screen crosshair with position, color and distance | [Crosshair and color picker](./tools#crosshair-and-color-picker) |
| Ruler | A screen ruler in pixels | [Screen ruler](./tools#screen-ruler) |
| Whiteboard | Draw on the screen | [Screen Whiteboard](./whiteboard) |
| Delay Before Capture | Seconds to wait before capturing | [Capturing the Screen](./capture#delayed-capture) |
| Open | Open images to edit | [Preview and Editor](./editor#opening-images) |
| Settings | Open the settings | [Settings](./settings) |
| Minimize to Tray | Hide the toolbar; the tray icon or a hotkey brings it back | |

The **Capture Full Screen** button is hidden by default and can be shown in the settings.

### Docking at a screen edge

Drop the toolbar against an edge of the screen and it docks there: it shrinks to a thin strip and slides out when the pointer reaches it. Drag it away from the edge to undock it.

### Opening images

Drop image files on the toolbar to open them in the editor.

### Changing the toolbar

In **Settings > Toolbar**:

- Uncheck a button to hide it; drag it or use **Move Up** and **Move Down** to reorder.
- **Toolbar size** changes the size of the buttons.

## The tray icon

Click the Glimpse icon in the notification area to show the toolbar. Its context menu has every capture mode, and:

- **Open Images...**
- **Close All Pins**
- **Show Toolbar**
- **Settings...**
- **About Glimpse...**
- **Exit**

## Hotkeys

The hotkeys work in any program (global hotkeys).

| Hotkey | Action |
| --- | --- |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd> | Capture window / region |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>2</kbd> | Screen whiteboard (while it is open: pass-through on or off) |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>3</kbd> | Capture full screen |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>P</kbd> | Pin region to screen |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>4</kbd> | Scan QR code |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>5</kbd> | Recognize and translate text |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>6</kbd> | Capture freehand region |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>7</kbd> | Screen ruler |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>8</kbd> | Crosshair / color picker |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>9</kbd> | Scrolling capture (experimental) |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>V</kbd> | Record screen (press again to stop) |

### Changing a hotkey

1. Open **Settings > Hotkeys**.
2. Click a field and press the new key combination. Clear a field to turn that hotkey off.
3. Click **OK**.

Hotkeys are paused while the settings are open.

When another program already uses a hotkey, Glimpse says so in the tray when it starts, and marks the hotkey "Already in use by another program" in the settings. Choose another combination.

![Hotkey settings](/images/en/settings-hotkeys.png)
