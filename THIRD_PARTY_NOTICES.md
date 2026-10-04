# Third-party notices

Glimpse itself is released under the MIT License (see `LICENSE`). It is built
on the components below, which keep their own licenses. Their license texts
are in the `licenses/` folder next to this file.

## Bundled with Glimpse

| Component | Version | License | License text | How Glimpse uses it |
|---|---|---|---|---|
| [Qt](https://www.qt.io/) (Core, Gui, Widgets, Svg, Network, Concurrent, Multimedia) | 6.12.0 | LGPL-3.0 | `Qt-GPL-3.0-and-LGPL-3.0.txt` | Shared libraries (DLLs) |
| [FFmpeg](https://ffmpeg.org/), shipped with Qt Multimedia | as in Qt 6.12.0 | LGPL-2.1-or-later | `FFmpeg-LGPL-2.1.txt` | Shared libraries (DLLs) |
| [kImageAnnotator](https://github.com/ksnip/kImageAnnotator) | 0.7.2 | LGPL-3.0 | `kImageAnnotator-LGPL-3.0.txt` | Shared library (annotation editor) |
| [kColorPicker](https://github.com/ksnip/kColorPicker) | 0.3.1 | LGPL-3.0 | `kColorPicker-LGPL-3.0.txt` | Shared library (annotation editor) |
| [ZXing-C++](https://github.com/zxing-cpp/zxing-cpp) | 3.1.1 | Apache-2.0 | `ZXing-cpp-Apache-2.0.txt` | Linked statically (QR codes) |
| [Tesseract](https://github.com/tesseract-ocr/tesseract) | 5.5.3 | Apache-2.0 | `Tesseract-Apache-2.0.txt` | Linked statically (text recognition) |
| [Leptonica](http://www.leptonica.org/) | 1.87.0 | BSD-2-Clause | `Leptonica-BSD-2-Clause.txt` | Linked statically (used by Tesseract) |
| [ONNX Runtime](https://onnxruntime.ai/) | 1.30.0 | MIT | `ONNXRuntime-MIT.txt`, `ONNXRuntime-ThirdPartyNotices.txt` | Shared library (PaddleOCR) |
| [llama.cpp](https://github.com/ggml-org/llama.cpp) (llama, ggml) | b11387 | MIT | `llama.cpp-MIT.txt`, `llama.cpp-nlohmann-json-MIT.txt` | Shared libraries (local translation) |
| LLVM OpenMP runtime (`libomp.dll`, shipped with llama.cpp) | | Apache-2.0 WITH LLVM-exception | `LLVM-OpenMP-Apache-2.0-with-LLVM-exception.txt` | Shared library |
| [Material Symbols](https://fonts.google.com/icons) | | Apache-2.0 | `Apache-2.0.txt` | Toolbar and window icons |
| MinGW-w64 GCC runtime (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`) | 13.1.0 | GPL-3.0 WITH GCC-exception-3.1 | `GCC-Runtime-Library-Exception-3.1.txt`, `GCC-GPL-3.0.txt` | Shared libraries |
| MinGW-w64 winpthreads (`libwinpthread-1.dll`) | | MIT-style | `MinGW-w64-winpthreads.txt` | Shared library |

### Firefox translation engine (`glimpse-bergamot.dll`)

Built from Mozilla's translation engine; all of the following are compiled
into that one library.

| Component | License | License text |
|---|---|---|
| [Bergamot translator](https://github.com/mozilla/translations) (inference) | MPL-2.0 | `Bergamot-MPL-2.0.txt` |
| [Marian NMT](https://marian-nmt.github.io/) (Mozilla's fork) | MIT | `Marian-MIT.txt` |
| [SentencePiece](https://github.com/google/sentencepiece) | Apache-2.0 | `SentencePiece-Apache-2.0.txt` |
| protobuf-lite (inside SentencePiece) | BSD-3-Clause | `protobuf-lite-BSD-3-Clause.txt` |
| [intgemm](https://github.com/kpu/intgemm) | MIT | `intgemm-MIT.txt` |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | MIT | `yaml-cpp-MIT.txt` |
| [pathie-cpp](https://github.com/Quintus/pathie-cpp) | BSD-2-Clause | `pathie-cpp-BSD-2-Clause.txt` |
| [spdlog](https://github.com/gabime/spdlog) | MIT | `spdlog-MIT.txt` |
| [ONNX.js](https://github.com/microsoft/onnxjs) (matrix kernels) | MIT | `onnxjs-MIT.txt` |
| [Eigen](https://eigen.tuxfamily.org/) (MPL-2.0 parts only: built with `EIGEN_MPL2_ONLY`) | MPL-2.0 | `Eigen-MPL-2.0.txt` |
| [ssplit-cpp](https://github.com/browsermt/ssplit-cpp) | Apache-2.0 | `ssplit-cpp-Apache-2.0.txt` |
| [PCRE2](https://github.com/PCRE2Project/pcre2) | BSD-3-Clause with PCRE2 exception | `PCRE2-BSD-3-Clause.txt` |

The MPL-2.0 covers the files of those projects only. Their source code is
available at the links above (Glimpse builds `mozilla/translations` at commit
`69455acaecbe8650cdba988dbcf7c10ca20e7c48`).

## LGPL components

Qt, FFmpeg, kImageAnnotator and kColorPicker are used as separate shared
libraries (DLLs next to `glimpse.exe`) and are not modified by Glimpse. You may
replace them with your own builds of compatible versions; Glimpse will use
them. Their source code is available from their projects (links above) and,
for Qt, from <https://download.qt.io/official_releases/qt/>. Qt itself
contains further third-party code; see
<https://doc.qt.io/qt-6/licenses-used-in-qt.html>.

## Downloaded on first use (not bundled)

These are fetched from their publishers only when you choose the feature, and
are used under their own terms:

| Data | Publisher | License |
|---|---|---|
| PaddleOCR models (PP-OCR, ONNX) | PaddlePaddle | Apache-2.0 |
| Tesseract language data (`*.traineddata`) | tesseract-ocr/tessdata | Apache-2.0 |
| Hy-MT2 1.8B / 7B (GGUF) | Tencent | Apache-2.0 |
| Qwen3 4B (GGUF) | Alibaba Qwen | Apache-2.0 |
| Firefox translation models | Mozilla | See <https://github.com/mozilla/translations> |

## Online services

The online translation services (Google Translate's free service, DeepL,
Google Cloud Translation, Microsoft Translator, Anthropic Claude) are operated
by their providers under their own terms of service. Text is sent to them only
when you choose them. The free Google service is unofficial and may change or
stop working at any time.
