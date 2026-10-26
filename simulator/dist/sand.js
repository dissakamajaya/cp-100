import { period } from './dsp.js';
import { SPEC } from './model.js';
export const SCENE = Object.freeze({windowHalf:SPEC.glassDiameterCm/(2*SPEC.plateCm),radiusFraction:.47*SPEC.glassDiameterCm/SPEC.plateCm});
const SIZE=81, COUNT=4400;
const MODES=[[2,4,1],[2,4,-1],[2,6,1],[4,6,-1],[4,6,1],[2,8,-1],[4,8,1],[6,8,-1],[6,8,1],[4,10,-1],[6,10,1],[8,10,-1],[8,12,1],[10,12,-1],[10,14,1],[12,14,-1]];
// A visual nodal-field model, not an elasticity solver or a calibrated plate.
function modesFor(frequency) {
  const position=Math.max(0,Math.min(MODES.length-1.001,Math.log2(Math.max(20,frequency)/35)*1.5));
  const index=Math.floor(position), t=position-index, mix=t*t*(3-2*t);
  return [{pair:MODES[index],weight:1-mix},{pair:MODES[index+1],weight:mix}];
}
function harmonicWeights(p) {
  const signal=period({...p,levelPct:100},512), result=[];
  for(let n=1;n<=7;n++){
    let re=0,im=0;
    for(let i=0;i<signal.length;i++){const angle=2*Math.PI*n*i/signal.length;re+=signal[i]*Math.cos(angle);im+=signal[i]*Math.sin(angle);}
    result.push({n,amplitude:Math.hypot(re,im)*2/signal.length});
  }
  const max=Math.max(...result.map(x=>x.amplitude),.001);
  return result.filter(x=>x.amplitude/max>.08).sort((a,b)=>b.amplitude-a.amplitude).slice(0,4).map(x=>({...x,weight:x.amplitude/max*(x.n===1?1:.65)}));
}
export class SandPreview {
  constructor(canvas) {
    canvas.parentElement.style.setProperty('--shoe-width',`${SCENE.radiusFraction*2*.51*100}%`);
    this.canvas=canvas;this.ctx=canvas.getContext('2d');this.field=new Float32Array(SIZE*SIZE);
    this.gx=new Float32Array(SIZE*SIZE);this.gy=new Float32Array(SIZE*SIZE);
    this.tx=new Float32Array(COUNT);this.ty=new Float32Array(COUNT);
    this.x=new Float32Array(COUNT);this.y=new Float32Array(COUNT);this.vx=new Float32Array(COUNT);this.vy=new Float32Array(COUNT);
    this.seed=86173;this.version=0;this.ready=false;this.lastField=0;this.lastDraw=0;this.spectrumKey='';
    this.reduceMotion=matchMedia('(prefers-reduced-motion: reduce)');
    this.resize=new ResizeObserver(()=>{this.draw();});this.resize.observe(canvas);
    this.frame=this.frame.bind(this);requestAnimationFrame(this.frame);
  }
  random(){this.seed=(Math.imul(this.seed,1664525)+1013904223)>>>0;return this.seed/4294967296;}
  setState(state){const p=state.parameters,key=`${p.frequencyHz}:${p.wave}:${p.foldPct}:${p.harmonics}`;this.dirty=this.dirty||key!==this.targetKey;this.targetKey=key;this.parameters={...p};this.moving=!state.muted&&p.levelPct>0;}
  updateField(){
    const p=this.parameters,key=`${p.wave}:${p.foldPct}:${p.harmonics}`;
    if(key!==this.spectrumKey){this.weights=harmonicWeights(p);this.spectrumKey=key;}
    this.field.fill(0);this.gx.fill(0);this.gy.fill(0);
    for(const h of this.weights)for(const mode of modesFor(p.frequencyHz*h.n)){
      const [m,n,sign]=mode.pair,weight=mode.weight*h.weight;
      const cm=[],cn=[],sm=[],sn=[];
      for(let i=0;i<SIZE;i++){
        const plateCoordinate=.5+((i/(SIZE-1))*2-1)*SCENE.windowHalf;
        cm.push(Math.cos(m*Math.PI*plateCoordinate));cn.push(Math.cos(n*Math.PI*plateCoordinate));
        sm.push(Math.sin(m*Math.PI*plateCoordinate));sn.push(Math.sin(n*Math.PI*plateCoordinate));
      }
      for(let y=0;y<SIZE;y++)for(let x=0;x<SIZE;x++){
        const i=y*SIZE+x;
        this.field[i]+=weight*(cm[x]*cn[y]+sign*cn[x]*cm[y]);
        this.gx[i]+=weight*Math.PI*SCENE.windowHalf*(-m*sm[x]*cn[y]-sign*n*sn[x]*cm[y]);
        this.gy[i]+=weight*Math.PI*SCENE.windowHalf*(-n*cm[x]*sn[y]-sign*m*cn[x]*sm[y]);
      }
    }
    this.version++;this.dirty=false;
    if(this.ready)this.targetSand();
  }
  sample(array,x,y){
    const xx=(x+1)*.5*(SIZE-1), yy=(y+1)*.5*(SIZE-1),i=Math.max(0,Math.min(SIZE-2,Math.floor(xx))),j=Math.max(0,Math.min(SIZE-2,Math.floor(yy)));
    const a=xx-i,b=yy-j,k=j*SIZE+i;
    return (array[k]*(1-a)+array[k+1]*a)*(1-b)+(array[k+SIZE]*(1-a)+array[k+SIZE+1]*a)*b;
  }
  targetSand(){
    // Sample a finite-width nodal band. Point attraction to field=0 collapses
    // grains into hairlines and jams the rim; this illustrative model retains
    // granular width and redistributes the same grains when the field changes.
    this.tx ||= new Float32Array(COUNT);this.ty ||= new Float32Array(COUNT);
    const cdf=new Float64Array(SIZE*SIZE);let total=0;
    for(let j=0;j<SIZE;j++)for(let i=0;i<SIZE;i++){
      const k=j*SIZE+i,x=i/(SIZE-1)*2-1,y=j/(SIZE-1)*2-1;
      if(x*x+y*y<.95*.95){
        const gradient=Math.hypot(this.gx[k],this.gy[k]);
        const distance=Math.abs(this.field[k])/Math.max(.15,gradient);
        total+=.006+Math.exp(-.5*(distance/.022)**2);
      }
      cdf[k]=total;
    }
    // Repeatable sampling keeps the preview stable for unchanged settings.
    const seed=this.seed;this.seed=86173;
    for(let i=0;i<COUNT;i++){
      const pick=this.random()*total;let lo=0,hi=cdf.length-1;
      while(lo<hi){const mid=(lo+hi)>>>1;if(cdf[mid]<=pick)lo=mid+1;else hi=mid;}
      this.tx[i]=((lo%SIZE)+this.random()-.5)/(SIZE-1)*2-1;
      this.ty[i]=(Math.floor(lo/SIZE)+this.random()-.5)/(SIZE-1)*2-1;
    }
    this.seed=seed;
  }
  seedSand(){
    this.targetSand();this.x.set(this.tx);this.y.set(this.ty);
    this.vx.fill(0);this.vy.fill(0);this.ready=true;
  }
  step(time,dt){
    const drive=this.parameters.levelPct/100,rate=Math.min(2,dt/30);
    for(let i=0;i<COUNT;i++){
      const agitation=.0012*drive;
      const tx=this.tx[i]+Math.sin(i*1.79+time*.003)*agitation;
      const ty=this.ty[i]+Math.cos(i*2.13+time*.002)*agitation;
      const blend=1-Math.exp(-rate*(.045+.07*drive));
      this.x[i]+=(tx-this.x[i])*blend;this.y[i]+=(ty-this.y[i])*blend;
    }
  }
  draw(){
    const w=this.canvas.clientWidth;if(!w)return;
    const ratio=Math.min(devicePixelRatio||1,1.5),size=Math.round(w*ratio);
    if(this.canvas.width!==size||this.canvas.height!==size){this.canvas.width=size;this.canvas.height=size;}
    const ctx=this.ctx;ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,w,w);
    const c=w*.5,r=w*SCENE.radiusFraction;
    ctx.fillStyle='#e8ebe0';ctx.fillRect(0,0,w,w);
    ctx.strokeStyle='#cdd3c1';ctx.lineWidth=.75;ctx.strokeRect(w*.03,w*.03,w*.94,w*.94);
    ctx.fillStyle='#29352c';ctx.beginPath();ctx.arc(c,c,r,0,Math.PI*2);ctx.fill();
    if(this.ready){
      const dot=Math.max(.55,w*.0012);
      // Fixed colour batches avoid allocating objects or paths per grain.
      for(let batch=0;batch<3;batch++){
        ctx.fillStyle=['#a9956d','#c7b78e','#e4d4aa'][batch];
        for(let i=batch;i<COUNT;i+=3)ctx.fillRect(c+this.x[i]*r,c+this.y[i]*r,dot,dot);
      }
    }
    ctx.strokeStyle='#86957c';ctx.lineWidth=1.1;ctx.beginPath();ctx.arc(c,c,r,0,Math.PI*2);ctx.stroke();
    ctx.strokeStyle='#c6cebb';ctx.lineWidth=.7;ctx.beginPath();ctx.arc(c,c,r+4,0,Math.PI*2);ctx.stroke();
    ctx.strokeStyle='#bac4ae';ctx.lineWidth=.7;
    for(const a of [.03,.97])for(const b of [.03,.97]){const x=w*a,y=w*b;ctx.beginPath();ctx.moveTo(x-3,y);ctx.lineTo(x+3,y);ctx.moveTo(x,y-3);ctx.lineTo(x,y+3);ctx.stroke();}
  }
  frame(time){
    requestAnimationFrame(this.frame);
    if(!this.parameters||document.hidden)return;
    if(this.dirty&&time-this.lastField>85){this.updateField();this.lastField=time;if(!this.ready||this.reduceMotion.matches)this.seedSand();this.needsPaint=true;}
    const dt=time-this.lastDraw;
    if(dt<30)return;
    if(this.moving&&!this.reduceMotion.matches&&this.ready){this.step(time,dt);this.needsPaint=true;}
    if(this.needsPaint){this.draw();this.needsPaint=false;}
    this.lastDraw=time;
  }
  diagnostics(){
    let hash=0;for(let i=0;i<60;i++)hash+=(this.x[i]+this.y[i])*(i+1);
    return {particles:COUNT,fieldVersion:this.version,moving:this.moving,positionHash:Number(hash.toFixed(5)),frequencyHz:this.parameters?.frequencyHz};
  }
}
