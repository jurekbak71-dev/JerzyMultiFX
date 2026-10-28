"""Authenticated panel bridge to the existing, UID-checked hardware broker.
Audio-buffer changes use the current JACK directly. This module never uses sudo,
changes network credentials, or launches bluetoothctl with user-provided commands.
"""
import json, os, re, socket, time
from pathlib import Path
from control import rpc
SOCKET = '/run/jerzy-audio-platform/control.sock'
STATUS = Path('/run/jerzy-audio-platform/status.json')

def status(data=None):
    try:
        value=json.loads(STATUS.read_text())
        if value.get('uid')!=os.getuid() or time.time()-value.get('updated',0)>12:raise ValueError('Stale broker')
        result={'ok':True,'available':True,**value}
    except (OSError,ValueError,TypeError):
        result={'ok':True,'available':False,'error':'Brak aktywnej usługi Bluetooth. USB MIDI jest pokazane oddzielnie poniżej.'}
    for key,command,default in [('midi_inputs','PORTS',[]),('audio_ports','AUDIOPORTS',{})]:
        try:
            response=rpc(command);result[key]=response['ports'] if key=='midi_inputs' else response
        except (OSError,ValueError,KeyError):result[key]=default
    pan=result.get('bluetooth',{}).get('pan',{})
    if isinstance(pan.get('url'),str):pan['url']=re.sub(r':8093(?=/|$)',':8097',pan['url'])
    if data is not None:
        try:result['relay']=json.loads((Path(data)/'bluetooth-midi.json').read_text())
        except (OSError,ValueError):result['relay']={'running':False}
    return result

def call(operation,**values):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(35);s.connect(SOCKET);s.sendall((json.dumps({'operation':operation,**values})+'\n').encode());raw=bytearray()
        while not raw.endswith(b'\n'):
            chunk=s.recv(16384)
            if not chunk or len(raw)>1024*1024:raise OSError('Niepełna odpowiedź usługi sprzętowej.')
            raw.extend(chunk)
    reply=json.loads(raw)
    if not reply.get('ok'):raise ValueError(reply.get('error','Nie udało się wykonać operacji.'))
    return reply

def action(data):
    operation=data.get('operation')
    if operation=='repair-audio':
        if not status()['available']:raise ValueError('Brak aktywnej usługi obsługi sprzętu.')
        return call('jack-restart')
    if operation=='input-mode':
        mode=data.get('mode')
        if type(mode) is not int or not 0<=mode<=2:raise ValueError('Wybierz stereo albo jedno wejście mono.')
        return rpc('INPUTMODE '+str(mode))
    if operation=='tempo':
        bpm=data.get('bpm')
        if type(bpm) not in (int,float) or not 20<=bpm<=400:raise ValueError('Tempo musi być od 20 do 400 BPM.')
        return rpc('TEMPO '+str(bpm))
    if operation=='buffer':
        frames=data.get('buffer')
        if type(frames) is not int or frames not in (64,128,256,512,1024):raise ValueError('Niepoprawny bufor.')
        return rpc('BUFFER '+str(frames))
    if operation=='audio-route':
        index=data.get('index');name=data.get('name')
        if type(index) is not int or not 0<=index<6 or not isinstance(name,str) or len(name)>255 or any(ord(c)<32 for c in name):raise ValueError('Niepoprawny port audio.')
        return rpc(f'AUDIOSET {index} {name}')
    if operation=='midi-input':
        name=data.get('name');enable=data.get('enable')
        if not isinstance(name,str) or not name or len(name)>255 or any(ord(c)<32 for c in name) or type(enable) is not bool:
            raise ValueError('Niepoprawne wejście MIDI.')
        return rpc(f'MIDIINPUT {int(enable)} {name}')
    if operation not in ('scan','pair','confirm','midi-connect','midi-disconnect','pan-connect','pan-disconnect'):
        raise ValueError('Nieobsługiwana operacja.')
    if not status()['available']:raise ValueError('Usługa Bluetooth jest niedostępna.')
    values={}
    if operation in ('pair','midi-connect','pan-connect'):
        address=data.get('address','')
        if not isinstance(address,str) or not re.fullmatch(r'(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}',address):raise ValueError('Wybierz urządzenie z listy.')
        values['address']=address
    if operation=='confirm':
        ident=data.get('id');value=data.get('value')
        if not isinstance(ident,str) or len(ident)>128 or not isinstance(value,(bool,int,str)):raise ValueError('Niepoprawne potwierdzenie parowania.')
        values={'id':ident,'value':value}
    return call(operation,**values)
