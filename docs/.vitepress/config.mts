import { defineConfig, type DefaultTheme } from 'vitepress'

// The guide is published under the homepage, at <homepage>/guide/, with the
// Chinese pages at the root and the English and Japanese ones under /en/ and
// /ja/. DOCS_BASE and DOCS_HOME can override the paths.
const base = process.env.DOCS_BASE || '/kaguya/glimpse/guide/'
const home = process.env.DOCS_HOME || 'https://pages.yanlei.org/kaguya/glimpse/'

type Names = Record<string, string>

// Heading anchors as VitePress makes them, but composed (NFC): its own
// slugify decomposes kana with voiced marks (ド into ト and ゛), so links
// written with the heading's text would miss.
function slugify(text: string): string {
  return text
    .normalize('NFC')
    .trim()
    .toLowerCase()
    .replace(/[\s!"#$%&'()*+,./:;<=>?@[\\\]^`{|}~、。，：；（）「」・／]+/g, '-')
    .replace(/^-+|-+$/g, '')
}

// One sidebar per language, from the page titles in that language.
function sidebar(prefix: string, groups: [string, Names][]): DefaultTheme.SidebarItem[] {
  return groups.map(([text, pages]) => ({
    text,
    items: Object.entries(pages).map(([page, title]) => ({ text: title, link: `${prefix}/${page}` })),
  }))
}

const zh = sidebar('', [
  ['开始', { '': '简介', 'getting-started': '快速上手', install: '下载与安装', toolbar: '工具栏、托盘与快捷键' }],
  ['截图', { capture: '截图', scrolling: '滚动截图', editor: '预览与编辑', pin: '贴图' }],
  ['文字', { ocr: '文字识别与翻译', translation: '翻译引擎与提示词' }],
  ['其他工具', { record: '录屏', whiteboard: '屏幕白板', tools: '二维码、十字线与标尺' }],
  ['其他', { settings: '设置', linux: 'Linux', faq: '常见问题' }],
])
const en = sidebar('/en', [
  ['Basics', { '': 'Introduction', 'getting-started': 'Getting Started', install: 'Download and Installation', toolbar: 'Toolbar, Tray and Hotkeys' }],
  ['Capturing', { capture: 'Capturing the Screen', scrolling: 'Scrolling Capture', editor: 'Preview and Editor', pin: 'Pinning to the Screen' }],
  ['Text', { ocr: 'Text Recognition and Translation', translation: 'Translation Engines and Prompts' }],
  ['More Tools', { record: 'Screen Recording', whiteboard: 'Screen Whiteboard', tools: 'QR Codes, Crosshair and Ruler' }],
  ['More', { settings: 'Settings', linux: 'Linux', faq: 'FAQ' }],
])
const ja = sidebar('/ja', [
  ['基本', { '': 'はじめに', 'getting-started': 'クイックスタート', install: 'ダウンロードとインストール', toolbar: 'ツールバー、トレイ、ショートカットキー' }],
  ['キャプチャ', { capture: '画面のキャプチャ', scrolling: 'スクロールキャプチャ', editor: 'プレビューとエディター', pin: '画面に貼り付け' }],
  ['テキスト', { ocr: '文字認識と翻訳', translation: '翻訳エンジンとプロンプト' }],
  ['その他のツール', { record: '画面録画', whiteboard: 'スクリーンホワイトボード', tools: 'QR コード、十字線、定規' }],
  ['その他', { settings: '設定', linux: 'Linux', faq: 'よくある質問' }],
])

export default defineConfig({
  base,
  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}logo.svg` }],
    ['meta', { name: 'theme-color', content: '#2d7dd2' }],
  ],
  lastUpdated: false,
  markdown: { anchor: { slugify } },
  // Dark by default, like the homepage; the switch in the nav bar still
  // changes it, and the choice is remembered.
  appearance: 'dark',
  locales: {
    root: {
      label: '简体中文',
      lang: 'zh-CN',
      title: 'Glimpse 使用说明',
      description: 'Glimpse 使用说明：截图、标注、贴图、录屏、文字识别与翻译。',
      themeConfig: {
        nav: [
          { text: '使用说明', link: '/getting-started', activeMatch: '^/(?!en/|ja/).+' },
          { text: '下载', link: `${home}releases.html` },
          { text: '主页', link: home },
        ],
        sidebar: zh,
        outline: { level: [2, 3], label: '本页目录' },
        docFooter: { prev: '上一页', next: '下一页' },
        darkModeSwitchLabel: '外观',
        lightModeSwitchTitle: '切换到浅色模式',
        darkModeSwitchTitle: '切换到深色模式',
        sidebarMenuLabel: '目录',
        returnToTopLabel: '返回顶部',
        langMenuLabel: '语言',
        notFound: { title: '页面不存在', quote: '所请求的页面不存在或已移动。', linkText: '返回首页' },
      },
    },
    en: {
      label: 'English',
      lang: 'en',
      link: '/en/',
      title: 'Glimpse Guide',
      description: 'Glimpse user guide: capture, annotate, pin, record, recognize and translate text.',
      themeConfig: {
        nav: [
          { text: 'Guide', link: '/en/getting-started', activeMatch: '^/en/.+' },
          { text: 'Download', link: `${home}releases.html` },
          { text: 'Home', link: home },
        ],
        sidebar: en,
        outline: { level: [2, 3], label: 'On this page' },
      },
    },
    ja: {
      label: '日本語',
      lang: 'ja',
      link: '/ja/',
      title: 'Glimpse ユーザーガイド',
      description: 'Glimpse ユーザーガイド：キャプチャ、注釈、貼り付け、録画、文字認識と翻訳。',
      themeConfig: {
        nav: [
          { text: 'ガイド', link: '/ja/getting-started', activeMatch: '^/ja/.+' },
          { text: 'ダウンロード', link: `${home}releases.html` },
          { text: 'ホーム', link: home },
        ],
        sidebar: ja,
        outline: { level: [2, 3], label: '目次' },
        docFooter: { prev: '前のページ', next: '次のページ' },
        darkModeSwitchLabel: '外観',
        lightModeSwitchTitle: 'ライトモードに切り替え',
        darkModeSwitchTitle: 'ダークモードに切り替え',
        sidebarMenuLabel: 'メニュー',
        returnToTopLabel: 'トップに戻る',
        langMenuLabel: '言語',
        notFound: { title: 'ページが見つかりません', quote: '要求されたページは存在しないか、移動されました。', linkText: 'ホームに戻る' },
      },
    },
  },
  themeConfig: {
    logo: '/logo.svg',
    siteTitle: 'Glimpse',
    search: {
      provider: 'local',
      options: {
        locales: {
          root: {
            translations: {
              button: { buttonText: '搜索', buttonAriaLabel: '搜索' },
              modal: {
                displayDetails: '显示详细信息',
                resetButtonTitle: '清除',
                backButtonTitle: '关闭',
                noResultsText: '未找到结果',
                footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' },
              },
            },
          },
          ja: {
            translations: {
              button: { buttonText: '検索', buttonAriaLabel: '検索' },
              modal: {
                displayDetails: '詳細を表示',
                resetButtonTitle: 'クリア',
                backButtonTitle: '閉じる',
                noResultsText: '結果が見つかりません',
                footer: { selectText: '選択', navigateText: '移動', closeText: '閉じる' },
              },
            },
          },
        },
      },
    },
  },
})
