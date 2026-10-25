// Design preview only. The 24 October 2026 local inventory is not a live PS5 scan.
const systems = window.shelfSystems;
const statusLabels = {
  native: 'Native on PS5',
  tested: 'Game tested here',
  mapped: 'RetroArch core listed',
  app: 'Emulator app listed',
  reference: 'Hardware reference only'
};
const yearText = system => system.year === null ? '' : String(system.year);

const $ = id => document.getElementById(id);
const appIcons = {
  gaming: 'assets/shelf-icon.png',
  classic: 'assets/shelf-icon-v1.png',
  jade: 'assets/shelf-icon-premium-v2.png'
};
const safeStorage = {
  get(key) { try { return localStorage.getItem(key); } catch { return null; } },
  set(key, value) { try { localStorage.setItem(key, value); } catch { /* Preview still works without persistence. */ } }
};
function savedChoice(key, allowed, fallback) {
  const value = safeStorage.get(key);
  return allowed.includes(value) ? value : fallback;
}
const settings = {
  look: savedChoice('shelf-look', ['images', 'cards'], 'images'),
  layout: savedChoice('shelf-layout', ['grid', 'carousel', 'shelf'], 'grid'),
  scope: savedChoice('shelf-scope', ['all', 'available'], 'all'),
  sort: savedChoice('shelf-sort', ['featured', 'az', 'za', 'newest', 'oldest', 'brand', 'games', 'favorites'], 'featured'),
  family: savedChoice('shelf-family', ['all', 'PlayStation', 'Nintendo', 'Sega', 'Sammy', 'Atari', 'NEC', 'SNK', 'Bandai', '3DO', 'Arcade', 'Commodore', 'Computer'], 'all'),
  availability: savedChoice('shelf-availability', ['all', 'tested', 'native', 'mapped', 'app', 'reference'], 'all'),
  icon: savedChoice('shelf-icon', Object.keys(appIcons), 'gaming')
};
function savedSystemSet(key) {
  try {
    const saved = JSON.parse(safeStorage.get(key) || '[]');
    return new Set(Array.isArray(saved) ? saved.filter(id => systems.some(system => system.id === id)) : []);
  } catch { return new Set(); }
}
const favorites = savedSystemSet('shelf-favorites');
const hiddenSystems = savedSystemSet('shelf-hidden-systems');
const settingsTabs = ['Appearance', 'Browse', 'Consoles'];
let settingsOpener = null;
let selected = null;
let toastTimer;
let drag = null;
let suppressCardClick = false;
let carouselId = safeStorage.get('shelf-carousel-selection');
let carouselCards = [];
let carouselIndex = 0;
let shelfColumns = 4;
const layouts = ['grid', 'carousel', 'shelf'];
const layoutNames = { grid: 'Grid', carousel: 'Swipe', shelf: 'Shelf' };
let orbitFrame = 0;
const orbitMotion = new ShelfOrbit.Motion();
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');

function toast(message) {
  const node = $('toast');
  node.textContent = message;
  node.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => node.classList.remove('show'), 3000);
}

function setChoice(name, value) {
  settings[name] = value;
  safeStorage.set('shelf-' + name, value);
  renderSettings();
  renderSystems();
}

function renderSettings() {
  $('lookImages').setAttribute('aria-pressed', String(settings.look === 'images'));
  $('lookCards').setAttribute('aria-pressed', String(settings.look === 'cards'));
  $('layoutGrid').setAttribute('aria-pressed', String(settings.layout === 'grid'));
  $('layoutCarousel').setAttribute('aria-pressed', String(settings.layout === 'carousel'));
  $('layoutShelf').setAttribute('aria-pressed', String(settings.layout === 'shelf'));
  $('scopeAll').setAttribute('aria-pressed', String(settings.scope === 'all'));
  $('scopeAvailable').setAttribute('aria-pressed', String(settings.scope === 'available'));
  $('sort').value = settings.sort;
  $('family').value = settings.family;
  $('availability').value = settings.availability;
  $('carouselControls').hidden = settings.layout !== 'carousel';
  $('swipeHint').hidden = settings.layout !== 'carousel';
  $('carouselCaption').hidden = settings.layout !== 'carousel';
  $('lookImages').disabled = settings.layout !== 'grid';
  $('lookCards').disabled = settings.layout !== 'grid';
  $('systems').className = settings.layout === 'carousel' ? 'systems-carousel' : settings.layout === 'shelf' ? 'systems-shelf' : 'systems-grid';
  const look = settings.layout !== 'grid' ? layoutNames[settings.layout] : settings.look === 'images' ? '3D icons · Grid' : 'Classic cards · Grid';
  const sort = $('sort').selectedOptions[0].textContent;
  const view = settings.scope === 'all' ? 'All consoles' : 'Emulator or games';
  $('shelfSummary').textContent = [look, sort, view, settings.family !== 'all' ? settings.family : '', settings.availability !== 'all' ? statusLabels[settings.availability] : ''].filter(Boolean).join(' · ');
  const next = layouts[(layouts.indexOf(settings.layout) + 1) % layouts.length];
  const nextLayout = layoutNames[next];
  $('toggleLayout').dataset.layout = settings.layout;
  $('toggleLayout').dataset.nextLayout = next;
  $('toggleLayout').setAttribute('aria-label', `Switch to ${nextLayout} layout`);
  $('toggleLayout').setAttribute('title', `Switch to ${nextLayout}`);
  $('layoutToggleLabel').textContent = nextLayout;
  if (selected) {
    const showImage = settings.layout !== 'grid' || settings.look === 'images';
    $('heroImage').hidden = !showImage;
    $('heroSymbol').hidden = showImage;
  }
}

function inScope(system) {
  return settings.scope === 'all' || system.status !== 'reference' || system.games.length > 0;
}

function pickerSystems() {
  const query = $('consoleSearch').value.trim().toLocaleLowerCase();
  return systems.filter(system => [system.name, system.family].some(value => value.toLocaleLowerCase().includes(query)));
}

function syncConsolePicker() {
  const matches = pickerSystems();
  $('visibleConsoleCount').textContent = `${systems.length - hiddenSystems.size} of ${systems.length} enabled`;
  const searching = $('consoleSearch').value.trim().length > 0;
  $('showAllConsoles').textContent = searching ? 'Show results' : 'Show all';
  $('hideAllConsoles').textContent = searching ? 'Hide results' : 'Hide all';
  $('showAllConsoles').disabled = !matches.some(system => hiddenSystems.has(system.id));
  $('hideAllConsoles').disabled = !matches.some(system => !hiddenSystems.has(system.id));
  $('consolePicker').querySelectorAll('input[data-console]').forEach(input => {
    input.checked = !hiddenSystems.has(input.dataset.console);
  });
  $('consolePicker').querySelectorAll('input[data-family]').forEach(input => {
    const members = matches.filter(system => system.family === input.dataset.family);
    const enabled = members.filter(system => !hiddenSystems.has(system.id)).length;
    input.checked = enabled === members.length;
    input.indeterminate = enabled > 0 && enabled < members.length;
    input.parentElement.querySelector('small').textContent = `${enabled} / ${members.length}`;
  });
}

function renderConsolePicker() {
  const matches = pickerSystems();
  const groups = [...new Set(matches.map(system => system.family))];
  const fragment = document.createDocumentFragment();
  for (const family of groups) {
    const group = document.createElement('div');
    group.className = 'console-picker-group';
    const heading = document.createElement('label');
    heading.className = 'console-group-heading';
    const groupToggle = document.createElement('input');
    groupToggle.type = 'checkbox';
    groupToggle.dataset.family = family;
    groupToggle.setAttribute('aria-label', `Show all ${family} consoles${$('consoleSearch').value.trim() ? ' in search results' : ''}`);
    const groupCount = document.createElement('small');
    heading.append(groupToggle, document.createTextNode(family), groupCount);
    group.append(heading);
    for (const system of matches.filter(system => system.family === family)) {
      const row = document.createElement('label');
      row.className = 'console-toggle';
      const image = document.createElement('img');
      image.src = system.image;
      image.alt = '';
      image.loading = 'lazy';
      const copy = document.createElement('span');
      const name = document.createElement('strong');
      name.textContent = system.name;
      const route = document.createElement('small');
      route.textContent = system.status === 'reference' ? 'Hardware reference only' : [system.emulators[0], system.games.length ? `${system.games.length} example games` : ''].filter(Boolean).join(' · ');
      copy.append(name, route);
      const toggle = document.createElement('input');
      toggle.type = 'checkbox';
      toggle.setAttribute('role', 'switch');
      toggle.setAttribute('aria-label', `Show ${system.name}`);
      toggle.dataset.console = system.id;
      row.append(image, copy, toggle);
      group.append(row);
    }
    fragment.append(group);
  }
  $('consolePicker').replaceChildren(fragment);
  $('emptyPicker').hidden = matches.length > 0;
  syncConsolePicker();
}

function saveConsoleVisibility() {
  safeStorage.set('shelf-hidden-systems', JSON.stringify(systems.filter(system => hiddenSystems.has(system.id)).map(system => system.id)));
  syncConsolePicker();
  renderSettings();
  renderSystems();
}

function setSettingsTab(name, focusTab = false) {
  for (const tab of settingsTabs) {
    const active = tab === name;
    $('tab' + tab).setAttribute('aria-selected', String(active));
    $('tab' + tab).tabIndex = active ? 0 : -1;
    $('panel' + tab).hidden = !active;
  }
  if (name === 'Consoles') renderConsolePicker();
  if (focusTab) $('tab' + name).focus();
  $('settingsDialog').querySelector('.settings-body').scrollTop = 0;
}

function openSettings(tab, focusId) {
  settingsOpener = document.activeElement;
  setSettingsTab(tab);
  $('settingsDialog').showModal();
  document.body.classList.add('settings-open');
  $(focusId || 'tab' + tab).focus();
}

function renderIcon() {
  const source = appIcons[settings.icon];
  $('brandIcon').src = source;
  $('appFavicon').href = source;
  $('appTouchIcon').href = source;
  $('iconStyle').value = settings.icon;
}

function sortedSystems() {
  const query = $('search').value.trim().toLocaleLowerCase();
  const shown = systems.filter(system =>
    !hiddenSystems.has(system.id) && inScope(system) &&
    (settings.family === 'all' || system.family === settings.family) &&
    (settings.availability === 'all' || system.status === settings.availability) &&
    [system.name, system.family, system.core || '', ...system.emulators].some(value => value.toLocaleLowerCase().includes(query))
  );
  const name = (a, b) => a.name.localeCompare(b.name);
  const featured = (a, b) => systems.indexOf(a) - systems.indexOf(b);
  const sorters = {
    featured,
    az: name,
    za: (a, b) => name(b, a),
    newest: (a, b) => (b.year ?? -1) - (a.year ?? -1) || name(a, b),
    oldest: (a, b) => (a.year ?? 9999) - (b.year ?? 9999) || name(a, b),
    brand: (a, b) => a.family.localeCompare(b.family) || name(a, b),
    games: (a, b) => b.games.length - a.games.length || name(a, b),
    favorites: (a, b) => Number(favorites.has(b.id)) - Number(favorites.has(a.id)) || featured(a, b)
  };
  return shown.sort(sorters[settings.sort]);
}

function makeCard(system) {
  const card = document.createElement('article');
  card.className = 'card ' + (settings.layout !== 'grid' || settings.look === 'images' ? 'icon-card' : 'classic-card');
  card.setAttribute('role', 'listitem');
  card.style.setProperty('--glow', system.color);
  card.dataset.symbol = system.symbol;
  card.dataset.system = system.id;

  const main = document.createElement('button');
  main.type = 'button';
  main.className = 'card-main';
  main.setAttribute('aria-label', 'Open ' + system.name);
  main.onclick = () => {
    if (suppressCardClick) return;
    if (settings.layout === 'carousel' && carouselId !== system.id) {
      selectCarousel(carouselCards.indexOf(card));
    } else openSystem(system.id);
  };

  const art = document.createElement('span');
  art.className = 'console-art';
  const image = document.createElement('img');
  image.src = system.image;
  image.alt = '';
  image.loading = 'lazy';
  image.decoding = 'async';
  image.onerror = () => { image.hidden = true; art.textContent = system.symbol; };
  art.append(image);

  const copy = document.createElement('span');
  copy.className = 'card-copy';
  const top = document.createElement('span');
  top.className = 'card-topline';
  top.append(document.createTextNode(system.family));
  const dot = document.createElement('span');
  dot.className = 'dot';
  dot.textContent = '•';
  if (system.year !== null) top.append(dot, document.createTextNode(yearText(system)));
  const title = document.createElement('strong');
  title.className = 'card-title';
  title.textContent = system.name;
  const badge = document.createElement('span');
  badge.className = `status-badge status-${system.status}`;
  badge.textContent = statusLabels[system.status];
  const meta = document.createElement('span');
  meta.className = 'card-meta';
  meta.textContent = system.status === 'reference' ? 'No verified PS5 emulator' :
    system.games.length ? `${system.games.length} recorded example ${system.games.length === 1 ? 'title' : 'titles'}` :
    system.core ? `Core: ${system.core}` : system.emulators[0];
  copy.append(top, title, badge, meta);
  main.append(art, copy);

  const favorite = document.createElement('button');
  favorite.type = 'button';
  favorite.className = 'favorite';
  favorite.textContent = favorites.has(system.id) ? '★' : '☆';
  favorite.setAttribute('aria-label', `${favorites.has(system.id) ? 'Remove' : 'Add'} ${system.name} ${favorites.has(system.id) ? 'from' : 'to'} favorites`);
  favorite.setAttribute('aria-pressed', String(favorites.has(system.id)));
  favorite.onclick = () => {
    if (favorites.has(system.id)) favorites.delete(system.id); else favorites.add(system.id);
    safeStorage.set('shelf-favorites', JSON.stringify([...favorites]));
    renderSystems();
    if (settings.layout === 'carousel') selectCarousel(carouselIndex);
    toast(`${system.name} ${favorites.has(system.id) ? 'added to' : 'removed from'} favorites.`);
  };
  card.append(main, favorite);
  return card;
}

function renderSystems() {
  const shown = sortedSystems();
  $('systemCount').textContent = `${shown.length} of ${systems.length} systems`;
  $('emptySearch').hidden = shown.length !== 0;
  $('emptyMessage').textContent = hiddenSystems.size === systems.length ? 'Your shelf is empty. Switch on a few consoles to make it yours.' : 'No systems match this view. Try another search or review your settings.';
  const cards = shown.map(makeCard);
  if (settings.layout === 'shelf') {
    shelfColumns = shelfColumnCount();
    const tiers = [];
    for (let i = 0; i < cards.length; i += shelfColumns) {
      const tier = document.createElement('div');
      tier.className = 'display-tier';
      tier.setAttribute('role', 'presentation');
      tier.style.setProperty('--shelf-columns', shelfColumns);
      tier.append(...cards.slice(i, i + shelfColumns));
      tiers.push(tier);
    }
    $('systems').replaceChildren(...tiers);
    placeShelfIcons();
  } else $('systems').replaceChildren(...cards);
  cancelAnimationFrame(orbitFrame);
  orbitFrame = 0;
  carouselCards = settings.layout === 'carousel' ? cards : [];
  const previous = shown.findIndex(system => system.id === carouselId);
  carouselIndex = previous < 0 ? 0 : previous;
  carouselId = shown[carouselIndex]?.id || null;
  orbitMotion.position = orbitMotion.target = carouselIndex;
  orbitMotion.active = false;
  updateCarouselPosition();
}

function shelfColumnCount() {
  const width = $('systems').clientWidth;
  return width < 560 ? 2 : width < 950 ? 3 : 4;
}

function placeShelfIcons() {
  if (settings.layout !== 'shelf' || $('home').hidden) return;
  $('systems').querySelectorAll('.card').forEach(card => {
    const bounds = shelfArtLayout[card.dataset.system];
    const surface = card.querySelector('.console-art');
    const image = surface.querySelector('img');
    if (!bounds || !image || !surface.clientWidth) return;
    const [x, y, w, h] = bounds.bounds;
    const fit = Math.min((surface.clientWidth - 30) / w, (surface.clientHeight - 23) / h);
    image.style.width = `${bounds.canvas[0] * fit}px`;
    image.style.height = `${bounds.canvas[1] * fit}px`;
    image.style.left = `${(surface.clientWidth - w * fit) / 2 - x * fit}px`;
    const supportY = bounds.supportY ?? y + h;
    image.style.top = `${surface.clientHeight - supportY * fit}px`;
    surface.style.setProperty('--art-foot-x', `${(x + w / 2) * fit}px`);
    surface.style.setProperty('--art-foot-y', `${supportY * fit}px`);
  });
}

function moveShelf(key, current = document.activeElement) {
  if (settings.layout !== 'shelf') return false;
  const cards = [...$('systems').querySelectorAll('.card')];
  const at = cards.indexOf(current?.closest('.card'));
  if (at < 0) return false;
  let next = at;
  if (key === 'ArrowLeft') next = at % shelfColumns ? at - 1 : at;
  else if (key === 'ArrowRight') next = at % shelfColumns < shelfColumns - 1 ? Math.min(at + 1, cards.length - 1) : at;
  else if (key === 'ArrowUp') {
    if (at < shelfColumns) { $('toggleLayout').focus(); return true; }
    next = at - shelfColumns;
  } else if (key === 'ArrowDown') next = Math.min(at + shelfColumns, cards.length - 1);
  else if (key === 'Home') next = 0;
  else if (key === 'End') next = cards.length - 1;
  else return false;
  cards[next]?.querySelector('.card-main').focus();
  return true;
}

function updateCarouselPosition() {
  if (settings.layout !== 'carousel') return;
  if (!carouselCards.length) {
    $('carouselPosition').textContent = '0 / 0';
    $('carouselPrev').disabled = true;
    $('carouselNext').disabled = true;
    $('carouselCaption').hidden = true;
    return;
  }
  const system = systems.find(item => item.id === carouselId);
  $('carouselCaption').hidden = false;
  $('carouselPosition').textContent = `${carouselIndex + 1} / ${carouselCards.length}`;
  $('carouselPrev').disabled = $('carouselNext').disabled = carouselCards.length < 2;
  $('carouselFamily').textContent = [system.family, yearText(system)].filter(Boolean).join(' / ');
  $('carouselTitle').textContent = system.name;
  $('carouselOpen').setAttribute('aria-label', 'Explore ' + system.name);
  $('carouselStatus').textContent = statusLabels[system.status];
  $('carouselStatus').className = `status-badge status-${system.status}`;
  $('systems').style.setProperty('--selection-glow', system.color);
  paintOrbit();
}

function moveCarousel(direction) {
  selectCarousel(carouselIndex + direction);
}

function selectCarousel(index, focus = true) {
  if (!carouselCards.length || settings.layout !== 'carousel') return;
  carouselIndex = ShelfOrbit.wrap(index, carouselCards.length);
  carouselId = carouselCards[carouselIndex].dataset.system;
  safeStorage.set('shelf-carousel-selection', carouselId);
  orbitMotion.select(carouselIndex, carouselCards.length, performance.now(), reducedMotion.matches);
  updateCarouselPosition();
  if (focus) carouselCards[carouselIndex].querySelector('.card-main').focus({ preventScroll: true });
  if (!orbitFrame && orbitMotion.active) orbitFrame = requestAnimationFrame(animateOrbit);
}

function paintOrbit() {
  const track = $('systems');
  if (settings.layout !== 'carousel') return;
  const width = track.clientWidth;
  carouselCards.forEach((card, index) => {
    const offset = ShelfOrbit.distance(index - orbitMotion.position, carouselCards.length);
    const pose = ShelfOrbit.pose(offset, width);
    card.style.transform = `translate3d(calc(-50% + ${pose.x}px), calc(-50% + ${pose.y}px), ${pose.z}px) rotateY(${pose.yaw}deg) scale(${pose.scale})`;
    card.style.opacity = String(pose.opacity);
    card.style.zIndex = String(Math.round(1000 - Math.abs(offset) * 100));
    card.style.visibility = pose.visible ? 'visible' : 'hidden';
    if (pose.visible) {
      const art = shelfArtLayout[card.dataset.system];
      const image = card.querySelector('.console-art img');
      if (art && image) {
        const [x,y,w,h] = art.bounds, fit = Math.min(card.offsetWidth / w, card.offsetHeight / h);
        image.style.width = `${art.canvas[0] * fit}px`;
        image.style.height = `${art.canvas[1] * fit}px`;
        image.style.left = `${(card.offsetWidth - w * fit) / 2 - x * fit}px`;
        image.style.top = `${(card.offsetHeight - h * fit) / 2 - y * fit}px`;
      }
    }
    card.inert = !pose.visible;
    card.setAttribute('aria-hidden', String(!pose.visible));
    card.classList.toggle('is-selected', index === carouselIndex);
    const main = card.querySelector('.card-main');
    main.tabIndex = index === carouselIndex ? 0 : -1;
    main.setAttribute('aria-current', String(index === carouselIndex));
    card.querySelector('.favorite').tabIndex = index === carouselIndex ? 0 : -1;
  });
  track.dataset.activeSystem = carouselId || '';
  track.dataset.animating = String(orbitMotion.active);
}

function animateOrbit(time) {
  orbitFrame = 0;
  if (settings.layout !== 'carousel') return;
  orbitMotion.update(time);
  paintOrbit();
  if (orbitMotion.active) orbitFrame = requestAnimationFrame(animateOrbit);
}

function openSystem(id) {
  selected = systems.find(system => system.id === id);
  if (!selected) return;
  carouselId = id;
  safeStorage.set('shelf-carousel-selection', id);
  $('home').hidden = true;
  $('detail').hidden = false;
  $('hero').style.setProperty('--glow', selected.color);
  $('detailFamily').textContent = [selected.family.toUpperCase(), yearText(selected)].filter(Boolean).join(' · ');
  $('detailName').textContent = selected.name;
  $('detailStatus').textContent = statusLabels[selected.status];
  $('detailStatus').className = `status-badge status-${selected.status}`;
  const descriptions = {
    native: selected.id === 'ps4' ? 'PS5 has built-in PS4 backward compatibility. Individual games vary. This preview does not scan your installed titles.' : 'PS5 games run on the console itself. This preview does not scan your installed titles.',
    tested: `At least one ${selected.name} game has run on this PS5. ${selected.core ? `RetroArch also has the ${selected.core} core mapped.` : ''} Other games may behave differently; the titles below are examples from earlier setup notes.`,
    mapped: `The 24 October RetroArch inventory maps this system to the installed ${selected.core} core. We have not confirmed gameplay for this system on this PS5.`,
    app: `${selected.emulators[0]} was recorded in the PS5 setup notes. Gameplay for this system has not been confirmed here.`,
    reference: 'Nintendo has officially shown the Switch 2 hardware, so it has a visual card. No Switch 2 emulator has been verified on this PS5; launching and scanning are unavailable.'
  };
  $('detailDescription').textContent = descriptions[selected.status];
  const image = $('heroImage');
  image.src = selected.image;
  image.alt = selected.name + ' console';
  image.hidden = settings.layout === 'grid' && settings.look !== 'images';
  image.onerror = () => { image.hidden = true; $('heroSymbol').hidden = false; };
  $('heroSymbol').textContent = selected.symbol;
  $('heroSymbol').hidden = settings.layout !== 'grid' || settings.look === 'images';
  const menu = $('emulator');
  menu.replaceChildren(...selected.emulators.map(emulator => new Option(emulator, emulator)));
  const saved = safeStorage.get('shelf-emulator-' + id);
  if (selected.emulators.includes(saved)) menu.value = saved;
  $('emulatorLabel').hidden = selected.status === 'reference';
  $('openEmulator').disabled = selected.status === 'reference';
  $('showLibrary').disabled = selected.status === 'reference';
  $('libraryPanel').hidden = true;
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

function renderGames() {
  if (!selected) return;
  $('libraryPanel').hidden = false;
  $('libraryTitle').textContent = `${selected.name} games`;
  $('libraryCount').textContent = `${selected.games.length} example ${selected.games.length === 1 ? 'game' : 'games'}`;
  const list = $('games');
  list.replaceChildren();
  if (!selected.games.length) {
    const empty = document.createElement('div');
    empty.className = 'empty-library';
    empty.textContent = 'No example games are shown. Core and app listings alone do not confirm that a game will run; library scanning still needs to be built.';
    list.append(empty);
  } else {
    selected.games.forEach(game => {
      const card = document.createElement('div');
      card.className = 'game';
      const label = document.createElement('strong');
      label.textContent = game;
      card.append(label);
      list.append(card);
    });
  }
  $('libraryPanel').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

$('lookImages').onclick = () => setChoice('look', 'images');
$('lookCards').onclick = () => setChoice('look', 'cards');
$('layoutGrid').onclick = () => setChoice('layout', 'grid');
$('layoutCarousel').onclick = () => setChoice('layout', 'carousel');
$('layoutShelf').onclick = () => setChoice('layout', 'shelf');
$('scopeAll').onclick = () => setChoice('scope', 'all');
$('scopeAvailable').onclick = () => setChoice('scope', 'available');
$('sort').onchange = event => setChoice('sort', event.target.value);
$('family').onchange = event => setChoice('family', event.target.value);
$('availability').onchange = event => setChoice('availability', event.target.value);
$('clearFilters').onclick = () => {
  settings.family = 'all';
  settings.availability = 'all';
  safeStorage.set('shelf-family', 'all');
  safeStorage.set('shelf-availability', 'all');
  renderSettings();
  renderSystems();
};
$('openAppearance').onclick = () => openSettings('Appearance');
$('toggleLayout').onclick = () => setChoice('layout', layouts[(layouts.indexOf(settings.layout) + 1) % layouts.length]);
$('reviewSettings').onclick = () => openSettings(hiddenSystems.size ? 'Consoles' : 'Browse');
for (const tab of settingsTabs) $('tab' + tab).onclick = () => setSettingsTab(tab);
$('settingsDialog').querySelector('.settings-tabs').onkeydown = event => {
  const index = settingsTabs.findIndex(tab => 'tab' + tab === event.target.id);
  if (index < 0 || !['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
  event.preventDefault();
  const next = event.key === 'Home' ? 0 : event.key === 'End' ? settingsTabs.length - 1 : (index + (event.key === 'ArrowRight' ? 1 : -1) + settingsTabs.length) % settingsTabs.length;
  setSettingsTab(settingsTabs[next], true);
};
$('closeSettings').onclick = $('doneSettings').onclick = () => $('settingsDialog').close();
$('settingsDialog').onclose = () => {
  document.body.classList.remove('settings-open');
  if (settingsOpener?.isConnected) settingsOpener.focus();
};
$('settingsDialog').onclick = event => {
  if (event.target !== $('settingsDialog')) return;
  const rect = $('settingsDialog').getBoundingClientRect();
  if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) $('settingsDialog').close();
};
$('consoleSearch').oninput = renderConsolePicker;
$('consolePicker').onchange = event => {
  const { console: id, family } = event.target.dataset;
  const matches = id ? systems.filter(system => system.id === id) : family ? pickerSystems().filter(system => system.family === family) : [];
  for (const system of matches) {
    if (event.target.checked) hiddenSystems.delete(system.id); else hiddenSystems.add(system.id);
  }
  if (matches.length) saveConsoleVisibility();
};
$('showAllConsoles').onclick = () => { for (const system of pickerSystems()) hiddenSystems.delete(system.id); saveConsoleVisibility(); };
$('hideAllConsoles').onclick = () => { for (const system of pickerSystems()) hiddenSystems.add(system.id); saveConsoleVisibility(); };
$('iconStyle').onchange = event => {
  settings.icon = event.target.value;
  safeStorage.set('shelf-icon', settings.icon);
  renderIcon();
};
$('search').oninput = renderSystems;
$('carouselPrev').onclick = () => moveCarousel(-1);
$('carouselNext').onclick = () => moveCarousel(1);
$('carouselOpen').onclick = () => { if (carouselId) openSystem(carouselId); };
$('systems').onkeydown = event => {
  if (settings.layout === 'shelf' && moveShelf(event.key, event.target)) { event.preventDefault(); return; }
  if (settings.layout !== 'carousel' || ['INPUT', 'SELECT'].includes(event.target.tagName)) return;
  if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') {
    event.preventDefault();
    moveCarousel(event.key === 'ArrowRight' ? 1 : -1);
  } else if (event.key === 'Home' || event.key === 'End') {
    event.preventDefault();
    selectCarousel(event.key === 'Home' ? 0 : carouselCards.length - 1);
  }
};
$('systems').onpointerdown = event => {
  if (settings.layout !== 'carousel' || event.button !== 0 || event.target.closest('.favorite')) return;
  orbitMotion.update(performance.now());
  drag = { id: event.pointerId, x: event.clientX, y: event.clientY, position: orbitMotion.position, index: carouselIndex, moved: false, distance: 0 };
};
$('systems').onpointermove = event => {
  if (!drag || drag.id !== event.pointerId) return;
  const distance = event.clientX - drag.x;
  if (!drag.moved && Math.abs(event.clientY - drag.y) > Math.max(10, Math.abs(distance))) { drag = null; return; }
  if (Math.abs(distance) > 8 && !drag.moved) {
    drag.moved = true;
    $('systems').setPointerCapture(event.pointerId);
    cancelAnimationFrame(orbitFrame);
    orbitFrame = 0;
  }
  if (drag.moved) {
    drag.distance = distance;
    $('systems').classList.add('dragging');
    orbitMotion.active = false;
    orbitMotion.position = drag.position - distance / Math.min(280, $('systems').clientWidth * .42);
    paintOrbit();
  }
};
function endDrag(event) {
  if (drag && event.pointerId !== drag.id) return;
  if (drag?.moved) {
    suppressCardClick = true;
    setTimeout(() => { suppressCardClick = false; }, 150);
    const stride = Math.min(280, $('systems').clientWidth * .42);
    const step = Math.round(-drag.distance / stride) || (Math.abs(drag.distance) > 36 ? -Math.sign(drag.distance) : 0);
    selectCarousel(drag.index + (event.type === 'pointercancel' ? 0 : step));
  }
  drag = null;
  $('systems').classList.remove('dragging');
}
window.addEventListener('pointerup', endDrag);
window.addEventListener('pointercancel', endDrag);
window.addEventListener('resize', () => {
  updateCarouselPosition();
  if (settings.layout !== 'shelf' || $('home').hidden) return;
  const focused = document.activeElement.closest('.card')?.dataset.system;
  if (shelfColumnCount() !== shelfColumns) {
    renderSystems();
    if (focused) $('systems').querySelector(`.card[data-system="${focused}"] .card-main`)?.focus({ preventScroll: true });
  } else placeShelfIcons();
});
reducedMotion.addEventListener('change', () => selectCarousel(carouselIndex, false));
let wheelAt = 0;
$('systems').addEventListener('wheel', event => {
  if (settings.layout !== 'carousel' || (!event.shiftKey && Math.abs(event.deltaX) <= Math.abs(event.deltaY))) return;
  const delta = event.deltaX || event.deltaY;
  if (!delta) return;
  event.preventDefault();
  if (performance.now() - wheelAt > 180) { moveCarousel(Math.sign(delta)); wheelAt = performance.now(); }
}, { passive: false });

// Standard-mapped controller input. Poll only while a controller is present;
// held sticks repeat at a deliberate pace and cannot accumulate a move queue.
let gamepadFrame = 0, gamepadDirection = 0, gamepadRepeat = 0, gamepadButtons = [];
function pollGamepad(time) {
  gamepadFrame = 0;
  let pad;
  try { pad = [...(navigator.getGamepads?.() || [])].find(item => item?.connected && item.mapping === 'standard'); } catch { return; }
  if (!pad || document.hidden) { gamepadDirection = 0; gamepadButtons = []; return; }
  const buttons = pad.buttons.map(button => button.pressed);
  if (!$('settingsDialog').open) {
    if (settings.layout === 'carousel' && !$('home').hidden) {
      const direction = buttons[14] ? -1 : buttons[15] ? 1 : Math.abs(pad.axes[0] || 0) > .55 ? Math.sign(pad.axes[0]) : 0;
      if (direction && (direction !== gamepadDirection || time >= gamepadRepeat)) {
        moveCarousel(direction);
        gamepadRepeat = time + (direction === gamepadDirection ? 180 : 340);
      }
      gamepadDirection = direction;
      if (buttons[0] && !gamepadButtons[0]) carouselCards[carouselIndex]?.querySelector('.card-main').click();
      if (buttons[3] && !gamepadButtons[3]) carouselCards[carouselIndex]?.querySelector('.favorite').click();
      if (buttons[9] && !gamepadButtons[9]) openSettings('Appearance');
    } else if (settings.layout === 'shelf' && !$('home').hidden) {
      const direction = buttons[12] ? 'ArrowUp' : buttons[13] ? 'ArrowDown' : buttons[14] ? 'ArrowLeft' : buttons[15] ? 'ArrowRight' :
        Math.abs(pad.axes[1] || 0) > .55 ? (pad.axes[1] < 0 ? 'ArrowUp' : 'ArrowDown') :
        Math.abs(pad.axes[0] || 0) > .55 ? (pad.axes[0] < 0 ? 'ArrowLeft' : 'ArrowRight') : '';
      if (direction && (direction !== gamepadDirection || time >= gamepadRepeat)) {
        if (!$('systems').contains(document.activeElement)) $('systems').querySelector('.card-main')?.focus();
        else moveShelf(direction);
        gamepadRepeat = time + (direction === gamepadDirection ? 180 : 340);
      }
      gamepadDirection = direction;
      if (buttons[0] && !gamepadButtons[0]) document.activeElement.closest('.card')?.querySelector('.card-main').click();
      if (buttons[3] && !gamepadButtons[3]) document.activeElement.closest('.card')?.querySelector('.favorite').click();
      if (buttons[9] && !gamepadButtons[9]) openSettings('Appearance');
    } else if (!$('detail').hidden && buttons[1] && !gamepadButtons[1]) $('back').click();
  } else {
    gamepadDirection = 0;
    if (buttons[1] && !gamepadButtons[1]) $('settingsDialog').close();
  }
  gamepadButtons = buttons;
  gamepadFrame = requestAnimationFrame(pollGamepad);
}
function startGamepad() { if (!gamepadFrame) gamepadFrame = requestAnimationFrame(pollGamepad); }
window.addEventListener('gamepadconnected', startGamepad);
document.addEventListener('visibilitychange', () => { if (!document.hidden) startGamepad(); });
startGamepad();
$('back').onclick = () => {
  $('detail').hidden = true; $('home').hidden = false; selected = null;
  requestAnimationFrame(() => {
    updateCarouselPosition();
    if (settings.layout === 'shelf') {
      if (shelfColumnCount() !== shelfColumns) renderSystems(); else placeShelfIcons();
      $('systems').querySelector(`.card[data-system="${carouselId}"] .card-main`)?.focus();
    }
    if (settings.layout === 'carousel') carouselCards[carouselIndex]?.querySelector('.card-main').focus({ preventScroll: true });
  });
};
$('emulator').onchange = () => {
  safeStorage.set('shelf-emulator-' + selected.id, $('emulator').value);
  toast(`${$('emulator').value} saved as the preferred emulator for ${selected.name} in this preview.`);
};
$('openEmulator').onclick = () => toast(`Preview only: the finished PS5 app would open ${$('emulator').value}.`);
$('showLibrary').onclick = renderGames;

renderSettings();
renderIcon();
renderSystems();
