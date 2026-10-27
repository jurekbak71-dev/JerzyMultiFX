#!/usr/bin/env python3
"""Actual core + HTTP functional requests; starts only test-owned processes."""
import json,os,socket,subprocess,sys,tempfile,time,urllib.request,urllib.error,wave,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
processes=[]
def start(args,env,log):
    f=open(log,'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=subprocess.STDOUT);f.close();processes.append(p);return p
with tempfile.TemporaryDirectory(prefix='multifx-test-') as directory:
    data=Path(directory);env={**os.environ,'MULTIFX_CONTROL_PORT':'19817'}
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
        assert len(metadata['parameters'])==919
        try:request('/api/state',auth=False);raise AssertionError('Unauthenticated access')
        except urllib.error.HTTPError as e:assert e.code==401
        parameter=next(p['id'] for p in metadata['parameters'] if p['key']=='drive.gain')
        assert request('/api/set',{'id':parameter,'value':.73})['ok']
        assert abs(request('/api/state')['values'][parameter]-.73)<1e-5
        assert request('/api/save',{'name':'integration'})['ok']
        request('/api/set',{'id':parameter,'value':.11})
        assert request('/api/load',{'name':'integration'})['ok']
        assert abs(request('/api/state')['values'][parameter]-.73)<1e-5
        path=data/'impulse.wav'
        with wave.open(str(path),'wb') as wav:wav.setnchannels(2);wav.setsampwidth(2);wav.setframerate(44100);wav.writeframes(b''.join(struct.pack('<hh',12000 if i==0 else 0,-6000 if i==0 else 0) for i in range(100)))
        assert request('/api/ir',path.read_bytes(),raw=True)['ok']
        assert request('/api/save',{'name':'embedded'})['ok']
        for message in ({'id':parameter,'value':1.5},{'id':-1,'value':.5}):
            try:
                result=request('/api/set',message)
                assert not result['ok']
            except urllib.error.HTTPError as e:assert e.code==400
        try:request('/api/load',{'name':'../outside'});raise AssertionError('Traversal')
        except urllib.error.HTTPError as e:assert e.code==400
        print('PASS actual core/HTTP: 919 parameters, authentication, parameter update, preset persistence, stereo WAV44100 -> 48000 IR import/embedding, invalid ranges and traversal rejected')
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
