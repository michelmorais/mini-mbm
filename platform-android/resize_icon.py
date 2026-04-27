#!/usr/bin/env python3
# resize_icon.py — Generate Android mipmap ic_launcher.png icons from a source PNG.
#
# Usage:
#   python3 resize_icon.py <src_png> <dst_res_dir>
#
# Creates:
#   <dst_res_dir>/mipmap-mdpi/ic_launcher.png      (48×48)
#   <dst_res_dir>/mipmap-hdpi/ic_launcher.png      (72×72)
#   <dst_res_dir>/mipmap-xhdpi/ic_launcher.png     (96×96)
#   <dst_res_dir>/mipmap-xxhdpi/ic_launcher.png    (144×144)
#   <dst_res_dir>/mipmap-xxxhdpi/ic_launcher.png   (192×192)
#
# Primary backend: Pillow  (pip install pillow)
# Fallback backend: ImageMagick convert
#
# Called by CMakeLists.txt at configure time when -DGAME_ICON_PNG is set.

import os
import sys
import subprocess

DENSITIES = [
    ("mipmap-mdpi",    48),
    ("mipmap-hdpi",    72),
    ("mipmap-xhdpi",   96),
    ("mipmap-xxhdpi",  144),
    ("mipmap-xxxhdpi", 192),
]


def _resize_with_pillow(src, dst_res_dir):
    from PIL import Image
    img = Image.open(src).convert("RGBA")
    # Composite onto white to flatten transparency.
    background = Image.new("RGB", img.size, (255, 255, 255))
    background.paste(img, mask=img.split()[3])
    for folder, size in DENSITIES:
        out_dir = os.path.join(dst_res_dir, folder)
        os.makedirs(out_dir, exist_ok=True)
        resized = background.resize((size, size), Image.LANCZOS)
        resized.save(os.path.join(out_dir, "ic_launcher.png"), "PNG")
    return True


def _resize_with_imagemagick(src, dst_res_dir):
    convert = None
    for candidate in ("convert", "magick"):
        try:
            subprocess.run([candidate, "--version"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           check=True)
            convert = candidate
            break
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
    if convert is None:
        return False
    for folder, size in DENSITIES:
        out_dir = os.path.join(dst_res_dir, folder)
        os.makedirs(out_dir, exist_ok=True)
        out_path = os.path.join(out_dir, "ic_launcher.png")
        args = [
            convert, src,
            "-background", "white",
            "-flatten",
            "-resize", f"{size}x{size}!",
            out_path,
        ]
        result = subprocess.run(args, stderr=subprocess.PIPE)
        if result.returncode != 0:
            print(f"ImageMagick error: {result.stderr.decode().strip()}", file=sys.stderr)
            return False
    return True


def generate_icons(src, dst_res_dir):
    if not os.path.isfile(src):
        print(f"Error: source PNG not found: {src}", file=sys.stderr)
        return False

    # Try Pillow first.
    try:
        result = _resize_with_pillow(src, dst_res_dir)
        if result:
            print(f"Android icon generated (Pillow): {dst_res_dir}/mipmap-*/ic_launcher.png")
            return True
    except ImportError:
        pass  # Pillow not installed, try ImageMagick.

    # Fall back to ImageMagick.
    if _resize_with_imagemagick(src, dst_res_dir):
        print(f"Android icon generated (ImageMagick): {dst_res_dir}/mipmap-*/ic_launcher.png")
        return True

    print(
        "Error: neither Pillow nor ImageMagick is available.\n"
        "Install one of:\n"
        "  pip install pillow\n"
        "  sudo apt-get install imagemagick   # Ubuntu/Debian\n"
        "  brew install imagemagick           # macOS",
        file=sys.stderr,
    )
    return False


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <src_png> <dst_res_dir>", file=sys.stderr)
        sys.exit(1)
    src_png    = sys.argv[1]
    dst_res    = sys.argv[2]
    sys.exit(0 if generate_icons(src_png, dst_res) else 1)
