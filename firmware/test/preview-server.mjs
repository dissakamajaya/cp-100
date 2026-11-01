// UI QA only. The child runs the same C++ domain engine/protocol as the ESP32.
// This is not ESP-IDF HTTP/radio/DAC emulation and is bound to loopback only.
import {createServer} from 'node:http';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {randomBytes} from 'node:crypto';
const binary=process.env.CP100_PREVIEW_BIN||'/tmp/cp100-remote-preview';
const driver=spawn(binary,[],{stdio:['pipe','pipe','inherit']});
let pending=[],token='';
createInterface({input:driver.stdout}).on('line',line=>{const resolve=pending.shift();if(resolve)resolve(JSON.parse(line));});
let pipeline=Promise.resolve();
const call=line=>{const task=pipeline.then(()=>new Promise(resolve=>{pending.push(resolve);driver.stdin.write(line+'\n');}));pipeline=task.catch(()=>{});return task;};
const web=new URL('../web/',import.meta.url);
const server=createServer(async(req,res)=>{
 try{
  if(req.method==='GET'&&['/','/app.js','/style.css'].includes(req.url)){
   const file=req.url==='/'?'index.html':req.url.slice(1);
   res.writeHead(200,{'Content-Type':file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':'text/html','Cache-Control':'no-store'});res.end(await readFile(new URL(file,web)));return;
  }
  let data,status=200;
  if(req.method==='GET'&&req.url==='/api/state')data=(await call('!state')).state;
  else if(req.method==='POST'&&req.url==='/api/session'&&req.headers['x-cp100-client']==='1'){
   data=await call('!claim');if(data.ok){token=randomBytes(16).toString('hex');data.token=token;}else{status=409;data.message='Panel sedang digunakan.';}
  }else if(req.method==='POST'&&req.url==='/api/action'){
   if(!token||req.headers['x-cp100-session']!==token||req.headers['x-cp100-client']!=='1'){status=403;data={ok:false,message:'Sesi tidak valid.'};}
   else{
    let body='';for await(const chunk of req){body+=chunk;if(body.length>4096)break;}
    data=await call(body);if(!data.ok){status=409;data.message='Perintah ditolak atau sesi berakhir.';}
   }
  }else{status=404;data={ok:false,message:'Not found'};}
  res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));
 }catch(error){res.writeHead(500);res.end(JSON.stringify({ok:false,message:error.message}));}
});
server.listen(8765,'127.0.0.1',()=>console.log('QA preview http://127.0.0.1:8765; C++ controller; NO HARDWARE'));
process.on('SIGINT',()=>{driver.kill();server.close();process.exit(0);});
process.on('SIGTERM',()=>{driver.kill();server.close();process.exit(0);});
