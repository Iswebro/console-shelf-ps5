const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const systems = require('../systems-data.js');

const root = path.resolve(__dirname, '..');
const target = path.join(root, 'assets', '3d-icons', 'manifest.json');
const prior = fs.existsSync(target) ? JSON.parse(fs.readFileSync(target, 'utf8')) : [];
const knownReferences = new Map(prior.map(entry => [entry.id, entry.reference]));
const officialSwitch2 = 'https://nintendoes-newsroom.prgloo.com/resources/nintendo-switch-2';

const manifest = systems.map(system => {
  const local = system.image;
  const data = fs.readFileSync(path.join(root, local));
  const reference = system.id === 'switch2' ? officialSwitch2 : (knownReferences.get(system.id) || null);
  return {
    id: system.id,
    local,
    bytes: data.length,
    sha256: crypto.createHash('sha256').update(data).digest('hex').toUpperCase(),
    reference,
    referenceType: system.id === 'switch2' ? 'official Nintendo press image' : reference ? 'credited hardware photo' : 'general hardware features and existing Shelf icon style'
  };
});

fs.writeFileSync(target, JSON.stringify(manifest, null, 2) + '\n');
