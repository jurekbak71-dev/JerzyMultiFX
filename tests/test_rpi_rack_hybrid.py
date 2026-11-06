import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class RaspberryPiRackHybridTests(unittest.TestCase):
    def test_skin_is_loaded_and_served(self):
        index = (ROOT / 'ui/index.html').read_text(encoding='utf-8')
        server = (ROOT / 'rpi/web.py').read_text(encoding='utf-8')
        packaging = (ROOT / 'scripts/package.py').read_text(encoding='utf-8')
        self.assertIn('href="/rack-hybrid.css"', index)
        self.assertIn("'/rack-hybrid.css'", server)
        self.assertIn("'rack-hybrid.css'", packaging)

    def test_skin_preserves_existing_control_logic(self):
        css = (ROOT / 'ui/rack-hybrid.css').read_text(encoding='utf-8')
        js = (ROOT / 'ui/app.js').read_text(encoding='utf-8')
        for selector in ('.chain', '.block[data-block="0"]',
                         '.block[data-block="1"]', '.block[data-block="2"]',
                         '.block[data-block="3"]', '.block[data-block="4"]',
                         '.subtabs', '.pedal', '.rotary', '#meters'):
            self.assertIn(selector, css)
        for semantic in ("await set(find('chain.order')", "function control(p",
                         "function renderPanel()", "learnButton("):
            self.assertIn(semantic, js)
        self.assertIn('rackQuickKeys=', js)
        self.assertIn('quick.append(control(p))', js)
        self.assertIn('.rack-quick', css)
        self.assertIn("order.filter(n=>n!==4).concat(order.includes(4)?[4]:[])", js)
        self.assertIn("detailsExpanded=false", js)
        self.assertIn("function dspLatencyDetails()", js)
        self.assertIn("async function enableLiveMode()", js)
        self.assertIn("async function restoreLiveMode()", js)
        self.assertIn("#controls[hidden]", css)
        self.assertNotIn('pointer-events:none!important', css)

if __name__ == '__main__':
    unittest.main()
