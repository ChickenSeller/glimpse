---
description: Capturing a window, a control, a rectangle, a freehand region or the full screen, delayed captures, and the capture settings.
---

# Capturing the Screen

## Window or region

Click **Window** on the toolbar, or press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>. The screen freezes; then:

- **To capture a window or control**, move the pointer over it. Glimpse outlines the window under the pointer and shows its title, program and size. Click to capture it. Turn the mouse wheel to go between the outer window and the controls inside it (buttons, panels and so on).
- **To capture a rectangle**, drag it out with the left button and release. Hold <kbd>Space</kbd> while dragging to move the whole rectangle.

![Picking a window](/images/en/capture-window.png)

Next to the pointer, Glimpse shows the position of the pixel under it and the rectangle's size, for exact alignment.

Press <kbd>Esc</kbd> or right-click to cancel.

::: info Linux
On Linux, windows cannot be picked; drag a rectangle instead. See [Linux](./linux).
:::

### Adjusting the area before taking it

With **Adjust the selected area before taking it** checked in **Settings > General**, a rectangle is not taken at once but can be adjusted first:

![Adjusting the area](/images/en/capture.png)

| Action | Effect |
| --- | --- |
| Drag the edges or handles | Move or resize the area |
| Arrow keys | Move by 1 pixel; with <kbd>Shift</kbd>, by 10 |
| <kbd>Ctrl</kbd>+arrow keys | Resize |
| <kbd>Enter</kbd>, or double-click inside | Take it |
| <kbd>Ctrl</kbd>+<kbd>C</kbd> | Copy the area to the clipboard and finish |
| Right-click | Drop the area and select again |

The setting applies to pinning, text recognition and QR codes too.

## Freehand region

Click **Freehand** on the toolbar, or press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>6</kbd>. Draw around what you want while holding the button, and release.

Outside the outline the capture is transparent by default (kept when saved as PNG). In **Settings > General**, uncheck **Transparent** under **Outside a freehand region** to fill it with a color instead.

## Full screen

Press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>3</kbd> to capture the whole screen. With several monitors, the capture spans all of them.

The **Capture Full Screen** button is hidden by default; show it in **Settings > Toolbar**.

## Delayed capture

To capture a menu, a tooltip or anything else that disappears as soon as the mouse moves:

1. Click **Delay Before Capture** on the toolbar and choose 1, 2, 3, 5 or 10 seconds, or **Custom...**.
2. Start the capture. A countdown appears; open the menu meanwhile.
3. When it ends, the screen freezes and you select the area.

Click or press <kbd>Esc</kbd> during the countdown to cancel. The delay stays set until you choose **No Delay**.

## After capturing

The capture opens in the preview window, where you can save, copy, edit or pin it, or recognize its text. See [Preview and Editor](./editor).

With **Copy each capture to the clipboard** checked in **Settings > General**, every capture is copied as well.

## Capture settings

In **Settings > General > Captures**:

| Setting | Description |
| --- | --- |
| Include the Glimpse toolbar | Leave the toolbar visible in captures |
| Include the mouse pointer | Keep the pointer in the capture (Windows only) |
| Copy each capture to the clipboard | Copy every capture |
| Outside a freehand region | Transparent, or a fill color |
| Adjust the selected area before taking it | See [Adjusting the area before taking it](#adjusting-the-area-before-taking-it) |
