// SPDX-License-Identifier: GPL-3.0-only
const { spawnSync } = require('node:child_process');
const fs = require('node:fs'), path = require('node:path');
const root = path.resolve(__dirname, '..'), build = path.join(root, '.cache/movie-tests', String(Date.now()));
fs.mkdirSync(build, { recursive: true });
const sources = ['native/src/codecs.cpp', 'native/src/movie.cpp', 'native/tests/movie_tests.cpp'].map(p => path.join(root, p));
const includes = ['native/include', 'native/third_party'].map(p => path.join(root, p));
const output = path.join(build, process.platform === 'win32' ? 'movie-tests.exe' : 'movie-tests');
function run(command, args, options = {}) { const r = spawnSync(command, args, { cwd: build, encoding: 'utf8', windowsHide: true, ...options }); if (r.stdout) process.stdout.write(r.stdout); if (r.stderr) process.stderr.write(r.stderr); if (r.error || r.status !== 0) throw r.error || new Error('Movie check failed: ' + r.status); }
if (process.platform === 'win32') {
  const vswhere = path.join(process.env['ProgramFiles(x86)'] || '', 'Microsoft Visual Studio/Installer/vswhere.exe');
  const result = spawnSync(vswhere, ['-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], { encoding: 'utf8', windowsHide: true });
  if (result.status !== 0 || !result.stdout.trim()) throw new Error('Existing Visual Studio build tools required.');
  const quote = p => { if (/["%\r\n]/.test(p)) throw new Error('Unsupported tool path.'); return '"' + p + '"'; };
  const command = `call ${quote(path.join(result.stdout.trim(), 'Common7/Tools/VsDevCmd.bat'))} -no_logo -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++17 /O2 /EHsc /utf-8 /D_CRT_SECURE_NO_WARNINGS /W4 /WX ${includes.map(p => '/I' + quote(p)).join(' ')} ${sources.map(quote).join(' ')} /Fe:${quote(output)}`;
  run(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', '"' + command + '"'], { windowsVerbatimArguments: true });
} else run('c++', ['-std=c++17', '-O2', ...includes.flatMap(p => ['-I', p]), ...sources, '-o', output]);
const originals = JSON.parse(fs.readFileSync(path.join(root, 'assets/room/intros/manifest.json')));
const systems = require('../systems-data.js');
if (originals.consoles.length !== systems.length || new Set(originals.consoles.map(c => c.id)).size !== systems.length) throw new Error('Original intro catalogue is incomplete.');
const crypto = require('node:crypto');
for (const console of originals.consoles) {
  if (!systems.some(s => s.id === console.id)) throw new Error('Unknown original intro identity.');
  for (const file of console.assets) {
    if (![console.id + '.mpg', console.id + '.mp4', console.id + '.png'].includes(file.file)) throw new Error('Invalid intro filename.');
    const raw = fs.readFileSync(path.join(root, 'assets/room/intros', file.file));
    if (raw.length !== file.bytes || crypto.createHash('sha256').update(raw).digest('hex') !== file.sha256) throw new Error('Original intro digest mismatch.');
  }
}
run(output, [path.join(root, 'assets/room/shelf-intro.mpg'), path.join(build, 'invalid.mpg'), ...originals.consoles.map(c => path.join(root, 'assets/room/intros', c.id + '.mpg'))]);
