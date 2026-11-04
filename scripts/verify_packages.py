#!/usr/bin/env python3
import hashlib,io,json,struct,subprocess,tarfile,tempfile,zipfile
from pathlib import Path,PurePosixPath
from package import architecture,VERSION,PRIVATE_NAMES,CACHE_PARTS
root=Path(__file__).resolve().parents[1]
packages=[root/'dist'/name for name in (f'JerzyMultiFX-VST3-Windows-x64-v{VERSION}.zip',f'JerzyMultiFX-RPi-aarch64-v{VERSION}.tar.gz')]
assert any(p.exists() for p in packages),'No current product packages'
for package in packages:
    if not package.exists():continue
    files={}
    if package.suffix=='.zip':
        with zipfile.ZipFile(package) as z:
            assert z.testzip() is None
            for name in z.namelist():
                assert not name.startswith('/') and '..' not in PurePosixPath(name).parts
                if not name.endswith('/'):files[name]=z.read(name)
    else:
        with tarfile.open(package) as t:
            for m in t:
                assert not m.name.startswith('/') and '..' not in PurePosixPath(m.name).parts
                assert not m.issym() and not m.islnk()
                if m.isfile():files[m.name]=t.extractfile(m).read()
    prefix=next(iter(files)).split('/')[0]+'/'
    for name in files:
        path=PurePosixPath(name)
        assert not any(part in CACHE_PARTS for part in path.parts),name
        assert path.name not in PRIVATE_NAMES and not path.name.startswith('.env'),name
        assert path.suffix not in ('.obj','.o','.pdb','.log','.pyc','.key','.pem'),name
    manifest=files[prefix+'SHA256SUMS'].decode()
    for line in manifest.splitlines():
        expected,name=line.split('  ',1)
        assert hashlib.sha256(files[prefix+name]).hexdigest()==expected,(package,name)
    info=json.loads(files[prefix+'BUILD-INFO.json'])
    assert info['version']==VERSION and len(info['commit'])==40
    if 'Windows' in package.name:
        dll=files[prefix+'Jerzy MultiFX.vst3/Contents/x86_64-win/Jerzy MultiFX.vst3']
        assert dll[:2]==b'MZ' and struct.unpack_from('<H',dll,struct.unpack_from('<I',dll,0x3c)[0]+4)[0]==0x8664
        assert json.loads(files[prefix+'VALIDATION.json'])['vst3_validator']['failed']==0
    else:
        b=files[prefix+'bin/jerzy-multifx'];assert b[:5]==b'\x7fELF\x02' and struct.unpack_from('<H',b,18)[0]==183
        for name in ('install.sh','uninstall.sh','rollback.sh','diagnostics.sh'):assert files[prefix+'scripts/'+name]
    with tarfile.open(fileobj=io.BytesIO(files[prefix+'source.tar.gz'])) as t:
        names=t.getnames();assert not any(PurePosixPath(n).name in PRIVATE_NAMES or any(part in CACHE_PARTS for part in PurePosixPath(n).parts) for n in names);assert 'JerzyMultiFX/core/Engine.cpp' in names
        assert not any('REQUIREMENTS_PL.md' in n or 'JERZY_MULTIFX_MATERIALY_REFERENCYJNE.zip' in n or n.endswith(('.png','.jpeg')) and 'references/' in n for n in names)
        if 'Windows' in package.name:assert 'JerzyMultiFX/dependencies/vst3sdk/REVISION' in names
    print('PASS',package.name,len(files),'files; all internal SHA256, actual architecture, sources, installer or validated VST3 verified')
fixed=root/'dist'/'MULTI_FX_FIXED_Windows_x64.zip'
if fixed.exists():
    with zipfile.ZipFile(fixed) as archive:
        assert archive.testzip() is None
        names=set(archive.namelist())
        prefix='MULTI_FX_FIXED_Windows_x64/'
        required={prefix+'Jerzy MultiFX.vst3/Contents/x86_64-win/Jerzy MultiFX.vst3',
                  prefix+'INSTALACJA.txt',prefix+'CHANGELOG.txt',prefix+'TEST_REPORT.txt',
                  prefix+'ZAINSTALUJ-VST3.cmd',prefix+'SHA256SUMS'}
        assert required.issubset(names),required-names
        dll=archive.read(prefix+'Jerzy MultiFX.vst3/Contents/x86_64-win/Jerzy MultiFX.vst3')
        assert dll[:2]==b'MZ' and struct.unpack_from('<H',dll,struct.unpack_from('<I',dll,0x3c)[0]+4)[0]==0x8664
    print('PASS',fixed.name,'requested installable x64 bundle and text reports verified')
deb=root/'dist'/f'JerzyMultiFX-RPi-aarch64-v{VERSION}.deb'
arm=packages[1]
if deb.exists():
    assert arm.exists(),'Debian package requires matching ARM64 tarball'
    fields=subprocess.check_output(['dpkg-deb','--field',str(deb)],text=True)
    assert f'Version: {VERSION}' in fields and 'Architecture: arm64' in fields
    listing=subprocess.check_output(['dpkg-deb','--contents',str(deb)],text=True)
    assert listing.splitlines()[0].startswith('drwxr-xr-x root/root'),'Unsafe DEB root permissions'
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['dpkg-deb','--extract',str(deb),tmp],check=True)
        payload=Path(tmp)/'usr/share/jerzy-multifx'
        with tarfile.open(arm) as archive:
            original={m.name.split('/',1)[1]:archive.extractfile(m).read() for m in archive if m.isfile()}
        extracted={p.relative_to(payload).as_posix():p.read_bytes() for p in payload.rglob('*') if p.is_file()}
        assert original==extracted,'DEB payload differs from verified ARM64 tarball'
        setup=Path(tmp)/'usr/bin/jerzy-multifx-setup'
        assert setup.stat().st_mode & 0o111 and b'sha256sum -c SHA256SUMS' in setup.read_bytes()
        assert not (Path(tmp)/'etc').exists(),'DEB must not activate or replace existing audio configuration'
    print('PASS',deb.name,'ARM64 metadata, identical checked payload, inert install')
