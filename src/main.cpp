#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "wifi_config.h"
#include "core/game_state.h"
#include "core/asset_manager.h"
#include "providers/provider_router.h"

WebServer server(80);
GameState state;
ProviderRouter router;

const char HTML_PROTOTYPE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='pt'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Switch Second Screen - Pico W Hub</title>
<style>
  body { background-color: #12121F; color: #FFFFFF; font-family: system-ui, sans-serif; margin: 0; padding: 0; user-select: none; }
  .top-bar { background: #1F1F38; padding: 14px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid #0F3460; }
  .title { color: #00F5D4; font-size: 18px; font-weight: bold; letter-spacing: 1px; }
  select { background: #0F3460; color: #00F5D4; border: 1px solid #00F5D4; padding: 8px 14px; border-radius: 6px; font-size: 14px; font-weight: bold; }
  .tabs { display: flex; background: #18182C; border-bottom: 1px solid #2E2E52; }
  .tab { flex: 1; padding: 14px; text-align: center; color: #8888AA; cursor: pointer; font-weight: bold; transition: 0.2s; }
  .tab.active { color: #00F5D4; border-bottom: 3px solid #00F5D4; background: #12121F; }
  .content { padding: 20px; display: none; max-width: 700px; margin: 0 auto; }
  .content.active { display: block; }
  .card { background: #1E1E36; border-radius: 10px; padding: 16px; margin-bottom: 16px; border: 1px solid #2E2E52; }
  .card h3 { margin-top: 0; color: #00F5D4; font-size: 16px; border-bottom: 1px solid #2E2E52; padding-bottom: 8px; }

  /* Pokémon HP Bars */
  .pkmn-row { margin-bottom: 12px; }
  .pkmn-header { display: flex; justify-content: space-between; font-size: 14px; margin-bottom: 4px; font-weight: bold; }
  .hp-bg { background: #0B1320; height: 14px; border-radius: 7px; overflow: hidden; border: 1px solid #2E2E52; }
  .hp-fill { height: 100%; transition: width 0.3s; }

  /* Zelda Hearts & Stamina */
  .hearts-container { display: flex; flex-wrap: wrap; gap: 6px; margin: 12px 0; }
  .heart-svg { width: 26px; height: 26px; filter: drop-shadow(0 0 4px rgba(255,23,68,0.5)); }
  .zelda-stats { display: flex; align-items: center; gap: 24px; margin-top: 15px; }
  .stamina-wheel { width: 54px; height: 54px; border-radius: 50%; border: 7px solid #00E676; box-shadow: 0 0 10px rgba(0,230,118,0.4); display: flex; align-items: center; justify-content: center; font-size: 11px; font-weight: bold; }

  /* Map Viewport & Tiles Sem Distorção */
  .map-viewport {
    width: 100%;
    max-width: 420px;
    aspect-ratio: 1 / 1;
    margin: 0 auto;
    background: #0B1320;
    border-radius: 10px;
    position: relative;
    overflow: hidden;
    border: 2px solid #0F3460;
  }
  .tile-grid {
    display: grid;
    width: 100%;
    height: 100%;
    position: absolute;
    top: 0;
    left: 0;
  }
  .map-tile-bg {
    background-size: 100% 100%;
    background-repeat: no-repeat;
  }

  .marker { position: absolute; transform: translate(-50%, -50%); font-size: 12px; font-weight: bold; display: flex; flex-direction: column; align-items: center; transition: 0.25s; z-index: 10; }
  .dot-player { width: 14px; height: 14px; background: #FF1744; border: 2px solid #FFF; border-radius: 50%; box-shadow: 0 0 8px #FF1744; }
  .dot-poi { width: 12px; height: 12px; background: #FFD600; border: 2px solid #12121F; border-radius: 3px; transform: rotate(45deg); }

  /* Macros Grid */
  .btn-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; }
  .btn { background-color: #0F3460; color: white; border: 1px solid #1F4E8C; padding: 16px; font-size: 15px; border-radius: 8px; cursor: pointer; font-weight: bold; }
  .btn:active { background-color: #00F5D4; color: #12121F; }
</style>
</head>
<body>

<div class='top-bar'>
  <div class='title'>SWITCH HUD // PICO W</div>
  <select id='gameSelect' onchange='changeGame(this.value)'>
    <option value='pkmn_za' selected>Pokémon Legends: Z-A (Lumiose)</option>
    <option value='pkmn_sv'>Pokémon Scarlet / Violet</option>
    <option value='zelda_totk'>Zelda: Tears of the Kingdom</option>
    <option value='zelda_botw'>Zelda: Breath of the Wild</option>
    <option value='mk8'>Mario Kart 8 Deluxe</option>
  </select>
</div>

<div class='tabs'>
  <div id='tab-status' class='tab active' onclick='switchTab("status")'>Status / HUD</div>
  <div id='tab-map' class='tab' onclick='switchTab("map")'>Mapa Dinâmico</div>
  <div id='tab-macros' class='tab' onclick='switchTab("macros")'>Macros</div>
  <div id='tab-config' class='tab' onclick='switchTab("config")'>Memória & Flash</div>
</div>

<!-- ABA 1: STATUS -->
<div id='content-status' class='content active'>
  <div id='hud-container'></div>
</div>

<!-- ABA 2: MAPA DINÂMICO POR JOGO -->
<div id='content-map' class='content'>
  <div style='display:flex; justify-content:space-between; margin-bottom:8px; font-size:13px; color:#00F5D4; font-family:monospace;'>
    <span id='map-zone'>Zona: Lumiose City (Prism Tower)</span>
    <span id='map-coords'>X: 2039 | Y: 2034</span>
  </div>
  <div class='map-viewport' id='mapViewport'>
    <div class='tile-grid' id='tileGrid'></div>
    <div class='marker' id='poi1' style='top:35%; left:60%;'>
      <div class='dot-poi'></div>
      <span id='poi1-label' style='color:#222; background:rgba(255,255,255,0.9); padding:1px 5px; border-radius:4px; font-size:10px; margin-top:2px; font-weight:bold;'>Mega Evolution Spot</span>
    </div>
    <div class='marker' id='playerMarker' style='top:50%; left:50%;'>
      <div class='dot-player'></div>
      <span style='color:#222; background:rgba(255,255,255,0.9); padding:1px 5px; border-radius:4px; font-size:10px; margin-top:2px; font-weight:bold;'>Você</span>
    </div>
  </div>
  <div style='margin-top:12px; display:flex; gap:8px;'>
    <button class='btn' style='padding:10px; flex:1;' onclick='movePlayer(-6, 0)'>◀ Oeste</button>
    <button class='btn' style='padding:10px; flex:1;' onclick='movePlayer(0, -6)'>▲ Norte</button>
    <button class='btn' style='padding:10px; flex:1;' onclick='movePlayer(0, 6)'>▼ Sul</button>
    <button class='btn' style='padding:10px; flex:1;' onclick='movePlayer(6, 0)'>Este ▶</button>
  </div>
</div>

<!-- ABA 3: MACROS -->
<div id='content-macros' class='content'>
  <div class='card'>
    <h3 id='macro-title'>Macros Dedicadas</h3>
    <div class='btn-grid' id='macro-buttons'></div>
  </div>
</div>

<!-- ABA 4: CONFIG -->
<div id='content-config' class='content'>
  <div class='card'>
    <h3>Arquitetura de Memória (Pico W)</h3>
    <p style='font-size:14px; color:#00E676;'>• <b>LittleFS (1MB Flash):</b> Servindo <code>/t/</code> (Lumiose) e <code>/assets/</code></p>
  </div>
</div>

<script>
let px = 50, py = 50;
let currentGame = 'pkmn_za';

function createHeartsHTML(total, full) {
  let html = '<div class="hearts-container">';
  for (let i = 0; i < total; i++) {
    const color = i < full ? '#FF1744' : '#3A3A55';
    html += `<svg class="heart-svg" viewBox="0 0 24 24" fill="${color}">
      <path d="M12 21.35l-1.45-1.32C5.4 15.36 2 12.28 2 8.5 2 5.42 4.42 3 7.5 3c1.74 0 3.41.81 4.5 2.09C13.09 3.81 14.76 3 16.5 3 19.58 3 22 5.42 22 8.5c0 3.78-3.4 6.86-8.55 11.54L12 21.35z"/>
    </svg>`;
  }
  html += '</div>';
  return html;
}

async function renderMapForGame(game) {
  const grid = document.getElementById('tileGrid');
  grid.innerHTML = '';

  if (game === 'pkmn_za') {
    try {
      const res = await fetch('/tiles_map.json');
      const data = await res.json();
      grid.style.gridTemplateColumns = `repeat(${data.cols}, 1fr)`;
      grid.style.gridTemplateRows = `repeat(${data.rows}, 1fr)`;

      data.tiles.forEach(tile => {
        const div = document.createElement('div');
        div.className = 'map-tile-bg';
        div.style.backgroundImage = `url("${tile.path}")`;
        div.style.gridColumn = `${tile.col + 1}`;
        div.style.gridRow = `${tile.row + 1}`;
        grid.appendChild(div);
      });
    } catch (err) {
      grid.innerHTML = `<div style='padding:20px; color:#FF1744;'>Erro ao ler /tiles_map.json</div>`;
    }
  } else {
    // Grelha tática topográfica dedicada para Zelda, Pokémon SV e MK8
    grid.style.gridTemplateColumns = 'repeat(3, 1fr)';
    grid.style.gridTemplateRows = 'repeat(3, 1fr)';

    const themes = {
      'zelda_totk': { bg: '#1B2A22', border: '#2E4F3E', label: 'Hyrule Sky/Surface', color: '#80CBC4' },
      'zelda_botw': { bg: '#1F291E', border: '#3B5238', label: 'Sheikah Slate Grid', color: '#4FC3F7' },
      'pkmn_sv':    { bg: '#251E38', border: '#3F325E', label: 'Paldea Terarium', color: '#CE93D8' },
      'mk8':        { bg: '#1A1C2E', border: '#2D3252', label: 'Track Sector', color: '#FFD54F' }
    };
    const t = themes[game] || themes['zelda_totk'];

    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        grid.innerHTML += `<div style='background:${t.bg}; border:1px solid ${t.border}; display:flex; flex-direction:column; align-items:center; justify-content:center; font-size:11px; color:${t.color}; font-family:monospace;'>
          <span>${t.label}</span>
          <span style='opacity:0.6; font-size:9px; margin-top:4px;'>Setor [${c},${r}]</span>
        </div>`;
      }
    }
  }
}

function switchTab(tabId) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));
  document.getElementById('tab-' + tabId).classList.add('active');
  document.getElementById('content-' + tabId).classList.add('active');
}

function movePlayer(dx, dy) {
  px = Math.max(10, Math.min(90, px + dx));
  py = Math.max(10, Math.min(90, py + dy));
  document.getElementById('playerMarker').style.left = px + '%';
  document.getElementById('playerMarker').style.top = py + '%';
  document.getElementById('map-coords').innerText = `X: ${2039 + Math.floor(px/5)} | Y: ${2034 + Math.floor(py/5)}`;
}

function changeGame(game) {
  currentGame = game;
  const hud = document.getElementById('hud-container');
  const macros = document.getElementById('macro-buttons');

  // Atualiza o mapa imediatamente conforme o jogo escolhido!
  renderMapForGame(game);

  if (game === 'pkmn_za') {
    document.getElementById('map-zone').innerText = 'Zona: Lumiose City (Prism Tower)';
    document.getElementById('poi1-label').innerText = 'Mega Evolution Spot';
    hud.innerHTML = `
      <div class='card'>
        <h3>Pokémon Legends: Z-A // Party Status</h3>
        <div class='pkmn-row'>
          <div class='pkmn-header'><span>⚡ Mega Pikachu Lv.90</span><span style='color:#00E676;'>260 / 260 HP</span></div>
          <div class='hp-bg'><div class='hp-fill' style='width:100%; background:#00E676;'></div></div>
        </div>
        <div class='pkmn-row'>
          <div class='pkmn-header'><span>🔥 Delphox Lv.88</span><span style='color:#00E676;'>290 / 290 HP</span></div>
          <div class='hp-bg'><div class='hp-fill' style='width:100%; background:#00E676;'></div></div>
        </div>
        <div class='pkmn-row'>
          <div class='pkmn-header'><span>🐉 Zygarde (50%) Lv.95</span><span style='color:#FFD600;'>195 / 350 HP</span></div>
          <div class='hp-bg'><div class='hp-fill' style='width:55%; background:#FFD600;'></div></div>
        </div>
      </div>`;
    macros.innerHTML = `
      <button class='btn'>Auto-Catch Alpha</button>
      <button class='btn'>Fast Travel Lumiose</button>
      <button class='btn'>Shiny Hunt Macro</button>
      <button class='btn'>Instant Mega-Evolve</button>`;
  }
  else if (game === 'pkmn_sv') {
    document.getElementById('map-zone').innerText = 'Zona: Paldea (Area Zero)';
    document.getElementById('poi1-label').innerText = 'Tera Raid 6★';
    hud.innerHTML = `
      <div class='card'>
        <h3>Pokémon Scarlet / Violet // Party Status</h3>
        <div class='pkmn-row'>
          <div class='pkmn-header'><span>🦎 Koraidon Lv.100</span><span style='color:#00E676;'>340 / 340 HP</span></div>
          <div class='hp-bg'><div class='hp-fill' style='width:100%; background:#00E676;'></div></div>
        </div>
        <div class='pkmn-row'>
          <div class='pkmn-header'><span>🗡️ Ceruledge Lv.92</span><span style='color:#FF1744;'>45 / 280 HP</span></div>
          <div class='hp-bg'><div class='hp-fill' style='width:16%; background:#FF1744;'></div></div>
        </div>
      </div>`;
    macros.innerHTML = `
      <button class='btn'>Auto-Hatch Ovos</button>
      <button class='btn'>Picnic Reset</button>
      <button class='btn'>Item Printer RNG</button>
      <button class='btn'>Auto Raid Ready</button>`;
  }
  else if (game === 'zelda_totk' || game === 'zelda_botw') {
    const isTotk = (game === 'zelda_totk');
    document.getElementById('map-zone').innerText = isTotk ? 'Zona: Central Hyrule (Sky Island)' : 'Zona: Hyrule Castle Sanctum';
    document.getElementById('poi1-label').innerText = isTotk ? 'Lightroot / Shrine' : 'Ancient Shrine';
    hud.innerHTML = `
      <div class='card'>
        <h3>${isTotk ? 'Zelda: Tears of the Kingdom' : 'Zelda: Breath of the Wild'} // Link Status</h3>
        ${createHeartsHTML(20, isTotk ? 17 : 14)}
        <div class='zelda-stats'>
          <div class='stamina-wheel'>300%</div>
          <div>
            <div style='font-size:14px; color:#00E676; font-weight:bold;'>Stamina Wheel: 3 Anéis</div>
            <div style='font-size:13px; color:#AAA; margin-top:4px;'>${isTotk ? '🔋 Zonaite Energy: 8 / 8 Células' : '🛡️ Daruk Protection: Pronto'}</div>
          </div>
        </div>
      </div>`;
    macros.innerHTML = isTotk ? `
      <button class='btn'>Duplicar Minérios</button>
      <button class='btn'>Hoverbike Auto-Build</button>
      <button class='btn'>Bow Fuse Rápido</button>
      <button class='btn'>Shield Surf Jump</button>` : `
      <button class='btn'>Windbomb Setup</button>
      <button class='btn'>Whistle Sprint</button>
      <button class='btn'>Menu Overload</button>
      <button class='btn'>Shield Block Reset</button>`;
  }
  else if (game === 'mk8') {
    document.getElementById('map-zone').innerText = 'Pista: Rainbow Road (200cc)';
    document.getElementById('poi1-label').innerText = 'Double Item Box';
    hud.innerHTML = `
      <div class='card'>
        <h3>Mario Kart 8 Deluxe // Telemetria</h3>
        <div style='display:flex; justify-content:space-between; align-items:center;'>
          <div style='font-size:36px; font-weight:bold; color:#FFD600;'>1º Lugar</div>
          <div style='text-align:right;'>
            <div style='font-size:16px; color:#00F5D4; font-weight:bold;'>🪙 10 / 10 Moedas</div>
            <div style='font-size:13px; color:#AAA; margin-top:4px;'>Volta: 3 / 3 • Mini-Turbo Ativo</div>
          </div>
        </div>
      </div>`;
    macros.innerHTML = `
      <button class='btn'>Soft Drift Assist</button>
      <button class='btn'>Rear-View Lock</button>
      <button class='btn'>Item Smuggle Drop</button>
      <button class='btn'>Fast Restart</button>`;
  }
}

changeGame('pkmn_za');
</script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!LittleFS.begin()) {
        Serial.println("[Erro] Falha ao montar LittleFS!");
    } else {
        Serial.println("[OK] LittleFS montado com sucesso!");
    }

    router.init(state);
    AssetManager::initSD();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("A ligar ao Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi conectado!");
    Serial.print("Aceda: http://");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", HTML_PROTOTYPE);
    });

    // Serve os tiles de Lumiose e todos os assets copiados de sdcard_staging
    server.serveStatic("/tiles_map.json", LittleFS, "/tiles_map.json", "max-age=3600");
    server.serveStatic("/t", LittleFS, "/t", "max-age=86400");
    server.serveStatic("/assets", LittleFS, "/assets", "max-age=86400");

    server.begin();
}

void loop() {
    server.handleClient();
}