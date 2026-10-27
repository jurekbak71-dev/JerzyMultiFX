"""Shared local control protocol, used by HTTP and authenticated BLE."""
import json, socket, os

def rpc(command):
    if len(command.encode()) > 4095 or '\n' in command or '\r' in command:
        raise ValueError('Invalid command')
    with socket.create_connection(('127.0.0.1', int(os.environ.get('MULTIFX_CONTROL_PORT', '9817'))), timeout=5) as s:
        s.sendall((command+'\n').encode())
        response = bytearray()
        while not response.endswith(b'\n'):
            part = s.recv(65536)
            if not part or len(response)>1024*1024:
                raise OSError('Invalid core response')
            response.extend(part)
        return json.loads(response)

def ble_command(message):
    if not isinstance(message, dict):
        raise ValueError('Object required')
    op = message.get('op')
    if op == 'set':
        i, v = message['id'], message['value']
        if not isinstance(i, int) or not isinstance(v, (int, float)) or not 0 <= v <= 1:
            raise ValueError('Invalid parameter')
        return rpc(f'SET {i} {v}')
    if op in ('ir','load','save'):
        from pathlib import Path
        name=message.get('name','')
        if not isinstance(name,str) or not name or len(name)>80 or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in name):
            raise ValueError('Local library name required')
        data=Path(os.environ.get('MULTIFX_DATA','/var/lib/jerzy-multifx'))
        path=data/('ir' if op=='ir' else 'presets')/(name+('.wav' if op=='ir' else '.jmfx'))
        if op=='save':path.parent.mkdir(parents=True,exist_ok=True)
        return rpc(('IR ' if op=='ir' else 'SAVE ' if op=='save' else 'LOAD ')+str(path))
    if op == 'page':
        start = int(message.get('start', 0))
        if start < 0:
            raise ValueError('Invalid page')
        state = rpc('STATE')
        return {'start': start, 'values': state['values'][start:start+16], 'in': state['in'], 'out':state['out']}
    raise ValueError('Unsupported BLE operation')

def ble_midi(data):
    """Decode one BLE-MIDI notification, including running status and timestamp bytes."""
    data = bytes(data)
    if len(data) < 3 or data[0] & 0x80 == 0:
        raise ValueError('BLE MIDI header')
    events, pos, status = [], 1, None
    while pos < len(data):
        if data[pos] & 0x80:
            pos += 1  # Timestamp low byte, as required at message boundaries.
        if pos >= len(data):
            break
        if data[pos] & 0x80:
            status = data[pos]
            pos += 1
        if status is None:
            raise ValueError('Missing MIDI status')
        size = 1 if status & 0xf0 in (0xc0,0xd0) else 2
        if pos+size > len(data) or any(x & 0x80 for x in data[pos:pos+size]):
            raise ValueError('MIDI data length')
        events.append(bytes([status])+data[pos:pos+size])
        pos += size
    return events
