// Local preview inventory, checked against this PS5's 24 October 2026 emulator notes.
// A mapped RetroArch core is an installed route to try, not a gameplay guarantee.
// Furniture scale is separate from artwork bounds: portable hardware should
// not fill the same physical shelf space as a full-sized home console.
const handhelds = new Set(['psp', 'switch2', 'switch', '3ds', 'ds', 'gba', 'gbc', 'gb', 'pokemonmini',
  'gamegear', 'atarilynx', 'neogeopocketcolor', 'neogeopocket', 'wonderswancolor', 'wonderswan']);
const system = ({ id, name, family, year = null, symbol, color, status, emulators, core = null, games = [] }) => ({
  id, name, family, year, symbol, color, status, emulators, core, games, roomScale: handhelds.has(id) ? .62 : 1,
  image: `assets/3d-icons/${id}.png`
});
const ra = (id, name, family, year, symbol, color, core, extra = {}) => system({
  id, name, family, year, symbol, color, core,
  status: 'mapped', emulators: [`RetroArch · ${core}`], ...extra
});

const shelfSystems = [
  system({ id: 'ps5', name: 'PlayStation 5', family: 'PlayStation', year: 2020, symbol: '5', color: '#4a4ca8', status: 'native', emulators: ['PS5 system'] }),
  system({ id: 'ps4', name: 'PlayStation 4', family: 'PlayStation', year: 2013, symbol: '4', color: '#345a99', status: 'native', emulators: ['PS5 backward compatibility'] }),
  system({ id: 'ps3', name: 'PlayStation 3', family: 'PlayStation', year: 2006, symbol: '3', color: '#813f70', status: 'tested', emulators: ['PS5SX3'], games: ['Guitar Hero III: Legends of Rock', 'Alice: Madness Returns'] }),
  ra('ps2', 'PlayStation 2', 'PlayStation', 2000, '2', '#374da0', 'ps5sx2', { status: 'tested', emulators: ['PS5SX2', 'RetroArch · ps5sx2'], games: ['Guitar Hero II Deluxe', 'Guitar Hero World Tour', 'Castlevania: Lament of Innocence'] }),
  ra('ps1', 'PlayStation 1', 'PlayStation', 1994, '1', '#80558b', 'swanstation', { status: 'tested', emulators: ['PSXS5', 'RetroArch · SwanStation'], games: ['Metal Slug X'] }),
  ra('psp', 'PSP', 'PlayStation', 2004, 'P', '#5d6e99', 'ppsspp'),

  system({ id: 'switch2', name: 'Nintendo Switch 2', family: 'Nintendo', year: 2025, symbol: 'NS2', color: '#bd1932', status: 'reference', emulators: [] }),
  system({ id: 'switch', name: 'Nintendo Switch', family: 'Nintendo', year: 2017, symbol: 'NS', color: '#ad5361', status: 'app', emulators: ['ProsperoEden'] }),
  system({ id: 'wiiu', name: 'Wii U', family: 'Nintendo', year: 2012, symbol: 'U', color: '#568aa8', status: 'app', emulators: ['PS5CEMU-HAR'] }),
  ra('wii', 'Wii', 'Nintendo', 2006, 'W', '#537e96', 'dolphin'),
  ra('gamecube', 'GameCube', 'Nintendo', 2001, 'GC', '#6957a2', 'dolphin'),
  ra('n64', 'Nintendo 64', 'Nintendo', 1996, '64', '#4b7b5e', 'mupen64plus_next', { status: 'tested', games: ['Super Mario 64', 'Mario Kart 64', 'Mario Party', 'Mario Party 2', 'Mario Party 3'] }),
  ra('3ds', 'Nintendo 3DS', 'Nintendo', 2011, '3D', '#a45165', 'azahar', { emulators: ['PS5CEMU-HAR', 'RetroArch · Azahar'] }),
  ra('ds', 'Nintendo DS', 'Nintendo', 2004, 'DS', '#7088b5', 'melondsds', { emulators: ['PS5CEMU-HAR', 'RetroArch · melonDS DS'] }),
  ra('gba', 'Game Boy Advance', 'Nintendo', 2001, 'GBA', '#6c6baa', 'mgba'),
  ra('gbc', 'Game Boy Color', 'Nintendo', 1998, 'GBC', '#7d7eb5', 'mgba'),
  ra('gb', 'Game Boy', 'Nintendo', 1989, 'GB', '#9a9290', 'mgba'),
  ra('snes', 'Super Nintendo', 'Nintendo', 1990, 'SN', '#7560a1', 'snes9x'),
  ra('nes', 'NES', 'Nintendo', 1983, 'N', '#a46555', 'fceumm'),
  ra('fds', 'Famicom Disk System', 'Nintendo', 1986, 'FDS', '#b67065', 'fceumm'),
  ra('virtualboy', 'Virtual Boy', 'Nintendo', 1995, 'VB', '#a23842', 'mednafen_vb'),
  ra('pokemonmini', 'Pokémon mini', 'Nintendo', 2001, 'PM', '#7a91ad', 'pokemini'),

  ra('dreamcast', 'Dreamcast', 'Sega', 1998, 'DC', '#9c694f', 'flycast'),
  ra('saturn', 'Sega Saturn', 'Sega', 1994, 'SS', '#646b8e', 'mednafen_saturn'),
  ra('naomi', 'Sega NAOMI', 'Sega', 1998, 'NM', '#66868b', 'flycast'),
  ra('genesis', 'Mega Drive / Genesis', 'Sega', 1988, 'MD', '#5b647e', 'genesis_plus_gx'),
  ra('segacd', 'Sega CD / Mega-CD', 'Sega', 1991, 'CD', '#516c8c', 'genesis_plus_gx'),
  ra('sega32x', 'Sega 32X', 'Sega', 1994, '32X', '#676783', 'picodrive'),
  ra('mastersystem', 'Master System', 'Sega', 1985, 'MS', '#69718a', 'genesis_plus_gx'),
  ra('gamegear', 'Game Gear', 'Sega', 1990, 'GG', '#737793', 'genesis_plus_gx'),
  ra('sg1000', 'SG-1000', 'Sega', 1983, 'SG', '#797c8c', 'genesis_plus_gx'),
  ra('atomiswave', 'Atomiswave', 'Sammy', 2003, 'AW', '#94796b', 'flycast'),

  ra('atarijaguar', 'Atari Jaguar', 'Atari', 1993, 'J', '#8e5d65', 'virtualjaguar'),
  ra('atarilynx', 'Atari Lynx', 'Atari', 1989, 'LX', '#777b82', 'handy'),
  ra('atari7800', 'Atari 7800', 'Atari', 1984, '78', '#a26955', 'prosystem'),
  ra('atari5200', 'Atari 5200', 'Atari', 1982, '52', '#8d745b', 'a5200'),
  ra('atari2600', 'Atari 2600', 'Atari', 1977, '26', '#a66e51', 'stella'),

  ra('pcengine', 'PC Engine / TurboGrafx-16', 'NEC', 1987, 'PCE', '#9a7484', 'mednafen_pce'),
  ra('supergrafx', 'SuperGrafx', 'NEC', 1989, 'SGX', '#7b7791', 'mednafen_pce'),
  ra('pcfx', 'PC-FX', 'NEC', 1994, 'PFX', '#9c8b84', 'mednafen_pcfx'),

  ra('neogeocd', 'Neo Geo CD', 'SNK', 1994, 'NG', '#888398', 'neocd'),
  ra('neogeopocketcolor', 'Neo Geo Pocket Color', 'SNK', 1999, 'NGC', '#718c88', 'mednafen_ngp'),
  ra('neogeopocket', 'Neo Geo Pocket', 'SNK', 1998, 'NGP', '#777c92', 'mednafen_ngp'),

  ra('wonderswancolor', 'WonderSwan Color', 'Bandai', 2000, 'WSC', '#8e8bac', 'mednafen_wswan'),
  ra('wonderswan', 'WonderSwan', 'Bandai', 1999, 'WS', '#888990', 'mednafen_wswan'),
  ra('threedo', '3DO Interactive Multiplayer', '3DO', 1993, '3DO', '#8b8f9f', 'opera'),

  ra('arcadefbneo', 'Arcade · FinalBurn Neo', 'Arcade', null, 'FB', '#8d6086', 'fbneo'),
  ra('arcademame', 'Arcade · MAME', 'Arcade', null, 'MA', '#81719b', 'mame'),
  ra('amiga', 'Amiga', 'Commodore', 1985, 'A', '#90919b', 'puae'),
  ra('c64', 'Commodore 64', 'Commodore', 1982, '64', '#a69184', 'vice_x64sc'),
  ra('dos', 'DOS PC games', 'Computer', null, 'DOS', '#718892', 'dosbox_pure'),
  ra('scummvm', 'ScummVM adventures', 'Computer', null, 'SC', '#947c9c', 'scummvm')
];

if (typeof window !== 'undefined') window.shelfSystems = shelfSystems;
if (typeof module !== 'undefined') module.exports = shelfSystems;
