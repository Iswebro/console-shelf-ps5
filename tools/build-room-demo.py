# SPDX-License-Identifier: GPL-3.0-only
"""Create Shelf's original, silent fallback intro. Requires Pillow and portable FFmpeg."""
import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', type=Path, required=True)
    args = parser.parse_args()
    output = ROOT / 'assets/room'
    output.mkdir(exist_ok=True)
    files = [output / ('shelf-intro.' + suffix) for suffix in ('mpg', 'mp4')]
    if any(p.exists() for p in files): raise FileExistsError('Refusing to overwrite an existing demo.')
    command = [str(args.ffmpeg.resolve(strict=True)), '-hide_banner', '-loglevel', 'error', '-nostdin', '-n',
               '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', '640x360', '-framerate', '25', '-i', 'pipe:0',
               '-an', '-c:v', 'mpeg1video', '-threads', '2', '-q:v', '4', '-bf', '0', '-g', '12', '-f', 'mpeg', str(files[0]),
               '-an', '-c:v', 'libx264', '-threads', '2', '-crf', '22', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(files[1])]
    logo = Image.open(ROOT / 'assets/shelf-icon.png').convert('RGBA').resize((138, 138), Image.Resampling.LANCZOS)
    font = ImageFont.truetype(str(ROOT / 'native/assets/fonts/Poppins-SemiBold.ttf'), 24)
    process = subprocess.Popen(command, stdin=subprocess.PIPE)
    try:
        for frame in range(125):
            time = frame / 25
            image = Image.new('RGB', (640, 360), (7, 19, 18)); draw = ImageDraw.Draw(image)
            phase = .35 + .65 * min(1, time / .8); glow = .6 + .4 * math.sin(time * 1.6)
            for radius in range(134, 92, -2):
                strength = (134 - radius) / 42 * phase * glow
                draw.ellipse((320 - radius, 155 - radius, 320 + radius, 155 + radius), outline=(8, int(22 + 10 * strength), int(21 + 8 * strength)), width=2)
            icon = logo.copy(); icon.putalpha(icon.getchannel('A').point(lambda a: int(a * phase)))
            image.paste(icon, (251, int(91 + (1 - phase) * 10)), icon)
            draw.text((320, 259), 'isShelf', fill=tuple(int(v * phase) for v in (209, 238, 226)), font=font, anchor='mm')
            draw.rounded_rectangle((269, 292, 371, 294), radius=1, fill=(23, 42, 36))
            draw.rounded_rectangle((269, 292, 269 + max(1, int(102 * min(1, time / 3))), 294), radius=1, fill=(105, 164, 139))
            process.stdin.write(image.tobytes())
        process.stdin.close()
        if process.wait(timeout=60) != 0: raise RuntimeError('FFmpeg failed to encode the original demo.')
    finally:
        if process.poll() is None: process.kill(); process.wait()
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = {'schema': 1, 'title': 'Original isShelf intro demo', 'creator': 'isShelf project',
                'license': 'GPL-3.0-only', 'officialConsoleIntro': False, 'silent': True,
                'width': 640, 'height': 360, 'fps': 25, 'seconds': 5,
                'logo': {'path': 'assets/shelf-icon.png', 'sha256': digest(ROOT / 'assets/shelf-icon.png')},
                'assets': [{'local': p.relative_to(ROOT).as_posix(), 'bytes': p.stat().st_size, 'sha256': digest(p)} for p in files]}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__': main()
