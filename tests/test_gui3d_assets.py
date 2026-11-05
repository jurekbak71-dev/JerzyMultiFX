import hashlib
import json
import struct
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "plugin-vst3" / "resources" / "gui3d"


def png_header(path: Path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise AssertionError(f"invalid PNG: {path}")
    width, height, bit_depth, colour_type = struct.unpack(">IIBB", data[16:26])
    return width, height, bit_depth, colour_type, data


class Gui3DAssets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((ASSETS / "asset_manifest.json").read_text(encoding="utf-8"))

    def test_manifest_and_checksums(self):
        self.assertEqual(self.manifest["design_surface"], [1280, 720])
        self.assertGreaterEqual(len(self.manifest["assets"]), 50)
        for item in self.manifest["assets"]:
            path = ASSETS / item["file"]
            width, height, depth, colour_type, data = png_header(path)
            self.assertEqual((width, height), (item["width"], item["height"]))
            self.assertEqual(depth, 8)
            self.assertEqual(colour_type, 6, f"{path} must use RGBA")
            self.assertEqual(hashlib.sha256(data).hexdigest(), item["sha256"])

    def test_knob_strips_are_independent_101_frame_assets(self):
        strips = [a for a in self.manifest["assets"] if a["type"] == "knob_strip"]
        self.assertEqual(len(strips), 15)
        self.assertEqual({a["family"] for a in strips}, {"master", "fx", "select"})
        for item in strips:
            self.assertEqual(item["frames"], 101)
            self.assertEqual(item["width"], item["frame_width"] * 101)
            self.assertEqual(item["height"], item["frame_height"])
            self.assertEqual(item["pivot"], [item["frame_width"] // 2, item["frame_height"] // 2])


if __name__ == "__main__":
    unittest.main()
