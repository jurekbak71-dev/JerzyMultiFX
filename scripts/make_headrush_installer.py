#!/usr/bin/env python3
"""Create the self-contained installer from the verified native ARM64 release."""
import argparse, base64, hashlib, importlib.util, json, tempfile, zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--checksums', type=Path, required=True)
    p.add_argument('--output', type=Path, default=root/'dist')
    a = p.parse_args()
    raw = a.package.read_bytes()
    matches = [line.split('  ', 1)[0] for line in a.checksums.read_text().splitlines()
               if '  ' in line and line.split('  ', 1)[1] == a.package.name]
    if matches != [hashlib.sha256(raw).hexdigest()]:
        raise SystemExit('Native package checksum mismatch or missing manifest entry')
    spec = importlib.util.spec_from_file_location('headrush_installer', root/'scripts/aktualizuj-headrush.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory() as d:
        extracted = module.unpack(raw, Path(d))
        binary = (extracted/'bin/jerzy-multifx').read_bytes()
        if binary[:5] != b'\x7fELF\x02' or int.from_bytes(binary[18:20], 'little') != 183:
            raise SystemExit('Package must contain an actual ELF64 AArch64 binary')
        info = json.loads((extracted/'BUILD-INFO.json').read_text())
    source = (root/'scripts/aktualizuj-headrush.py').read_text()
    marker = "if __name__ == '__main__':"
    if source.count(marker) != 1: raise SystemExit('Invalid installer template')
    payload = 'PAYLOAD_SHA256 = '+repr(hashlib.sha256(raw).hexdigest())+'\nPAYLOAD = '+repr(base64.b64encode(raw).decode())+'\n\n'
    script = source.replace(marker, payload+marker)
    compile(script, 'JerzyMultiFX-AKTUALIZUJ.py', 'exec')
    a.output.mkdir(parents=True, exist_ok=True)
    target = a.output/'JerzyMultiFX-INSTALATOR.zip'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('JerzyMultiFX-AKTUALIZUJ.py', script)
        z.writestr('ZAINSTALUJ-NA-RASPBERRY.cmd',(root/'scripts/install-from-windows.cmd').read_bytes())
        z.writestr('PRZECZYTAJ-MNIE.txt', 'ROZPAKUJ CALY ZIP. Na Windows kliknij dwa razy ZAINSTALUJ-NA-RASPBERRY.cmd.\nWlacz HeadRush i scisz glosniki. Podawaj haslo jerzy, gdy zapyta.\nInstalator dla bassambient, użytkownik jerzy, HeadRush Core.\nZachowuje sieć i dotychczasowy JACK. Stary program można przywrócić.\nPo skopiowaniu pliku na Raspberry Pi uruchom bez sudo:\npython3 ~/JerzyMultiFX-AKTUALIZUJ.py\nNie został jeszcze przetestowany na fizycznym Raspberry Pi.\n')
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        assert z.namelist() == ['JerzyMultiFX-AKTUALIZUJ.py', 'ZAINSTALUJ-NA-RASPBERRY.cmd', 'PRZECZYTAJ-MNIE.txt']
        assert z.read('JerzyMultiFX-AKTUALIZUJ.py').decode() == script
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    (a.output/'SHA256SUMS-INSTALATOR').write_text(digest+'  '+target.name+'\n')
    (a.output/'FILES-INSTALATOR.json').write_text(json.dumps({'binary_source': info, 'files': [{'name':target.name,'bytes':target.stat().st_size,'sha256':digest}], 'contents':['JerzyMultiFX-AKTUALIZUJ.py','ZAINSTALUJ-NA-RASPBERRY.cmd','PRZECZYTAJ-MNIE.txt']}, indent=2)+'\n')
    print('Verified self-contained AArch64 installer:', target.name)

if __name__ == '__main__': main()
