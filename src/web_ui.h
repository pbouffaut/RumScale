#pragma once
#include <Arduino.h>

// L'app est embarquée dans le firmware : un seul `pio run -t upload` et tout est
// à jour, rien à téléverser dans un système de fichiers.
// Ce fichier ne doit être inclus que par net.cpp.

static const char INDEX_HTML[] PROGMEM = R"RUMHTML(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#14100e">
<title>Tonneau</title>
<style>
:root{
  --bg:#14100e; --card:#1f1815; --card-2:#261e19; --line:#33291f;
  --ink:#f2e8dc; --ink-2:#b8a894; --ink-3:#9a8b78;
  --amber:#e8a33d; --amber-soft:#f0b45a; --amber-deep:#b4762a;
  --teal:#5fb0a5; --danger:#e2705f;
  --r:14px;
}
*{box-sizing:border-box}
html,body{margin:0;padding:0}
body{
  background:var(--bg); color:var(--ink);
  font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  -webkit-text-size-adjust:100%;
  padding:0 0 40px;
}
.wrap{max-width:640px;margin:0 auto;padding:0 16px}
header{display:flex;align-items:center;gap:10px;padding:18px 0 10px}
header h1{font-size:18px;font-weight:600;margin:0;letter-spacing:.2px;flex:1}
.dot{width:8px;height:8px;border-radius:50%;background:var(--ink-3);flex:0 0 auto}
.dot.on{background:var(--teal)}
.dot.off{background:var(--danger)}
.sub{color:var(--ink-3);font-size:12px}

.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:16px;margin:12px 0}
.card h2{font-size:13px;font-weight:600;text-transform:uppercase;letter-spacing:.08em;
  color:var(--ink-3);margin:0 0 12px}

/* --- héros ------------------------------------------------------------- */
.hero{display:flex;gap:18px;align-items:center}
.hero svg{flex:0 0 108px;height:auto}
.hero .num{font-size:52px;font-weight:600;line-height:1;letter-spacing:-1px;
  font-variant-numeric:tabular-nums}
.hero .num.idle{font-size:22px;font-weight:500;color:var(--ink-2);letter-spacing:0}
.hero .num small{font-size:22px;font-weight:500;color:var(--ink-2);margin-left:4px}
.hero .vol{font-size:16px;color:var(--ink-2);margin-top:6px;font-variant-numeric:tabular-nums}
.hero .settling{font-size:11px;color:var(--ink-3);margin-top:8px;display:none}
.hero .settling.show{display:block}

/* --- tuiles ------------------------------------------------------------ */
.tiles{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.tile{background:var(--card-2);border:1px solid var(--line);border-radius:10px;padding:12px}
.tile .k{font-size:11px;color:var(--ink-3);text-transform:uppercase;letter-spacing:.06em}
.tile .v{font-size:22px;font-weight:600;margin-top:4px;font-variant-numeric:tabular-nums}
.tile .u{font-size:12px;color:var(--ink-2);font-weight:400;margin-left:3px}
.tile .note{display:block;font-size:11px;color:var(--ink-3);font-weight:400;margin-top:3px}

/* --- graphique --------------------------------------------------------- */
.filters{display:flex;gap:6px;margin-bottom:12px}
.filters button{background:transparent;border:1px solid var(--line);color:var(--ink-2);
  border-radius:999px;padding:5px 12px;font-size:13px;cursor:pointer}
.filters button[aria-pressed=true]{background:var(--card-2);color:var(--ink);border-color:#4a3a2b}
.chartbox{position:relative}
.chartbox svg{width:100%;height:auto;display:block;touch-action:pan-y}
.tip{position:absolute;pointer-events:none;opacity:0;transition:opacity .1s;
  background:#0f0c0a;border:1px solid var(--line);border-radius:8px;padding:7px 9px;
  font-size:12px;white-space:nowrap;transform:translate(-50%,-100%);z-index:2;
  box-shadow:0 4px 14px rgba(0,0,0,.5)}
.tip.below{transform:translate(-50%,0)}
.tip b{font-variant-numeric:tabular-nums}
.tip .d{color:var(--ink-3);display:block;font-size:11px;margin-top:2px}
.empty{color:var(--ink-3);font-size:13px;padding:22px 0;text-align:center}
details.data{margin-top:12px}
details.data summary{font-size:12px;color:var(--ink-3);cursor:pointer}
table{width:100%;border-collapse:collapse;margin-top:10px;font-size:12px}
th,td{text-align:left;padding:5px 6px;border-bottom:1px solid var(--line)}
th{color:var(--ink-3);font-weight:500}
td.n{text-align:right;font-variant-numeric:tabular-nums}

/* --- événements -------------------------------------------------------- */
ul.ev{list-style:none;margin:0;padding:0}
ul.ev li{display:flex;gap:10px;align-items:baseline;padding:9px 0;border-bottom:1px solid var(--line)}
ul.ev li:last-child{border-bottom:0}
ul.ev .ic{flex:0 0 18px;text-align:center;font-size:13px}
ul.ev .t{flex:1}
ul.ev .when{color:var(--ink-3);font-size:12px}
ul.ev .amt{font-variant-numeric:tabular-nums;font-weight:600}

/* --- formulaires ------------------------------------------------------- */
label{display:block;font-size:12px;color:var(--ink-3);margin:12px 0 4px}
input[type=text],input[type=number],input[type=password]{
  width:100%;background:#0f0c0a;border:1px solid var(--line);color:var(--ink);
  border-radius:9px;padding:10px 11px;font-size:15px}
input:focus{outline:2px solid var(--amber-deep);outline-offset:1px}
.row{display:flex;gap:10px}
.row>*{flex:1}
button.act{background:var(--amber);color:#231703;border:0;border-radius:10px;
  padding:11px 16px;font-size:14px;font-weight:600;cursor:pointer;width:100%;margin-top:14px}
button.act.ghost{background:transparent;border:1px solid var(--line);color:var(--ink-2);font-weight:500}
button.act.warn{background:transparent;border:1px solid #5a2b24;color:var(--danger)}
button.act:disabled{opacity:.45;cursor:default}
.hint{font-size:12px;color:var(--ink-3);margin-top:8px}
.live{font-variant-numeric:tabular-nums;font-size:26px;font-weight:600;margin:6px 0 2px}
.step{border-top:1px solid var(--line);padding-top:14px;margin-top:14px}
.step:first-of-type{border-top:0;padding-top:0;margin-top:0}
.step h3{font-size:14px;margin:0 0 6px;font-weight:600}
.step p{margin:0;font-size:13px;color:var(--ink-2)}
.ok{color:var(--teal)}
.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(20px);
  background:#0f0c0a;border:1px solid var(--line);border-radius:10px;padding:11px 16px;
  font-size:13px;opacity:0;transition:.2s;z-index:9;max-width:90vw;text-align:center}
.toast.show{opacity:1;transform:translateX(-50%)}
.toast.bad{border-color:#5a2b24;color:#ffd9d2}
nav.tabs{display:flex;gap:4px;margin:14px 0 0;border-bottom:1px solid var(--line)}
nav.tabs button{background:none;border:0;border-bottom:2px solid transparent;color:var(--ink-3);
  padding:9px 12px;font-size:14px;cursor:pointer}
nav.tabs button[aria-selected=true]{color:var(--ink);border-bottom-color:var(--amber)}
section[hidden]{display:none}
</style>
</head>
<body>
<div class="wrap">

  <header>
    <span class="dot" id="dot"></span>
    <h1 id="title">Tonneau</h1>
    <span class="sub" id="agesub"></span>
  </header>

  <nav class="tabs" role="tablist">
    <button role="tab" aria-selected="true"  data-tab="view">Suivi</button>
    <button role="tab" aria-selected="false" data-tab="setup">Réglages</button>
  </nav>

  <!-- ================= SUIVI ================= -->
  <section id="tab-view">

    <div class="card">
      <div class="hero">
        <svg viewBox="0 0 120 150" role="img" aria-labelledby="btitle">
          <title id="btitle">Niveau du tonneau</title>
          <defs>
            <clipPath id="clipBarrel">
              <path d="M28,14 C10,42 10,110 28,138 L92,138 C110,110 110,42 92,14 Z"/>
            </clipPath>
            <linearGradient id="gLiq" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0" stop-color="#f0b45a"/>
              <stop offset="1" stop-color="#a86a22"/>
            </linearGradient>
          </defs>
          <path d="M28,14 C10,42 10,110 28,138 L92,138 C110,110 110,42 92,14 Z" fill="#241c17"/>
          <g clip-path="url(#clipBarrel)">
            <rect id="liq" x="6" y="138" width="108" height="0" fill="url(#gLiq)"/>
            <rect id="liqTop" x="6" y="138" width="108" height="2" fill="#ffd58f" opacity=".9"/>
            <rect x="6" y="34" width="108" height="7" fill="#5c4a38" opacity=".85"/>
            <rect x="6" y="108" width="108" height="7" fill="#5c4a38" opacity=".85"/>
          </g>
          <path d="M28,14 C10,42 10,110 28,138 L92,138 C110,110 110,42 92,14 Z"
                fill="none" stroke="#4a3a2b" stroke-width="2"/>
          <ellipse cx="60" cy="14" rx="32" ry="6" fill="#2e241d" stroke="#4a3a2b" stroke-width="2"/>
        </svg>
        <div>
          <div class="num" id="pct">—<small>%</small></div>
          <div class="vol" id="vol">en attente de mesure</div>
          <div class="settling" id="settling">Mesure en train de se stabiliser…</div>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="tiles">
        <div class="tile"><div class="k">Vieillissement</div><div class="v" id="tAge">—</div></div>
        <div class="tile"><div class="k">Dernier service</div><div class="v" id="tServe">—</div></div>
        <div class="tile"><div class="k">Part des anges</div><div class="v" id="tEvap">—</div></div>
        <div class="tile"><div class="k">Poids sur la base</div><div class="v" id="tWeight">—</div></div>
      </div>
    </div>

    <div class="card">
      <h2>Niveau au fil du temps · litres</h2>
      <div class="filters" role="group" aria-label="Période">
        <button data-days="7"  aria-pressed="false">7 jours</button>
        <button data-days="30" aria-pressed="true">30 jours</button>
        <button data-days="0"  aria-pressed="false">Tout</button>
      </div>
      <div class="chartbox">
        <svg id="chart" viewBox="0 0 600 220"></svg>
        <div class="tip" id="tip"></div>
      </div>
      <div class="empty" id="chartEmpty" hidden>Pas encore assez d'historique. Le tonneau
        enregistre un point par heure.</div>
      <details class="data">
        <summary>Voir les données</summary>
        <div id="dataTable"></div>
      </details>
    </div>

    <div class="card">
      <h2>Journal</h2>
      <ul class="ev" id="events"></ul>
      <div class="empty" id="evEmpty">Aucun événement pour l'instant.</div>
    </div>

  </section>

  <!-- ================= RÉGLAGES ================= -->
  <section id="tab-setup" hidden>

    <div class="card" id="wizard">
      <h2>Initialisation</h2>
      <div class="live" id="liveW">—</div>
      <div class="hint">Poids mesuré en direct sur la base.</div>

      <div class="step">
        <h3>1. Base vide <span id="s1" class="ok"></span></h3>
        <p>Enlève tout de la base, puis fais le zéro.</p>
        <button class="act ghost" onclick="post('/api/tare')">Faire le zéro</button>
      </div>

      <div class="step">
        <h3>2. Poids connu <span id="s2" class="ok"></span></h3>
        <p>Pose un objet dont tu connais la masse — une bouteille d'eau d'un litre
           pleine fait 1000 g — et saisis-la.</p>
        <label for="known">Masse posée (grammes)</label>
        <input type="number" id="known" value="1000" min="100" step="10">
        <button class="act ghost" onclick="calibrate()">Calibrer la balance</button>
      </div>

      <div class="step">
        <h3>3. Tonneau vide <span id="s3" class="ok"></span></h3>
        <p>Pose le tonneau vide, bonde et robinet compris, et attends que la valeur
           du haut se stabilise.</p>
        <button class="act ghost" onclick="post('/api/empty')">Enregistrer le tonneau vide</button>
      </div>

      <div class="step">
        <h3>4. Tonneau plein <span id="s4" class="ok"></span></h3>
        <p>Remplis le tonneau, repose-le, attends la stabilisation puis valide.
           La densité du liquide est calculée automatiquement.</p>
        <label for="cap">Capacité du tonneau (litres)</label>
        <input type="number" id="cap" value="5" min="0.25" step="0.25">
        <button class="act" onclick="markFull()">Enregistrer le tonneau plein</button>
      </div>
    </div>

    <div class="card">
      <h2>Le tonneau</h2>
      <label for="nm">Nom affiché</label>
      <input type="text" id="nm" maxlength="20" placeholder="Tonneau">
      <div class="row">
        <div>
          <label for="dens">Densité (g/ml)</label>
          <input type="number" id="dens" step="0.01" min="0.7" max="1.3">
        </div>
        <div>
          <label for="cap2">Capacité (L)</label>
          <input type="number" id="cap2" step="0.25" min="0.25">
        </div>
      </div>
      <button class="act ghost" onclick="saveBarrel()">Enregistrer</button>
      <button class="act ghost" onclick="resetAging()">Redémarrer le vieillissement</button>
      <div class="hint">Le bouton du tonneau fait la même chose : maintiens-le 6 secondes.</div>
    </div>

    <div class="card">
      <h2>Alertes Telegram</h2>
      <p class="hint">Écris à <b>@BotFather</b> sur Telegram, envoie
         <b>/newbot</b>, colle le jeton ici. Puis écris un message à ton nouveau
         bot et ouvre <b>@userinfobot</b> pour connaître ton identifiant.</p>
      <label for="tok">Jeton du bot</label>
      <input type="password" id="tok" placeholder="laisser vide = inchangé" autocomplete="off">
      <label for="chat">Identifiant de discussion</label>
      <input type="text" id="chat" placeholder="123456789">
      <label style="display:flex;align-items:center;gap:8px;margin-top:14px">
        <input type="checkbox" id="aos" style="width:auto"> Me prévenir à chaque service
      </label>
      <button class="act ghost" onclick="saveTg()">Enregistrer</button>
      <button class="act ghost" onclick="post('/api/telegram/test')">Envoyer un test</button>
      <div class="hint" id="tgState"></div>
    </div>

    <div class="card">
      <h2>Système</h2>
      <div class="hint" id="sys"></div>
      <button class="act ghost" onclick="post('/api/reboot')">Redémarrer</button>
      <button class="act warn" onclick="confirmPost('/api/wifi/forget','Oublier le réseau Wi-Fi ?')">Oublier le Wi-Fi</button>
      <button class="act warn" onclick="confirmPost('/api/factory','Tout effacer, y compris la calibration et l\'historique ?')">Remise à zéro complète</button>
    </div>

  </section>
</div>

<div class="toast" id="toast"></div>

<script>
const nf = (v,d=0)=>new Intl.NumberFormat('fr-FR',{minimumFractionDigits:d,maximumFractionDigits:d}).format(v);
const $ = s=>document.querySelector(s);
let S = {}, days = 30, hist = [];

/* ---------- onglets ---------- */
document.querySelectorAll('nav.tabs button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('nav.tabs button').forEach(x=>x.setAttribute('aria-selected', x===b));
  $('#tab-view').hidden  = b.dataset.tab!=='view';
  $('#tab-setup').hidden = b.dataset.tab!=='setup';
});

/* ---------- toast ---------- */
let toastT;
function toast(msg, bad){
  const t=$('#toast'); t.textContent=msg; t.classList.toggle('bad',!!bad); t.classList.add('show');
  clearTimeout(toastT); toastT=setTimeout(()=>t.classList.remove('show'),3200);
}

/* ---------- appels API ---------- */
async function post(url, body){
  try{
    const r = await fetch(url,{method:'POST',
      headers:body?{'Content-Type':'application/json'}:{},
      body:body?JSON.stringify(body):undefined});
    const j = await r.json().catch(()=>({}));
    toast(j.message || (r.ok?'C\'est fait.':'Échec.'), !r.ok);
    refresh();
    return r.ok;
  }catch(e){ toast('Le tonneau ne répond pas.',true); return false; }
}
function confirmPost(url,q){ if(confirm(q)) post(url); }
function calibrate(){
  const g = parseFloat($('#known').value);
  if(!(g>=100)) return toast('Il faut au moins 100 g.',true);
  post('/api/calibrate',{known_g:g});
}
function markFull(){
  const l = parseFloat($('#cap').value);
  if(!(l>0)) return toast('Capacité invalide.',true);
  post('/api/full',{capacity_ml:Math.round(l*1000)});
}
function saveBarrel(){
  post('/api/config',{
    name: $('#nm').value.trim() || 'Tonneau',
    density: parseFloat($('#dens').value),
    capacity_ml: Math.round(parseFloat($('#cap2').value)*1000)
  });
}
function saveTg(){
  const body = {tg_chat:$('#chat').value.trim(), alert_on_serve:$('#aos').checked};
  if($('#tok').value.trim()) body.tg_token = $('#tok').value.trim();
  $('#tok').value='';
  post('/api/config', body);
}
function resetAging(){
  if(confirm('Repartir de zéro jour de vieillissement ?')) post('/api/aging/reset');
}

/* ---------- rendu de l'état ---------- */
function fmtAge(d){
  if(d===-2) return 'en cours';        // court, mais l'heure n'est pas revenue
  if(d<0) return 'à démarrer';
  if(d<31) return d+(d>1?' jours':' jour');
  const m=Math.floor(d/30.44);
  return m<12 ? m+' mois' : nf(d/365.25,1)+' ans';
}
function render(s){
  S=s;
  $('#title').textContent = s.name || 'Tonneau';
  document.title = s.name || 'Tonneau';
  $('#dot').className = 'dot on';

  const pct = s.pct;
  const H=124, y0=138;
  const h = pct>=0 ? Math.max(0, Math.min(1, pct/100)) * H : 0;
  $('#liq').setAttribute('y', y0-h);      $('#liq').setAttribute('height', h);
  $('#liqTop').setAttribute('y', y0-h);   $('#liqTop').setAttribute('height', h>2?2:0);

  $('#pct').classList.toggle('idle', pct<0);
  if(pct>=0){
    $('#pct').innerHTML = nf(pct,0)+'<small>%</small>';
    $('#vol').textContent = nf(s.ml/1000,2)+' L sur '+nf(s.capacity_ml/1000,2)+' L';
  }else{
    $('#pct').textContent = 'Pas encore initialisé';
    $('#vol').textContent = s.calibrated ? 'Réglages · étape 3'
                                         : 'Réglages · étape 1';
  }
  $('#settling').classList.toggle('show', !s.stable);

  $('#agesub').textContent = s.aging_days>=0 ? 'J+'+s.aging_days
                           : (s.aging_days===-2 ? 'J+?' : '');
  $('#tAge').innerHTML = fmtAge(s.aging_days);
  if(s.aging_days===-2)
    $('#tAge').innerHTML += '<span class="note">le compte reprend au retour du Wi-Fi</span>';
  $('#tServe').innerHTML = s.last_serve_at
      ? nf(s.last_serve_g/(s.density||0.94),0)+'<span class="u">ml</span>'
      : '—';
  const evapPct = (s.full_g>s.empty_g) ? 100*s.evaporated_g/(s.full_g-s.empty_g) : 0;
  $('#tEvap').innerHTML = s.evaporated_g>0
      ? nf(evapPct,1)+'<span class="u">%</span>' : '—';
  $('#tWeight').innerHTML = nf(s.total_g/1000,2)+'<span class="u">kg</span>';

  $('#liveW').textContent = nf(s.total_g,0)+' g'+(s.stable?'':' …');
  $('#s1').textContent = s.calibrated?'✓':'';
  $('#s2').textContent = s.calibrated?'✓ '+nf(s.counts_per_g,1)+' counts/g':'';
  $('#s3').textContent = s.empty_g>0?'✓ '+nf(s.empty_g,0)+' g':'';
  $('#s4').textContent = s.initialized?'✓ '+nf(s.full_g,0)+' g':'';

  if(document.activeElement!==$('#nm'))   $('#nm').value = s.name;
  if(document.activeElement!==$('#dens')) $('#dens').value = (s.density||0.94).toFixed(2);
  if(document.activeElement!==$('#cap2')) $('#cap2').value = s.capacity_ml/1000;
  if(document.activeElement!==$('#chat')) $('#chat').value = s.tg_chat||'';
  $('#aos').checked = !!s.alert_on_serve;
  $('#tgState').textContent = s.telegram
      ? (s.tg_error ? 'Configuré, mais le dernier envoi a échoué (code '+s.tg_error+').'
                    : 'Configuré et fonctionnel.')
      : 'Pas encore configuré.';
  $('#sys').textContent = 'IP '+s.ip+' · '+s.rssi+' dBm · '
      + (s.hx||[]).map((v,i)=>'HX'+(i?'B':'A')+(v?' ok':' muet')).join(' · ')
      + ' · heure '+(s.time_valid?'synchronisée':'non synchronisée')
      + ' · ' + Math.floor(s.uptime_s/3600)+' h en marche';
}

/* ---------- graphique : une série, hover + tableau ---------- */
const CH = {w:600,h:220,l:44,r:10,t:12,b:26};
function drawChart(pts){
  const svg = $('#chart'), box = $('#chartEmpty');
  svg.innerHTML='';
  $('#tip').style.opacity=0;          // sinon un tooltip survit au redessin
  if(!pts || pts.length<2){ box.hidden=false; svg.style.display='none'; buildTable([]); return; }
  box.hidden=true; svg.style.display='block';

  const dens = S.density||0.94;
  const data = pts.map(p=>({t:p[0]*1000, v:p[1]/dens/1000}));   // litres
  const x0=CH.l, x1=CH.w-CH.r, y0=CH.t, y1=CH.h-CH.b;
  const tMin=data[0].t, tMax=data[data.length-1].t || tMin+1;
  // L'axe part de zéro et monte au moins jusqu'à la capacité : on veut lire
  // « ce qu'il reste sur le total », pas une variation grossie.
  const capL=(S.capacity_ml||5000)/1000;
  const dataMax=Math.max(...data.map(d=>d.v));
  const vMax=dataMax>capL ? dataMax*1.05 : capL;
  const sx=t=>x0+(x1-x0)*(t-tMin)/Math.max(1,tMax-tMin);
  const sy=v=>y1-(y1-y0)*v/vMax;
  const NS='http://www.w3.org/2000/svg';
  const el=(n,a)=>{const e=document.createElementNS(NS,n);for(const k in a)e.setAttribute(k,a[k]);return e;};

  // grille et graduations, volontairement discrètes, sur un pas rond
  const niceStep=x=>{const p=Math.pow(10,Math.floor(Math.log10(x))),n=x/p;
    return (n<=1?1:n<=2?2:n<=2.5?2.5:n<=5?5:10)*p;};
  const step=niceStep(vMax/5), dec=step<1?1:0;
  for(let v=0; v<=vMax+1e-9; v+=step){
    const y=sy(v);
    svg.appendChild(el('line',{x1:x0,y1:y,x2:x1,y2:y,stroke:'#33291f','stroke-width':1}));
    const tx=el('text',{x:x0-8,y:y+4,'text-anchor':'end',fill:'#9a8b78','font-size':11});
    tx.textContent=nf(v,dec); svg.appendChild(tx);
  }

  // dates aux extrémités
  const dfmt=t=>new Intl.DateTimeFormat('fr-FR',{day:'numeric',month:'short'}).format(new Date(t));
  const d1=el('text',{x:x0,y:CH.h-8,fill:'#9a8b78','font-size':11}); d1.textContent=dfmt(tMin);
  const d2=el('text',{x:x1,y:CH.h-8,'text-anchor':'end',fill:'#9a8b78','font-size':11});
  d2.textContent=dfmt(tMax); svg.appendChild(d1); svg.appendChild(d2);

  // aire très légère sous la courbe, puis la ligne
  const dPath = data.map((d,i)=>(i?'L':'M')+sx(d.t).toFixed(1)+','+sy(d.v).toFixed(1)).join('');
  svg.appendChild(el('path',{d:dPath+'L'+sx(tMax)+','+y1+'L'+sx(tMin)+','+y1+'Z',
    fill:'#e8a33d',opacity:'.10'}));
  svg.appendChild(el('path',{d:dPath,fill:'none',stroke:'#e8a33d','stroke-width':2,
    'stroke-linejoin':'round','stroke-linecap':'round'}));

  // dernier point étiqueté : la seule valeur écrite sur le graphique
  const last=data[data.length-1];
  svg.appendChild(el('circle',{cx:sx(last.t),cy:sy(last.v),r:4,fill:'#e8a33d',
    stroke:'#1f1815','stroke-width':2}));

  // couche de survol
  const cross=el('line',{x1:0,y1:y0,x2:0,y2:y1,stroke:'#f2e8dc','stroke-width':1,
    opacity:'0','stroke-dasharray':'3 3'});
  const mark=el('circle',{cx:0,cy:0,r:5,fill:'#f0b45a',stroke:'#1f1815','stroke-width':2,opacity:'0'});
  svg.appendChild(cross); svg.appendChild(mark);
  const hit=el('rect',{x:x0,y:y0,width:x1-x0,height:y1-y0,fill:'transparent'});
  svg.appendChild(hit);

  const tip=$('#tip');
  function move(ev){
    const r=svg.getBoundingClientRect();
    const px=(ev.clientX-r.left)/r.width*CH.w;
    let best=0,bd=1e9;
    data.forEach((d,i)=>{const dd=Math.abs(sx(d.t)-px); if(dd<bd){bd=dd;best=i}});
    const d=data[best], X=sx(d.t), Y=sy(d.v);
    cross.setAttribute('x1',X); cross.setAttribute('x2',X); cross.setAttribute('opacity','.35');
    mark.setAttribute('cx',X); mark.setAttribute('cy',Y); mark.setAttribute('opacity','1');
    tip.innerHTML='<b>'+nf(d.v,2)+' L</b><span class="d">'
      + new Intl.DateTimeFormat('fr-FR',{day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'})
          .format(new Date(d.t)) + '</span>';
    tip.style.opacity=1;
    // Rester dans le cadre : basculer sous le point quand il est trop haut, et
    // ne jamais dépasser sur les bords.
    const lx=X/CH.w*r.width, ly=Y/CH.h*r.height;
    const tw=tip.offsetWidth, th=tip.offsetHeight;
    const below = ly < th+10;
    tip.classList.toggle('below', below);
    tip.style.left=Math.max(tw/2+2, Math.min(r.width-tw/2-2, lx))+'px';
    tip.style.top=(below ? ly+12 : ly-8)+'px';
  }
  function leave(){ cross.setAttribute('opacity','0'); mark.setAttribute('opacity','0'); tip.style.opacity=0; }
  hit.addEventListener('pointermove',move);
  hit.addEventListener('pointerdown',move);
  hit.addEventListener('pointerleave',leave);

  buildTable(data);
}
function buildTable(data){
  if(!data.length){ $('#dataTable').innerHTML='<p class="hint">Rien à afficher.</p>'; return; }
  const rows=data.slice(-40).reverse().map(d=>'<tr><td>'
    + new Intl.DateTimeFormat('fr-FR',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'})
        .format(new Date(d.t))
    + '</td><td class="n">'+nf(d.v,2)+'</td></tr>').join('');
  $('#dataTable').innerHTML='<table><thead><tr><th>Date</th><th class="n">Litres</th></tr></thead><tbody>'
    + rows + '</tbody></table>';
}

/* ---------- journal ---------- */
const EVMETA={serve:{i:'🥃',l:'Service'},refill:{i:'⬆',l:'Remplissage'},
  aging_reset:{i:'🛢',l:'Vieillissement redémarré'},cancelled:{i:'∅',l:'Variation ignorée'}};
async function loadEvents(){
  try{
    const list = await (await fetch('/api/events')).json();
    const shown = list.filter(e=>e.type!=='cancelled');
    $('#evEmpty').hidden = shown.length>0;
    $('#events').innerHTML = shown.map(e=>{
      const m=EVMETA[e.type]||{i:'•',l:e.type};
      const when = e.t ? new Intl.DateTimeFormat('fr-FR',{day:'numeric',month:'short',
        hour:'2-digit',minute:'2-digit'}).format(new Date(e.t*1000)) : 'date inconnue';
      const amt = e.type==='serve' ? '−'+nf(Math.abs(e.dml),0)+' ml'
                : e.type==='refill' ? '+'+nf(Math.abs(e.dml),0)+' ml' : '';
      return '<li><span class="ic">'+m.i+'</span><span class="t">'+m.l
        +'<br><span class="when">'+when+'</span></span><span class="amt">'+amt+'</span></li>';
    }).join('');
  }catch(e){}
}

/* ---------- chargement de l'historique ---------- */
document.querySelectorAll('.filters button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.filters button').forEach(x=>x.setAttribute('aria-pressed',x===b));
  days=+b.dataset.days; loadHistory();
});
async function loadHistory(){
  try{
    hist = await (await fetch('/api/history?days='+days+'&max=300')).json();
    drawChart(hist);
  }catch(e){ drawChart([]); }
}

/* ---------- temps réel ---------- */
async function refresh(){
  try{ render(await (await fetch('/api/state')).json()); }
  catch(e){ $('#dot').className='dot off'; }
}

/* Le WebSocket est le chemin normal. S'il tombe — proxy capricieux, Wi-Fi
   faible — on repasse en interrogation toutes les 3 s plutôt que d'afficher un
   écran figé. */
let wsAlive=false, pollTimer=null;
function startPolling(){ if(!pollTimer) pollTimer=setInterval(()=>{ if(!wsAlive) refresh(); },3000); }
function connect(){
  let ws;
  try{ ws = new WebSocket('ws://'+location.host+'/ws'); }
  catch(e){ startPolling(); return setTimeout(connect,8000); }
  ws.onopen  = ()=>{ wsAlive=true; };
  ws.onmessage = e=>{ try{ render(JSON.parse(e.data)); }catch(_){} };
  ws.onclose = ()=>{ wsAlive=false; startPolling(); setTimeout(connect,8000); };
  ws.onerror = ()=>{ try{ws.close()}catch(_){} };
}

refresh().then(()=>{ loadHistory(); loadEvents(); });
connect();
setInterval(loadEvents, 60000);
setInterval(loadHistory, 300000);
</script>
</body>
</html>
)RUMHTML";
