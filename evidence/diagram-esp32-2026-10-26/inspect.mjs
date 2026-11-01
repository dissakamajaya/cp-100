import {writeFileSync} from 'node:fs';
const directory='/Users/0xtx/Documents/GitHub/cp-100/evidence/diagram-esp32-2026-10-26';
export default async({page,saveScreenshot})=>{
 await page.goto('https://claude.ai/artifact/XsySKHHtZkEHcpqHidqYKs');
 await page.waitForLoadState('load');
 const snapshot=(await page.snapshot()).formattedTree;
 const info=await page.evaluate(()=>({url:location.href,title:document.title,frames:[...document.querySelectorAll('iframe')].map(f=>({src:f.src,title:f.title})),links:[...document.querySelectorAll('a')].map(a=>({text:a.textContent,url:a.href})),text:document.body.innerText.slice(0,12000)}));
 writeFileSync(`${directory}/artifact-inspect.json`,JSON.stringify({snapshot,info},null,2));await saveScreenshot(`${directory}/artifact.png`);
 return {snapshot,info};
};
