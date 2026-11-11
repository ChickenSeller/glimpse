---
description: Glimpse 在 Linux（X11 和 Wayland）上的功能差异和注意事项。
---

# Linux

Glimpse 提供 `.deb` 和 `.rpm` 安装包，安装方法参见[下载与安装](./install#linux)。

## 功能差异

| 功能 | Windows | X11 | Wayland |
| --- | --- | --- | --- |
| 矩形、手绘区域和全屏截图 | ✓ | ✓ | ✓ |
| 选择窗口、截入鼠标指针 | ✓ | 计划中 | — |
| 滚动截图 | ✓ | 计划中 | 计划中 |
| 全局快捷键 | ✓ | GNOME | GNOME |
| 录屏中显示鼠标点击和按键 | ✓ | 计划中 | — |
| PaddleOCR、Tesseract | ✓ | ✓ | ✓ |
| Windows OCR | ✓ | — | — |
| 本地模型翻译 | ✓ | ✓ | ✓ |
| Firefox 翻译 | ✓ | 计划中 | 计划中 |
| 程序内更新 | ✓ | 提示新版本 | 提示新版本 |

不支持的功能在界面中显示为灰色。

## 快捷键（GNOME）

在 GNOME 上，Glimpse 把快捷键登记为 GNOME 设置中的自定义键盘快捷键。Glimpse 未运行时按下快捷键，也会启动 Glimpse 并执行对应的功能。其他桌面环境暂不支持全局快捷键，请使用工具栏或托盘菜单。

## Wayland

- Glimpse 在 Wayland 会话中通过 XWayland 运行（除非设置了 `QT_QPA_PLATFORM` 环境变量），因为 Wayland 窗口无法自行定位或保持在最上层。
- 截图通过桌面门户（xdg-desktop-portal）进行。首次截图时系统会询问是否允许，只需确认一次。如曾经拒绝，Glimpse 会提示 **重新询问**。
- 截图中不能包含鼠标指针；录屏中可以显示指针。
