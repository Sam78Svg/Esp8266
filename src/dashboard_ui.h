#pragma once
#include <pgmspace.h>

// Static dashboard served from flash (PROGMEM) - uses ~0 heap.
// Serve with: server.send_P(200, "text/html", DASHBOARD_HTML);

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP8266 DNS Blocker</title>
<style>
:root{--bg:#f4f6f8;--card:#fff;--text:#1f2933;--muted:#6b7785;--line:#e4e8ec;--accent:#3b82f6;--red:#e5484d;--green:#2f9e63;--term:#0e1116;--termtext:#d6dde6;--shadow:0 1px 3px rgba(0,0,0,.08)}
[data-theme=dark]{--bg:#0f1318;--card:#171c23;--text:#e6ebf1;--muted:#8a96a3;--line:#262d36;--accent:#5b9cff;--red:#ff6b6f;--green:#4ade80;--term:#0a0d11;--shadow:none}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:900px;margin:auto;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px;flex-wrap:wrap}
h1{font-size:20px;margin:0}
.sub{color:var(--muted);font-size:13px}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--green);margin-right:6px;box-shadow:0 0 0 3px rgba(47,158,99,.2)}
.dot.off{background:var(--red);box-shadow:0 0 0 3px rgba(229,72,77,.2)}
.card{background:var(--card);border-radius:14px;padding:16px;box-shadow:var(--shadow);border:1px solid var(--line)}
.grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));margin-bottom:12px}
.stat .n{font-size:28px;font-weight:700;line-height:1.1}
.stat .l{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em}
.red{color:var(--red)}.green{color:var(--green)}
.meter{height:8px;background:var(--line);border-radius:99px;overflow:hidden;margin:6px 0 10px}
.meter>i{display:block;height:100%;background:var(--accent);width:0;transition:width .4s}
.row{display:flex;justify-content:space-between;font-size:13px;color:var(--muted)}
.row b{color:var(--text);font-weight:600}
.tabs{display:flex;gap:6px;margin:16px 0 12px;overflow-x:auto}
.tab{background:transparent;border:1px solid var(--line);color:var(--muted);padding:8px 14px;border-radius:99px;cursor:pointer;font-size:14px;white-space:nowrap}
.tab.on{background:var(--accent);border-color:var(--accent);color:#fff}
.pane{display:none}.pane.on{display:block}
.bar{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px;align-items:center}
input[type=text]{flex:1;min-width:140px;padding:10px 12px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--text);font-size:15px}
button{font:inherit;cursor:pointer}
.btn{padding:9px 14px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text);font-size:14px}
.btn.pri{background:var(--accent);border-color:var(--accent);color:#fff}
.btn.dng{color:var(--red)}
.chip{padding:5px 11px;border-radius:99px;border:1px solid var(--line);background:transparent;color:var(--muted);font-size:13px}
.chip.on{background:var(--text);color:var(--bg);border-color:var(--text)}
.log-action{padding:1px 6px;margin-left:4px;font-size:10px;line-height:1.3;vertical-align:middle}
.term{background:var(--term);color:var(--termtext);height:360px;overflow-y:auto;border-radius:10px;padding:10px;font:12.5px/1.65 ui-monospace,Menlo,Consolas,monospace}
.ln{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.t{color:#7d8896}.b{color:#ff6b6f}.a{color:#4ade80}
.empty{color:var(--muted);padding:20px;text-align:center}
.item{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--line);gap:8px}
.item:last-child{border:0}
.item span{overflow:hidden;text-overflow:ellipsis;word-break:break-all}
#list{max-height:360px;overflow-y:auto}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--text);color:var(--bg);padding:10px 16px;border-radius:8px;opacity:0;transition:opacity .25s;pointer-events:none;font-size:14px}
#toast.show{opacity:1}
.cols{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}
.chart{min-height:220px}
.pie-wrap{display:flex;align-items:center;justify-content:center;gap:24px;flex-wrap:wrap}
.pie{position:relative;width:190px;height:190px;max-width:48vw;border-radius:50%;display:grid;place-items:center;background:conic-gradient(var(--line) 0% 100%)}
.pie-center{display:grid;place-content:center;text-align:center;width:92px;height:92px;border-radius:50%;background:var(--card);line-height:1.2}
.pie-total{font-size:18px;font-weight:700}
.pie-caption{font-size:9px;color:var(--muted)}
.legend{min-width:170px}
.legend .row{margin:8px 0}
.legend-label{display:flex;align-items:center;gap:7px}
.swatch{display:inline-block;width:10px;height:10px;border-radius:50%}
.swatch.allowed{background:var(--green)}.swatch.blocked{background:var(--red)}
.domain-bars{display:grid;gap:10px;margin-top:12px}
.domain-bar{display:grid;grid-template-columns:minmax(90px,1fr) minmax(70px,2fr) 42px;gap:10px;align-items:center}
.domain-name{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.domain-track{height:10px;background:var(--line);border-radius:99px;overflow:hidden}
.domain-fill{height:100%;background:var(--red);border-radius:99px}
</style>
</head>
<body>
<div class="wrap">

<header>
  <div>
    <h1>ESP8266 DNS Blocker</h1>
    <div class="sub"><span id="dot" class="dot"></span><span id="net">Connecting…</span></div>
  </div>
  <button class="btn" id="theme">Theme</button>
</header>

<div class="grid">
  <div class="card stat"><div class="n red" id="sBlocked">–</div><div class="l">Blocked</div></div>
  <div class="card stat"><div class="n green" id="sAllowed">–</div><div class="l">Allowed</div></div>
  <div class="card stat"><div class="n" id="sTotal">–</div><div class="l">Total</div></div>
  <div class="card stat"><div class="n" id="sPct">–</div><div class="l">Block rate</div></div>
</div>

<div class="tabs">
  <button class="tab on" data-p="live">Live log</button>
  <button class="tab" data-p="block">Blocklist</button>
  <button class="tab" data-p="sys">System</button>
  <button class="tab" data-p="reliability">Reliability</button>
  <button class="tab" data-p="graphs">Graphs</button>
</div>

<!-- LIVE LOG -->
<div class="pane on card" id="live">
  <div class="bar">
    <button class="chip on" data-f="all">All</button>
    <button class="chip" data-f="blocked">Blocked</button>
    <button class="chip" data-f="allowed">Allowed</button>
    <input type="text" id="q" placeholder="Filter domain or device IP…">
  </div>
  <div class="bar">
    <button class="btn" id="pause">Pause</button>
    <button class="btn dng" id="clear">Clear logs</button>
    <span class="sub" id="logInfo"></span>
  </div>
  <div class="term" id="term"><div class="empty">Waiting for DNS requests…</div></div>
</div>

<!-- BLOCKLIST -->
<div class="pane card" id="block">
  <form class="bar" id="addForm">
    <input type="text" id="dom" placeholder="ads.example.com" autocapitalize="off" autocomplete="off" required>
    <button class="btn pri" type="submit">Block domain</button>
  </form>
  <div class="row" style="margin-bottom:6px">
    <span>Custom domains: <b id="cCount">0</b> / <b id="cMax">150</b></span>
    <a href="/custom-blocks.txt" style="color:var(--accent)">Download backup</a>
  </div>
  <div class="meter"><i id="cBar"></i></div>
  <div class="sub" id="auto" style="margin-bottom:8px"></div>
  <div id="list"></div>
</div>

<!-- SYSTEM -->
<div class="pane" id="sys">
  <div class="cols">
    <div class="card">
      <div class="row"><span>Network</span><b id="ssid">–</b></div>
      <div class="row"><span>ESP IP</span><b id="ip">–</b></div>
      <div class="row"><span>Router</span><b id="gw">–</b></div>
      <div class="row"><span>Signal</span><b id="rssi">–</b></div>
      <div class="row"><span>Uptime</span><b id="up">–</b></div>
      <div class="row"><span>Upstream DNS</span><b>1.1.1.1</b></div>
    </div>
    <div class="card">
      <div class="row"><span>Firmware</span><b id="fwT">–</b></div>
      <div class="meter"><i id="fwB"></i></div>
      <div class="row"><span>LittleFS</span><b id="fsT">–</b></div>
      <div class="meter"><i id="fsB"></i></div>
      <div class="row"><span>RAM used</span><b id="ramUsed">–</b></div>
      <div class="meter"><i id="ramB"></i></div>
      <div class="row"><span>Free heap</span><b id="heap">–</b></div>
      <div class="row"><span>Largest free block</span><b id="blk">–</b></div>
      <div class="row"><span>Heap fragmentation</span><b id="heapFrag">–</b></div>
      <div class="row"><span>Total flash</span><b id="flash">–</b></div>
    </div>
  </div>
</div>

<!-- RELIABILITY -->
<div class="pane card" id="reliability">
  <h2 style="font-size:17px;margin:0 0 10px">Reliability controls</h2>
  <div class="bar">
    <button class="btn" id="pause5">Pause blocking 5 min</button>
    <button class="btn" id="resumeBlock">Resume blocking</button>
    <button class="btn" id="refreshClients">Device statistics</button>
    <button class="btn" id="setSchedule">Set night schedule</button>
    <button class="btn" id="clearSchedule">Disable schedule</button>
    <a class="btn" href="/update">OTA firmware update</a>
  </div>
  <div id="reliabilityStats" class="sub">Reliability statistics load with system stats.</div>
  <div id="clients" class="sub" style="margin-top:10px"></div>
</div>

<!-- GRAPHS -->
<div class="pane" id="graphs">
  <div class="cols">
    <section class="card chart">
      <h2 style="font-size:17px;margin:0 0 10px">Request breakdown</h2>
      <div class="pie-wrap">
        <div id="requestPie" role="img" aria-label="Allowed and blocked request pie chart"></div>
        <div id="requestLegend" class="legend"></div>
      </div>
    </section>
    <section class="card chart">
      <h2 style="font-size:17px;margin:0">Top 5 blocked domains</h2>
      <div class="sub">Ranked from the latest 100 DNS log entries</div>
      <div id="topDomains" class="domain-bars"></div>
    </section>
  </div>
</div>

</div>
<div id="toast"></div>

<script>
const $=s=>document.querySelector(s);
const fmtB=b=>b>=1048576?(b/1048576).toFixed(2)+' MiB':b>=1024?(b/1024).toFixed(1)+' KiB':b+' B';
const pad=n=>String(n).padStart(2,'0');
const fmtT=ms=>{if(ms>1000000000)return new Date(ms*1000).toLocaleString();let s=Math.floor(ms/1000),h=Math.floor(s/3600),m=Math.floor(s%3600/60);s%=60;return pad(h)+':'+pad(m)+':'+pad(s)};
const esc=t=>t.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let logs=[],filter='all',paused=false;

function toast(m){const t=$('#toast');t.textContent=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),1800)}

// theme
const root=document.documentElement;
try{const s=localStorage.getItem('theme');if(s)root.dataset.theme=s;else if(matchMedia('(prefers-color-scheme:dark)').matches)root.dataset.theme='dark'}catch(e){}
$('#theme').onclick=()=>{const d=root.dataset.theme==='dark'?'light':'dark';root.dataset.theme=d;try{localStorage.setItem('theme',d)}catch(e){}};

// tabs
document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.tab').forEach(x=>x.classList.toggle('on',x===b));
  document.querySelectorAll('.pane').forEach(p=>p.classList.toggle('on',p.id===b.dataset.p));
  if(b.dataset.p==='block')loadList();
  if(b.dataset.p==='reliability')loadClients();
  if(b.dataset.p==='graphs'){loadStats();loadLogs()}
});

// stats
async function loadStats(){
  try{
    const d=await (await fetch('/api/stats')).json();
    $('#dot').classList.remove('off');
    $('#net').textContent='Online · '+d.ssid+' · '+d.ip; $('#reliabilityStats').textContent='Cache hits: '+(d.cacheHits||0)+' · misses: '+(d.cacheMisses||0)+' · upstream fallback: '+(d.upstreamFallbacks||0)+' · upstream failures: '+(d.upstreamFailures||0)+' · '+(d.paused?'BLOCKING PAUSED':'blocking active');
    $('#sBlocked').textContent=d.blocked;$('#sAllowed').textContent=d.allowed;$('#sTotal').textContent=d.total;
    $('#sPct').textContent=d.total?Math.round(d.blocked*100/d.total)+'%':'0%';
    renderRequestPie(d);
    $('#ssid').textContent=d.ssid;$('#ip').textContent=d.ip;$('#gw').textContent=d.gw;
    $('#rssi').textContent=d.rssi+' dBm';$('#up').textContent=fmtT(d.uptime);
    $('#fwT').textContent=fmtB(d.fw)+' of '+fmtB(d.fw+d.ota);
    $('#fwB').style.width=(d.fw*100/(d.fw+d.ota))+'%';
    $('#fsT').textContent=fmtB(d.fsUsed)+' of '+fmtB(d.fsTotal);
    $('#fsB').style.width=(d.fsTotal?d.fsUsed*100/d.fsTotal:0)+'%';
    $('#ramUsed').textContent=fmtB(d.ramUsed)+' of '+fmtB(d.ramTotal)+' ('+d.ramUsedPct+'%)';
    $('#ramB').style.width=d.ramUsedPct+'%';
    $('#heap').textContent=fmtB(d.heap);$('#blk').textContent=fmtB(d.maxBlock);$('#heapFrag').textContent=d.heapFrag+'%';$('#flash').textContent=fmtB(d.flash);
    $('#cCount').textContent=d.custom;$('#cMax').textContent=d.customMax;
    $('#cBar').style.width=(d.custom*100/d.customMax)+'%';
    $('#auto').textContent=d.autoReady?('Generated blocklist active: '+d.autoCount.toLocaleString()+' domains'):'Generated blocklist not loaded';
  }catch(e){$('#dot').classList.add('off');$('#net').textContent='Offline'}
}

// logs
function renderLogs(){
  const q=$('#q').value.trim().toLowerCase();
  const v=logs.filter(l=>(filter==='all'||(filter==='blocked')===l.blocked)&&(!q||l.domain.toLowerCase().includes(q)||((l.client||'').toLowerCase().includes(q))));
  $('#logInfo').textContent=v.length+' / '+logs.length+' shown';
  const t=$('#term');
  renderTopDomains();
  if(!v.length){t.innerHTML='<div class="empty">'+(logs.length?'No matches':'Waiting for DNS requests…')+'</div>';return}
  const stick=t.scrollTop+t.clientHeight>=t.scrollHeight-30;
  t.innerHTML=v.map(l=>'<div class="ln"><span class="t">'+fmtT(l.time)+'</span> '+(l.blocked?'<span class="b">[BLOCKED]</span>':'<span class="a">[ALLOWED]</span>')+' '+esc(l.domain)+' <span class="t">from '+esc(l.client||'unknown')+'</span> <button class="chip log-action" data-action="'+(l.blocked?'allow':'block')+'" data-domain="'+esc(l.domain)+'">'+(l.blocked?'Allow':'Block')+'</button></div>').join('');
  t.querySelectorAll('button[data-action]').forEach(b=>b.onclick=async()=>{
    const body='domain='+encodeURIComponent(b.dataset.domain);
    const url=b.dataset.action==='allow'?'/api/allow':'/add?ajax=1';
    const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    toast(r.ok?(b.dataset.action==='allow'?'Allowlisted':'Blocked')+' '+b.dataset.domain:'Action failed');
    loadList();loadStats();
  });
  if(stick)t.scrollTop=t.scrollHeight;
}

function renderTopDomains(){
 const counts={}; logs.filter(x=>x.blocked).forEach(x=>counts[x.domain]=(counts[x.domain]||0)+1);
 const top=Object.entries(counts).sort((a,b)=>b[1]-a[1]).slice(0,5);
 const area=document.querySelector('#topDomains');
 if(!area)return;
 if(!top.length){area.innerHTML='<div class="empty">No blocked requests in the recent logs</div>';return}
 const maxCount=top[0][1];
 area.innerHTML=top.slice(0,5).map(([domain,count])=>'<div class="domain-bar"><span class="domain-name" title="'+esc(domain)+'">'+esc(domain)+'</span><div class="domain-track"><div class="domain-fill" style="width:'+Math.max(2,count*100/maxCount)+'%"></div></div><b>'+count+'</b></div>').join('');
}

function renderRequestPie(stats){
 const blocked=Number(stats.blocked)||0,allowed=Number(stats.allowed)||0,total=Number(stats.total)||0;
 const sum=blocked+allowed;
 const blockedPct=sum?blocked*100/sum:0;
 const roundedBlockedPct=Math.round(blockedPct);
 const allowedPct=sum?100-roundedBlockedPct:0;
 const pie=$('#requestPie');
 pie.style.background=sum
   ?'conic-gradient(var(--red) 0% '+blockedPct+'%, var(--green) '+blockedPct+'% 100%)'
   :'conic-gradient(var(--line) 0% 100%)';
 pie.className='pie';
 pie.innerHTML='<div class="pie-center"><span class="pie-total">'+total.toLocaleString()+'</span><span class="pie-caption">TOTAL REQUESTS</span></div>';
 pie.setAttribute('aria-label','Allowed '+allowed+', blocked '+blocked+', total '+total);
 $('#requestLegend').innerHTML='<div class="row"><span class="legend-label"><i class="swatch allowed"></i>Allowed</span><b>'+allowed.toLocaleString()+' · '+allowedPct+'%</b></div><div class="row"><span class="legend-label"><i class="swatch blocked"></i>Blocked</span><b>'+blocked.toLocaleString()+' · '+roundedBlockedPct+'%</b></div><div class="row"><span>Total requests</span><b>'+total.toLocaleString()+'</b></div>';
}

async function loadLogs(){
  if(paused)return;
  try{logs=await (await fetch('/logs')).json();renderLogs()}catch(e){}
}
document.querySelectorAll('.chip').forEach(c=>c.onclick=()=>{
  filter=c.dataset.f;document.querySelectorAll('.chip').forEach(x=>x.classList.toggle('on',x===c));renderLogs();
});
$('#q').oninput=renderLogs;
$('#pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Resume':'Pause'};
$('#clear').onclick=async()=>{await fetch('/clear-logs',{method:'POST'});logs=[];renderLogs();toast('Logs cleared')};

// blocklist
async function loadList(){
  try{
    const a=await (await fetch('/api/blocklist')).json();
    $('#list').innerHTML=a.length?a.map((d,i)=>'<div class="item"><span>'+esc(d)+'</span><button class="btn dng" data-i="'+i+'">Remove</button></div>').join(''):'<div class="empty">No custom domains yet</div>';
    document.querySelectorAll('#list button').forEach(b=>b.onclick=async()=>{
      const r=await fetch('/remove?index='+b.dataset.i+'&ajax=1');
      toast(r.ok?'Removed':'Failed');loadList();loadStats();
    });
  }catch(e){}
}
$('#addForm').onsubmit=async e=>{
  e.preventDefault();
  const d=$('#dom').value.trim();if(!d)return;
  const r=await fetch('/add?ajax=1&domain='+encodeURIComponent(d));
  if(r.ok){$('#dom').value='';toast('Blocked '+d)}else toast('Could not add (list full or invalid)');
  loadList();loadStats();
};


async function loadClients(){
  try{
    const a=await(await fetch('/api/clients')).json();
    const rows=a.sort((x,y)=>y.total-x.total).map(x=>'<div class="row"><span>'+esc(x.ip)+'</span><b>'+x.total+' queries · '+x.blocked+' blocked</b></div>').join('');
    $('#clients').innerHTML='<b>Active devices</b>'+(rows||'<div class="sub">No client queries recorded yet</div>');
  }catch(e){$('#clients').textContent='Could not load device statistics'}
}
$('#pause5').onclick=async()=>{let r=await fetch('/api/pause',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'minutes=5'});toast(r.ok?'Blocking paused for 5 minutes':'Pause failed');loadStats()};
$('#resumeBlock').onclick=async()=>{let r=await fetch('/api/resume',{method:'POST'});toast(r.ok?'Blocking resumed':'Resume failed');loadStats()};
$('#setSchedule').onclick=async()=>{
  const domain=prompt('Domain to block during schedule (e.g. instagram.com)'); if(!domain)return;
  const client=prompt('Client IP (leave blank for all devices)')||'';
  const start=prompt('Start hour, 0-23','22'); if(start===null)return;
  const end=prompt('End hour, 0-23','7'); if(end===null)return;
  const body='domain='+encodeURIComponent(domain)+'&client='+encodeURIComponent(client)+'&start='+encodeURIComponent(start)+'&end='+encodeURIComponent(end);
  const r=await fetch('/api/schedule',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  toast(r.ok?'Schedule saved':'Schedule failed');
};
$('#clearSchedule').onclick=async()=>{const r=await fetch('/api/schedule/disable',{method:'POST'});toast(r.ok?'Schedule disabled':'Failed')};
$('#refreshClients').onclick=loadClients;

loadStats();loadLogs();
setInterval(loadStats,1500);
setInterval(loadLogs,2000);
</script>
</body>
</html>)rawliteral";
