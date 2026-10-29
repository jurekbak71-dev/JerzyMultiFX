#!/usr/bin/env python3
"""Wrap the verified ARM64 runtime in an inert Debian delivery package."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from package import ROOT, VERSION, architecture, sums


def build(stage: Path, output: Path, version: str = VERSION) -> Path:
    stage = stage.resolve()
    info = json.loads((stage / 'BUILD-INFO.json').read_text())
    if info['version'] != version or info['platform'] != 'RPi-aarch64':
        raise ValueError('ARM64 stage version/platform mismatch')
    if not re.fullmatch(r'[0-9a-f]{40}', info['commit']):
        raise ValueError('Invalid source commit')
    for name in ('jerzy-multifx', 'jerzy-multifx-midi'):
        architecture(stage / 'bin' / name, 'arm64')
    for row in (stage / 'SHA256SUMS').read_text().splitlines():
        digest, name = row.split('  ', 1)
        if name.startswith('/') or '..' in Path(name).parts:
            raise ValueError('Unsafe package path')
        if hashlib.sha256((stage / name).read_bytes()).hexdigest() != digest:
            raise ValueError('ARM64 stage checksum mismatch: ' + name)
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'JerzyMultiFX-RPi-aarch64-v{version}.deb'
    with tempfile.TemporaryDirectory(prefix='jerzy-deb-') as tmp:
        root = Path(tmp)
        payload = root / 'usr/share/jerzy-multifx'
        shutil.copytree(stage, payload)
        command = root / 'usr/bin/jerzy-multifx-setup'
        command.parent.mkdir(parents=True)
        shutil.copy2(ROOT / 'scripts/deb-setup.sh', command)
        command.chmod(0o755)
        meta = root / 'DEBIAN'
        meta.mkdir()
        meta.chmod(0o755)
        (meta / 'control').write_text(
            f'Package: jerzy-multifx-preview\nVersion: {version}\nArchitecture: arm64\n'
            'Maintainer: Jerzy MultiFX project\nSection: sound\nPriority: optional\n'
            'Description: Jerzy MultiFX ARM64 preview delivery package\n'
            ' Installs verified files only. Run jerzy-multifx-setup to configure audio.\n'
        )
        result = subprocess.run(['dpkg-deb', '--build', '--root-owner-group', str(root), str(target)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode:
            raise RuntimeError('dpkg-deb failed: ' + result.stderr.strip())
    return target


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--stage', type=Path, default=ROOT / '.build/package-arm64')
    parser.add_argument('--out', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    target = build(args.stage, args.out)
    sums(args.out)
    print(target)


if __name__ == '__main__':
    main()
