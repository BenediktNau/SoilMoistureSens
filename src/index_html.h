#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Bodenfeuchte</title>
<style>
:root{--bg:#0d1116;--card:#141a21;--line:#242c36;--field:#0a0e13;--tx:#e6eaef;--mut:#8a94a1;--dim:#5b6572;--acc:#5eb1e8;--dry:#e8a85c;--wet:#9b8cf5}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif}
.wrap{max-width:480px;margin:0 auto;padding:20px 16px 28px;display:flex;flex-direction:column;gap:14px}
.mono{font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace;font-variant-numeric:tabular-nums}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:18px 16px;display:flex;flex-direction:column;gap:14px}
h2{font-size:12px;font-weight:600;letter-spacing:.09em;text-transform:uppercase;color:var(--mut);margin:0}
label{display:block;font-size:13px;color:#b7c0ca;margin-bottom:6px}
input{width:100%;height:44px;background:var(--field);border:1px solid var(--line);border-radius:8px;padding:0 12px;font:inherit;color:var(--tx)}
input.mono{font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
input::placeholder{color:var(--dim)}
input:focus{outline:none;border-color:var(--acc)}
button{height:44px;border-radius:8px;padding:0 14px;font:inherit;font-size:14px;font-weight:600;white-space:nowrap;cursor:pointer;background:transparent;border:1px solid #2d3742;color:#cdd5de}
button.pri{background:var(--acc);border-color:var(--acc);color:#061018;height:48px;font-size:15px}
button.sm{height:36px;font-size:13px}
button:disabled{opacity:.5;cursor:default}
.row{display:flex;gap:8px}.row>*:first-child{flex:1;min-width:0}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:10px}.g3{display:grid;grid-template-columns:2fr 1fr;gap:10px}
.hint{font-size:12px;color:var(--mut);margin:0;line-height:1.45}
.head{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;padding:0 2px 6px}
.pill{display:flex;align-items:center;gap:8px;background:var(--card);border:1px solid var(--line);border-radius:999px;padding:6px 10px 6px 8px;font-size:12px;color:#b7c0ca;white-space:nowrap}
.dot{width:8px;height:8px;border-radius:50%;background:var(--acc);box-shadow:0 0 8px var(--acc)}
.big{font-size:64px;font-weight:600;line-height:1;letter-spacing:-.03em}
.badge{font-size:13px;font-weight:600;padding:5px 10px;border-radius:999px;border:1px solid var(--line)}
.bar{position:relative;height:10px;display:flex;gap:2px}.bar div{height:10px}
.z1{background:#3a2a17;border-radius:999px 0 0 999px}.z2{background:#172d3a}.z3{background:#221d3a;border-radius:0 999px 999px 0}
.mark{position:absolute;top:-4px;width:4px;height:18px;margin-left:-2px;border-radius:2px;left:0}
.scale{display:flex;justify-content:space-between;font-size:11px;color:var(--dim);margin-top:8px}
.msg{min-height:18px;font-size:13px;text-align:center;color:var(--mut)}
.msg.err{color:var(--dry)}.msg.ok{color:var(--acc)}
.unit{position:relative}.unit span{position:absolute;right:12px;top:12px;color:var(--dim);font-size:14px}
.ok{color:var(--acc)}
.tbox{display:flex;align-items:center;justify-content:space-between;gap:10px;background:var(--field);border:1px solid var(--line);border-radius:8px;padding:10px 12px}
.actions{display:flex;flex-direction:column;gap:10px;padding-top:6px}
</style></head><body><div class="wrap">

<div class="head">
 <div><div style="font-size:18px;font-weight:700">Bodenfeuchte</div><div class="mono" style="font-size:12px;color:var(--mut)" id="ids">&ndash;</div></div>
 <div class="pill"><span class="dot"></span>Konfigmodus <span class="mono" id="left" style="color:var(--tx)">&ndash;:&ndash;&ndash;</span></div>
</div>

<div class="card">
 <div style="display:flex;justify-content:space-between;align-items:flex-end;gap:12px">
  <div><h2>Aktuelle Feuchte</h2><div style="display:flex;align-items:baseline;gap:6px"><span class="mono big" id="pct">&ndash;</span><span class="mono" style="font-size:22px;color:var(--mut)">%</span></div></div>
  <div style="display:flex;flex-direction:column;align-items:flex-end;gap:8px;padding-bottom:4px"><span class="badge" id="lvl">&ndash;</span><span class="mono" style="font-size:12px;color:var(--mut)">Roh <span id="raw">&ndash;</span></span></div>
 </div>
 <div>
  <div class="bar"><div class="z1" id="z1"></div><div class="z2" id="z2"></div><div class="z3" id="z3"></div><div class="mark" id="mark"></div></div>
  <div class="mono scale"><span>0</span><span id="t1" style="color:var(--mut)">30 trocken</span><span id="t2" style="color:var(--mut)">70 nass</span><span>100</span></div>
 </div>
 <p class="hint">Aktualisiert alle 2 Sekunden. Prozent rechnet mit den unten eingetragenen Kalibrierwerten.</p>
</div>

<div class="card">
 <h2>Kalibrierung</h2>
 <div><label for="dryRaw">Rohwert trocken (Sensor an der Luft)</label><div class="row"><input class="mono" id="dryRaw" type="number" inputmode="numeric"><button type="button" onclick="cal('dryRaw')">Aktuellen Wert &uuml;bernehmen</button></div></div>
 <div><label for="wetRaw">Rohwert nass (Sensor im Wasser)</label><div class="row"><input class="mono" id="wetRaw" type="number" inputmode="numeric"><button type="button" onclick="cal('wetRaw')">Aktuellen Wert &uuml;bernehmen</button></div></div>
 <p class="hint">Sensor in Luft halten, Wert &uuml;bernehmen. Dann in ein Wasserglas, Wert &uuml;bernehmen.</p>
</div>

<div class="card">
 <h2>Bewertung und Intervall</h2>
 <div class="g2">
  <div><label for="dryBelowPct">Trocken unter</label><div class="unit"><input class="mono" id="dryBelowPct" type="number" min="0" max="100"><span>%</span></div></div>
  <div><label for="wetAbovePct">Nass ab</label><div class="unit"><input class="mono" id="wetAbovePct" type="number" min="0" max="100"><span>%</span></div></div>
 </div>
 <div><label for="intervalMin">Sendeintervall</label><div class="unit"><input class="mono" id="intervalMin" type="number" min="1" max="180"><span>Minuten</span></div></div>
 <p class="hint">Zwischen zwei Messungen schl&auml;ft der Sensor. K&uuml;rzere Intervalle kosten Akku.</p>
</div>

<div class="card">
 <div style="display:flex;justify-content:space-between;align-items:center"><h2>WLAN</h2><span class="mono" id="wifiState" style="font-size:12px">&ndash;</span></div>
 <div><label for="ssid">Netzwerk</label><div class="row"><input id="ssid" list="nets" autocomplete="off"><button type="button" id="scanBtn" onclick="scan()">Suchen</button></div><datalist id="nets"></datalist></div>
 <div><label for="wifiPassword">Passwort</label><input class="mono" id="wifiPassword" type="password" autocomplete="off"></div>
 <div class="g2">
  <div><label for="staticIp">Statische IP</label><input class="mono" id="staticIp" placeholder="leer = DHCP" inputmode="decimal"></div>
  <div><label for="gateway">Gateway</label><input class="mono" id="gateway" placeholder="192.168.1.1" inputmode="decimal"></div>
 </div>
 <div class="g2">
  <div><label for="subnet">Subnetzmaske</label><input class="mono" id="subnet" inputmode="decimal"></div>
  <div><label for="dns">DNS</label><input class="mono" id="dns" placeholder="leer = Gateway" inputmode="decimal"></div>
 </div>
 <p class="hint">Eine feste IP spart beim Aufwachen die DHCP-Zeit und damit Akku. Gateway und Maske sind dann Pflicht.</p>
</div>

<div class="card">
 <h2>MQTT</h2>
 <div class="g3">
  <div><label for="mqttHost">Broker</label><input class="mono" id="mqttHost"></div>
  <div><label for="mqttPort">Port</label><input class="mono" id="mqttPort" type="number" min="1" max="65535"></div>
 </div>
 <div class="g2">
  <div><label for="mqttUser">Benutzer</label><input id="mqttUser" placeholder="optional" autocomplete="off"></div>
  <div><label for="mqttPassword">Passwort</label><input class="mono" id="mqttPassword" type="password" placeholder="optional" autocomplete="off"></div>
 </div>
 <div class="g2">
  <div><label for="topicPrefix">Topic-Pr&auml;fix</label><input class="mono" id="topicPrefix"></div>
  <div><label for="deviceName">Ger&auml;tename</label><input class="mono" id="deviceName"></div>
 </div>
 <div class="tbox"><div><div style="font-size:12px;color:var(--mut)">Sendet nach</div><div class="mono" id="topic" style="font-size:13px">&ndash;</div></div><button type="button" class="sm" onclick="mqttTest()">Testnachricht</button></div>
</div>

<div class="actions">
 <div class="msg" id="msg"></div>
 <button type="button" onclick="save()">Speichern</button>
 <button type="button" class="pri" onclick="saveSleep()">Speichern und Messbetrieb starten</button>
 <p class="hint" style="text-align:center">Danach misst der Sensor, sendet und schl&auml;ft bis zum n&auml;chsten Intervall. Zur&uuml;ck ins Men&uuml;: Taster am Geh&auml;use dr&uuml;cken.</p>
</div>

</div>
<script>
const $=id=>document.getElementById(id);
const F=['ssid','wifiPassword','staticIp','gateway','subnet','dns','mqttHost','mqttPort','mqttUser','mqttPassword','topicPrefix','deviceName','dryRaw','wetRaw','dryBelowPct','wetAbovePct','intervalMin'];
const NUM=['mqttPort','dryRaw','wetRaw','dryBelowPct','wetAbovePct','intervalMin'];
const NAMES={dry:'trocken',ok:'ok',wet:'nass'},COL={dry:'#e8a85c',ok:'#5eb1e8',wet:'#9b8cf5'};
let lastRaw=null,timer=null,ended=false;
function num(id){const v=parseInt($(id).value,10);return isNaN(v)?0:v}
function pctOf(raw){const d=num('dryRaw'),w=num('wetRaw');if(d===w)return 0;const p=Math.trunc((raw-d)*100/(w-d));return Math.max(0,Math.min(100,p))}
function levelOf(p){return p<num('dryBelowPct')?'dry':p>=num('wetAbovePct')?'wet':'ok'}
function render(){
 const d=num('dryBelowPct'),w=num('wetAbovePct');
 $('z1').style.flexBasis=d+'%';$('z2').style.flexBasis=Math.max(0,w-d)+'%';$('z3').style.flexBasis=Math.max(0,100-w)+'%';
 $('t1').textContent=d+' trocken';$('t2').textContent=w+' nass';
 $('topic').textContent=$('topicPrefix').value+'/'+$('deviceName').value+'/state';
 if(lastRaw===null)return;
 const p=pctOf(lastRaw),l=levelOf(p),c=COL[l];
 $('raw').textContent=lastRaw;$('pct').textContent=p;$('pct').style.color=c;$('pct').style.textShadow='0 0 24px '+c+'73';
 const b=$('lvl');b.textContent=NAMES[l];b.style.color=c;b.style.borderColor=c+'59';b.style.background=c+'1f';
 const m=$('mark');m.style.left=p+'%';m.style.background=c;m.style.boxShadow='0 0 10px '+c+'73';
}
function fmt(s){s=Math.max(0,s);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0')}
async function poll(){
 if(ended)return;
 try{
  const r=await fetch('/api/status');const s=await r.json();
  lastRaw=s.raw;$('left').textContent=fmt(s.secondsLeft);
  $('ids').textContent=$('deviceName').value+' · '+s.apIp;
  const w=$('wifiState');w.textContent=s.staIp?'verbunden · '+s.staIp:'nicht verbunden';w.className='mono'+(s.staIp?' ok':'');
  render();
 }catch(e){say('Keine Verbindung zum Sensor','err')}
}
function collect(){const o={};for(const k of F){if(NUM.includes(k)){const v=$(k).value.trim();if(v==='')continue;o[k]=parseInt(v,10)}else o[k]=$(k).value}return o}
async function loadCfg(){
 const r=await fetch('/api/config');const c=await r.json();
 for(const k of F)if(k in c)$(k).value=c[k];
 $('wifiPassword').placeholder=c.wifiPasswordSet?'•••••••• (unverändert)':'';
 $('mqttPassword').placeholder=c.mqttPasswordSet?'•••••••• (unverändert)':'optional';
 render();
}
function say(t,cls){const m=$('msg');m.textContent=t;m.className='msg '+(cls||'')}
async function post(url,body){
 const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
 const t=await r.text();if(!r.ok)throw new Error(t||('HTTP '+r.status));return t;
}
async function save(){try{await post('/api/config',collect());$('wifiPassword').value='';$('mqttPassword').value='';await loadCfg();say('Gespeichert','ok')}catch(e){say(e.message,'err')}}
async function saveSleep(){
 try{await post('/api/config',collect());await post('/api/sleep');ended=true;clearInterval(timer);
  document.querySelectorAll('button').forEach(b=>b.disabled=true);
  say('Sensor misst, sendet und schläft. Zurück ins Menü mit dem Taster.','ok')}catch(e){say(e.message,'err')}
}
async function mqttTest(){try{await post('/api/config',collect());const t=await post('/api/mqtt-test');say(t||'Testnachricht gesendet','ok')}catch(e){say(e.message,'err')}}
async function scan(){
 const b=$('scanBtn');b.disabled=true;b.textContent='Suche…';
 try{const r=await fetch('/api/scan');const nets=await r.json();const dl=$('nets');dl.innerHTML='';
  for(const n of nets){const o=document.createElement('option');o.value=n.ssid;o.label=n.rssi+' dBm'+(n.secure?', gesichert':'');dl.appendChild(o)}
  say(nets.length+' Netzwerke gefunden, Feld Netzwerk antippen')}
 catch(e){say('Suche fehlgeschlagen','err')}
 b.disabled=false;b.textContent='Suchen';
}
function cal(id){if(lastRaw!==null){$(id).value=lastRaw;render()}}
document.querySelectorAll('input').forEach(i=>i.addEventListener('input',render));
loadCfg().then(poll);timer=setInterval(poll,2000);
</script></body></html>)rawliteral";
