#!/usr/bin/env python3
"""Make the app icons and logo images in apple/InsertCoin/Assets.xcassets from
the artwork in art/ (needs Pillow and numpy: run it with ~/venvs/afic/bin/python).

The icon is the AF mark with INSERT COIN under it on dark navy. iPhone/iPad
get it flat at 1024 px, the Mac at every size inside its rounded square (the
text left off below 128 px), Apple TV as a layered (parallax) icon (navy,
the red and orange AF, then the yellow F and the text) plus the Top Shelf
images. The catalog also gets the logo for the splash screen and About box
(Logo), the launch screen's logo sized per device (LaunchLogo), the
library's top-bar logos on iPhone and iPad (NavMark, NavLogo) and the
launch screen colour (LaunchBackground). Run it again after
changing art/; the PNGs are committed.
"""

import json
import os
import shutil

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CATALOG = os.path.join(ROOT, "apple", "InsertCoin", "Assets.xcassets")
ART = os.path.join(ROOT, "art")
INFO = {"author": "xcode", "version": 1}

NAVY = (22, 22, 32)


def art(name):
    return Image.open(os.path.join(ART, name)).convert("RGBA")


AF = art("af-mark.png")
COIN = art("insert-coin-text.png")  # "INSERT COIN", yellow

def is_yellow(rgba):
    r, g, b = rgba[:, :, 0].astype(int), rgba[:, :, 1].astype(int), rgba[:, :, 2].astype(int)
    return (g > 160) & (r > 200) & (b < 120)


def split_af():
    """The AF mark's yellow F half, and the red and orange rest."""
    a = np.array(AF)
    y = is_yellow(a)
    front, rest = a.copy(), a.copy()
    front[:, :, 3] = np.where(y, a[:, :, 3], 0)
    rest[:, :, 3] = np.where(y, 0, a[:, :, 3])
    return Image.fromarray(front), Image.fromarray(rest)


YELLOW_F, RED_AF = split_af()


def navy(w, h):
    return Image.new("RGBA", (w, h), NAVY + (255,))


def clear(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def group_size(width, text):
    """Size of the AF mark (width px wide) with INSERT COIN under it."""
    a_h = round(AF.height * width / AF.width)
    if not text:
        return width, a_h, 0, 0, 0
    tw = round(width * 0.86)
    th = round(COIN.height * tw / COIN.width)
    gap = round(width * 0.10)
    return width, a_h + gap + th, tw, th, gap


def paint(canvas, parts, frac, text=True):
    """Draws the chosen pieces ("af", "red", "yellow", "coin") centred as one
    group of the same layout; frac is the mark's width as a fraction of the
    canvas width."""
    w, h = canvas.size
    gw = round(w * frac)
    gw, gh, tw, th, gap = group_size(gw, text)
    x0, y0 = (w - gw) // 2, (h - gh) // 2
    a_h = round(AF.height * gw / AF.width)
    for part in parts:
        if part == "coin":
            if text:
                t = COIN.resize((tw, th), Image.LANCZOS)
                canvas.alpha_composite(t, (x0 + (gw - tw) // 2, y0 + a_h + gap))
            continue
        src = {"af": AF, "red": RED_AF, "yellow": YELLOW_F}[part]
        canvas.alpha_composite(src.resize((gw, a_h), Image.LANCZOS), (x0, y0))
    return canvas


def mark_fraction(w, h):
    """The mark's width as a fraction of the icon's: wide canvases (Apple TV,
    Top Shelf) get a smaller one so the group fits the height."""
    return min(0.72, 0.72 * (h / w) * 1.35)


def flat(w, h):
    """The whole icon on navy."""
    return paint(navy(w, h), ["af", "coin"], mark_fraction(w, h))


def layer_back(w, h):
    return navy(w, h)


def layer_middle(w, h):
    return paint(clear(w, h), ["red"], mark_fraction(w, h))


def layer_front(w, h):
    return paint(clear(w, h), ["yellow", "coin"], mark_fraction(w, h))


def mac_icon(px):
    """The macOS shape: the picture in a rounded square with a margin. Under
    128 px the text is left off, it would not be legible."""
    inset = round(px * 100 / 1024)
    side = px - 2 * inset
    tile = paint(navy(side, side), ["af", "coin"], 0.72, text=px >= 128)
    mask = Image.new("L", (side, side), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, side - 1, side - 1], radius=round(side * 0.225), fill=255)
    out = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    out.paste(tile, (inset, inset), mask)
    return out


def write_json(path, data):
    os.makedirs(path, exist_ok=True)
    with open(os.path.join(path, "Contents.json"), "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def save(im, path, opaque=False):
    (im.convert("RGB") if opaque else im).save(path, optimize=True)


def app_icon():
    path = os.path.join(CATALOG, "AppIcon.appiconset")
    write_json(path, {})
    images = [{"filename": "ios-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024"}]
    save(flat(1024, 1024), os.path.join(path, "ios-1024.png"), opaque=True)
    for pt in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f"mac-{pt}@{scale}x.png"
            save(mac_icon(pt * scale), os.path.join(path, name))
            images.append({"filename": name, "idiom": "mac", "scale": f"{scale}x", "size": f"{pt}x{pt}"})
    write_json(path, {"images": images, "info": INFO})


def imageset(path, idiom, sizes, draw, opaque):
    images = []
    for scale, (w, h) in sizes:
        name = f"{w}x{h}.png"
        os.makedirs(path, exist_ok=True)
        save(draw(w, h), os.path.join(path, name), opaque)
        images.append({"filename": name, "idiom": idiom, "scale": f"{scale}x"})
    write_json(path, {"images": images, "info": INFO})


def imagestack(path, idiom, sizes):
    layers = [("Front", layer_front, False), ("Middle", layer_middle, False), ("Back", layer_back, True)]
    write_json(path, {"info": INFO, "layers": [{"filename": f"{n}.imagestacklayer"} for n, _, _ in layers]})
    for name, draw, opaque in layers:
        layer = os.path.join(path, f"{name}.imagestacklayer")
        write_json(layer, {"info": INFO})
        imageset(os.path.join(layer, "Content.imageset"), idiom, sizes, draw, opaque)


def tv_icons():
    path = os.path.join(CATALOG, "App Icon & Top Shelf Image.brandassets")
    write_json(path, {"assets": [
        {"filename": "App Icon - App Store.imagestack", "idiom": "tv", "role": "primary-app-icon", "size": "1280x768"},
        {"filename": "App Icon.imagestack", "idiom": "tv", "role": "primary-app-icon", "size": "400x240"},
        {"filename": "Top Shelf Image Wide.imageset", "idiom": "tv", "role": "top-shelf-image-wide", "size": "2320x720"},
        {"filename": "Top Shelf Image.imageset", "idiom": "tv", "role": "top-shelf-image", "size": "1920x720"},
    ], "info": INFO})
    imagestack(os.path.join(path, "App Icon - App Store.imagestack"), "tv-marketing", [(1, (1280, 768))])
    imagestack(os.path.join(path, "App Icon.imagestack"), "tv", [(1, (400, 240)), (2, (800, 480))])
    imageset(os.path.join(path, "Top Shelf Image Wide.imageset"), "tv", [(1, (2320, 720)), (2, (4640, 1440))], flat, True)
    imageset(os.path.join(path, "Top Shelf Image.imageset"), "tv", [(1, (1920, 720)), (2, (3840, 1440))], flat, True)


def logo_assets():
    """Logo: the full lockup (Arcade Foundry: Insert Coin), transparent, for
    the splash screen and About box. LaunchBackground: the navy behind it."""
    path = os.path.join(CATALOG, "Logo.imageset")
    os.makedirs(path)
    shutil.copy(os.path.join(ART, "logo-insert-coin.png"), os.path.join(path, "logo.png"))
    write_json(path, {"images": [{"filename": "logo.png", "idiom": "universal"}], "info": INFO})
    path = os.path.join(CATALOG, "LaunchBackground.colorset")
    c = {k: f"{v / 255:.4f}" for k, v in zip(("red", "green", "blue"), NAVY)}
    write_json(path, {"colors": [{"idiom": "universal", "color": {"color-space": "srgb", "components": {**c, "alpha": "1.000"}}}], "info": INFO})


# The launch screen shows its image at the image's own size in points (it is
# not scaled to the screen), so the logo is made at the width each device
# shows it (points), at its screens' scales. SplashView uses the same widths.
LAUNCH_LOGO = [("iphone", 280, (2, 3)), ("ipad", 520, (1, 2)), ("tv", 800, (1, 2))]


def launch_logo():
    """LaunchLogo: the logo sized for the iPhone, iPad and Apple TV launch screens."""
    path = os.path.join(CATALOG, "LaunchLogo.imageset")
    os.makedirs(path, exist_ok=True)
    logo = art("logo-insert-coin.png")
    images = []
    for idiom, pt, scales in LAUNCH_LOGO:
        for scale in scales:
            w = pt * scale
            h = round(logo.height * w / logo.width)
            name = f"{idiom}-{pt}@{scale}x.png"
            save(logo.resize((w, h), Image.LANCZOS), os.path.join(path, name))
            images.append({"filename": name, "idiom": idiom, "scale": f"{scale}x"})
    write_json(path, {"images": images, "info": INFO})


# The library's top bar on iPhone and iPad (LibraryView): the AF mark alone on
# an iPhone, where the bar is narrow, and the mark with INSERT COIN beside it
# on an iPad. Both are this many points tall.
NAV_HEIGHT = 28


TV_HEIGHT = 56  # Apple TV's library heading: twice the bar's logo


def nav_logos():
    """NavMark and NavLogo: transparent, at 1x, 2x and 3x; TVLogo, NavLogo
    drawn at Apple TV's size (1x and 2x) so it stays sharp."""
    for name, text, height, scales in (("NavMark", False, NAV_HEIGHT, (1, 2, 3)),
                                       ("NavLogo", True, NAV_HEIGHT, (1, 2, 3)),
                                       ("TVLogo", True, TV_HEIGHT, (1, 2))):
        path = os.path.join(CATALOG, f"{name}.imageset")
        os.makedirs(path, exist_ok=True)
        images = []
        for scale in scales:
            h = height * scale
            aw = round(AF.width * h / AF.height)
            if text:
                th = round(h * 0.34)
                tw = round(COIN.width * th / COIN.height)
                gap = round(h * 0.3)
                im = clear(aw + gap + tw, h)
                im.alpha_composite(COIN.resize((tw, th), Image.LANCZOS), (aw + gap, (h - th) // 2))
            else:
                im = clear(aw, h)
            im.alpha_composite(AF.resize((aw, h), Image.LANCZOS), (0, 0))
            file = f"{name.lower()}@{scale}x.png"
            save(im, os.path.join(path, file))
            images.append({"filename": file, "idiom": "universal", "scale": f"{scale}x"})
        write_json(path, {"images": images, "info": INFO})


def main():
    if os.path.isdir(CATALOG):
        shutil.rmtree(CATALOG)
    write_json(CATALOG, {"info": INFO})
    app_icon()
    tv_icons()
    logo_assets()
    launch_logo()
    nav_logos()
    print(f"make_icons: wrote {os.path.relpath(CATALOG, ROOT)}")


if __name__ == "__main__":
    main()
