---
description: 下载、安装、更新和卸载 Glimpse，以及设置文件的位置。
---

# 下载与安装

## 下载

各版本的安装包可从以下位置下载，内容相同：

- 本站的[版本页面](https://pages.yanlei.org/kaguya/glimpse/releases.html)
- [GitHub](https://github.com/ChickenSeller/glimpse/releases)

| 文件 | 适用于 |
| --- | --- |
| `Glimpse-<版本>-windows-x64.zip` | Windows 10、11（x64） |
| `glimpse_<版本>_amd64.deb` | Debian、Ubuntu 等 |
| `glimpse-<版本>-1.x86_64.rpm` | Fedora、RHEL 等 |

## Windows

Glimpse 是绿色软件，无需安装：

1. 将压缩包解压到任意文件夹，例如 `C:\Tools\Glimpse`。
2. 运行其中的 `glimpse.exe`。

如需登录 Windows 时自动启动，请在 **设置 > 常规** 中勾选 **登录时启动 Glimpse**。此时 Glimpse 启动后只在通知区域中待命，不显示工具栏。

::: tip 放在可写入的文件夹中
Glimpse 把设置保存在程序文件夹中的 `config.ini`，截图和录像默认也保存在程序文件夹的 `output` 子文件夹中。请不要解压到 `C:\Program Files` 等需要管理员权限才能写入的位置。
:::

## Linux

安装对应的软件包，例如：

```sh
sudo apt install ./glimpse_<版本>_amd64.deb     # Debian、Ubuntu
sudo dnf install ./glimpse-<版本>-1.x86_64.rpm    # Fedora、RHEL
```

Glimpse 安装在 `/opt/glimpse`，并添加菜单项和 `glimpse` 命令。Linux 上的功能差异参见 [Linux](./linux)。

## 更新

Windows 版在启动时检查新版本。检查方式在 **设置 > 常规 > 更新** 中选择：

| 选项 | 说明 |
| --- | --- |
| 每次更新前确认 | 有新版本时询问是否更新（默认） |
| 自动更新 | 自动下载、校验并安装新版本，完成后重新启动 Glimpse |
| 自动进行必须的更新 | 仅自动安装作者标为“必须”的版本，其他版本照常询问 |
| 永不更新 | 启动时不检查 |

也可以随时单击 **立即检查并更新**。Linux 版有新版本时会提示，请从版本页面下载新的安装包。

## 设置文件

| 环境 | 位置 |
| --- | --- |
| 程序文件夹可写入（Windows 的解压版） | 程序文件夹中的 `config.ini` |
| 程序文件夹不可写入（Linux 安装包等） | Windows：注册表；Linux：`~/.config/Glimpse/Glimpse.conf` |

`config.ini` 随程序文件夹一起移动，复制整个文件夹即可把设置（包括编辑器的工具颜色、线宽和字体）带到另一台电脑。

识别和翻译模型保存在用户的数据文件夹中，不在程序文件夹里，更新或移动 Glimpse 后无需重新下载。

## 卸载

- Windows：退出 Glimpse（托盘菜单 > **退出**），删除程序文件夹即可。如已勾选登录时启动，请先在设置中取消。
- Linux：使用包管理器卸载，例如 `sudo apt remove glimpse`。
