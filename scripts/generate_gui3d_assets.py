#!/usr/bin/env python3
"""Generate deterministic production GUI3D sprites from the approved Jerzy style.

The supplied atlases are design references. This script recreates clean assets with
stable dimensions, pivots, straight alpha and checksums; it does not crop text or
shared shadows from the concept sheets.
"""
from __future__ import annotations

import hashlib
import json
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "plugin-vst3" / "resources" / "gui3d"
COLORS = {
    "neutral": (190, 198, 201),
    "amber": (255, 153, 20),
    "cyan": (36, 218, 242),
    "red": (255, 73, 70),
    "purple": (184, 77, 255),
}
FAMILIES = {"master": 112, "fx": 88, "select": 64}


def mix(a: tuple[int, ...], b: tuple[int, ...], t: float) -> tuple[int, ...]:
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def save(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)


def metal_disc(size: int, accent: tuple[int, int, int]) -> Image.Image:
    scale = 3
    s = size * scale
    image = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    pixels = image.load()
    c = (s - 1) / 2
    radius = s * 0.39
    for y in range(s):
        for x in range(s):
            dx, dy = x - c, y - c
            r = math.hypot(dx, dy)
            if r > radius:
                continue
            radial = min(1.0, r / radius)
            light = max(0.0, 1.0 - math.hypot(dx + radius * .30, dy + radius * .35) / (radius * 1.6))
            brushed = 5.0 * math.sin(math.atan2(dy, dx) * 44.0 + radial * 18.0)
            value = int(45 + light * 145 - radial * 40 + brushed)
            pixels[x, y] = (value, value + 2, value + 3, 255)
    d = ImageDraw.Draw(image)
    box = lambda inset: (inset, inset, s - inset - 1, s - inset - 1)
    d.ellipse(box(int(s * .055)), outline=(4, 5, 6, 245), width=max(2, int(s * .055)))
    d.ellipse(box(int(s * .085)), outline=(*accent, 235), width=max(2, int(s * .025)))
    d.ellipse(box(int(s * .125)), outline=(225, 228, 226, 210), width=max(2, int(s * .018)))
    for i in range(21):
        angle = math.radians(-135 + i * 13.5)
        r1, r2 = s * .445, s * (.485 if i % 5 == 0 else .47)
        x1, y1 = c + math.sin(angle) * r1, c - math.cos(angle) * r1
        x2, y2 = c + math.sin(angle) * r2, c - math.cos(angle) * r2
        d.line((x1, y1, x2, y2), fill=(*accent, 230) if i in (0, 20) else (188, 192, 190, 220),
               width=max(2, int(s * .012)))
    return image.resize((size, size), Image.Resampling.LANCZOS)


def pointer(size: int, angle_deg: float, accent: tuple[int, int, int]) -> Image.Image:
    scale = 4
    s = size * scale
    c = s / 2
    angle = math.radians(angle_deg)
    ux, uy = math.sin(angle), -math.cos(angle)
    image = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    glow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.line((c + ux * s * .05, c + uy * s * .05, c + ux * s * .30, c + uy * s * .30),
            fill=(*accent, 210), width=max(6, int(s * .07)))
    glow = glow.filter(ImageFilter.GaussianBlur(s * .018))
    image.alpha_composite(glow)
    d = ImageDraw.Draw(image)
    d.line((c + ux * s * .04, c + uy * s * .04, c + ux * s * .31, c + uy * s * .31),
           fill=(14, 12, 10, 255), width=max(5, int(s * .052)))
    d.line((c + ux * s * .05, c + uy * s * .05, c + ux * s * .30, c + uy * s * .30),
           fill=(248, 247, 235, 255), width=max(3, int(s * .026)))
    d.line((c + ux * s * .22, c + uy * s * .22, c + ux * s * .31, c + uy * s * .31),
           fill=(*accent, 255), width=max(2, int(s * .014)))
    return image.resize((size, size), Image.Resampling.LANCZOS)


def knob_assets() -> list[dict]:
    assets = []
    for family, size in FAMILIES.items():
        shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        sd = ImageDraw.Draw(shadow)
        sd.ellipse((size * .13, size * .18, size * .90, size * .94), fill=(0, 0, 0, 185))
        shadow = shadow.filter(ImageFilter.GaussianBlur(size * .055))
        shadow_path = OUT / "shadows" / f"knob_{family}_shadow.png"
        save(shadow, shadow_path)
        assets.append(asset_record("knob_shadow", family, shadow_path, pivot=[size // 2, size // 2]))
        for colour, rgb in COLORS.items():
            base = metal_disc(size, rgb)
            strip = Image.new("RGBA", (size * 101, size), (0, 0, 0, 0))
            for frame in range(101):
                item = base.copy()
                item.alpha_composite(pointer(size, -135.0 + frame * 2.7, rgb))
                strip.paste(item, (frame * size, 0), item)
            path = OUT / "knobs" / f"knob_{family}_{colour}_101.png"
            save(strip, path)
            assets.append(asset_record("knob_strip", family, path, frames=101,
                                       frame_width=size, frame_height=size,
                                       pivot=[size // 2, size // 2], colour=colour,
                                       angle_min_deg=-135, angle_max_deg=135))
    return assets


def rounded_asset(size: tuple[int, int], accent: tuple[int, int, int], state: str) -> Image.Image:
    w, h = size
    scale = 3
    image = Image.new("RGBA", (w * scale, h * scale), (0, 0, 0, 0))
    d = ImageDraw.Draw(image)
    box = (scale * 4, scale * 4, scale * (w - 4), scale * (h - 4))
    if state in {"on", "hover", "pressed"}:
        glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.rounded_rectangle(box, radius=scale * 9, outline=(*accent, 230), width=scale * 7)
        image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(scale * 4)))
    top = (54, 54, 52, 255) if state != "disabled" else (45, 45, 45, 150)
    bottom = (10, 10, 10, 255) if state != "pressed" else (25, 17, 8, 255)
    for y in range(box[1], box[3] + 1):
        t = (y - box[1]) / max(1, box[3] - box[1])
        colour = mix(top, bottom, t)
        d.rounded_rectangle((box[0], y, box[2], y + 1), radius=scale * 8, fill=colour)
    outline = accent if state in {"on", "hover", "pressed"} else (120, 122, 119)
    alpha = 255 if state != "disabled" else 120
    d.rounded_rectangle(box, radius=scale * 8, outline=(*outline, alpha), width=scale * (3 if state == "pressed" else 2))
    d.rounded_rectangle((box[0] + scale * 3, box[1] + scale * 3, box[2] - scale * 3, box[3] - scale * 3),
                        radius=scale * 6, outline=(230, 228, 214, 90), width=scale)
    return image.resize(size, Image.Resampling.LANCZOS)


def button_assets() -> list[dict]:
    assets = []
    for colour, rgb in COLORS.items():
        for state in ("off", "on", "hover", "pressed", "disabled"):
            path = OUT / "switches" / f"button_{colour}_{state}.png"
            save(rounded_asset((180, 48), rgb, state), path)
            assets.append(asset_record("button", "routing", path, colour=colour, state=state))
        for state in ("off", "on"):
            light = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
            ld = ImageDraw.Draw(light)
            if state == "on":
                glow = Image.new("RGBA", light.size, (0, 0, 0, 0))
                ImageDraw.Draw(glow).ellipse((5, 5, 43, 43), fill=(*rgb, 180))
                light.alpha_composite(glow.filter(ImageFilter.GaussianBlur(8)))
            ld.ellipse((12, 12, 36, 36), fill=(8, 8, 8, 255), outline=(170, 172, 168, 255), width=2)
            ld.ellipse((16, 16, 32, 32), fill=(*rgb, 255) if state == "on" else (54, 54, 52, 255))
            path = OUT / "lights" / f"led_{colour}_{state}.png"
            save(light, path)
            assets.append(asset_record("led", "status", path, colour=colour, state=state))
    return assets


def brushed_panel(size: tuple[int, int], accent: tuple[int, int, int] | None = None) -> Image.Image:
    w, h = size
    random.seed(71 + w + h + (accent[0] if accent else 0))
    image = Image.new("RGBA", size, (18, 19, 20, 255))
    p = image.load()
    for y in range(h):
        for x in range(w):
            grain = random.randint(-5, 5) + int(4 * math.sin(x * .18 + y * .025))
            base = max(4, min(48, 24 + grain - int(y / max(1, h) * 9)))
            p[x, y] = (base, base + 1, base + 2, 255)
    d = ImageDraw.Draw(image)
    d.rounded_rectangle((1, 1, w - 2, h - 2), radius=max(4, min(w, h) // 16),
                        outline=(154, 157, 155, 255), width=2)
    d.rounded_rectangle((4, 4, w - 5, h - 5), radius=max(3, min(w, h) // 18),
                        outline=(4, 5, 6, 255), width=2)
    if accent:
        glow = Image.new("RGBA", size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.line((12, h - 5, w - 12, h - 5), fill=(*accent, 230), width=3)
        image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(4)))
        d.line((12, h - 5, w - 12, h - 5), fill=(*accent, 255), width=1)
    return image


def panel_assets() -> list[dict]:
    assets = []
    path = OUT / "panels" / "chassis_dark_256.png"
    save(brushed_panel((256, 256)), path)
    assets.append(asset_record("panel", "chassis", path, nine_slice=[16, 16, 16, 16]))
    for colour, rgb in COLORS.items():
        path = OUT / "panels" / f"module_{colour}_320x180.png"
        save(brushed_panel((320, 180), rgb), path)
        assets.append(asset_record("panel", "module", path, colour=colour, nine_slice=[18, 18, 18, 18]))
    return assets


def meter_assets() -> list[dict]:
    assets = []
    w, h = 220, 90
    face = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(face)
    d.rounded_rectangle((2, 2, w - 3, h - 3), radius=10, fill=(12, 11, 9, 255), outline=(173, 176, 172, 255), width=3)
    for y in range(10, h - 9):
        t = (y - 10) / (h - 20)
        d.rounded_rectangle((10, y, w - 11, y + 1), radius=5,
                            fill=mix((255, 222, 126, 255), (125, 69, 18, 255), t))
    pivot = (w // 2, h - 10)
    for i in range(11):
        a = math.radians(-58 + i * 11.6)
        r1, r2 = 59, 69
        p1 = (pivot[0] + math.sin(a) * r1, pivot[1] - math.cos(a) * r1)
        p2 = (pivot[0] + math.sin(a) * r2, pivot[1] - math.cos(a) * r2)
        d.line((*p1, *p2), fill=(80, 29, 10, 255), width=2)
    face_path = OUT / "meters" / "vu_face.png"
    save(face, face_path)
    assets.append(asset_record("meter_face", "vu", face_path, pivot=list(pivot)))
    needle = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    nd = ImageDraw.Draw(needle)
    nd.line((pivot[0], pivot[1], pivot[0], 16), fill=(55, 8, 5, 255), width=5)
    nd.line((pivot[0], pivot[1], pivot[0], 16), fill=(244, 62, 35, 255), width=2)
    nd.ellipse((pivot[0] - 6, pivot[1] - 6, pivot[0] + 6, pivot[1] + 6), fill=(51, 45, 36, 255), outline=(218, 201, 160, 255), width=2)
    needle_path = OUT / "meters" / "vu_needle.png"
    save(needle, needle_path)
    assets.append(asset_record("meter_needle", "vu", needle_path, pivot=list(pivot), angle_min_deg=-58, angle_max_deg=58))
    glass = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glass)
    gd.ellipse((-30, -65, w + 30, 48), fill=(255, 255, 255, 26))
    glass_path = OUT / "meters" / "vu_glass.png"
    save(glass, glass_path)
    assets.append(asset_record("meter_glass", "vu", glass_path))
    return assets


def asset_record(kind: str, family: str, path: Path, **extra) -> dict:
    image = Image.open(path)
    record = {
        "type": kind,
        "family": family,
        "file": path.relative_to(OUT).as_posix(),
        "width": image.width,
        "height": image.height,
        "alpha": "straight",
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "author": "Jerzy MultiFX / procedural recreation from user-approved generated references",
    }
    record.update(extra)
    return record


def main() -> None:
    assets = knob_assets() + button_assets() + panel_assets() + meter_assets()
    manifest = {
        "version": 1,
        "design_surface": [1280, 720],
        "source": "User-supplied JerzyMultiFX GUI3D concept atlases; assets recreated procedurally",
        "reference_sha256": {
            "knobs_atlas": "e6225b53e6cf14b6fdc2f811d746c0a2b165db3646817c9509e6741f5ca8c2b0",
            "buttons_atlas": "e9951a12742320371999c95d5cd2a54b1a3af7d9b214072f3b0f92c2fc19a1b3",
            "panels_atlas": "d70f47f77888cc691a097f816518db3c618532a3861ab78bdc0040c374049639",
            "meters_atlas": "6264fc9cb72aee8aa80700a28fefb7bdaa455e142f90cad728ec467f78ec508e",
        },
        "assets": assets,
    }
    (OUT / "asset_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {len(assets)} GUI3D assets in {OUT}")


if __name__ == "__main__":
    main()
