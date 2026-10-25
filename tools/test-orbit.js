// SPDX-License-Identifier: GPL-3.0-only
const assert = require('node:assert/strict');
const orbit = require('../orbit.js');
const layout = require('../assets/3d-icons/layout.js');
for(const image of require('../assets/3d-icons/manifest.json')) {
  const measured=layout[image.id], [x,y,w,h]=measured.bounds;
  assert.equal(measured.sha256.toUpperCase(),image.sha256,'display bounds must belong to the current icon');
  assert.ok(x>=0&&y>=0&&w>0&&h>0&&x+w<=measured.canvas[0]&&y+h<=measured.canvas[1]);
}
assert.equal(orbit.wrap(-1, 52), 51);
assert.equal(orbit.wrap(52, 52), 0);
assert.equal(orbit.wrap(2, 0), 0);
for (const width of [358, 1344, 1760]) {
  const poses = [0, 1, 2, 3].map(offset => orbit.pose(offset, width));
  assert.equal(poses[0].x, 0);
  for (let i = 1; i < poses.length; i++) {
    assert.ok(poses[i].scale < poses[i-1].scale && poses[i].z < poses[i-1].z);
    assert.ok(poses[i].opacity < poses[i-1].opacity);
    assert.equal(orbit.pose(-i, width).x, -poses[i].x);
  }
  assert.equal(orbit.pose(4, width).visible, false);
}
const motion = new orbit.Motion();
motion.select(51, 52, 0);
motion.update(100);
assert.ok(motion.active && motion.position < 0 && motion.position > -1);
const position = motion.position;
motion.select(1, 52, 100);
assert.equal(motion.position, position);
motion.update(520);
assert.ok(!motion.active && Math.abs(motion.position - 1) < .0001);
motion.select(20, 52, 600);
assert.equal(motion.active, false, 'long list jumps should not spin through hidden consoles');
motion.select(21, 52, 700, true);
assert.equal(motion.active, false);
motion.position = 20.7; // Dragging to part of a slot must still snap back to the existing target.
motion.select(21, 52, 800);
motion.update(1220);
assert.ok(Math.abs(motion.position - 21) < .0001);
console.log('PASS: orbit symmetry/depth at desktop/mobile sizes, short wrap, smooth retarget, long jumps, reduced motion and drag snapping.');
