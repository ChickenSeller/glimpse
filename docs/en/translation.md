---
description: The translation engines and their settings, local models, OpenAI-compatible APIs (DeepSeek, Ollama and others), and custom prompts.
---

# Translation Engines and Prompts

**Settings > Translation** chooses the translation engine; the Recognize and Translate Text window can use another one for a single translation.

![Translation settings](/images/en/settings-translation.png)

## The engines

| Engine | Online | Key | Strengths |
| --- | --- | --- | --- |
| Firefox translation (default) | No | No | Fast and small; runs on any computer |
| Local model (llama.cpp) | No | No | A language model on this computer; faster with a graphics card |
| Google Translate (free) | Yes | No | Good quality, many languages; unofficial, so it may be limited |
| Bilibili Index-Translate (free) | Yes | No | A translation model with natural results, 150 languages; slower |
| DeepL | Yes | Yes | Natural translations, especially between European languages and Japanese; 500,000 characters a month free |
| Google Cloud Translation | Yes | Yes | Many languages, fast |
| Microsoft Translator | Yes | Yes | Many languages; 2 million characters a month free |
| Claude | Yes | Yes | A large language model: reads through OCR mistakes, keeps tone and terms; billed per use |
| OpenAI-compatible API | Depends | Depends | Online services such as DeepSeek or Qwen, or Ollama and LM Studio on this computer |

Translations go into Simplified Chinese, Traditional Chinese, Japanese, English, Korean, French, German, Spanish or Russian. The source language is detected.

::: info Privacy
With an offline engine, the text never leaves this computer. With an online one, the text is recognized here and sent to the chosen service; the capture itself is not sent. API keys are kept in your user settings on this computer.
:::

## Firefox translation

The engine of Firefox's offline translations (Bergamot). Each language direction downloads 35–70 MB of files the first time. It translates between English and other languages; other pairs, such as Chinese and Japanese, go through English.

Available on Windows only so far.

## Local models

A language model run on this computer by llama.cpp: on the graphics card through Vulkan where there is one, else on the CPU. Choose it in **Model**:

| Model | Size | Notes |
| --- | --- | --- |
| Hy-MT2 1.8B (default) | 1.1 GB | Light: runs on any laptop |
| Hy-MT2 7B | 4.6 GB | The best; wants a graphics card |
| Index-Translate 2B | 1.3 GB | By Bilibili, light |
| Index-Translate 9B | 5.8 GB | By Bilibili; wants a graphics card |
| Qwen3 4B | 2.5 GB | A general model; takes a custom prompt |

The chosen model is downloaded the first time. **Other GGUF model file...** uses a GGUF model you have downloaded yourself.

The model is loaded for translating and stays in memory (or video memory) for 5 minutes, so translating again meanwhile does not load it again.

## OpenAI-compatible API

For any language model served with OpenAI's chat API (`/chat/completions`):

| Field | Description |
| --- | --- |
| API address | The address with `/v1`, e.g. `https://api.deepseek.com/v1` |
| Model name | e.g. `deepseek-chat` |
| API key | The online service's key; a server on this computer usually needs none |

### With Ollama

1. Install [Ollama](https://ollama.com/) and pull a model, e.g. `ollama pull qwen3:8b`.
2. In **Settings > Translation**, choose **OpenAI-compatible API**.
3. Enter `http://localhost:11434/v1` as the **API address** and `qwen3:8b` as the **Model name**; leave the **API key** empty.

To a server on this computer, Glimpse sends one paragraph at a time, so that requests do not wait in line until they time out; with Ollama it also turns the model's thinking off, which makes it faster. Loading a model the first time can take a while; if it times out, wait a moment and try again.

LM Studio and other local servers are set up the same way; see their documentation for the address (LM Studio's default is `http://localhost:1234/v1`).

## Prompts

Language-model engines take a custom prompt, for the style, terms or format of the translation.

| Engine | Custom prompt |
| --- | --- |
| Claude | Yes. Glimpse adds the reply format (a JSON array of the paragraphs), whatever the prompt |
| OpenAI-compatible API | Yes |
| Local model: Qwen3 4B and other general GGUF models | Yes |
| Local model: Hy-MT2, Index-Translate | No: they use the prompt they were trained with |
| Bilibili Index-Translate (online) | No: it uses a fixed prompt |
| Firefox, Google, DeepL, Microsoft | Not applicable (machine translation, no prompt) |

A custom GGUF file whose name contains `hy-mt`, `hunyuan-mt` or `index-translate` is taken for a translation model and uses its built-in prompt as well.

### The default prompt

Change it in **Default prompt** in **Settings > Translation**; **Default Prompt** puts it back. `{language}` stands for the target language and is replaced by its name. Without `{language}` in the prompt, Glimpse names the target language at its end.

The default prompt is:

```
Translate the text into {language}. It was recognized from a screenshot and may contain OCR mistakes; read through them. Keep the meaning, tone, numbers, names and line breaks. Reply with the translation only, without notes or quotes.
```

(Claude's has no last sentence; Glimpse adds the reply format separately.)

### A prompt for one translation

Click **Prompt** in the Recognize and Translate Text window to change the prompt for that window only, leaving the default in the settings as it is. **Default** goes back to the default prompt.
