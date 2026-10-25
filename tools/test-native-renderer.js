// SPDX-License-Identifier: GPL-3.0-only
// Exercises the compiled app through its SDL event loop, without a visible window.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname,'..');
for(const file of require('../native/third_party/SOURCE.json')){
  const data=fs.readFileSync(path.join(root,file.local));
  assert.equal(data.length,file.bytes,file.local+' bytes');
  assert.equal(crypto.createHash('sha256').update(data).digest('hex'),file.sha256,file.local+' digest');
}
const directory=path.join(root,'.cache/native-render-tests',String(Date.now()));
const data=path.join(directory,'data'),captures=path.join(directory,'captures');
fs.mkdirSync(data,{recursive:true});fs.mkdirSync(captures,{recursive:true});
const executable=path.join(root,'.cache/native-host',process.platform==='win32'?'shelf-host.exe':'shelf-host');
function run(script,ownedData=data,expected=0){
  const result=spawnSync(executable,['--asset-root',root,'--data-dir',ownedData,'--capture-dir',captures,'--headless','--script',script],{cwd:root,encoding:'utf8',windowsHide:true,timeout:60000});
  if(expected===0){if(result.stdout)process.stdout.write(result.stdout);if(result.stderr)process.stderr.write(result.stderr);}
  assert.equal(result.error,undefined,'native host process should finish');assert.equal(result.status,expected,'native host process exit status');
  return result;
}
run('shot:home.png,right,confirm,shot:details.png,back,settings,shot:appearance.png,confirm,tab,shot:browse.png,tab,down,confirm,text:Nintendo,settings,shot:consoles.png,down,down,confirm,report:changed.json,back,shot:swipe.png');
const changed=JSON.parse(fs.readFileSync(path.join(captures,'changed.json')));
assert.equal(changed.page,3);assert.equal(changed.tab,2);assert.equal(changed.layout,'swipe');
assert.equal(changed.shown,36);assert.equal(changed.hidden.length,16);assert.ok(changed.hidden.includes('n64'));assert.ok(!changed.hidden.includes('ps2'));
assert.equal(changed.launchConnected,false,'host build must not claim PS5 launching');
run('report:reopened.json');
const reopened=JSON.parse(fs.readFileSync(path.join(captures,'reopened.json')));
assert.equal(reopened.layout,'swipe');assert.deepEqual(reopened.hidden,changed.hidden);assert.equal(reopened.shown,36);
const allData=path.join(directory,'all-systems-data');fs.mkdirSync(allData);
const catalogue=require('../systems-data.js');
const walking=['search','confirm','right','confirm','report:keyboard-before.json','favorite','report:keyboard-cleared.json','back','down'];
for(let i=0;i<catalogue.length;i++){if(i)walking.push('right');if(catalogue[i].id==='genesis')walking.push('confirm','shot:megadrive.png','back');}
walking.push('report:all-systems.json');run(walking.join(','),allData);
const keyboardBefore=JSON.parse(fs.readFileSync(path.join(captures,'keyboard-before.json'))),keyboardCleared=JSON.parse(fs.readFileSync(path.join(captures,'keyboard-cleared.json')));
assert.equal(keyboardBefore.page,4);assert.equal(keyboardBefore.queryLength,2);assert.equal(keyboardCleared.queryLength,0);assert.equal(keyboardCleared.shown,52);
const walked=JSON.parse(fs.readFileSync(path.join(captures,'all-systems.json')));assert.equal(walked.shown,52);assert.equal(walked.selected,catalogue.at(-1).id);
assert.match(run('unknown-action',allData,1).stderr,/Unknown scripted action/);
const images=['home.png','details.png','appearance.png','browse.png','consoles.png','swipe.png','megadrive.png'];
const output=[];
for(const name of images){const filename=path.join(captures,name),bytes=fs.readFileSync(filename);assert.equal(bytes.subarray(0,8).toString('hex'),'89504e470d0a1a0a');assert.equal(bytes.readUInt32BE(16),1920);assert.equal(bytes.readUInt32BE(20),1080);assert.ok(bytes.length>40000,name+' should contain rendered UI');output.push({name,bytes:bytes.length,sha256:crypto.createHash('sha256').update(bytes).digest('hex')});}
fs.writeFileSync(path.join(root,'.cache/native-render-tests/latest.json'),JSON.stringify({directory,captures,processClosed:true,images:output},null,2)+'\n');
console.log('PASS: compiled native renderer, SDL controller-button event mapping, on-screen typing, settings, scoped visibility, persisted reopen, all 52 console images and seven 1080p captures. Physical DualSense and PS5 tests remain pending.');
