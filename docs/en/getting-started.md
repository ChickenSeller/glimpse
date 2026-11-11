---
description: Download and start Glimpse, then take a first capture, recognize text and translate it.
---

# Getting Started

## 1. Download and start

Download the latest Windows archive (`Glimpse-<version>-windows-x64.zip`) from the [releases page](https://pages.yanlei.org/kaguya/glimpse/releases.html), unpack it into any folder and run `glimpse.exe`. Nothing is installed, and no administrator rights are needed.

The Glimpse toolbar appears on the screen, and the Glimpse icon in the notification area (tray).

![The toolbar](/images/en/toolbar.png)

## 2. Take a capture

1. Click **Window** on the toolbar, or press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>.
2. Click the window to capture, or drag a rectangle.
3. The capture opens in the preview window. From there you can **Save As**, **Copy**, **Edit** it (add arrows, text and other marks) or **Pin to Screen**.

See [Capturing the Screen](./capture) and [Preview and Editor](./editor).

## 3. Recognize and translate text

1. Click **Text** on the toolbar, or press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>5</kbd>.
2. Drag over the text. The first time, Glimpse offers to download the recognition model (about 31 MB).
3. The recognized text appears in the **Recognize and Translate Text** window. Edit it if needed, then click **Copy**.
4. Choose the target language and click **Translate**. The default engine is the offline Firefox translation, which downloads the files for each language direction the first time.
5. Click **Translated Image** for a copy of the capture with the translation painted where the text was.

![The Recognize and Translate Text window](/images/en/ocr.png)

To choose another engine, see [Translation Engines and Prompts](./translation).

## 4. Common hotkeys

| Hotkey | Action |
| --- | --- |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd> | Capture window / region |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>3</kbd> | Capture full screen |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>P</kbd> | Pin region to screen |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>5</kbd> | Recognize and translate text |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>V</kbd> | Record screen (press again to stop) |

All hotkeys are listed in [Toolbar, Tray and Hotkeys](./toolbar#hotkeys).

## Next steps

- Capture a long web page: [Scrolling Capture](./scrolling)
- Translate offline, or with your own language model: [Translation Engines and Prompts](./translation)
- Record a demonstration: [Screen Recording](./record)
- Where captures are saved, starting at sign-in and more: [Settings](./settings)
