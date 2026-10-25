// SPDX-License-Identifier: GPL-3.0-only
// Render the requested vector logo and its PS5 home-screen composition.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '..');
const backgrounds = path.join(root, 'assets/backgrounds');
const branding = path.join(root, 'assets/branding');
const vector = fs.readFileSync(path.join(branding, 'shelf-gaming-mark.svg'), 'utf8');
const iconOnly = process.argv.slice(2).includes('--icon-only');
if (process.argv.slice(2).some(value => value !== '--icon-only')) throw Error('Unknown artwork option');
const iconMark = vector.replace('<svg ', '<svg x="132" y="116" ').replace('width="1656" height="1728"', 'width="760" height="792"');
const icon = `<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024" role="img" aria-labelledby="iconTitle">
  <title id="iconTitle">isShelf home-screen icon</title>
  <defs><linearGradient id="home-tile-bg" x2="0" y2="1"><stop stop-color="#040B14"/><stop offset="1" stop-color="#0A1926"/></linearGradient></defs>
  <rect width="1024" height="1024" fill="url(#home-tile-bg)"/>
  ${iconMark}
</svg>\n`;
const plate = fs.readFileSync(path.join(backgrounds, 'shelf-ps5-home-v2-plate.png'));
const placedMark = vector.replace('<svg ', '<svg x="2200" y="500" ').replace('width="1656" height="1728"', 'width="1160" height="1210"');
const composition = `<svg xmlns="http://www.w3.org/2000/svg" width="3840" height="2160" viewBox="0 0 3840 2160" role="img" aria-labelledby="artTitle artDesc">
  <title id="artTitle">isShelf — Glass and Light</title>
  <desc id="artDesc">PS5 home-screen artwork with a large transparent vector Shelf logo at the right, sculptural dark glass and mint light, and a quiet left side for the system interface.</desc>
  <defs>
    <filter id="hero-depth" x="-25%" y="-25%" width="150%" height="150%" color-interpolation-filters="sRGB">
      <feDropShadow dx="0" dy="14" stdDeviation="18" flood-color="#000B13" flood-opacity=".55"/>
      <feDropShadow dx="0" dy="0" stdDeviation="30" flood-color="#20E9E2" flood-opacity=".10"/>
    </filter>
  </defs>
  <rect width="3840" height="2160" fill="#030A13"/>
  <image href="data:image/png;base64,${plate.toString('base64')}" width="3840" height="2160" preserveAspectRatio="xMidYMid slice"/>
  <g filter="url(#hero-depth)">${placedMark}</g>
</svg>\n`;

async function render(page, svg, width, height, filename, transparent = false) {
  await page.setViewportSize({ width, height });
  await page.setContent(`<style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent}body>svg{display:block;width:100vw;height:100vh}</style>${svg}`);
  await page.evaluate(async () => {
    await Promise.all([...document.querySelectorAll('image')].map(node => {
      const image = new Image();
      image.src = node.getAttribute('href');
      return image.decode();
    }));
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
  });
  await page.screenshot({ path: filename, omitBackground: transparent });
}

(async () => {
  const edge = 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
  const browser = await chromium.launch({ headless: true, ...(fs.existsSync(edge) ? { executablePath: edge } : {}) });
  try {
    const page = await browser.newPage({ deviceScaleFactor: 1 });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    fs.writeFileSync(path.join(branding, 'isshelf-home-icon.svg'), icon);
    await render(page, icon, 1024, 1024, path.join(branding, 'isshelf-home-icon.png'));
    if (!iconOnly) {
      fs.writeFileSync(path.join(backgrounds, 'shelf-ps5-home-v2.svg'), composition);
      await render(page, vector, 1656, 1728, path.join(branding, 'shelf-gaming-mark.png'), true);
      await render(page, composition, 3840, 2160, path.join(backgrounds, 'shelf-ps5-home-v2.png'));
      await render(page, composition, 1920, 1080, path.join(root, 'previews/shelf-ps5-home-v2.png'));
    }
    if (errors.length) throw Error(errors.join('\n'));
    const identity = relative => {
      const raw = fs.readFileSync(path.join(root, relative));
      return { path: relative, bytes: raw.length, sha256: crypto.createHash('sha256').update(raw).digest('hex') };
    };
    const manifestPath = path.join(root, 'native/ps5/presentation-assets.json');
    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    const source = identity('assets/branding/isshelf-home-icon.png');
    manifest.icon = { source: source.path, width: 1024, height: 1024, bytes: source.bytes, sha256: source.sha256,
      vectorSource: identity('assets/branding/isshelf-home-icon.svg'),
      markSource: identity('assets/branding/shelf-gaming-mark.svg'),
      purpose: 'PS5 home tile from the vector mark; Vector is the in-app default alongside Gaming, Classic mint and Jade', hardwareTest: 'pending' };
    fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n');
    console.log(JSON.stringify({
      background: 'assets/backgrounds/shelf-ps5-home-v2.png',
      backgroundSize: [3840, 2160],
      logo: 'assets/branding/shelf-gaming-mark.svg',
      transparentLogo: 'assets/branding/shelf-gaming-mark.png',
      transparentLogoSize: [1656, 1728],
      homeIcon: manifest.icon,
      generatedPlateSize: [plate.readUInt32BE(16), plate.readUInt32BE(20)],
      renderErrors: errors
    }, null, 2));
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
