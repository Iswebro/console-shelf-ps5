# SPDX-License-Identifier: GPL-3.0-only
"""Render original Shelf console intros using this project's unchanged hardware art."""
import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT, FPS, FRAMES = 640, 336, 25, 110


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    systems = json.loads(subprocess.check_output(['node', '-e', 'console.log(JSON.stringify(require("./systems-data.js")))'], cwd=ROOT, encoding='utf-8'))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=ROOT / 'assets/room/intros')
    parser.add_argument('--console', action='append', choices=[s['id'] for s in systems], help='Only these systems; omit for the complete catalogue')
    args = parser.parse_args()
    output = args.out.resolve(); output.mkdir(parents=True, exist_ok=False)
    executable = args.ffmpeg.resolve(strict=True)
    fonts = ROOT / 'native/assets/fonts'
    regular = lambda size: ImageFont.truetype(str(fonts / 'Poppins-Regular.ttf'), size)
    bold = lambda size: ImageFont.truetype(str(fonts / 'Poppins-SemiBold.ttf'), size)
    mark = Image.open(ROOT / 'assets/shelf-icon.png').convert('RGBA').resize((28, 28), Image.Resampling.LANCZOS)
    entries = []
    for system in systems:
        if args.console and system['id'] not in args.console: continue
        ident = system['id']; accent = tuple(int(system['color'][i:i + 2], 16) for i in (1, 3, 5))
        original = ROOT / system['image']; artwork = Image.open(original).convert('RGBA')
        bounds = artwork.getchannel('A').point(lambda alpha: 255 if alpha > 8 else 0).getbbox()
        artwork = artwork.crop(bounds)
        fit = min(288 / artwork.width, 218 / artwork.height)
        figure = artwork.resize((round(artwork.width * fit), round(artwork.height * fit)), Image.Resampling.LANCZOS)
        background = Image.new('RGBA', (WIDTH, HEIGHT), (7, 15, 17, 255))
        haze = Image.new('RGBA', background.size); haze_draw = ImageDraw.Draw(haze)
        haze_draw.ellipse((289, -40, 724, 354), fill=(*accent, 70))
        background = Image.alpha_composite(background, haze.filter(ImageFilter.GaussianBlur(65)))
        draw = ImageDraw.Draw(background)
        draw.line((38, 73, 252, 73), fill=(*accent, 90), width=1)
        draw.line((38, 285, 600, 285), fill=(160, 195, 187, 35), width=1)
        name = system['name']; title_font = bold(32); lines = []
        for size in (32, 29, 26, 23, 20):
            title_font = bold(size); lines = []; line = ''
            for word in name.split():
                candidate = (line + ' ' + word).strip()
                if line and draw.textlength(candidate, font=title_font) > 236: lines.append(line); line = word
                else: line = candidate
            if line: lines.append(line)
            if len(lines) <= 3: break
        step = round(title_font.size * 1.22)

        def frame_at(time):
            image = background.copy(); layer = Image.new('RGBA', image.size); paint = ImageDraw.Draw(layer)
            ease = 1 - (1 - min(1, time / .85)) ** 3
            scale = .97 + .03 * ease
            device = figure.resize((round(figure.width * scale), round(figure.height * scale)), Image.Resampling.LANCZOS)
            device.putalpha(device.getchannel('A').point(lambda alpha: round(alpha * (.86 + .14 * ease))))
            x, y = round(467 - device.width / 2), round(157 - device.height / 2 + (1 - ease) * 5)
            shadow = Image.new('RGBA', image.size); shade = ImageDraw.Draw(shadow)
            shade.ellipse((373, 255, 560, 270), fill=(0, 0, 0, 105))
            image = Image.alpha_composite(image, shadow.filter(ImageFilter.GaussianBlur(7)))
            image.alpha_composite(device, (x, y))
            paint.text((38, 39), system['family'].upper(), font=bold(12), fill=(180, 201, 197, 255))
            top = max(102, 162 - len(lines) * step / 2)
            for index, line in enumerate(lines): paint.text((38, round(top + index * step)), line, font=title_font, fill=(234, 243, 238, 255))
            if system['year']: paint.text((39, 247), str(system['year']), font=regular(15), fill=(160, 186, 178, 255))
            paint.text((38, 305), 'isShelf ORIGINAL', font=bold(10), fill=(154, 182, 172, 255))
            brightness = round(95 + 45 * (.5 + .5 * math.sin(time * 2)))
            length = round(36 + 178 * min(1, time / 1.2))
            paint.line((38, 73, 38 + length, 73), fill=(brightness, min(230, brightness + 60), min(220, brightness + 42), 255), width=2)
            image = Image.alpha_composite(image, layer)
            image.alpha_composite(mark, (584, 30))
            return image.convert('RGB')

        poster = output / (ident + '.png'); frame_at(1.6).save(poster, optimize=True)
        files = [output / (ident + '.' + ext) for ext in ('mpg', 'mp4')]
        command = [str(executable), '-hide_banner', '-loglevel', 'error', '-nostdin', '-n',
                   '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', f'{WIDTH}x{HEIGHT}', '-framerate', str(FPS), '-i', 'pipe:0',
                   '-an', '-c:v', 'mpeg1video', '-threads', '2', '-q:v', '3', '-bf', '0', '-g', '12', '-f', 'mpeg', str(files[0]),
                   '-an', '-c:v', 'libx264', '-threads', '2', '-crf', '20', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(files[1])]
        process = subprocess.Popen(command, stdin=subprocess.PIPE)
        try:
            for frame in range(FRAMES): process.stdin.write(frame_at(frame / FPS).tobytes())
            process.stdin.close()
            if process.wait(timeout=60) != 0: raise RuntimeError('Intro encoder failed: ' + ident)
        finally:
            if process.poll() is None: process.kill(); process.wait()
        entries.append({'id': ident, 'name': name, 'artwork': {'path': system['image'], 'sha256': digest(original)},
                        'assets': [{'file': p.name, 'bytes': p.stat().st_size, 'sha256': digest(p)} for p in [poster, *files]]})
        print('Prepared original intro:', ident, flush=True)
    manifest = {'schema': 1, 'creator': 'isShelf project', 'license': 'GPL-3.0-only',
                'officialConsoleIntros': False, 'silent': True, 'width': WIDTH, 'height': HEIGHT,
                'fps': FPS, 'frames': FRAMES, 'seconds': FRAMES / FPS, 'posterSeconds': 1.6,
                'generator': 'tools/build-console-intros.py', 'encoderSha256': digest(executable), 'consoles': entries}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'directory': str(output), 'systems': len(entries), 'mediaBytes': sum(a['bytes'] for e in entries for a in e['assets'])}))


if __name__ == '__main__': main()
