# Resources

- `glimpse.svg`: the application icon. The window, taskbar, tray and About
  window use it directly; the homepage uses a copy (`site/logo.svg`).
- `glimpse.ico`: the same icon for `glimpse.exe` (16 to 256 px), embedded
  through `glimpse.rc.in`. After changing the SVG, render it at 1024 px on a
  transparent background and run `python make-ico.py <png> glimpse.ico`.
- `icons/`: toolbar and window icons (see `icons/README.md`).
