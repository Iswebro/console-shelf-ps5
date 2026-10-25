// SPDX-License-Identifier: GPL-3.0-only
// Compile the native model on the host. No PS5 build, deployment or global settings.
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const build = path.join(root, '.cache/native-host');
fs.mkdirSync(build, { recursive: true });
const source = path.join(root, 'native/src/model.cpp');
const tests = path.join(root, 'native/tests/model_tests.cpp');
const settings = path.join(root, 'native/src/settings.cpp');
const settingsTests = path.join(root, 'native/tests/settings_tests.cpp');
const ui = path.join(root, 'native/src/ui.cpp');
const uiTests = path.join(root, 'native/tests/ui_tests.cpp');
const includes = path.join(root, 'native/include');
const output = path.join(build, process.platform === 'win32' ? 'model-tests.exe' : 'model-tests');
function run(command, args, options = {}) {
  const result = spawnSync(command, args, { cwd: build, stdio: 'inherit', windowsHide: true, ...options });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`Native check command failed with exit code ${result.status}`);
}
if (process.platform === 'win32') {
  const vswhere = path.join(process.env['ProgramFiles(x86)'] || '', 'Microsoft Visual Studio/Installer/vswhere.exe');
  const result = spawnSync(vswhere, ['-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], { encoding: 'utf8', windowsHide: true });
  const installation = result.stdout?.trim();
  if (result.error || result.status !== 0 || !installation) throw new Error('An existing Visual Studio C++ toolset is required for the Windows host check.');
  const developerCommand = path.join(installation, 'Common7/Tools/VsDevCmd.bat');
  const quoted = value => {
    if (/["%\r\n]/.test(value)) throw new Error('Unsupported character in compiler input path.');
    return `"${value}"`;
  };
  const command = `call ${quoted(developerCommand)} -no_logo -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++17 /EHsc /utf-8 /D_CRT_SECURE_NO_WARNINGS /W4 /WX /I${quoted(includes)} ${quoted(source)} ${quoted(settings)} ${quoted(ui)} ${quoted(tests)} ${quoted(settingsTests)} ${quoted(uiTests)} /Fe:model-tests.exe`;
  run(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', `"${command}"`], { windowsVerbatimArguments: true });
} else {
  run('c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', includes, source, settings, ui, tests, settingsTests, uiTests, '-o', output]);
}
run(output, []);
