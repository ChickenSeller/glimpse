"""Installs Qt for MinGW, with the MinGW it was built with and Ninja, from
Qt's online repository.

aqtinstall (behind install-qt-action) does not know the repository layout Qt
uses since 6.10, one folder per toolchain (qt6_6120/qt6_6120_mingw), so this
reads the repository's Updates.xml itself. Archives are checked against the
repository's SHA-1 sums and unpacked with 7-Zip.

    python install-qt.py --version 6.12.0 --modules qtmultimedia --dir <dir>

Qt lands in <dir>/<version>/mingw_64 and the tools in <dir>/Tools. Under
GitHub Actions the Qt, MinGW and Ninja bin directories are added to PATH and
QT_ROOT_DIR is set. A second run with the same arguments only does that.
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

BASE = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop'
SEVEN_ZIP = shutil.which('7z') or r'C:\Program Files\7-Zip\7z.exe'


def fetch(url):
    for attempt in range(4):
        try:
            with urllib.request.urlopen(url, timeout=120) as response:
                return response.read()
        except OSError as error:
            if attempt == 3:
                raise
            print(f'  retrying {url}: {error}', flush=True)
            time.sleep(5 * (attempt + 1))


def install(repo, packages, target):
    """Unpacks every archive of the named packages of one repository into target."""
    updates = ET.fromstring(fetch(f'{BASE}/{repo}/Updates.xml'))
    available = {p.findtext('Name'): p for p in updates.iter('PackageUpdate')}
    target.mkdir(parents=True, exist_ok=True)
    for name in packages:
        package = available.get(name)
        if package is None:
            sys.exit(f'{name} is not in {BASE}/{repo}')
        version = package.findtext('Version')
        archives = [a.strip() for a in (package.findtext('DownloadableArchives') or '').split(',') if a.strip()]
        for archive in archives:
            url = f'{BASE}/{repo}/{name}/{version}{archive}'
            print(f'{name}: {archive}', flush=True)
            data = fetch(url)
            expected = fetch(url + '.sha1').decode().split()[0].lower()
            if hashlib.sha1(data).hexdigest() != expected:
                sys.exit(f'Checksum mismatch: {url}')
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp, archive)
                path.write_bytes(data)
                subprocess.run([SEVEN_ZIP, 'x', '-y', '-bso0', '-bsp0', f'-o{target}', str(path)], check=True)


def find_dir(root, file_name):
    for path in root.rglob(file_name):
        return path.parent
    sys.exit(f'{file_name} not found under {root}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', required=True)
    parser.add_argument('--modules', nargs='*', default=[])
    parser.add_argument('--mingw', default='mingw1310')
    parser.add_argument('--dir', required=True, type=Path)
    args = parser.parse_args()

    major, minor, patch = args.version.split('.')
    tag = f'{major}{minor}{patch}'
    qt_dir = args.dir / args.version / 'mingw_64'
    tools_dir = args.dir / 'Tools'
    stamp = args.dir / f'installed-{args.version}-{args.mingw}-{"-".join(sorted(args.modules))}'

    if not stamp.exists():
        shutil.rmtree(args.dir, ignore_errors=True)
        install(f'qt{major}_{tag}/qt{major}_{tag}_mingw',
                [f'qt.qt{major}.{tag}.win64_mingw']
                + [f'qt.qt{major}.{tag}.addons.{m}.win64_mingw' for m in args.modules],
                qt_dir)
        # The MinGW archive holds Tools/mingw1310_64; Ninja's only ninja.exe.
        install(f'tools_{args.mingw}', [f'qt.tools.win64_{args.mingw}'], args.dir)
        install('tools_ninja', ['qt.tools.ninja'], tools_dir / 'Ninja')
        stamp.touch()

    paths = [qt_dir / 'bin', find_dir(tools_dir, 'g++.exe'), tools_dir / 'Ninja']
    for path in paths:
        print(f'PATH += {path}')
    print(f'QT_ROOT_DIR = {qt_dir}')
    if os.environ.get('GITHUB_PATH'):
        with open(os.environ['GITHUB_PATH'], 'a', encoding='utf-8') as f:
            f.writelines(f'{path}\n' for path in paths)
        with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as f:
            f.write(f'QT_ROOT_DIR={qt_dir}\n')


if __name__ == '__main__':
    main()
