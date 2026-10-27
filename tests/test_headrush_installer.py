import hashlib, importlib.util, io, tarfile, tempfile, unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('headrush_install', Path(__file__).resolve().parents[1]/'scripts/aktualizuj-headrush.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ExistingHeadRushInstallerTests(unittest.TestCase):
    def archive(self, files):
        raw = io.BytesIO()
        with tarfile.open(fileobj=raw, mode='w:gz') as t:
            for name, value in files.items():
                m = tarfile.TarInfo(name)
                if value is None:
                    m.type = tarfile.SYMTYPE; m.linkname = '/etc/passwd'
                    t.addfile(m)
                else:
                    m.size = len(value); t.addfile(m, io.BytesIO(value))
        return raw.getvalue()

    def test_verified_package_extracts(self):
        binary = b'fixture'
        manifest = (hashlib.sha256(binary).hexdigest()+'  bin/jerzy-multifx\n').encode()
        raw = self.archive({'pkg/bin/jerzy-multifx': binary, 'pkg/SHA256SUMS': manifest})
        with tempfile.TemporaryDirectory() as d:
            root = module.unpack(raw, Path(d))
            self.assertEqual((root/'bin/jerzy-multifx').read_bytes(), binary)

    def test_corrupted_binary_is_rejected(self):
        raw = self.archive({'pkg/bin/jerzy-multifx': b'bad', 'pkg/SHA256SUMS': b'0000  bin/jerzy-multifx\n'})
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(RuntimeError, 'Uszkodzony'):
                module.unpack(raw, Path(d))

    def test_traversal_and_symlink_are_rejected_before_extraction(self):
        for files in [{'../escape': b'bad'}, {'pkg/link': None}]:
            with self.subTest(files=files), tempfile.TemporaryDirectory() as d:
                with self.assertRaisesRegex(RuntimeError, 'Nieprawidłowa'):
                    module.unpack(self.archive(files), Path(d))
                self.assertEqual(list(Path(d).iterdir()), [])
