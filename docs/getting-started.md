---
description: 下载并启动 Glimpse，完成第一次截图、文字识别和翻译。
---

# 快速上手

## 1. 下载并启动

从[版本页面](https://pages.yanlei.org/kaguya/glimpse/releases.html)下载最新的 Windows 压缩包（`Glimpse-<版本>-windows-x64.zip`），解压到任意文件夹，运行其中的 `glimpse.exe`。无需安装，也不需要管理员权限。

启动后屏幕上会出现 Glimpse 的工具栏，通知区域（托盘）中也会出现 Glimpse 的图标。

![工具栏](/images/toolbar.png)

## 2. 截一张图

1. 单击工具栏上的 **窗口** 按钮，或按 <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>。
2. 将鼠标移到要截取的窗口上单击；或者按住鼠标拖出一个矩形。
3. 截取的图片显示在预览窗口中。在这里可以 **另存为**、**复制**、**编辑**（加箭头、文字等标注），或 **贴到屏幕** 上。

详细说明参见[截图](./capture)和[预览与编辑](./editor)。

## 3. 识别并翻译文字

1. 单击工具栏上的 **文字** 按钮，或按 <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>5</kbd>。
2. 拖出包含文字的区域。首次使用时，Glimpse 会询问是否下载识别模型（约 31 MB）。
3. 识别出的文字显示在 **文字识别和翻译** 窗口中，可以修改后 **复制**。
4. 选择目标语言，单击 **翻译**。默认使用离线的 Firefox 翻译，首次翻译某个语言方向时会下载该方向的语言文件。
5. 单击 **翻译图片**，可得到把译文画在原文位置上的图片。

![文字识别和翻译窗口](/images/ocr.png)

翻译引擎的选择参见[翻译引擎与提示词](./translation)。

## 4. 常用快捷键

| 快捷键 | 功能 |
| --- | --- |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd> | 捕捉窗口/区域 |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>3</kbd> | 捕捉全屏 |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>P</kbd> | 贴图到屏幕 |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>5</kbd> | 文字识别和翻译 |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>V</kbd> | 录制屏幕（再按一次停止） |

全部快捷键参见[工具栏、托盘与快捷键](./toolbar#快捷键)。

## 后续步骤

- 截取长网页：[滚动截图](./scrolling)
- 不联网也能翻译，或使用自己的大模型：[翻译引擎与提示词](./translation)
- 录制操作演示：[录屏](./record)
- 截图自动保存的位置、开机启动等：[设置](./settings)
