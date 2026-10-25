// SPDX-License-Identifier: GPL-3.0-only
// Local preview assets only. No network requests, credentials or PS5 writes.
window.ShelfRoom = class {
  constructor({ state, toast, icon }) {
    this.state = state; this.toast = toast; this.icon = icon;
    this.images = new Map(); this.videos = new Map(); this.signature = ''; this.rotation = 0; this.lastTurn = Date.now();
    this.currentVideo = null; this.staticTimer = 0;
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
    document.getElementById('introVideo').onchange = event => this.storeVideo('intro:' + select.value, event.target.files[0], event.target);
    document.getElementById('removeIntroVideo').onclick = () => this.removeImage('intro:' + select.value, 'videos');
    this.database = new Promise((resolve, reject) => {
      const request = indexedDB.open('console-shelf-room-assets', 2);
      request.onupgradeneeded = () => {
        for (const store of ['pictures', 'videos']) if (!request.result.objectStoreNames.contains(store)) request.result.createObjectStore(store);
      };
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
      request.onblocked = () => reject(new Error('Room asset storage is busy in another tab.'));
    });
    this.database.then(database => {
      for (const [store, map] of [['pictures', this.images], ['videos', this.videos]]) {
      const request = database.transaction(store).objectStore(store).openCursor();
      request.onsuccess = () => {
        const cursor = request.result;
        if (cursor) { map.set(cursor.key, URL.createObjectURL(cursor.value)); cursor.continue(); }
        else this.render(true);
      };
      }
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
  async storeVideo(key, file, input) {
    if (!file) return;
    let url = '';
    try {
      if (!file.name.toLowerCase().endsWith('.mp4') || !file.size || file.size > 32 * 1024 * 1024) throw new Error('Choose an MP4 smaller than 32 MB.');
      url = URL.createObjectURL(file);
      const video = document.createElement('video'); video.preload = 'metadata'; video.src = url;
      await new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error('Video metadata could not be read.')), 10000);
        video.onloadedmetadata = () => { clearTimeout(timeout); resolve(); };
        video.onerror = () => { clearTimeout(timeout); reject(new Error('This browser could not read that MP4.')); };
      });
      const okay = video.videoWidth > 0 && video.videoHeight > 0 && video.videoWidth <= 640 && video.videoHeight <= 368 && video.duration > 0 && video.duration <= 60;
      video.removeAttribute('src'); video.load();
      if (!okay) throw new Error('Choose a clip up to 640 × 368 and 60 seconds.');
      const database = await this.database;
      await new Promise((resolve, reject) => {
        const transaction = database.transaction('videos', 'readwrite'); transaction.objectStore('videos').put(file, key);
        transaction.oncomplete = resolve; transaction.onerror = () => reject(transaction.error);
      });
      const previous = this.videos.get(key); if (previous) URL.revokeObjectURL(previous);
      this.videos.set(key, url); url = ''; this.render(true); this.toast('Video saved for this browser preview. Use the PC tool for the PS5 copy.');
    } catch (error) { this.toast(error.message || 'The video could not be saved.'); }
    finally { if (url) URL.revokeObjectURL(url); input.value = ''; }
  }
  async removeImage(key, store = 'pictures') {
    try {
      const database = await this.database;
      await new Promise((resolve, reject) => {
        const transaction = database.transaction(store, 'readwrite'); transaction.objectStore(store).delete(key);
        transaction.oncomplete = resolve; transaction.onerror = () => reject(transaction.error);
      });
      const map = store === 'videos' ? this.videos : this.images;
      const previous = map.get(key); if (previous) URL.revokeObjectURL(previous);
      map.delete(key); this.render(true); this.toast('Personal media removed from this preview.');
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
    const { active, system, mode, noise, message, interval, introAll, reduced } = this.state();
    const tv = document.getElementById('roomTv');
    tv.hidden = !active || mode === 'off';
    if (tv.hidden || !system) {
      this.currentVideo?.pause(); this.currentVideo = null; clearInterval(this.staticTimer); this.staticTimer = 0; this.signature = ''; return;
    }
    this.place();
    const games = window.shelfSystems.flatMap(system => system.games.filter(game => this.favorites.has(this.key(system, game))).map(game => ({ system, game })));
    if (Date.now() - this.lastTurn >= interval * 1000) { this.rotation++; this.lastTurn = Date.now(); }
    const favorite = games[this.rotation % Math.max(1, games.length)];
    const date = new Date(), time = date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
    const eligible = introAll || system.status !== 'reference';
    const noEmulator = system.status === 'reference', noGames = !system.games.length;
    const staticOn = noise === 'emulator' && noEmulator || noise === 'games' && noGames || noise === 'either' && (noEmulator || noGames);
    const clip = this.videos.get('intro:' + system.id);
    const custom = mode === 'picture' ? this.images.get('picture') : mode === 'intro' && eligible ?
      this.images.get('intro:' + system.id) || `assets/room/intros/${system.id}.png` : null;
    const signature = JSON.stringify([mode, system.id, message, custom, clip, staticOn, reduced, mode === 'clock' ? time : '', mode === 'favorites' ? favorite?.game : '', eligible]);
    if (signature === this.signature) return;
    this.currentVideo?.pause(); this.currentVideo = null; clearInterval(this.staticTimer); this.staticTimer = 0;
    this.signature = signature;
    const content = document.createElement('div'); content.className = 'tv-content';
    const image = (source, className = '') => { const node = document.createElement('img'); node.src = source; node.alt = ''; node.className = className; content.append(node); };
    const text = (value, tag = 'strong', className = '') => { const node = document.createElement(tag); node.textContent = value; node.className = className; content.append(node); };
    if (staticOn) {
      const canvas = document.createElement('canvas'), bounds = tv.getBoundingClientRect();
      canvas.width = Math.max(1, Math.min(960, Math.ceil(bounds.width * devicePixelRatio)));
      canvas.height = Math.max(1, Math.min(540, Math.ceil(bounds.height * devicePixelRatio))); canvas.className = 'tv-static';
      const context = canvas.getContext('2d'), pixels = context.createImageData(canvas.width, canvas.height); let phase = 0;
      const paint = () => {
        let random = phase + 4711;
        for (let row = 0; row < canvas.height; row++) {
          const shade = ((row + phase * 2) % 216 < 24 ? 5 : 0) + (row % 2 ? 3 : 0);
          for (let col = 0; col < canvas.width; col++) {
            random ^= random << 13; random ^= random >>> 17; random ^= random << 5;
            const level = 62 + Math.floor(((random & 255) + ((random >>> 8) & 255)) * 82 / 510) - shade;
            const i = (row * canvas.width + col) * 4;
            pixels.data[i] = pixels.data[i + 1] = pixels.data[i + 2] = level; pixels.data[i + 3] = 255;
          }
        }
        phase++;
        context.putImageData(pixels, 0, 0);
      };
      paint(); if (!reduced) this.staticTimer = setInterval(paint, 50);
      content.style.padding = '0'; content.append(canvas);
    } else if (mode === 'video' && eligible) {
      const video = document.createElement('video'); video.muted = true; video.playsInline = true; video.preload = 'auto';
      video.src = clip || `assets/room/intros/${system.id}.mp4`; video.loop = false;
      content.append(video); this.currentVideo = video;
      let fallback = 0;
      video.onerror = () => {
        if (++fallback > 2) return;
        video.src = fallback === 1 ? `assets/room/intros/${system.id}.mp4` : 'assets/room/shelf-intro.mp4';
        if (fallback === 2) text('Shelf demo · add your own console intro', 'small', 'tv-video-caption');
        if (!reduced) video.play().catch(() => {});
      };
      if (reduced) video.addEventListener('loadeddata', () => video.pause(), { once: true });
      else video.play().catch(() => { text('Video unavailable in this browser', 'small', 'tv-video-caption'); });
    } else if (custom) image(custom, 'tv-personal-picture');
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
      else if (mode === 'intro') text('Add your own console intro picture', 'small');
    }
    tv.replaceChildren(content); tv.setAttribute('aria-label', content.textContent || `TV displaying ${system.name}`);
    if (!reduced && mode === 'intro') content.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 420, easing: 'ease-out' });
  }
};
