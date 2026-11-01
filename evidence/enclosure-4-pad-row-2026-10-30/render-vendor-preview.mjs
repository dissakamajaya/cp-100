export default async ({page,saveScreenshot}) => {
  await page.setViewportSize(2400,1800);
  await page.goto('http://127.0.0.1:5181/vendor-preview.html');
  await page.snapshot();
  const result=await page.evaluate(async()=>{
    const end=Date.now()+20000;
    while(!window.previewReady && Date.now()<end) await new Promise(r=>setTimeout(r,100));
    if(!window.previewReady) return {errors:window.renderErrors,title:document.title,body:document.body.innerText.slice(0,1000)};
    await document.fonts.ready;
    return window.previewReady;
  });
  if(result.triangles!==7884) throw new Error(JSON.stringify(result));
  const file='/Users/0xtx/Documents/GitHub/cp-100/enclosure/print-ready/CP-100-4-pad-vendor-preview.png';
  await saveScreenshot(file);
  return {file,...result};
};
