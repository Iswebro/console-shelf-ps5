// SPDX-License-Identifier: GPL-3.0-only
// Loopback-only protocol fixture. Never connects to a console or changes its files.
const net = require('node:net');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const owned = path.join(root, '.cache/library-ftp-tests', String(Date.now()));
fs.mkdirSync(owned, { recursive: true });
const executable = path.join(root, '.cache/native-host', process.platform === 'win32' ? 'shelf-host.exe' : 'shelf-host');
const files = new Map();
function file(name, bytes) {
  files.set(name, Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes));
  let parent = path.posix.dirname(name);
  while (parent !== '/') { if (!files.has(parent)) files.set(parent, null); parent = path.posix.dirname(parent); }
}
file('/user/app/PPSA99169/eboot.bin', 'PROGRAM - must never be downloaded');
file('/user/app/PPSA99169/sce_sys/param.json', JSON.stringify({ titleId: 'PPSA99169', titleName: 'RetroArch' }));
file('/data/homebrew/PPSA99169/cores/mupen64plus_next_libretro.so', 'CORE - must never be downloaded');
file('/data/homebrew/PPSA99169/content/n64/Example game.z64', Buffer.from('8037124000000000', 'hex'));
file('/data/homebrew/PPSA99169/retroarch.cfg', 'PRIVATE - must never be read');
file('/user/app/PPSA12345/sce_sys/param.json', JSON.stringify({ titleId: 'PPSA12345', localizedParameters: { 'en-US': { titleName: 'Registered example' } } }));
file('/user/app/PPSA12345/mount.lnk', 'a mount marker, not a launch guarantee');
function list(folder) {
  const rows = [];
  for (const [name, data] of files) if (path.posix.dirname(name) === folder) rows.push(`${data === null ? 'd' : '-'}rwxr-xr-x 1 root wheel ${data === null ? 0 : data.length} Oct 25 12:00 ${path.posix.basename(name)}\r\n`);
  return rows.join('');
}
function run(args) {
  return new Promise((resolve, reject) => {
    const child = spawn(executable, ['--asset-root', root, '--data-dir', owned, '--headless', ...args], { cwd: root, windowsHide: true });
    let stdout = '', stderr = '';
    const timeout = setTimeout(() => { child.kill(); reject(Error('FTP test exceeded 30 seconds')); }, 30000);
    child.stdout.on('data', data => stdout += data); child.stderr.on('data', data => stderr += data);
    child.on('error', reject); child.on('close', code => { clearTimeout(timeout); resolve({ code, stdout, stderr }); });
  });
}
async function scenario(pasv, malformed = false, background = false) {
  const commands = [], sockets = new Set(), passiveServers = new Set();
  let failure;
  const server = net.createServer(socket => {
    sockets.add(socket); socket.on('error', () => {}); socket.on('close', () => sockets.delete(socket));
    socket.write('220-Read-only protocol fixture\r\n220 Ready\r\n');
    let input = '', dataConnection, dataReady, rest = 0;
    socket.on('data', data => {
      input += data.toString();
      while (input.includes('\n')) {
        const split = input.indexOf('\n'), line = input.slice(0, split).replace(/\r$/, ''); input = input.slice(split + 1);
        const space = line.indexOf(' '), verb = space < 0 ? line : line.slice(0, space), arg = space < 0 ? '' : line.slice(space + 1);
        commands.push({ verb, arg });
        if (!['USER', 'PASS', 'TYPE', 'EPSV', 'PASV', 'LIST', 'REST', 'RETR'].includes(verb)) { failure = Error('Non-read command: ' + verb); socket.destroy(); continue; }
        if (verb === 'USER') socket.write('331 Password\r\n');
        else if (verb === 'PASS') socket.write('230 Anonymous\r\n');
        else if (verb === 'TYPE') socket.write('200 Binary\r\n');
        else if (verb === 'EPSV' && pasv) socket.write('502 Use PASV\r\n');
        else if (verb === 'EPSV' || verb === 'PASV') {
          const passive = net.createServer(peer => { dataConnection = peer; sockets.add(peer); peer.on('error', () => {}); peer.on('close', () => sockets.delete(peer)); dataReady(); });
          passiveServers.add(passive); dataReady = () => {};
          passive.listen(0, '127.0.0.1', () => {
            const port = passive.address().port;
            socket.write(verb === 'EPSV' ? `229 Entering extended passive (|||${port}|)\r\n` : `227 Entering passive (203,0,113,99,${port >> 8},${port & 255})\r\n`);
          });
        } else if (verb === 'REST') { rest = Number(arg); socket.write('350 Offset accepted\r\n'); }
        else if (verb === 'LIST' || verb === 'RETR') {
          if (!files.has(arg) || (verb === 'LIST' ? files.get(arg) !== null : files.get(arg) === null)) { socket.write('550 No such file or directory\r\n'); dataConnection?.end(); continue; }
          if (verb === 'RETR' && !arg.endsWith('/sce_sys/param.json') && !arg.endsWith('.z64')) { failure = Error('Unexpected file read: ' + arg); socket.destroy(); continue; }
          socket.write('150 Data follows\r\n');
          const send = () => {
            const payload = verb === 'LIST' ? Buffer.from(list(arg) + (malformed && arg === '/data/PCSX2/games' ? 'truncated' : '')) : files.get(arg).subarray(rest);
            dataConnection.end(payload, () => { if (!socket.destroyed) socket.write('226 Complete\r\n'); });
          };
          if (dataConnection) send(); else dataReady = send;
        }
      }
    });
  });
  files.set('/data/PCSX2/games', null);
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  try {
    const result = await run(['--library-ftp', '127.0.0.1', '--library-port', String(server.address().port),
      ...(background ? ['--background-scan', '--script', 'shot:async-loading.png,wait:1000,frame:1200,report:async-library.json'] : ['--scan-only'])]);
    if (failure) throw failure;
    assert.equal(result.code, malformed ? 2 : 0, result.stderr + result.stdout);
    assert.match(result.stdout, /2 games, 1 emulator\/core routes/);
    assert.match(result.stdout, /n64: 1 games, 1 routes/);
    assert.match(result.stdout, /ps5: 1 games, 0 routes/);
    if (!background) assert.match(result.stdout, malformed ? /first failure listing data \(226\)/ : /first failure none \(0\)/,
      'the bounded diagnostic distinguishes a malformed transfer from successful discovery');
    if (background) assert.equal(JSON.parse(fs.readFileSync(path.join(owned,'async-library.json'))).libraryGames,2,
      'a finished worker commits its inventory to the interface');
    assert.ok(commands.some(c => c.verb === 'RETR'));
    if (pasv) assert.ok(commands.some(c => c.verb === 'PASV'), 'PASV fallback should use original peer, not advertised external IP');
  } finally {
    for (const socket of sockets) socket.destroy();
    for (const passive of passiveServers) passive.close();
    await new Promise(resolve => server.close(resolve));
  }
}
async function cancelledGreeting() {
  const peers = new Set(); let accepted = 0;
  const server = net.createServer(socket => { accepted++; peers.add(socket); socket.on('error',()=>{}); socket.on('close',()=>peers.delete(socket)); });
  await new Promise(resolve => server.listen(0,'127.0.0.1',resolve));
  try {
    const start=Date.now();
    const result=await run(['--library-ftp','127.0.0.1','--library-port',String(server.address().port),'--background-scan',
      '--script','shot:cancel-loading.png,wait:150,circle-down,frame:2000,frame:2000,frame:400']);
    assert.equal(result.code,0,result.stderr);
    assert.ok(accepted>0,'the cancellation test reached a real socket waiting for its greeting');
    assert.ok(Date.now()-start<2500,'Circle closure cancels a pending greeting before its 3-second network timeout');
  } finally { for (const peer of peers) peer.destroy(); await new Promise(resolve=>server.close(resolve)); }
}
(async () => {
  await scenario(false); await scenario(true); await scenario(false, true);
  await scenario(false,false,true); await cancelledGreeting();
  assert.equal((await run(['--library-ftp', 'example.com', '--scan-only'])).code, 1, 'no hostname resolution');
  console.log('PASS: bounded read-only FTP, EPSV/PASV pinning, metadata, malformed listing refusal, background inventory commit, responsive scan cancellation and no hostname lookup.');
})().catch(error => { console.error(error); process.exitCode = 1; });
