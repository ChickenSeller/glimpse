---
description: Recording part of the screen to MP4, pausing and stopping, recording sound, showing mouse clicks and keys, and the recording settings.
---

# Screen Recording

Glimpse records a window or an area of the screen to an MP4 video, for demonstrations and bug reports. The microphone and the sound the computer plays can be recorded with it.

## Starting and stopping

1. Click **Record** on the toolbar, or press <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>V</kbd>.
2. Click the window to record, or drag the area. With a [delay before capture](./capture#delayed-capture) set, a countdown comes first.
3. Recording starts and a recording panel appears, to **Pause**, **Resume** or **Stop Recording**.
4. Pressing <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>V</kbd> again, or **Record** on the toolbar, also stops it.

Recordings are saved in the `Recordings` subfolder of the folder chosen in the settings. When one stops, the **Recording Saved** window shows its length and size:

| Button | Action |
| --- | --- |
| Open | Play it in the system's player |
| Show in Folder | Show it in the file manager |
| Copy Path | Copy the file's path to the clipboard |
| Discard | Move the recording to the Recycle Bin |

## Sound

Sound is off by default. In **Settings > Recording > Sound**:

| Setting | Description |
| --- | --- |
| Record the microphone | Records the input device chosen. **Default device** follows the system's default microphone |
| Record the sound the computer plays | Records what the default output device plays, such as videos and notification sounds |

With both on, they are mixed into one track, saved as AAC in the MP4. While the recording is paused, no sound is recorded, so it stays in step with the picture.

If the microphone cannot be opened, the recording does not start and the reason is shown. On Windows, check that desktop apps may use the microphone in **Settings > Privacy & security > Microphone**.

On Linux, recording the sound the computer plays needs PipeWire.

## Mouse and keys in the video

In **Settings > Recording**:

| Setting | Description |
| --- | --- |
| Highlight the mouse pointer | Makes the pointer easy to follow |
| Show mouse clicks | Marks every click in the video |
| Show keys pressed | Shows the keys pressed in the video |
| Style | **Key labels** (such as `Ctrl + S`), **Full keyboard** (keys light up on an on-screen keyboard) or **Keyboard and labels** |
| Position | Inside the picture at the top, bottom, left or right, or outside it on one side |

Outside the picture, the video gets a band on that side for the keys, so they cover nothing; the video is that much larger.

::: warning
Everything typed is shown, including passwords.
:::

These are available on Windows only so far.

## Quality

| Setting | Description |
| --- | --- |
| Video frame rate | 15, 24, 30 (default) or 60 fps |
| Color detail | **Standard**: like most videos, one color for every 2×2 pixels, which smears thin colored lines. **High**: every pixel's color is kept, for charts and highlighted code; files are about three times larger |

Areas wider or taller than 2048 pixels are saved as H.265, which some players cannot play.

![Recording settings](/images/en/settings-recording.png)
