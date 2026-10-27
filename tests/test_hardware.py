import json,os,socket,sys,tempfile,threading,time,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'rpi'))
import hardware
from bluetooth_relay import decode_event

class HardwareIntegrationTests(unittest.TestCase):
    def test_broker_unavailable_does_not_hide_usb_midi(self):
        def rpc(cmd):return {'ports':[{'name':'a2j:USB [24] (capture): port 0','connected':True}]} if cmd=='PORTS' else {'inputs':['system:capture_1'],'outputs':['system:playback_1'],'routing':['']*6}
        with patch.object(hardware,'STATUS',Path('/nonexistent/jerzy/status.json')),patch.object(hardware,'rpc',side_effect=rpc):
            result=hardware.status()
        self.assertFalse(result['available']);self.assertEqual(len(result['midi_inputs']),1)
        self.assertEqual(result['audio_ports']['inputs'],['system:capture_1'])
    def test_existing_broker_is_uid_checked_and_phone_url_uses_current_panel(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'status.json';path.write_text(json.dumps({'uid':os.getuid(),'updated':time.time(),'bluetooth':{'pan':{'url':'http://10.0.0.2:8093/'}}}))
            with patch.object(hardware,'STATUS',path),patch.object(hardware,'rpc',return_value={'ports':[]}):
                result=hardware.status();self.assertTrue(result['available']);self.assertEqual(result['bluetooth']['pan']['url'],'http://10.0.0.2:8097/')
                value=json.loads(path.read_text());value['uid']+=1;path.write_text(json.dumps(value));self.assertFalse(hardware.status()['available'])
    def test_actual_unix_broker_message_framing(self):
        with tempfile.TemporaryDirectory() as d:
            path=str(Path(d)/'broker.sock');server=socket.socket(socket.AF_UNIX);server.bind(path);server.listen(1);received=[]
            def reply():
                with server.accept()[0] as c:
                    data=b''
                    while not data.endswith(b'\n'):data+=c.recv(1024)
                    received.append(json.loads(data));c.sendall(b'{"ok":true,"pending":true}\n')
            thread=threading.Thread(target=reply);thread.start()
            try:
                with patch.object(hardware,'SOCKET',path):self.assertTrue(hardware.call('midi-connect',address='AA:BB:CC:DD:EE:FF')['pending'])
                thread.join(timeout=2);self.assertFalse(thread.is_alive());self.assertEqual(received,[{'operation':'midi-connect','address':'AA:BB:CC:DD:EE:FF'}])
            finally:server.close()
    def test_no_arbitrary_commands_or_midi_port_newline_injection(self):
        for data in [{'operation':'shell','command':'anything'},{'operation':'midi-input','name':'port\nSET 0 1','enable':True},{'operation':'buffer','buffer':12},{'operation':'audio-route','index':-1,'name':'system:capture_1'}]:
            with self.subTest(data=data),patch.object(hardware,'call') as call,patch.object(hardware,'rpc') as rpc:
                with self.assertRaises(ValueError):hardware.action(data)
                call.assert_not_called();rpc.assert_not_called()
    def test_actual_bluetooth_payloads_preserve_cc_pc_clock_and_panic(self):
        for message in ([176,74,127],[192,3],[248],[144,60,100]):
            self.assertEqual(decode_event(json.dumps({'data':message}).encode()),bytes(message))
        self.assertEqual(decode_event(b'{"panic":true}'),b'!')
        for message in ([176,128,0],[0],[176,74,True],[],[176,74,127,0]):
            with self.assertRaises(ValueError):decode_event(json.dumps({'data':message}).encode())
