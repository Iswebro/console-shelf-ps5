// SPDX-License-Identifier: GPL-3.0-only
// Read the RGBA PNGs produced by the native renderer for stationary-room checks.
const fs = require('node:fs'), zlib = require('node:zlib'), assert = require('node:assert/strict');
module.exports = function pngRegion(filename, x, y, w, h) {
  const png = fs.readFileSync(filename); assert.equal(png.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
  const width = png.readUInt32BE(16), height = png.readUInt32BE(20);
  assert.equal(png[24], 8); assert.equal(png[25], 6); assert.equal(png[28], 0);
  assert.ok(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= width && y + h <= height);
  const blocks = [];
  for (let at = 8; at < png.length;) {
    const length = png.readUInt32BE(at), type = png.subarray(at + 4, at + 8).toString('ascii');
    assert.ok(at + length + 12 <= png.length);
    if (type === 'IDAT') blocks.push(png.subarray(at + 8, at + 8 + length));
    at += length + 12;
  }
  const stride = width * 4, raw = zlib.inflateSync(Buffer.concat(blocks), { maxOutputLength: (stride + 1) * height });
  assert.equal(raw.length, (stride + 1) * height);
  const result = Buffer.alloc(w * h * 4); let previous = Buffer.alloc(stride), row = Buffer.alloc(stride);
  const paeth = (a, b, c) => { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? b : c; };
  for (let line = 0; line < y + h; line++) {
    const start = line * (stride + 1), filter = raw[start]; assert.ok(filter <= 4);
    for (let at = 0; at < stride; at++) {
      const left = at >= 4 ? row[at - 4] : 0, above = previous[at], corner = at >= 4 ? previous[at - 4] : 0;
      const predictor = filter === 0 ? 0 : filter === 1 ? left : filter === 2 ? above : filter === 3 ? (left + above) >>> 1 : paeth(left, above, corner);
      row[at] = (raw[start + 1 + at] + predictor) & 255;
    }
    if (line >= y) row.copy(result, (line - y) * w * 4, x * 4, (x + w) * 4);
    [previous, row] = [row, previous];
  }
  return result;
};
