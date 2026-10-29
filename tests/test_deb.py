import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import build_deb
from package import sums


class DebianPackageTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('dpkg-deb'), 'dpkg-deb unavailable')
    def test_build_keeps_verified_arm_payload_and_does_not_activate_audio(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            stage = root / 'stage'
            (stage / 'bin').mkdir(parents=True)
            elf = bytearray(64)
            elf[:5] = b'\x7fELF\x02'
            elf[18:20] = (183).to_bytes(2, 'little')
            for binary in ('jerzy-multifx', 'jerzy-multifx-midi'):
                (stage / 'bin' / binary).write_bytes(elf)
            (stage / 'BUILD-INFO.json').write_text(json.dumps({
                'version': build_deb.VERSION, 'platform': 'RPi-aarch64', 'commit': 'a' * 40,
            }))
            (stage / 'scripts').mkdir()
            (stage / 'scripts/install.sh').write_text('#!/bin/bash\n')
            sums(stage)
            target = build_deb.build(stage, root / 'out')
            fields = subprocess.check_output(['dpkg-deb', '--field', str(target)], text=True)
            self.assertIn('Architecture: arm64', fields)
            self.assertIn('Package: jerzy-multifx-preview', fields)
            unpack = root / 'unpacked'
            subprocess.run(['dpkg-deb', '--extract', str(target), str(unpack)], check=True)
            payload = unpack / 'usr/share/jerzy-multifx'
            for item in stage.rglob('*'):
                if item.is_file():
                    self.assertEqual((payload / item.relative_to(stage)).read_bytes(), item.read_bytes())
            self.assertTrue((unpack / 'usr/bin/jerzy-multifx-setup').exists())
            self.assertFalse((unpack / 'etc').exists())
            control = subprocess.check_output(['dpkg-deb', '--ctrl-tarfile', str(target)])
            self.assertNotIn(b'postinst', control)

    def test_corrupt_stage_is_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / 'BUILD-INFO.json').write_text(json.dumps({
                'version': build_deb.VERSION, 'platform': 'Windows', 'commit': 'a' * 40,
            }))
            with self.assertRaisesRegex(ValueError, 'platform mismatch'):
                build_deb.build(root, root / 'out')


if __name__ == '__main__':
    unittest.main()
