#!/usr/bin/env python3
"""Bounded, read-only Pi audio inventory. Never collect credentials or environment."""
import argparse,json,os,platform,socket,subprocess,time
from pathlib import Path
COMMANDS={
 'alsa_capture':['arecord','-l'], 'alsa_playback':['aplay','-l'],
 'usb_devices':['lsusb'], 'usb_tree':['lsusb','-t'],
 'midi_alsa':['aconnect','-l'], 'midi_raw':['amidi','-l'],
 'kernel_rt':['cat','/sys/kernel/realtime'],
 'jack_ports':['jack_lsp'],
}
FILES={'os_release':'/etc/os-release','model':'/proc/device-tree/model',
       'cpu':'/proc/cpuinfo','memory':'/proc/meminfo','alsa_cards':'/proc/asound/cards',
       'usb_audio':'/proc/asound/modules'}
UNITS=['jerzy-jack.service','jerzy-audio-hardware.service','bluetooth.service']
USER_UNITS=['jerzy-multifx-preview-core.service','jerzy-multifx-preview-web.service',
            'jerzy-multifx-usb-midi.service','jerzy-multifx-bluetooth-midi.service']
def read(path):
    try:
        value=Path(path).read_bytes()[:16000].replace(b'\0',b' ').decode('utf-8','replace')
        if path=='/proc/cpuinfo':value='\n'.join(line for line in value.splitlines() if not line.lower().startswith(('serial','revision')))
        return value
    except OSError:return None
def run(args):
    try:
        environment={'PATH':'/usr/sbin:/usr/bin:/sbin:/bin','LC_ALL':'C'}
        runtime=os.environ.get('XDG_RUNTIME_DIR')
        if runtime==f'/run/user/{os.getuid()}':environment['XDG_RUNTIME_DIR']=runtime
        result=subprocess.run(args,capture_output=True,text=True,timeout=3,env=environment)
        return {'exit':result.returncode,'output':result.stdout[:16000].strip()}
    except (OSError,subprocess.TimeoutExpired):return {'exit':None,'output':'unavailable'}
def core_state():
    try:
        with socket.create_connection(('127.0.0.1',9817),timeout=.5) as connection:
            connection.sendall(b'STATE\n')
            data=b''
            while not data.endswith(b'\n') and len(data)<65536:
                part=connection.recv(8192)
                if not part:break
                data+=part
        state=json.loads(data)
        return {key:state.get(key) for key in ('sampleRate','bufferSamples','latencySamples',
            'realtime','xruns','cpu','clips','midiEvents','lastMidi')}
    except (OSError,ValueError):return None

def latency(state):
    if not state:return {'round_trip_measured_ms':None,'measurement':'no physical loopback measurement'}
    rate=state.get('sampleRate');period=state.get('bufferSamples');algorithm=state.get('latencySamples')
    if not rate or not isinstance(rate,(int,float)) or rate<=0:return {'round_trip_measured_ms':None}
    return {'algorithm_samples':algorithm,'algorithm_ms':round(1000*algorithm/rate,3) if isinstance(algorithm,(int,float)) else None,
        'one_period_ms':round(1000*period/rate,3) if isinstance(period,(int,float)) else None,
        'round_trip_measured_ms':None,'measurement':'ADC/USB/DAC not measured; loopback required'}
def collect():
    report={'schema':1,'timestamp_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
        'host':{'machine':platform.machine(),'kernel':platform.release(),
                'python':platform.python_version(),'cpu_count':os.cpu_count()},
        'files':{name:read(path) for name,path in FILES.items()},
        'commands':{name:run(command) for name,command in COMMANDS.items()},
        'services':{name:run(['systemctl','is-active',name]) for name in UNITS},
        'user_services':{name:run(['systemctl','--user','is-active',name]) for name in USER_UNITS},
        'multifx':core_state()}
    report['latency']=latency(report['multifx'])
    # No addresses, tokens, login data, environment or logs. USB product names
    # and MIDI port names are retained to identify devices; owner controls sharing.
    return report
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    if args.output.is_symlink():raise SystemExit('Unsafe diagnostics destination')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    fd=os.open(args.output,os.O_WRONLY|os.O_CREAT|os.O_TRUNC|os.O_NOFOLLOW,0o600)
    with os.fdopen(fd,'w') as file:json.dump(collect(),file,ensure_ascii=False,indent=2)
    os.chmod(args.output,0o600)
if __name__=='__main__':main()
