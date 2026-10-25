// SPDX-License-Identifier: GPL-3.0-only
// Deterministic carousel geometry; no rendering or device dependencies.
const ShelfOrbit = (() => {
  const duration = 420;
  const wrap = (value, count) => count ? ((value % count) + count) % count : 0;
  const distance = (value, count) => count ? wrap(value + count / 2, count) - count / 2 : 0;
  function pose(offset, width) {
    const depth = Math.abs(offset);
    const angle = Math.min(depth, 4) * Math.PI / 7.5;
    return {
      x: Math.sign(offset) * width * (width < 600 ? .88 : .53) * Math.sin(angle),
      y: -72 * (1 - Math.cos(angle)), z: 72 - 480 * (1 - Math.cos(angle)),
      scale: Math.max(.30, 1.18 - depth * .28), yaw: -Math.sign(offset) * Math.min(depth * 9, 25),
      opacity: Math.max(0, 1 - depth * .19) * Math.min(1, Math.max(0, 4 - depth)), visible: depth < 4
    };
  }
  class Motion {
    constructor(position = 0) { this.position = this.target = this.from = position; this.start = 0; this.active = false; }
    select(index, count, time, immediate = false) {
      this.update(time);
      const target = this.position + distance(index - this.position, count);
      immediate = immediate || Math.abs(target - this.position) > 3;
      if (Math.abs(target - this.target) < .0001 && !immediate && (this.active || Math.abs(target - this.position) < .0001)) return;
      this.from = this.position; this.target = target; this.start = time;
      this.active = !immediate && Math.abs(target - this.position) > .0001;
      if (!this.active) this.position = target;
    }
    update(time) {
      if (!this.active) return false;
      const progress = Math.max(0, Math.min(1, (time - this.start) / duration));
      this.position = this.from + (this.target - this.from) * (1 - Math.pow(1 - progress, 4));
      if (progress === 1) { this.position = this.target; this.active = false; }
      return this.active;
    }
  }
  return { wrap, distance, pose, Motion, duration };
})();
if (typeof module !== 'undefined') module.exports = ShelfOrbit;
