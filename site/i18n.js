// English is in the page itself; this holds the Chinese text and the switch.
(function () {
  var zh = {
    'nav.features': '功能',
    'nav.translate': '翻译',
    'nav.hotkeys': '快捷键',
    'nav.build': '构建',
    'hero.title': '截下屏幕，然后用它做点什么。',
    'hero.lead': 'Glimpse 能截图、录屏、标注，还能识别截图里的文字并翻译。需要时可以完全离线，免费且开源。',
    'hero.download': '下载',
    'hero.source': '源代码',
    'dl.github': '从 GitHub 下载',
    'dl.gitlab': '从 GitLab 下载',
    'hero.meta': 'Windows · Linux（X11 和 Wayland）· MIT 许可证',
    'features.title': '一个小工具栏，什么都有',
    'f.capture.t': '截图',
    'f.capture.d': '窗口、矩形、手绘区域或全屏，可设置延时。手绘区域以外的部分可以设为透明。',
    'f.edit.t': '标注',
    'f.edit.d': '箭头、形状、文字、序号、高亮和打码，然后保存、复制，或者保存并复制文件路径。',
    'f.rec.t': '录屏',
    'f.rec.d': '录制为 MP4，高亮鼠标指针和点击，并在底部显示按下的键或完整键盘。',
    'f.ocr.t': '文字识别',
    'f.ocr.d': '用 Windows OCR、Tesseract 或 PaddleOCR 把屏幕上任何地方的文字复制出来。',
    'f.tr.t': '翻译',
    'f.tr.d': '框选屏幕上的文字，用你的语言阅读，在线或完全离线都可以。',
    'f.qr.t': '二维码',
    'f.qr.d': '直接从屏幕识别二维码和条形码，不用掏手机。',
    'f.color.t': '屏幕取色',
    'f.color.d': '拾取屏幕上任意像素的颜色，复制为 HEX、RGB 或 HSL。',
    'f.cross.t': '十字线',
    'f.cross.d': '全屏十字线，显示坐标；锁定一个点后可以量出到另一点的距离，精确到像素。',
    'tr.title': '离线也能翻译',
    'tr.lead': '按电脑配置和文字内容选择引擎。离线引擎只需下载一次模型，屏幕内容不会发送到任何地方。',
    'tr.offline': '离线',
    'tr.online': '在线',
    'tr.firefox': 'Firefox 翻译',
    'tr.firefox.d': 'Mozilla 的 Bergamot 引擎。又快又轻，办公本也能流畅运行。',
    'tr.llm': '本地大模型',
    'tr.llm.d': '通过 llama.cpp 运行 Hy-MT2 或 Qwen3。优先使用显卡（Vulkan），不行时自动改用 CPU。',
    'hk.title': '快捷键',
    'hk.lead': '每个功能都有全局快捷键，都可以在设置中修改。',
    'hk.window': '窗口',
    'hk.region': '区域',
    'hk.full': '全屏',
    'hk.qr': '二维码',
    'hk.ocr': '文字识别',
    'hk.free': '手绘区域',
    'hk.color': '屏幕取色',
    'hk.cross': '十字线',
    'hk.rec': '录屏',
    'hk.tr': '翻译',
    'pf.title': '平台支持',
    'pf.capture': '区域、手绘区域和全屏截图',
    'pf.window': '窗口截图、截图中的鼠标指针',
    'pf.hotkeys': '全局快捷键',
    'pf.overlay': '录屏中的点击和按键显示',
    'pf.ocr': 'Tesseract 和 PaddleOCR',
    'pf.llm': '本地大模型翻译',
    'pf.firefox': 'Firefox 翻译',
    'pf.planned': '计划中',
    'b.title': '从源码构建',
    'b.lead': '需要 Qt 6.8 或更高版本、CMake 3.24 和支持 C++20 的编译器。其他库会优先使用系统已安装的版本，否则自动下载固定版本。',
    'foot.license': 'MIT 许可证',
    'foot.notices': '第三方声明',
    'foot.icons': '图标：Material Symbols'
  };

  var nodes = document.querySelectorAll('[data-i18n]');
  var en = {};
  for (var i = 0; i < nodes.length; i++) en[nodes[i].getAttribute('data-i18n')] = nodes[i].textContent;

  var button = document.getElementById('lang');

  function apply(lang) {
    var table = lang === 'zh' ? zh : en;
    for (var i = 0; i < nodes.length; i++) {
      var text = table[nodes[i].getAttribute('data-i18n')];
      if (text) nodes[i].textContent = text;
    }
    document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en';
    button.textContent = lang === 'zh' ? 'English' : '中文';
    try { localStorage.setItem('glimpse-lang', lang); } catch (e) {}
  }

  var saved = null;
  try { saved = localStorage.getItem('glimpse-lang'); } catch (e) {}
  var lang = saved || (/^zh/i.test(navigator.language || '') ? 'zh' : 'en');
  apply(lang);

  button.addEventListener('click', function () {
    lang = lang === 'zh' ? 'en' : 'zh';
    apply(lang);
  });

  // The download menu closes on a click elsewhere or Escape.
  var download = document.getElementById('download');
  document.addEventListener('click', function (event) {
    if (download.open && !download.contains(event.target)) download.open = false;
  });
  document.addEventListener('keydown', function (event) {
    if (event.key === 'Escape' && download.open) {
      download.open = false;
      download.querySelector('summary').focus();
    }
  });
})();
