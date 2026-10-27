#!/usr/bin/env python3
"""Package an explicit product/source allowlist; never sweep workspace logs or private inputs."""
import argparse,hashlib,io,json,os,re,shutil,struct,subprocess,tarfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text().strip()
SDK_REVISION='3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96'
PRIVATE_NAMES={'REQUIREMENTS_PL.md','JERZY_MULTIFX_PROMPT_DO_CODEXA.md','Wklejony tekst.txt','JERZY_MULTIFX_MATERIALY_REFERENCYJNE.zip','token'}
CACHE_PARTS={'.git','.build','build','dist','__pycache__','node_modules','.cache','references','attachments'}
RUNTIME_SCRIPTS=('install.sh','uninstall.sh','rollback.sh','diagnostics.sh')
RUNTIME_RPI=('control.py','web.py','ble.py','lcd.py','lcd_service.py','jerzy-multifx.service','jerzy-multifx-web.service','jerzy-multifx-jack.service','jerzy-multifx-ble.service')
RUNTIME_UI=('index.html','metadata.js')

def architecture(path,expected):
    b=Path(path).read_bytes()
    if expected=='windows':
        assert b[:2]==b'MZ','Not PE'
        offset=struct.unpack_from('<I',b,0x3c)[0]
        assert b[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',b,offset+4)[0]==0x8664,'Not Windows x64'
        assert struct.unpack_from('<H',b,offset+24)[0]==0x20b,'Not PE32+'
    else:
        assert b[:5]==b'\x7fELF\x02' and struct.unpack_from('<H',b,18)[0]==183,'Not ELF64 AArch64'

def source_filter(member):
    path=Path(member.name)
    if any(part in CACHE_PARTS for part in path.parts) or path.name in PRIVATE_NAMES:return None
    if path.name.startswith('.env') or path.suffix.lower() in {'.pyc','.log','.obj','.o','.a','.lib','.dll','.exe','.pdb','.pem','.key','.zip'}:return None
    if member.issym() or member.islnk():raise ValueError('Source archive must contain files, not filesystem links')
    member.uid=member.gid=0;member.uname=member.gname='root'
    return member

def sources(destination,sdk=None):
    roots=('core','effects','vendor','plugin-vst3','rpi','ui','tests','scripts','cmake','docs','.github')
    files=('CMakeLists.txt','VERSION','LICENSE','NOTICE','.gitignore','.gitattributes','.clang-format','README_PL.md','INSTALACJA_VST3_PL.md','INSTALACJA_RPI_PL.md','WIFI_BLUETOOTH_PL.md','HARDWARE_TESTS_PL.md','LICENSES.md','CHANGELOG.md','STATUS.md')
    tracked_result=subprocess.run(['git','-C',str(ROOT),'ls-files','-z'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    tracked=set(tracked_result.stdout.decode().split('\0')) if tracked_result.returncode==0 else None
    def product_filter(member):
        rel=Path(member.name).relative_to('JerzyMultiFX').as_posix()
        if member.isfile() and tracked is not None and rel not in tracked:return None
        return source_filter(member)
    with tarfile.open(destination,'w:gz') as archive:
        for name in (*roots,*files):
            item=ROOT/name
            if item.exists():archive.add(item,arcname='JerzyMultiFX/'+name,filter=product_filter)
        if sdk:
            sdk=Path(sdk)
            sdk_paths=['CMakeLists.txt','base','cmake','pluginterfaces','public.sdk/source','public.sdk/samples/vst-hosting/CMakeLists.txt','public.sdk/samples/vst-hosting/validator','public.sdk/samples/vst-utilities/CMakeLists.txt','public.sdk/samples/vst-utilities/moduleinfotool']
            sdk_paths += [p.name for p in sdk.glob('LICENSE*') if p.is_file()]
            for name in sdk_paths:
                archive.add(sdk/name,arcname='JerzyMultiFX/dependencies/vst3sdk/'+name,filter=source_filter)
            revision=(SDK_REVISION+'\n').encode();info=tarfile.TarInfo('JerzyMultiFX/dependencies/vst3sdk/REVISION');info.size=len(revision);archive.addfile(info,io.BytesIO(revision))

def sums(directory):
    rows=[]
    for f in sorted(directory.rglob('*')):
        if f.is_file() and f.name!='SHA256SUMS':rows.append(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(directory).as_posix())
    (directory/'SHA256SUMS').write_text('\n'.join(rows)+'\n')

def main():
    p=argparse.ArgumentParser();p.add_argument('platform',choices=['windows','arm64']);p.add_argument('--build',type=Path,required=True);p.add_argument('--sdk',type=Path);p.add_argument('--out',type=Path,default=ROOT/'dist');a=p.parse_args()
    assert re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?',VERSION),'Invalid VERSION'
    a.build=a.build.resolve();a.out.mkdir(exist_ok=True,parents=True)
    label='VST3-Windows-x64' if a.platform=='windows' else 'RPi-aarch64'
    stage=ROOT/'.build'/('package-'+a.platform)
    if stage.exists():shutil.rmtree(stage) # Only this script's generated staging directory.
    stage.mkdir(parents=True);validation={'architecture':a.platform,'hardware':'AWAITING HARDWARE VALIDATION'}
    if a.platform=='windows':
        candidates=sorted(x for x in a.build.rglob('JerzyMultiFX.vst3') if x.is_file())
        assert candidates,'No compiled VST3 binary'
        dll=candidates[0];architecture(dll,'windows')
        bundle=stage/'Jerzy MultiFX.vst3';destination=bundle/'Contents/x86_64-win/Jerzy MultiFX.vst3';destination.parent.mkdir(parents=True);shutil.copy2(dll,destination)
        (bundle/'Contents/Resources').mkdir()
        validator=next(a.build.rglob('validator.exe'))
        command=[str(validator),str(bundle)] if os.name=='nt' else [str(ROOT/'scripts/wine-run.sh'),str(validator),'Z:'+str(bundle).replace('/','\\')]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (ROOT/'.build/package-validator.log').write_bytes(result.stdout) # Private intermediate, never auto-published.
        matches=re.findall(rb'Result:\s*(\d+) tests passed, (\d+) tests failed',result.stdout)
        assert result.returncode==0 and matches and int(matches[-1][1])==0,'VST3 validation failed; see private .build/package-validator.log'
        validation['vst3_validator']={'passed':int(matches[-1][0]),'failed':0,'runner':'native Windows' if os.name=='nt' else 'Wine'}
        shutil.copy(ROOT/'INSTALACJA_VST3_PL.md',stage)
    else:
        binary=a.build/'jerzy-multifx';architecture(binary,'arm64')
        (stage/'bin').mkdir();shutil.copy2(binary,stage/'bin/jerzy-multifx')
        (stage/'rpi').mkdir();(stage/'ui').mkdir()
        for name in RUNTIME_RPI:shutil.copy2(ROOT/'rpi'/name,stage/'rpi'/name)
        for name in RUNTIME_UI:shutil.copy2(ROOT/'ui'/name,stage/'ui'/name)
        (stage/'scripts').mkdir()
        for name in RUNTIME_SCRIPTS:shutil.copy2(ROOT/'scripts'/name,stage/'scripts'/name)
        for name in ('INSTALACJA_RPI_PL.md','WIFI_BLUETOOTH_PL.md','HARDWARE_TESTS_PL.md'):shutil.copy(ROOT/name,stage)
    for name in ('README_PL.md','LICENSE','NOTICE','LICENSES.md','STATUS.md','CHANGELOG.md'):shutil.copy(ROOT/name,stage)
    commit=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    info={'version':VERSION,'platform':label,'commit':commit,'hardware':'AWAITING HARDWARE VALIDATION','distribution':'private preview; see LICENSES.md'}
    if os.environ.get('GITHUB_RUN_ID'):info['actions_run']=f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
    (stage/'BUILD-INFO.json').write_text(json.dumps(info,indent=2)+'\n')
    (stage/'VALIDATION.json').write_text(json.dumps(validation,indent=2)+'\n')
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
        with tarfile.open(output,'w:gz') as t:t.add(stage,arcname=name,filter=lambda m:normalize(m))
    sums(a.out);print(output)

def normalize(member):
    member.uid=member.gid=0;member.uname=member.gname='root';return member
if __name__=='__main__':main()
