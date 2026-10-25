# SPDX-License-Identifier: GPL-3.0-only
"""Prepare personal TV clips locally. Never contacts a PS5 or modifies the input."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    ids = re.findall(r'^\s*\{"([a-z0-9_-]+)",', (ROOT / 'native/include/shelf/catalog.generated.hpp').read_text(encoding='utf-8'), re.M)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--console', choices=ids, required=True)
    parser.add_argument('--ffmpeg', type=Path)
    parser.add_argument('--start', type=float, default=0)
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--out', type=Path, help='New output directory; existing directories are refused')
    args = parser.parse_args()
    if not 0 <= args.start <= 3600 or not 0 < args.seconds <= 60:
        parser.error('Start must be 0–3600 seconds and length must be 0–60 seconds.')
    source = args.input.resolve(strict=True)
    if not source.is_file(): parser.error('Input must be a local video file.')
    cached = ROOT / '.cache/tools/ffmpeg-9.0.2/ffmpeg.exe'
    executable = args.ffmpeg or (cached if cached.is_file() else shutil.which('ffmpeg'))
    if not executable: parser.error('Provide --ffmpeg with your portable FFmpeg executable.')
    executable = Path(executable).resolve(strict=True)
    ffprobe = executable.with_name('ffprobe' + executable.suffix)
    if not ffprobe.is_file(): parser.error('ffprobe must be beside ffmpeg.')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    output = (args.out or ROOT / '.cache/room-intros' / (stamp + '-' + args.console)).resolve()
    # Refuse an existing target; the original clip is never an output.
    output.mkdir(parents=True, exist_ok=False)
    digest = lambda p: hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
    original = digest(source)
    common = [str(executable), '-hide_banner', '-loglevel', 'error', '-nostdin', '-n',
              '-protocol_whitelist', 'file,pipe', '-ss', str(args.start), '-i', str(source),
              '-t', str(args.seconds), '-map', '0:v:0', '-an', '-sn', '-dn',
              '-vf', 'scale=640:360:force_original_aspect_ratio=decrease:force_divisible_by=2,pad=640:360:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=25',
              '-pix_fmt', 'yuv420p', '-threads', '2', '-map_metadata', '-1']
    records = []
    for suffix, codec in [('mpg', ['-c:v', 'mpeg1video', '-q:v', '3', '-bf', '0', '-g', '12', '-f', 'mpeg']),
                          ('mp4', ['-c:v', 'libx264', '-crf', '20', '-preset', 'medium', '-movflags', '+faststart'])]:
        target = output / (args.console + '.' + suffix)
        subprocess.run(common + codec + [str(target)], check=True, timeout=240)
        metadata = json.loads(subprocess.check_output([str(ffprobe), '-v', 'error', '-protocol_whitelist', 'file,pipe',
            '-show_streams', '-show_format', '-of', 'json', str(target)], timeout=30))
        video = [s for s in metadata['streams'] if s['codec_type'] == 'video']
        expected = 'mpeg1video' if suffix == 'mpg' else 'h264'
        if len(video) != 1 or video[0]['codec_name'] != expected or video[0]['width'] != 640 or video[0]['height'] != 360 or not 0 < target.stat().st_size <= 32 * 1024 * 1024:
            raise RuntimeError('Converted clip failed its format/size checks; no transfer was attempted.')
        records.append({'file': target.name, 'bytes': target.stat().st_size, 'sha256': digest(target), 'codec': expected})
    if digest(source) != original: raise RuntimeError('The source changed while converting; discard these outputs.')
    result = {'console': args.console, 'inputSha256': original, 'sourceUnchanged': True, 'silent': True,
              'startSeconds': args.start, 'lengthSeconds': args.seconds, 'files': records,
              'ps5Destination': '/data/console-shelf/intros/' + args.console + '.mpg',
              'transfer': 'Not attempted. Use the MPEG on PS5 while Shelf is closed; MP4 is for the PC preview.'}
    (output / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'prepared': str(output), **result}, indent=2))


if __name__ == '__main__':
    main()
