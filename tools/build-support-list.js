const fs = require('node:fs');
const path = require('node:path');
const systems = require('../systems-data.js');

const labels = {
  native: 'Built-in PS5 route',
  tested: 'A game reached gameplay in the demo snapshot',
  mapped: 'RetroArch core mapped in the demo snapshot',
  app: 'Dedicated app recorded in the demo snapshot',
  reference: 'Hardware reference only'
};
const clean = value => String(value).replaceAll('|', '\\|');
const rows = systems.map(system => `| ${clean(system.name)} | ${clean(system.family)} | ${labels[system.status]} | ${clean(system.emulators.join(', ') || 'None')} |`).join('\n');

const markdown = `# Demo system catalogue

This is the **static demonstration inventory** bundled with the PC preview, dated 24 October 2026. It is not a live PS5 scan or a compatibility list for every console. An installed core is only a route to try; it does not establish that a game runs well or that required BIOS files exist. Individual game titles in the preview are examples.

The demo contains ${systems.length} system cards, including ${systems.filter(system => system.core).length} RetroArch core mappings. Nintendo Switch 2 is a hardware reference only; it has no verified PS5 emulator route and its launch controls are disabled. The preview does not contain a PS6 card because no official hardware design was verified.

| System / platform | Brand | Demo evidence | Route shown in Shelf |
| --- | --- | --- | --- |
${rows}

## Sources and limits

The catalogue was assembled from one PS5 setup snapshot. The upstream projects used to identify possible routes include [PS5 RetroArch](https://github.com/mihawk-99/PS5_RetroArch), [PS5-ECAM](https://github.com/premohq/PS5-ECAM), [PS5SX2](https://github.com/Swordpdf/PS5SX2), [PSXS5](https://github.com/SynoPiia/PSXS5), and [ProsperoEden](https://github.com/Swordpdf/ProsperoEden). The app also references [Sony's PS4 backward compatibility guidance](https://www.playstation.com/en-ph/support/games/ps5-backward-compatibility-games/) and [Nintendo's official Switch 2 hardware photo](https://nintendoes-newsroom.prgloo.com/resources/nintendo-switch-2). See [project credits](../THIRD_PARTY.md) and [image credits](../assets/CREDITS.md).

The future PS5 build should populate route and game availability from the owner's console. This PC preview only demonstrates the filter against the bundled snapshot.
`;

const target = path.join(__dirname, '..', 'docs', 'SYSTEMS.md');
fs.mkdirSync(path.dirname(target), { recursive: true });
fs.writeFileSync(target, markdown);
