const { chromium } = require('playwright');
const { pathToFileURL } = require('node:url');
const path = require('node:path');
const fs = require('node:fs');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const systems = require('../systems-data.js');
const manifest = require('../assets/3d-icons/manifest.json');

(async () => {
  const root = path.resolve(__dirname, '..');
  const capturePreviews = process.env.SHELF_CAPTURE_PREVIEWS === '1';
  assert.equal(manifest.length, 52);
  for (const entry of manifest) {
    const data = fs.readFileSync(path.join(root, entry.local));
    assert.equal(data.length, entry.bytes, `${entry.id} byte count`);
    assert.equal(crypto.createHash('sha256').update(data).digest('hex').toUpperCase(), entry.sha256, `${entry.id} SHA-256`);
  }
  const referenceManifestPath = path.join(root, 'assets', 'references', 'manifest.json');
  if (fs.existsSync(referenceManifestPath)) {
    const switch2Reference = JSON.parse(fs.readFileSync(referenceManifestPath, 'utf8'));
    const pressPath = path.join(root, switch2Reference.local);
    const pressImage = fs.readFileSync(pressPath);
    assert.equal(pressImage.length, switch2Reference.bytes);
    assert.equal(crypto.createHash('sha256').update(pressImage).digest('hex').toUpperCase(), switch2Reference.sha256);
  }
  const edge = process.platform === 'win32' && [
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    'C:/Program Files/Microsoft/Edge/Application/msedge.exe'
  ].find(candidate => fs.existsSync(candidate));
  const browser = await chromium.launch(edge ? { headless: true, executablePath: edge } : { headless: true });
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 }, deviceScaleFactor: 1 });
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(pathToFileURL(path.join(root, 'index.html')).href);
  await page.locator('.card').first().waitFor();
  assert.equal(systems.length, 52);
  assert.equal(systems.filter(system => system.core).length, 46);
  assert.equal(await page.locator('.card').count(), 52);
  await page.click('#scopeAvailable');
  assert.equal(await page.locator('.card').count(), 51, 'demo inventory should hide unsupported Switch 2');
  assert.equal(await page.locator('.card[data-system="switch2"]').count(), 0);
  await page.reload();
  assert.equal(await page.locator('#scopeAvailable').getAttribute('aria-pressed'), 'true');
  assert.equal(await page.locator('.card').count(), 51);
  await page.click('#scopeAll');
  assert.equal(await page.locator('.card').count(), 52);
  assert.ok((await page.locator('.card[data-system="switch2"] .console-art img').getAttribute('src')).endsWith('switch2.png'));
  await page.locator('.console-art img').first().waitFor({ state: 'visible' });
  const broken = await page.locator('.console-art img').evaluateAll(async images => {
    const results = await Promise.all(images.map(image => new Promise(resolve => {
      const check = new Image();
      check.onload = () => resolve(null);
      check.onerror = () => resolve(image.src);
      check.src = image.src;
    })));
    return results.filter(Boolean);
  });
  assert.deepEqual(broken, [], 'all 52 system images should load');
  if (capturePreviews) await page.locator('.card[data-system="switch2"]').screenshot({ path: path.join(root, 'previews', 'switch2-3d-card.png') });
  if (capturePreviews) await page.screenshot({ path: path.join(root, 'previews', 'console-images-grid.png'), fullPage: false });
  await page.evaluate(() => window.scrollTo(0, 570));
  if (capturePreviews) await page.screenshot({ path: path.join(root, 'previews', 'console-images-gallery.png'), fullPage: false });
  await page.evaluate(() => window.scrollTo(0, 0));

  await page.selectOption('#sort', 'az');
  assert.equal(await page.locator('.card-title').first().textContent(), '3DO Interactive Multiplayer');
  await page.selectOption('#sort', 'newest');
  assert.equal(await page.locator('.card-title').first().textContent(), 'Nintendo Switch 2');
  await page.selectOption('#sort', 'oldest');
  assert.equal(await page.locator('.card-title').first().textContent(), 'Atari 2600');
  await page.selectOption('#sort', 'featured');
  await page.locator('.card[data-system="ps2"] .favorite').click();
  await page.selectOption('#sort', 'favorites');
  assert.equal(await page.locator('.card-title').first().textContent(), 'PlayStation 2');
  await page.selectOption('#family', 'Nintendo');
  assert.equal(await page.locator('.card').count(), 16);
  await page.selectOption('#family', 'all');
  await page.selectOption('#availability', 'mapped');
  assert.equal(await page.locator('.card').count(), 43);
  await page.selectOption('#family', 'Atari');
  assert.equal(await page.locator('.card').count(), 5);
  await page.locator('#toast').evaluate(element => element.classList.remove('show'));
  if (capturePreviews) await page.locator('#systems').screenshot({ path: path.join(root, 'previews', 'atari-expanded-icons.png') });
  await page.selectOption('#availability', 'all');
  await page.selectOption('#family', 'all');
  await page.fill('#search', 'mednafen_wswan');
  assert.equal(await page.locator('.card').count(), 2);
  await page.fill('#search', '');

  await page.click('#lookCards');
  assert.equal(await page.locator('.classic-card').count(), 52);
  await page.click('#layoutCarousel');
  assert.equal(await page.locator('.systems-carousel').count(), 1);
  assert.equal(await page.locator('.icon-card').count(), 52, 'swipe view always uses icons');
  assert.equal(await page.locator('#lookCards').isDisabled(), true, 'grid look control is disabled in swipe view');
  const floating = await page.locator('.systems-carousel .card').first().evaluate(element => {
    const card = getComputedStyle(element);
    const art = getComputedStyle(element.querySelector('.console-art'));
    return { background: card.backgroundImage, border: card.borderTopWidth, shadow: card.boxShadow, artBackground: art.backgroundImage };
  });
  assert.deepEqual(floating, { background: 'none', border: '0px', shadow: 'none', artBackground: 'none' });
  await page.locator('#toast').evaluate(element => element.classList.remove('show'));
  await page.evaluate(() => window.scrollTo(0, 620));
  if (capturePreviews) await page.screenshot({ path: path.join(root, 'previews', 'console-images-swipe.png'), fullPage: false });
  await page.click('#carouselNext');
  await page.waitForTimeout(550);
  assert.ok(await page.locator('#systems').evaluate(element => element.scrollLeft) > 0, 'carousel should advance');

  await page.reload();
  assert.equal(await page.locator('#lookCards').getAttribute('aria-pressed'), 'true');
  assert.equal(await page.locator('#layoutCarousel').getAttribute('aria-pressed'), 'true');
  assert.equal(await page.locator('.icon-card').count(), 52);
  assert.equal(await page.locator('#sort').inputValue(), 'favorites');
  assert.equal(await page.locator('.card-title').first().textContent(), 'PlayStation 2');
  await page.click('#layoutGrid');
  await page.click('#lookImages');
  await page.locator('.card[data-system="ps2"] .card-main').click();
  await page.click('#showLibrary');
  assert.equal(await page.locator('.game').count(), 3);
  if (capturePreviews) await page.screenshot({ path: path.join(root, 'previews', 'playstation-2-detail.png'), fullPage: false });
  await page.click('#back');
  await page.locator('.card[data-system="switch2"] .card-main').click();
  assert.ok((await page.locator('#heroImage').getAttribute('src')).endsWith('switch2.png'));
  assert.equal(await page.locator('#openEmulator').isDisabled(), true);
  assert.equal(await page.locator('#showLibrary').isDisabled(), true);
  assert.equal(await page.locator('#detailStatus').textContent(), 'Hardware reference only');
  if (capturePreviews) await page.screenshot({ path: path.join(root, 'previews', 'switch2-reference-detail.png'), fullPage: false });
  await page.click('#back');
  await page.setViewportSize({ width: 390, height: 844 });
  if (capturePreviews) await page.screenshot({ path: path.join(root, 'previews', 'mobile-grid.png'), fullPage: false });
  await page.click('#layoutCarousel');
  if (capturePreviews) await page.locator('#systems').screenshot({ path: path.join(root, 'previews', 'mobile-swipe.png') });
  const width = await page.evaluate(() => ({ page: document.documentElement.scrollWidth, viewport: innerWidth }));
  assert.ok(width.page <= width.viewport + 1, `mobile view should not overflow: ${JSON.stringify(width)}`);
  assert.deepEqual(errors, [], 'the page should have no JavaScript errors');
  await browser.close();
  console.log('PASS: 52 images, 46 RetroArch mappings, status labels/filter, unsupported Switch 2, view modes, swipe, sorting, favorites, persistence, and PS2 library.');
})().catch(error => { console.error(error); process.exitCode = 1; });
