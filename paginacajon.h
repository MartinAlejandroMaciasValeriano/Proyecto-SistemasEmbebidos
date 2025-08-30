#ifndef PAGINACAJON_H
#define PAGINACAJON_H

#include <WebServer.h>

class PaginaWeb {
public:
  static void handlePagina(WebServer &server) {
    static const char page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Inventario — ESP32</title>
<style>
  *{box-sizing:border-box}
  body{margin:0;font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial;background:#0b0f14;color:#e9eef3;display:flex;min-height:100vh}
  .wrap{margin:auto; width:min(900px, 96vw); padding:20px}
  .card{background:#141a22;border:1px solid #1f2a36;border-radius:16px; padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.25)}
  h1{margin:0 0 12px 0;font-size:22px}
  .row{display:flex; gap:12px; flex-wrap:wrap}
  input,button{padding:12px;border-radius:10px;border:1px solid #273446;background:#0f141b;color:#e9eef3;font-size:15px;outline:none}
  input:focus{border-color:#3a89ff}
  button{background:#3a89ff;border-color:#3a89ff;cursor:pointer}
  button.secondary{background:#223044;border-color:#223044}
  button:disabled{opacity:.45; cursor:not-allowed}
  .list{max-height:340px; overflow:auto; border:1px solid #273446; border-radius:12px; padding:8px}
  .item{display:flex; justify-content:space-between; align-items:center; padding:6px 8px; border-bottom:1px dashed #273446}
  .item:last-child{border-bottom:0}
  .muted{opacity:.7}
  .tabs{display:flex; gap:8px; margin:12px 0}
  .tab{padding:10px 14px; border-radius:10px; border:1px solid #273446; cursor:pointer}
  .tab.active{background:#3a89ff; border-color:#3a89ff}
  .view{display:none}
  .view.active{display:block}
  .qtypill{padding:6px 12px; border-radius:999px; background:#0f141b; border:1px solid #273446; font-size:14px}
</style>
<script>
window.addEventListener('gesturestart', e=> e.preventDefault());
window.addEventListener('gesturechange', e=> e.preventDefault());
</script>
<script type="module">
  import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.4/firebase-app.js";
  import { getDatabase, ref, get, update, set, serverTimestamp } from "https://www.gstatic.com/firebasejs/10.12.4/firebase-database.js";

  const firebaseConfig = {
    apiKey: "AIzaSyCnhFQC4vMfuHdPmXGrGgH1qFT-j4136Gw",
    authDomain: "inventario-f73ee.firebaseapp.com",
    databaseURL: "https://inventario-f73ee-default-rtdb.firebaseio.com",
    projectId: "inventario-f73ee",
    storageBucket: "inventario-f73ee.firebasestorage.app",
    messagingSenderId: "422498677931",
    appId: "1:422498677931:web:5610503783d1d3afce73ee"
  };
  const app = initializeApp(firebaseConfig);
  const db  = getDatabase(app);

  const state = { step:1, mode:"prestar", user:null, catalog:{}, drawers:{}, cart:{}, borrowed:{} };
  const qs  = s => document.querySelector(s);
  const qsa = s => [...document.querySelectorAll(s)];
  function show(step){ qsa('.view').forEach(v=>v.classList.remove('active')); qs('#view-'+step).classList.add('active'); state.step=step; }
  function setMode(mode){
    state.mode = mode;
    qsa('.tab').forEach(t=>t.classList.remove('active'));
    qs(mode==='prestar' ? '#tab-prestar' :
       mode==='devolver'? '#tab-devolver' : '#tab-agregar').classList.add('active');
    qs('#btn-commit').textContent =
      mode==='prestar'  ? 'Confirmar préstamo' :
      mode==='devolver' ? 'Confirmar devolución' :
                          'Confirmar agregado';
  }
  function renderCurrentList(){ 
    if(state.mode==='devolver') renderBorrowed();
    else renderCatalog();
  }

  // Paso 1: solo matrícula y usuario (minúsculas)
  async function saveUserProfile(matricula, usuario){
    const m = (matricula || "").trim().toLowerCase();
    const u = (usuario  || "").trim().toLowerCase();
    if(!m || !u) throw new Error("Faltan datos");
    state.user = { matricula: m, usuario: u };
    localStorage.setItem("inv_user", JSON.stringify(state.user));
    await update(ref(db, `users/${m}`), { usuario: u });
  }

  // Datos base y saldos
  async function loadData(){
    const [catSnap, drwSnap] = await Promise.all([
      get(ref(db, "catalog")), get(ref(db, "drawers"))
    ]);
    state.catalog = catSnap.exists()? catSnap.val() : {};
    state.drawers = drwSnap.exists()? drwSnap.val() : {};
  }
  async function loadBorrowed(matricula){
    const m = (matricula || "").trim().toLowerCase();
    if(!m){ state.borrowed = {}; return; }
    const snap = await get(ref(db, `byUser/${m}/items`));
    state.borrowed = snap.exists()? snap.val() : {};
  }

  // Diagnóstico: llamar al endpoint del ESP32 para iluminar N
  async function locateDrawer(n){
    try{
      const r = await fetch(`/api/cajon?numero=${n}`);
      alert(`Señal enviada al cajón ${n} ${r.ok ? '(OK)' : '(falló)'}`);
    }catch(e){ console.error(e); alert('No se pudo enviar la señal al ESP32.'); }
  }

  // PRESTAR / AGREGAR: lista completa (disponibilidad viva en PRESTAR)
  function renderCatalog(){
    const all = Object.keys(state.catalog).sort();
    const cont = qs("#list-comp");
    cont.innerHTML = "";
    for(const name of all){
      const drawer = state.catalog[name].drawer;
      const stock  = (state.drawers?.[drawer]?.components?.[name]) ?? 0;
      const alreadyInCart = state.cart[name] || 0;
      const availableLeft = Math.max(0, stock - (state.mode==='prestar' ? alreadyInCart : 0));
      const disabled = (state.mode==='prestar' && availableLeft===0);

      const row = document.createElement("div");
      row.className = "item";

      const sub = state.mode==='prestar'
        ? `Caja ${drawer} • Disponible: ${availableLeft}`
        : `Caja ${drawer} • Disponible: ${stock}`;

      const qtyInput = state.mode==='prestar'
        ? `<input type="number" min="1" max="${Math.max(1, availableLeft)}" value="1" style="width:90px" />`
        : `<input type="number" min="1" value="1" style="width:90px" />`;

      row.innerHTML = `
        <div>
          <div>${name}</div>
          <div class="muted">${sub}</div>
        </div>
        <div class="right">
          <button class="secondary" data-drawer="${drawer}">Ubicar</button>
          ${qtyInput}
          <button data-name="${name}" ${disabled?'disabled':''}>Agregar</button>
        </div>`;
      cont.appendChild(row);
    }
    cont.querySelectorAll("button[data-name]").forEach(btn=>{
      btn.onclick = ()=>{
        const item = btn.closest(".item");
        const input = item.querySelector("input");
        let n = parseInt(input.value||"1");
        const name = btn.dataset.name;
        addToCart(name, n);
      };
    });
    cont.querySelectorAll("button.secondary[data-drawer]").forEach(btn=>{
      btn.onclick = ()=> locateDrawer(parseInt(btn.dataset.drawer));
    });
  }

  // DEVOLVER: solo lo pendiente (restando lo ya puesto en carrito)
  function renderBorrowed(){
    const keys = Object.keys(state.borrowed||{}).filter(k=> (state.borrowed[k]||0) > 0).sort();
    const cont = qs("#list-comp");
    cont.innerHTML = "";
    if(keys.length===0){ cont.innerHTML = `<div class="muted">No tienes componentes pendientes por devolver.</div>`; return; }
    for(const name of keys){
      const drawer = state.catalog[name]?.drawer || "?";
      const haveTotal   = state.borrowed[name] || 0;
      const alreadyInCart = state.cart[name] || 0;
      const haveLeft = Math.max(0, haveTotal - alreadyInCart);
      const disabled = haveLeft===0;

      const row = document.createElement("div");
      row.className = "item";
      row.innerHTML = `
        <div>
          <div>${name}</div>
          <div class="muted">En tu poder: ${haveLeft} • Caja ${drawer}</div>
        </div>
        <div class="right">
          <button class="secondary" data-drawer="${drawer}">Ubicar</button>
          <input type="number" min="1" max="${Math.max(1, haveLeft)}" value="1" style="width:90px" />
          <button data-name="${name}" ${disabled?'disabled':''}>Agregar</button>
        </div>`;
      cont.appendChild(row);
    }
    cont.querySelectorAll("button[data-name]").forEach(btn=>{
      btn.onclick = ()=>{
        const item = btn.closest(".item");
        const input = item.querySelector("input");
        let n = parseInt(input.value||"1");
        const name = btn.dataset.name;
        addToCart(name, n);
      };
    });
    cont.querySelectorAll("button.secondary[data-drawer]").forEach(btn=>{
      btn.onclick = ()=> locateDrawer(parseInt(btn.dataset.drawer));
    });
  }

  // Carrito (sin inputs editables)
  function addToCart(name, n){
    if(n<=0 || isNaN(n)) return;
    const drawer = state.catalog[name]?.drawer;

    if(state.mode==='prestar'){
      const stock  = (state.drawers?.[drawer]?.components?.[name]) ?? 0;
      const already= state.cart[name] || 0;
      const left   = Math.max(0, stock - already);
      n = Math.min(n, left);
    } else if(state.mode==='devolver'){
      const have   = state.borrowed[name] || 0;
      const already= state.cart[name] || 0;
      const left   = Math.max(0, have - already);
      n = Math.min(n, left);
    }
    if(n<=0) return;

    state.cart[name] = (state.cart[name]||0) + n;
    renderCart();
    renderCurrentList();
  }

  function renderCart(){
    const cont = qs("#cart");
    cont.innerHTML = "";
    const names = Object.keys(state.cart);
    if(names.length===0){ cont.innerHTML = `<div class="muted">Sin items</div>`; return; }
    for(const name of names){
      const n = state.cart[name];
      const drawer = state.catalog[name]?.drawer;
      const row = document.createElement("div"); row.className="item";
      row.innerHTML = `
        <div>
          <div>${name}</div>
          <div class="muted">(Caja ${drawer})</div>
        </div>
        <div class="right">
          <span class="qtypill">x&nbsp;${n}</span>
          <button class="secondary" data-name="${name}">Quitar</button>
        </div>`;
      cont.appendChild(row);
    }
    cont.querySelectorAll("button.secondary").forEach(btn=>{
      btn.onclick = ()=>{
        delete state.cart[btn.dataset.name];
        renderCart();
        renderCurrentList();
      };
    });
  }

  // Confirmar operación
  async function commitOperation(){
    if(!state.user){ alert("Primero ingresa tu usuario y matrícula."); show(1); return; }
    const items = {...state.cart};
    if(Object.keys(items).length===0){ alert("Agrega al menos un componente."); return; }

    const now = Date.now();
    const m = (state.user.matricula || "").toLowerCase();
    const u = (state.user.usuario  || "").toLowerCase();

    const base = state.mode === "prestar" ? "history/loans"
               : state.mode === "devolver" ? "history/returns"
               : "history/adds";
    const userKey = `${m}__${u}`;
    const tsKey   = String(now);
    await set(ref(db, `${base}/${userKey}/${tsKey}`), {
      matricula: m, usuario: u, items,
      ts: serverTimestamp(), ts_client_ms: now, iso_client: new Date(now).toISOString()
    });

    const updates = {};

    if(state.mode === 'prestar' || state.mode === 'devolver'){
      const mult = (state.mode==='prestar') ? -1 : +1;

      // drawers
      for(const [name, qty] of Object.entries(items)){
        const drawer = state.catalog[name]?.drawer; if(!drawer) continue;
        const curr = (state.drawers?.[drawer]?.components?.[name]) ?? 0;
        const nv = Math.max(0, curr + mult*qty);
        updates[`drawers/${drawer}/components/${name}`]= nv;
      }

      // byUser
      updates[`byUser/${m}/usuario`] = u;
      for(const [name, qty] of Object.entries(items)){
        const current = (state.borrowed?.[name] || 0);
        const next = state.mode==='prestar' ? (current + qty) : Math.max(0, current - qty);
        updates[`byUser/${m}/items/${name}`] = next>0 ? next : null;
      }
    } else {
      // AGREGAR: solo stock
      for(const [name, qty] of Object.entries(items)){
        const drawer = state.catalog[name]?.drawer; if(!drawer) continue;
        const curr = (state.drawers?.[drawer]?.components?.[name]) ?? 0;
        updates[`drawers/${drawer}/components/${name}`]= Math.max(0, curr + qty);
      }
    }

    await update(ref(db), updates);

    // Reset: volver al Paso 1
    state.cart = {};
    state.user = null;
    localStorage.removeItem('inv_user');
    if(qs('#f-matricula')) qs('#f-matricula').value = '';
    if(qs('#f-usuario'))   qs('#f-usuario').value   = '';
    setMode('prestar');
    show(1);
    alert("Operación registrada correctamente.");
  }

  // Boot
  window.addEventListener("load", async ()=>{
    const saved = localStorage.getItem("inv_user");
    if(saved){
      state.user = JSON.parse(saved);
      qs("#f-matricula").value = state.user.matricula || "";
      qs("#f-usuario").value   = state.user.usuario   || "";
    }
    await loadData();
    setMode("prestar");
    show(1);

    // Paso 1 → Paso 2
    qs("#btn-next-1").onclick = async ()=>{
      try{
        const matricula = (qs("#f-matricula")?.value || "").trim().toLowerCase();
        const usuario   = (qs("#f-usuario")?.value   || "").trim().toLowerCase();
        if(!matricula || !usuario){ alert("Completa matrícula y usuario ESPOL."); return; }
        await saveUserProfile(matricula, usuario);
        show(2);
      }catch(e){ console.error(e); alert("No se pudo continuar. Refresca con Ctrl+F5."); }
    };

    // Paso 2 → Paso 3
    qs("#tab-prestar").onclick  = ()=> setMode("prestar");
    qs("#tab-devolver").onclick = ()=> setMode("devolver");
    qs("#tab-agregar").onclick  = ()=> setMode("agregar");
    qs("#btn-next-2").onclick = async ()=>{
      if(!state.user || !state.user.matricula){ alert("Primero ingresa tu matrícula."); show(1); return; }
      if(state.mode==="devolver"){ await loadBorrowed(state.user.matricula); }
      renderCurrentList(); show(3);
    };

    // Paso 3
    qs("#btn-commit").onclick = ()=> commitOperation();
    qs("#btn-back-2").onclick = ()=> show(1);
    qs("#btn-back-3").onclick = ()=> show(2);
  });
</script>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>Inventario inteligente — ESP32</h1>

      <!-- Paso 1: Identificación -->
      <div id="view-1" class="view active">
        <h3>Identificación</h3>
        <div class="row">
          <input id="f-usuario"   placeholder="Usuario ESPOL">
          <input id="f-matricula" placeholder="Matrícula (ej: 202209367)">
        </div>
        <div class="row" style="justify-content:flex-end;margin-top:12px">
          <button id="btn-next-1">Siguiente</button>
        </div>
      </div>

      <!-- Paso 2: Operación -->
      <div id="view-2" class="view">
        <h3>¿Qué deseas hacer?</h3>
        <div class="tabs">
          <div id="tab-prestar"  class="tab active">Prestar</div>
          <div id="tab-devolver" class="tab">Devolver</div>
          <div id="tab-agregar"  class="tab">Agregar</div>
        </div>
        <div class="row" style="justify-content:space-between;margin-top:12px">
          <button class="secondary" id="btn-back-2">Atrás</button>
          <button id="btn-next-2">Siguiente</button>
        </div>
      </div>

      <!-- Paso 3: Lista / Carrito -->
      <div id="view-3" class="view">
        <div class="row" style="align-items:flex-start">
          <div style="flex:1;min-width:280px">
            <h3>Componentes</h3>
            <div id="list-comp" class="list"></div>
          </div>
          <div style="flex:1;min-width:280px">
            <h3>Selección</h3>
            <div id="cart" class="list" style="min-height:120px"></div>
            <div class="row" style="margin-top:12px; justify-content:space-between">
              <button class="secondary" id="btn-back-3">Atrás</button>
              <button id="btn-commit">Confirmar préstamo</button>
            </div>
          </div>
        </div>
      </div>

    </div>
  </div>
</body>
</html>
)=====";

    server.send(200, "text/html; charset=utf-8", FPSTR(page));
  }

  static void handleCajon(WebServer &server, void (*accion)(int)) {
    if (server.hasArg("numero")) {
      int n = server.arg("numero").toInt();
      accion(n);
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(400, "text/plain", "Falta parámetro 'numero'");
    }
  }
};

#endif
