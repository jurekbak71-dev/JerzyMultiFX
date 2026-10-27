#!/usr/bin/env python3
"""Create actual binary packages, corresponding source and SHA-256 manifests."""
import argparse,hashlib,json,os,shutil,struct,subprocess,tarfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.1.0-preview.1'

def architecture(path,expected):
    b=Path(path).read_bytes()
    if expected=='windows':
        assert b[:2]==b'MZ','Not PE'
        offset=struct.unpack_from('<I',b,0x3c)[0]
        assert b[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',b,offset+4)[0]==0x8664,'Not Windows x64'
        assert struct.unpack_from('<H',b,offset+24)[0]==0x20b,'Not PE32+'
    else:
        assert b[:5]==b'\x7fELF\x02' and struct.unpack_from('<H',b,18)[0]==183,'Not ELF64 AArch64'

def sources(destination,sdk=None):
    allowed=('core','effects','vendor','plugin-vst3','rpi','ui','tests','scripts','cmake','docs','.github')
    with tarfile.open(destination,'w:gz') as archive:
        for item in sorted(ROOT.iterdir()):
            if item.name in allowed or item.suffix=='.md' or item.name in ('CMakeLists.txt','LICENSE','NOTICE','.gitignore','.clang-format'):
                archive.add(item,arcname='JerzyMultiFX/'+item.name,filter=lambda m:None if '/__pycache__/' in m.name or m.name.endswith('.pyc') else m)
        if sdk:
            sdk=Path(sdk)
            for item in sorted(sdk.iterdir()):
                if item.name not in ('.git','doc','vstgui4','tutorials'):
                    archive.add(item,arcname='JerzyMultiFX/dependencies/vst3sdk/'+item.name,filter=lambda m:None if '/.git' in m.name else m)

def sums(directory):
    lines=[]
    for path in sorted(directory.rglob('*')):
        if path.is_file() and path.name!='SHA256SUMS':
            lines.append(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.relative_to(directory).as_posix())
    (directory/'SHA256SUMS').write_text('\n'.join(lines)+'\n')

def main():
    p=argparse.ArgumentParser();p.add_argument('platform',choices=['windows','arm64']);p.add_argument('--build',type=Path,required=True);p.add_argument('--sdk',type=Path);p.add_argument('--out',type=Path,default=ROOT/'dist');a=p.parse_args()
    a.out.mkdir(exist_ok=True,parents=True)
    label='VST3-Windows-x64' if a.platform=='windows' else 'RPi-aarch64'
    stage=ROOT/'.build'/('package-'+a.platform)
    if stage.exists():shutil.rmtree(stage) # Only package.py-owned generated staging directory.
    stage.mkdir(parents=True)
    reports=stage/'test-reports';reports.mkdir()
    if a.platform=='windows':
        candidates=sorted(x for x in a.build.rglob('JerzyMultiFX.vst3') if x.is_file())
        assert candidates,'No compiled VST3 binary'
        dll=candidates[0];architecture(dll,'windows')
        bundle=stage/'Jerzy MultiFX.vst3';destination=bundle/'Contents/x86_64-win/Jerzy MultiFX.vst3';destination.parent.mkdir(parents=True);shutil.copy2(dll,destination)
        (bundle/'Contents/Resources').mkdir()
        validator=next(a.build.rglob('validator.exe'))
        command=[str(validator),str(bundle.resolve())] if os.name=='nt' else [str(ROOT/'scripts/wine-run.sh'),str(validator),'Z:'+str(bundle.resolve()).replace('/','\\')]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (reports/'vst3-validator.log').write_bytes(result.stdout)
        assert result.returncode==0 and b'0 tests failed' in result.stdout,'VST3 validation failed'
        shutil.copy(ROOT/'INSTALACJA_VST3_PL.md',stage)
    else:
        binary=a.build/'jerzy-multifx';architecture(binary,'arm64')
        (stage/'bin').mkdir();shutil.copy2(binary,stage/'bin/jerzy-multifx')
        for name in ('rpi','ui','scripts'):shutil.copytree(ROOT/name,stage/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
        for name in ('INSTALACJA_RPI_PL.md','WIFI_BLUETOOTH_PL.md','HARDWARE_TESTS_PL.md'):shutil.copy(ROOT/name,stage)
    for name in ('README_PL.md','LICENSE','NOTICE','LICENSES.md','STATUS.md','CHANGELOG.md'):shutil.copy(ROOT/name,stage)
    for path in (ROOT/'.build').glob('*.log'):
        if any(k in path.name for k in ('tests','integration','validator','gui')):shutil.copy(path,reports)
    try:commit=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    except subprocess.CalledProcessError:commit='uncommitted'
    (stage/'BUILD-INFO.json').write_text(json.dumps({'version':VERSION,'platform':label,'commit':commit,'hardware':'AWAITING HARDWARE VALIDATION','distribution':'private preview; see LICENSES.md'},indent=2))
    sources(stage/'source.tar.gz',a.sdk if a.platform=='windows' else None)
    sums(stage)
    name=f'JerzyMultiFX-{label}-v{VERSION}'
    if a.platform=='windows':
        output=a.out/(name+'.zip')
        with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
            for f in sorted(stage.rglob('*')):
                if f.is_file():z.write(f,name+'/'+f.relative_to(stage).as_posix())
    else:
        output=a.out/(name+'.tar.gz')
        with tarfile.open(output,'w:gz') as t:t.add(stage,arcname=name)
    print(output)
    hashes=[]
    for f in sorted(a.out.iterdir()):
        if f.is_file() and f.name!='SHA256SUMS':hashes.append(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name)
    (a.out/'SHA256SUMS').write_text('\n'.join(hashes)+'\n')
if __name__=='__main__':main()
