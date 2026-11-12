/* Recursos da placa na interface web (Second Screen OLED) - v2 visual.
 * Usa as rotas do firmware: /api/party, /api/teleport, /api/display/flip,
 * /api/pokemon/edit, /api/sd/status, /api/sd/upload, /api/sd/reload.
 * Sprites: tenta /api/sprite?sp=N (RGB565+alfa, 40x40, 3 bytes/px). Sem a rota,
 * mostra a inicial da espécie.
 */
(function () {
  'use strict';

  const MAPA = { xMin: -1320, xMax: -340, zMin: -990, zMax: -10 };
  const SPRITE = 40;

  const ACOES = [
    [0, 'Curar HP'], [1, 'Curar status'], [2, 'Restaurar PP'], [3, 'Restauração total'],
    [4, 'Alternar shiny'], [5, 'IVs máximos'], [6, 'Equipar item'], [7, 'Curar time inteiro'],
    [8, 'Level +1'], [9, 'Level +10'], [10, 'Level 100'], [11, 'EV Atk+Spe'],
    [12, 'EV SpA+Spe'], [13, 'EV HP+Def'], [14, 'Zerar EVs'], [15, 'Definir natureza'],
    [16, 'Alternar Alpha XXL']
  ];
  const NATUREZAS = ['Hardy', 'Lonely', 'Brave', 'Adamant', 'Naughty', 'Bold', 'Docile', 'Relaxed',
    'Impish', 'Lax', 'Timid', 'Hasty', 'Serious', 'Jolly', 'Naive', 'Modest', 'Mild', 'Quiet',
    'Bashful', 'Rash', 'Calm', 'Gentle', 'Sassy', 'Careful', 'Quirky'];

  const CSS = `
  #recursos-za{margin:0;max-width:none;background:#12121F;border:none;border-radius:0;padding:6px;height:224px;overflow-y:auto;color:#fff;font-family:system-ui,sans-serif;font-size:12px;box-sizing:border-box}
  #recursos-za h2{margin:0 0 10px;font-size:15px;color:#2D8CFF;text-transform:uppercase;letter-spacing:1px}
  #recursos-za .aba{background:#18182C;border:1px solid #2D3E66;border-radius:10px;padding:10px;margin-bottom:10px}
  #recursos-za .aba h3{margin:0 0 8px;font-size:13px;color:#A8B8E0}
  #recursos-za .grade{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  #recursos-za .card{background:#0F3460;border:2px solid #1F4E8C;border-radius:8px;padding:8px;display:flex;gap:8px;align-items:center;min-height:64px}
  #recursos-za .card.vivo{border-color:#00E676;background:#133E35}
  #recursos-za .card.zero{opacity:.45}
  #recursos-za canvas,#recursos-za .sprite{width:40px;height:40px;flex:0 0 40px;background:#1F1F38;border-radius:6px;display:flex;align-items:center;justify-content:center;font-weight:bold;color:#A8B8E0;image-rendering:pixelated}
  #recursos-za .info{flex:1;min-width:0}
  #recursos-za .nome{font-weight:bold;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
  #recursos-za .sub{color:#A8B8E0;font-size:11px}
  #recursos-za .barra{height:8px;background:#1F1F38;border-radius:4px;overflow:hidden;margin-top:4px}
  #recursos-za .barra>div{height:100%;background:#00E676}
  #recursos-za .barra>div.baixo{background:#FF1744}
  #recursos-za .tag{display:inline-block;background:#FFD740;color:#12121F;border-radius:4px;padding:0 4px;font-size:10px;font-weight:bold;margin-left:4px}
  #recursos-za button,#recursos-za select,#recursos-za input{font:inherit;color:#fff;background:#1F1F38;border:2px solid #2D3E66;border-radius:8px;padding:6px 10px;cursor:pointer}
  #recursos-za button:active{background:#2D3E66}
  #recursos-za button.primario{background:#0F3460;border-color:#2D8CFF}
  #recursos-za label{display:block;margin:6px 0;color:#A8B8E0}
  #recursos-za label>*{margin-left:6px}
  #recursos-za .linha{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  #recursos-za .msg{min-height:1.2em;margin-top:6px;font-size:12px}
  `;

  function $(id) { return document.getElementById(id); }
  function esc(t) { return String(t).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c])); }

  async function post(url, params) {
    const r = await fetch(url, { method: 'POST', body: params ? new URLSearchParams(params) : undefined });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  }
  function msg(el, texto, erro) {
    el.textContent = texto;
    el.style.color = erro ? '#FF8A80' : '#00E676';
  }

  // ---- Mapa: duplo clique teleporta ----
  function pctParaMundo(px, py) {
    return {
      wx: MAPA.xMin + (px / 100) * (MAPA.xMax - MAPA.xMin),
      wz: MAPA.zMax - (py / 100) * (MAPA.zMax - MAPA.zMin)
    };
  }
  function ligarTeleporte() {
    const mapa = $('mapbox');
    const status = $('rz-tp-msg');
    if (!mapa) return;
    mapa.addEventListener('dblclick', async (e) => {
      const r = mapa.getBoundingClientRect();
      const px = (e.clientX - r.left) / r.width * 100;
      const py = (e.clientY - r.top) / r.height * 100;
      if (px < 0 || px > 100 || py < 0 || py > 100) return;
      const c = pctParaMundo(px, py);
      if (!confirm('Teleportar para X=' + c.wx.toFixed(1) + ' Z=' + c.wz.toFixed(1) + '?')) return;
      try {
        await post('/api/teleport', { wx: c.wx.toFixed(2), wz: c.wz.toFixed(2) });
        msg(status, 'Teleporte enviado (X ' + c.wx.toFixed(0) + ', Z ' + c.wz.toFixed(0) + ').');
      } catch (err) { msg(status, 'Falha no teleporte: ' + err.message, true); }
    });
  }

  // ---- Time: cards com sprite, nível, HP e status ----
  function desenharSprite(el, bytes) {
    // LVGL 8 CF_TRUE_COLOR_ALPHA: por pixel [cor RGB565 (lo, hi), alfa]
    const c = document.createElement('canvas');
    c.width = SPRITE; c.height = SPRITE;
    const ctx = c.getContext('2d');
    const img = ctx.createImageData(SPRITE, SPRITE);
    for (let i = 0; i < SPRITE * SPRITE; i++) {
      const v = bytes[i * 3] | (bytes[i * 3 + 1] << 8);
      const a = bytes[i * 3 + 2];
      img.data[i * 4]     = ((v >> 11) & 0x1F) * 255 / 31;
      img.data[i * 4 + 1] = ((v >> 5) & 0x3F) * 255 / 63;
      img.data[i * 4 + 2] = (v & 0x1F) * 255 / 31;
      img.data[i * 4 + 3] = a;
    }
    ctx.putImageData(img, 0, 0);
    el.replaceWith(c);
  }

  const spriteOk = {};
  async function carregarSprite(sp, el) {
    if (spriteOk[sp] === false) return;
    try {
      const r = await fetch('/api/sprite?sp=' + sp, { cache: 'force-cache' });
      if (!r.ok) throw new Error();
      const buf = new Uint8Array(await r.arrayBuffer());
      if (buf.length !== SPRITE * SPRITE * 3) throw new Error();
      desenharSprite(el, buf);
    } catch (_) { spriteOk[sp] = false; }
  }

  function montarTime(grade) {
    grade.innerHTML = '';
    for (let i = 0; i < 6; i++) {
      const c = document.createElement('div');
      c.className = 'card zero';
      c.innerHTML = '<div class="sprite" data-sp="0">-</div>' +
        '<div class="info"><div class="nome">---</div><div class="sub">sem dados</div><div class="barra"><div style="width:0"></div></div></div>';
      grade.appendChild(c);
    }
  }

  async function atualizarTime() {
    const grade = $('rz-time');
    try {
      const d = await (await fetch('/api/party', { cache: 'no-store' })).json();
      const cards = grade.children;
      (d.party || []).forEach((p, i) => {
        const c = cards[i];
        if (!c) return;
        const vazio = !p.sp;
        c.className = 'card ' + (vazio ? 'zero' : 'vivo');
        const sprite = c.querySelector('.sprite');
        if (!vazio && sprite.dataset.sp !== String(p.sp)) {
          sprite.dataset.sp = p.sp;
          sprite.textContent = (p.n || '?').charAt(0).toUpperCase();
          carregarSprite(p.sp, sprite);
        }
        const pct = p.max > 0 ? Math.max(0, Math.min(100, p.hp / p.max * 100)) : 0;
        const tags = (p.shiny ? '<span class="tag">✦ shiny</span>' : '') + (p.alpha ? '<span class="tag">α</span>' : '');
        c.querySelector('.info').innerHTML =
          '<div class="nome">' + esc(p.n || '---') + tags + '</div>' +
          '<div class="sub">Nv ' + (p.lv || 0) + ' · ' + (p.hp || 0) + '/' + (p.max || 0) + (p.st ? ' · ' + esc(p.st) : '') + '</div>' +
          '<div class="barra"><div class="' + (pct < 30 ? 'baixo' : '') + '" style="width:' + pct.toFixed(0) + '%"></div></div>';
      });
    } catch (_) { /* sem conexao: mantem o ultimo estado */ }
  }

  // ---- Tela: girar 180 ----
  function ligarRotacao() {
    const btn = $('rz-flip');
    const status = $('rz-flip-msg');
    if (!btn) return;
    btn.addEventListener('click', async () => {
      try {
        const d = await post('/api/display/flip');
        msg(status, 'Tela ' + (d.flip ? 'girada 180°' : 'na posição normal') + '.');
      } catch (err) { msg(status, 'Falha ao girar: ' + err.message, true); }
    });
  }

  // ---- Editor de Pokémon ----
  function montarEditor() {
    const slot = $('rz-pk-slot');
    const acao = $('rz-pk-acao');
    const item = $('rz-pk-item');
    const nat = $('rz-pk-nat');
    const btn = $('rz-pk-aplicar');
    const status = $('rz-pk-msg');
    for (let i = 0; i < 6; i++) slot.add(new Option('Slot ' + (i + 1), i));
    ACOES.forEach(([v, t]) => acao.add(new Option(t, v)));
    NATUREZAS.forEach((n, i) => nat.add(new Option(n, i)));
    const atualiza = () => {
      const a = Number(acao.value);
      item.parentElement.style.display = a === 6 ? 'block' : 'none';
      nat.parentElement.style.display = a === 15 ? 'block' : 'none';
    };
    acao.addEventListener('change', atualiza);
    atualiza();
    btn.addEventListener('click', async () => {
      const a = Number(acao.value);
      let valor = 0;
      if (a === 6) valor = Number(item.value) || 0;
      if (a === 15) valor = Number(nat.value);
      try {
        await post('/api/pokemon/edit', { slot: slot.value, action: a, item: valor });
        msg(status, 'Aplicado: ' + acao.options[acao.selectedIndex].text + ' no slot ' + (Number(slot.value) + 1) + '.');
      } catch (err) { msg(status, 'Falha: ' + err.message, true); }
    });
  }

  // ---- SD ----
  async function atualizarSD() {
    const el = $('rz-sd-status');
    try {
      const d = await (await fetch('/api/sd/status', { cache: 'no-store' })).json();
      msg(el, d.sd_ready ? 'SD pronta: ' + d.used_mb + ' MB de ' + d.total_mb + ' MB usados.' : 'SD não encontrada.', !d.sd_ready);
    } catch (err) { msg(el, 'Falha ao ler a SD: ' + err.message, true); }
  }
  function ligarSD() {
    const status = $('rz-sd-msg');
    $('rz-sd-enviar').addEventListener('click', async () => {
      const arq = $('rz-sd-arquivo');
      if (!arq.files || !arq.files[0]) { msg(status, 'Escolha um arquivo.', true); return; }
      const f = arq.files[0];
      const destino = ($('rz-sd-caminho').value || '/' + f.name).trim();
      const form = new FormData();
      form.append('file', f, f.name);
      try {
        const r = await fetch('/api/sd/upload?path=' + encodeURIComponent(destino), { method: 'POST', body: form });
        const d = await r.json();
        if (!r.ok || !d.ok) throw new Error('a placa recusou o arquivo');
        msg(status, 'Gravado em ' + d.path + '.');
        atualizarSD();
      } catch (err) { msg(status, 'Falha no envio: ' + err.message, true); }
    });
    $('rz-sd-recarregar').addEventListener('click', async () => {
      try { await post('/api/sd/reload'); msg(status, 'Assets recarregados.'); }
      catch (err) { msg(status, 'Falha: ' + err.message, true); }
    });
    atualizarSD();
  }

  // ---- Painel ----
  function montarPainel() {
    const estilo = document.createElement('style');
    estilo.textContent = CSS;
    document.head.appendChild(estilo);

    const sec = document.createElement('section');
    sec.id = 'recursos-za';
    sec.innerHTML =
      '<h2>Recursos da placa</h2>' +

      '<div class="aba"><h3>Time</h3><div class="grade" id="rz-time"></div></div>' +

      '<div class="aba"><h3>Mapa</h3>' +
      '<div class="sub" style="color:#A8B8E0">Duplo clique no mapa acima para teleportar.</div>' +
      '<div class="msg" id="rz-tp-msg"></div></div>' +

      '<div class="aba"><h3>Tela</h3>' +
      '<div class="linha"><button class="primario" id="rz-flip">Girar 180°</button></div>' +
      '<div class="msg" id="rz-flip-msg"></div></div>' +

      '<div class="aba"><h3>Editor de Pokémon</h3>' +
      '<label>Pokémon: <select id="rz-pk-slot"></select></label>' +
      '<label>Ação: <select id="rz-pk-acao"></select></label>' +
      '<label>Item (ID): <input type="number" id="rz-pk-item" min="0" max="65535" value="0"></label>' +
      '<label>Natureza: <select id="rz-pk-nat"></select></label>' +
      '<div class="linha"><button class="primario" id="rz-pk-aplicar">Aplicar</button></div>' +
      '<div class="msg" id="rz-pk-msg"></div></div>' +

      '<div class="aba"><h3>Cartão SD</h3>' +
      '<div class="msg" id="rz-sd-status">Lendo SD...</div>' +
      '<label>Arquivo: <input type="file" id="rz-sd-arquivo"></label>' +
      '<label>Caminho: <input type="text" id="rz-sd-caminho" placeholder="/assets/pasta/arquivo.png" style="width:65%"></label>' +
      '<div class="linha"><button class="primario" id="rz-sd-enviar">Enviar</button>' +
      '<button id="rz-sd-recarregar">Recarregar assets</button></div>' +
      '<div class="msg" id="rz-sd-msg"></div></div>';
    const pagina = document.createElement('div');
    pagina.className = 'page';
    pagina.id = 'p-recursos';
    pagina.style.overflow = 'hidden';
    pagina.appendChild(sec);
    (document.querySelector('#stage') || document.body).appendChild(pagina);

    const abas = document.querySelector('#tabs');
    if (abas) {
      const botao = document.createElement('div');
      botao.className = 'tab';
      botao.id = 't-recursos';
      botao.textContent = 'Recursos';
      botao.onclick = () => window.tab('recursos');
      abas.appendChild(botao);
    }
    if (typeof window.tab === 'function') {
      const original = window.tab;
      window.tab = function (id) {
        original(id);
        const ativo = id === 'recursos';
        pagina.classList.toggle('active', ativo);
        const b = document.getElementById('t-recursos');
        if (b) b.classList.toggle('active', ativo);
      };
    }

    montarTime($('rz-time'));
    montarEditor();
  }

  document.addEventListener('DOMContentLoaded', () => {
    montarPainel();
    ligarTeleporte();
    ligarRotacao();
    ligarSD();
    atualizarTime();
    setInterval(atualizarTime, 1500);
  });
})();
