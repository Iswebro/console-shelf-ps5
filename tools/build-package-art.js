// SPDX-License-Identifier: GPL-3.0-only
const fs = require('node:fs'), path = require('node:path'), { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..'), build = path.join(root, '.cache/package-tools');
fs.mkdirSync(build, { recursive: true });
const source = path.join(root, 'tools/prepare-package-art.cpp'), include = path.join(root, 'native/third_party');
function run(command, args, options = {}) {
  const result = spawnSync(command, args, { cwd: build, windowsHide: true, stdio: 'inherit', ...options });
  if (result.error) throw result.error; if (result.status !== 0) throw Error('Package artwork tool failed');
}
if (process.platform === 'win32') {
  const vswhere = path.join(process.env['ProgramFiles(x86)'] || '', 'Microsoft Visual Studio/Installer/vswhere.exe');
  const found = spawnSync(vswhere, ['-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], { encoding: 'utf8', windowsHide: true });
  if (found.status !== 0 || !found.stdout.trim()) throw Error('Existing Visual Studio C++ tools required');
  const q = value => { if (/["%\r\n]/.test(value)) throw Error('Unsupported compiler path'); return '"' + value + '"'; };
  const setup = path.join(found.stdout.trim(), 'Common7/Tools/VsDevCmd.bat');
  const command = `call ${q(setup)} -no_logo -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++17 /O2 /EHsc /utf-8 /D_CRT_SECURE_NO_WARNINGS /W4 /WX /I${q(include)} ${q(source)} /Fe:prepare-package-art.exe`;
  run(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', `"${command}"`], { windowsVerbatimArguments: true });
} else run('c++', ['-std=c++17', '-O2', '-I', include, source, '-o', 'prepare-package-art']);
