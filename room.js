// SPDX-License-Identifier: GPL-3.0-only
// Local preview assets only. No network requests, videos, credentials or PS5 writes.
window.ShelfRoom = class {
  constructor({ state, toast, icon }) {
    this.state = state; this.toast = toast; this.icon = icon;
    this.images = new Map(); this.signature = ''; this.rotation = 0; this.lastTurn = Date.now();
    this.favorites = new Set();
    try {
      const saved = JSON.parse(localStorage.getItem('shelf-game-favorites') || '[]');
      if (Array.isArray(saved)) this.favorites = new Set(saved.filter(key => typeof key === 'string' && key.length < 512).slice(0, 64));
    } catch { /* Start with an empty personal list. */ }
    const select = document.getElementById('introConsole');
    window.shelfSystems.forEach(system => {
      const option = document.createElement('option'); option.value = system.id; option.textContent = system.name; select.append(option);
    });
    document.getElementById('tvPicture').onchange = event => this.storeImage('picture', event.target.files[0], event.target);
    document.getElementById('introPicture').onchange = event => this.storeImage('intro:' + select.value, event.target.files[0], event.target);
    document.getElementById('removeTvPicture').onclick = () => this.removeImage('picture');
    document.getElementById('removeIntroPicture').onclick = () => this.removeImage('intro:' + select.value);
    this.database = new Promise((resolve, reject) => {
      const request = indexedDB.open('console-shelf-room-assets', 1);
      request.onupgradeneeded = () => request.result.createObjectStore('pictures');
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
      request.onblocked = () => reject(new Error('Room asset storage is busy in another tab.'));
    });
    this.database.then(database => {
      const request = database.transaction('pictures').objectStore('pictures').openCursor();
      request.onsuccess = () => {
        const cursor = request.result;
        if (cursor) { this.images.set(cursor.key, URL.createObjectURL(cursor.value)); cursor.continue(); }
        else this.render(true);
      };
    }).catch(() => {});
    this.timer = setInterval(() => this.render(), 1000);
  }
  key(system, game) { return system.id + ':' + game; }
  toggleFavorite(system, game) {
    const key = this.key(system, game);
    if (this.favorites.has(key)) this.favorites.delete(key);
    else if (this.favorites.size < 64) this.favorites.add(key);
    else { this.toast('Your 64 favourite game spaces are full.'); return; }
    try { localStorage.setItem('shelf-game-favorites', JSON.stringify([...this.favorites])); }
    catch { this.toast('This browser could not save your favourite games.'); }
    this.render(true);
  }
  async storeImage(key, file, input) {
    if (!file) return;
    try {
      if (file.size > 8 * 1024 * 1024) throw new Error('Choose a PNG smaller than 8 MB.');
      const bytes = new Uint8Array(await file.slice(0, 24).arrayBuffer()), view = new DataView(bytes.buffer);
      if (bytes.length < 24 || [...bytes.slice(0, 8)].join(',') !== '137,80,78,71,13,10,26,10' ||
          view.getUint32(12) !== 0x49484452 || !view.getUint32(16) || !view.getUint32(20) ||
          view.getUint32(16) > 2048 || view.getUint32(20) > 2048) throw new Error('Choose a PNG up to 2048 × 2048 pixels.');
      const bitmap = await createImageBitmap(file); bitmap.close();
      const database = await this.database;
      await new Promise((resolve, reject) => {
        const transaction = database.transaction('pictures', 'readwrite');
        transaction.objectStore('pictures').put(file, key);
        transaction.oncomplete = resolve; transaction.onerror = () => reject(transaction.error); transaction.onabort = () => reject(transaction.error);
      });
      const previous = this.images.get(key); if (previous) URL.revokeObjectURL(previous);
      this.images.set(key, URL.createObjectURL(file)); this.render(true);
      this.toast('Picture saved in this browser preview. The PS5 uses your own Shelf data folder.');
    } catch (error) { this.toast(error.message || 'The picture could not be saved on this device.'); }
    finally { input.value = ''; }
  }
  async removeImage(key) {
    try {
      const database = await this.database;
      await new Promise((resolve, reject) => {
        const transaction = database.transaction('pictures', 'readwrite'); transaction.objectStore('pictures').delete(key);
        transaction.oncomplete = resolve; transaction.onerror = () => reject(transaction.error);
      });
      const previous = this.images.get(key); if (previous) URL.revokeObjectURL(previous);
      this.images.delete(key); this.render(true); this.toast('Personal picture removed from this preview.');
    } catch { this.toast('The picture could not be removed.'); }
  }
  place() {
    const scene = window.shelfRoomScene, scale = Math.max(innerWidth / scene.width, innerHeight / scene.height);
    const x = (innerWidth - scene.width * scale) / 2 + scene.tv.x * scale;
    const y = (innerHeight - scene.height * scale) / 2 + scene.tv.y * scale;
    const tv = document.getElementById('roomTv');
    Object.assign(tv.style, { left: `${x}px`, top: `${y}px`, width: `${scene.tv.width * scale}px`, height: `${scene.tv.height * scale}px`, fontSize: `${Math.max(8, 15 * scale)}px` });
    // Shorten controls to the wall before the television; portrait crops use the available width.
    document.body.style.setProperty('--room-controls-right', `${Math.max(innerWidth * .0417, innerWidth - x + 28 * scale)}px`);
  }
  render(force = false) {
    const { active, system, mode, message, interval, introAll, reduced } = this.state();
    const tv = document.getElementById('roomTv');
    tv.hidden = !active || mode === 'off';
    if (tv.hidden || !system) return;
    this.place();
    const games = window.shelfSystems.flatMap(system => system.games.filter(game => this.favorites.has(this.key(system, game))).map(game => ({ system, game })));
    if (Date.now() - this.lastTurn >= interval * 1000) { this.rotation++; this.lastTurn = Date.now(); }
    const favorite = games[this.rotation % Math.max(1, games.length)];
    const date = new Date(), time = date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
    const eligible = introAll || system.status !== 'reference';
    const custom = mode === 'picture' ? this.images.get('picture') : mode === 'intro' && eligible ? this.images.get('intro:' + system.id) : null;
    const signature = JSON.stringify([mode, system.id, message, custom, mode === 'clock' ? time : '', mode === 'favorites' ? favorite?.game : '', eligible]);
    if (!force && signature === this.signature) return;
    this.signature = signature;
    const content = document.createElement('div'); content.className = 'tv-content';
    const image = (source, className = '') => { const node = document.createElement('img'); node.src = source; node.alt = ''; node.className = className; content.append(node); };
    const text = (value, tag = 'strong', className = '') => { const node = document.createElement(tag); node.textContent = value; node.className = className; content.append(node); };
    if (custom) image(custom, 'tv-personal-picture');
    else if (mode === 'clock') {
      image(this.icon(), 'tv-logo'); text(time, 'strong', 'tv-clock');
      text(date.toLocaleDateString([], { weekday: 'long', day: 'numeric', month: 'long' }), 'span'); text(message, 'small');
    } else if (mode === 'message' || mode === 'picture') {
      image(this.icon(), 'tv-logo'); text(message);
      if (mode === 'picture') text('Your own picture goes here', 'small');
    } else if (mode === 'favorites') {
      image(favorite ? favorite.system.image : this.icon(), favorite ? '' : 'tv-logo');
      text(favorite ? favorite.game : 'Your favourite games');
      text(favorite ? 'FROM YOUR FAVOURITES' : 'Mark games with a star in Shelf’s game library', 'small');
    } else if (mode === 'intro' && !eligible) { image(this.icon(), 'tv-logo'); text('Your play room'); }
    else {
      image(system.image); text(system.name);
      if (mode === 'console') text(`${system.games.length} example games · ${system.emulators.length} listed routes`, 'small');
    }
    tv.replaceChildren(content); tv.setAttribute('aria-label', content.textContent || `TV displaying ${system.name}`);
    if (!reduced && mode === 'intro') content.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 420, easing: 'ease-out' });
  }
};
