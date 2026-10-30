#!/usr/bin/env python3
"""Actual core + HTTP functional requests; starts only test-owned processes."""
import http.cookiejar
import json,os,socket,subprocess,sys,tempfile,time,urllib.request,urllib.error,wave,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
processes=[]
def start(args,env,log):
    f=open(log,'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=subprocess.STDOUT);f.close();processes.append(p);return p
with tempfile.TemporaryDirectory(prefix='multifx-test-') as directory:
    data=Path(directory);env={**os.environ,'MULTIFX_CONTROL_PORT':'19817','MULTIFX_DATA':str(data)}
    try:
        core=os.environ.get('MULTIFX_BINARY',str(root/'.build/native/jerzy-multifx'))
        prefix=json.loads(os.environ.get('MULTIFX_RUNNER','[]'))
        start([*prefix,core,'--test-server','19817'],env,data/'core.log')
        start([sys.executable,str(root/'rpi/web.py'),'--data',str(data),'--port','18097'],env,data/'web.log')
        for _ in range(100):
            if (data/'token').exists():
                try:
                    urllib.request.urlopen('http://127.0.0.1:18097/',timeout=.2);break
                except OSError:pass
            time.sleep(.05)
        token=(data/'token').read_text()
        def request(path,body=None,auth=True,raw=False):
            headers={'Authorization':'Bearer '+token} if auth else {}
            if body is not None and not raw:body=json.dumps(body).encode();headers['Content-Type']='application/json'
            r=urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:18097'+path,body,headers),timeout=5)
            return json.load(r)
        for _ in range(100):
            try:metadata=request('/api/metadata');break
            except (OSError,urllib.error.HTTPError):time.sleep(.05)
        else:raise RuntimeError('Core did not become ready')
        assert len(metadata['parameters'])>=924
        try:request('/api/state',auth=False);raise AssertionError('Unauthenticated access')
        except urllib.error.HTTPError as e:assert e.code==401
        try:request('/api/midi-reset',{'confirm':'CLEAR_ALL_MIDI'},auth=False);raise AssertionError('Unauthenticated MIDI reset')
        except urllib.error.HTTPError as e:assert e.code==403
        # A normal browser receives an HttpOnly session with the page, no pasted key.
        browser=urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        page=browser.open('http://127.0.0.1:18097/')
        cookie=page.headers['Set-Cookie']
        assert 'HttpOnly' in cookie and 'SameSite=Strict' in cookie
        assert b'id="token"' not in page.read()
        assert len(json.load(browser.open('http://127.0.0.1:18097/api/metadata'))['parameters'])>=924
        for headers in ({'Origin':'http://evil.invalid'},{'Sec-Fetch-Site':'cross-site'}):
            req=urllib.request.Request('http://127.0.0.1:18097/api/state',headers=headers)
            try:browser.open(req);raise AssertionError('Cross-origin cookie access')
            except urllib.error.HTTPError as e:assert e.code==401
        parameter=next(p['id'] for p in metadata['parameters'] if p['key']=='drive.gain')
        assert request('/api/set',{'id':parameter,'value':.73})['ok']
        assert abs(request('/api/state')['values'][parameter]-.73)<1e-5
        assert request('/api/save',{'name':'integration'})['ok']
        request('/api/set',{'id':parameter,'value':.11})
        time.sleep(.6) # Ensure the different sound is persisted before LOAD.
        assert request('/api/load',{'name':'integration'})['ok']
        assert abs(request('/api/state')['values'][parameter]-.73)<1e-5
        time.sleep(.6)
        processes[0].terminate();processes[0].wait(timeout=5)
        start([*prefix,core,'--test-server','19817'],env,data/'core-after-load.log')
        for _ in range(100):
            try:
                assert abs(request('/api/state')['values'][parameter]-.73)<1e-5,'Preset LOAD survives restart without further edits'
                break
            except (OSError,urllib.error.HTTPError):time.sleep(.05)
        else:raise AssertionError('Restart after LOAD failed')
        path=data/'impulse.wav'
        with wave.open(str(path),'wb') as wav:wav.setnchannels(2);wav.setsampwidth(2);wav.setframerate(44100);wav.writeframes(b''.join(struct.pack('<hh',12000 if i==0 else 0,-6000 if i==0 else 0) for i in range(100)))
        assert request('/api/ir',path.read_bytes(),raw=True)['ok']
        imported=request('/api/state');ir_mix=next(p['id'] for p in metadata['parameters'] if p['key']=='ir.mix');ir_bypass=next(p['id'] for p in metadata['parameters'] if p['key']=='block.4.bypass')
        assert imported['values'][ir_mix]==1 and imported['values'][ir_bypass]==0,'IR import must actually enable its audio mix'
        find=lambda key:next(p['id'] for p in metadata['parameters'] if p['key']==key)
        request('/api/set',{'id':find('delayA.type'),'value':.25});request('/api/set',{'id':find('delayA.mx.slot0_algorithm'),'value':.5})
        visible=request('/api/view',{'block':2,'section':0,'mxSlot':0,'advanced':0})['ids'];keys=[metadata['parameters'][i]['key'] for i in visible]
        assert any('_algo4_' in k for k in keys) and not any('_algo0_' in k or '.memory.' in k for k in keys),'MD only selected model page'
        request('/api/set',{'id':find('delayA.type'),'value':.5})
        keys=[metadata['parameters'][i]['key'] for i in request('/api/view',{'block':2,'section':0,'mxSlot':0,'advanced':0})['ids']]
        assert any('.memory.' in k for k in keys) and not any('.mx.' in k for k in keys)
        hardware=request('/api/hardware');assert 'midi_inputs' in hardware and 'audio_ports' in hardware

        assert request('/api/save',{'name':'embedded'})['ok']
        # New RPi settings, preset uploads and discrete choices use the real RPC.
        assert request('/api/hardware',{'operation':'tempo','bpm':97.5})['ok']
        assert request('/api/hardware',{'operation':'input-mode','mode':1})['ok']
        current=request('/api/state')
        assert current['bpm']==97.5 and current['inputMode']==1
        choice=find('td.shift')
        assert request('/api/set',{'id':choice,'value':.37})['ok']
        assert request('/api/state')['values'][choice]==.5,'Discrete modes snap to actual positions'
        request('/api/set',{'id':parameter,'value':.12})
        assert request('/api/preset',(data/'presets/embedded.jmfx').read_bytes(),raw=True)['ok']
        assert abs(request('/api/state')['values'][parameter]-.73)<1e-5
        for message in ({'id':parameter,'value':1.5},{'id':-1,'value':.5}):
            try:
                result=request('/api/set',message)
                assert not result['ok']
            except urllib.error.HTTPError as e:assert e.code==400
        try:request('/api/load',{'name':'../outside'});raise AssertionError('Traversal')
        except urllib.error.HTTPError as e:assert e.code==400
        def control(command):
            with socket.create_connection(('127.0.0.1',19817),timeout=3) as sock:
                sock.sendall((command+'\n').encode());sock.shutdown(socket.SHUT_WR)
                return json.loads(sock.makefile().readline())
        switches=[p for p in metadata['parameters'] if p['key'].endswith('.bypass') and p['id']<metadata['controlCount']]
        for index,p in enumerate(switches):
            cc=20+index
            assert cc<128
            request('/api/set',{'id':find('midi.learn'),'value':(p['id']+1)/metadata['controlCount']})
            assert control(f'MIDI 176 {cc} 127')['ok']
            assert request('/api/state')['values'][p['id']]==1,p['key']
            assert control(f'MIDI 176 {cc} 0')['ok']
            midi_state=request('/api/state');assert midi_state['values'][p['id']]==0,p['key']
            assert midi_state['midiEvents']>=2 and (midi_state['lastMidi']>>8)&127==cc
        # One CC address per musical control follows every engine within a bank.
        for bank in ('delayA','delayB'):
            for kind,cc in (('time',90),('feedback',91),('mix',92)):
                cc+=3*(bank=='delayB')
                canonical=find(bank+'.'+kind)
                request('/api/set',{'id':find('midi.learn'),'value':(canonical+1)/metadata['controlCount']})
                assert control(f'MIDI 176 {cc} 64')['ok']
                for engine,target in enumerate((kind, 'mx.slot0_'+kind,
                    {'time':'memory.delay','feedback':'memory.feedback','mix':'mix'}[kind],
                    'reel.'+kind,'oil.'+kind)):
                    request('/api/set',{'id':find(bank+'.type'),'value':engine/4})
                    assert control(f'MIDI 176 {cc} 96')['ok']
                    key=bank+'.'+(target if kind!='mix' or engine==0 else 'mix')
                    if engine==1 and kind=='mix':key=bank+'.mix'
                    value=request('/api/state')['values'][find(key)]
                    assert abs(value-96/127)<1e-5,(bank,kind,engine,key,value)
        request('/api/set',{'id':find('delayA.type'),'value':.25})
        request('/api/set',{'id':find('delayA.mx.slot1_enable'),'value':1})
        assert control('MIDI 176 90 32')['ok']
        state=request('/api/state')
        for key in ('delayA.mx.slot0_time','delayA.mx.slot1_time'):
            assert abs(state['values'][find(key)]-32/127)<1e-5,key
        # Program Change selects a complete global sound including its CC map.
        first=find('delayA.feedback');second=find('delayB.feedback');cc=100
        request('/api/set',{'id':find('midi.learn'),'value':(first+1)/metadata['controlCount']})
        assert control(f'MIDI 176 {cc} 64')['ok']
        assert request('/api/save',{'name':'program42'})['ok']
        request('/api/set',{'id':find('midi.learn'),'value':(second+1)/metadata['controlCount']})
        assert control(f'MIDI 176 {cc} 64')['ok']
        assert request('/api/save',{'name':'program43'})['ok']
        for program,target in ((42,first),(43,second)):
            assert control(f'MIDI 192 {program}')['ok']
            for _ in range(60):
                state=request('/api/state')
                mapped=round(-1+metadata['controlCount']*state['values'][find('midi.cc.100')])
                if mapped==target:break
                time.sleep(.02)
            else:raise AssertionError('PC did not load its own complete MIDI mapping')
        assert control('MIDI 192 0')['ok']
        for _ in range(60):
            state=request('/api/state')
            if state['values'][find('midi.cc.100')]==0:break
            time.sleep(.02)
        else:raise AssertionError('Factory preset must reset its individual MIDI map')
        assert control('MIDI 192 43')['ok']
        for _ in range(60):
            state=request('/api/state')
            if round(-1+metadata['controlCount']*state['values'][find('midi.cc.100')])==second:break
            time.sleep(.02)
        else:raise AssertionError('Saved global preset did not restore its MIDI map')
        # Physical control layout is calibrated once; MIDI targets still belong to each preset.
        binding={'profile':'hotone','control':'EXP','message':{'kind':'cc','number':74,'channel':1}}
        assert request('/api/controller-layout',binding)['layout']['hotone']['EXP']['number']==74
        assert request('/api/controller-layout')['layout']['hotone']['EXP']['number']==74
        try:request('/api/controller-layout',{'profile':'hotone','control':'EXP','message':{'kind':'cc','number':200,'channel':1}});raise AssertionError('Invalid CC accepted')
        except urllib.error.HTTPError as e:assert e.code==400
        request('/api/set',{'id':find('attack.expressionEnabled'),'value':1})
        sound_before=request('/api/state')['values'][parameter]
        ir_before=request('/api/state')['ir']
        try:request('/api/midi-reset',{'confirm':'no'});raise AssertionError('Unconfirmed reset accepted')
        except urllib.error.HTTPError as e:assert e.code==400
        reset=request('/api/midi-reset',{'confirm':'CLEAR_ALL_MIDI'})
        assert reset['ok'] and reset['presets']>=2
        cleared=request('/api/state')
        assert cleared['values'][find('midi.cc.100')]==0 and cleared['values'][find('attack.expressionEnabled')]==0
        assert cleared['values'][parameter]==sound_before and cleared['ir']==ir_before
        names=request('/api/presets')['presets']
        assert 'program42-bez-PC' in names and 'program43-bez-PC' in names and 'program43' not in names
        assert request('/api/controller-layout')['layout']['hotone']['EXP']['number']==74,'Physical layout survives MIDI assignment reset'
        assert control('MIDI 192 43')['ok']
        assert request('/api/state')['values'][find('midi.cc.100')]==0,'Old PC slot cannot restore erased assignments'
        assert request('/api/load',{'name':'program43-bez-PC'})['ok']
        assert request('/api/state')['values'][find('midi.cc.100')]==0,'Archived sound keeps its music but has no MIDI map'
        # Graceful restart is the same state restore path used at boot.
        before=request('/api/state')['values']
        processes[-1].terminate();processes[-1].wait(timeout=5)
        assert (data/'last-state.jmfx').is_file()
        start([*prefix,core,'--test-server','19817'],env,data/'core-restarted.log')
        for _ in range(100):
            try:
                restored=request('/api/state')['values']
                if len(restored)==len(before):break
            except (OSError,urllib.error.HTTPError):pass
            time.sleep(.05)
        else:raise AssertionError('Restart failed')
        assert restored==before,'last sound and MIDI maps survive a core restart'
        persisted=request('/api/state')
        assert persisted['bpm']==97.5 and persisted['inputMode']==1,'Tempo and mono input survive restart'
        print('PASS actual core/HTTP: 924+ parameters, authentication, parameter update, preset persistence, stereo WAV44100 -> 48000 IR import/embedding, invalid ranges and traversal rejected')
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
