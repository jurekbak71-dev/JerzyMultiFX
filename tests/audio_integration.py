#!/usr/bin/env python3
import os,subprocess,time,socket,json,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];processes=[]
with tempfile.TemporaryDirectory(prefix='multifx-audio-') as tmp:
 env={**os.environ,'JACK_DEFAULT_SERVER':'multifx-test-'+str(os.getpid())}
 def start(command,name):
  with open(Path(tmp)/(name+'.log'),'w') as f:p=subprocess.Popen(command,env=env,stdout=f,stderr=subprocess.STDOUT)
  processes.append(p)
 def rpc(command):
  with socket.create_connection(('127.0.0.1',19818),timeout=2) as s:
   s.sendall((command+'\n').encode());b=b''
   while not b.endswith(b'\n'):b+=s.recv(65536)
  return json.loads(b)
 try:
  start(['jackd','--name',env['JACK_DEFAULT_SERVER'],'--no-realtime','-d','dummy','-r','48000','-p','128'],'jack');time.sleep(.6)
  start([str(root/'.build/native/jerzy-multifx'),'--jack','19818'],'core')
  for i in range(100):
   try:metadata=rpc('METADATA');break
   except OSError:time.sleep(.05)
  else:raise RuntimeError('Audio readiness')
  def probe(midi=False):
   args=[str(root/'.build/native/jack_probe'),'JerzyMultiFX','300']
   if midi:args+=['midi']
   return json.loads(subprocess.check_output(args,env=env,stderr=subprocess.DEVNULL))
  baseline=probe();assert baseline['rms']>.001,baseline
  gain=next(p['id'] for p in metadata['parameters'] if p['key']=='cpd.gain')
  rpc(f'SET {gain} 0.3');louder=probe();assert louder['rms']>baseline['rms']*1.15,(baseline,louder)
  learn=next(p['id'] for p in metadata['parameters'] if p['key']=='midi.learn')
  rpc(f'SET {learn} {(gain+1)/metadata["controlCount"]}')
  midi=probe(True);state=rpc('STATE');assert state['values'][gain]>.99,'USB MIDI CC Learn'
  assert state['sampleRate']==48000 and midi['rms']>.001 and midi['peak']>.001
  print('PASS real JACK audio signal + gain response + USB-MIDI CC74 learn; baseline/louder:',baseline,louder,'xruns',state['xruns'])
 except BaseException:
  for p in Path(tmp).glob('*.log'):print(p.name,p.read_text()[-2500:])
  raise
 finally:
  for p in reversed(processes):
   if p.poll() is None:
    p.terminate()
    try:p.wait(timeout=5)
    except subprocess.TimeoutExpired:p.kill();p.wait()
