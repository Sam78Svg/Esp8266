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
.term{background:var(--term);color:var(--termtext);height:360px;overflow-y:auto;border-radius:10px;padding:10px;font:12.5px/1.65 ui-monospace,Menlo,Consolas,monospace}
.ln{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.t{color:#7d8896}.b{color:#ff6b6f}.a{color:#4ade80}
.empty{color:var(--muted);padding:20px;text-align:center}
.item{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--line);gap:8px}
.item:last-child{border:0}
.item span{overflow:hidden;text-overflow:ellipsis;word-break:break-all}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--text);color:var(--bg);padding:10px 16px;border-radius:8px;opacity:0;transition:opacity .25s;pointer-events:none;font-size:14px}
#toast.show{opacity:1}
.cols{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}
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
</div>

<!-- LIVE LOG -->
<div class="pane on card" id="live">
  <div class="bar">
    <button class="chip on" data-f="all">All</button>
    <button class="chip" data-f="blocked">Blocked</button>
    <button class="chip" data-f="allowed">Allowed</button>
    <input type="text" id="q" placeholder="Filter domains…">
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
      <div class="row"><span>Free heap</span><b id="heap">–</b></div>
      <div class="row"><span>Largest block</span><b id="blk">–</b></div>
      <div class="row"><span>Total flash</span><b id="flash">–</b></div>
    </div>
  </div>
</div>

</div>
<div id="toast"></div>

<script>
const $=s=>document.querySelector(s);
const fmtB=b=>b>=1048576?(b/1048576).toFixed(2)+' MiB':b>=1024?(b/1024).toFixed(1)+' KiB':b+' B';
const pad=n=>String(n).padStart(2,'0');
const fmtT=ms=>{let s=Math.floor(ms/1000),h=Math.floor(s/3600),m=Math.floor(s%3600/60);s%=60;return pad(h)+':'+pad(m)+':'+pad(s)};
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
});

// stats
async function loadStats(){
  try{
    const d=await (await fetch('/api/stats')).json();
    $('#dot').classList.remove('off');
    $('#net').textContent='Online · '+d.ssid+' · '+d.ip;
    $('#sBlocked').textContent=d.blocked;$('#sAllowed').textContent=d.allowed;$('#sTotal').textContent=d.total;
    $('#sPct').textContent=d.total?Math.round(d.blocked*100/d.total)+'%':'0%';
    $('#ssid').textContent=d.ssid;$('#ip').textContent=d.ip;$('#gw').textContent=d.gw;
    $('#rssi').textContent=d.rssi+' dBm';$('#up').textContent=fmtT(d.uptime);
    $('#fwT').textContent=fmtB(d.fw)+' of '+fmtB(d.fw+d.ota);
    $('#fwB').style.width=(d.fw*100/(d.fw+d.ota))+'%';
    $('#fsT').textContent=fmtB(d.fsUsed)+' of '+fmtB(d.fsTotal);
    $('#fsB').style.width=(d.fsTotal?d.fsUsed*100/d.fsTotal:0)+'%';
    $('#heap').textContent=fmtB(d.heap);$('#blk').textContent=fmtB(d.maxBlock);$('#flash').textContent=fmtB(d.flash);
    $('#cCount').textContent=d.custom;$('#cMax').textContent=d.customMax;
    $('#cBar').style.width=(d.custom*100/d.customMax)+'%';
    $('#auto').textContent=d.autoReady?('Generated blocklist active: '+d.autoCount.toLocaleString()+' domains'):'Generated blocklist not loaded';
  }catch(e){$('#dot').classList.add('off');$('#net').textContent='Offline'}
}

// logs
function renderLogs(){
  const q=$('#q').value.trim().toLowerCase();
  const v=logs.filter(l=>(filter==='all'||(filter==='blocked')===l.blocked)&&(!q||l.domain.includes(q)));
  $('#logInfo').textContent=v.length+' / '+logs.length+' shown';
  const t=$('#term');
  if(!v.length){t.innerHTML='<div class="empty">'+(logs.length?'No matches':'Waiting for DNS requests…')+'</div>';return}
  const stick=t.scrollTop+t.clientHeight>=t.scrollHeight-30;
  t.innerHTML=v.map(l=>'<div class="ln"><span class="t">'+fmtT(l.time)+'</span> '+(l.blocked?'<span class="b">[BLOCKED]</span>':'<span class="a">[ALLOWED]</span>')+' '+esc(l.domain)+'</div>').join('');
  if(stick)t.scrollTop=t.scrollHeight;
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

loadStats();loadLogs();
setInterval(loadStats,3000);
setInterval(loadLogs,2000);
</script>
</body>
</html>)rawliteral";
