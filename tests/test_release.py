import io,json,os,sys,tarfile,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import publish_release as release
import package
class ReleaseTests(unittest.TestCase):
    def test_development_tags_and_no_collision_between_attempts(self):
        self.assertEqual(release.release_tag('0.1.0-preview.1',42,1),'v0.1.0-preview.1.ci.42.1')
        self.assertEqual(release.release_tag('1.2.3',42,1),'v1.2.3-dev.42.1')
        self.assertNotEqual(release.release_tag('1.2.3',42,1),release.release_tag('1.2.3',42,2))
        for version in ('../private','1.0.0\nsecret','1.0.0;echo'):
            with self.assertRaises(ValueError):release.release_tag(version,1,1)
    def test_only_expected_product_assets_are_uploaded(self):
        self.assertEqual(release.asset_names('windows','0.1.0-preview.1'),['JerzyMultiFX-VST3-Windows-x64-v0.1.0-preview.1.zip','INSTALACJA_VST3_PL.md'])
        self.assertNotIn('SHA256SUMS',release.asset_names('arm64','0.1.0-preview.1'))
        with self.assertRaises(ValueError):release.asset_names('other','0.1.0-preview.1')
    def test_release_notes_include_installation_inventory_and_limits(self):
        notes=release.notes('owner/repo','v1-preview','1.0.0','a'*40,42)
        for required in ('Instalacja','Wykaz plików','INSTALACJA_VST3_PL.md','INSTALACJA_RPI_PL.md','SHA256SUMS-Windows-x64','AWAITING HARDWARE VALIDATION'):
            self.assertIn(required,notes)
    def test_source_filter_rejects_private_inputs_and_intermediates(self):
        for name in ('JerzyMultiFX/docs/REQUIREMENTS_PL.md','JerzyMultiFX/references/diagram.png','JerzyMultiFX/.env','JerzyMultiFX/token','JerzyMultiFX/.build/cache','JerzyMultiFX/core/output.obj','JerzyMultiFX/rpi/__pycache__/x.pyc','JerzyMultiFX/private.key'):
            self.assertIsNone(package.source_filter(tarfile.TarInfo(name)),name)
        self.assertIsNotNone(package.source_filter(tarfile.TarInfo('JerzyMultiFX/core/Engine.cpp')))
        link=tarfile.TarInfo('JerzyMultiFX/escape');link.type=tarfile.SYMTYPE;link.linkname='/etc/passwd'
        with self.assertRaises(ValueError):package.source_filter(link)
    def test_source_archive_contains_no_private_prompt_or_cache(self):
        with tempfile.TemporaryDirectory() as name:
            output=Path(name)/'source.tar.gz';package.sources(output)
            with tarfile.open(output) as t:
                names=t.getnames()
                self.assertIn('JerzyMultiFX/core/Engine.cpp',names)
                self.assertIn('JerzyMultiFX/VERSION',names)
                self.assertFalse(any('REQUIREMENTS_PL' in n or 'references/' in n or '/.build/' in n for n in names))
    def test_publisher_never_deletes_or_clobbers(self):
        source=Path(release.__file__).read_text()
        self.assertNotIn("'--clobber'",source)
        self.assertNotIn("'delete'",source)
        self.assertIn("'prerelease':True",source)
if __name__=='__main__':unittest.main()

class PrivacyFixtureTests(unittest.TestCase):
    def test_private_values_never_enter_source_archive(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);(root/'core').mkdir();(root/'docs').mkdir();(root/'rpi').mkdir()
            (root/'core/Engine.cpp').write_text('// public product source')
            (root/'docs/REQUIREMENTS_PL.md').write_text('PRIVATE_PROMPT_MARKER')
            (root/'rpi/token').write_text('PRIVATE_TOKEN_MARKER')
            (root/'rpi/.env').write_text('PRIVATE_ENV_MARKER')
            out=root/'source.tar.gz'
            with patch.object(package,'ROOT',root):package.sources(out)
            with tarfile.open(out) as t:
                payload=b''.join(t.extractfile(m).read() for m in t if m.isfile())
            for marker in (b'PRIVATE_PROMPT_MARKER',b'PRIVATE_TOKEN_MARKER',b'PRIVATE_ENV_MARKER'):
                self.assertNotIn(marker,payload)
            self.assertIn(b'public product source',payload)
