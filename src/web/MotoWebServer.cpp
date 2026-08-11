#include "MotoWebServer.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "config/Config.h"

namespace
{
constexpr char kPage[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Gymio</title><style>
:root{color-scheme:dark}body{margin:0;background:#10151b;color:#edf3f7;font:16px system-ui,sans-serif}.app{max-width:620px;margin:auto;padding:16px}.card{background:#1a232d;border-radius:16px;padding:20px;margin:14px 0}h1{font-size:20px;letter-spacing:.08em;text-align:center}.label{color:#9cb0bf;text-transform:uppercase;font-size:12px;letter-spacing:.1em}.value{font-size:48px;font-weight:700;margin:8px 0}.timer{font-variant-numeric:tabular-nums;font-size:42px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.small{font-size:22px}#connection{color:#83df9c;text-align:center;font-size:14px}.rider-card{border:2px solid #3d5262}.rider-name{font-size:24px;font-weight:700;margin:8px 0}.rider-list{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;margin:16px 0}.rider-option{display:flex;gap:6px}.rider{border:2px solid #3d5262;background:#172029;color:#edf3f7;min-height:62px;flex:1;padding:10px;border-radius:12px;font-size:18px;font-weight:700;word-break:break-word}.rider.selected{border-color:#83df9c;background:#19382a;color:#b4f5c4}.rider-delete{border:1px solid #85414a;background:#351d22;color:#ffb5bd;border-radius:12px;min-width:56px;font-size:22px}.rider-form,.gate-form{display:flex;gap:10px}.rider-form input,.gate-form input{min-width:0;flex:1;border:1px solid #3d5262;background:#10151b;color:#edf3f7;border-radius:10px;padding:14px;font-size:17px}.rider-form button,.gate-form button,.setup-button{border:0;background:#83df9c;color:#10151b;border-radius:10px;min-width:74px;padding:12px;font-size:16px;font-weight:700}.setup-button{width:100%;margin:14px 0 8px}.setup-button.active{background:#ffcb6b}.gate-form .secondary{background:#3d5262;color:#edf3f7}.best-card{text-align:center}.best-card .value{font-variant-numeric:tabular-nums;font-size:36px}.history{padding:0;list-style:none;margin:12px 0 0}.history li{display:flex;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid #2b3945;font-variant-numeric:tabular-nums}.history small{display:block;color:#9cb0bf;margin-top:3px}.debug-card summary{cursor:pointer;color:#9cb0bf}.debug-card .value{font-size:34px}.gate-help{color:#9cb0bf;font-size:13px}.empty{color:#9cb0bf}@media (orientation:landscape) and (min-width:640px){.app{max-width:960px;padding:18px}.timer-card,.debug-card{max-width:650px;margin-left:auto;margin-right:auto}.rider-list{grid-template-columns:repeat(3,minmax(0,1fr))}}</style></head><body><main class="app"><h1>GYMIO</h1><div id="connection">Connecting…</div><section class="card timer-card"><div class="label">Timer state</div><div class="value small" id="state">—</div><div class="timer" id="time">00:00.000</div><div class="label">Rider</div><div class="small" id="timerRider">ANON</div></section><section class="card rider-card"><div class="label">Select rider for next attempt</div><div class="rider-name" id="selectedRider">ANON</div><div class="rider-list" id="riderList"></div><form class="rider-form" id="riderForm"><input id="riderName" maxlength="24" placeholder="New rider name" required><button>Add</button></form></section><section class="card best-card"><div class="label" id="bestLabel">Session best</div><div class="value" id="best">—</div></section><section class="card history-card"><div class="label">Attempt history (session)</div><ul class="history" id="history"><li class="empty">No completed attempts yet</li></ul></section><details class="card debug-card"><summary>Sensor debug</summary><button class="setup-button" id="sensorSetupButton" type="button">Enable Sensor Setup</button><div class="label">Distance</div><div class="value"><span id="distance">—</span> <small id="distanceUnit">mm</small></div><div class="grid"><div><div class="label">Filtered</div><div class="small"><span id="filtered">—</span> mm</div></div><div><div class="label">Gate limit</div><div class="small"><span id="trigger">—</span> mm</div></div></div><p class="gate-help">Sensor Setup blocks attempts. A distance at or below the gate limit is a crossing.</p><form class="gate-form" id="gateLimitForm"><input id="gateLimit" type="number" min="0" max="3999" step="1" required><button>Set limit</button><button class="secondary" id="useCurrentLimit" type="button">Use current</button></form></details></main><script>
const $=id=>document.getElementById(id),distance=$('distance'),distanceUnit=$('distanceUnit'),filtered=$('filtered'),trigger=$('trigger'),state=$('state'),time=$('time'),connection=$('connection'),historyList=$('history'),riderList=$('riderList'),gateLimit=$('gateLimit'),setupButton=$('sensorSetupButton');
const f=v=>{const m=Math.floor(v/60000),s=Math.floor(v/1000)%60,ms=v%1000;return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0')+'.'+String(ms).padStart(3,'0')};
async function api(url,options){const r=await fetch(url,options);if(!r.ok)throw new Error((await r.json()).error||'Request failed');return r.json()}
function renderRiders(data){$('selectedRider').textContent=data.riders.find(r=>r.id===data.selectedId).name;riderList.replaceChildren();data.riders.forEach(r=>{const option=document.createElement('div');option.className='rider-option';const b=document.createElement('button');b.className='rider'+(r.id===data.selectedId?' selected':'');b.textContent=r.name;b.onclick=async()=>{await api('/api/riders/select',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'id='+r.id});refresh()};option.appendChild(b);if(r.id!==0){const remove=document.createElement('button');remove.className='rider-delete';remove.textContent='×';remove.setAttribute('aria-label','Remove '+r.name);remove.onclick=async()=>{if(confirm('Remove rider '+r.name+'?')){try{await api('/api/riders?id='+r.id,{method:'DELETE'});refresh()}catch(e){connection.textContent=e.message}}};option.appendChild(remove)}riderList.appendChild(option)})}
async function setAttemptValidity(number,valid){const action=valid?'revalidate':'invalidate';if(!confirm((valid?'Revalidate':'Invalidate')+' attempt '+number+'?'+(valid?' It will be eligible for best times again.':' It will be excluded from best times.')))return;try{await api('/api/attempts/'+action,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'number='+number});refresh()}catch(err){connection.textContent=err.message}}
async function refresh(){try{const [s,h,r]=await Promise.all([api('/api/status'),api('/api/history'),api('/api/riders')]);distance.textContent=s.distanceMm<0?'∞':s.distanceMm;distanceUnit.textContent=s.distanceMm<0?'':'mm';filtered.textContent=s.filteredDistanceMm<0?'—':s.filteredDistanceMm;trigger.textContent=s.triggerDistanceMm<0?'—':s.triggerDistanceMm;if(document.activeElement!==gateLimit)gateLimit.value=s.triggerDistanceMm>=0?s.triggerDistanceMm:'';gateLimit.disabled=!s.sensorSetupActive;$('gateLimitForm').querySelectorAll('button').forEach(b=>b.disabled=!s.sensorSetupActive);setupButton.textContent=s.sensorSetupActive?'Finish Sensor Setup':'Enable Sensor Setup';setupButton.classList.toggle('active',s.sensorSetupActive);setupButton.disabled=!s.calibrated&& !s.sensorSetupActive;state.textContent=s.sensorSetupActive?'SENSOR SETUP':s.state;time.textContent=f(s.timeMs);$('timerRider').textContent=s.riderName;$('best').textContent=s.sessionBestMs?f(s.sessionBestMs):'—';$('bestLabel').textContent=s.riderName+' · session best';connection.textContent=s.sensorSetupActive?'Sensor Setup: attempts blocked.':s.calibrated?'Live':'Calibrating gate…';historyList.innerHTML=h.attempts.length?h.attempts.map(a=>'<li style="'+(a.valid?'':'opacity:.45;text-decoration:line-through')+'"><span>Attempt '+a.number+'<small>'+a.riderName+(a.valid?'':' · INVALID')+'</small></span><span><strong>'+f(a.durationMs)+'</strong>'+(a.valid?'<button style="margin-left:10px" onclick="setAttemptValidity('+a.number+',false)">Invalidate</button>':'<button style="margin-left:10px" onclick="setAttemptValidity('+a.number+',true)">Revalidate</button>')+'</span></li>').join(''):'<li class="empty">No completed attempts yet</li>';renderRiders(r)}catch(e){connection.textContent=e.message||'Connection lost'}}$('riderForm').onsubmit=async e=>{e.preventDefault();const input=$('riderName');try{await api('/api/riders',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(input.value)});input.value='';refresh()}catch(err){connection.textContent=err.message}};$('gateLimitForm').onsubmit=async e=>{e.preventDefault();try{await api('/api/gate/limit',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'limit='+encodeURIComponent(gateLimit.value)});refresh()}catch(err){connection.textContent=err.message}};$('useCurrentLimit').onclick=async()=>{try{await api('/api/gate/limit/current',{method:'POST'});refresh()}catch(err){connection.textContent=err.message}};setupButton.onclick=async()=>{if(!setupButton.classList.contains('active')&&!confirm('Enter Sensor Setup? Attempts will be blocked until you finish setup.'))return;try{await api('/api/sensor-setup',{method:'POST'});refresh()}catch(err){connection.textContent=err.message}};refresh();setInterval(refresh,500);
</script></body></html>
)HTML";
} // namespace

bool MotoWebServer::begin()
{
    if (!WiFi.softAP(Config::kWifiAccessPointName, Config::kWifiAccessPointPassword))
    {
        return false;
    }

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/history", HTTP_GET, [this]() { handleHistory(); });
    server_.on("/api/attempts/invalidate", HTTP_POST, [this]() { handleInvalidateAttempt(); });
    server_.on("/api/attempts/revalidate", HTTP_POST, [this]() { handleRevalidateAttempt(); });
    server_.on("/api/riders", HTTP_GET, [this]() { handleRiders(); });
    server_.on("/api/riders", HTTP_POST, [this]() { handleAddRider(); });
    server_.on("/api/riders", HTTP_DELETE, [this]() { handleRemoveRider(); });
    server_.on("/api/riders/select", HTTP_POST, [this]() { handleSelectRider(); });
    server_.on("/api/gate/limit", HTTP_POST, [this]() { handleSetGateLimit(); });
    server_.on("/api/gate/limit/current", HTTP_POST, [this]() { handleUseCurrentGateLimit(); });
    server_.on("/api/sensor-setup", HTTP_POST, [this]() { handleSensorSetup(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "Not found"); });
    server_.begin();

    mdnsStarted_ = MDNS.begin(Config::kMdnsHostname);
    if (mdnsStarted_)
    {
        MDNS.addService("http", "tcp", 80);
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

    riders_.recordResult(attemptRider_.id, durationMs);
    attemptRider_.sessionBestMs = riders_.find(attemptRider_.id).sessionBestMs;

    // Each new attempt starts anonymous unless a rider is explicitly selected.
    riders_.select(0);
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

const char *MotoWebServer::displayRiderName() const
{
    return riderForStatus().name;
}

void MotoWebServer::handleClient()
{
    server_.handleClient();
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
}

void MotoWebServer::handleRoot()
{
    server_.send_P(200, "text/html", kPage);
}

void MotoWebServer::handleStatus()
{
    const uint32_t timeMs = status_.timerState == TimerState::Result ? status_.resultMs : status_.elapsedMs;
    char response[256];
    snprintf(
        response,
        sizeof(response),
        "{\"distanceMm\":%d,\"filteredDistanceMm\":%d,\"triggerDistanceMm\":%d,\"calibrated\":%s,\"sensorSetupActive\":%s,\"state\":\"%s\",\"timeMs\":%lu,\"riderName\":\"%s\",\"sessionBestMs\":%lu}",
        status_.distanceMm,
        status_.filteredDistanceMm,
        status_.triggerDistanceMm,
        status_.calibrated ? "true" : "false",
        sensorSetupActive_ ? "true" : "false",
        timerStateText(),
        static_cast<unsigned long>(timeMs),
        riderForStatus().name,
        static_cast<unsigned long>(bestForRider(riderForStatus().id)));
    server_.send(200, "application/json", response);
}

void MotoWebServer::handleHistory()
{
    char response[2048];
    size_t used = snprintf(response, sizeof(response), "{\"attempts\":[");

    // Return newest first. The ring buffer keeps only the most recent runs.
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const uint8_t index = (nextHistoryIndex_ + kHistoryCapacity - 1 - i) % kHistoryCapacity;
        const Attempt &attempt = history_[index];
        used += snprintf(
            response + used,
            sizeof(response) - used,
            "%s{\"number\":%lu,\"durationMs\":%lu,\"riderName\":\"%s\",\"valid\":%s}",
            i == 0 ? "" : ",",
            static_cast<unsigned long>(attempt.number),
            static_cast<unsigned long>(attempt.durationMs),
            attempt.rider.name,
            attempt.valid ? "true" : "false");
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
            if (attempt.valid == valid)
            {
                server_.send(
                    409,
                    "application/json",
                    valid ? "{\"error\":\"Attempt is already valid.\"}" : "{\"error\":\"Attempt is already invalid.\"}");
                return;
            }

            attempt.valid = valid;
            server_.send(200, "application/json", "{\"ok\":true}");
            return;
        }
    }

    server_.send(404, "application/json", "{\"error\":\"Attempt not found.\"}");
}

void MotoWebServer::handleRiders()
{
    char response[2048];
    size_t used = snprintf(
        response,
        sizeof(response),
        "{\"selectedId\":%lu,\"riders\":[{\"id\":0,\"name\":\"ANON\",\"sessionBestMs\":%lu}",
        static_cast<unsigned long>(riders_.selected().id),
        static_cast<unsigned long>(riders_.find(0).sessionBestMs));

    for (uint8_t i = 0; i < riders_.count(); ++i)
    {
        const Rider &rider = riders_.at(i);
        used += snprintf(
            response + used,
            sizeof(response) - used,
            ",{\"id\":%lu,\"name\":\"%s\",\"sessionBestMs\":%lu}",
            static_cast<unsigned long>(rider.id),
            rider.name,
            static_cast<unsigned long>(bestForRider(rider.id)));
    }

    snprintf(response + used, sizeof(response) - used, "]}");
    server_.send(200, "application/json", response);
}

void MotoWebServer::handleAddRider()
{
    uint32_t newId = 0;
    if (!server_.hasArg("name") || !riders_.add(server_.arg("name").c_str(), newId))
    {
        server_.send(400, "application/json", "{\"error\":\"Use 1-24 letters, numbers, spaces, hyphens, or underscores.\"}");
        return;
    }

    riders_.select(newId);
    server_.send(201, "application/json", "{\"ok\":true}");
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
    pendingGateLimitMm_ = status_.distanceMm < 0
                              ? Config::kTofNoTargetDistanceMm - 1
                              : status_.distanceMm;
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

const Rider &MotoWebServer::riderForStatus() const
{
    if (attemptRiderLocked_ && status_.timerState != TimerState::Ready)
    {
        return attemptRider_;
    }
    return riders_.selected();
}

uint32_t MotoWebServer::bestForRider(uint32_t riderId) const
{
    uint32_t bestMs = 0;
    for (uint8_t i = 0; i < historyCount_; ++i)
    {
        const Attempt &attempt = history_[i];
        if (!attempt.valid || attempt.rider.id != riderId)
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
