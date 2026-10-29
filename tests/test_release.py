import io,json,os,sys,tarfile,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import publish_release as release
import package
class ReleaseTests(unittest.TestCase):
    def test_github_output_is_decoded_as_utf8_on_windows(self):
        import subprocess
        result=subprocess.CompletedProcess(['gh'],0,'{"body":"zażółć"}','')
        with patch.object(release.subprocess,'run',return_value=result) as call:
            self.assertEqual(release.gh('api','repos/owner/repo').stdout,result.stdout)
            self.assertEqual(call.call_args.kwargs['encoding'],'utf-8')
    def test_development_tags_and_no_collision_between_attempts(self):
        self.assertEqual(release.release_tag('0.1.0-preview.1',42,1),'v0.1.0-preview.1.ci.42.1')
        self.assertEqual(release.release_tag('1.2.3',42,1),'v1.2.3-dev.42.1')
        self.assertNotEqual(release.release_tag('1.2.3',42,1),release.release_tag('1.2.3',42,2))
        for version in ('../private','1.0.0\nsecret','1.0.0;echo'):
            with self.assertRaises(ValueError):release.release_tag(version,1,1)
    def test_only_expected_product_assets_are_uploaded(self):
        self.assertEqual(release.asset_names('windows','0.1.0-preview.1'),['JerzyMultiFX-VST3-Windows-x64-v0.1.0-preview.1.zip','INSTALACJA_VST3_PL.md'])
        self.assertNotIn('SHA256SUMS',release.asset_names('arm64','0.1.0-preview.1'))
        self.assertIn('JerzyMultiFX-RPi-aarch64-v0.1.0-preview.1.deb',release.asset_names('arm64','0.1.0-preview.1'))
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

class GithubArchiveTests(unittest.TestCase):
    def test_generated_repository_archive_excludes_private_references(self):
        import subprocess
        root=Path(__file__).resolve().parents[1]
        result=subprocess.run(['git','-C',str(root),'archive','--worktree-attributes','HEAD'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        if result.returncode:self.skipTest('Git source metadata not present')
        with tarfile.open(fileobj=io.BytesIO(result.stdout)) as t:names=t.getnames()
        self.assertNotIn('JERZY_MULTIFX_MATERIALY_REFERENCYJNE.zip',names)
        self.assertNotIn('docs/REQUIREMENTS_PL.md',names)
        self.assertFalse(any(n.startswith('references/') for n in names))

class PublicationTransactionTests(unittest.TestCase):
    def fixture(self,root):
        (root/'dist').mkdir()
        for name in release.asset_names('arm64',release.VERSION):
            p=root/'dist'/name if name.startswith('JerzyMultiFX-') else root/name
            p.write_bytes(b'isolated publisher fixture')
    def run_publication(self,collision=False):
        import subprocess
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);self.fixture(root);calls=[];gets=0
            def mock_gh(*args,**kwargs):
                nonlocal gets
                calls.append(args)
                result=lambda code,data='',error='':subprocess.CompletedProcess(args,code,json.dumps(data) if not isinstance(data,str) else data,error)
                if args==('api','repos/owner/repo'):return result(0,{'private':True})
                if '--method' in args:
                    payload=json.loads(kwargs['input']);self.assertTrue(payload['prerelease']);self.assertFalse(payload['draft'])
                    self.assertEqual(payload['target_commitish'],'a'*40)
                    return result(1,error='HTTP 422: release already exists (other platform won race)')
                if args[0]=='release':return result(0)
                gets+=1
                if gets==1 and not collision:return result(1,error='HTTP 404')
                assets=[]
                if collision:assets=[{'name':release.asset_names('arm64',release.VERSION)[0]}]
                elif gets>=3:
                    for file in (root/'.build/release-upload/arm64').iterdir():assets.append({'name':file.name,'size':file.stat().st_size,'digest':'sha256:'+__import__('hashlib').sha256(file.read_bytes()).hexdigest()})
                return result(0,{'prerelease':True,'draft':False,'assets':assets,'html_url':'https://github.com/owner/repo/releases/tag/test'})
            env={'GITHUB_REPOSITORY':'owner/repo','GITHUB_SHA':'a'*40,'GITHUB_RUN_ID':'123','GITHUB_RUN_ATTEMPT':'1','GITHUB_STEP_SUMMARY':''}
            with patch.object(release,'ROOT',root),patch.object(release,'gh',mock_gh),patch.dict(os.environ,env),patch.object(sys,'argv',['publish_release.py','--platform','arm64']),patch('sys.stdout',io.StringIO()):
                if collision:
                    with self.assertRaisesRegex(RuntimeError,'Refusing to overwrite'):release.main()
                else:release.main()
            return calls
    def test_parallel_release_creation_race_recovers_and_verifies_upload(self):
        calls=self.run_publication()
        upload=next(c for c in calls if c[:2]==('release','upload'))
        self.assertNotIn('--clobber',upload)
        self.assertTrue(all('dist/*' not in item for item in upload))
    def test_existing_asset_is_never_replaced_or_deleted(self):
        calls=self.run_publication(collision=True)
        self.assertFalse(any(c[0]=='release' for c in calls))

if __name__=='__main__':unittest.main()
