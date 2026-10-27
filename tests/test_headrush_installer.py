import hashlib, importlib.util, io, tarfile, tempfile, unittest, os, sys, json
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

class UpgradeTransactionTests(unittest.TestCase):
    def run_upgrade(self,fail=False):
        import base64,contextlib,hashlib,io,subprocess,types,urllib.request
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            home=Path(directory);units=home/'.config/systemd/user';units.mkdir(parents=True)
            base=home/'.local/share/jerzy-multifx-preview';old=base/'instalacja-old/dane';(old/'presets').mkdir(parents=True)
            (old/'presets/moj-bas.jmfx').write_bytes(b'user preset must survive')
            (old/'last-state.jmfx').write_bytes(b'previous state must survive rollback')
            (old/'token').write_text('test-fixture-token-'+'a'*40)
            previous={u:{'enabled':u in module.NEW[:2]+[module.OLD[2]],'active':u in module.NEW[:2]+[module.OLD[2]]} for u in module.OLD+module.NEW+[module.BRIDGE]}
            for unit in module.NEW[:2]:(units/unit).write_text('previous unit '+unit)
            files={'pkg/bin/jerzy-multifx':b'fixture binary','pkg/bin/jerzy-multifx-midi':b'fixture MIDI bridge'}
            files['pkg/SHA256SUMS']=''.join(hashlib.sha256(v).hexdigest()+'  '+n.removeprefix('pkg/')+'\n' for n,v in files.items()).encode()
            raw=ExistingHeadRushInstallerTests().archive(files)
            required={'system:capture_1','system:capture_2','system:playback_1','system:playback_2','JerzyMultiFX:input_L','JerzyMultiFX:output_L','JerzyMultiFX:midi_control'}
            calls=[];data=base/'instalacja-new/dane';real_read=Path.read_text;real_run=subprocess.run
            tools=home/'fake-bin';tools.mkdir();fake=tools/'systemctl';fake.write_text('#!/bin/sh\nexit 0\n');fake.chmod(0o755)
            def run(command,**kwargs):
                calls.append(command)
                if command[0]=='bash':return real_run(command,env={**os.environ,'PATH':str(tools)+':'+os.environ['PATH']},capture_output=True)
                return subprocess.CompletedProcess(command,1 if command[0]=='pgrep' else 0,'yes\n' if command[0]=='loginctl' else '', '')
            def ctl(*args,**kwargs):
                calls.append(('ctl',*args))
                if args[:2]==('enable','--now') and data.exists():(data/'bluetooth-midi-bridge.json').write_text(json.dumps({'running':True,'connected':True}))
                return subprocess.CompletedProcess(args,0,'MULTIFX_DATA='+str(old) if args[0]=='show' else '', '')
            def read(path,*args,**kwargs):
                return 'VERSION_CODENAME=trixie\n' if str(path)=='/etc/os-release' else real_read(path,*args,**kwargs)
            def urlopen(*args,**kwargs):
                if fail:raise OSError('simulated failed audio startup')
                return io.StringIO(json.dumps({'values':[0]*953}))
            with contextlib.ExitStack() as stack:
                stack.enter_context(patch.object(module.os, 'geteuid', return_value=1000))
                stack.enter_context(patch.object(module.pwd, 'getpwuid', return_value=types.SimpleNamespace(pw_name='jerzy')))
                stack.enter_context(patch.object(Path, 'home', return_value=home))
                stack.enter_context(patch.object(module.platform, 'machine', return_value='aarch64'))
                stack.enter_context(patch.object(Path, 'read_text', read))
                stack.enter_context(patch.object(module, 'state', side_effect=lambda u: previous[u]))
                stack.enter_context(patch.object(module, 'ctl', side_effect=ctl))
                stack.enter_context(patch.object(module, 'jack_ports', return_value=required))
                stack.enter_context(patch.object(module, 'PAYLOAD', base64.b64encode(raw), create=True))
                stack.enter_context(patch.object(module, 'PAYLOAD_SHA256', hashlib.sha256(raw).hexdigest(), create=True))
                stack.enter_context(patch.object(module.subprocess, 'run', side_effect=run))
                stack.enter_context(patch.object(module.time, 'strftime', return_value='instalacja-new'))
                stack.enter_context(patch.object(module.time, 'monotonic', side_effect=iter([0, 100]) if fail else lambda: 0))
                stack.enter_context(patch.object(module.time, 'sleep'))
                stack.enter_context(patch.dict(sys.modules, {'control': types.SimpleNamespace(rpc=lambda x: {'ok': True})}))
                stack.enter_context(patch.object(urllib.request, 'urlopen', side_effect=urlopen))
                stack.enter_context(patch.dict(os.environ, {}))
                stack.enter_context(patch.object(sys, 'argv', ['installer.py']))
                stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                if fail:
                    with self.assertRaisesRegex(RuntimeError,'nie potwierdził'):module.main()
                else:module.main()
            self.assertEqual((old/'last-state.jmfx').read_bytes(),b'previous state must survive rollback')
            self.assertEqual((data/'presets/moj-bas.jmfx').read_bytes(),b'user preset must survive')
            self.assertFalse((data/'last-state.jmfx').exists(),'new version starts clean without overwriting old sound')
            self.assertFalse(any('jerzy-jack.service' in list(c) for c in calls),'must not restart or replace existing JACK')
            if fail:
                self.assertTrue(any(c[0]=='bash' and c[1].endswith('COFNIJ.sh') for c in calls))
                self.assertEqual((units/module.NEW[0]).read_text(),'previous unit '+module.NEW[0])
                self.assertFalse((units/module.NEW[2]).exists())
            else:
                self.assertIn('/rpi/bluetooth_relay.py',(units/module.NEW[2]).read_text())
                self.assertIn('Wants=',(units/module.NEW[1]).read_text())
                self.assertNotIn('Requires=',(units/module.NEW[1]).read_text())
    def test_upgrade_preserves_old_version_network_jack_and_presets(self):self.run_upgrade()
    def test_failed_upgrade_runs_rollback_and_restores_previous_unit_files(self):self.run_upgrade(fail=True)
