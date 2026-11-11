---
description: Recognizing the text on the screen, copying or translating it, translated images, and the recognition engines, languages and model downloads.
---

# Text Recognition and Translation

## Recognizing text

1. Click **Text** on the toolbar, or press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>5</kbd>, and select the area with the text. In the preview window, **Recognize and Translate Text...** does the same for a capture.
2. The **Recognize and Translate Text** window opens. At the top is the capture, with every recognized line outlined.
3. The recognized text is in the middle, one paragraph per line. Edit it if needed, then click **Copy**.

![The Recognize and Translate Text window](/images/en/ocr.png)

**Recognize with** recognizes again with another engine, for this window only; **Settings > Text Recognition** sets the engine the window opens with.

## Translating

1. Choose the target language in **Translate into**. The source language is detected.
2. Choose the translation engine or local model in **Engine**. **Settings > Translation** sets the one the window opens with.
3. Click **Translate**. The translation appears below; edit it if needed and click **Copy Translation**.

When you have edited the recognized text, the translation follows its lines and blank lines.

With a language-model engine (Claude, an OpenAI-compatible API or a general local model), a **Prompt** button appears for changing the prompt for this window. See [Translation Engines and Prompts](./translation#prompts).

## Translated images

**Translated Image** paints the translation over the capture, where the text was, and opens the result in the preview window to save, copy or edit.

<div class="compare">

![The original](/images/en/original.png)

![The translated image](/images/en/translated.png)

</div>

Glimpse erases the original text, fills its place from the background around it, and sets the translation in the original's color, size, weight and alignment. Plain and gradient backgrounds work best; textures and photos behind text cannot be fully restored.

::: tip
A translated image needs the text to keep one recognized paragraph per line. If paragraphs were joined or split by hand, Glimpse asks you to restore the lines and translate again.
:::

## Recognition engines

| Engine | Strengths | First use |
| --- | --- | --- |
| PaddleOCR (default) | The most accurate, especially for Chinese and Japanese; one model reads every supported language | Downloads about 31 MB |
| Windows OCR | Built into Windows, nothing to download; reads one language at a time | Needs the language's optical character recognition feature in Windows |
| Tesseract | The classic open-source engine | Downloads 2–4 MB per language |

Windows OCR uses the first selected language whose OCR feature is installed. If none is, add the language in Windows **Settings > Time & language > Language**, with its optical character recognition feature.

### Recognition languages

In **Settings > Text Recognition**, check the languages (scripts) to recognize: Simplified Chinese, Traditional Chinese, English, Japanese and French. The fewer languages, the faster and more accurate the recognition.

![Text recognition settings](/images/en/settings-ocr.png)

## Downloading models

Recognition models and the offline translation files are not bundled. The first time one is needed, Glimpse lists the files and offers to download them; from then on they work offline.

If downloading is slow:

- **Copy Official Links** or **Copy Mirror Links**, download the files in a browser or a download manager, then click **Choose Files...** and pick them (several at once; Glimpse matches them by name).
- Change where Glimpse downloads from in **Settings > General > Downloads**; see [Settings](./settings#downloads).

A download can be canceled halfway, to come back and use another way.
