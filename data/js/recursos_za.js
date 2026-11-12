/* Recursos da placa que faltavam na interface web (Second Screen OLED).
 * Usa as rotas que o firmware ja expoe: /api/teleport, /api/display/flip,
 * /api/pokemon/edit, /api/sd/status, /api/sd/upload, /api/sd/reload.
 * Carregado por <script src="/js/recursos_za.js"> no index.html.
 */
(function () {
  'use strict';

  // Espelha za_memory_map.h: bordas do mundo em Lumiose.
  const MAPA = { xMin: -1320, xMax: -340, zMin: -990, zMax: -10 };

  const ACOES = [
    [0, 'Curar HP'], [1, 'Curar status'], [2, 'Restaurar PP'], [3, 'Restauracao total'],
    [4, 'Alternar shiny'], [5, 'IVs maximos'], [6, 'Equipar item'], [7, 'Curar time inteiro'],
    [8, 'Level +1'], [9, 'Level +10'], [10, 'Level 100'], [11, 'EV Atk+Spe'],
    [12, 'EV SpA+Spe'], [13, 'EV HP+Def'], [14, 'Zerar EVs'], [15, 'Definir natureza'],
    [16, 'Alternar Alpha XXL']
  ];

  const NATUREZAS = ['Hardy', 'Lonely', 'Brave', 'Adamant', 'Naughty', 'Bold', 'Docile', 'Relaxed',
    'Impish', 'Lax', 'Timid', 'Hasty', 'Serious', 'Jolly', 'Naive', 'Modest', 'Mild', 'Quiet',
    'Bashful', 'Rash', 'Calm', 'Gentle', 'Sassy', 'Careful', 'Quirky'];

  function $(id) { return document.getElementById(id); }

  async function post(url, params) {
    const body = params ? new URLSearchParams(params) : undefined;
    const r = await fetch(url, { method: 'POST', body: body });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  }

  function msg(el, texto, erro) {
    el.textContent = texto;
    el.style.color = erro ? '#ff8a80' : '#9ccc65';
  }

  // Clique no mapa (em % da caixa) -> coordenadas do jogo.
  // px: 0 = esquerda (X minimo); py: 0 = topo (Z maximo, norte).
  function pctParaMundo(px, py) {
    const wx = MAPA.xMin + (px / 100) * (MAPA.xMax - MAPA.xMin);
    const wz = MAPA.zMax - (py / 100) * (MAPA.zMax - MAPA.zMin);
    return { wx: wx, wz: wz };
  }

  function ligarTeleporte() {
    const mapa = $('mapbox');
    const status = $('rz-tp-status');
    if (!mapa) return;
    mapa.addEventListener('dblclick', async (e) => {
      const r = mapa.getBoundingClientRect();
      const px = (e.clientX - r.left) / r.width * 100;
      const py = (e.clientY - r.top) / r.height * 100;
      if (px < 0 || px > 100 || py < 0 || py > 100) return;
      const c = pctParaMundo(px, py);
      if (!confirm('Teleportar o personagem para X=' + c.wx.toFixed(1) + ' Z=' + c.wz.toFixed(1) + '?')) return;
      try {
        await post('/api/teleport', { wx: c.wx.toFixed(2), wz: c.wz.toFixed(2) });
        msg(status, 'Teleporte enviado (X ' + c.wx.toFixed(0) + ', Z ' + c.wz.toFixed(0) + ').');
      } catch (err) {
        msg(status, 'Falha no teleporte: ' + err.message, true);
      }
    });
  }

  function ligarRotacao() {
    const btn = $('rz-flip');
    const status = $('rz-flip-status');
    if (!btn) return;
    btn.addEventListener('click', async () => {
      try {
        const d = await post('/api/display/flip');
        msg(status, 'Tela ' + (d.flip ? 'girada 180 graus' : 'na posicao normal') + '.');
      } catch (err) {
        msg(status, 'Falha ao girar: ' + err.message, true);
      }
    });
  }

  function montarEditorPokemon(container) {
    container.innerHTML = '';
    const linha = (rotulo, el) => {
      const d = document.createElement('label');
      d.style.display = 'block';
      d.style.margin = '6px 0';
      d.textContent = rotulo + ' ';
      d.appendChild(el);
      return d;
    };

    const slot = document.createElement('select');
    slot.id = 'rz-pk-slot';
    for (let i = 0; i < 6; i++) {
      const o = document.createElement('option');
      o.value = i; o.textContent = 'Slot ' + (i + 1);
      slot.appendChild(o);
    }

    const acao = document.createElement('select');
    acao.id = 'rz-pk-acao';
    ACOES.forEach(([v, t]) => {
      const o = document.createElement('option');
      o.value = v; o.textContent = t;
      acao.appendChild(o);
    });

    const item = document.createElement('input');
    item.id = 'rz-pk-item';
    item.type = 'number'; item.min = 0; item.max = 65535; item.value = 0;
    item.placeholder = 'ID do item';

    const natureza = document.createElement('select');
    natureza.id = 'rz-pk-natureza';
    NATUREZAS.forEach((n, i) => {
      const o = document.createElement('option');
      o.value = i; o.textContent = n;
      natureza.appendChild(o);
    });

    const enviarBtn = document.createElement('button');
    enviarBtn.textContent = 'Aplicar';
    const status = document.createElement('div');
    status.id = 'rz-pk-status';
    status.style.minHeight = '1.2em';

    const atualizaCampos = () => {
      const a = Number(acao.value);
      item.parentElement.style.display = a === 6 ? 'block' : 'none';
      natureza.parentElement.style.display = a === 15 ? 'block' : 'none';
    };
    acao.addEventListener('change', atualizaCampos);

    enviarBtn.addEventListener('click', async () => {
      const a = Number(acao.value);
      let valor = 0;
      if (a === 6) valor = Number(item.value) || 0;
      if (a === 15) valor = Number(natureza.value);
      try {
        await post('/api/pokemon/edit', { slot: slot.value, action: a, item: valor });
        msg(status, 'Aplicado: ' + acao.options[acao.selectedIndex].text + ' no slot ' + (Number(slot.value) + 1) + '.');
      } catch (err) {
        msg(status, 'Falha: ' + err.message, true);
      }
    });

    container.appendChild(linha('Pokemon:', slot));
    container.appendChild(linha('Acao:', acao));
    container.appendChild(linha('Item:', item));
    container.appendChild(linha('Natureza:', natureza));
    container.appendChild(enviarBtn);
    container.appendChild(status);
    atualizaCampos();
  }

  async function atualizarStatusSD() {
    const el = $('rz-sd-status');
    try {
      const r = await fetch('/api/sd/status');
      const d = await r.json();
      el.textContent = d.sd_ready
        ? 'SD pronta: ' + d.used_mb + ' MB usados de ' + d.total_mb + ' MB.'
        : 'SD nao encontrada.';
      el.style.color = d.sd_ready ? '#9ccc65' : '#ff8a80';
    } catch (err) {
      el.textContent = 'Falha ao ler a SD: ' + err.message;
      el.style.color = '#ff8a80';
    }
  }

  function ligarSD() {
    const up = $('rz-sd-enviar');
    const arq = $('rz-sd-arquivo');
    const caminho = $('rz-sd-caminho');
    const recarregar = $('rz-sd-recarregar');
    const status = $('rz-sd-msg');
    if (!up) return;

    atualizarStatusSD();

    up.addEventListener('click', async () => {
      if (!arq.files || !arq.files[0]) { msg(status, 'Escolha um arquivo.', true); return; }
      const f = arq.files[0];
      const destino = (caminho.value || '/' + f.name).trim();
      const form = new FormData();
      form.append('file', f, f.name);
      try {
        const r = await fetch('/api/sd/upload?path=' + encodeURIComponent(destino), { method: 'POST', body: form });
        const d = await r.json();
        if (!r.ok || !d.ok) throw new Error('a placa recusou o arquivo');
        msg(status, 'Gravado em ' + d.path + '.');
        atualizarStatusSD();
      } catch (err) {
        msg(status, 'Falha no envio: ' + err.message, true);
      }
    });

    recarregar.addEventListener('click', async () => {
      try {
        await post('/api/sd/reload');
        msg(status, 'Assets recarregados da SD.');
      } catch (err) {
        msg(status, 'Falha ao recarregar: ' + err.message, true);
      }
    });
  }

  function montarPainel() {
    const estilo = document.createElement('style');
    estilo.textContent = '#recursos-za{margin:24px auto;max-width:720px;padding:16px;border:1px solid #37474f;border-radius:10px;font-family:system-ui,sans-serif}' +
      '#recursos-za h2{margin:0 0 10px;font-size:18px}' +
      '#recursos-za .bloco{margin:12px 0;padding-top:10px;border-top:1px solid #263238}' +
      '#recursos-za button{padding:6px 12px;margin-top:6px;cursor:pointer}';
    document.head.appendChild(estilo);

    const sec = document.createElement('section');
    sec.id = 'recursos-za';
    sec.innerHTML =
      '<h2>Recursos da placa</h2>' +
      '<div class="bloco"><strong>Mapa: teleporte</strong>' +
      '<p>De duplo clique no mapa para teleportar o personagem para aquele ponto.</p>' +
      '<div id="rz-tp-status"></div></div>' +
      '<div class="bloco"><strong>Tela</strong>' +
      '<p><button id="rz-flip">Girar tela 180 graus</button></p>' +
      '<div id="rz-flip-status"></div></div>' +
      '<div class="bloco"><strong>Editor de Pokemon</strong><div id="rz-pk"></div></div>' +
      '<div class="bloco"><strong>Cartao SD</strong>' +
      '<div id="rz-sd-status">Lendo SD...</div>' +
      '<label style="display:block;margin:6px 0">Arquivo: <input type="file" id="rz-sd-arquivo"></label>' +
      '<label style="display:block;margin:6px 0">Caminho na SD: <input type="text" id="rz-sd-caminho" placeholder="/assets/pasta/arquivo.png" style="width:70%"></label>' +
      '<button id="rz-sd-enviar">Enviar para a SD</button> ' +
      '<button id="rz-sd-recarregar">Recarregar assets</button>' +
      '<div id="rz-sd-msg"></div></div>';
    document.body.appendChild(sec);
    montarEditorPokemon($('rz-pk'));
  }

  document.addEventListener('DOMContentLoaded', () => {
    montarPainel();
    ligarTeleporte();
    ligarRotacao();
    ligarSD();
  });
})();
