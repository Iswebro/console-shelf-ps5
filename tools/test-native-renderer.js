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
function run(script,ownedData=data,expected=0,animate=false){
  const result=spawnSync(executable,['--asset-root',root,'--data-dir',ownedData,'--capture-dir',captures,'--headless',...(animate?['--animate-script']:[]),'--script',script],{cwd:root,encoding:'utf8',windowsHide:true,timeout:60000});
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
run('search,confirm,right,confirm,report:keyboard-before.json,favorite,report:keyboard-cleared.json,back',allData);
const walking=[];
for(let i=0;i<catalogue.length;i++){if(i)walking.push(i%4===0?'down':'right');if(catalogue[i].id==='genesis')walking.push('confirm','shot:megadrive.png','back');}
walking.push('report:all-systems.json');run(walking.join(','),allData);
const keyboardBefore=JSON.parse(fs.readFileSync(path.join(captures,'keyboard-before.json'))),keyboardCleared=JSON.parse(fs.readFileSync(path.join(captures,'keyboard-cleared.json')));
assert.equal(keyboardBefore.page,4);assert.equal(keyboardBefore.queryLength,2);assert.equal(keyboardCleared.queryLength,0);assert.equal(keyboardCleared.shown,52);
const walked=JSON.parse(fs.readFileSync(path.join(captures,'all-systems.json')));assert.equal(walked.shown,52);assert.equal(walked.selected,catalogue.at(-1).id);
assert.match(run('unknown-action',allData,1).stderr,/Unknown scripted action/);
const motionData=path.join(directory,'motion-data');fs.mkdirSync(motionData);
const shelfData=path.join(directory,'shelf-data');fs.mkdirSync(shelfData);
run('layout,layout,shot:display-shelf.png,right,right,right,tab,shot:display-shelf-page2.png,report:display-shelf.json',shelfData);
const displayed=JSON.parse(fs.readFileSync(path.join(captures,'display-shelf.json')));
assert.equal(displayed.layout,'shelf');assert.equal(displayed.shown,52);assert.equal(displayed.selected,catalogue[11].id);
run('shot:display-shelf-reopened.png,report:display-shelf-reopened.json',shelfData);
assert.equal(JSON.parse(fs.readFileSync(path.join(captures,'display-shelf-reopened.json'))).layout,'shelf');
const colourData=path.join(directory,'colour-data');fs.mkdirSync(colourData);
run('layout,layout,shot:missing-monochrome.png,settings,down,down,down,shot:missing-setting.png,confirm,report:missing-colour.json,back,shot:missing-full-colour.png',colourData);
assert.equal(JSON.parse(fs.readFileSync(path.join(captures,'missing-colour.json'))).monochromeMissing,false);
run('report:missing-colour-reopened.json',colourData);
assert.equal(JSON.parse(fs.readFileSync(path.join(captures,'missing-colour-reopened.json'))).monochromeMissing,false);
run('settings,confirm,back,right,shot:orbit-start.png,frame:120,shot:orbit-middle.png,frame:400,shot:orbit-end.png',motionData,0,true);
run('right,shot:orbit-reference.png',motionData);
const digest=name=>crypto.createHash('sha256').update(fs.readFileSync(path.join(captures,name))).digest('hex');
const roomData=path.join(directory,'room-data');fs.mkdirSync(roomData);
run('layout,layout,settings,tab,tab,tab,shot:room-settings.png,confirm,back,frame:500,shot:room-intro.png,settings,confirm,back,shot:room-picture-fallback.png,settings,confirm,back,shot:room-clock.png,settings,confirm,down,confirm,favorite,text:MY PLAY ROOM,back,back,shot:room-message.png,settings,confirm,back,shot:room-favourites.png,settings,confirm,back,shot:room-screen-off.png,report:room.json',roomData,0,true);
const roomReport=JSON.parse(fs.readFileSync(path.join(captures,'room.json')));
assert.equal(roomReport.tvMode,6);assert.equal(roomReport.tvTextLength,12);assert.equal(roomReport.favoriteGames,0);
run('shot:shelf-swap-before.png,tab,shot:shelf-swap-start.png,frame:80,shot:shelf-swap-out.png,frame:160,shot:shelf-swap-in.png,frame:200,shot:shelf-swap-end.png',roomData,0,true);
run('tab,shot:shelf-swap-reference.png',roomData);
assert.notEqual(digest('shelf-swap-start.png'),digest('shelf-swap-out.png'),'outgoing hardware has a visible intermediate position');
assert.notEqual(digest('shelf-swap-out.png'),digest('shelf-swap-in.png'),'the next set of consoles enters');
assert.equal(digest('shelf-swap-end.png'),digest('shelf-swap-reference.png'),'the page settles on the correct opaque consoles');
const region=require('./png-regions');
for(const area of [[200,548,1500,4],[200,739,1500,3],[100,820,1700,70]]){
  const stationary=region(path.join(captures,'shelf-swap-before.png'),...area);
  for(const frame of ['shelf-swap-start.png','shelf-swap-out.png','shelf-swap-in.png','shelf-swap-end.png'])
    assert.deepEqual(region(path.join(captures,frame),...area),stationary,'wood, wall and floor pixels stay fixed during page changes');
}
assert.notEqual(digest('missing-monochrome.png'),digest('missing-full-colour.png'),'missing-emulator style changes the rendered artwork');
const tvData=path.join(directory,'tv-detail-data');fs.mkdirSync(tvData);
run('layout,layout,settings,tab,tab,tab,right,right,right,right,back,shot:tv-main.png,confirm,shot:tv-detail.png,down,down,confirm,shot:tv-library.png',tvData);
const screen=[1200,88,568,293];
for(const name of ['tv-detail.png','tv-library.png']) assert.ok(region(path.join(captures,name),...screen).equals(region(path.join(captures,'tv-main.png'),...screen)),'TV content and room screen plane stay intact inside console details and library');
const videoData=path.join(directory,'tv-video-data');fs.mkdirSync(videoData);
run('layout,layout,settings,tab,tab,tab,left,down,down,down,confirm,back,report:tv-video.json,shot:tv-video-start.png,frame:1000,shot:tv-video-middle.png,frame:1000,shot:tv-video-later.png',videoData,0,true);
const videoReport=JSON.parse(fs.readFileSync(path.join(captures,'tv-video.json')));
assert.equal(videoReport.layout,'shelf');assert.equal(videoReport.tvMode,7);
assert.ok(!region(path.join(captures,'tv-video-start.png'),...screen).equals(region(path.join(captures,'tv-video-middle.png'),...screen)),'the native decoder displays actual moving MPEG video');
assert.ok(region(path.join(captures,'tv-video-start.png'),100,820,1700,70).equals(region(path.join(captures,'tv-video-middle.png'),100,820,1700,70)),'video never animates the room outside the TV');
const noiseData=path.join(directory,'tv-noise-data');fs.mkdirSync(noiseData);
run('layout,layout,settings,tab,tab,tab,down,down,down,down,confirm,back,shot:tv-static-a.png,frame:200,shot:tv-static-b.png,settings,previous-tab,previous-tab,previous-tab,down,down,down,down,confirm,back,shot:tv-static-reduced-a.png,frame:200,shot:tv-static-reduced-b.png',noiseData,0,true);
assert.ok(!region(path.join(captures,'tv-static-a.png'),...screen).equals(region(path.join(captures,'tv-static-b.png'),...screen)),'optional TV grain animates inside the screen');
assert.ok(region(path.join(captures,'tv-static-reduced-a.png'),...screen).equals(region(path.join(captures,'tv-static-reduced-b.png'),...screen)),'reduced motion keeps the static screen still');
const fineGrain=region(path.join(captures,'tv-static-a.png'),1300,160,100,100);
let grainChanges=0;
for(let y=0;y<100;y++)for(let x=1;x<100;x++)if(fineGrain[(y*100+x)*4]!==fineGrain[(y*100+x-1)*4])grainChanges++;
assert.ok(grainChanges>100*99*.85,'TV static has fine pixel-level grain instead of large repeated blocks');
const introDefault=path.join(directory,'original-intro-data'), introPersonal=path.join(directory,'personal-intro-data');
for(const folder of [introDefault,introPersonal]){fs.mkdirSync(folder);fs.copyFileSync(path.join(videoData,'settings.ini'),path.join(folder,'settings.ini'));}
fs.mkdirSync(path.join(introPersonal,'intros'));fs.copyFileSync(path.join(root,'assets/room/shelf-intro.mpg'),path.join(introPersonal,'intros/ps5.mpg'));
run('shot:original-intro.png,frame:1000,shot:original-intro-moving.png',introDefault,0,true);
run('shot:personal-intro.png,frame:1000,shot:personal-intro-moving.png',introPersonal,0,true);
assert.ok(!region(path.join(captures,'original-intro.png'),...screen).equals(region(path.join(captures,'personal-intro.png'),...screen)),'a valid personal movie takes priority over the bundled console intro');
fs.writeFileSync(path.join(introPersonal,'intros/ps5.mpg'),'incomplete personal video');
run('shot:bad-personal-intro-fallback.png',introPersonal,0,true);
assert.ok(region(path.join(captures,'original-intro.png'),...screen).equals(region(path.join(captures,'bad-personal-intro-fallback.png'),...screen)),'a corrupt personal movie falls back to the matching original console intro');
const menuPatch=region(path.join(captures,'display-shelf.png'),65,131,8,42);
assert.ok([...menuPatch].every((value,index)=>index%4===3||value<50),'the room backboard quiets wall texture behind menu text');
assert.notEqual(digest('orbit-start.png'),digest('orbit-middle.png'),'native motion must draw a new intermediate frame');
assert.notEqual(digest('orbit-middle.png'),digest('orbit-end.png'),'native motion must continue to its target');
assert.equal(digest('orbit-end.png'),digest('orbit-reference.png'),'animated motion must settle at the correct console pose');
const introData=path.join(directory,'intro-data');fs.mkdirSync(introData);
run('loading,shot:loading.png,prepare,shot:prepared-cover.png,frame:340,shot:loading-motion.png,reveal,shot:reveal-start.png,frame:500,shot:reveal-middle.png,frame:500,shot:reveal-end.png',introData,0,true);
run('shot:reveal-reference.png',introData);
assert.notEqual(digest('loading.png'),digest('loading-motion.png'),'loading has visible motion');
assert.equal(digest('prepared-cover.png'),digest('loading.png'),'heavy scene preparation stays hidden behind the branded loading picture');
assert.notEqual(digest('reveal-start.png'),digest('reveal-middle.png'),'opening blends into the shelf');
assert.equal(digest('reveal-end.png'),digest('reveal-reference.png'),'opening settles on the unaltered shelf');
run('circle-down,frame:1500,shot:hold-middle.png,circle-up,shot:hold-cancelled.png,report:hold-cancelled.json,circle-down,frame:1500,frame:1500,shot:close-start.png,frame:180,shot:close-middle.png,frame:180,unknown-action',introData,0,true);
assert.equal(JSON.parse(fs.readFileSync(path.join(captures,'hold-cancelled.json'))).page,0,'releasing a long hold keeps the current page');
assert.equal(digest('hold-cancelled.png'),digest('reveal-reference.png'),'a cancelled hold restores the shelf');
assert.notEqual(digest('close-start.png'),digest('close-middle.png'),'closing draws a separate fade before exit');
run('right,right,right,up,up,shot:settings-focus.png,down,shot:layout-focus.png',introData);
run('launch-art,shot:launch-art.png',introData);
run('back,frame:250,shot:close-hint.png,report:close-hint.json,frame:2000,frame:600,shot:close-hint-fade.png,frame:400,shot:close-hint-gone.png',introData,0,true);
assert.equal(JSON.parse(fs.readFileSync(path.join(captures,'close-hint.json'))).page,0,'a Circle tap keeps the home page');
assert.notEqual(digest('close-hint.png'),digest('close-hint-fade.png'),'the compact hint fades away');
assert.equal(digest('close-hint-gone.png'),digest('reveal-reference.png'),'the timed hint leaves no modal or overlay');
const images=['home.png','details.png','appearance.png','browse.png','consoles.png','swipe.png','megadrive.png','display-shelf.png','display-shelf-page2.png','display-shelf-reopened.png','orbit-start.png','orbit-middle.png','orbit-end.png','orbit-reference.png','loading.png','prepared-cover.png','loading-motion.png','reveal-start.png','reveal-middle.png','reveal-end.png','reveal-reference.png','hold-middle.png','hold-cancelled.png','close-start.png','close-middle.png','settings-focus.png','layout-focus.png','launch-art.png','close-hint.png','close-hint-fade.png','close-hint-gone.png'];
images.push('missing-monochrome.png','missing-setting.png','missing-full-colour.png');
images.push('room-settings.png','room-intro.png','room-picture-fallback.png','room-clock.png','room-message.png','room-favourites.png','room-screen-off.png','shelf-swap-before.png','shelf-swap-start.png','shelf-swap-out.png','shelf-swap-in.png','shelf-swap-end.png','shelf-swap-reference.png');
images.push('tv-main.png','tv-detail.png','tv-library.png','tv-video-start.png','tv-video-middle.png','tv-video-later.png','tv-static-a.png','tv-static-b.png','tv-static-reduced-a.png','tv-static-reduced-b.png');
images.push('original-intro.png','original-intro-moving.png','personal-intro.png','personal-intro-moving.png','bad-personal-intro-fallback.png');
const output=[];
for(const name of images){const filename=path.join(captures,name),bytes=fs.readFileSync(filename);assert.equal(bytes.subarray(0,8).toString('hex'),'89504e470d0a1a0a');assert.equal(bytes.readUInt32BE(16),1920);assert.equal(bytes.readUInt32BE(20),1080);assert.ok(bytes.length>(name.startsWith('loading')?1000:40000),name+' should contain its rendered frame');output.push({name,bytes:bytes.length,sha256:crypto.createHash('sha256').update(bytes).digest('hex')});}
fs.writeFileSync(path.join(root,'.cache/native-render-tests/latest.json'),JSON.stringify({directory,captures,processClosed:true,images:output},null,2)+'\n');
console.log(`PASS: native renderer, room displays, stationary room pixels, console-only page swaps, three layouts, persistence, 52 icons and Circle hold/cancel/fade in ${images.length} captures. Full PS5 acceptance remains pending.`);
