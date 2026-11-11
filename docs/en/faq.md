---
description: Common questions and how to solve them.
---

# FAQ

## A hotkey does nothing

Another program may use it already. Glimpse lists the hotkeys it could not register in the tray when it starts, and marks them in **Settings > Hotkeys**; choose another combination. See [Changing a hotkey](./toolbar#changing-a-hotkey).

## The toolbar is gone

It may be minimized to the tray, or docked at a screen edge as a thin strip. Click the Glimpse icon in the tray to show it; when docked, move the pointer onto the strip at the edge.

## A menu or tooltip disappears when I capture

Use a [delayed capture](./capture#delayed-capture): set a delay, start the capture and open the menu during the countdown.

## Model downloads are slow or fail

Change where Glimpse downloads from in **Settings > General > Downloads**, or copy the links in the download window, download the files in a browser or download manager and choose them. See [Downloading models](./ocr#downloading-models).

## Windows OCR says no language is available

Windows needs the language's optical character recognition feature; see [Recognition engines](./ocr#recognition-engines). Or use PaddleOCR.

## The recognized text is wrong

- Check only the languages you need in **Settings > Text Recognition**.
- Use PaddleOCR for Chinese and Japanese.
- For very small text, zoom the page in before capturing.

## A translated image asks me to keep the lines

A translated image needs one recognized paragraph per line. If you joined or split paragraphs while editing, restore the lines and translate again. See [Translated images](./ocr#translated-images).

## Ollama times out

Loading a model takes time the first time, especially a large one or without a graphics card. Wait and try again, or use a smaller model. See [With Ollama](./translation#with-ollama).

## There is no Prompt button

Only engines that take a custom prompt show it; translation models such as Hy-MT2 and Index-Translate use their built-in prompt and do not. See [Prompts](./translation#prompts).

## Scrolling capture does not line up

Scrolling capture is still experimental. Try:

- <kbd>Ctrl</kbd>+<kbd>Z</kbd> to take back the last part, then scroll slowly;
- holding <kbd>Ctrl</kbd> while choosing the area, to scroll yourself;
- an area that covers only what scrolls, leaving out fixed headers and side bars.

See [Scrolling Capture](./scrolling).

## Where are the settings, and how do I move them?

See [Settings file](./install#settings-file). Copy the whole program folder to take the settings with it.
