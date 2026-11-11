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
      'tr.title': "离线也能翻译",
      'tr.lead': "按电脑配置和文字内容选择引擎。离线引擎只需下载一次模型，屏幕内容不会发送到任何地方；在线引擎也只会收到识别出的文字。",
      'tr.offline': "离线",
      'tr.online': "在线",
      'tr.firefox': "Firefox 翻译",
      'tr.firefox.d': "Mozilla 的 Bergamot 引擎。又快又轻，办公本也能流畅运行。",
      'tr.llm': "本地大模型",
      'tr.llm.d': "通过 llama.cpp 运行 Hy-MT2、Index-Translate 或 Qwen3，也可以用任意 GGUF 模型。优先使用显卡（Vulkan），不行时自动改用 CPU。",
      'hk.title': "在任何程序里，一键调用",
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
      'hero.eyebrow': "识别并翻译截图中的文字，离线也可以",
      'hero.title': "截下屏幕，然后用它做点什么。",
      'hero.lead': "Glimpse 能截图、录屏，能标注，能把截图贴在屏幕上，还能识别截图里的文字并翻译。需要时可以完全离线，免费且开源。",
      'hero.guide': "快速上手",
      'hero.meta': "Windows · Linux（X11 和 Wayland）· 简体中文、日本語、English · MIT 许可证",
      'fact.ocr': "文字识别，也支持 Windows OCR",
      'fact.engines.b': "9 种引擎",
      'fact.engines': "翻译，离线或在线",
      'fact.rec': "录屏，可显示点击和按键",
      'fact.portable.b': "绿色软件",
      'fact.portable': "解压即用，无需安装",
      'features.kicker': "功能",
      'features.title': "一个小工具栏，什么都有",
      'f.capture.t': "想截哪里就截哪里",
      'f.capture.d': "单击窗口或其中的某个控件、拖出矩形、手绘任意轮廓，或者截取全屏。选择时屏幕是冻结的，菜单和提示框不会消失。",
      'sc.capture.1': "滚动鼠标滚轮，在窗口和其中的控件之间切换",
      'sc.capture.2': "截取前可按像素微调选区",
      'sc.capture.3': "滚动截图，拼接长网页（Windows）",
      'f.ocr.t': "识别文字，顺手翻译",
      'f.ocr.d': "框选屏幕上的任何地方，把文字复制出来，图片里的、不让选中的都行。在同一个窗口里就能翻译。",
      'sc.ocr.1': "PaddleOCR、Windows OCR 或 Tesseract",
      'sc.ocr.2': "复制或翻译前可以修改识别结果",
      'sc.ocr.3': "大模型翻译可以自己写提示词",
      'f.image.t': "把截图整个翻译过来",
      'f.image.d': "Glimpse 抹去原文、补上背景，再按原文的颜色、字号和粗细把译文排回原处，就像汉化组的嵌字。",
      'sc.image.1': "保留列表、标题和对齐方式",
      'sc.image.2': "在预览窗口中打开，可保存或复制",
      'sc.image.3': "所有翻译引擎都能用",
      'cmp.before': "截图",
      'cmp.after': "翻译图片",
      'f.edit.t': "标注好，再发出去",
      'f.edit.d': "箭头、形状、文字、编号、高亮和打码；裁剪、剪除、调整大小、旋转。然后保存、复制，或者保存并复制文件路径。",
      'sc.edit.1': "自动递增的编号",
      'sc.edit.2': "模糊或马赛克遮住不该露出的内容",
      'sc.edit.3': "“保存并复制路径”，方便交给只认文件的工具",
      'more': "查看使用说明",
      'bento.kicker': "更多功能",
      'bento.title': "日常用得上的小工具",
      't.board.t': "屏幕白板",
      't.board.d': "在实时刷新的屏幕上直接画，也可以先冻结画面或换成白板再画。打开鼠标穿透，就能边画边操作下面的程序。",
      't.pin.t': "贴图",
      't.pin.d': "把截取的区域固定在所有窗口之上，就在截取的位置。可以拖动，用滚轮缩放，按住 Ctrl 滚动调整透明度。",
      't.rec.t': "录屏",
      't.rec.d': "把窗口或区域录制为 MP4，可高亮鼠标指针和点击，并在视频中显示按下的键或完整键盘。",
      't.scroll.t': "滚动截图",
      't.scroll.d': "Glimpse 自动滚动网页、文档或列表，拼成一张长图；也可以自己滚动，由它来拼。Windows，实验性。",
      't.qr.t': "二维码",
      't.qr.d': "直接从屏幕识别二维码和条形码，不用掏手机，还能直接打开链接。",
      't.cross.t': "十字线和取色",
      't.cross.d': "全屏十字线，显示坐标：锁定一个点后可以量出到另一点的距离；双击像素可以把颜色复制为 HEX、RGB 或 HSL。",
      't.ruler.t': "屏幕标尺",
      't.ruler.d': "横竖两把标尺交叉成透明的十字，浮在所有窗口之上，以像素读出鼠标到交叉点的距离。",
      'tr.kicker': "翻译",
      'tr.google': "Google 翻译",
      'tr.index': "B 站 Index-Translate",
      'tr.ms': "微软翻译",
      'tr.openai': "OpenAI 兼容接口（DeepSeek、Ollama 等）",
      'hk.kicker': "快捷键",
      'pf.kicker': "兼容性",
      'cta.title': "在你自己的屏幕上试试。",
      'cta.lead': "解压、运行，然后在任何选不中的文字上按 Ctrl+Alt+5。",
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
      'tr.title': "オフラインでも翻訳",
      'tr.lead': "コンピューターと文章に合ったエンジンを選べます。オフラインのエンジンはモデルを一度ダウンロードするだけで、画面の内容をどこにも送りません。オンラインのエンジンにも、認識した文字だけを送ります。",
      'tr.offline': "オフライン",
      'tr.online': "オンライン",
      'tr.firefox': "Firefox 翻訳",
      'tr.firefox.d': "Mozilla の Bergamot エンジン。速くて軽く、事務用ノート PC でも快適に動きます。",
      'tr.llm': "ローカル LLM",
      'tr.llm.d': "llama.cpp で Hy-MT2、Index-Translate、Qwen3、または任意の GGUF モデルを実行。グラフィックカード（Vulkan）を使い、使えなければ CPU で動作。",
      'hk.title': "どのプログラムからでも、キー 1 つで",
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
      'hero.eyebrow': "スクリーンショットの文字を認識・翻訳、オフラインでも",
      'hero.title': "画面をキャプチャして、その先まで。",
      'hero.lead': "Glimpse はスクリーンショットと画面録画、注釈、画面へのピン留めに加えて、画像内の文字を認識して翻訳できます。オフラインでも使える、無料のオープンソースソフトウェアです。",
      'hero.guide': "クイックスタート",
      'hero.meta': "Windows · Linux（X11 と Wayland）· 日本語、English、简体中文 · MIT ライセンス",
      'fact.ocr': "文字認識。Windows OCR にも対応",
      'fact.engines.b': "9 種類のエンジン",
      'fact.engines': "翻訳はオフラインでもオンラインでも",
      'fact.rec': "クリックやキーも映せる画面録画",
      'fact.portable.b': "ポータブル",
      'fact.portable': "展開してすぐ使え、インストール不要",
      'features.kicker': "機能",
      'features.title': "小さなツールバーに、すべてを",
      'f.capture.t': "撮りたいところを正確に",
      'f.capture.d': "ウィンドウやその中のコントロールをクリック、矩形をドラッグ、フリーハンドで囲む、または全画面。選んでいる間は画面が静止するので、メニューやツールチップも消えません。",
      'sc.capture.1': "マウスホイールでウィンドウと内側のコントロールを切り替え",
      'sc.capture.2': "取り込む前にピクセル単位で範囲を調整",
      'sc.capture.3': "スクロールキャプチャで長いページをつなげる（Windows）",
      'f.ocr.t': "文字を読み取って、そのまま翻訳",
      'f.ocr.d': "画面のどこでも範囲を選んで文字をコピーできます。画像の中の文字や、選択できない文字も。同じウィンドウで翻訳まで行えます。",
      'sc.ocr.1': "PaddleOCR、Windows OCR、Tesseract",
      'sc.ocr.2': "コピーや翻訳の前に認識結果を修正",
      'sc.ocr.3': "言語モデルには自分のプロンプトを",
      'f.image.t': "画像ごと翻訳",
      'f.image.d': "Glimpse は元の文字を消して背景を補い、元の色・大きさ・太さに合わせて訳文を配置します。翻訳版の写植のように仕上がります。",
      'sc.image.1': "箇条書き、見出し、揃え方を保持",
      'sc.image.2': "プレビューで開いて保存・コピー",
      'sc.image.3': "どの翻訳エンジンでも使える",
      'cmp.before': "キャプチャ",
      'cmp.after': "翻訳画像",
      'f.edit.t': "注釈を付けて、すぐ共有",
      'f.edit.d': "矢印、図形、テキスト、番号、ハイライト、ぼかし。トリミング、切り取り、サイズ変更、回転も。保存、コピー、または保存してパスをコピー。",
      'sc.edit.1': "自動で増える番号",
      'sc.edit.2': "見せたくない部分はぼかしやモザイクで",
      'sc.edit.3': "「保存してパスをコピー」でファイルを受け取るツールへ",
      'more': "使い方を見る",
      'bento.kicker': "その他の機能",
      'bento.title': "毎日使える小さな道具",
      't.board.t': "スクリーンペン",
      't.board.d': "更新され続けるライブの画面、静止した画面、ホワイトボードのどれにでも描けます。クリックスルーにすれば、描いた内容を残したまま下のプログラムを操作できます。",
      't.pin.t': "画面にピン留め",
      't.pin.d': "キャプチャした領域を、撮った位置のまますべてのウィンドウの前面に固定。ドラッグで移動、ホイールで拡大縮小、Ctrl+ホイールで透明度を変更。",
      't.rec.t': "画面録画",
      't.rec.d': "ウィンドウや範囲を MP4 に録画。ポインターやクリックを強調し、押したキーやキーボード全体を動画に表示できます。",
      't.scroll.t': "スクロールキャプチャ",
      't.scroll.d': "ページや文書、リストを Glimpse がスクロールして 1 枚の縦長画像に。自分でスクロールして、つなげるのを任せることも。Windows、試験的。",
      't.qr.t': "QR コード",
      't.qr.d': "画面上の QR コードやバーコードをスマートフォンなしで読み取り、リンクも開けます。",
      't.cross.t': "クロスヘアと色の取得",
      't.cross.d': "座標付きの全画面クロスヘア。点を固定して別の点までの距離を測ったり、ピクセルをダブルクリックして色を HEX、RGB、HSL でコピーしたりできます。",
      't.ruler.t': "スクリーンルーラー",
      't.ruler.d': "水平と垂直の定規を十字に重ねて半透明で最前面に表示し、交点からポインターまでの距離をピクセル単位で示します。",
      'tr.kicker': "翻訳",
      'tr.google': "Google 翻訳",
      'tr.index': "bilibili Index-Translate",
      'tr.ms': "Microsoft 翻訳",
      'tr.openai': "OpenAI 互換 API（DeepSeek、Ollama など）",
      'hk.kicker': "ホットキー",
      'pf.kicker': "対応状況",
      'cta.title': "自分の画面で試してみてください。",
      'cta.lead': "展開して起動し、選択できない文字の上で Ctrl+Alt+5 を押すだけです。",
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
