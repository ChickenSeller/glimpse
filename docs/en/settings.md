---
description: "The pages of the settings window: General, Hotkeys, Toolbar, Recording, Text Recognition and Translation."
---

# Settings

Click **Settings** on the toolbar, or choose **Settings...** in the tray menu. **Restore Defaults** puts every setting back to its default.

## General

![General settings](/images/en/settings-general.png)

| Setting | Description |
| --- | --- |
| Language | The interface language: the system's, English, Simplified Chinese or Japanese |
| Save files in | Screenshots go to its `Screenshots` subfolder, recordings to `Recordings`. By default, `output` in the program folder |
| Clear | Move saved screenshots, recordings or both to the Recycle Bin |
| Start Glimpse when I sign in | Start with your session, waiting in the tray |

### Captures

See [Capture settings](./capture#capture-settings). For **Show a preview during scrolling capture** and its **Opacity**, see [Scrolling Capture](./scrolling#preview).

### Downloads

Recognition models, Firefox translation files and local translation models are downloaded the first time they are needed. They come from Hugging Face, GitHub and other sites, which are slow in some places, so Glimpse also offers a mirror.

| Download from | Description |
| --- | --- |
| Mirror first, then the official source | Default |
| Official source first, then the mirror | |
| Official source only | No mirror |

**Mirror** is `https://pages.yanlei.org/glimpse-mirror` by default and can point to a mirror of your own. Every file is verified, and one that is not what was expected is not used.

### Updates

For **At startup** and **Check and Update Now**, see [Download and Installation](./install#updates).

**Send anonymous usage statistics** sends which Glimpse version runs and how updating goes, with a random ID for this installation. Nothing about you, your files or your screen is sent. Uncheck it to send nothing.

## Hotkeys

A global hotkey for each action; see [Changing a hotkey](./toolbar#changing-a-hotkey).

## Toolbar

Which buttons the toolbar shows, in what order, and its size; see [Changing the toolbar](./toolbar#changing-the-toolbar).

## Recording

Mouse and keys in the video, frame rate and color detail; see [Screen Recording](./record#mouse-and-keys-in-the-video). Recording the microphone and the sound the computer plays; see [Sound](./record#sound).

## Text Recognition

The recognition engine and languages; see [Recognition engines](./ocr#recognition-engines).

## Translation

The service, target language, API keys, local model and default prompt; see [Translation Engines and Prompts](./translation).
