import 'dart:math';
import 'package:flutter/material.dart';
import '../models/game_state.dart';
import '../services/emuiibo_ftp_service.dart';
import '../services/sysbotbase_client.dart';
import '../widgets/lumiose_map_painter.dart';

class HudHome extends StatefulWidget {
  final SysBotbaseClient client;
  const HudHome({super.key, required this.client});

  @override
  State<HudHome> createState() => _HudHomeState();
}

class _HudHomeState extends State<HudHome> {
  int _tabIdx = 0; // 0=Status, 1=Mapa, 2=Macros, 3=Rede, 4=Config
  int _macroCat = -1;
  bool _followCam = true;
  double _mapZoom = 2.0;
  bool _warpMode = false;
  int _mapLayerIdx = 0;

  static const _mapLayers = [
    (id: 'lumiose', name: 'Lumiose', grid: 4),
    (id: 'lysandre', name: 'Lysandre', grid: 3),
    (id: 'sewers_bleu', name: 'Bleu', grid: 2),
    (id: 'sewers_saison', name: 'Saison', grid: 2),
  ];

  late final TextEditingController _ipCtrl;
  late final TextEditingController _portCtrl;
  late final TextEditingController _amiiboSearchCtrl;
  late final TextEditingController _ftpPortCtrl;

  List<AmiiboCatalogItem> _amiiboCatalog = EmuiiboFtpService.quickCatalog;
  List<String> _sdAmiibos = [];
  String _amiiboMsg = '';
  bool _amiiboBusy = false;
  String _botwShrineFilter = 'all'; // 'all', 'done', 'missing', 'off'
  bool _botwShowKoroks = false;
  ({String name, double x, double z})? _botwNavTarget;

  static const _tabs = [
    (icon: Icons.favorite_rounded, label: 'Status'),
    (icon: Icons.map_rounded, label: 'Mapa'),
    (icon: Icons.gamepad_rounded, label: 'Macros'),
    (icon: Icons.nfc_rounded, label: 'Amiibo'),
    (icon: Icons.wifi_rounded, label: 'Rede'),
    (icon: Icons.tune_rounded, label: 'Config'),
  ];

  static const _zaCats = [
    (name: 'Batalha/Cura', ic: Icons.flash_on, slots: [1, 7, 6, 8, 15, 16]),
    (name: 'Movimento', ic: Icons.directions_run, slots: [10, 11, 14]),
    (name: 'Farm', ic: Icons.loop, slots: [3, 2, 9]),
    (name: 'Sistema', ic: Icons.settings, slots: [12, 13, 4, 5, 0]),
  ];

  static const _zaSlotNames = {
    0: 'Parear L+R',
    1: 'Combo Esq+Y',
    2: 'Turbo A',
    3: 'AFK Circulo',
    4: 'Mapa (+)',
    5: 'Cancelar 2xB',
    6: 'Auto-Catch',
    7: 'Esquiva',
    8: 'Spam Y',
    9: 'Spam B',
    10: 'Andar reto',
    11: 'Correr',
    12: 'Menu (X)',
    13: 'Home',
    14: 'Reset Cam',
    15: 'Curar HP',
    16: 'Curar Status',
  };

  @override
  void initState() {
    super.initState();
    _ipCtrl = TextEditingController(text: widget.client.switchIp);
    _portCtrl = TextEditingController(text: '${widget.client.switchPort}');
    _amiiboSearchCtrl = TextEditingController();
    _ftpPortCtrl = TextEditingController(text: '5000');
    widget.client.addListener(_onClientUpdate);
  }

  void _onClientUpdate() {
    if (!mounted) return;
    if (_ipCtrl.text != widget.client.switchIp && !FocusScope.of(context).hasFocus) {
      _ipCtrl.text = widget.client.switchIp;
    }
    setState(() {});
  }

  @override
  void dispose() {
    widget.client.removeListener(_onClientUpdate);
    _ipCtrl.dispose();
    _portCtrl.dispose();
    _amiiboSearchCtrl.dispose();
    _ftpPortCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final c = widget.client;
    final th = HudThemeConfig.byIndex(c.uiTheme);

    return Scaffold(
      backgroundColor: th.bg,
      body: SafeArea(
        child: Column(
          children: [
            _buildTopBar(c, th),
            if (!th.isSidebar) _buildTopTabs(th),
            Expanded(
              child: Row(
                children: [
                  if (th.isSidebar) _buildSidebarTabs(th),
                  Expanded(
                    child: Padding(
                      padding: const EdgeInsets.all(6),
                      child: IndexedStack(
                        index: _tabIdx,
                        children: [
                          _buildStatusTab(c, th),
                          _buildMapTab(c, th),
                          _buildMacrosTab(c, th),
                          _buildAmiiboTab(c, th),
                          _buildNetTab(c, th),
                          _buildConfigTab(c, th),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            ),
            if (!th.isSidebar) _buildFooter(c, th),
          ],
        ),
      ),
    );
  }

  Widget _buildTopBar(SysBotbaseClient c, HudThemeConfig th) {
    return Container(
      height: th.isSidebar ? 36 : 44,
      padding: const EdgeInsets.symmetric(horizontal: 10),
      decoration: BoxDecoration(
        color: th.topBg,
        border: Border(bottom: BorderSide(color: th.cardBorder.withValues(alpha: 0.4))),
      ),
      child: Row(
        children: [
          Text(
            'SWITCH HUD',
            style: TextStyle(
              color: th.titleColor,
              fontWeight: FontWeight.w800,
              fontSize: 15,
              letterSpacing: 0.6,
            ),
          ),
          const SizedBox(width: 8),
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: c.connected ? const Color(0xFF00E676) : const Color(0xFFFF5252),
            ),
          ),
          const SizedBox(width: 4),
          Text(
            c.connected ? 'Wi-Fi ON' : 'OFF',
            style: TextStyle(
              fontSize: 11,
              color: c.connected ? const Color(0xFF00E676) : const Color(0xFFFF8A80),
              fontWeight: FontWeight.w600,
            ),
          ),
          const Spacer(),
          DropdownButton<GameProfile>(
            value: c.profile,
            dropdownColor: th.cardBg,
            underline: const SizedBox(),
            style: const TextStyle(color: Colors.white, fontSize: 12),
            items: const [
              DropdownMenuItem(value: GameProfile.pokemonZa, child: Text('Pokemon Legends ZA')),
              DropdownMenuItem(value: GameProfile.zeldaBotw, child: Text('Zelda: Breath of the Wild')),
              DropdownMenuItem(value: GameProfile.pokemonFr, child: Text('Pokemon FireRed')),
            ],
            onChanged: (v) {
              if (v != null) c.setProfile(v);
            },
          ),
        ],
      ),
    );
  }

  Widget _buildTopTabs(HudThemeConfig th) {
    return Container(
      height: 38,
      color: th.barBg,
      child: Row(
        children: List.generate(_tabs.length, (i) {
          final active = _tabIdx == i;
          return Expanded(
            child: InkWell(
              onTap: () => setState(() => _tabIdx = i),
              child: Container(
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: active ? th.tabActiveBg : Colors.transparent,
                  border: Border(
                    bottom: BorderSide(
                      color: active ? th.accent : Colors.transparent,
                      width: 3,
                    ),
                  ),
                ),
                child: Text(
                  _tabs[i].label,
                  style: TextStyle(
                    fontSize: 12,
                    fontWeight: active ? FontWeight.w700 : FontWeight.w500,
                    color: active ? th.accent : const Color(0xFF99A2C8),
                  ),
                ),
              ),
            ),
          );
        }),
      ),
    );
  }

  Widget _buildSidebarTabs(HudThemeConfig th) {
    return Container(
      width: 60,
      decoration: BoxDecoration(
        color: th.barBg,
        border: Border(right: BorderSide(color: th.cardBorder.withValues(alpha: 0.5))),
      ),
      child: Column(
        children: List.generate(_tabs.length, (i) {
          final active = _tabIdx == i;
          return Expanded(
            child: InkWell(
              onTap: () => setState(() => _tabIdx = i),
              child: Container(
                width: double.infinity,
                decoration: BoxDecoration(
                  color: active ? th.tabActiveBg : Colors.transparent,
                  border: Border(
                    left: BorderSide(
                      color: active ? th.accent : Colors.transparent,
                      width: 3,
                    ),
                  ),
                ),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(_tabs[i].icon, size: 18, color: active ? th.accent : const Color(0xFF99A2C8)),
                    const SizedBox(height: 2),
                    Text(
                      _tabs[i].label,
                      style: TextStyle(
                        fontSize: 9,
                        fontWeight: active ? FontWeight.w700 : FontWeight.w500,
                        color: active ? th.accent : const Color(0xFF99A2C8),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          );
        }),
      ),
    );
  }

  Widget _buildStatusTab(SysBotbaseClient c, HudThemeConfig th) {
    if (c.profile == GameProfile.zeldaBotw) {
      return _buildBotwStatusTab(c, th);
    }
    final shinyHit = c.testShinyActive || c.shinyStashFound || c.party.any((p) => p.active && p.isShiny);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 2, vertical: 2),
          child: Row(
            children: [
              Expanded(
                child: Text(
                  c.shinyStashFound
                      ? 'SHINY STASH: ${c.shinyStashName}!'
                      : 'Zona: ${c.zoneName}',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    fontSize: 12,
                    fontWeight: shinyHit ? FontWeight.w700 : FontWeight.w600,
                    color: shinyHit ? const Color(0xFFFFD700) : th.titleColor,
                  ),
                ),
              ),
              Text(
                c.linkStatus,
                style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA)),
              ),
            ],
          ),
        ),
        const SizedBox(height: 4),
        Expanded(
          child: LayoutBuilder(
            builder: (context, constraints) {
              final cols = constraints.maxWidth > 620 ? 3 : 2;
              final rows = (6 / cols).ceil();
              final cardH = ((constraints.maxHeight - (rows - 1) * 6) / rows).clamp(64.0, 140.0);
              return GridView.builder(
                physics: const NeverScrollableScrollPhysics(),
                gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                  crossAxisCount: cols,
                  mainAxisSpacing: 6,
                  crossAxisSpacing: 6,
                  mainAxisExtent: cardH,
                ),
                itemCount: 6,
                itemBuilder: (_, i) => _buildPartyCard(i, c.party[i], th),
              );
            },
          ),
        ),
      ],
    );
  }

  Widget _buildPartyCard(int idx, PokemonSlot p, HudThemeConfig th) {
    final pct = (p.active && p.maxHp > 0) ? (p.currentHp / p.maxHp).clamp(0.0, 1.0) : 0.0;
    final hpColor = pct > 0.5
        ? const Color(0xFF00E676)
        : pct > 0.2
            ? const Color(0xFFFFD600)
            : const Color(0xFFFF1744);
    final borderColor = p.isShiny ? const Color(0xFFFFD700) : th.cardBorder;
    final spriteUrl = p.active && p.species > 0
        ? (p.isShiny
            ? 'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/shiny/${p.species}.png'
            : 'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/${p.species}.png')
        : null;

    return InkWell(
      onTap: p.active ? () => _openPokemonModal(idx, p, th) : null,
      borderRadius: BorderRadius.circular(th.cardRadius),
      child: Container(
        decoration: BoxDecoration(
          color: th.cardBg,
          borderRadius: BorderRadius.circular(th.cardRadius),
          border: Border.all(color: borderColor, width: p.isShiny ? 1.6 : 1.0),
        ),
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
        child: Row(
          children: [
            Container(
              width: 44,
              height: 44,
              decoration: BoxDecoration(
                color: const Color(0xFF101324),
                borderRadius: BorderRadius.circular(6),
              ),
              child: spriteUrl != null
                  ? Image.network(
                      spriteUrl,
                      fit: BoxFit.contain,
                      errorBuilder: (_, _, _) => Center(
                        child: Text('#${p.species}', style: const TextStyle(fontSize: 10, color: Color(0xFF7780A8))),
                      ),
                    )
                  : Center(
                      child: Text(
                        '${idx + 1}',
                        style: const TextStyle(fontSize: 12, color: Color(0xFF555E80)),
                      ),
                    ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          p.active ? (p.name.isNotEmpty ? p.name : '#${p.species}') : 'Slot ${idx + 1} vazio',
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: TextStyle(
                            fontSize: 12,
                            fontWeight: FontWeight.w700,
                            color: p.active ? Colors.white : const Color(0xFF667094),
                          ),
                        ),
                      ),
                      if (p.active)
                        Text(
                          'Lv${p.level}',
                          style: const TextStyle(fontSize: 10, color: Color(0xFF99A2C8)),
                        ),
                    ],
                  ),
                  ClipRRect(
                    borderRadius: BorderRadius.circular(3),
                    child: LinearProgressIndicator(
                      value: pct,
                      minHeight: 5,
                      backgroundColor: const Color(0xFF101324),
                      valueColor: AlwaysStoppedAnimation<Color>(hpColor),
                    ),
                  ),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(
                        p.active ? '${p.currentHp}/${p.maxHp}' : '-',
                        style: const TextStyle(fontSize: 10, color: Color(0xFFAAAACC)),
                      ),
                      Text(
                        [
                          if (p.isShiny) '★',
                          if (p.isAlpha) 'α',
                          if (p.isMaxIv) '6IV',
                          if (p.status.isNotEmpty) p.status,
                        ].join(' '),
                        style: TextStyle(
                          fontSize: 10,
                          fontWeight: FontWeight.w700,
                          color: p.isShiny ? const Color(0xFFFFD700) : th.accent,
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _openPokemonModal(int slot, PokemonSlot p, HudThemeConfig th) {
    showModalBottomSheet(
      context: context,
      backgroundColor: th.cardBg,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(14)),
      ),
      builder: (ctx) {
        final actions = [
          ('Curar HP', PokeEditAction.healHp, Colors.teal.shade800),
          ('Curar Status', PokeEditAction.healStatus, Colors.teal.shade800),
          ('PP Max', PokeEditAction.restorePp, Colors.teal.shade800),
          ('Tudo 100%', PokeEditAction.fullRestore, Colors.green.shade800),
          (p.isShiny ? 'Shiny: ON' : 'Shiny: OFF', PokeEditAction.toggleShiny, Colors.amber.shade900),
          ('IVs 31', PokeEditAction.maxIvs, Colors.amber.shade900),
          (p.isAlpha ? 'Alpha: ON' : 'Alpha: OFF', PokeEditAction.toggleAlpha, Colors.indigo.shade800),
          ('+1 Lv', PokeEditAction.levelPlus1, Colors.blueGrey.shade800),
          ('+10 Lv', PokeEditAction.levelPlus10, Colors.blueGrey.shade800),
          ('Lv 100', PokeEditAction.level100, Colors.blueGrey.shade800),
          ('EV Atk/Vel', PokeEditAction.evAtkSpe, Colors.deepPurple.shade800),
          ('EV SpA/Vel', PokeEditAction.evSpaSpe, Colors.deepPurple.shade800),
          ('EV Tank', PokeEditAction.evHpTank, Colors.deepPurple.shade800),
          ('Zerar EV', PokeEditAction.evReset, Colors.red.shade900),
        ];
        return Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  Text(
                    '${p.name} (Slot ${slot + 1} • Lv${p.level})',
                    style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 14),
                  ),
                  const Spacer(),
                  IconButton(
                    icon: const Icon(Icons.close, color: Colors.white70),
                    onPressed: () => Navigator.pop(ctx),
                  ),
                ],
              ),
              const SizedBox(height: 6),
              Wrap(
                spacing: 6,
                runSpacing: 6,
                children: actions.map((a) {
                  return SizedBox(
                    width: (MediaQuery.of(ctx).size.width - 42) / 3,
                    height: 38,
                    child: ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: a.$3,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(horizontal: 4),
                        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                      ),
                      onPressed: () {
                        widget.client.editPokemon(slot, a.$2);
                        Navigator.pop(ctx);
                      },
                      child: Text(a.$1, style: const TextStyle(fontSize: 11), textAlign: TextAlign.center),
                    ),
                  );
                }).toList(),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildBotwStatusTab(SysBotbaseClient c, HudThemeConfig th) {
    const beasts = ['Vah Ruta', 'Vah Rudania', 'Vah Medoh', 'Vah Naboris'];
    final maxHearts = (c.botwHeartsMax / 4).ceil().clamp(1, 30);
    final fullHearts = (c.botwHeartsCur ~/ 4).clamp(0, maxHearts);
    final hasPartial = (c.botwHeartsCur % 4) > 0;
    final staminaWheel = (c.botwStaminaPct / 100.0).clamp(0.0, 3.0);

    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  'Hyrule HUD (BotW)',
                  style: TextStyle(color: th.titleColor, fontWeight: FontWeight.w700, fontSize: 13),
                ),
              ),
              Text(
                c.buildId.isNotEmpty
                    ? 'Build: ${c.buildId.length > 16 ? c.buildId.substring(0, 16) : c.buildId}'
                    : (c.titleId.isNotEmpty ? 'ID: ${c.titleId}' : c.linkStatus),
                style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA)),
              ),
            ],
          ),
          const SizedBox(height: 6),
          // Visual Hearts + Stamina + Rupees bar
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Row(
              children: [
                // Hearts visual
                Expanded(
                  flex: 3,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Coracoes (${(c.botwHeartsCur / 4).toStringAsFixed(1)}/$maxHearts)',
                        style: const TextStyle(fontSize: 11, color: Color(0xFFFF6E88), fontWeight: FontWeight.w700),
                      ),
                      const SizedBox(height: 4),
                      Wrap(
                        spacing: 2,
                        runSpacing: 2,
                        children: List.generate(maxHearts, (i) {
                          if (i < fullHearts) {
                            return const Icon(Icons.favorite, size: 16, color: Color(0xFFFF1744));
                          } else if (i == fullHearts && hasPartial) {
                            return const Icon(Icons.favorite, size: 16, color: Color(0x99FF5252));
                          }
                          return const Icon(Icons.favorite_border, size: 16, color: Color(0xFF4A2230));
                        }),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                // Stamina Ring
                Column(
                  children: [
                    SizedBox(
                      width: 38,
                      height: 38,
                      child: Stack(
                        alignment: Alignment.center,
                        children: [
                          CircularProgressIndicator(
                            value: (staminaWheel / 3.0).clamp(0.05, 1.0),
                            strokeWidth: 5,
                            backgroundColor: const Color(0xFF10241A),
                            valueColor: const AlwaysStoppedAnimation<Color>(Color(0xFF00E676)),
                          ),
                          Text(
                            '${c.botwStaminaPct}%',
                            style: const TextStyle(fontSize: 9, fontWeight: FontWeight.w700, color: Color(0xFF00E676)),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 2),
                    const Text('Stamina', style: TextStyle(fontSize: 9, color: Color(0xFF88AA99))),
                  ],
                ),
                const SizedBox(width: 10),
                // Rupees badge
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                  decoration: BoxDecoration(
                    color: const Color(0xFF101324),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: const Color(0xFF00E676).withValues(alpha: 0.4)),
                  ),
                  child: Column(
                    children: [
                      const Icon(Icons.diamond_rounded, size: 18, color: Color(0xFF00E676)),
                      Text(
                        '${c.botwRupees}',
                        style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: Colors.white),
                      ),
                      const Text('Rupees', style: TextStyle(fontSize: 9, color: Color(0xFF8888AA))),
                    ],
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 6),
          Wrap(
            spacing: 6,
            runSpacing: 6,
            children: [
              _miniBtn('Curar Full', const Color(0xFF880E4F), () => c.pokeBotwHearts(c.botwHeartsMax)),
              _miniBtn('30 Coracoes', const Color(0xFFB71C1C), () => c.pokeBotwHearts(120, setMax: true)),
              _miniBtn('Stamina 3x', const Color(0xFF00695C), () => c.pokeBotwStaminaFull(threeWheels: true)),
              _miniBtn('+10k Rupees', const Color(0xFF1B5E20), () => c.pokeBotwRupees(c.botwRupees + 10000)),
              _miniBtn('999k Rupees', const Color(0xFF2E7D32), () => c.pokeBotwRupees(999999)),
            ],
          ),
          const SizedBox(height: 6),
          Row(
            children: [
              const Text('Versao:', style: TextStyle(fontSize: 11, color: Color(0xFF99A2C8))),
              const SizedBox(width: 6),
              ...['auto', '1.0.0', '1.6.0'].map((v) {
                final sel = c.botwSelectedVersion == v;
                return Padding(
                  padding: const EdgeInsets.only(right: 6),
                  child: ChoiceChip(
                    label: Text(v == 'auto' ? 'Auto (BuildID)' : v, style: const TextStyle(fontSize: 10)),
                    selected: sel,
                    selectedColor: const Color(0xFF0F3460),
                    backgroundColor: const Color(0xFF101324),
                    onSelected: (_) => c.setBotwVersion(v),
                  ),
                );
              }),
            ],
          ),
          const SizedBox(height: 8),
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Text('Progresso em Hyrule (Auto)',
                        style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 12)),
                    const Spacer(),
                    _miniBtn(
                      c.botwProgressSyncing ? 'Lendo...' : 'Sincronizar',
                      const Color(0xFF0F3460),
                      c.botwProgressSyncing ? () {} : () => c.syncBotwProgress(),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: Container(
                        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                        decoration: BoxDecoration(
                          color: const Color(0xFF101324),
                          borderRadius: BorderRadius.circular(8),
                          border: Border.all(color: th.cardBorder),
                        ),
                        child: Text(
                          'Shrines: ${c.botwShrines}/${c.botwShrineList.isNotEmpty ? c.botwShrineList.length : 120}',
                          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: Color(0xFF00E5FF)),
                        ),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Container(
                        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                        decoration: BoxDecoration(
                          color: const Color(0xFF101324),
                          borderRadius: BorderRadius.circular(8),
                          border: Border.all(color: th.cardBorder),
                        ),
                        child: Text(
                          'Koroks no mapa: ${c.botwKorokList.length}',
                          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: Color(0xFFFFD166)),
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 6,
                  runSpacing: 6,
                  children: [
                    ...List.generate(4, (i) {
                      final on = (c.botwBeasts & (1 << i)) != 0;
                      return _statusBadge(beasts[i], on, const Color(0xFF1B5E20));
                    }),
                    _statusBadge('Master Sword', c.botwMasterSword, const Color(0xFF0F3460)),
                    _statusBadge('Hylian Shield', c.botwHylianShield, const Color(0xFF0F3460)),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text('Acoes Rapidas (Wi-Fi sys-botbase)',
                    style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 12)),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(child: _miniBtn('Runa/Amiibo (L)', const Color(0xFF4527A0), () => c.triggerMacro(40))),
                    const SizedBox(width: 4),
                    Expanded(child: _miniBtn('Quick Save', const Color(0xFF1B5E20), () => c.triggerMacro(44))),
                    const SizedBox(width: 4),
                    Expanded(child: _miniBtn('Mapa (-)', const Color(0xFF006064), () => c.triggerMacro(42))),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildMapTab(SysBotbaseClient c, HudThemeConfig th) {
    if (c.profile == GameProfile.zeldaBotw) {
      return _buildBotwMapTab(c, th);
    }
    final distC20 = sqrt(pow(c.posX - (-830.0), 2) + pow(c.posY - (-500.0), 2)).round();
    final layer = _mapLayers[_mapLayerIdx];
    final pn = LumioseMapPainter.worldToNorm(c.posX, c.posY);

    return LayoutBuilder(
      builder: (context, box) {
        final wide = box.maxWidth > 520;
        final mapWidget = Expanded(
          flex: 3,
          child: LayoutBuilder(
            builder: (_, mbox) {
              final w = mbox.maxWidth;
              final h = mbox.maxHeight;
              final tx = _followCam ? (w / 2 - pn.dx * w * _mapZoom) : (w / 2 * (1 - _mapZoom));
              final ty = _followCam ? (h / 2 - pn.dy * h * _mapZoom) : (h / 2 * (1 - _mapZoom));
              final n = layer.grid;

              return GestureDetector(
                onTapUp: (d) {
                  if (!_warpMode) return;
                  final local = d.localPosition;
                  double nx = local.dx / w;
                  double ny = local.dy / h;
                  if (_followCam) {
                    nx = pn.dx + (nx - 0.5) / _mapZoom;
                    ny = pn.dy + (ny - 0.5) / _mapZoom;
                  } else if (_mapZoom != 1.0) {
                    nx = 0.5 + (nx - 0.5) / _mapZoom;
                    ny = 0.5 + (ny - 0.5) / _mapZoom;
                  }
                  final target = LumioseMapPainter.normToWorld(nx.clamp(0.0, 1.0), ny.clamp(0.0, 1.0));
                  c.teleportPlayer(target.dx, c.posAlt, target.dy);
                },
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(th.cardRadius),
                  child: Container(
                    decoration: BoxDecoration(
                      color: const Color(0xFF0C1020),
                      border: Border.all(color: _warpMode ? const Color(0xFFFFD700) : th.cardBorder, width: 1.5),
                    ),
                    child: Stack(
                      fit: StackFit.expand,
                      children: [
                        ClipRect(
                          child: Transform(
                            transform: Matrix4.translationValues(tx, ty, 0)..scaleByDouble(_mapZoom, _mapZoom, 1.0, 1.0),
                            child: SizedBox(
                              width: w,
                              height: h,
                              child: Column(
                                children: List.generate(n, (row) {
                                  return Expanded(
                                    child: Row(
                                      children: List.generate(n, (col) {
                                        return Expanded(
                                          child: Image.asset(
                                            'assets/m/pkmn_za/${layer.id}/${col}_$row.jpg',
                                            fit: BoxFit.fill,
                                            gaplessPlayback: true,
                                          ),
                                        );
                                      }),
                                    ),
                                  );
                                }),
                              ),
                            ),
                          ),
                        ),
                        CustomPaint(
                          size: Size.infinite,
                          painter: LumioseMapPainter(
                            posX: c.posX,
                            posY: c.posY,
                            trail: c.trail,
                            shinyAlert: c.testShinyActive || c.shinyStashFound,
                            accent: th.accent,
                            zoom: _mapZoom,
                            followCam: _followCam,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              );
            },
          ),
        );

        final sidePanel = Expanded(
          flex: 2,
          child: Container(
            padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(c.zoneName, style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 12)),
                    const SizedBox(height: 4),
                    Container(
                      width: double.infinity,
                      padding: const EdgeInsets.all(6),
                      decoration: BoxDecoration(
                        color: const Color(0xFF101324),
                        borderRadius: BorderRadius.circular(6),
                      ),
                      child: Text(
                        'X: ${c.posX.toStringAsFixed(1)}\nZ: ${c.posY.toStringAsFixed(1)}\nAlt: ${c.posAlt.toStringAsFixed(1)}',
                        style: const TextStyle(fontFamily: 'monospace', fontSize: 11, color: Colors.white),
                      ),
                    ),
                    const SizedBox(height: 4),
                    Text(
                      'Dist C20: ${distC20}m | Zoom: ${_mapZoom.toInt()}x',
                      style: const TextStyle(fontSize: 10, color: Color(0xFF99A2C8)),
                    ),
                  ],
                ),
                Column(
                  children: [
                    Row(
                      children: [
                        Expanded(
                          child: _miniBtn(
                            'Mapa: ${layer.name}',
                            const Color(0xFF23294E),
                            () => setState(() => _mapLayerIdx = (_mapLayerIdx + 1) % _mapLayers.length),
                          ),
                        ),
                        const SizedBox(width: 4),
                        Expanded(
                          child: _miniBtn(
                            _followCam ? 'Cam: Radar' : 'Cam: Fixo',
                            const Color(0xFF0F3460),
                            () => setState(() => _followCam = !_followCam),
                          ),
                        ),
                        const SizedBox(width: 4),
                        Expanded(
                          child: _miniBtn(
                            'Zoom ${_mapZoom.toInt()}x',
                            const Color(0xFF23294E),
                            () => setState(() => _mapZoom = _mapZoom == 1.0 ? 2.0 : _mapZoom == 2.0 ? 4.0 : 1.0),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Expanded(
                          child: _miniBtn(
                            'Pontos',
                            const Color(0xFF006064),
                            () => _openWarpsModal(c, th),
                          ),
                        ),
                        const SizedBox(width: 4),
                        Expanded(
                          child: _miniBtn(
                            _warpMode ? 'Warp: ON' : 'Warp: OFF',
                            _warpMode ? const Color(0xFF1B5E20) : const Color(0xFF23294E),
                            () => setState(() => _warpMode = !_warpMode),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ],
            ),
          ),
        );

        return wide
            ? Row(children: [mapWidget, const SizedBox(width: 6), sidePanel])
            : Column(children: [mapWidget, const SizedBox(height: 6), sidePanel]);
      },
    );
  }

  Widget _miniBtn(String label, Color bg, VoidCallback onTap) {
    return SizedBox(
      height: 34,
      child: ElevatedButton(
        style: ElevatedButton.styleFrom(
          backgroundColor: bg,
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(horizontal: 4),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(6)),
        ),
        onPressed: onTap,
        child: Text(label, style: const TextStyle(fontSize: 11), overflow: TextOverflow.ellipsis),
      ),
    );
  }

  void _openWarpsModal(SysBotbaseClient c, HudThemeConfig th) {
    showModalBottomSheet(
      context: context,
      backgroundColor: th.cardBg,
      isScrollControlled: true,
      builder: (ctx) => Padding(
        padding: const EdgeInsets.all(12),
        child: SizedBox(
          height: 340,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Text('Teleportes Rapidos & Favoritos',
                  style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 14)),
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: _miniBtn('Centro / Prisma', const Color(0xFF0F3460), () {
                      c.teleportPlayer(-830.0, c.posAlt, -500.0);
                      Navigator.pop(ctx);
                    }),
                  ),
                  const SizedBox(width: 6),
                  Expanded(
                    child: _miniBtn('Salvar Fav 1 Aqui', const Color(0xFF6B4E00), () {
                      c.saveFavWarp(0, c.zoneName.split(' ').take(2).join(' '));
                      Navigator.pop(ctx);
                    }),
                  ),
                ],
              ),
              const SizedBox(height: 6),
              Row(
                children: List.generate(3, (i) {
                  final f = c.favWarps[i];
                  return Expanded(
                    child: Padding(
                      padding: EdgeInsets.only(right: i < 2 ? 4 : 0),
                      child: _miniBtn(
                        f.valid ? f.name : 'Fav ${i + 1}',
                        const Color(0xFF23294E),
                        () {
                          if (f.valid) {
                            c.teleportPlayer(f.x, f.y, f.z);
                            Navigator.pop(ctx);
                          } else {
                            c.saveFavWarp(i, 'Fav ${i + 1}');
                            Navigator.pop(ctx);
                          }
                        },
                      ),
                    ),
                  );
                }),
              ),
              const SizedBox(height: 8),
              Expanded(
                child: GridView.builder(
                  gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                    crossAxisCount: 4,
                    mainAxisSpacing: 5,
                    crossAxisSpacing: 5,
                    childAspectRatio: 2.1,
                  ),
                  itemCount: SysBotbaseClient.wildZones.length,
                  itemBuilder: (_, i) {
                    final z = SysBotbaseClient.wildZones[i];
                    return _miniBtn('Campo ${z.id}', const Color(0xFF18223D), () {
                      final w = LumioseMapPainter.normToWorld(z.nx, z.ny);
                      c.teleportPlayer(w.dx, c.posAlt, w.dy);
                      Navigator.pop(ctx);
                    });
                  },
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildMacrosTab(SysBotbaseClient c, HudThemeConfig th) {
    if (c.profile == GameProfile.zeldaBotw) {
      const botwMacros = [
        (40, 'Runa / Amiibo (L)', Icons.nfc_rounded),
        (47, 'Amiibo Loop (1.5s)', Icons.loop),
        (45, 'Whistle Sprint', Icons.directions_run),
        (46, 'Spam A (Loot/Cook)', Icons.touch_app),
        (44, 'Quick Save', Icons.save_rounded),
        (41, 'Inventario (+)', Icons.backpack_rounded),
        (42, 'Mapa (-)', Icons.map_rounded),
        (43, 'Scope (R3)', Icons.center_focus_strong),
      ];
      return Column(
        children: [
          Row(
            children: [
              Expanded(
                child: Text('Macros Zelda: Breath of the Wild',
                    style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 13)),
              ),
              _miniBtn('■ PARAR', const Color(0xFF7B112C), c.stopAllMacros),
            ],
          ),
          const SizedBox(height: 8),
          Expanded(
            child: GridView.builder(
              gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                crossAxisCount: 2,
                mainAxisSpacing: 6,
                crossAxisSpacing: 6,
                childAspectRatio: 2.3,
              ),
              itemCount: botwMacros.length,
              itemBuilder: (_, i) {
                final m = botwMacros[i];
                final on = c.activeMacroSlot == m.$1;
                return InkWell(
                  onTap: () => c.triggerMacro(m.$1),
                  borderRadius: BorderRadius.circular(th.cardRadius),
                  child: Container(
                    decoration: BoxDecoration(
                      color: on ? const Color(0xFF133E35) : th.cardBg,
                      borderRadius: BorderRadius.circular(th.cardRadius),
                      border: Border.all(color: on ? const Color(0xFF00E676) : th.cardBorder, width: 1.5),
                    ),
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(m.$3, color: th.accent, size: 20),
                        const SizedBox(height: 3),
                        Text(m.$2, style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12)),
                      ],
                    ),
                  ),
                );
              },
            ),
          ),
        ],
      );
    }
    if (_macroCat < 0) {
      return GridView.count(
        crossAxisCount: 2,
        mainAxisSpacing: 8,
        crossAxisSpacing: 8,
        childAspectRatio: 2.1,
        children: [
          ...List.generate(_zaCats.length, (k) {
            final cat = _zaCats[k];
            final active = cat.slots.contains(c.activeMacroSlot);
            return InkWell(
              onTap: () => setState(() => _macroCat = k),
              borderRadius: BorderRadius.circular(th.cardRadius),
              child: Container(
                decoration: BoxDecoration(
                  color: active ? const Color(0xFF133E35) : th.cardBg,
                  borderRadius: BorderRadius.circular(th.cardRadius),
                  border: Border.all(color: active ? const Color(0xFF00E676) : th.cardBorder, width: 1.5),
                ),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(cat.ic, color: th.accent, size: 22),
                    const SizedBox(height: 4),
                    Text(cat.name, style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13)),
                    Text('${cat.slots.length} macros', style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA))),
                  ],
                ),
              ),
            );
          }),
          InkWell(
            onTap: c.stopAllMacros,
            borderRadius: BorderRadius.circular(th.cardRadius),
            child: Container(
              decoration: BoxDecoration(
                color: const Color(0xFF3A1424),
                borderRadius: BorderRadius.circular(th.cardRadius),
                border: Border.all(color: const Color(0xFF8C2B3F), width: 1.5),
              ),
              child: const Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(Icons.stop_circle_rounded, color: Color(0xFFFF6E88), size: 24),
                  SizedBox(height: 4),
                  Text('PARAR TUDO', style: TextStyle(fontWeight: FontWeight.w700, fontSize: 13)),
                ],
              ),
            ),
          ),
        ],
      );
    }

    final cat = _zaCats[_macroCat];
    return Column(
      children: [
        Row(
          children: [
            _miniBtn('‹ Voltar', const Color(0xFF1E2749), () => setState(() => _macroCat = -1)),
            const SizedBox(width: 8),
            Expanded(
              child: Text(cat.name,
                  textAlign: TextAlign.center,
                  style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 14)),
            ),
            const SizedBox(width: 8),
            _miniBtn('■ PARAR', const Color(0xFF7B112C), c.stopAllMacros),
          ],
        ),
        const SizedBox(height: 8),
        Expanded(
          child: GridView.builder(
            gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
              crossAxisCount: 2,
              mainAxisSpacing: 6,
              crossAxisSpacing: 6,
              childAspectRatio: 2.3,
            ),
            itemCount: cat.slots.length,
            itemBuilder: (_, idx) {
              final slot = cat.slots[idx];
              final on = c.activeMacroSlot == slot;
              return InkWell(
                onTap: () => c.triggerMacro(slot),
                borderRadius: BorderRadius.circular(th.cardRadius),
                child: Container(
                  decoration: BoxDecoration(
                    color: on ? const Color(0xFF133E35) : th.cardBg,
                    borderRadius: BorderRadius.circular(th.cardRadius),
                    border: Border.all(color: on ? const Color(0xFF00E676) : th.cardBorder, width: 1.5),
                  ),
                  alignment: Alignment.center,
                  child: Text(
                    _zaSlotNames[slot] ?? 'Macro $slot',
                    style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13),
                  ),
                ),
              );
            },
          ),
        ),
      ],
    );
  }

  Widget _buildAmiiboTab(SysBotbaseClient c, HudThemeConfig th) {
    final ftpPort = int.tryParse(_ftpPortCtrl.text.trim()) ?? 5000;
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text('Controle emuiibo (Tesla via sys-botbase)',
                    style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 12)),
                const SizedBox(height: 6),
                Row(
                  children: [
                    Expanded(
                      flex: 2,
                      child: _miniBtn('Tesla (L+▼+R3)', const Color(0xFF4527A0), c.openTeslaOverlay),
                    ),
                    const SizedBox(width: 4),
                    Expanded(child: _miniBtn('▲', const Color(0xFF1E2749), () => c.sendButtonClick('DUP'))),
                    const SizedBox(width: 4),
                    Expanded(child: _miniBtn('▼', const Color(0xFF1E2749), () => c.sendButtonClick('DDOWN'))),
                    const SizedBox(width: 4),
                    Expanded(child: _miniBtn('A', const Color(0xFF1B5E20), () => c.sendButtonClick('A'))),
                    const SizedBox(width: 4),
                    Expanded(child: _miniBtn('B', const Color(0xFF7B112C), () => c.sendButtonClick('B'))),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text('Enviar Amiibo p/ SD (sys-ftpd)',
                          style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 12)),
                    ),
                    SizedBox(
                      width: 68,
                      height: 28,
                      child: TextField(
                        controller: _ftpPortCtrl,
                        keyboardType: TextInputType.number,
                        style: const TextStyle(color: Colors.white, fontSize: 11),
                        decoration: const InputDecoration(
                          contentPadding: EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                          labelText: 'FTP',
                          labelStyle: TextStyle(fontSize: 10, color: Color(0xFF99A2C8)),
                          filled: true,
                          fillColor: Color(0xFF101324),
                          border: OutlineInputBorder(),
                        ),
                      ),
                    ),
                    const SizedBox(width: 6),
                    _miniBtn('Listar SD', const Color(0xFF006064), _amiiboBusy ? () {} : () async {
                      setState(() {
                        _amiiboBusy = true;
                        _amiiboMsg = 'Lendo /emuiibo/amiibo...';
                      });
                      try {
                        final list = await EmuiiboFtpService.listAmiibos(c.switchIp, port: ftpPort);
                        setState(() {
                          _sdAmiibos = list;
                          _amiiboMsg = '${list.length} amiibo(s) no SD';
                        });
                      } catch (e) {
                        setState(() => _amiiboMsg = 'Erro FTP (${c.switchIp}:$ftpPort): $e');
                      } finally {
                        setState(() => _amiiboBusy = false);
                      }
                    }),
                  ],
                ),
                const SizedBox(height: 6),
                Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _amiiboSearchCtrl,
                        style: const TextStyle(color: Colors.white, fontSize: 12),
                        decoration: const InputDecoration(
                          isDense: true,
                          hintText: 'Buscar Amiibo (ex: Link, Zelda, Mario...)',
                          hintStyle: TextStyle(color: Color(0xFF667094), fontSize: 11),
                          filled: true,
                          fillColor: Color(0xFF101324),
                          border: OutlineInputBorder(),
                        ),
                      ),
                    ),
                    const SizedBox(width: 6),
                    _miniBtn('Buscar', const Color(0xFF0F3460), () async {
                      setState(() {
                        _amiiboBusy = true;
                        _amiiboMsg = 'Buscando na AmiiboAPI...';
                      });
                      final q = _amiiboSearchCtrl.text.trim();
                      final res = q.isEmpty
                          ? EmuiiboFtpService.catalogForGame(c.profile)
                          : await EmuiiboFtpService.searchApi(q);
                      setState(() {
                        _amiiboCatalog = res;
                        _amiiboBusy = false;
                        _amiiboMsg = '${res.length} encontrado(s)';
                      });
                    }),
                    if (c.profile == GameProfile.zeldaBotw) ...[
                      const SizedBox(width: 6),
                      _miniBtn('Enviar 24', const Color(0xFF1B5E20), () async {
                        if (_amiiboBusy) return;
                        setState(() {
                          _amiiboBusy = true;
                          _amiiboMsg = 'Enviando 24 Amiibos BOTW...';
                        });
                        int ok = 0;
                        try {
                          for (final item in EmuiiboFtpService.botwCatalog) {
                            await EmuiiboFtpService.uploadVirtualAmiibo(c.switchIp, item, port: ftpPort);
                            ok++;
                            if (mounted) setState(() => _amiiboMsg = 'Enviados $ok/24...');
                          }
                          final list = await EmuiiboFtpService.listAmiibos(c.switchIp, port: ftpPort);
                          setState(() {
                            _sdAmiibos = list;
                            _amiiboBusy = false;
                            _amiiboMsg = '$ok Amiibos BOTW enviados para /emuiibo/amiibo!';
                          });
                        } catch (e) {
                          setState(() {
                            _amiiboBusy = false;
                            _amiiboMsg = 'Erro FTP ($ok/24): $e';
                          });
                        }
                      }),
                    ],
                  ],
                ),
                if (_amiiboMsg.isNotEmpty) ...[
                  const SizedBox(height: 6),
                  Text(_amiiboMsg, style: TextStyle(color: th.accent, fontSize: 11)),
                ],
                const SizedBox(height: 6),
                ...(_amiiboSearchCtrl.text.trim().isEmpty
                        ? EmuiiboFtpService.catalogForGame(c.profile)
                        : _amiiboCatalog)
                    .take(24)
                    .map((item) => Container(
                      margin: const EdgeInsets.only(bottom: 4),
                      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
                      decoration: BoxDecoration(
                        color: const Color(0xFF101324),
                        borderRadius: BorderRadius.circular(6),
                        border: Border.all(color: th.cardBorder.withValues(alpha: 0.4)),
                      ),
                      child: Row(
                        children: [
                          if (item.image.isNotEmpty)
                            Image.network(item.image, width: 28, height: 28, errorBuilder: (_, _, _) => const Icon(Icons.nfc, size: 22))
                          else
                            const Icon(Icons.nfc, size: 22),
                          const SizedBox(width: 8),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(item.name, style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600)),
                                Text(item.gameSeries, style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA))),
                              ],
                            ),
                          ),
                          _miniBtn('Enviar', const Color(0xFF1B5E20), () async {
                            setState(() {
                              _amiiboBusy = true;
                              _amiiboMsg = 'Enviando ${item.name}...';
                            });
                            try {
                              await EmuiiboFtpService.uploadVirtualAmiibo(c.switchIp, item, port: ftpPort);
                              final list = await EmuiiboFtpService.listAmiibos(c.switchIp, port: ftpPort);
                              setState(() {
                                _sdAmiibos = list;
                                _amiiboMsg = '${item.name} enviado p/ /emuiibo/amiibo!';
                              });
                            } catch (e) {
                              setState(() => _amiiboMsg = 'Erro FTP: $e');
                            } finally {
                              setState(() => _amiiboBusy = false);
                            }
                          }),
                        ],
                      ),
                    )),
                if (_sdAmiibos.isNotEmpty) ...[
                  const SizedBox(height: 6),
                  Text('No SD (/emuiibo/amiibo/):', style: TextStyle(color: th.accent, fontSize: 11, fontWeight: FontWeight.w700)),
                  const SizedBox(height: 4),
                  Wrap(
                    spacing: 6,
                    runSpacing: 6,
                    children: _sdAmiibos.map((folder) => Chip(
                          backgroundColor: const Color(0xFF18223D),
                          label: Text(folder, style: const TextStyle(fontSize: 11, color: Colors.white)),
                          deleteIcon: const Icon(Icons.delete_outline, size: 16, color: Color(0xFFFF6E88)),
                          onDeleted: () async {
                            try {
                              await EmuiiboFtpService.deleteAmiibo(c.switchIp, folder, port: ftpPort);
                              final list = await EmuiiboFtpService.listAmiibos(c.switchIp, port: ftpPort);
                              setState(() {
                                _sdAmiibos = list;
                                _amiiboMsg = '$folder removido do SD';
                              });
                            } catch (e) {
                              setState(() => _amiiboMsg = 'Erro ao remover: $e');
                            }
                          },
                        )).toList(),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildNetTab(SysBotbaseClient c, HudThemeConfig th) {
    return SingleChildScrollView(
      child: Container(
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: th.cardBg,
          borderRadius: BorderRadius.circular(th.cardRadius),
          border: Border.all(color: th.cardBorder),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text('Conexao Direta sys-botbase (Wi-Fi TCP)',
                style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 13)),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  flex: 3,
                  child: TextField(
                    controller: _ipCtrl,
                    keyboardType: TextInputType.number,
                    style: const TextStyle(color: Colors.white, fontSize: 13),
                    decoration: const InputDecoration(
                      labelText: 'IP do Switch',
                      labelStyle: TextStyle(color: Color(0xFF99A2C8)),
                      filled: true,
                      fillColor: Color(0xFF101324),
                      border: OutlineInputBorder(),
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  flex: 1,
                  child: TextField(
                    controller: _portCtrl,
                    keyboardType: TextInputType.number,
                    style: const TextStyle(color: Colors.white, fontSize: 13),
                    decoration: const InputDecoration(
                      labelText: 'Porta',
                      labelStyle: TextStyle(color: Color(0xFF99A2C8)),
                      filled: true,
                      fillColor: Color(0xFF101324),
                      border: OutlineInputBorder(),
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    icon: const Icon(Icons.link, size: 16),
                    label: const Text('Conectar'),
                    style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF0F3460), foregroundColor: Colors.white),
                    onPressed: () {
                      final p = int.tryParse(_portCtrl.text.trim()) ?? 6000;
                      c.updateTarget(_ipCtrl.text.trim(), p);
                    },
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: ElevatedButton.icon(
                    icon: const Icon(Icons.radar, size: 16),
                    label: Text(c.scanning ? '${c.scanProgress}%' : 'Detectar Switch'),
                    style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF006064), foregroundColor: Colors.white),
                    onPressed: c.scanning ? null : c.scanNetwork,
                  ),
                ),
              ],
            ),
            if (c.scanResult.isNotEmpty) ...[
              const SizedBox(height: 8),
              Text(c.scanResult, style: TextStyle(color: th.accent, fontSize: 12)),
            ],
            const SizedBox(height: 8),
            Text('Status: ${c.linkStatus}', style: const TextStyle(color: Color(0xFF99A2C8), fontSize: 12)),
            if (c.titleId.isNotEmpty)
              Text('Title: ${c.titleId} | Build: ${c.buildId.isNotEmpty ? c.buildId : "-"}',
                  style: const TextStyle(color: Colors.white54, fontSize: 11)),
            const SizedBox(height: 6),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              dense: true,
              title: const Text('Auto-detectar Switch na rede', style: TextStyle(fontSize: 12, color: Colors.white)),
              value: c.autoDetectSwitch,
              activeThumbColor: th.accent,
              onChanged: (_) => c.toggleAutoDetectSwitch(),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              dense: true,
              title: const Text('Trocar jogo/versao automaticamente', style: TextStyle(fontSize: 12, color: Colors.white)),
              value: c.autoDetectGame,
              activeThumbColor: th.accent,
              onChanged: (_) => c.toggleAutoDetectGame(),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildConfigTab(SysBotbaseClient c, HudThemeConfig th) {
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('Diagnostico & Acoes',
                    style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 13)),
                const SizedBox(height: 4),
                Text('Switch: ${c.switchIp}:${c.switchPort} (${c.linkStatus})',
                    style: const TextStyle(fontSize: 12, color: Colors.white70)),
                Text('Macros executadas: ${c.macroExecutions}',
                    style: const TextStyle(fontSize: 12, color: Colors.white70)),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: ElevatedButton(
                        style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF6B4E00), foregroundColor: const Color(0xFFFFE27A)),
                        onPressed: c.triggerTestShiny,
                        child: const Text('Testar Shiny (5s)', style: TextStyle(fontSize: 12)),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: ElevatedButton(
                        style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF1B5E20), foregroundColor: Colors.white),
                        onPressed: () => c.editPokemon(0, PokeEditAction.healPartyAll),
                        child: const Text('Curar Time (RAM)', style: TextStyle(fontSize: 12)),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 10),
          GridView.count(
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            crossAxisCount: 2,
            mainAxisSpacing: 8,
            crossAxisSpacing: 8,
            childAspectRatio: 3.0,
            children: [
              ElevatedButton(
                style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF4527A0), foregroundColor: Colors.white),
                onPressed: () => c.setTheme(c.uiTheme + 1),
                child: Text('Tema: ${th.name}'),
              ),
              ElevatedButton(
                style: ElevatedButton.styleFrom(
                  backgroundColor: c.soundEnabled ? const Color(0xFF1B5E20) : const Color(0xFF37474F),
                  foregroundColor: Colors.white,
                ),
                onPressed: c.toggleSound,
                child: Text(c.soundEnabled ? 'Som: ON' : 'Som: OFF'),
              ),
              ElevatedButton(
                style: ElevatedButton.styleFrom(
                  backgroundColor: c.autoStopShiny ? const Color(0xFF1B5E20) : const Color(0xFF37474F),
                  foregroundColor: Colors.white,
                ),
                onPressed: c.toggleAutoStop,
                child: Text(c.autoStopShiny ? 'AutoStop: ON' : 'AutoStop: OFF'),
              ),
              ElevatedButton(
                style: ElevatedButton.styleFrom(
                  backgroundColor: c.autoHeal ? const Color(0xFF1B5E20) : const Color(0xFF37474F),
                  foregroundColor: Colors.white,
                ),
                onPressed: c.toggleAutoHeal,
                child: Text(c.autoHeal ? 'AutoCura: ON' : 'AutoCura: OFF'),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildBotwMapTab(SysBotbaseClient c, HudThemeConfig th) {
    const spots = [
      ('Great Plateau', -796.0, 196.0, 1845.0),
      ('Temple of Time', -814.0, 240.0, 1957.0),
      ('Hyrule Castle', -254.0, 290.0, -1060.0),
      ('Kakariko', 1827.0, 235.0, 995.0),
      ('Hateno', 3395.0, 240.0, 2125.0),
      ('Zora Domain', 3310.0, 265.0, -520.0),
      ('Goron City', 1665.0, 505.0, -2445.0),
      ('Rito Village', -3620.0, 285.0, -1815.0),
      ('Gerudo Town', -3860.0, 160.0, 2890.0),
      ('Korok Forest', 420.0, 255.0, -2140.0),
    ];

    final nx = ((c.posX + 6000.0) / 12000.0).clamp(0.0, 1.0);
    final nz = ((c.posY + 5000.0) / 10000.0).clamp(0.0, 1.0);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                _botwNavTarget != null
                    ? _botwNavSummary(c.posX, c.posY, _botwNavTarget!)
                    : '${c.zoneName.isNotEmpty ? c.zoneName : "Hyrule"} | X:${c.posX.toStringAsFixed(0)} Y:${c.posAlt.toStringAsFixed(0)} Z:${c.posY.toStringAsFixed(0)}',
                style: TextStyle(
                  fontSize: 11,
                  color: _botwNavTarget != null ? const Color(0xFFFFD166) : th.accent,
                  fontWeight: FontWeight.w700,
                ),
                overflow: TextOverflow.ellipsis,
              ),
            ),
            if (_botwNavTarget != null) ...[
              const SizedBox(width: 4),
              _miniBtn(
                'Limpar GPS',
                const Color(0xFF37474F),
                () => setState(() => _botwNavTarget = null),
              ),
            ],
            const SizedBox(width: 6),
            _miniBtn(
              '+ Pin Aqui',
              const Color(0xFF6A1B9A),
              () => _showBotwTeleportDialog(
                c,
                c.posX,
                max(c.posAlt, 200.0),
                c.posY,
                defaultName: 'Pin ${c.botwPins.length + 1}',
              ),
            ),
          ],
        ),
        const SizedBox(height: 5),
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: Row(
            children: [
              _miniBtn(
                'Shrines: Todos',
                _botwShrineFilter == 'all' ? const Color(0xFF00838F) : const Color(0xFF1A2038),
                () => setState(() => _botwShrineFilter = 'all'),
              ),
              const SizedBox(width: 4),
              _miniBtn(
                'Feitos (${c.botwShrines})',
                _botwShrineFilter == 'done' ? const Color(0xFF00838F) : const Color(0xFF1A2038),
                () => setState(() => _botwShrineFilter = 'done'),
              ),
              const SizedBox(width: 4),
              _miniBtn(
                'Faltantes',
                _botwShrineFilter == 'missing' ? const Color(0xFFE65100) : const Color(0xFF1A2038),
                () => setState(() => _botwShrineFilter = 'missing'),
              ),
              const SizedBox(width: 4),
              _miniBtn(
                'Off',
                _botwShrineFilter == 'off' ? const Color(0xFF37474F) : const Color(0xFF1A2038),
                () => setState(() => _botwShrineFilter = 'off'),
              ),
              const SizedBox(width: 8),
              _miniBtn(
                _botwShowKoroks ? 'Koroks: ON' : 'Koroks: OFF',
                _botwShowKoroks ? const Color(0xFF2E7D32) : const Color(0xFF1A2038),
                () => setState(() => _botwShowKoroks = !_botwShowKoroks),
              ),
            ],
          ),
        ),
        const SizedBox(height: 6),
        Expanded(
          child: Center(
            child: AspectRatio(
              aspectRatio: 1.2,
              child: Container(
                decoration: BoxDecoration(
                  color: const Color(0xFF0A141E),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: th.cardBorder),
                ),
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(9),
                  child: InteractiveViewer(
                    minScale: 1.0,
                    maxScale: 10.0,
                    panEnabled: true,
                    scaleEnabled: true,
                    child: LayoutBuilder(
                      builder: (_, box) => GestureDetector(
                        onLongPressStart: (d) {
                          final tx = (d.localPosition.dx / box.maxWidth) * 12000.0 - 6000.0;
                          final tz = (d.localPosition.dy / box.maxHeight) * 10000.0 - 5000.0;
                          final ty = max(c.posAlt, 260.0);
                          _showBotwTeleportDialog(c, tx, ty, tz);
                        },
                        child: Stack(
                          fit: StackFit.expand,
                          children: [
                            Image.asset('assets/m/zelda_botw/hyrule.png', fit: BoxFit.fill),
                            CustomPaint(
                              size: Size(box.maxWidth, box.maxHeight),
                              painter: _BotwMapPainter(
                                linkNormX: nx,
                                linkNormZ: nz,
                                spots: spots,
                                accent: th.accent,
                                shrines: c.botwShrineList,
                                shrineFilter: _botwShrineFilter,
                                koroks: c.botwKorokList,
                                showKoroks: _botwShowKoroks,
                                shrineDoneCount: c.botwShrines,
                                pins: c.botwPins,
                                navTarget: _botwNavTarget,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
        const SizedBox(height: 6),
        SizedBox(
          height: 32,
          child: ListView.separated(
            scrollDirection: Axis.horizontal,
            itemCount: c.botwPins.length + spots.length,
            separatorBuilder: (_, _) => const SizedBox(width: 6),
            itemBuilder: (_, i) {
              if (i < c.botwPins.length) {
                final pin = c.botwPins[i];
                return _miniBtn(
                  '📍 ${pin.name}',
                  const Color(0xFF6A1B9A),
                  () => _showBotwTeleportDialog(
                    c,
                    pin.x,
                    pin.y,
                    pin.z,
                    defaultName: pin.name,
                    pinIndex: i,
                  ),
                );
              }
              final s = spots[i - c.botwPins.length];
              return _miniBtn(
                s.$1,
                const Color(0xFF0F3460),
                () => _showBotwTeleportDialog(c, s.$2, s.$3, s.$4, defaultName: s.$1),
              );
            },
          ),
        ),
      ],
    );
  }

  String _botwNavSummary(double lx, double lz, ({String name, double x, double z}) t) {
    final dx = t.x - lx;
    final dz = t.z - lz;
    final dist = sqrt(dx * dx + dz * dz).round();
    final deg = (atan2(dx, -dz) * 180 / pi + 360) % 360;
    const dirs = ['N', 'NE', 'L', 'SE', 'S', 'SO', 'O', 'NO'];
    final dir = dirs[((deg + 22.5) / 45).floor() % 8];
    return '🧭 ${t.name}: ${dist}m ($dir)';
  }

  Future<void> _showBotwTeleportDialog(
    SysBotbaseClient c,
    double x,
    double y,
    double z, {
    String? defaultName,
    int? pinIndex,
  }) async {
    final nameCtrl = TextEditingController(text: defaultName ?? 'Pin ${c.botwPins.length + 1}');
    await showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF13172B),
        title: Text(
          pinIndex != null ? 'Pin: ${c.botwPins[pinIndex].name}' : 'Navegação / Teleporte',
          style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w700, color: Colors.white),
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              'X: ${x.toStringAsFixed(0)}   Y: ${y.toStringAsFixed(0)}   Z: ${z.toStringAsFixed(0)}',
              style: const TextStyle(fontSize: 12, color: Color(0xFF00E5FF), fontWeight: FontWeight.w600),
            ),
            if (pinIndex == null) ...[
              const SizedBox(height: 10),
              TextField(
                controller: nameCtrl,
                style: const TextStyle(fontSize: 13, color: Colors.white),
                decoration: const InputDecoration(
                  labelText: 'Nome do Pin (opcional)',
                  labelStyle: TextStyle(fontSize: 12, color: Colors.white60),
                  isDense: true,
                  border: OutlineInputBorder(),
                ),
              ),
            ],
          ],
        ),
        actions: [
          if (pinIndex != null)
            TextButton(
              onPressed: () {
                c.removeBotwPin(pinIndex);
                Navigator.of(ctx).pop();
              },
              child: const Text('Remover', style: TextStyle(color: Colors.redAccent)),
            )
          else
            TextButton(
              onPressed: () {
                final label = nameCtrl.text.trim().isEmpty ? 'Pin ${c.botwPins.length + 1}' : nameCtrl.text.trim();
                c.addBotwPin(label, x, y, z);
                Navigator.of(ctx).pop();
              },
              child: const Text('Salvar Pin', style: TextStyle(color: Color(0xFFFFD166))),
            ),
          TextButton(
            onPressed: () {
              final label = pinIndex != null
                  ? c.botwPins[pinIndex].name
                  : (nameCtrl.text.trim().isEmpty ? 'Destino' : nameCtrl.text.trim());
              setState(() => _botwNavTarget = (name: label, x: x, z: z));
              Navigator.of(ctx).pop();
            },
            child: const Text('Destino (GPS)', style: TextStyle(color: Color(0xFF00E5FF))),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF00838F)),
            onPressed: () {
              Navigator.of(ctx).pop();
              c.pokeBotwPosition(x, y, z);
            },
            child: const Text('Teleporte (Exp.)'),
          ),
        ],
      ),
    );
  }

  Widget _buildFooter(SysBotbaseClient c, HudThemeConfig th) {
    return Container(
      height: 24,
      padding: const EdgeInsets.symmetric(horizontal: 10),
      color: th.topBg,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Expanded(
            child: Text(
              c.scanning
                  ? c.scanResult
                  : (c.lastLog.isNotEmpty ? c.lastLog : '[sys-botbase] ${c.switchIp}:${c.switchPort}'),
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA)),
            ),
          ),
          Text('Tema: ${th.name}', style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA))),
        ],
      ),
    );
  }

  Widget _statusBadge(String label, bool active, Color activeBg) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 5),
      decoration: BoxDecoration(
        color: active ? activeBg : const Color(0xFF101324),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: active ? Colors.white38 : const Color(0xFF282E4A)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(
            active ? Icons.check_circle_rounded : Icons.radio_button_unchecked,
            size: 13,
            color: active ? Colors.white : const Color(0xFF6B7294),
          ),
          const SizedBox(width: 5),
          Text(
            label,
            style: TextStyle(
              fontSize: 11,
              fontWeight: active ? FontWeight.w700 : FontWeight.w500,
              color: active ? Colors.white : const Color(0xFF8E95B8),
            ),
          ),
        ],
      ),
    );
  }
}

class _BotwMapPainter extends CustomPainter {
  final double linkNormX;
  final double linkNormZ;
  final List<(String, double, double, double)> spots;
  final Color accent;
  final List<BotwMarker> shrines;
  final String shrineFilter;
  final List<BotwMarker> koroks;
  final bool showKoroks;
  final int shrineDoneCount;
  final List<BotwPin> pins;
  final ({String name, double x, double z})? navTarget;

  const _BotwMapPainter({
    required this.linkNormX,
    required this.linkNormZ,
    required this.spots,
    required this.accent,
    required this.shrines,
    required this.shrineFilter,
    required this.koroks,
    required this.showKoroks,
    required this.shrineDoneCount,
    required this.pins,
    this.navTarget,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final gridPaint = Paint()
      ..color = Colors.white.withValues(alpha: 0.08)
      ..strokeWidth = 0.8;
    for (int i = 1; i < 5; i++) {
      final dx = size.width * (i / 5);
      final dy = size.height * (i / 5);
      canvas.drawLine(Offset(dx, 0), Offset(dx, size.height), gridPaint);
      canvas.drawLine(Offset(0, dy), Offset(size.width, dy), gridPaint);
    }

    if (showKoroks) {
      final korokFill = Paint()..color = const Color(0xFF9BE564);
      final korokBorder = Paint()
        ..color = Colors.black.withValues(alpha: 0.65)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 0.7;
      for (final k in koroks) {
        final kx = ((k.x + 6000.0) / 12000.0).clamp(0.0, 1.0) * size.width;
        final kz = ((k.z + 5000.0) / 10000.0).clamp(0.0, 1.0) * size.height;
        canvas.drawCircle(Offset(kx, kz), 1.6, korokFill);
        canvas.drawCircle(Offset(kx, kz), 1.6, korokBorder);
      }
    }

    if (shrineFilter != 'off') {
      final donePaint = Paint()..color = const Color(0xFF00E5FF);
      final missPaint = Paint()..color = const Color(0xFFFF9F1C);
      final shrineBorder = Paint()
        ..color = Colors.black.withValues(alpha: 0.8)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 0.9;
      for (final s in shrines) {
        if (shrineFilter == 'done' && !s.done) continue;
        if (shrineFilter == 'missing' && s.done) continue;
        final sx = ((s.x + 6000.0) / 12000.0).clamp(0.0, 1.0) * size.width;
        final sz = ((s.z + 5000.0) / 10000.0).clamp(0.0, 1.0) * size.height;
        final path = Path()
          ..moveTo(sx, sz - 2.8)
          ..lineTo(sx + 2.8, sz)
          ..lineTo(sx, sz + 2.8)
          ..lineTo(sx - 2.8, sz)
          ..close();
        canvas.drawPath(path, s.done ? donePaint : missPaint);
        canvas.drawPath(path, shrineBorder);
      }
    }

    final spotPaint = Paint()..color = const Color(0xFF4ECDC4);
    final spotBorder = Paint()
      ..color = Colors.black.withValues(alpha: 0.75)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.2;
    for (final s in spots) {
      final sx = ((s.$2 + 6000.0) / 12000.0).clamp(0.0, 1.0) * size.width;
      final sz = ((s.$4 + 5000.0) / 10000.0).clamp(0.0, 1.0) * size.height;
      canvas.drawCircle(Offset(sx, sz), 3.5, spotPaint);
      canvas.drawCircle(Offset(sx, sz), 3.5, spotBorder);
      final tp = TextPainter(
        text: TextSpan(
          text: s.$1,
          style: const TextStyle(
            color: Color(0xFFE8F4FA),
            fontSize: 9,
            fontWeight: FontWeight.w600,
            shadows: [Shadow(color: Colors.black, blurRadius: 3)],
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, Offset((sx + 5).clamp(0, size.width - tp.width), (sz - 5).clamp(0, size.height - tp.height)));
    }

    final pinFill = Paint()..color = const Color(0xFFE040FB);
    final pinBorder = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.3;
    for (final p in pins) {
      final px = ((p.x + 6000.0) / 12000.0).clamp(0.0, 1.0) * size.width;
      final pz = ((p.z + 5000.0) / 10000.0).clamp(0.0, 1.0) * size.height;
      canvas.drawCircle(Offset(px, pz), 4.5, pinFill);
      canvas.drawCircle(Offset(px, pz), 4.5, pinBorder);
      final tp = TextPainter(
        text: TextSpan(
          text: p.name,
          style: const TextStyle(
            color: Color(0xFFFFD166),
            fontSize: 9,
            fontWeight: FontWeight.w700,
            shadows: [Shadow(color: Colors.black, blurRadius: 3)],
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, Offset((px + 6).clamp(0, size.width - tp.width), (pz - 6).clamp(0, size.height - tp.height)));
    }

    final lx = linkNormX * size.width;
    final lz = linkNormZ * size.height;

    if (navTarget != null) {
      final tx = ((navTarget!.x + 6000.0) / 12000.0).clamp(0.0, 1.0) * size.width;
      final tz = ((navTarget!.z + 5000.0) / 10000.0).clamp(0.0, 1.0) * size.height;
      canvas.drawLine(
        Offset(lx, lz),
        Offset(tx, tz),
        Paint()
          ..color = const Color(0xFFFFD166).withValues(alpha: 0.85)
          ..strokeWidth = 2.0,
      );
      canvas.drawCircle(
        Offset(tx, tz),
        7,
        Paint()
          ..color = const Color(0xFFFF5252)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2.0,
      );
      canvas.drawCircle(Offset(tx, tz), 3, Paint()..color = const Color(0xFFFFD166));
    }

    canvas.drawCircle(
      Offset(lx, lz),
      9,
      Paint()..color = accent.withValues(alpha: 0.25),
    );
    canvas.drawCircle(
      Offset(lx, lz),
      5,
      Paint()..color = const Color(0xFFFFD166),
    );
    canvas.drawCircle(
      Offset(lx, lz),
      5,
      Paint()
        ..color = Colors.white
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5,
    );
  }

  @override
  bool shouldRepaint(covariant _BotwMapPainter old) =>
      old.linkNormX != linkNormX ||
      old.linkNormZ != linkNormZ ||
      old.accent != accent ||
      old.shrineFilter != shrineFilter ||
      old.showKoroks != showKoroks ||
      old.shrineDoneCount != shrineDoneCount ||
      old.shrines.length != shrines.length ||
      old.koroks.length != koroks.length ||
      old.pins.length != pins.length ||
      old.navTarget != navTarget;
}
