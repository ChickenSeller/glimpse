---
description: 各翻译引擎的特点和设置，本地模型，OpenAI 兼容接口（DeepSeek、Ollama 等），以及自定义提示词。
---

# 翻译引擎与提示词

翻译引擎在 **设置 > 翻译** 中选择，也可以在文字识别和翻译窗口中为单次翻译临时更换。

![翻译设置](/images/settings-translation.png)

## 引擎一览

| 引擎 | 联网 | 需要密钥 | 特点 |
| --- | --- | --- | --- |
| Firefox 翻译（默认） | 否 | 否 | 速度快、模型小，任何电脑都能运行 |
| 本地模型（llama.cpp） | 否 | 否 | 在本机运行大语言模型，有显卡时更快 |
| Google 翻译（免费） | 是 | 否 | 质量好、语种多；非官方接口，可能被限流 |
| B 站 Index-Translate（免费） | 是 | 否 | 翻译大模型，译文自然，支持 150 种语言；速度较慢 |
| DeepL | 是 | 是 | 译文自然，尤其擅长欧洲语言和日语；免费版每月 50 万字符 |
| Google Cloud 翻译 | 是 | 是 | 语种多、速度快 |
| 微软翻译 | 是 | 是 | 语种多，免费额度每月 200 万字符 |
| Claude | 是 | 是 | 大语言模型，能读懂识别错误，保留语气和术语；按用量计费 |
| OpenAI 兼容接口 | 视服务而定 | 视服务而定 | DeepSeek、通义千问等在线服务，或本机的 Ollama、LM Studio |

可翻译为：简体中文、繁体中文、日语、英语、韩语、法语、德语、西班牙语、俄语。原文语言自动判断。

::: info 隐私
使用离线引擎时，文字不会离开这台电脑。使用在线引擎时，文字在本机识别后发送给所选服务翻译，截图本身不会发送。API 密钥保存在本机的用户设置中。
:::

## Firefox 翻译

Firefox 浏览器离线翻译所用的引擎（Bergamot）。每个翻译方向首次使用时下载 35–70 MB 的语言文件。它在英语和其他语言之间互译，其他组合（如中日互译）经英语中转。

目前仅 Windows 版提供。

## 本地模型

在本机用 llama.cpp 运行语言模型。有显卡时通过 Vulkan 使用 GPU，否则使用 CPU。在 **模型** 中选择：

| 模型 | 大小 | 说明 |
| --- | --- | --- |
| 混元翻译 Hy-MT2 1.8B（默认） | 1.1 GB | 轻量，普通笔记本也能运行 |
| 混元翻译 Hy-MT2 7B | 4.6 GB | 质量最好，建议有显卡 |
| Index-Translate 2B | 1.3 GB | B 站出品，轻量 |
| Index-Translate 9B | 5.8 GB | B 站出品，需要显卡 |
| Qwen3 4B | 2.5 GB | 通用模型，可自定义提示词 |

所选模型首次使用时下载。也可以选择 **其他 GGUF 模型文件...**，使用自己下载的 GGUF 格式模型。

模型在翻译时加载，之后在内存（或显存）中保留 5 分钟，期间再次翻译不必重新加载。

## OpenAI 兼容接口

用于任何提供 OpenAI 聊天接口（`/chat/completions`）的大模型服务：

| 项目 | 说明 |
| --- | --- |
| 接口地址 | 包含 `/v1` 的地址，例如 `https://api.deepseek.com/v1` |
| 模型名 | 例如 `deepseek-chat` |
| API 密钥 | 在线服务的密钥；本机运行的服务一般不需要，可不填 |

### 使用 Ollama

1. 安装 [Ollama](https://ollama.com/) 并下载模型，例如 `ollama pull qwen3:8b`。
2. 在 **设置 > 翻译** 中选择 **OpenAI 兼容接口**。
3. **接口地址** 填 `http://localhost:11434/v1`，**模型名** 填 `qwen3:8b`，**API 密钥** 留空。

对本机的服务，Glimpse 一次只发送一个段落，以免排队超时；对 Ollama 还会关闭模型的“思考”过程，以加快速度。模型首次加载可能需要一些时间，如提示超时，请稍候重试。

LM Studio 等其他本地服务的设置方法相同，接口地址请参考其说明（LM Studio 默认为 `http://localhost:1234/v1`）。

## 提示词

大语言模型类的引擎可以自定义提示词，用来指定译文的风格、术语或格式。

| 引擎 | 自定义提示词 |
| --- | --- |
| Claude | 可以。回复格式（各段落组成的 JSON 数组）由 Glimpse 自动附加，不受提示词影响 |
| OpenAI 兼容接口 | 可以 |
| 本地模型：Qwen3 4B 及其他通用 GGUF 模型 | 可以 |
| 本地模型：Hy-MT2、Index-Translate | 不可以，使用模型训练时的内置提示词 |
| B 站 Index-Translate（在线） | 不可以，使用固定的提示词 |
| Firefox、Google、DeepL、微软 | 不适用（传统机器翻译，没有提示词） |

自定义的 GGUF 文件名中含有 `hy-mt`、`hunyuan-mt` 或 `index-translate` 时，视为翻译专用模型，同样使用内置提示词。

### 默认提示词

在 **设置 > 翻译** 的 **默认提示词** 中修改，单击 **恢复默认** 可还原。`{language}` 代表目标语言，翻译时替换为语言名称。提示词中没有 `{language}` 时，Glimpse 会在末尾注明目标语言。

默认提示词为：

```
Translate the text into {language}. It was recognized from a screenshot and may contain OCR mistakes; read through them. Keep the meaning, tone, numbers, names and line breaks. Reply with the translation only, without notes or quotes.
```

（Claude 的默认提示词没有最后一句，回复格式由 Glimpse 另行附加。）

### 单次翻译的提示词

在文字识别和翻译窗口中单击 **提示词**，可以只为这个窗口修改提示词，不影响设置中的默认值。单击 **恢复默认** 可改回设置中的默认提示词。
