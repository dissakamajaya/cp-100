const dir = '/Users/0xtx/Documents/GitHub/cp-100/evidence/enclosure-4-pad-row-2026-10-30';
export default async ({ page, saveScreenshot }) => {
  await page.goto('http://127.0.0.1:5181/');
  await page.setViewportSize(1440, 1000);
  const status = await page.evaluate(async () => {
    const end = Date.now() + 20000;
    while (!/printed parts/.test(document.getElementById('status').textContent) && Date.now() < end) {
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
    return document.getElementById('status').textContent;
  });
  if (status !== '7 printed parts') throw new Error(`Geometry failed: ${status}`);
  const snapshot = await page.snapshot();
  await page.locator('[data-g="Pad controller"]').click();
  await page.locator('#explode').fill('0');
  await page.locator('[data-cam="front"]').click();
  await page.evaluate(() => new Promise((resolve) => setTimeout(resolve, 600)));
  await saveScreenshot(`${dir}/pad-front.png`);
  await page.locator('[data-cam="iso"]').click();
  await page.evaluate(() => new Promise((resolve) => setTimeout(resolve, 600)));
  await saveScreenshot(`${dir}/pad-assembled.png`);
  await page.locator('#explode').fill('0.35');
  await page.locator('[data-cam="iso"]').click();
  await page.evaluate(() => new Promise((resolve) => setTimeout(resolve, 600)));
  await saveScreenshot(`${dir}/pad-exploded.png`);
  await page.locator('[data-cam="back"]').click();
  await page.evaluate(() => new Promise((resolve) => setTimeout(resolve, 600)));
  await saveScreenshot(`${dir}/pad-underside.png`);
  const text = await page.evaluate(() => document.body.innerText);
  if (!text.includes('4 windows 41 x 41 R2 (1 row x 4 pads)') || !text.includes('×4')) {
    throw new Error('Preview does not describe four pads');
  }
  return { status, fourPadLabels: true, layout: "1 row x 4 pads", artifacts: [`${dir}/pad-front.png`, `${dir}/pad-assembled.png`, `${dir}/pad-exploded.png`, `${dir}/pad-underside.png`] };
};
