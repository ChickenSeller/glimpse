"""Fills (or updates) a mirror of everything Glimpse downloads.

    glimpse --list-downloads downloads.tsv
    python mirror-downloads.py downloads.tsv <mirror directory> [--exclude TEXT ...]

Each file lands at <mirror directory>/<host>/<path> of its official address,
the layout Glimpse expects of a mirror (see src/ocr/ModelStore.h); serve that
directory as the mirror's base URL. Files already complete are skipped,
interrupted downloads resume, and files with a known SHA-256 are checked.

--exclude skips files whose address contains TEXT, e.g. --exclude Hy-MT2-7B
leaves out the 4.6 GB model. Uses HTTPS_PROXY / HTTP_PROXY when set.
"""
import argparse
import hashlib
import os
import sys
import urllib.request

CHUNK = 1 << 20


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as f:
        while block := f.read(CHUNK):
            digest.update(block)
    return digest.hexdigest()


def remote_size(url):
    request = urllib.request.Request(url, method='HEAD')
    with urllib.request.urlopen(request, timeout=60) as response:
        length = response.headers.get('Content-Length')
        return int(length) if length else None


def fetch(url, target):
    """Downloads url to target, resuming target + '.part' if present."""
    part = target + '.part'
    done = os.path.getsize(part) if os.path.exists(part) else 0
    request = urllib.request.Request(url, headers={'Range': f'bytes={done}-'} if done else {})
    with urllib.request.urlopen(request, timeout=60) as response:
        if done and response.status != 206:  # the server ignored the range: start over
            done = 0
        total = response.headers.get('Content-Length')
        total = int(total) + done if total else None
        with open(part, 'ab' if done else 'wb') as out:
            while block := response.read(CHUNK):
                out.write(block)
                done += len(block)
                if total:
                    print(f'\r    {done / 1e6:8.1f} / {total / 1e6:.1f} MB', end='', flush=True)
    print()
    os.replace(part, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('list', help='output of glimpse --list-downloads')
    parser.add_argument('mirror', help='directory to fill')
    parser.add_argument('--exclude', action='append', default=[], metavar='TEXT')
    args = parser.parse_args()

    entries = []
    with open(args.list, encoding='utf-8') as f:
        for line in f:
            if line.strip():
                url, path, sha256 = line.rstrip('\n').split('\t')
                if not any(text in url for text in args.exclude):
                    entries.append((url, path, None if sha256 == '-' else sha256.lower()))

    failed = 0
    for index, (url, path, sha256) in enumerate(entries, 1):
        target = os.path.join(args.mirror, *path.split('/'))
        print(f'[{index}/{len(entries)}] {path}')
        try:
            if os.path.exists(target):
                if sha256 and sha256_of(target) == sha256:
                    continue
                if not sha256 and remote_size(url) == os.path.getsize(target):
                    continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            fetch(url, target)
            if sha256 and sha256_of(target) != sha256:
                os.remove(target)
                raise RuntimeError('checksum mismatch')
        except Exception as error:  # report and go on with the others
            failed += 1
            print(f'    FAILED: {error}', file=sys.stderr)

    print(f'{len(entries) - failed} of {len(entries)} files in place.')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
