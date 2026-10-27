#!/usr/bin/env python3
"""Receive the existing UID-protected hardware broker's BLE MIDI; no old-panel actions."""
import argparse,asyncio,fcntl,json,os,signal,socket,stat,sys,time
from pathlib import Path

def decode_event(raw):
    if len(raw)>2048:raise ValueError('MIDI packet too long')
    value=json.loads(raw)
    if not isinstance(value,dict):raise ValueError('MIDI object required')
    if value.get('panic'):return b'!'
    data=value.get('data')
    if not isinstance(data,list) or not 1<=len(data)<=3 or any(type(b) is not int or not 0<=b<=255 for b in data):raise ValueError('Invalid MIDI bytes')
    if data[0]<128 or any(b>127 for b in data[1:]):raise ValueError('Invalid MIDI message')
    return bytes(data)

class Relay(asyncio.DatagramProtocol):
    def __init__(self,runtime,data):self.runtime=runtime;self.data=data;self.received=0;self.dropped=0;self.last=None
    def datagram_received(self,raw,address):
        try:
            packet=decode_event(raw)
            with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as s:s.setblocking(False);s.sendto(packet,str(self.runtime/'jerzy-midi-jack.sock'))
            self.received+=1;self.last={'bytes':list(packet),'time':time.time()}
        except (ValueError,TypeError,OSError,json.JSONDecodeError):self.dropped+=1
    async def status(self):
        while True:
            tmp=self.data/'bluetooth-midi.json.tmp';tmp.write_text(json.dumps({'running':True,'received':self.received,'dropped':self.dropped,'last_event':self.last}));tmp.chmod(0o600);os.replace(tmp,self.data/'bluetooth-midi.json');await asyncio.sleep(1)

def remove_owned_socket(path):
    if path.exists():
        info=path.lstat()
        if info.st_uid!=os.getuid() or not stat.S_ISSOCK(info.st_mode):raise RuntimeError('Unsafe MIDI socket path')
        path.unlink()

async def main():
    p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--data',type=Path,required=True);a=p.parse_args()
    runtime=Path('/run/user')/str(os.getuid());a.data.mkdir(parents=True,exist_ok=True)
    with (runtime/'jerzy-multifx-midi.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        for name in ('jerzy-midi-ble.sock','jerzy-midi-jack.sock'):remove_owned_socket(runtime/name)
        bridge=await asyncio.create_subprocess_exec(str(a.binary),str(runtime/'jerzy-midi-jack.sock'),str(a.data/'bluetooth-midi-bridge.json'))
        transport=None;tasks=[]
        try:
            for attempt in range(100):
                if (runtime/'jerzy-midi-jack.sock').exists():break
                if bridge.returncode is not None:raise RuntimeError('Native MIDI bridge could not start')
                await asyncio.sleep(.025)
            else:raise RuntimeError('Native MIDI bridge did not create its socket')
            relay=Relay(runtime,a.data)
            transport,_=await asyncio.get_running_loop().create_datagram_endpoint(lambda:relay,local_addr=str(runtime/'jerzy-midi-ble.sock'),family=socket.AF_UNIX)
            (runtime/'jerzy-midi-ble.sock').chmod(0o600)
            stop=asyncio.Event()
            for signum in (signal.SIGINT,signal.SIGTERM):asyncio.get_running_loop().add_signal_handler(signum,stop.set)
            tasks=[asyncio.create_task(relay.status()),asyncio.create_task(bridge.wait()),asyncio.create_task(stop.wait())]
            done,_=await asyncio.wait(tasks,return_when=asyncio.FIRST_COMPLETED)
            for task in done:
                if task.exception():raise task.exception()
            if bridge.returncode is not None:raise RuntimeError('JACK MIDI bridge stopped; systemd will reconnect')
        finally:
            if bridge.returncode is None:
                bridge.terminate()
                try:await asyncio.wait_for(bridge.wait(),4)
                except asyncio.TimeoutError:bridge.kill();await bridge.wait()
            if transport:transport.close()
            for task in tasks:task.cancel()
            for name in ('jerzy-midi-ble.sock','jerzy-midi-jack.sock'):remove_owned_socket(runtime/name)
if __name__=='__main__':
    try:asyncio.run(main())
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
