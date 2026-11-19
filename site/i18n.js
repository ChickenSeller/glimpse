// English is in the page itself; this holds the Chinese and Japanese text and the switch.
(function () {
  var text = {
    zh: {
      'nav.features': "功能",
      'nav.translate': "翻译",
      'nav.hotkeys': "快捷键",
      'nav.build': "构建",
      'hero.download': "下载",
      'hero.source': "源代码",
      'dl.site': "从本站下载",
      'nav.releases': "版本",
      'rel.title': "版本",
      'rel.lead': "从本站下载各个版本的 Windows 和 Linux 安装包，与 GitLab 上的发布内容相同。",
      'rel.latest': "最新",
      'rel.download': "下载",
      'rel.dl.windows': "下载 Windows 版",
      'rel.dl.deb': "下载 Linux 版（.deb）",
      'rel.dl.rpm': "下载 Linux 版（.rpm）",
      'rel.dl.other': "下载",
      'rel.pkg.windows': "Windows x64",
      'rel.pkg.deb': "Linux .deb",
      'rel.pkg.rpm': "Linux .rpm",
      'rel.note.deb': "Debian、Ubuntu",
      'rel.note.rpm': "Fedora、openSUSE、RHEL",
      'rel.missing': "该版本未提供",
      'rel.elsewhere': "也可以从这里下载：",
      'rel.loading': "正在读取版本列表...",
      'rel.none': "还没有发布任何版本。",
      'rel.error': "暂时无法读取版本列表，可以先到上面的 GitHub 或 GitLab 下载。",
      'dl.github': "从 GitHub 下载",
      'dl.gitlab': "从 GitLab 下载",
      'tr.title': "翻译引擎",
      'tr.lead': "可根据电脑性能和文本内容选择引擎。离线引擎只需下载一次模型，不发送任何内容；在线引擎只接收识别出的文字。",
      'tr.offline': "离线",
      'tr.online': "在线",
      'tr.firefox': "Firefox 翻译",
      'tr.firefox.d': "Mozilla 的 Bergamot 引擎，速度快、占用小，普通笔记本即可运行。",
      'tr.llm': "本地大模型",
      'tr.llm.d': "通过 llama.cpp 运行 Hy-MT2、Index-Translate、Qwen3 或其他 GGUF 模型。有显卡时使用显卡（Vulkan），否则使用 CPU。",
      'hk.title': "全局快捷键",
      'hk.lead': "每个功能都有全局快捷键，都可以在设置中修改。",
      'hk.window': "窗口或区域",
      'hk.board': "屏幕白板",
      'hk.full': "全屏",
      'hk.pin': "贴图到屏幕",
      'hk.qr': "二维码",
      'hk.ocr': "文字识别和翻译",
      'hk.free': "手绘区域",
      'hk.ruler': "屏幕标尺",
      'hk.cross': "十字线和取色",
      'hk.scroll': "滚动截图（实验性）",
      'hk.rec': "录屏",
      'pf.title': "Windows 和 Linux",
      'pf.capture': "矩形、手绘区域和全屏截图",
      'pf.window': "选取窗口、截图中的鼠标指针",
      'pf.scroll': "滚动截图（实验性）",
      'pf.hotkeys': "全局快捷键",
      'pf.sound': "录屏时录制麦克风和电脑声音",
      'pf.overlay': "录屏中的点击和按键显示",
      'pf.ocr': "Tesseract 和 PaddleOCR",
      'pf.llm': "本地大模型翻译",
      'pf.firefox': "Firefox 翻译",
      'pf.planned': "计划中",
      'b.title': "从源码构建",
      'b.lead': "需要 Qt 6.8 或更高版本、CMake 3.24 和支持 C++20 的编译器。其他库会优先使用系统已安装的版本，否则自动下载固定版本。",
      'foot.license': "MIT 许可证",
      'foot.notices': "第三方声明",
      'foot.icons': "图标：Material Symbols",
      'nav.guide': "使用说明",
      'hero.eyebrow': "支持离线文字识别与翻译",
      'hero.title': "截图、录屏与文字识别工具",
      'hero.lead': "Glimpse 可以截图和录屏，为截图添加标注或贴在屏幕上，并识别和翻译截图中的文字。支持离线使用，免费开源。",
      'hero.guide': "快速上手",
      'hero.meta': "Windows · Linux（X11 和 Wayland）· 简体中文、日本語、English · MIT 许可证",
      'fact.ocr': "文字识别，也支持 Windows OCR",
      'fact.engines.b': "9 种引擎",
      'fact.engines': "翻译，离线或在线",
      'fact.rec': "录屏，可显示点击和按键",
      'fact.portable.b': "免安装",
      'fact.portable': "解压即可使用",
      'features.kicker': "功能",
      'features.title': "主要功能",
      'f.capture.t': "截图",
      'f.capture.d': "可截取窗口或其中的控件、矩形区域、手绘区域或全屏。选择区域时屏幕处于冻结状态，菜单和提示框也能截取。",
      'sc.capture.1': "用鼠标滚轮在窗口与其中的控件之间切换",
      'sc.capture.2': "截取前可按像素调整选区",
      'sc.capture.3': "滚动截图，适用于长网页（Windows）",
      'f.ocr.t': "文字识别与翻译",
      'f.ocr.d': "识别屏幕上任意区域中的文字，包括图片中的文字和无法选中的文字，并在同一窗口中复制或翻译。",
      'sc.ocr.1': "PaddleOCR、Windows OCR 或 Tesseract",
      'sc.ocr.2': "复制或翻译前可修改识别结果",
      'sc.ocr.3': "大语言模型类引擎支持自定义提示词",
      'f.image.t': "翻译图片",
      'f.image.d': "Glimpse 去除原文并还原背景，按原文的颜色、字号和粗细将译文排入原来的位置。",
      'sc.image.1': "保留列表、标题和对齐方式",
      'sc.image.2': "在预览窗口中打开，可保存或复制",
      'sc.image.3': "适用于所有翻译引擎",
      'cmp.before': "原图",
      'cmp.after': "翻译图片",
      'f.edit.t': "标注与编辑",
      'f.edit.d': "可添加箭头、形状、文字、编号、高亮和模糊，也可裁剪、剪除、调整大小或旋转图像。完成后可保存、复制，或保存并复制文件路径。",
      'sc.edit.1': "自动递增的编号",
      'sc.edit.2': "用模糊或马赛克遮盖敏感信息",
      'sc.edit.3': "“保存并复制路径”，便于交给接受文件路径的工具",
      'more': "查看使用说明",
      'bento.kicker': "更多功能",
      'bento.title': "其他工具",
      't.board.t': "屏幕白板",
      't.board.d': "可在实时屏幕、冻结的屏幕或白板上绘制。开启鼠标穿透后，绘制的内容保留，鼠标可操作下层的程序。",
      't.pin.t': "贴图",
      't.pin.d': "将截取的区域固定在所有窗口之上，位置与截取时相同。可拖动、用滚轮缩放，按住 Ctrl 滚动可调整不透明度。",
      't.rec.t': "录屏",
      't.rec.d': "将窗口或区域录制为 MP4，可同时录制麦克风和电脑播放的声音，高亮鼠标指针和点击，并在视频中显示按键或完整键盘。",
      't.scroll.t': "滚动截图",
      't.scroll.d': "自动滚动网页、文档或列表并拼接为一张长图，也支持手动滚动。仅限 Windows，实验性功能。",
      't.qr.t': "二维码",
      't.qr.d': "识别屏幕上的二维码和条形码，并可打开其中的链接。",
      't.cross.t': "十字线和取色",
      't.cross.d': "全屏十字线，显示坐标。锁定一点后可测量到另一点的距离；双击像素可将颜色复制为 HEX、RGB 或 HSL。",
      't.ruler.t': "屏幕标尺",
      't.ruler.d': "水平和垂直两把标尺交叉显示，半透明并置于最上层，以像素显示鼠标到交叉点的距离。",
      'tr.kicker': "翻译",
      'tr.google': "Google 翻译",
      'tr.index': "B 站 Index-Translate",
      'tr.ms': "微软翻译",
      'tr.openai': "OpenAI 兼容接口（DeepSeek、Ollama 等）",
      'hk.kicker': "快捷键",
      'pf.kicker': "兼容性",
      'cta.title': "下载 Glimpse",
      'cta.lead': "解压后运行 glimpse.exe 即可使用，无需安装。",
      'cta.guide': "阅读使用说明"
    },
    ja: {
      'nav.features': "機能",
      'nav.translate': "翻訳",
      'nav.hotkeys': "ショートカット",
      'nav.build': "ビルド",
      'hero.download': "ダウンロード",
      'hero.source': "ソースコード",
      'dl.site': "このサイトから",
      'nav.releases': "リリース",
      'rel.title': "リリース",
      'rel.lead': "各バージョンの Windows / Linux 版パッケージをこのサイトからダウンロードできます。GitLab のリリースと同じ内容です。",
      'rel.latest': "最新",
      'rel.download': "ダウンロード",
      'rel.dl.windows': "Windows 版をダウンロード",
      'rel.dl.deb': "Linux 版（.deb）をダウンロード",
      'rel.dl.rpm': "Linux 版（.rpm）をダウンロード",
      'rel.dl.other': "ダウンロード",
      'rel.pkg.windows': "Windows x64",
      'rel.pkg.deb': "Linux .deb",
      'rel.pkg.rpm': "Linux .rpm",
      'rel.note.deb': "Debian、Ubuntu",
      'rel.note.rpm': "Fedora、openSUSE、RHEL",
      'rel.missing': "このリリースにはありません",
      'rel.elsewhere': "こちらからも入手できます：",
      'rel.loading': "リリース一覧を読み込んでいます...",
      'rel.none': "まだリリースはありません。",
      'rel.error': "現在リリース一覧を読み込めません。上の GitHub か GitLab からダウンロードしてください。",
      'dl.github': "GitHub から",
      'dl.gitlab': "GitLab から",
      'tr.title': "翻訳エンジン",
      'tr.lead': "コンピューターの性能や文章に応じてエンジンを選べます。オフラインのエンジンはモデルを一度ダウンロードするだけで、何も送信しません。オンラインのエンジンには、認識した文字だけを送信します。",
      'tr.offline': "オフライン",
      'tr.online': "オンライン",
      'tr.firefox': "Firefox 翻訳",
      'tr.firefox.d': "Mozilla の Bergamot エンジン。高速かつ軽量で、一般的なノート PC で動作します。",
      'tr.llm': "ローカル LLM",
      'tr.llm.d': "llama.cpp で Hy-MT2、Index-Translate、Qwen3 などの GGUF モデルを実行します。グラフィックカード（Vulkan）があれば使用し、なければ CPU で動作します。",
      'hk.title': "グローバルホットキー",
      'hk.lead': "すべての機能にグローバルショートカットがあり、どれも設定で変更できます。",
      'hk.window': "ウィンドウまたは領域",
      'hk.board': "画面ホワイトボード",
      'hk.full': "全画面",
      'hk.pin': "画面に貼り付け",
      'hk.qr': "QR コード",
      'hk.ocr': "文字認識と翻訳",
      'hk.free': "フリーハンド領域",
      'hk.ruler': "スクリーンルーラー",
      'hk.cross': "十字線とカラーピッカー",
      'hk.scroll': "スクロールキャプチャ（試験的）",
      'hk.rec': "画面録画",
      'pf.title': "Windows と Linux",
      'pf.capture': "矩形、フリーハンド、全画面のキャプチャ",
      'pf.window': "ウィンドウの選択、キャプチャ内のマウスポインター",
      'pf.scroll': "スクロールキャプチャ（試験的）",
      'pf.hotkeys': "グローバルショートカット",
      'pf.sound': "録画時のマイクと再生中の音の録音",
      'pf.overlay': "録画中のクリックとキーの表示",
      'pf.ocr': "Tesseract と PaddleOCR",
      'pf.llm': "ローカル LLM による翻訳",
      'pf.firefox': "Firefox 翻訳",
      'pf.planned': "予定",
      'b.title': "ソースからビルド",
      'b.lead': "Qt 6.8 以降、CMake 3.24、C++20 対応のコンパイラーが必要です。その他のライブラリはシステムにあるものを使い、なければ固定バージョンを自動でダウンロードします。",
      'foot.license': "MIT ライセンス",
      'foot.notices': "サードパーティーの表示",
      'foot.icons': "アイコン：Material Symbols",
      'nav.guide': "使い方",
      'hero.eyebrow': "オフラインでの文字認識と翻訳に対応",
      'hero.title': "スクリーンショット・画面録画・文字認識ツール",
      'hero.lead': "Glimpse は画面のキャプチャと録画、注釈の追加、画面へのピン留めを行い、キャプチャ内の文字を認識して翻訳できます。オフラインでも使用できる、無料のオープンソースソフトウェアです。",
      'hero.guide': "クイックスタート",
      'hero.meta': "Windows · Linux（X11 と Wayland）· 日本語、English、简体中文 · MIT ライセンス",
      'fact.ocr': "文字認識。Windows OCR にも対応",
      'fact.engines.b': "9 種類のエンジン",
      'fact.engines': "翻訳はオフラインでもオンラインでも",
      'fact.rec': "クリックやキーも映せる画面録画",
      'fact.portable.b': "インストール不要",
      'fact.portable': "展開するだけで使用可能",
      'features.kicker': "機能",
      'features.title': "主な機能",
      'f.capture.t': "画面のキャプチャ",
      'f.capture.d': "ウィンドウやその中のコントロール、矩形、フリーハンド領域、全画面をキャプチャできます。範囲の選択中は画面が静止するため、メニューやツールチップもキャプチャできます。",
      'sc.capture.1': "マウスホイールでウィンドウと内側のコントロールを切り替え",
      'sc.capture.2': "取り込む前にピクセル単位で範囲を調整",
      'sc.capture.3': "スクロールキャプチャで長いページにも対応（Windows）",
      'f.ocr.t': "文字認識と翻訳",
      'f.ocr.d': "画像内の文字や選択できない文字を含め、画面上の任意の範囲の文字を認識し、同じウィンドウでコピーや翻訳ができます。",
      'sc.ocr.1': "PaddleOCR、Windows OCR、Tesseract",
      'sc.ocr.2': "コピーや翻訳の前に認識結果を修正可能",
      'sc.ocr.3': "大規模言語モデル系のエンジンはプロンプトを変更可能",
      'f.image.t': "翻訳画像",
      'f.image.d': "元の文字を消して背景を復元し、元の色・大きさ・太さに合わせて訳文を同じ位置に配置します。",
      'sc.image.1': "箇条書き、見出し、揃え方を保持",
      'sc.image.2': "プレビューウィンドウで開き、保存やコピーが可能",
      'sc.image.3': "すべての翻訳エンジンで利用可能",
      'cmp.before': "元の画像",
      'cmp.after': "翻訳画像",
      'f.edit.t': "注釈と編集",
      'f.edit.d': "矢印、図形、テキスト、番号、ハイライト、ぼかしを追加でき、トリミング、切り取り、サイズ変更、回転も行えます。編集後は保存、コピー、または保存してパスをコピーできます。",
      'sc.edit.1': "自動で増える番号",
      'sc.edit.2': "ぼかしやモザイクで機密情報を隠す",
      'sc.edit.3': "「保存してパスをコピー」で、ファイルパスを受け付けるツールに渡せる",
      'more': "使い方を見る",
      'bento.kicker': "その他の機能",
      'bento.title': "その他のツール",
      't.board.t': "スクリーンペン",
      't.board.d': "ライブの画面、静止した画面、ホワイトボードに描画できます。クリックスルーをオンにすると、描いた内容を残したまま下のプログラムを操作できます。",
      't.pin.t': "画面にピン留め",
      't.pin.d': "キャプチャした領域を、キャプチャした位置のまますべてのウィンドウの前面に固定します。ドラッグで移動、ホイールで拡大縮小、Ctrl+ホイールで不透明度を変更できます。",
      't.rec.t': "画面録画",
      't.rec.d': "ウィンドウや範囲を MP4 に録画します。マイクや再生中の音の録音、ポインターやクリックの強調、押したキーやキーボード全体の表示にも対応しています。",
      't.scroll.t': "スクロールキャプチャ",
      't.scroll.d': "ページ、文書、リストを自動でスクロールし、1 枚の画像につなげます。手動スクロールにも対応。Windows のみ、試験的機能です。",
      't.qr.t': "QR コード",
      't.qr.d': "画面上の QR コードやバーコードを読み取り、含まれるリンクを開けます。",
      't.cross.t': "クロスヘアと色の取得",
      't.cross.d': "座標付きの全画面クロスヘアです。点を固定して別の点までの距離を測ったり、ピクセルをダブルクリックして色を HEX、RGB、HSL でコピーしたりできます。",
      't.ruler.t': "スクリーンルーラー",
      't.ruler.d': "水平と垂直の定規を十字に重ねて半透明で最前面に表示し、交点からポインターまでの距離をピクセル単位で示します。",
      'tr.kicker': "翻訳",
      'tr.google': "Google 翻訳",
      'tr.index': "bilibili Index-Translate",
      'tr.ms': "Microsoft 翻訳",
      'tr.openai': "OpenAI 互換 API（DeepSeek、Ollama など）",
      'hk.kicker': "ホットキー",
      'pf.kicker': "対応状況",
      'cta.title': "Glimpse をダウンロード",
      'cta.lead': "アーカイブを展開して glimpse.exe を実行するだけで使用できます。インストールは不要です。",
      'cta.guide': "使い方を読む"
    }
  };
  var names = {en: 'English', zh: '简体中文', ja: '日本語'};
  var htmlLang = {en: 'en', zh: 'zh-CN', ja: 'ja'};

  var button = document.getElementById('lang');
  var menu = button.parentNode;

  // Rescans the page each time, so parts added later (the release list) are
  // translated too; each element keeps its English text in data-en.
  function apply(lang) {
    var table = text[lang] || {};
    var nodes = document.querySelectorAll('[data-i18n]');
    for (var i = 0; i < nodes.length; i++) {
      var node = nodes[i];
      if (node.dataset.en === undefined) node.dataset.en = node.textContent;
      node.textContent = table[node.getAttribute('data-i18n')] || node.dataset.en;
    }
    document.documentElement.lang = htmlLang[lang];
    // The guide has a version per language: Chinese at its root.
    // data-guide holds the page within the guide ("" for its start).
    var guides = document.querySelectorAll('[data-guide]');
    for (var g = 0; g < guides.length; g++)
      guides[g].setAttribute('href', 'guide/' + {zh: '', en: 'en/', ja: 'ja/'}[lang] + guides[g].getAttribute('data-guide'));
    // Screenshots in the page's language.
    var shots = document.querySelectorAll('[data-shot]');
    for (var t = 0; t < shots.length; t++)
      shots[t].setAttribute('src', 'guide/images/' + {zh: '', en: 'en/', ja: 'ja/'}[lang] + shots[t].getAttribute('data-shot'));
    button.textContent = names[lang];
    var items = menu.querySelectorAll('[data-lang]');
    for (var j = 0; j < items.length; j++)
      items[j].setAttribute('aria-current', items[j].getAttribute('data-lang') === lang ? 'true' : 'false');
    try { localStorage.setItem('glimpse-lang', lang); } catch (e) {}
  }

  function preferred() {
    var saved = null;
    try { saved = localStorage.getItem('glimpse-lang'); } catch (e) {}
    if (names[saved]) return saved;
    var langs = navigator.languages || [navigator.language || ''];
    for (var i = 0; i < langs.length; i++) {
      if (/^zh/i.test(langs[i])) return 'zh';
      if (/^ja/i.test(langs[i])) return 'ja';
      if (/^en/i.test(langs[i])) return 'en';
    }
    return 'en';
  }

  var lang = preferred();
  apply(lang);

  menu.addEventListener('click', function (event) {
    var item = event.target.closest('[data-lang]');
    if (!item) return;
    event.preventDefault();
    lang = item.getAttribute('data-lang');
    apply(lang);
    menu.open = false;
  });
  window.glimpseRetranslate = function () { apply(lang); };

  // Drop-down menus (download, language, the release list's) close on a click
  // elsewhere or Escape; one is open at a time.
  function openMenus() {
    return Array.prototype.slice.call(document.querySelectorAll('details.dropdown[open]'));
  }
  document.addEventListener('click', function (event) {
    openMenus().forEach(function (open) {
      if (!open.contains(event.target)) open.open = false;
    });
  });
  document.addEventListener('keydown', function (event) {
    if (event.key !== 'Escape') return;
    openMenus().forEach(function (open) {
      open.open = false;
      open.querySelector('summary').focus();
    });
  });

  // Sections fade in as they scroll into view.
  if ('IntersectionObserver' in window) {
    var seen = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { e.target.classList.add('in'); seen.unobserve(e.target); }
      });
    }, {rootMargin: '0px 0px -10% 0px'});
    document.querySelectorAll('.show, .tile, .head, .checks, .split > *').forEach(function (el) {
      el.classList.add('reveal'); seen.observe(el);
    });
  }
})();
