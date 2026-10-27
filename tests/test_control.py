import sys,unittest,json,tempfile
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'rpi'))
import control
from lcd import HD44780
from lcd_service import lines
class ControlTests(unittest.TestCase):
    def test_ble_midi_cc_and_program(self):
        self.assertEqual(control.ble_midi(bytes([0x80,0x80,0xb0,74,127])),[bytes([0xb0,74,127])])
        self.assertEqual(control.ble_midi(bytes([0x80,0x80,0xc2,4])),[bytes([0xc2,4])])
        with self.assertRaises(ValueError):control.ble_midi(bytes([0x80,0x80,0xb0,74]))
    def test_ble_commands_and_bounds(self):
        with patch.object(control,'rpc',return_value={'ok':True}) as rpc:
            self.assertEqual(control.ble_command({'op':'set','id':6,'value':.5}),{'ok':True})
            rpc.assert_called_once_with('SET 6 0.5')
        for message in ({'op':'set','id':0,'value':float('nan')},{'op':'set','id':0,'value':1.1},{'op':'ir','name':'../secret'}):
            with self.assertRaises(ValueError):control.ble_command(message)
    def test_ble_state_page_is_under_512_bytes(self):
        with patch.object(control,'rpc',return_value={'values':[.123456789]*1000,'in':.2,'out':.4}):
            response=control.ble_command({'op':'page','start':32})
            self.assertEqual(len(response['values']),16)
            self.assertLess(len(json.dumps(response).encode()),512)
    def test_lcd_four_bit_protocol_and_exact_line_width(self):
        writes=[];driver=HD44780(lambda rs,e,b:writes.append((rs,e,b)),sleep=lambda _:None)
        driver.initialize();writes.clear();driver.display('Preset test','MIDI BLE WIFI')
        self.assertEqual(len(writes),34*6)
        decoded=[]
        for i in range(0,len(writes),6):
            high=sum(v<<k for k,v in enumerate(writes[i][2]));low=sum(v<<k for k,v in enumerate(writes[i+3][2]));decoded.append((writes[i][0],high*16+low))
        self.assertEqual(decoded[0],(0,0x80));self.assertEqual(decoded[17],(0,0xc0))
        self.assertEqual(bytes(v for rs,v in decoded if rs).decode(),'Preset test'.ljust(16)+'MIDI BLE WIFI'.ljust(16))
        self.assertEqual([len(x) for x in lines({'out':.2,'xruns':3})],[16,16])
if __name__=='__main__':unittest.main()
