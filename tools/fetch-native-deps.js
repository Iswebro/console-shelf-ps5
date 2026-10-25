// SPDX-License-Identifier: GPL-3.0-only
// Official, pinned host-only SDL2 package. No system-wide installation.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const cache = path.join(root, '.cache/native-deps');
const version = '2.32.10';
const name = `SDL2-devel-${version}-VC.zip`;
const url = `https://github.com/libsdl-org/SDL/releases/download/release-${version}/${name}`;
const expected = 'af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e';
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
(async () => {
  if (process.platform !== 'win32') throw new Error('This package is the Windows host dependency. Linux builds use an existing SDL2 development installation.');
  fs.mkdirSync(cache, { recursive: true });
  const archive = path.join(cache, name);
  let data = fs.existsSync(archive) ? fs.readFileSync(archive) : null;
  if (!data || hash(data) !== expected) {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`SDL download failed: HTTP ${response.status}`);
    data = Buffer.from(await response.arrayBuffer());
    if (data.length !== 7172966 || hash(data) !== expected) throw new Error('Official SDL2 asset size/SHA-256 mismatch.');
    fs.writeFileSync(archive, data);
  }
  const list = spawnSync('tar', ['-tf', archive], { encoding: 'utf8', windowsHide: true });
  if (list.status !== 0) throw new Error('Unable to inspect the SDL2 ZIP.');
  for (const entry of list.stdout.trim().split(/\r?\n/)) {
    if (!entry.startsWith(`SDL2-${version}/`) || entry.replace(/\\/g, '/').split('/').includes('..')) throw new Error('Unexpected SDL2 archive path.');
  }
  const extracted = spawnSync('tar', ['-xf', archive, '-C', cache], { stdio: 'inherit', windowsHide: true });
  if (extracted.status !== 0) throw new Error('SDL2 ZIP integrity/extraction failed.');
  console.log(`Verified official SDL2 ${version} host package: ${expected}`);
})().catch(error => { console.error(error.message); process.exitCode = 1; });
