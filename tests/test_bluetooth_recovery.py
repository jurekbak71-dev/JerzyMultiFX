import importlib.util,subprocess,unittest
from pathlib import Path
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('bluetooth_recovery',Path(__file__).resolve().parents[1]/'rpi/bluetooth_recovery.py')
radio=importlib.util.module_from_spec(spec);spec.loader.exec_module(radio)
class RadioRecoveryTests(unittest.TestCase):
    def test_healthy_radio_is_not_restarted(self):
        with patch.object(radio,'powered',return_value=True),patch.object(radio,'run') as run:
            self.assertTrue(radio.recover());run.assert_not_called()
    def test_busy_is_retried_after_unblocking(self):
        def command(*args,**kwargs):return subprocess.CompletedProcess(args,0,'Soft blocked: yes\nHard blocked: no' if args[0]=='rfkill' else '', '')
        with patch.object(radio,'powered',side_effect=[False,False,True]),patch.object(radio,'run',side_effect=command) as run,patch.object(radio.time,'sleep'):
            self.assertTrue(radio.recover());self.assertEqual(sum(c.args[:3]==('bluetoothctl','--timeout','4') for c in run.call_args_list),2)
            self.assertFalse(any(c.args[:2]==('systemctl','restart') for c in run.call_args_list))
    def test_hard_block_is_not_treated_as_repaired(self):
        with patch.object(radio,'powered',return_value=False),patch.object(radio,'run',return_value=subprocess.CompletedProcess([],0,'Hard blocked: yes','')):
            with self.assertRaisesRegex(RuntimeError,'sprzętowo'):radio.recover()
    def test_broker_is_restored_when_restart_fails(self):
        calls=[]
        def command(*args,**kwargs):
            calls.append(args)
            if args[:2]==('systemctl','restart'):raise subprocess.CalledProcessError(1,args)
            return subprocess.CompletedProcess(args,0,'','')
        with patch.object(radio,'powered',return_value=False),patch.object(radio,'run',side_effect=command),patch.object(radio.time,'sleep'):
            with self.assertRaises(subprocess.CalledProcessError):radio.recover()
        self.assertIn(('systemctl','start','jerzy-audio-hardware.service'),calls)
