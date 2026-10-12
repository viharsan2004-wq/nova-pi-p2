#include "web_ui.h"

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NOVA-PI P2</title>
<style>
:root{--bg:#0f1418;--card:#1a2229;--fg:#e6edf3;--mut:#8b98a5;--ok:#2ea043;--bad:#da3633;--warn:#d29922;--acc:#388bfd}
*{box-sizing:border-box}body{margin:0 auto;max-width:760px;background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif;padding:10px}
.card{background:var(--card);border-radius:10px;padding:10px;margin-bottom:10px}
h1{font-size:18px;margin:0}h2{font-size:12px;color:var(--mut);margin:0 0 8px;text-transform:uppercase;letter-spacing:.06em}
.row{display:flex;gap:6px;flex-wrap:wrap;margin-top:6px;align-items:center}.grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}
button,select,input{font:inherit;border-radius:8px;border:0;padding:11px 8px;background:#2b333b;color:var(--fg);width:100%}
button{flex:1;font-weight:600}#bStop{background:var(--bad);font-size:19px;padding:18px}#bStart{background:var(--ok)}
label{font-size:12px;color:var(--mut);display:block}label input,label select{margin-top:3px}
.pill{padding:4px 9px;border-radius:99px;background:#2b333b;font-size:12px;font-weight:600;width:auto}
.ok{background:var(--ok)}.bad{background:var(--bad)}.warn{background:var(--warn);color:#000}
.det{font-size:26px;font-weight:800;text-align:center;padding:12px;border-radius:8px;background:#2b333b}.det.on{background:var(--ok)}
.bar{height:10px;background:#2b333b;border-radius:5px;margin:8px 0;overflow:hidden}#barf{height:100%;width:0;background:var(--acc)}
.ban{display:none;padding:8px;border-radius:8px;margin-bottom:10px;font-weight:600;text-align:center}
canvas{width:100%;height:130px;background:#12181d;border-radius:8px;display:block}
.v b{display:block;font-size:17px}.v{font-size:11px;color:var(--mut)}pre{margin:0;font-size:12px;white-space:pre-wrap;color:var(--mut)}
</style></head><body>
<div id="ban" class="ban"></div>
<div class="card"><div class="row" style="justify-content:space-between;margin:0"><h1>NOVA-PI P2</h1><span id="conn" class="pill bad">OFFLINE</span></div>
<div class="row"><span id="pMode" class="pill">IDLE</span><span id="pPulse" class="pill">PULSES OFF</span><span id="pCal" class="pill warn">NOT CALIBRATED</span><span id="pAdc" class="pill">ADC ?</span></div></div>

<div class="card"><div id="det" class="det">NO TARGET</div>
<div class="v" style="margin-top:6px">Target class: <b id="classify" style="display:inline">Unknown — classification not supported by current measurements</b></div>
<div class="v">Metal type is NOT identified. The decay curve, time constant and distance below are indicators only; they need your own labelled test targets to mean anything.</div>
<div class="bar"><div id="barf"></div></div>
<div class="v">Relative signal strength (100% = threshold): <b id="rel" style="display:inline">0</b>%</div>
<div class="grid" style="margin-top:8px">
<div class="v">Raw ADC (counts)<b id="raw">-</b></div><div class="v">Filtered<b id="filt">-</b></div>
<div class="v">Baseline<b id="base">-</b></div><div class="v">Delta (counts)<b id="delta">-</b></div>
<div class="v">Delta (sigma)<b id="sigma">-</b></div><div class="v">Noise (counts)<b id="noise">-</b></div>
<div class="v">ADC input (V, if Vref OK)<b id="volt">-</b></div><div class="v">Suggested threshold (6 sigma)<b id="sugg">-</b></div>
<div class="v">Decay curve (counts per delay)<b id="taps">-</b></div><div class="v">Decay time constant (us)<b id="tau">-</b></div>
<div class="v">Ground balance<b id="gbs">-</b></div><div class="v">Distance estimate (cm)<b id="dist">-</b></div></div>
<div class="row"><input id="refcm" type="number" placeholder="reference distance, cm" style="width:60%"><button id="bRef">SET REFERENCE</button></div>
<div class="v">Place a known target at that distance, wait for METAL RESPONSE, then press. The estimate is relative to that target (same size, orientation, soil); expect +-50% at best.</div></div>

<div class="card"><h2>Controls</h2>
<button id="bStop">STOP</button>
<div class="row"><select id="mode"><option value="sim">Simulated data (no hardware)</option><option value="test">ADC test (pulses OFF, safe low-voltage input only)</option><option value="detect">PI detect (COIL PULSES ON)</option></select></div>
<div class="row"><button id="bStart">START</button></div>
<div class="row"><button id="bCal">CALIBRATE</button><button id="bRst">RESET BASELINE</button></div></div>

<div class="card"><h2>Signal response trend (frame averages, not the full pulse waveform) &nbsp;<span style="color:#d29922">- - threshold</span></h2><canvas id="c1"></canvas>
<h2 style="margin-top:10px">Raw ADC mean (counts) &nbsp;<span style="color:#8b98a5">- - baseline</span></h2><canvas id="c2"></canvas></div>

<div class="card"><h2>Detection (live)</h2><div class="grid">
<label>Threshold (counts)<input id="thr" type="number" step="any"></label>
<label>Hysteresis (release ratio)<input id="hy" type="number" step="0.05"></label>
<label>Average (measurements/frame)<input id="avg" type="number"></label>
<label>EMA alpha<input id="al" type="number" step="0.01"></label>
<label>Confirm frames<input id="cf" type="number"></label>
<label>Drift tracking<input id="dft" type="number" step="0.001"></label>
<label>Calibration frames<input id="cal" type="number"></label>
<label>Ground balance (0 auto, 1 on, 2 off)<input id="gb" type="number"></label>
<label>Distance exponent (6 compact, 3-4 large)<input id="dx" type="number" step="0.5"></label></div>
<div class="v">Ground balance: press CALIBRATE, then raise and lower the coil 10-20 cm over bare soil for the whole calibration.</div></div>

<div class="card"><h2>Acquisition (only while stopped)</h2><div class="grid">
<label>Pulse width (us)<input id="pw" type="number"></label>
<label>Period (us)<input id="pr" type="number"></label>
<label>First sample delay (us)<input id="dl" type="number"></label>
<label>Delay taps (1-4, decay curve)<input id="tp" type="number"></label>
<label>Tap step (us)<input id="ds" type="number"></label>
<label>Samples / pulse<input id="spp" type="number"></label>
<label>ADC data rate<select id="dr"><option value="240">30000 SPS</option><option value="224">15000</option><option value="208">7500</option><option value="192">3750</option><option value="176">2000</option><option value="161">1000</option></select></label>
<label>ADC gain<select id="g"><option value="0">x1</option><option value="1">x2</option><option value="2">x4</option><option value="3">x8</option><option value="4">x16</option><option value="5">x32</option><option value="6">x64</option></select></label></div>
<div class="row"><button id="bApply">APPLY SETTINGS</button></div>
<div class="v" style="margin-top:6px">Limits are derived from the coil/power-stage ESTIMATES in config.h (see Diagnostics). Measure to confirm.</div></div>

<div class="card"><h2>Diagnostics</h2><pre id="diag">-</pre></div>

<script>
const $=id=>document.getElementById(id),HIST=300;let ws,dH=[],rH=[],bH=[],lastErr='',cur={thr:0,base:0,lsb:0};
const send=o=>{if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))};
function connect(){ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onopen=()=>{$('conn').textContent='CONNECTED';$('conn').className='pill ok';send({cmd:'get'})};
ws.onclose=()=>{$('conn').textContent='OFFLINE';$('conn').className='pill bad';setTimeout(connect,1000)};
ws.onmessage=e=>{let m;try{m=JSON.parse(e.data)}catch(x){return}if(m.t==='f')onFrame(m);else if(m.t==='s')onStatus(m)}}
setInterval(()=>send({cmd:'hb'}),500);
const f=(v,d)=>(+v).toFixed(d===undefined?1:d);
function onFrame(m){cur.base=m.base;
$('raw').textContent=f(m.raw);$('filt').textContent=f(m.filt);$('base').textContent=m.bv?f(m.base):'-';
$('delta').textContent=m.bv?f(m.d):'-';$('sigma').textContent=m.bv?f(m.sg):'-';$('noise').textContent=m.bv?f(m.nz):'-';
$('sugg').textContent=m.bv?f(m.nz*6,0):'-';
$('taps').textContent=m.tp&&m.tp.length?m.tp.map(v=>f(v,0)).join(' / '):'-';$('tau').textContent=m.tau>0?f(m.tau,0):'-';
$('gbs').textContent=m.bv&&m.tp&&m.tp.length>1?(m.gb?'ON':'OFF')+' g='+f(m.g,2)+(m.gm?' (measured)':' (soil model)'):'-';$('dist').textContent=m.dist>0?f(m.dist,0):'-';$('volt').textContent=cur.lsb?f(m.raw*cur.lsb,4):'-';
$('rel').textContent=f(m.rel,0);$('barf').style.width=Math.min(100,m.rel)+'%';
const d=$('det');d.className='det'+(m.det?' on':'');d.textContent=m.sat?'SATURATED':(!m.ok?'INVALID READING':(m.bv?(m.det?'METAL RESPONSE':'NO TARGET'):(m.cal?'CALIBRATING '+m.cp+'%':'NOT CALIBRATED')));
$('classify').textContent=m.det?'Detected response — metal type unknown':'Unknown — no target classification';
const c=$('pCal');c.textContent=m.bv?'CALIBRATED':(m.cal?'CALIBRATING '+m.cp+'%':'NOT CALIBRATED');c.className='pill '+(m.bv?'ok':'warn');
dH.push(m.d);rH.push(m.raw);bH.push(m.bv?m.base:null);if(dH.length>HIST){dH.shift();rH.shift();bH.shift()}draw()}
const MODES=['IDLE','SIMULATED','ADC TEST','PI DETECT'];
function onStatus(m){cur.lsb=m.lsb;cur.thr=m.set.thr;$('pMode').textContent=(m.rq!==m.mode)?(m.rq?'STARTING...':'STOPPING...'):MODES[m.mode];
const p=$('pPulse');p.textContent=m.pulsing?'PULSING':'PULSES OFF';p.className='pill '+(m.pulsing?'bad':'');
const a=$('pAdc');a.textContent=m.adc?'ADC OK':(m.ap?'ADC NOT CONNECTED':'ADC PINS UNSET');a.className='pill '+(m.adc?'ok':'warn');
const b=$('ban');if(m.mode===1){b.style.display='block';b.style.background='#d29922';b.style.color='#000';b.textContent='SIMULATED DATA - not a coil measurement'}
else if(m.mode===2){b.style.display='block';b.style.background='#388bfd';b.textContent='ADC TEST - pulses OFF. Apply only a safe, known low voltage.'}
else if(m.mode===3){b.style.display='block';b.style.background='#da3633';b.textContent='PI DETECT - coil pulses active only while this page stays connected'}
else if(m.err){b.style.display='block';b.style.background='#da3633';b.style.color='#fff';b.textContent=m.err}else b.style.display='none';
for(const k in m.set){const i=$(k);if(i&&document.activeElement!==i)i.value=m.set[k]}
$('diag').textContent='pulse pins set: '+(m.pp?'yes':'NO')+'   ADC pins set: '+(m.ap?'yes':'NO')+'   ADC ID: '+m.id+
'\nADC reads '+m.rd+'  DRDY timeouts '+m.to+'\ngate edges '+m.ed+'  frames '+m.fr+'  saturated frames '+m.sat+
'\ninvalid samples '+m.inv+'  late '+m.lt+'  sync anomalies '+m.sa+'  missed '+m.mi+'  no-edge '+m.ne+
'\ncoil model: Ipk '+m.coil.ipk+' A, Imean '+m.coil.iavg+' A, clamp '+m.coil.cw+' W, min first delay '+m.coil.md+' us, S&H '+(m.coil.sh?'yes':'no')+
'\n'+(m.err?'LAST ERROR: '+m.err:'')}
function draw(){plot($('c1'),[{d:dH,c:'#388bfd'}],cur.thr?[cur.thr,-cur.thr]:[]);plot($('c2'),[{d:rH,c:'#2ea043'}],[],bH)}
function plot(cv,s,hl,bl){const x=cv.getContext('2d'),w=cv.width=cv.clientWidth*devicePixelRatio,h=cv.height=cv.clientHeight*devicePixelRatio;
let a=[];s.forEach(q=>{a=a.concat(q.d)});hl.forEach(v=>a.push(v));if(bl)bl.forEach(v=>{if(v!==null)a.push(v)});if(!a.length)return;
let mn=Math.min.apply(null,a),mx=Math.max.apply(null,a);if(mx-mn<1){mx+=1;mn-=1}const p=(mx-mn)*.1;mn-=p;mx+=p;const Y=v=>h-(v-mn)/(mx-mn)*h;
x.setLineDash([6,6]);x.strokeStyle='#d29922';hl.forEach(v=>{x.beginPath();x.moveTo(0,Y(v));x.lineTo(w,Y(v));x.stroke()});
if(bl){x.strokeStyle='#8b98a5';x.beginPath();let st=false;bl.forEach((v,i)=>{if(v===null)return;const X=i/(HIST-1)*w;st?x.lineTo(X,Y(v)):x.moveTo(X,Y(v));st=true});x.stroke()}
x.setLineDash([]);x.lineWidth=2*devicePixelRatio;s.forEach(q=>{x.strokeStyle=q.c;x.beginPath();q.d.forEach((v,i)=>{const X=i/(HIST-1)*w;i?x.lineTo(X,Y(v)):x.moveTo(X,Y(v))});x.stroke()});
x.fillStyle='#8b98a5';x.font=(11*devicePixelRatio)+'px sans-serif';x.fillText(mx.toFixed(0),4,12*devicePixelRatio);x.fillText(mn.toFixed(0),4,h-4)}
$('bStop').onclick=()=>{send({cmd:'stop'});fetch('/stop',{method:'POST'}).catch(()=>{})};
$('bStart').onclick=()=>{const m=$('mode').value;if(m==='detect'&&!confirm('This ENABLES COIL PULSING.\nConfirm: clamp + analog front end verified, fuse and master switch fitted, hardware enable armed.\nStart pulses?'))return;
dH=[];rH=[];bH=[];send({cmd:'start',mode:m,confirm:m==='detect'})};
$('bCal').onclick=()=>send({cmd:'calibrate'});$('bRef').onclick=()=>send({cmd:'gref',cm:+$('refcm').value});$('bRst').onclick=()=>send({cmd:'reset_baseline'});
$('bApply').onclick=()=>{const o={cmd:'set'};['pw','pr','dl','tp','ds','spp','dr','g','avg','al','thr','hy','cf','dft','cal','gb','dx'].forEach(k=>o[k]=+$(k).value);send(o)};
window.addEventListener('resize',draw);connect();
</script></body></html>)rawliteral";
