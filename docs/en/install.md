---
description: Downloading, installing, updating and removing Glimpse, and where its settings are kept.
---

# Download and Installation

## Download

Every release can be downloaded from either place; the files are the same:

- The [releases page](https://pages.yanlei.org/kaguya/glimpse/releases.html) of this site
- [GitHub](https://github.com/ChickenSeller/glimpse/releases)

| File | For |
| --- | --- |
| `Glimpse-<version>-windows-x64.zip` | Windows 10 and 11 (x64) |
| `glimpse_<version>_amd64.deb` | Debian, Ubuntu and others |
| `glimpse-<version>-1.x86_64.rpm` | Fedora, RHEL and others |

## Windows

Glimpse is portable and needs no installation:

1. Unpack the archive into any folder, for example `C:\Tools\Glimpse`.
2. Run `glimpse.exe` in it.

To start Glimpse when you sign in to Windows, check **Start Glimpse when I sign in** in **Settings > General**. Started that way, Glimpse waits in the notification area without showing the toolbar.

::: tip Use a folder you can write to
Glimpse keeps its settings in `config.ini` in its own folder, and by default saves captures and recordings in the `output` subfolder there. Do not unpack it into `C:\Program Files` or another folder that needs administrator rights to write to.
:::

## Linux

Install the package, for example:

```sh
sudo apt install ./glimpse_<version>_amd64.deb     # Debian, Ubuntu
sudo dnf install ./glimpse-<version>-1.x86_64.rpm  # Fedora, RHEL
```

Glimpse is installed in `/opt/glimpse`, with a menu entry and the `glimpse` command. For the differences on Linux, see [Linux](./linux).

## Updates

On Windows, Glimpse checks for a new version at start. **Settings > General > Updates** chooses what it does then:

| Option | Description |
| --- | --- |
| Ask before updating | Asks whether to update when there is a new version (default) |
| Update automatically | Downloads, verifies and installs new versions, then restarts Glimpse |
| Install required updates automatically | Installs only versions marked as required; asks for the others |
| Never update | Does not check at start |

**Check and Update Now** checks at any time. On Linux, Glimpse tells you about a new version; download the new package from the releases page.

## Settings file

| Situation | Location |
| --- | --- |
| The program folder can be written to (the Windows archive) | `config.ini` in the program folder |
| It cannot (Linux packages and the like) | Windows: the registry; Linux: `~/.config/Glimpse/Glimpse.conf` |

`config.ini` moves with the program folder: copy the whole folder to take the settings, the editor's tool colors, widths and fonts included, to another computer.

Recognition and translation models are kept in your user data folder, not in the program folder, so updating or moving Glimpse does not download them again.

## Removing Glimpse

- Windows: quit Glimpse (tray menu > **Exit**) and delete its folder. If it starts at sign-in, uncheck that in the settings first.
- Linux: remove the package, for example `sudo apt remove glimpse`.
