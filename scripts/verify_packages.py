#!/usr/bin/env python3
import hashlib,io,json,struct,tarfile,zipfile
from pathlib import Path,PurePosixPath
from package import architecture,VERSION,PRIVATE_NAMES,CACHE_PARTS
root=Path(__file__).resolve().parents[1]
for package in sorted((root/'dist').glob('JerzyMultiFX-*')):
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
