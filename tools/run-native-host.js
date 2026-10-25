// SPDX-License-Identifier: GPL-3.0-only
// User-invoked interactive PC build. Uses only project-owned development data.
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const root=path.resolve(__dirname,'..');
const built=spawnSync(process.execPath,[path.join(root,'tools/build-native-host.js')],{cwd:root,stdio:'inherit',windowsHide:true});
if(built.error)throw built.error;
if(built.status!==0)process.exit(built.status||1);
const data=path.join(root,'.cache/native-host/user-data');
fs.mkdirSync(data,{recursive:true});
const executable=path.join(root,'.cache/native-host',process.platform==='win32'?'shelf-host.exe':'shelf-host');
const run=spawnSync(executable,['--asset-root',root,'--data-dir',data],{cwd:root,stdio:'inherit',windowsHide:true});
if(run.error)throw run.error;
process.exitCode=run.status||0;
