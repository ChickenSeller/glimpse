---
description: The preview window after a capture, the annotation editor's tools and image operations, and opening existing images.
---

# Preview and Editor

## The preview window

After a capture, the image opens in the preview window; the status bar shows its size in pixels.

![The preview window](/images/en/editor.png)

| Button | Shortcut | Action |
| --- | --- | --- |
| Save As... | <kbd>Ctrl</kbd>+<kbd>S</kbd> | Save as PNG, JPEG or BMP |
| Save and Copy Path... | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>S</kbd> | Save, then copy the file's full path to the clipboard |
| Copy | <kbd>Ctrl</kbd>+<kbd>C</kbd> | Copy the image to the clipboard |
| Edit... | <kbd>Ctrl</kbd>+<kbd>E</kbd> | Edit in the annotation editor |
| Pin to Screen | <kbd>Ctrl</kbd>+<kbd>P</kbd> | Pin the image to the screen; see [Pinning to the Screen](./pin) |
| Recognize and Translate Text... | <kbd>Ctrl</kbd>+<kbd>T</kbd> | Recognize the text in the image; see [Text Recognition and Translation](./ocr) |
| Close | <kbd>Esc</kbd> | Close the preview |

The save dialog opens in the `Screenshots` subfolder of the folder chosen in the settings, with the name `Glimpse_<date>_<time>.png`.

::: tip Save and Copy Path
To hand a capture to a tool that takes file paths (an AI coding assistant in a terminal, for example), use **Save and Copy Path** and paste the path.
:::

## The annotation editor

Click **Edit...** in the preview window to open the annotation editor.

![The annotation editor](/images/en/annotate.png)

### Annotation tools

The tools are on the left; the small arrow beside some of them opens related tools:

| Tool | Description |
| --- | --- |
| Select | Select and move marks, or change their properties |
| Duplicate | Copy a part of the image to somewhere else |
| Line, arrow | Lines, arrows and double arrows |
| Pen | Freehand lines |
| Marker | See-through highlighting: freehand, rectangle or ellipse |
| Text | Text, text with a pointer, text with an arrow |
| Number | Numbers counting up, with a pointer or an arrow if wanted |
| Blur, pixelate | Hide sensitive details |
| Shapes | Rectangles, ellipses and triangles |
| Sticker | Icon-like stickers |

The bar above sets color, width, fill, shadow, opacity and so on. Each tool's color, width and font are kept in the settings for next time.

### Image operations

The toolbar at the top:

| Button | Action |
| --- | --- |
| Done (<kbd>Ctrl</kbd>+<kbd>Enter</kbd>) | Apply the edits to the capture and close the editor |
| Save, Save As... (<kbd>Ctrl</kbd>+<kbd>S</kbd>) | Save to a file (for an opened image, **Save** overwrites it) |
| Copy Image (<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>C</kbd>) | Copy the edited image to the clipboard |
| Undo, Redo (<kbd>Ctrl</kbd>+<kbd>Z</kbd>, <kbd>Ctrl</kbd>+<kbd>Y</kbd>) | Undo or redo the last step |
| Crop | Crop the image |
| Cut Out | Cut a horizontal or vertical strip out of the image; the parts on either side close up |
| Resize | Scale the image, in pixels or percent |
| Rotate | Rotate or flip the image |
| Canvas | Change the canvas size and color, for example to add a margin |

The view can be zoomed at the bottom.

## Opening images

Any of these opens existing images in the annotation editor:

- **Open** on the toolbar, or **Open Images...** in the tray menu;
- dropping image files on the toolbar;
- `glimpse <file>...` on the command line, or choosing `glimpse.exe` under "Open with" in File Explorer.

Several files can be opened at once.
