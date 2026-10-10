"""Installs Qt from Qt's online repository: for MinGW, with the MinGW it was
built with and Ninja, or for Linux (GCC, x86-64).

aqtinstall (behind install-qt-action) does not know the repository layout Qt
uses since 6.10, one folder per toolchain (qt6_6120/qt6_6120_mingw), so this
reads the repository's Updates.xml itself. Archives are checked against the
repository's SHA-1 sums and unpacked with 7-Zip.

    python install-qt.py --version 6.12.0 --modules qtmultimedia --dir <dir>
    python install-qt.py --host linux --version 6.12.0 --modules qtmultimedia --dir <dir>

Qt lands in <dir>/<version>/mingw_64 (Linux: gcc_64) and the MinGW tools in
<dir>/Tools. Under GitHub Actions the Qt bin directory (and on Windows the
MinGW and Ninja ones) is added to PATH and QT_ROOT_DIR is set. A second run
with the same arguments only does that.
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

REPOSITORY = 'https://download.qt.io/online/qtsdkrepository'
BASES = {
    'windows': f'{REPOSITORY}/windows_x86/desktop',
    'linux': f'{REPOSITORY}/linux_x64/desktop',
}
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


def install(base, repo, packages, target):
    """Unpacks every archive of the named packages of one repository into target."""
    updates = ET.fromstring(fetch(f'{base}/{repo}/Updates.xml'))
    available = {p.findtext('Name'): p for p in updates.iter('PackageUpdate')}
    target.mkdir(parents=True, exist_ok=True)
    for name in packages:
        package = available.get(name)
        if package is None:
            sys.exit(f'{name} is not in {base}/{repo}')
        version = package.findtext('Version')
        archives = [a.strip() for a in (package.findtext('DownloadableArchives') or '').split(',') if a.strip()]
        for archive in archives:
            url = f'{base}/{repo}/{name}/{version}{archive}'
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
    parser.add_argument('--host', choices=sorted(BASES), default='windows')
    parser.add_argument('--version', required=True)
    parser.add_argument('--modules', nargs='*', default=[])
    parser.add_argument('--mingw', default='mingw1310')
    parser.add_argument('--dir', required=True, type=Path)
    args = parser.parse_args()

    major, minor, patch = args.version.split('.')
    tag = f'{major}{minor}{patch}'
    base = BASES[args.host]
    linux = args.host == 'linux'
    qt_dir = args.dir / args.version / ('gcc_64' if linux else 'mingw_64')
    tools_dir = args.dir / 'Tools'
    toolchain = 'gcc_64' if linux else args.mingw
    stamp = args.dir / f'installed-{args.version}-{toolchain}-{"-".join(sorted(args.modules))}'

    if not stamp.exists():
        shutil.rmtree(args.dir, ignore_errors=True)
        if linux:
            # The system's GCC and Ninja build against it.
            install(base, f'qt{major}_{tag}/qt{major}_{tag}',
                    [f'qt.qt{major}.{tag}.linux_gcc_64']
                    + [f'qt.qt{major}.{tag}.addons.{m}.linux_gcc_64' for m in args.modules],
                    qt_dir)
        else:
            install(base, f'qt{major}_{tag}/qt{major}_{tag}_mingw',
                    [f'qt.qt{major}.{tag}.win64_mingw']
                    + [f'qt.qt{major}.{tag}.addons.{m}.win64_mingw' for m in args.modules],
                    qt_dir)
            # The MinGW archive holds Tools/mingw1310_64; Ninja's only ninja.exe.
            install(base, f'tools_{args.mingw}', [f'qt.tools.win64_{args.mingw}'], args.dir)
            install(base, 'tools_ninja', ['qt.tools.ninja'], tools_dir / 'Ninja')
        stamp.touch()

    paths = [qt_dir / 'bin']
    if not linux:
        paths += [find_dir(tools_dir, 'g++.exe'), tools_dir / 'Ninja']
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
