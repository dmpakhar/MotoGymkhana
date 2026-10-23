#include "MotoWebServer.h"

#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config/Config.h"
#include "config/Version.h"
#include "protocol/DisplayStatusProtocol.h"
#include "protocol/FinishGateProtocol.h"
#include "protocol/GateControlProtocol.h"
#include "protocol/OtaControlProtocol.h"

namespace
{
constexpr char kRiderPage[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Gymio</title><style>
:root{color-scheme:dark}body{margin:0;background:#10151b;color:#edf3f7;font:16px system-ui,sans-serif}.app{max-width:620px;margin:auto;padding:16px}.card{background:#1a232d;border-radius:16px;padding:20px;margin:14px 0}h1{font-size:20px;letter-spacing:.08em;text-align:center}.label{color:#9cb0bf;text-transform:uppercase;font-size:12px;letter-spacing:.1em}.value{font-size:48px;font-weight:700;margin:8px 0}.timer{font-variant-numeric:tabular-nums;font-size:42px}.small{font-size:22px}#connection{color:#83df9c;text-align:center;font-size:14px}.rider-card{border:2px solid #3d5262}.rider-name{font-size:28px;font-weight:700;margin:8px 0}.rider-form{display:flex;gap:10px}.rider-form input{min-width:0;flex:1;border:1px solid #3d5262;background:#10151b;color:#edf3f7;border-radius:10px;padding:14px;font-size:17px}.rider-form button,.prepare-button,.logout-button{border:0;background:#83df9c;color:#10151b;border-radius:10px;min-width:110px;padding:12px;font-size:16px;font-weight:700}.prepare-button{width:100%;margin:10px 0}.logout-button{display:none;width:100%;margin-top:10px;background:#3d5262;color:#edf3f7}.prepare-button:disabled{opacity:.45}.best-card{text-align:center}.best-card .value{font-variant-numeric:tabular-nums;font-size:36px}.times{padding:0;list-style:none;margin:12px 0 0}.times li{display:flex;justify-content:space-between;padding:10px 0;border-top:1px solid #2b3945;font-variant-numeric:tabular-nums}.empty{color:#9cb0bf}</style></head><body><main class="app"><h1>GYMIO RIDER</h1><div id="connection">Connecting…</div><section class="card timer-card"><div class="label">Timer state</div><div class="value small" id="state">—</div><div class="timer" id="time">00:00.000</div><div class="label">Current attempt</div><div class="small" id="timerRider">NONAME</div></section><section class="card rider-card"><div class="label">This phone</div><div class="rider-name" id="phoneRider">Not registered</div><button class="prepare-button" id="prepareButton" type="button" disabled>Prepare for ride</button><button class="logout-button" id="logoutButton" type="button">Delete rider</button><form class="rider-form" id="riderForm"><input id="riderName" maxlength="24" placeholder="Your rider name" required><button>Register</button></form></section><section class="card best-card"><div class="label" id="bestLabel">Your best time</div><div class="value" id="best">—</div></section><section class="card"><div class="label">Your last 5 times</div><ul class="times" id="personalTimes"><li class="empty">No completed attempts yet</li></ul></section><span hidden id="distance"></span><span hidden id="distanceUnit"></span><span hidden id="filtered"></span><span hidden id="trigger"></span><input hidden id="gateLimit"><button hidden id="sensorSetupButton"></button><form hidden id="gateLimitForm"></form><button hidden id="useCurrentLimit"></button><ul hidden id="history"></ul><div hidden id="riderList"></div></main><script>
const $=id=>document.getElementById(id),distance=$('distance'),distanceUnit=$('distanceUnit'),filtered=$('filtered'),trigger=$('trigger'),state=$('state'),time=$('time'),connection=$('connection'),historyList=$('history'),riderList=$('riderList'),gateLimit=$('gateLimit'),setupButton=$('sensorSetupButton');
let clientId=localStorage.getItem('gymioClientId');if(!clientId){clientId=(crypto.randomUUID?crypto.randomUUID():Date.now().toString(36)+'-'+Math.random().toString(36).slice(2));localStorage.setItem('gymioClientId',clientId)}
const f=v=>{const m=Math.floor(v/60000),s=Math.floor(v/1000)%60,ms=v%1000;return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0')+'.'+String(ms).padStart(3,'0')};
async function api(url,options){const r=await fetch(url,options);if(!r.ok)throw new Error((await r.json()).error||'Request failed');return r.json()}
function renderRiders(data){const phone=data.phoneRiderId===0?null:data.riders.find(r=>r.id===data.phoneRiderId),prepare=$('prepareButton'),logout=$('logoutButton');$('phoneRider').textContent=phone?phone.name:'Not registered';$('riderForm').style.display=phone?'none':'flex';prepare.disabled=!phone;prepare.style.display=phone?'block':'none';logout.style.display=phone?'block':'none';prepare.textContent=phone&&phone.id===data.preparedId?'Prepared ✓':'Prepare for ride';$('best').textContent=phone&&phone.sessionBestMs?f(phone.sessionBestMs):'—';$('bestLabel').textContent=(phone?phone.name:'Your')+' · best time';const times=$('personalTimes');times.innerHTML=phone&&phone.lastTimesMs.length?phone.lastTimesMs.map((t,i)=>'<li><span>Ride '+(i+1)+'</span><strong>'+f(t)+'</strong></li>').join(''):'<li class="empty">No completed attempts yet</li>'}
async function setAttemptValidity(number,valid){const action=valid?'revalidate':'invalidate';if(!confirm((valid?'Revalidate':'Invalidate')+' attempt '+number+'?'+(valid?' It will be eligible for best times again.':' It will be excluded from best times.')))return;try{await api('/api/attempts/'+action,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'number='+number});refresh()}catch(err){connection.textContent=err.message}}
async function refresh(){try{const [s,h,r]=await Promise.all([api('/api/status'),api('/api/history'),api('/api/riders?clientId='+encodeURIComponent(clientId))]);distance.textContent=s.distanceMm<0?'∞':s.distanceMm;distanceUnit.textContent=s.distanceMm<0?'':'mm';filtered.textContent=s.filteredDistanceMm<0?'—':s.filteredDistanceMm;trigger.textContent=s.triggerDistanceMm<0?'—':s.triggerDistanceMm;if(document.activeElement!==gateLimit)gateLimit.value=s.triggerDistanceMm>=0?s.triggerDistanceMm:'';gateLimit.disabled=!s.sensorSetupActive;$('gateLimitForm').querySelectorAll('button').forEach(b=>b.disabled=!s.sensorSetupActive);setupButton.textContent=s.sensorSetupActive?'Finish Sensor Setup':'Enable Sensor Setup';setupButton.classList.toggle('active',s.sensorSetupActive);setupButton.disabled=!s.calibrated&& !s.sensorSetupActive;state.textContent=s.sensorSetupActive?'SENSOR SETUP':s.state;time.textContent=f(s.timeMs);$('timerRider').textContent=s.riderName==='ANON'?'NONAME':s.riderName;$('best').textContent=s.sessionBestMs?f(s.sessionBestMs):'—';$('bestLabel').textContent=s.riderName+' · session best';connection.textContent=s.sensorSetupActive?'Sensor Setup: attempts blocked.':s.calibrated?'Live · v'+s.version:'Calibrating gate…';historyList.innerHTML=h.attempts.length?h.attempts.map(a=>'<li style="'+(a.valid?'':'opacity:.45;text-decoration:line-through')+'"><span>Attempt '+a.number+'<small>'+a.riderName+(a.valid?'':a.autoInvalid?' · BELOW MINIMUM':' · INVALID')+'</small></span><span><strong>'+f(a.durationMs)+'</strong>'+(a.valid?'<button style="margin-left:10px" onclick="setAttemptValidity('+a.number+',false)">Invalidate</button>':a.autoInvalid?'':'<button style="margin-left:10px" onclick="setAttemptValidity('+a.number+',true)">Revalidate</button>')+'</span></li>').join(''):'<li class="empty">No completed attempts yet</li>';renderRiders(r)}catch(e){connection.textContent=e.message||'Connection lost'}}$('riderForm').onsubmit=async e=>{e.preventDefault();const input=$('riderName');try{await api('/api/riders',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(input.value)+'&clientId='+encodeURIComponent(clientId)});input.value='';refresh()}catch(err){connection.textContent=err.message}};$('logoutButton').onclick=async()=>{if(!confirm('Delete this rider registration? Completed times will remain in history.'))return;try{await api('/api/riders/logout',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'clientId='+encodeURIComponent(clientId)});refresh()}catch(err){connection.textContent=err.message}};$('prepareButton').onclick=async()=>{try{await api('/api/riders/prepare',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'clientId='+encodeURIComponent(clientId)});refresh()}catch(err){connection.textContent=err.message}};$('gateLimitForm').onsubmit=async e=>{e.preventDefault();try{await api('/api/gate/limit',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'limit='+encodeURIComponent(gateLimit.value)});refresh()}catch(err){connection.textContent=err.message}};$('useCurrentLimit').onclick=async()=>{try{await api('/api/gate/limit/current',{method:'POST'});refresh()}catch(err){connection.textContent=err.message}};setupButton.onclick=async()=>{if(!setupButton.classList.contains('active')&&!confirm('Enter Sensor Setup? Attempts will be blocked until you finish setup.'))return;try{await api('/api/sensor-setup',{method:'POST'});refresh()}catch(err){connection.textContent=err.message}};refresh();setInterval(refresh,500);
</script></body></html>
)HTML";
constexpr char kPage[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Gymio</title><style>
:root{color-scheme:dark}body{margin:0;background:#10151b;color:#edf3f7;font:16px system-ui,sans-serif}.app{max-width:620px;margin:auto;padding:16px}.card{background:#1a232d;border-radius:16px;padding:20px;margin:14px 0}h1{font-size:20px;letter-spacing:.08em;text-align:center}.label{color:#9cb0bf;text-transform:uppercase;font-size:12px;letter-spacing:.1em}.value{font-size:48px;font-weight:700;margin:8px 0}.timer{font-variant-numeric:tabular-nums;font-size:42px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.small{font-size:22px}#connection{color:#83df9c;text-align:center;font-size:14px}.rider-card{border:2px solid #3d5262}.rider-name{font-size:24px;font-weight:700;margin:8px 0}.rider-list{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;margin:16px 0}.rider-option{display:flex;gap:6px}.rider{border:2px solid #3d5262;background:#172029;color:#edf3f7;min-height:62px;flex:1;padding:10px;border-radius:12px;font-size:18px;font-weight:700;word-break:break-word}.rider.selected{border-color:#83df9c;background:#19382a;color:#b4f5c4}.rider-delete{border:1px solid #85414a;background:#351d22;color:#ffb5bd;border-radius:12px;min-width:56px;font-size:22px}.rider-form,.gate-form{display:flex;gap:10px}.rider-form input,.gate-form input{min-width:0;flex:1;border:1px solid #3d5262;background:#10151b;color:#edf3f7;border-radius:10px;padding:14px;font-size:17px}.rider-form button,.gate-form button,.setup-button{border:0;background:#83df9c;color:#10151b;border-radius:10px;min-width:74px;padding:12px;font-size:16px;font-weight:700}.setup-button{width:100%;margin:14px 0 8px}.setup-button.active{background:#ffcb6b}.gate-form .secondary{background:#3d5262;color:#edf3f7}.best-card{text-align:center}.best-card .value{font-variant-numeric:tabular-nums;font-size:36px}.history{padding:0;list-style:none;margin:12px 0 0}.history li{display:flex;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid #2b3945;font-variant-numeric:tabular-nums}.history small{display:block;color:#9cb0bf;margin-top:3px}.debug-card summary{cursor:pointer;color:#9cb0bf}.debug-card .value{font-size:34px}.gate-help{color:#9cb0bf;font-size:13px}.empty{color:#9cb0bf}@media (orientation:landscape) and (min-width:640px){.app{max-width:960px;padding:18px}.timer-card,.debug-card{max-width:650px;margin-left:auto;margin-right:auto}.rider-list{grid-template-columns:repeat(3,minmax(0,1fr))}}</style></head><body><main class="app"><h1>GYMIO</h1><div id="connection">Connecting…</div><section class="card timer-card"><div class="label">Timer state</div><div class="value small" id="state">—</div><div class="timer" id="time">00:00.000</div><div class="label">Rider</div><div class="small" id="timerRider">ANON</div></section><section class="card rider-card"><div class="label">Select rider for next attempt</div><div class="rider-name" id="selectedRider">ANON</div><div class="rider-list" id="riderList"></div><form class="rider-form" id="riderForm"><input id="riderName" maxlength="24" placeholder="New rider name" required><button>Add</button></form></section><section class="card best-card"><div class="label" id="bestLabel">Session best</div><div class="value" id="best">—</div></section><section class="card history-card"><div class="label">Attempt history (session)</div><ul class="history" id="history"><li class="empty">No completed attempts yet</li></ul></section><details class="card debug-card"><summary>Sensor debug</summary><button class="setup-button" id="sensorSetupButton" type="button">Enable Sensor Setup</button><div class="label">Distance</div><div class="value"><span id="distance">—</span> <small id="distanceUnit">mm</small></div><div class="grid"><div><div class="label">Filtered</div><div class="small"><span id="filtered">—</span> mm</div></div><div><div class="label">Gate limit</div><div class="small"><span id="trigger">—</span> mm</div></div></div><p class="gate-help">Sensor Setup blocks attempts. A distance at or below the gate limit is a crossing.</p><form class="gate-form" id="gateLimitForm"><input id="gateLimit" type="number" min="0" max="3999" step="1" required><button>Set limit</button><button class="secondary" id="useCurrentLimit" type="button">Use current</button></form></details></main><script>
const $=id=>document.getElementById(id),distance=$('distance'),distanceUnit=$('distanceUnit'),filtered=$('filtered'),trigger=$('trigger'),state=$('state'),time=$('time'),connection=$('connection'),historyList=$('history'),riderList=$('riderList'),gateLimit=$('gateLimit'),setupButton=$('sensorSetupButton');
const f=v=>{const m=Math.floor(v/60000),s=Math.floor(v/1000)%60,ms=v%1000;return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0')+'.'+String(ms).padStart(3,'0')};
async function api(url,options){const r=await fetch(url,options);if(!r.ok)throw new Error((await r.json()).error||'Request failed');return r.json()}
function renderRiders(data){$('selectedRider').textContent=data.riders.find(r=>r.id===data.selectedId).name;riderList.replaceChildren();data.riders.forEach(r=>{const option=document.createElement('div');option.className='rider-option';const b=document.createElement('button');b.className='rider'+(r.id===data.selectedId?' selected':'');b.textContent=r.name;b.onclick=async()=>{await api('/api/riders/select',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'id='+r.id});refresh()};option.appendChild(b);if(r.id!==0){const remove=document.createElement('button');remove.className='rider-delete';remove.textContent='×';remove.setAttribute('aria-label','Remove '+r.name);remove.onclick=async()=>{if(confirm('Remove rider '+r.name+'?')){try{await api('/api/riders?id='+r.id,{method:'DELETE'});refresh()}catch(e){connection.textContent=e.message}}};option.appendChild(remove)}riderList.appendChild(option)})}
async function setAttemptValidity(number,valid){const action=valid?'revalidate':'invalidate';if(!confirm((valid?'Revalidate':'Invalidate')+' attempt '+number+'?'+(valid?' It will be eligible for best times again.':' It will be excluded from best times.')))return;try{await api('/api/attempts/'+action,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'number='+number});refresh()}catch(err){connection.textContent=err.message}}
async function assignAttempt(number,riderId){try{await api('/api/attempts/assign',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'number='+number+'&riderId='+encodeURIComponent(riderId)});document.activeElement.blur();refresh()}catch(err){connection.textContent=err.message;refresh()}}
function showReassign(number){const select=$('attempt-rider-'+number);select.style.display='inline-block';select.focus()}
function renderHistory(data,riderData){if(document.activeElement&&document.activeElement.classList.contains('attempt-rider'))return;if(!data.attempts.length){historyList.innerHTML='<li class="empty">No completed attempts yet</li>';return}historyList.innerHTML=data.attempts.map(a=>{let riders=riderData.riders.slice();if(!riders.some(r=>r.id===a.riderId))riders.push({id:a.riderId,name:a.riderName+' (deleted)'});const options=riders.map(r=>'<option value="'+r.id+'" '+(r.id===a.riderId?'selected':'')+'>'+(r.id===0?'NONAME':r.name)+'</option>').join('');const validity=a.valid?'<button style="margin-left:8px" onclick="setAttemptValidity('+a.number+',false)">Invalidate</button>':a.autoInvalid?'':'<button style="margin-left:8px" onclick="setAttemptValidity('+a.number+',true)">Revalidate</button>';const riderName=a.riderId===0?'NONAME':a.riderName;return '<li style="'+(a.valid?'':'opacity:.6')+'"><span>Attempt '+a.number+'<small><button style="padding:0;border:0;background:none;color:#9cb0bf;text-decoration:underline;font:inherit" onclick="showReassign('+a.number+')">'+riderName+'</button>'+(a.valid?'':a.autoInvalid?' · BELOW MINIMUM':' · INVALID')+'</small></span><span style="text-align:right"><strong>'+f(a.durationMs)+'</strong><br><button style="margin-top:6px" onclick="showReassign('+a.number+')">Reassign</button>'+validity+'<br><select id="attempt-rider-'+a.number+'" class="attempt-rider" style="display:none;margin-top:8px;max-width:190px;background:#10151b;color:#edf3f7;border:1px solid #3d5262;border-radius:8px;padding:7px" onchange="assignAttempt('+a.number+',this.value)">'+options+'</select></span></li>'}).join('')}
async function refresh(){try{const [s,h,r]=await Promise.all([api('/api/status'),api('/api/history'),api('/api/riders')]);distance.textContent=s.distanceMm<0?'∞':s.distanceMm;distanceUnit.textContent=s.distanceMm<0?'':'mm';filtered.textContent=s.filteredDistanceMm<0?'—':s.filteredDistanceMm;trigger.textContent=s.triggerDistanceMm<0?'—':s.triggerDistanceMm;if(document.activeElement!==gateLimit)gateLimit.value=s.triggerDistanceMm>=0?s.triggerDistanceMm:'';gateLimit.disabled=!s.sensorSetupActive;$('gateLimitForm').querySelectorAll('button').forEach(b=>b.disabled=!s.sensorSetupActive);setupButton.textContent=s.sensorSetupActive?'Finish Sensor Setup':'Enable Sensor Setup';setupButton.classList.toggle('active',s.sensorSetupActive);setupButton.disabled=!s.calibrated&& !s.sensorSetupActive;state.textContent=s.sensorSetupActive?'SENSOR SETUP':s.state;time.textContent=f(s.timeMs);$('timerRider').textContent=s.riderName;$('best').textContent=s.absoluteBestMs?f(s.absoluteBestMs):'—';$('bestLabel').textContent='Absolute best · '+(s.absoluteBestMs?s.absoluteBestRider:'—');connection.textContent=s.sensorSetupActive?'Sensor Setup: attempts blocked.':s.calibrated?'Live · v'+s.version:'Calibrating gate…';renderHistory(h,r);renderRiders(r)}catch(e){connection.textContent=e.message||'Connection lost'}}$('riderForm').onsubmit=async e=>{e.preventDefault();const input=$('riderName');try{await api('/api/riders',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(input.value)});input.value='';refresh()}catch(err){connection.textContent=err.message}};$('gateLimitForm').onsubmit=async e=>{e.preventDefault();try{await api('/api/gate/limit',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'limit='+encodeURIComponent(gateLimit.value)});refresh()}catch(err){connection.textContent=err.message}};$('useCurrentLimit').onclick=async()=>{try{await api('/api/gate/limit/current',{method:'POST'});refresh()}catch(err){connection.textContent=err.message}};setupButton.onclick=async()=>{if(!setupButton.classList.contains('active')&&!confirm('Enter Sensor Setup? Attempts will be blocked until you finish setup.'))return;try{await api('/api/sensor-setup',{method:'POST'});refresh()}catch(err){connection.textContent=err.message}};refresh();setInterval(refresh,500);
</script><script>
document.querySelector('main').insertAdjacentHTML('beforeend','<details class="card debug-card"><summary>Time validation</summary><p class="gate-help">Attempts shorter than this time stay in history but do not count toward best times. Set 0 to disable.</p><form class="gate-form" id="minimumTimeForm"><input id="minimumTimeSeconds" type="number" min="0" max="3600" step="0.1" inputmode="decimal" required><button>Save</button></form></details>');
document.querySelector('main').insertAdjacentHTML('beforeend','<section class="card"><div class="label">System</div><p><a href="/configuration" style="color:#83df9c;font-weight:700">Configuration</a></p><p><a href="/update" style="color:#83df9c;font-weight:700">Firmware update</a></p></section>');
document.querySelector('.timer-card').insertAdjacentHTML('beforeend','<button id="cancelAttemptButton" type="button" style="width:100%;margin-top:16px;border:1px solid #b94c59;background:#6d2730;color:#fff;border-radius:10px;padding:13px;font-size:16px;font-weight:700">Stop current attempt</button>');
document.querySelector('main').insertAdjacentHTML('beforeend','<details class="card debug-card"><summary>Finish gate · <span id="gateMode">ONE GATE</span></summary><button class="setup-button" id="finishSetupButton" type="button">Enable Finish Setup</button><div class="grid"><div><div class="label">Distance</div><div class="small"><span id="finishDistance">—</span> mm</div></div><div><div class="label">Filtered</div><div class="small"><span id="finishFiltered">—</span> mm</div></div><div><div class="label">Gate limit</div><div class="small"><span id="finishTrigger">—</span> mm</div></div></div><p class="gate-help">Finish setup blocks attempts and requires READY.</p><form class="gate-form" id="finishGateLimitForm"><input id="finishGateLimit" type="number" min="0" max="3999" required><button>Set limit</button><button class="secondary" id="finishUseCurrent" type="button">Use current</button></form></details>');
async function refreshFinish(){try{const s=await api('/api/status');$('gateMode').textContent=s.gateMode;$('finishDistance').textContent=s.finishGateConnected?(s.finishDistanceMm<0?'∞':s.finishDistanceMm):'—';$('finishFiltered').textContent=s.finishGateConnected?s.finishFilteredDistanceMm:'—';$('finishTrigger').textContent=s.finishGateConnected?s.finishTriggerDistanceMm:'—';const active=s.finishSensorSetupActive,b=$('finishSetupButton'),input=$('finishGateLimit'),minimum=$('minimumTimeSeconds');b.textContent=active?'Finish Setup':'Enable Finish Setup';b.classList.toggle('active',active);b.disabled=!s.finishGateConnected;input.disabled=!active;$('finishGateLimitForm').querySelectorAll('button').forEach(x=>x.disabled=!active);if(document.activeElement!==input&&s.finishTriggerDistanceMm>=0)input.value=s.finishTriggerDistanceMm;if(document.activeElement!==minimum)minimum.value=(s.minimumValidTimeMs/1000).toFixed(1);connection.textContent=s.sensorSetupActive||active?'Sensor Setup: attempts blocked.':s.calibrated?s.gateMode+' · Live · v'+s.version:'Calibrating gate…'}catch(e){}}
$('minimumTimeForm').onsubmit=async e=>{e.preventDefault();const seconds=Number($('minimumTimeSeconds').value);try{await api('/api/settings/minimum-valid-time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'minimumMs='+encodeURIComponent(Math.round(seconds*1000))});refresh();refreshFinish()}catch(x){connection.textContent=x.message}};
$('finishSetupButton').onclick=async()=>{try{await api('/api/finish-gate/sensor-setup',{method:'POST'});setTimeout(refreshFinish,150)}catch(e){connection.textContent=e.message}};
$('finishGateLimitForm').onsubmit=async e=>{e.preventDefault();try{await api('/api/finish-gate/limit',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'limit='+encodeURIComponent($('finishGateLimit').value)});setTimeout(refreshFinish,150)}catch(x){connection.textContent=x.message}};
$('finishUseCurrent').onclick=async()=>{try{await api('/api/finish-gate/limit/current',{method:'POST'});setTimeout(refreshFinish,150)}catch(e){connection.textContent=e.message}};
$('cancelAttemptButton').onclick=async()=>{if(!confirm('Stop current attempt without saving it?'))return;try{await api('/api/attempt/cancel',{method:'POST'});refresh();refreshFinish()}catch(e){connection.textContent=e.message}};
refreshFinish();setInterval(refreshFinish,500);
</script></body></html>
)HTML";

constexpr uint8_t kEspNowBroadcastAddress[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

constexpr char kConfigurationPage[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Gymio configuration</title><style>
:root{color-scheme:dark}body{margin:0;background:#10151b;color:#edf3f7;font:16px system-ui,sans-serif}.app{max-width:620px;margin:auto;padding:16px}.card{background:#1a232d;border-radius:16px;padding:20px;margin:14px 0}h1{font-size:22px}label{display:block;color:#9cb0bf;margin-top:12px}input,button{display:block;width:100%;box-sizing:border-box;padding:14px;margin:8px 0;font-size:16px;border-radius:10px}input{background:#10151b;color:#edf3f7;border:1px solid #3d5262}button{border:0;background:#83df9c;color:#10151b;font-weight:700}.secondary{background:#3d5262;color:#edf3f7}.note{color:#9cb0bf;font-size:14px}a{color:#83df9c}.status{font-weight:700}</style></head><body><main class="app"><h1>GYMIO CONFIGURATION</h1><p><a href="/main">← Main page</a></p>
<section class="card"><h2>Internet connection</h2><p class="note">Gymio access point remains available while the main gate joins this network.</p><p id="wifiStatus" class="status">Loading…</p><p id="wifiDetails" class="note"></p><form id="wifiForm"><label>Wi-Fi name (SSID)</label><input id="ssid" maxlength="32" autocomplete="off" required><label>Password</label><input id="password" type="password" maxlength="64" autocomplete="new-password" placeholder="Leave empty to keep saved password"><button>Save and connect</button></form><button id="disconnect" class="secondary">Forget home Wi-Fi</button></section>
<section class="card"><h2>Administration security</h2><p class="note">When enabled, this page, its configuration API and firmware updates require the configured administrator login.</p><label><input id="auth" type="checkbox" style="display:inline;width:auto"> Require administrator login</label><button id="saveSecurity">Save security setting</button></section>
<section class="card"><h2>Firmware</h2><p>Installed version: <strong id="version">—</strong></p><a href="/update">Open update center</a></section><p id="message" class="note"></p></main><script>
const $=id=>document.getElementById(id);let loaded=false;async function request(url,options){const r=await fetch(url,options);const d=await r.json();if(!r.ok)throw new Error(d.error||'Request failed');return d}async function refresh(){try{const s=await request('/api/configuration');if(!loaded||document.activeElement!==$('ssid'))$('ssid').value=s.ssid||'';if(document.activeElement!==$('auth'))$('auth').checked=s.authenticationEnabled;loaded=true;$('version').textContent=s.version;$('wifiStatus').textContent=s.connected?'Connected':s.connecting?'Connecting…':s.configured?'Offline':'Not configured';$('wifiDetails').textContent=(s.connected?'IP '+s.ip+' · ':'')+'Gymio channel '+s.channel}catch(e){$('message').textContent=e.message}}$('wifiForm').onsubmit=async e=>{e.preventDefault();try{await request('/api/configuration/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent($('ssid').value)+'&password='+encodeURIComponent($('password').value)});$('password').value='';$('message').textContent='Saved. Gymio may reconnect on another channel…';setTimeout(refresh,2000)}catch(e){$('message').textContent=e.message}};$('disconnect').onclick=async()=>{if(!confirm('Forget the saved home Wi-Fi network?'))return;try{await request('/api/configuration/wifi',{method:'DELETE'});$('message').textContent='Home Wi-Fi removed';refresh()}catch(e){$('message').textContent=e.message}};$('saveSecurity').onclick=async()=>{try{await request('/api/configuration/security',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'enabled='+($('auth').checked?'1':'0')});$('message').textContent='Security setting saved'+($('auth').checked?'. Login will be required on the next request.':'')}catch(e){$('message').textContent=e.message}};refresh();setInterval(refresh,3000);
</script></body></html>
)HTML";

constexpr char kOtaPage[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Gymio firmware update</title><style>
:root{color-scheme:dark}body{margin:0;background:#10151b;color:#edf3f7;font:16px system-ui,sans-serif}.app{max-width:620px;margin:auto;padding:16px}.card{background:#1a232d;border-radius:16px;padding:20px;margin:14px 0}h1{font-size:22px}button,input{display:block;width:100%;box-sizing:border-box;padding:14px;margin:12px 0;font-size:16px}button{border:0;border-radius:10px;background:#83df9c;color:#10151b;font-weight:700}.secondary{background:#3d5262;color:#edf3f7}.note{color:#9cb0bf;font-size:14px}a{color:#83df9c}</style></head><body><main class="app"><h1>GYMIO FIRMWARE UPDATE</h1><p><a href="/main">← Main page</a></p>
<section class="card"><h2>Release</h2><p>Installed: <strong id="installed">—</strong></p><p id="releaseStatus" class="note">Press the button to check GitHub.</p><button onclick="checkUpdates()">Check for updates</button></section>
<section class="card"><h2>Main gate</h2><p class="note">Use firmware.bin from the active main-gate PlatformIO environment.</p><form method="POST" action="/update/main" enctype="multipart/form-data"><input type="file" name="firmware" accept=".bin" required><button>Update main gate</button></form></section>
<section class="card"><h2>Finish gate</h2><p class="note">The finish gate temporarily joins Gymio Wi-Fi at 192.168.4.2 for three minutes.</p><button onclick="prepare('finish')">Prepare finish gate</button><a id="finishLink" href="http://192.168.4.2/update" hidden>Open finish-gate updater</a></section>
<section class="card"><h2>TFT display</h2><p class="note">The display temporarily joins Gymio Wi-Fi at 192.168.4.3 for three minutes.</p><button onclick="prepare('tft')">Prepare TFT display</button><a id="tftLink" href="http://192.168.4.3/update" hidden>Open TFT updater</a></section><p id="status" class="note"></p></main><script>
async function checkUpdates(){const s=document.getElementById('releaseStatus');s.textContent='Checking GitHub…';try{const r=await fetch('/api/update/check',{method:'POST'}),d=await r.json();if(!r.ok)throw new Error(d.error||'Check failed');s.textContent=d.updateAvailable?'Update available: '+d.latestVersion:'Up to date · '+d.currentVersion}catch(e){s.textContent=e.message}}
async function prepare(target){const status=document.getElementById('status');status.textContent='Sending maintenance command…';const r=await fetch('/api/ota/prepare?target='+target,{method:'POST'});const data=await r.json();if(!r.ok){status.textContent=data.error||'Failed';return}status.textContent='Device is joining Wi-Fi. Wait about 3 seconds, then open its updater.';setTimeout(()=>document.getElementById(target+'Link').hidden=false,3000)}
fetch('/api/configuration').then(r=>r.json()).then(d=>document.getElementById('installed').textContent=d.version).catch(()=>{});
</script></body></html>
)HTML";
} // namespace

MotoWebServer *MotoWebServer::instance_ = nullptr;

void MotoWebServer::receiveEspNow(const uint8_t *, const uint8_t *data, int length)
{
    if (instance_ == nullptr)
    {
        return;
    }

    if (length == static_cast<int>(sizeof(GateControlProtocol::Command)))
    {
        GateControlProtocol::Command command;
        memcpy(&command, data, sizeof(command));
        if (command.magic != GateControlProtocol::kMagic ||
            command.version != GateControlProtocol::kVersion ||
            command.type != GateControlProtocol::CommandType::CancelAttempt)
        {
            return;
        }

        portENTER_CRITICAL(&instance_->finishGateMux_);
        if (instance_->controlBootId_ != command.bootId ||
            instance_->controlSequence_ != command.sequence)
        {
            instance_->controlBootId_ = command.bootId;
            instance_->controlSequence_ = command.sequence;
            instance_->attemptCancellationPending_ = true;
        }
        portEXIT_CRITICAL(&instance_->finishGateMux_);
        return;
    }

    if (length != static_cast<int>(sizeof(FinishGateProtocol::Frame)))
    {
        return;
    }

    FinishGateProtocol::Frame frame;
    memcpy(&frame, data, sizeof(frame));
    if (frame.magic != FinishGateProtocol::kMagic || frame.version != FinishGateProtocol::kVersion)
    {
        return;
    }

    portENTER_CRITICAL(&instance_->finishGateMux_);
    if (instance_->finishBootId_ != frame.bootId)
    {
        instance_->finishBootId_ = frame.bootId;
        instance_->finishCrossingCount_ = frame.crossingCount;
        instance_->finishCrossingPending_ = false;
    }
    else if (instance_->finishCrossingCount_ != frame.crossingCount)
    {
        instance_->finishCrossingCount_ = frame.crossingCount;
        instance_->finishCrossingPending_ = true;
    }
    instance_->finishFlags_ = frame.flags;
    instance_->finishDistanceMm_ = frame.distanceMm;
    instance_->finishFilteredDistanceMm_ = frame.filteredDistanceMm;
    instance_->finishTriggerDistanceMm_ = frame.triggerDistanceMm;
    instance_->finishAppliedCommandId_ = frame.appliedCommandId;
    if (instance_->finishCommandPending_ &&
        frame.appliedCommandId == instance_->finishCommandId_)
    {
        instance_->finishCommandPending_ = false;
    }
    instance_->lastFinishPacketMs_ = millis();
    portEXIT_CRITICAL(&instance_->finishGateMux_);
}

bool MotoWebServer::begin()
{
    instance_ = this;
    // Avoid accepting a stale command acknowledgement after the main gate
    // reboots while the finish gate remains powered.
    finishCommandId_ = esp_random();
    if (preferences_.begin("gymio", false))
    {
        minimumValidTimeMs_ = preferences_.getUInt("minValidMs", 0);
        stationSsid_ = preferences_.getString("staSsid", "");
        stationPassword_ = preferences_.getString("staPass", "");
        adminAuthenticationEnabled_ = preferences_.getBool(
            "adminAuth", Config::kAdminAuthenticationEnabledByDefault);
        if (stationSsid_.isEmpty() && !preferences_.getBool("staConfigured", false))
        {
            stationSsid_ = Config::kDebugDefaultStationSsid;
            stationPassword_ = Config::kDebugDefaultStationPassword;
            preferences_.putString("staSsid", stationSsid_);
            preferences_.putString("staPass", stationPassword_);
            preferences_.putBool("staConfigured", true);
        }
    }
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(
            Config::kWifiAccessPointName,
            Config::kWifiAccessPointPassword,
            Config::kWifiAccessPointChannel,
            false,
            Config::kWifiAccessPointMaxConnections))
    {
        return false;
    }

    // Keep the SoftAP security mode deterministic for embedded clients.
    wifi_config_t accessPointConfig{};
    if (esp_wifi_get_config(WIFI_IF_AP, &accessPointConfig) != ESP_OK)
    {
        return false;
    }
    accessPointConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
    accessPointConfig.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    if (esp_wifi_set_config(WIFI_IF_AP, &accessPointConfig) != ESP_OK)
    {
        return false;
    }

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/main", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/main/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/rider", HTTP_GET, [this]() { handleRiderRoot(); });
    server_.on("/rider/", HTTP_GET, [this]() { handleRiderRoot(); });
    server_.on("/update", HTTP_GET, [this]() { handleOtaPage(); });
    server_.on("/configuration", HTTP_GET, [this]() { handleConfigurationPage(); });
    server_.on("/api/configuration", HTTP_GET, [this]() { handleConfigurationStatus(); });
    server_.on("/api/configuration/wifi", HTTP_POST, [this]() { handleSaveWifiConfiguration(); });
    server_.on("/api/configuration/wifi", HTTP_DELETE, [this]() { handleDisconnectWifi(); });
    server_.on("/api/configuration/security", HTTP_POST, [this]() { handleSaveSecurityConfiguration(); });
    server_.on("/api/update/check", HTTP_POST, [this]() { handleCheckForUpdates(); });
    server_.on("/api/ota/prepare", HTTP_POST, [this]() { handlePrepareRemoteOta(); });
    server_.on(
        "/update/main", HTTP_POST,
        [this]() { handleMainOtaComplete(); },
        [this]() { handleMainOtaUpload(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/history", HTTP_GET, [this]() { handleHistory(); });
    server_.on("/api/attempts/invalidate", HTTP_POST, [this]() { handleInvalidateAttempt(); });
    server_.on("/api/attempts/revalidate", HTTP_POST, [this]() { handleRevalidateAttempt(); });
    server_.on("/api/attempts/assign", HTTP_POST, [this]() { handleAssignAttempt(); });
    server_.on("/api/attempt/cancel", HTTP_POST, [this]() { handleCancelAttempt(); });
    server_.on("/api/settings/minimum-valid-time", HTTP_POST, [this]() { handleSetMinimumValidTime(); });
    server_.on("/api/riders", HTTP_GET, [this]() { handleRiders(); });
    server_.on("/api/riders", HTTP_POST, [this]() { handleAddRider(); });
    server_.on("/api/riders", HTTP_DELETE, [this]() { handleRemoveRider(); });
    server_.on("/api/riders/select", HTTP_POST, [this]() { handleSelectRider(); });
    server_.on("/api/riders/prepare", HTTP_POST, [this]() { handlePrepareRider(); });
    server_.on("/api/riders/logout", HTTP_POST, [this]() { handleLogoutRider(); });
    server_.on("/api/gate/limit", HTTP_POST, [this]() { handleSetGateLimit(); });
    server_.on("/api/gate/limit/current", HTTP_POST, [this]() { handleUseCurrentGateLimit(); });
    server_.on("/api/sensor-setup", HTTP_POST, [this]() { handleSensorSetup(); });
    server_.on("/api/finish-gate/sensor-setup", HTTP_POST, [this]() { handleFinishSensorSetup(); });
    server_.on("/api/finish-gate/limit", HTTP_POST, [this]() { handleSetFinishGateLimit(); });
    server_.on("/api/finish-gate/limit/current", HTTP_POST, [this]() { handleUseCurrentFinishGateLimit(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "Not found"); });
    server_.begin();
    if (esp_now_init() == ESP_OK)
    {
        esp_now_peer_info_t displayPeer{};
        memcpy(displayPeer.peer_addr, kEspNowBroadcastAddress, ESP_NOW_ETH_ALEN);
        // Channel 0 follows the current AP/STA radio channel. AP+STA uses the
        // channel selected by the upstream access point.
        displayPeer.channel = 0;
        displayPeer.ifidx = WIFI_IF_AP;
        displayPeer.encrypt = false;
        espNowReady_ = esp_now_add_peer(&displayPeer) == ESP_OK;
        if (espNowReady_)
        {
            espNowReady_ = esp_now_register_recv_cb(receiveEspNow) == ESP_OK;
        }
    }
    Serial.println(espNowReady_ ? "Display ESP-NOW broadcaster ready."
                                : "Display ESP-NOW broadcaster initialization failed.");

    restartMdns();

    if (!stationSsid_.isEmpty())
    {
        startStationConnection();
    }

    return true;
}

void MotoWebServer::beginAttempt()
{
    attemptRider_ = riders_.selected();
    attemptRiderLocked_ = true;
}

void MotoWebServer::recordAttempt(uint32_t durationMs)
{
    if (!attemptRiderLocked_)
    {
        beginAttempt();
    }

    history_[nextHistoryIndex_] = {++attemptNumber_, durationMs, attemptRider_, true};
    nextHistoryIndex_ = (nextHistoryIndex_ + 1) % kHistoryCapacity;

    if (historyCount_ < kHistoryCapacity)
    {
        ++historyCount_;
    }

    if (durationMs >= minimumValidTimeMs_)
    {
        riders_.recordResult(attemptRider_.id, durationMs);
    }
    attemptRider_.sessionBestMs = riders_.find(attemptRider_.id).sessionBestMs;

    // Each new attempt starts anonymous unless a rider is explicitly selected.
    riders_.select(0);
}

void MotoWebServer::cancelAttempt()
{
    attemptRiderLocked_ = false;
    attemptRider_ = {};
    riders_.select(0);
}

bool MotoWebServer::takeAttemptCancellation()
{
    portENTER_CRITICAL(&finishGateMux_);
    const bool pending = attemptCancellationPending_;
    attemptCancellationPending_ = false;
    portEXIT_CRITICAL(&finishGateMux_);
    return pending;
}

bool MotoWebServer::takeGateLimitUpdate(int16_t &limitMm)
{
    if (!gateLimitUpdatePending_)
    {
        return false;
    }

    limitMm = pendingGateLimitMm_;
    gateLimitUpdatePending_ = false;
    return true;
}

bool MotoWebServer::isSensorSetupActive() const
{
    return sensorSetupActive_;
}

bool MotoWebServer::isFinishGateConnected(uint32_t nowMs)
{
    uint32_t lastPacketMs;
    portENTER_CRITICAL(&finishGateMux_);
    lastPacketMs = lastFinishPacketMs_;
    portEXIT_CRITICAL(&finishGateMux_);
    return lastPacketMs != 0 && nowMs - lastPacketMs <= Config::kFinishGateTimeoutMs;
}

bool MotoWebServer::isFinishGateSetupActive()
{
    uint8_t flags;
    portENTER_CRITICAL(&finishGateMux_);
    flags = finishFlags_;
    portEXIT_CRITICAL(&finishGateMux_);
    return isFinishGateConnected(millis()) &&
           (flags & FinishGateProtocol::SetupActive) != 0;
}

bool MotoWebServer::takeFinishCrossing()
{
    portENTER_CRITICAL(&finishGateMux_);
    const bool pending = finishCrossingPending_;
    finishCrossingPending_ = false;
    portEXIT_CRITICAL(&finishGateMux_);
    return pending;
}

const char *MotoWebServer::displayRiderName() const
{
    return riderForStatus().name;
}

bool MotoWebServer::isUpdateModeActive(uint32_t nowMs) const
{
    return updateModeUntilMs_ != 0 && static_cast<int32_t>(updateModeUntilMs_ - nowMs) > 0;
}

const char *MotoWebServer::stationSsid() const
{
    return stationSsid_.c_str();
}

String MotoWebServer::stationIpAddress() const
{
    return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

const char *MotoWebServer::updateModeStatus() const
{
    return updateModeStatus_.c_str();
}

void MotoWebServer::handleClient()
{
    server_.handleClient();
    serviceStationConnection();
    if (otaRestartAtMs_ != 0 && static_cast<int32_t>(millis() - otaRestartAtMs_) >= 0)
    {
        ESP.restart();
    }
}

bool MotoWebServer::isMdnsStarted() const
{
    return mdnsStarted_;
}

void MotoWebServer::updateStatus(
    int16_t distanceMm,
    int16_t filteredDistanceMm,
    int16_t triggerDistanceMm,
    bool calibrated,
    TimerState timerState,
    uint32_t elapsedMs,
    uint32_t resultMs)
{
    status_.distanceMm = distanceMm;
    status_.filteredDistanceMm = filteredDistanceMm;
    status_.triggerDistanceMm = triggerDistanceMm;
    status_.calibrated = calibrated;
    status_.timerState = timerState;
    status_.elapsedMs = elapsedMs;
    status_.resultMs = resultMs;
    broadcastDisplayStatus();
}

void MotoWebServer::broadcastDisplayStatus()
{
    const uint32_t nowMs = millis();
    if (nowMs - lastDisplayBroadcastMs_ < Config::kDisplayBroadcastIntervalMs)
    {
        return;
    }
    lastDisplayBroadcastMs_ = nowMs;

    const uint32_t timeMs = status_.timerState == TimerState::Result ? status_.resultMs : status_.elapsedMs;
    if (!espNowReady_)
    {
        return;
    }

    DisplayStatusProtocol::Frame frame;
    frame.sequence = ++displaySequence_;
    frame.gateCount = isFinishGateConnected(nowMs) ? 2 : 1;
    frame.timeMs = timeMs;
    frame.absoluteBestMs = absoluteBest();
    strlcpy(frame.state, timerStateText(), sizeof(frame.state));
    strlcpy(frame.riderName, riderForStatus().name, sizeof(frame.riderName));
    strlcpy(frame.absoluteBestRider, absoluteBestRiderName(), sizeof(frame.absoluteBestRider));
    esp_now_send(kEspNowBroadcastAddress,
                 reinterpret_cast<const uint8_t *>(&frame),
                 sizeof(frame));

    if (otaCommandUntilMs_ != 0 &&
        static_cast<int32_t>(otaCommandUntilMs_ - nowMs) > 0)
    {
        OtaControlProtocol::Command command;
        command.target = otaTarget_;
        command.commandId = otaCommandId_;
        esp_now_send(kEspNowBroadcastAddress,
                     reinterpret_cast<const uint8_t *>(&command),
                     sizeof(command));
    }

    if (finishCommandPending_)
    {
        FinishGateProtocol::Command command;
        command.type = static_cast<FinishGateProtocol::CommandType>(finishCommandType_);
        command.commandId = finishCommandId_;
        command.value = finishCommandValue_;
        esp_now_send(kEspNowBroadcastAddress,
                     reinterpret_cast<const uint8_t *>(&command),
                     sizeof(command));
    }
}

void MotoWebServer::handleRoot()
{
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server_.sendHeader("Pragma", "no-cache");
    server_.send_P(200, "text/html", kPage);
}

void MotoWebServer::handleRiderRoot()
{
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server_.sendHeader("Pragma", "no-cache");
    server_.send_P(200, "text/html", kRiderPage);
}

bool MotoWebServer::authenticateOta()
{
    if (!adminAuthenticationEnabled_)
    {
        return true;
    }
    if (server_.authenticate(Config::kOtaAdminUser, Config::kOtaAdminPassword))
    {
        return true;
    }
    server_.requestAuthentication();
    return false;
}

bool MotoWebServer::authenticateAdministration()
{
    return authenticateOta();
}

void MotoWebServer::handleConfigurationPage()
{
    if (!authenticateAdministration())
        return;
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/html", kConfigurationPage);
}

void MotoWebServer::handleConfigurationStatus()
{
    if (!authenticateAdministration())
        return;
    const bool connected = WiFi.status() == WL_CONNECTED;
    const bool connecting = !stationSsid_.isEmpty() && !connected && stationConnectStartedMs_ != 0 &&
                            millis() - stationConnectStartedMs_ < Config::kStationConnectTimeoutMs;
    String response = F("{\"version\":\"");
    response += Version::kString;
    response += F("\",\"ssid\":\"");
    // SSIDs cannot contain a quote in the ESP API without breaking this small
    // JSON response; replace control/quote characters defensively.
    String safeSsid = stationSsid_;
    safeSsid.replace("\\", "_");
    safeSsid.replace("\"", "_");
    response += safeSsid;
    response += F("\",\"configured\":");
    response += stationSsid_.isEmpty() ? F("false") : F("true");
    response += F(",\"connected\":");
    response += connected ? F("true") : F("false");
    response += F(",\"connecting\":");
    response += connecting ? F("true") : F("false");
    response += F(",\"ip\":\"");
    response += connected ? WiFi.localIP().toString() : String();
    response += F("\",\"channel\":");
    response += WiFi.channel();
    response += F(",\"authenticationEnabled\":");
    response += adminAuthenticationEnabled_ ? F("true}") : F("false}");
    server_.send(200, "application/json", response);
}

void MotoWebServer::handleSaveWifiConfiguration()
{
    if (!authenticateAdministration())
        return;
    const String ssid = server_.arg("ssid");
    if (ssid.isEmpty() || ssid.length() > 32)
    {
        server_.send(400, "application/json", "{\"error\":\"SSID is required\"}");
        return;
    }
    stationSsid_ = ssid;
    const String password = server_.arg("password");
    if (!password.isEmpty())
        stationPassword_ = password;
    preferences_.putString("staSsid", stationSsid_);
    preferences_.putString("staPass", stationPassword_);
    preferences_.putBool("staConfigured", true);
    server_.send(200, "application/json", "{\"ok\":true}");
    // Let the TCP response reach the browser before STA association is allowed
    // to move the shared AP/ESP-NOW radio onto another channel.
    stationConnectScheduledAtMs_ = millis() + 750;
}

void MotoWebServer::handleDisconnectWifi()
{
    if (!authenticateAdministration())
        return;
    WiFi.disconnect(false, true);
    stationSsid_ = "";
    stationPassword_ = "";
    stationConnectStartedMs_ = 0;
    preferences_.remove("staSsid");
    preferences_.remove("staPass");
    preferences_.putBool("staConfigured", true);
    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleSaveSecurityConfiguration()
{
    if (!authenticateAdministration())
        return;
    adminAuthenticationEnabled_ = server_.arg("enabled") == "1";
    preferences_.putBool("adminAuth", adminAuthenticationEnabled_);
    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::startStationConnection()
{
    if (stationSsid_.isEmpty())
        return;
    WiFi.begin(stationSsid_.c_str(), stationPassword_.c_str());
    stationConnectStartedMs_ = millis();
    stationLastAttemptMs_ = stationConnectStartedMs_;
    Serial.print("Connecting STA to: ");
    Serial.println(stationSsid_);
}

void MotoWebServer::serviceStationConnection()
{
    const uint32_t nowMs = millis();
    const bool stationConnected = WiFi.status() == WL_CONNECTED;
    if (stationConnected != stationWasConnected_)
    {
        stationWasConnected_ = stationConnected;
        // ESPmDNS is initialized before the optional STA interface has an IP.
        // Rebind it whenever that interface appears or disappears so
        // gymio.local works on both the Gymio AP and the upstream LAN.
        restartMdns();
    }
    if (stationConnectScheduledAtMs_ != 0 &&
        static_cast<int32_t>(nowMs - stationConnectScheduledAtMs_) >= 0)
    {
        stationConnectScheduledAtMs_ = 0;
        startStationConnection();
        return;
    }
    if (stationSsid_.isEmpty() || WiFi.status() == WL_CONNECTED)
        return;
    if (nowMs - stationLastAttemptMs_ >= Config::kStationReconnectIntervalMs)
    {
        WiFi.disconnect(false, false);
        startStationConnection();
    }
}

void MotoWebServer::restartMdns()
{
    if (mdnsStarted_)
    {
        MDNS.end();
        mdnsStarted_ = false;
    }
    mdnsStarted_ = MDNS.begin(Config::kMdnsHostname);
    if (mdnsStarted_)
    {
        MDNS.addService("http", "tcp", 80);
        Serial.print("mDNS available at http://");
        Serial.print(Config::kMdnsHostname);
        Serial.println(".local");
    }
    else
    {
        Serial.println("mDNS restart failed.");
    }
}

void MotoWebServer::handleOtaPage()
{
    if (!authenticateOta())
        return;
    updateModeUntilMs_ = millis() + Config::kOtaMaintenanceTimeoutMs;
    updateModeStatus_ = WiFi.status() == WL_CONNECTED ? "READY FOR UPDATE" : "HOME WIFI OFFLINE";
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/html", kOtaPage);
}

void MotoWebServer::handleCheckForUpdates()
{
    if (!authenticateAdministration())
        return;
    updateModeUntilMs_ = millis() + Config::kOtaMaintenanceTimeoutMs;
    if (WiFi.status() != WL_CONNECTED)
    {
        updateModeStatus_ = "HOME WIFI OFFLINE";
        server_.send(503, "application/json", "{\"error\":\"Home Wi-Fi is offline\"}");
        return;
    }
    if (status_.timerState != TimerState::Ready || sensorSetupActive_ || isFinishGateSetupActive())
    {
        updateModeStatus_ = "TIMER NOT READY";
        server_.send(409, "application/json", "{\"error\":\"Update check requires READY state\"}");
        return;
    }

    updateModeStatus_ = "CHECKING GITHUB...";
    WiFiClientSecure client;
    // Prototype transport: target validation still protects installation.
    // Pin GitHub's CA certificate before production deployment.
    client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    http.begin(client, Config::kGithubLatestReleaseApi);
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("User-Agent", "Gymio-main-gate");
    const int code = http.GET();
    const String payload = code == HTTP_CODE_OK ? http.getString() : String();
    http.end();
    if (code != HTTP_CODE_OK)
    {
        updateModeStatus_ = "GITHUB CHECK FAILED";
        server_.send(502, "application/json", "{\"error\":\"GitHub release check failed\"}");
        return;
    }

    const int key = payload.indexOf("\"tag_name\"");
    const int colon = key >= 0 ? payload.indexOf(':', key) : -1;
    const int quote = colon >= 0 ? payload.indexOf('"', colon) : -1;
    const int endQuote = quote >= 0 ? payload.indexOf('"', quote + 1) : -1;
    if (quote < 0 || endQuote < 0)
    {
        updateModeStatus_ = "BAD RELEASE DATA";
        server_.send(502, "application/json", "{\"error\":\"Invalid GitHub response\"}");
        return;
    }
    String latest = payload.substring(quote + 1, endQuote);
    String normalized = latest;
    if (normalized.startsWith("v"))
        normalized.remove(0, 1);
    const bool available = normalized != Version::kString;
    updateModeStatus_ = available ? String("AVAILABLE ") + latest : "FIRMWARE CURRENT";
    String response = String("{\"currentVersion\":\"") + Version::kString +
                      "\",\"latestVersion\":\"" + latest +
                      "\",\"updateAvailable\":" + (available ? "true}" : "false}");
    server_.send(200, "application/json", response);
}

void MotoWebServer::handlePrepareRemoteOta()
{
    if (!authenticateOta())
        return;
    if (status_.timerState != TimerState::Ready || sensorSetupActive_ || isFinishGateSetupActive())
    {
        server_.send(409, "application/json", "{\"error\":\"OTA requires READY state and closed sensor setup\"}");
        return;
    }

    const String target = server_.arg("target");
    if (target == "finish")
    {
        if (!isFinishGateConnected(millis()))
        {
            server_.send(409, "application/json", "{\"error\":\"Finish gate is offline\"}");
            return;
        }
        otaTarget_ = OtaControlProtocol::Target::FinishGate;
    }
    else if (target == "tft")
    {
        otaTarget_ = OtaControlProtocol::Target::TftDisplay;
    }
    else
    {
        server_.send(400, "application/json", "{\"error\":\"Unknown OTA target\"}");
        return;
    }

    otaCommandId_ = esp_random();
    otaCommandUntilMs_ = millis() + 5000;
    lastDisplayBroadcastMs_ = 0;
    broadcastDisplayStatus();
    server_.send(200, "application/json", target == "finish"
                                                   ? "{\"ok\":true,\"url\":\"http://192.168.4.2/update\"}"
                                                   : "{\"ok\":true,\"url\":\"http://192.168.4.3/update\"}");
}

void MotoWebServer::handleMainOtaUpload()
{
    if (!server_.authenticate(Config::kOtaAdminUser, Config::kOtaAdminPassword))
        return;

    HTTPUpload &upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        otaUploadSucceeded_ = false;
        otaTargetValidator_.reset();
        otaUploadStarted_ = status_.timerState == TimerState::Ready &&
                            !sensorSetupActive_ && !isFinishGateSetupActive() &&
                            Update.begin(UPDATE_SIZE_UNKNOWN);
    }
    else if (upload.status == UPLOAD_FILE_WRITE && otaUploadStarted_)
    {
        otaTargetValidator_.consume(upload.buf, upload.currentSize);
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            Update.abort();
            otaUploadStarted_ = false;
        }
    }
    else if (upload.status == UPLOAD_FILE_END && otaUploadStarted_)
    {
        otaUploadSucceeded_ = otaTargetValidator_.found() && Update.end(true);
        if (!otaUploadSucceeded_)
        {
            Update.abort();
        }
        otaUploadStarted_ = false;
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Update.abort();
        otaUploadStarted_ = false;
    }
}

void MotoWebServer::handleMainOtaComplete()
{
    if (!authenticateOta())
        return;
    if (!otaUploadSucceeded_)
    {
        server_.send(500, "text/plain", "Update failed or timer was not READY. Existing firmware is unchanged.");
        return;
    }
    server_.send(200, "text/plain", "Main-gate update installed. Restarting.");
    otaRestartAtMs_ = millis() + 750;
}

void MotoWebServer::handleStatus()
{
    const uint32_t timeMs = status_.timerState == TimerState::Result ? status_.resultMs : status_.elapsedMs;
    const bool finishConnected = isFinishGateConnected(millis());
    int16_t finishDistanceMm;
    int16_t finishFilteredDistanceMm;
    int16_t finishTriggerDistanceMm;
    portENTER_CRITICAL(&finishGateMux_);
    finishDistanceMm = finishDistanceMm_;
    finishFilteredDistanceMm = finishFilteredDistanceMm_;
    finishTriggerDistanceMm = finishTriggerDistanceMm_;
    portEXIT_CRITICAL(&finishGateMux_);
    char response[704];
    snprintf(
        response,
        sizeof(response),
        "{\"version\":\"%s\",\"distanceMm\":%d,\"filteredDistanceMm\":%d,\"triggerDistanceMm\":%d,\"calibrated\":%s,\"sensorSetupActive\":%s,\"gateMode\":\"%s\",\"finishGateConnected\":%s,\"finishSensorSetupActive\":%s,\"finishDistanceMm\":%d,\"finishFilteredDistanceMm\":%d,\"finishTriggerDistanceMm\":%d,\"minimumValidTimeMs\":%lu,\"state\":\"%s\",\"timeMs\":%lu,\"riderName\":\"%s\",\"sessionBestMs\":%lu,\"absoluteBestMs\":%lu,\"absoluteBestRider\":\"%s\"}",
        Version::kString,
        status_.distanceMm,
        status_.filteredDistanceMm,
        status_.triggerDistanceMm,
        status_.calibrated ? "true" : "false",
        sensorSetupActive_ ? "true" : "false",
        finishConnected ? "TWO GATES" : "ONE GATE",
        finishConnected ? "true" : "false",
        isFinishGateSetupActive() ? "true" : "false",
        finishDistanceMm,
        finishFilteredDistanceMm,
        finishTriggerDistanceMm,
        static_cast<unsigned long>(minimumValidTimeMs_),
        timerStateText(),
        static_cast<unsigned long>(timeMs),
        riderForStatus().name,
        static_cast<unsigned long>(bestForRider(riderForStatus().id)),
        static_cast<unsigned long>(absoluteBest()),
        absoluteBestRiderName());
    server_.send(200, "application/json", response);
}

void MotoWebServer::handleHistory()
{
    char response[4096];
    size_t used = snprintf(response, sizeof(response), "{\"attempts\":[");

    // Return newest first. The ring buffer keeps only the most recent runs.
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const uint8_t index = (nextHistoryIndex_ + kHistoryCapacity - 1 - i) % kHistoryCapacity;
        const Attempt &attempt = history_[index];
        used += snprintf(
            response + used,
            sizeof(response) - used,
            "%s{\"number\":%lu,\"durationMs\":%lu,\"riderId\":%lu,\"riderName\":\"%s\",\"valid\":%s,\"autoInvalid\":%s}",
            i == 0 ? "" : ",",
            static_cast<unsigned long>(attempt.number),
            static_cast<unsigned long>(attempt.durationMs),
            static_cast<unsigned long>(attempt.rider.id),
            attempt.rider.name,
            isAttemptValid(attempt) ? "true" : "false",
            isAttemptAutomaticallyInvalid(attempt) ? "true" : "false");
    }

    snprintf(response + used, sizeof(response) - used, "]}");
    server_.send(200, "application/json", response);
}

void MotoWebServer::handleInvalidateAttempt()
{
    handleSetAttemptValidity(false);
}

void MotoWebServer::handleRevalidateAttempt()
{
    handleSetAttemptValidity(true);
}

void MotoWebServer::handleCancelAttempt()
{
    if (status_.timerState != TimerState::RunningAwaitingClear &&
        status_.timerState != TimerState::RunningArmed)
    {
        server_.send(409, "application/json", "{\"error\":\"There is no active attempt.\"}");
        return;
    }

    portENTER_CRITICAL(&finishGateMux_);
    attemptCancellationPending_ = true;
    portEXIT_CRITICAL(&finishGateMux_);
    server_.send(202, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleSetAttemptValidity(bool valid)
{
    if (!server_.hasArg("number"))
    {
        server_.send(400, "application/json", "{\"error\":\"Missing attempt number.\"}");
        return;
    }

    const uint32_t number = strtoul(server_.arg("number").c_str(), nullptr, 10);
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const uint8_t index = (nextHistoryIndex_ + kHistoryCapacity - 1 - i) % kHistoryCapacity;
        Attempt &attempt = history_[index];
        if (attempt.number == number)
        {
            if (valid && isAttemptAutomaticallyInvalid(attempt))
            {
                server_.send(409, "application/json", "{\"error\":\"Attempt is below the configured minimum time.\"}");
                return;
            }
            if (attempt.manuallyValid == valid)
            {
                server_.send(
                    409,
                    "application/json",
                    valid ? "{\"error\":\"Attempt is already valid.\"}" : "{\"error\":\"Attempt is already invalid.\"}");
                return;
            }

            attempt.manuallyValid = valid;
            server_.send(200, "application/json", "{\"ok\":true}");
            return;
        }
    }

    server_.send(404, "application/json", "{\"error\":\"Attempt not found.\"}");
}

void MotoWebServer::handleSetMinimumValidTime()
{
    if (!server_.hasArg("minimumMs"))
    {
        server_.send(400, "application/json", "{\"error\":\"Missing minimum time.\"}");
        return;
    }

    const String argument = server_.arg("minimumMs");
    char *end = nullptr;
    const unsigned long value = strtoul(argument.c_str(), &end, 10);
    constexpr unsigned long kMaximumMinimumTimeMs = 3600000UL;
    if (argument.length() == 0 || *end != '\0' || value > kMaximumMinimumTimeMs)
    {
        server_.send(400, "application/json", "{\"error\":\"Enter a minimum from 0 to 3600 seconds.\"}");
        return;
    }

    minimumValidTimeMs_ = static_cast<uint32_t>(value);
    preferences_.putUInt("minValidMs", minimumValidTimeMs_);
    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleAssignAttempt()
{
    if (!server_.hasArg("number") || !server_.hasArg("riderId"))
    {
        server_.send(400, "application/json", "{\"error\":\"Missing attempt number or rider.\"}");
        return;
    }

    const uint32_t number = strtoul(server_.arg("number").c_str(), nullptr, 10);
    const uint32_t riderId = strtoul(server_.arg("riderId").c_str(), nullptr, 10);
    const Rider &rider = riders_.find(riderId);
    if (riderId != 0 && rider.id != riderId)
    {
        server_.send(404, "application/json", "{\"error\":\"Registered rider not found.\"}");
        return;
    }

    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        Attempt &attempt = history_[i];
        if (attempt.number == number)
        {
            attempt.rider = rider;
            server_.send(200, "application/json", "{\"ok\":true}");
            return;
        }
    }

    server_.send(404, "application/json", "{\"error\":\"Attempt not found.\"}");
}

void MotoWebServer::handleRiders()
{
    const Rider &phoneRider = server_.hasArg("clientId")
                                   ? riders_.findByClientId(server_.arg("clientId").c_str())
                                   : riders_.find(0);
    char response[4096];
    size_t used = snprintf(
        response,
        sizeof(response),
        "{\"selectedId\":%lu,\"preparedId\":%lu,\"phoneRiderId\":%lu,\"riders\":[",
        static_cast<unsigned long>(riders_.selected().id),
        static_cast<unsigned long>(riders_.selected().id),
        static_cast<unsigned long>(phoneRider.id));

    for (uint8_t riderIndex = 0; riderIndex <= riders_.count(); ++riderIndex)
    {
        const Rider &rider = riderIndex == 0 ? riders_.find(0) : riders_.at(riderIndex - 1);
        used += snprintf(
            response + used,
            sizeof(response) - used,
            "%s{\"id\":%lu,\"name\":\"%s\",\"sessionBestMs\":%lu,\"lastTimesMs\":[",
            riderIndex == 0 ? "" : ",",
            static_cast<unsigned long>(rider.id),
            rider.name,
            static_cast<unsigned long>(bestForRider(rider.id)));

        uint8_t recentCount = 0;
        for (uint8_t historyOffset = 0; historyOffset < historyCount_ && recentCount < 5; ++historyOffset)
        {
            const uint8_t historyIndex =
                (nextHistoryIndex_ + kHistoryCapacity - 1 - historyOffset) % kHistoryCapacity;
            const Attempt &attempt = history_[historyIndex];
            if (!isAttemptValid(attempt) || attempt.rider.id != rider.id)
            {
                continue;
            }
            used += snprintf(
                response + used,
                sizeof(response) - used,
                "%s%lu",
                recentCount == 0 ? "" : ",",
                static_cast<unsigned long>(attempt.durationMs));
            ++recentCount;
        }
        used += snprintf(response + used, sizeof(response) - used, "]}");
    }

    snprintf(response + used, sizeof(response) - used, "]}");
    server_.send(200, "application/json", response);
}

void MotoWebServer::handleAddRider()
{
    uint32_t newId = 0;
    const String clientIdArgument = server_.hasArg("clientId") ? server_.arg("clientId") : String();
    const char *clientId = clientIdArgument.length() > 0 ? clientIdArgument.c_str() : nullptr;
    if (!server_.hasArg("name") || !riders_.add(server_.arg("name").c_str(), clientId, newId))
    {
        server_.send(400, "application/json", "{\"error\":\"Invalid name, phone identifier, or this phone is already registered.\"}");
        return;
    }

    char response[64];
    // Riders created by the operator page are selected immediately. A phone
    // registration remains unprepared until the rider presses Prepare.
    if (clientId == nullptr)
    {
        riders_.select(newId);
    }
    snprintf(response, sizeof(response), "{\"ok\":true,\"riderId\":%lu}", static_cast<unsigned long>(newId));
    server_.send(201, "application/json", response);
}

void MotoWebServer::handleSelectRider()
{
    if (!server_.hasArg("id") || !riders_.select(strtoul(server_.arg("id").c_str(), nullptr, 10)))
    {
        server_.send(400, "application/json", "{\"error\":\"Unknown rider.\"}");
        return;
    }

    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handlePrepareRider()
{
    if (!server_.hasArg("clientId") || status_.timerState != TimerState::Ready || sensorSetupActive_)
    {
        server_.send(409, "application/json", "{\"error\":\"Preparation is available only while the timer is READY.\"}");
        return;
    }

    const Rider &rider = riders_.findByClientId(server_.arg("clientId").c_str());
    if (rider.id == 0 || !riders_.select(rider.id))
    {
        server_.send(404, "application/json", "{\"error\":\"Register this phone first.\"}");
        return;
    }

    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleLogoutRider()
{
    if (!server_.hasArg("clientId"))
    {
        server_.send(400, "application/json", "{\"error\":\"Missing phone identifier.\"}");
        return;
    }

    const Rider &rider = riders_.findByClientId(server_.arg("clientId").c_str());
    if (rider.id == 0)
    {
        server_.send(404, "application/json", "{\"error\":\"This phone is not registered.\"}");
        return;
    }
    if (attemptRiderLocked_ && status_.timerState != TimerState::Ready && attemptRider_.id == rider.id)
    {
        server_.send(409, "application/json", "{\"error\":\"Cannot log out during an active attempt.\"}");
        return;
    }

    riders_.remove(rider.id);
    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleRemoveRider()
{
    const uint32_t riderId = server_.hasArg("id") ? strtoul(server_.arg("id").c_str(), nullptr, 10) : 0;
    if (riderId == 0)
    {
        server_.send(400, "application/json", "{\"error\":\"ANON cannot be removed.\"}");
        return;
    }

    if (attemptRiderLocked_ && status_.timerState != TimerState::Ready && attemptRider_.id == riderId)
    {
        server_.send(409, "application/json", "{\"error\":\"This rider is assigned to the active attempt.\"}");
        return;
    }

    if (!riders_.remove(riderId))
    {
        server_.send(404, "application/json", "{\"error\":\"Unknown rider.\"}");
        return;
    }

    server_.send(200, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleSetGateLimit()
{
    if (!sensorSetupActive_ || !status_.calibrated || status_.timerState != TimerState::Ready || !server_.hasArg("limit"))
    {
        server_.send(409, "application/json", "{\"error\":\"Enable Sensor Setup while the calibrated timer is READY.\"}");
        return;
    }

    const String limitArgument = server_.arg("limit");
    char *end = nullptr;
    const long value = strtol(limitArgument.c_str(), &end, 10);
    if (limitArgument.length() == 0 || *end != '\0' || value < 0 || value >= Config::kTofNoTargetDistanceMm)
    {
        server_.send(400, "application/json", "{\"error\":\"Enter a limit from 0 to 3999 mm.\"}");
        return;
    }

    pendingGateLimitMm_ = static_cast<int16_t>(value);
    gateLimitUpdatePending_ = true;
    server_.send(202, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleUseCurrentGateLimit()
{
    if (!sensorSetupActive_ || !status_.calibrated || status_.timerState != TimerState::Ready)
    {
        server_.send(409, "application/json", "{\"error\":\"Enable Sensor Setup while the calibrated timer is READY.\"}");
        return;
    }

    // Use the newest sensor sample, not the moving average. During setup the
    // average may still contain previous far readings and lag behind the object.
    const int16_t normalizedReferenceMm =
        status_.filteredDistanceMm < 0 ? Config::kTofNoTargetDistanceMm
                                       : status_.filteredDistanceMm;
    pendingGateLimitMm_ = normalizedReferenceMm - Config::kGateApproachMm;
    gateLimitUpdatePending_ = true;
    server_.send(202, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleSensorSetup()
{
    if (!status_.calibrated || status_.timerState != TimerState::Ready)
    {
        server_.send(409, "application/json", "{\"error\":\"Sensor Setup is available only when the calibrated timer is READY.\"}");
        return;
    }

    sensorSetupActive_ = !sensorSetupActive_;
    server_.send(200, "application/json", sensorSetupActive_ ? "{\"active\":true}" : "{\"active\":false}");
}

void MotoWebServer::sendFinishCommand(uint8_t type, int16_t value)
{
    finishCommandType_ = type;
    finishCommandValue_ = value;
    ++finishCommandId_;
    finishCommandPending_ = true;
}

void MotoWebServer::handleFinishSensorSetup()
{
    if (!isFinishGateConnected(millis()) || status_.timerState != TimerState::Ready)
    {
        server_.send(409, "application/json", "{\"error\":\"Finish gate setup requires a connected finish gate while timer is READY.\"}");
        return;
    }
    sendFinishCommand(
        static_cast<uint8_t>(FinishGateProtocol::CommandType::SetSetupActive),
        isFinishGateSetupActive() ? 0 : 1);
    server_.send(202, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleSetFinishGateLimit()
{
    if (!isFinishGateConnected(millis()) || !isFinishGateSetupActive() ||
        status_.timerState != TimerState::Ready || !server_.hasArg("limit"))
    {
        server_.send(409, "application/json", "{\"error\":\"Enable finish gate setup while timer is READY.\"}");
        return;
    }
    const String argument = server_.arg("limit");
    char *end = nullptr;
    const long value = strtol(argument.c_str(), &end, 10);
    if (argument.length() == 0 || *end != '\0' || value < 0 || value >= Config::kTofNoTargetDistanceMm)
    {
        server_.send(400, "application/json", "{\"error\":\"Enter a limit from 0 to 3999 mm.\"}");
        return;
    }
    sendFinishCommand(
        static_cast<uint8_t>(FinishGateProtocol::CommandType::SetTriggerDistance),
        static_cast<int16_t>(value));
    server_.send(202, "application/json", "{\"ok\":true}");
}

void MotoWebServer::handleUseCurrentFinishGateLimit()
{
    if (!isFinishGateConnected(millis()) || !isFinishGateSetupActive() ||
        status_.timerState != TimerState::Ready)
    {
        server_.send(409, "application/json", "{\"error\":\"Enable finish gate setup while timer is READY.\"}");
        return;
    }
    sendFinishCommand(
        static_cast<uint8_t>(FinishGateProtocol::CommandType::UseCurrentDistance), 0);
    server_.send(202, "application/json", "{\"ok\":true}");
}

const Rider &MotoWebServer::riderForStatus() const
{
    if (attemptRiderLocked_ && status_.timerState != TimerState::Ready)
    {
        return attemptRider_;
    }
    return riders_.selected();
}

bool MotoWebServer::isAttemptValid(const Attempt &attempt) const
{
    return attempt.manuallyValid && !isAttemptAutomaticallyInvalid(attempt);
}

bool MotoWebServer::isAttemptAutomaticallyInvalid(const Attempt &attempt) const
{
    return minimumValidTimeMs_ != 0 && attempt.durationMs < minimumValidTimeMs_;
}

uint32_t MotoWebServer::bestForRider(uint32_t riderId) const
{
    uint32_t bestMs = 0;
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const Attempt &attempt = history_[i];
        if (!isAttemptValid(attempt) || attempt.rider.id != riderId)
        {
            continue;
        }

        if (bestMs == 0 || attempt.durationMs < bestMs)
        {
            bestMs = attempt.durationMs;
        }
    }
    return bestMs;
}

uint32_t MotoWebServer::absoluteBest() const
{
    uint32_t bestMs = 0;
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const Attempt &attempt = history_[i];
        if (!isAttemptValid(attempt) || (bestMs != 0 && attempt.durationMs >= bestMs))
        {
            continue;
        }
        bestMs = attempt.durationMs;
    }
    return bestMs;
}

const char *MotoWebServer::absoluteBestRiderName() const
{
    const Attempt *bestAttempt = nullptr;
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const Attempt &attempt = history_[i];
        if (!isAttemptValid(attempt) || (bestAttempt != nullptr && attempt.durationMs >= bestAttempt->durationMs))
        {
            continue;
        }
        bestAttempt = &attempt;
    }
    return bestAttempt == nullptr || bestAttempt->rider.id == 0 ? "NONAME" : bestAttempt->rider.name;
}

const char *MotoWebServer::timerStateText() const
{
    if (!status_.calibrated)
    {
        return "CALIBRATING";
    }

    switch (status_.timerState)
    {
    case TimerState::Ready:
        return "READY";
    case TimerState::RunningAwaitingClear:
        return "CLEAR GATE";
    case TimerState::RunningArmed:
        return "RUNNING";
    case TimerState::Result:
        return "RESULT";
    }

    return "UNKNOWN";
}
