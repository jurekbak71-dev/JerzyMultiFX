import importlib.util,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('mfx_diagnostics',Path(__file__).resolve().parents[1]/'scripts/collect_rpi_diagnostics.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class DiagnosticsTests(unittest.TestCase):
    def test_latency_distinguishes_calculation_from_physical_measurement(self):
        result=module.latency({'sampleRate':48000,'bufferSamples':128,'latencySamples':2048})
        self.assertEqual(result['algorithm_ms'],42.667)
        self.assertEqual(result['one_period_ms'],2.667)
        self.assertIsNone(result['round_trip_measured_ms'])
    def test_inventory_does_not_read_credentials_or_environment(self):
        with patch.object(module,'read',return_value=None),patch.object(module,'run',return_value={'exit':1,'output':''}),patch.object(module,'core_state',return_value=None):
            report=module.collect()
        self.assertNotIn('environment',report)
        self.assertNotIn('token',str(report).lower())
        self.assertIsNone(report['latency']['round_trip_measured_ms'])
        self.assertEqual(module.COMMANDS['jack_connections'],['jack_lsp','-c'])
        self.assertEqual(module.COMMANDS['jack_port_latency'],['jack_lsp','-l'])
