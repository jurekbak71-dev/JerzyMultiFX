#!/usr/bin/env python3
"""Publish a strict product allowlist from this job; never delete or replace assets."""
import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
from package import ROOT,VERSION

def gh(*args,input=None,check=True):
    result=subprocess.run(['gh',*args],input=input,text=True,encoding='utf-8',
                          stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or 'GitHub CLI request failed')
    return result

def release_tag(version,run_id,attempt):
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?',version):raise ValueError('Invalid project VERSION')
    if not str(run_id).isdigit() or not str(attempt).isdigit():raise ValueError('Numeric Actions run metadata required')
    suffix=('.ci.' if '-' in version else '-dev.')+str(run_id)+'.'+str(attempt)
    return 'v'+version+suffix

def asset_names(platform,version):
    if platform=='windows':
        return [f'JerzyMultiFX-VST3-Windows-x64-v{version}.zip','MULTI_FX_FIXED_Windows_x64.zip',
                'JerzyMultiFX-GUI3D-Windows-x64.zip','GUI3D_Library.zip','INSTALACJA_VST3_PL.md']
    if platform=='arm64':
        return [f'JerzyMultiFX-RPi-aarch64-v{version}.tar.gz',f'JerzyMultiFX-RPi-aarch64-v{version}.deb','JerzyMultiFX-INSTALATOR.zip','INSTALACJA_RPI_PROSTO.txt','INSTALACJA_RPI_PL.md','WIFI_BLUETOOTH_PL.md','HARDWARE_TESTS_PL.md']
    raise ValueError('Unsupported platform')

def notes(repo,tag,version,sha,run_id,platform=None):
    prefix=f'https://github.com/{repo}/releases/download/{tag}/'
    if platform=='arm64':
        return f"""# Jerzy MultiFX {version} — RPi preview

Źródła: `{sha}`. [Natywny build ARM64 i testy](https://github.com/{repo}/actions/runs/{run_id}).

## Instalacja

[Pobierz JerzyMultiFX-INSTALATOR.zip]({prefix}JerzyMultiFX-INSTALATOR.zip).
Rozpakuj cały ZIP na Windows i uruchom `ZAINSTALUJ-NA-RASPBERRY.cmd`.
Włącz Raspberry Pi i HeadRush; podawaj hasło użytkownika `jerzy`.
Panel otwiera się bez klucza. Brzmienie i mapy MIDI startują razem z Pi.
Instalator dla istniejącego bassambient/jerzy/HeadRush Core na Debian 13 ARM64;
zachowuje Wi-Fi, JACK, presety oraz kopię poprzedniej instalacji i rollback.
[Instrukcja]({prefix}INSTALACJA_RPI_PL.md).

## Zmiany

Zielone WŁĄCZONY / czerwone WYŁĄCZONY-BYPASS, szybsze sterowanie, listy i przełączniki
trybów, MIDI Learn wszystkich bypassów. CC Toggle 0=efekt włączony, 127=bypass.
Tape Warm/mechanika, dedykowana dyfuzja Cloud, poprawki Decay/Shimmer,
wygładzania COMP/Digital oraz optymalizacja Memory. Autosave poza wątkiem kontroli,
BPM/mono/stereo i import pełnego presetu VST. Odzyskiwanie USB MIDI/radia Bluetooth
oraz restart core po restarcie JACK. Szczegóły przeglądu DSP w źródłach paczki.
Cloud to własny algorytm ambientowy, bez deklaracji identyczności ze Strymon.

## Wykaz plików

- `JerzyMultiFX-INSTALATOR.zip` — samowystarczalny instalator z rzeczywistymi binariami AArch64.
- `JerzyMultiFX-RPi-aarch64-v{version}.tar.gz` — binaria ARM64, aplikacja, odpowiadające źródła i manifest SHA256.
- `JerzyMultiFX-RPi-aarch64-v{version}.deb` — pakiet Debian ARM64; po instalacji uruchom `sudo jerzy-multifx-setup` i wybierz audio. Samo `dpkg -i` nie przestawia urządzenia.
- `SHA256SUMS-RPi-aarch64`, `FILES-RPi-aarch64.json` — sumy i rozmiary plików do pobrania.
- `INSTALACJA_RPI_PROSTO.txt`, `INSTALACJA_RPI_PL.md`, `WIFI_BLUETOOTH_PL.md`, `HARDWARE_TESTS_PL.md` — instrukcje.

Windows VST3 pojawi się w tym samym Release tylko po osobnym, pozytywnym jobie Windows.
Testy programowe są w zalinkowanym runie. Fizyczny Pi/HeadRush, radio BLE, odsłuch A/B:
**AWAITING HARDWARE VALIDATION**. Nie zmieniono widoczności repozytorium.
"""
    return f'''# Jerzy MultiFX {version} — development prerelease

Source commit: `{sha}`. [Build and test run](https://github.com/{repo}/actions/runs/{run_id}).
Native Windows x64 and ARM64 jobs publish independently after their tests. Files may appear successively; an absent platform means its job has not completed successfully. No Actions Artifacts transfer is used.

## Instalacja

- **Windows:** rozpakuj ZIP, zamknij DAW i kliknij dwa razy `ZAINSTALUJ-VST3.cmd`. Możesz też skopiować cały `Jerzy MultiFX.vst3` do `C:\\Program Files\\Common Files\\VST3`. [Pełna instrukcja]({prefix}INSTALACJA_VST3_PL.md).
- **RPi bassambient / HeadRush:** [pobierz instalator ZIP]({prefix}JerzyMultiFX-INSTALATOR.zip), rozpakuj cały ZIP na Windows i kliknij dwa razy `ZAINSTALUJ-NA-RASPBERRY.cmd`. Włącz HeadRush i Raspberry Pi; podawaj hasło użytkownika `jerzy`. Aktualizacja zachowuje Wi-Fi, JACK, presety i możliwość cofnięcia. TAR.GZ jest paczką do ręcznej instalacji na innym RPi ARM64. [Pełna instrukcja]({prefix}INSTALACJA_RPI_PL.md).
- Sprawdź sumy w pliku SHA256SUMS dla właściwej platformy. Każda paczka zawiera również własny manifest SHA256 oraz odpowiadające źródła.

## Wykaz plików

- `JerzyMultiFX-VST3-Windows-x64-v{version}.zip` — rzeczywisty bundle VST3 x64.
- `JerzyMultiFX-GUI3D-Windows-x64.zip` — instalator VST3 z raportem i zrzutami GUI3D.
- `GUI3D_Library.zip` — osobne produkcyjne assety, manifest i generator.
- `JerzyMultiFX-RPi-aarch64-v{version}.tar.gz` — rzeczywisty ELF AArch64, aplikacja i instalator.
- `JerzyMultiFX-INSTALATOR.zip` — samowystarczalny instalator RPi, uruchamiany dwuklikiem z Windows; `INSTALACJA_RPI_PROSTO.txt` — krótka instrukcja.
- `INSTALACJA_VST3_PL.md`, `INSTALACJA_RPI_PL.md`, `WIFI_BLUETOOTH_PL.md`, `HARDWARE_TESTS_PL.md` — instrukcje i ograniczenia.
- `FILES-Windows-x64.json`, `FILES-RPi-aarch64.json` — wykaz plików, wielkości i sumy SHA256 dla opublikowanej platformy.
- `SHA256SUMS-Windows-x64`, `SHA256SUMS-RPi-aarch64` — sumy gotowych plików do pobrania.

## Status

Preview, nie stable. Fizyczny FL Studio/RPi, radio BLE/iOS i LCD: **AWAITING HARDWARE VALIDATION**. Znane ograniczenia: STATUS.md w paczce. Publikacja zachowuje widoczność repozytorium; nie publikuje materiałów referencyjnych, promptów użytkownika, sekretów, konfiguracji lokalnej, cache ani plików pośrednich. Istniejące wydania i pliki nie są kasowane ani nadpisywane.
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--platform',required=True,choices=['windows','arm64']);p.add_argument('--dry-run',action='store_true');a=p.parse_args()
    repo=os.environ.get('GITHUB_REPOSITORY','jurekbak71-dev/JerzyMultiFX')
    sha=os.environ.get('GITHUB_SHA') or subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    run_id=os.environ.get('GITHUB_RUN_ID','0');attempt=os.environ.get('GITHUB_RUN_ATTEMPT','1')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',repo) or not re.fullmatch('[0-9a-f]{40}',sha):raise ValueError('Invalid trusted repository/commit metadata')
    tag=release_tag(VERSION,run_id,attempt)
    label='Windows-x64' if a.platform=='windows' else 'RPi-aarch64'
    output=ROOT/'dist';staging=ROOT/'.build'/'release-upload'/a.platform;staging.mkdir(parents=True,exist_ok=True)
    selected=[]
    for name in asset_names(a.platform,VERSION):
        source=(output/name) if (output/name).is_file() else ROOT/name
        if not source.is_file():raise FileNotFoundError('Required product file missing: '+name)
        destination=staging/name
        destination.write_bytes(source.read_bytes());selected.append(destination)
    manifest={'version':VERSION,'tag':tag,'commit':sha,'platform':label,'files':[{'name':f.name,'size':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in selected]}
    filelist=staging/f'FILES-{label}.json';filelist.write_text(json.dumps(manifest,indent=2)+'\n');selected.append(filelist)
    checksums=staging/f'SHA256SUMS-{label}';checksums.write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in selected));selected.append(checksums)
    body=notes(repo,tag,VERSION,sha,run_id,a.platform)
    if a.dry_run:
        print(json.dumps({'tag':tag,'prerelease':True,'assets':[f.name for f in selected]},indent=2));return
    repository=json.loads(gh('api',f'repos/{repo}').stdout)
    if not repository.get('private'):
        raise RuntimeError('This preview contains sources with unresolved original license statements; public publication requires an explicit licensing review. Repository visibility was not changed.')
    get=lambda:gh('api',f'repos/{repo}/releases/tags/{tag}',check=False)
    existing=get()
    if existing.returncode:
        payload={'tag_name':tag,'target_commitish':sha,'name':f'Jerzy MultiFX {VERSION} — preview CI {run_id}.{attempt}','body':body,'draft':False,'prerelease':True}
        created=gh('api','--method','POST',f'repos/{repo}/releases','--input','-',input=json.dumps(payload),check=False)
        if created.returncode:
            # The other validated platform may have created the same release first.
            for retry in range(3):
                existing=get()
                if not existing.returncode:break
                time.sleep(.5)
            else:raise RuntimeError(created.stderr.strip() or 'Release creation failed; check contents:write')
        else:existing=created
    release=json.loads(existing.stdout)
    if not release.get('prerelease') or release.get('draft'):raise RuntimeError('Refusing to change an existing non-preview release')
    existing_names={item['name'] for item in release.get('assets',[])}
    collisions=existing_names.intersection(f.name for f in selected)
    if collisions:raise RuntimeError('Refusing to overwrite existing assets: '+', '.join(sorted(collisions)))
    gh('release','upload',tag,*[str(f) for f in selected],'--repo',repo)
    verified=json.loads(gh('api',f'repos/{repo}/releases/tags/{tag}').stdout)
    published={f['name']:f for f in verified['assets']}
    for file in selected:
        item=published.get(file.name)
        if not item or item['size']!=file.stat().st_size:raise RuntimeError('Uploaded asset verification failed: '+file.name)
        if item.get('digest') and item['digest']!='sha256:'+hashlib.sha256(file.read_bytes()).hexdigest():raise RuntimeError('Uploaded SHA256 mismatch')
    summary=f"Published {label} directly: {verified['html_url']}\n"+'\n'.join('- '+f.name for f in selected)+'\n'
    print(summary)
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'],'a',encoding='utf-8') as f:f.write(summary)
if __name__=='__main__':main()
