import test from 'node:test';import assert from 'node:assert/strict';
import {createHandler,UPSTREAM} from './worker.mjs';
const base='https://houseofexp.com/research/cp-100';
test('canonical slash redirect preserves query',async()=>{const r=await createHandler()(new Request(base+'?utm_source=team'),{});assert.equal(r.status,308);assert.equal(r.headers.get('location'),base+'/?utm_source=team');});
test('module, PDF and root paths map to the fixed upstream and strip credentials',async()=>{
 for(const file of ['','app.js','dsp.js','CP-100-Guide.pdf']){
  let seen;const r=await createHandler(async(url,options)=>{seen={url:String(url),options};return new Response('content',{headers:{'set-cookie':'remote=value','content-type':file.endsWith('.pdf')?'application/pdf':'text/javascript'}});})(new Request(base+'/'+file+'?v=5',{headers:{Cookie:'hox_admin=private',Authorization:'Bearer private','Cf-Access-Jwt-Assertion':'private',Range:'bytes=0-127'}}),{});
  assert.equal(seen.url,UPSTREAM+'/'+file+'?v=5');for(const k of ['cookie','authorization','cf-access-jwt-assertion'])assert.equal(seen.options.headers.has(k),false);assert.equal(seen.options.headers.get('range'),'bytes=0-127');assert.equal(r.headers.has('set-cookie'),false);assert.equal(await r.text(),'content');
 }
});
test('simulator redirect stays under custom prefix; external redirect stays external',async()=>{
 for(const [location,expected] of [['/app.js',base+'/app.js'],[UPSTREAM+'/app.js',base+'/app.js'],['https://example.com/','https://example.com/']]){
  const r=await createHandler(async()=>new Response(null,{status:302,headers:{location}}))(new Request(base+'/'),{});assert.equal(r.headers.get('location'),expected);
 }
});
test('sibling slug and main-site requests fall through unchanged',async()=>{for(const path of ['/research/cp-100-notes','/research','/']){let seen;const request=new Request('https://houseofexp.com'+path);const r=await createHandler()(request,{WEBSITE:{fetch:r=>{seen=r;return new Response('main site');}}});assert.equal(seen,request);assert.equal(await r.text(),'main site');}});
test('write requests are rejected without forwarding; HEAD has no body',async()=>{let calls=0;const handle=createHandler(async()=>{calls++;return new Response('body');});assert.equal((await handle(new Request(base+'/',{method:'POST'}),{})).status,405);assert.equal(calls,0);const r=await handle(new Request(base+'/app.js',{method:'HEAD'}),{});assert.equal(await r.text(),'');});
test('upstream failure returns uncached 502',async()=>{const r=await createHandler(async()=>{throw Error('offline');})(new Request(base+'/'),{});assert.equal(r.status,502);assert.equal(r.headers.get('cache-control'),'no-store');});
