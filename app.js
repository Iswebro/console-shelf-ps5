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
  layout: savedChoice('shelf-layout', ['grid', 'carousel'], 'grid'),
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
  $('scopeAll').setAttribute('aria-pressed', String(settings.scope === 'all'));
  $('scopeAvailable').setAttribute('aria-pressed', String(settings.scope === 'available'));
  $('sort').value = settings.sort;
  $('family').value = settings.family;
  $('availability').value = settings.availability;
  $('carouselControls').hidden = settings.layout !== 'carousel';
  $('swipeHint').hidden = settings.layout !== 'carousel';
  $('lookImages').disabled = settings.layout === 'carousel';
  $('lookCards').disabled = settings.layout === 'carousel';
  $('systems').className = settings.layout === 'carousel' ? 'systems-carousel' : 'systems-grid';
  const look = settings.layout === 'carousel' ? 'Swipe' : settings.look === 'images' ? '3D icons · Grid' : 'Classic cards · Grid';
  const sort = $('sort').selectedOptions[0].textContent;
  const view = settings.scope === 'all' ? 'All consoles' : 'Emulator or games';
  $('shelfSummary').textContent = [look, sort, view, settings.family !== 'all' ? settings.family : '', settings.availability !== 'all' ? statusLabels[settings.availability] : ''].filter(Boolean).join(' · ');
  const filterCount = Number(settings.family !== 'all') + Number(settings.availability !== 'all');
  $('filterBadge').textContent = String(filterCount);
  $('filterBadge').hidden = filterCount === 0;
  $('openBrowse').setAttribute('aria-label', `Sort and filter consoles${filterCount ? `, ${filterCount} filters active` : ''}`);
  const omitted = systems.filter(system => hiddenSystems.has(system.id) || !inScope(system)).length;
  $('consoleBadge').textContent = String(omitted);
  $('consoleBadge').hidden = omitted === 0;
  $('openConsoles').setAttribute('aria-label', `Choose visible consoles${omitted ? `, ${omitted} hidden` : ''}`);
  if (selected) {
    const showImage = settings.layout === 'carousel' || settings.look === 'images';
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
  card.className = 'card ' + (settings.layout === 'carousel' || settings.look === 'images' ? 'icon-card' : 'classic-card');
  card.setAttribute('role', 'listitem');
  card.style.setProperty('--glow', system.color);
  card.dataset.symbol = system.symbol;
  card.dataset.system = system.id;

  const main = document.createElement('button');
  main.type = 'button';
  main.className = 'card-main';
  main.setAttribute('aria-label', 'Open ' + system.name);
  main.onclick = () => { if (!suppressCardClick) openSystem(system.id); };

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
  $('systems').replaceChildren(...shown.map(makeCard));
  $('systems').scrollLeft = 0;
  updateCarouselPosition();
}

function updateCarouselPosition() {
  if (settings.layout !== 'carousel') return;
  const track = $('systems');
  const cards = [...track.children];
  if (!cards.length) {
    $('carouselPosition').textContent = '0 / 0';
    $('carouselPrev').disabled = true;
    $('carouselNext').disabled = true;
    return;
  }
  const gap = parseFloat(getComputedStyle(track).gap) || 0;
  const stride = cards[0].getBoundingClientRect().width + gap;
  const index = Math.min(cards.length - 1, Math.max(0, Math.round(track.scrollLeft / stride)));
  $('carouselPosition').textContent = `${index + 1} / ${cards.length}`;
  $('carouselPrev').disabled = track.scrollLeft < 5;
  $('carouselNext').disabled = track.scrollLeft >= track.scrollWidth - track.clientWidth - 5;
}

function moveCarousel(direction) {
  const track = $('systems');
  const first = track.firstElementChild;
  if (!first) return;
  const stride = first.getBoundingClientRect().width + (parseFloat(getComputedStyle(track).gap) || 0);
  track.scrollBy({ left: direction * stride, behavior: 'smooth' });
}

function openSystem(id) {
  selected = systems.find(system => system.id === id);
  if (!selected) return;
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
  image.hidden = settings.layout !== 'carousel' && settings.look !== 'images';
  image.onerror = () => { image.hidden = true; $('heroSymbol').hidden = false; };
  $('heroSymbol').textContent = selected.symbol;
  $('heroSymbol').hidden = settings.layout === 'carousel' || settings.look === 'images';
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
$('openBrowse').onclick = () => openSettings('Browse', 'sort');
$('openConsoles').onclick = () => openSettings('Consoles', 'consoleSearch');
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
$('systems').onscroll = updateCarouselPosition;
$('systems').onkeydown = event => {
  if (settings.layout !== 'carousel' || ['INPUT', 'SELECT'].includes(event.target.tagName)) return;
  if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') {
    event.preventDefault();
    moveCarousel(event.key === 'ArrowRight' ? 1 : -1);
  }
};
$('systems').onpointerdown = event => {
  if (settings.layout !== 'carousel' || event.pointerType !== 'mouse' || event.button !== 0) return;
  drag = { x: event.clientX, left: $('systems').scrollLeft, moved: false };
};
$('systems').onpointermove = event => {
  if (!drag) return;
  const distance = event.clientX - drag.x;
  if (Math.abs(distance) > 6) drag.moved = true;
  if (drag.moved) {
    $('systems').classList.add('dragging');
    $('systems').scrollLeft = drag.left - distance;
  }
};
function endDrag() {
  if (drag?.moved) {
    suppressCardClick = true;
    setTimeout(() => { suppressCardClick = false; }, 150);
  }
  drag = null;
  $('systems').classList.remove('dragging');
}
window.addEventListener('pointerup', endDrag);
window.addEventListener('pointercancel', endDrag);
window.addEventListener('resize', updateCarouselPosition);
$('back').onclick = () => { $('detail').hidden = true; $('home').hidden = false; selected = null; };
$('emulator').onchange = () => {
  safeStorage.set('shelf-emulator-' + selected.id, $('emulator').value);
  toast(`${$('emulator').value} saved as the preferred emulator for ${selected.name} in this preview.`);
};
$('openEmulator').onclick = () => toast(`Preview only: the finished PS5 app would open ${$('emulator').value}.`);
$('showLibrary').onclick = renderGames;

renderSettings();
renderIcon();
renderSystems();
