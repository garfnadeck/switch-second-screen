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
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  'Hyrule HUD (Breath of the Wild)',
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
          if (c.titleId.isNotEmpty || c.buildId.isNotEmpty) ...[
            const SizedBox(height: 4),
            Text(
              'Title: ${c.titleId} | Build: ${c.buildId} | Perfil: ${c.activeBotwProfile?.versionLabel ?? "sem offsets mapeados para esta build"}',
              style: const TextStyle(fontSize: 10, color: Color(0xFF00F5D4)),
            ),
          ],
          const SizedBox(height: 6),
          Row(
            children: [
              const Text('Versao BotW:', style: TextStyle(fontSize: 11, color: Color(0xFF99A2C8))),
              const SizedBox(width: 8),
              ...['auto', '1.0.0', '1.6.0'].map((v) {
                final sel = c.botwSelectedVersion == v;
                return Padding(
                  padding: const EdgeInsets.only(right: 6),
                  child: ChoiceChip(
                    label: Text(v == 'auto' ? 'Auto (BuildID)' : v, style: const TextStyle(fontSize: 11)),
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
                Text('Progresso em Hyrule',
                    style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 12)),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: Row(
                        children: [
                          Text('Shrines: ${c.botwShrines}/120',
                              style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600)),
                          const Spacer(),
                          _miniBtn('-1', const Color(0xFF23294E), () => c.setBotwShrines(-1)),
                          const SizedBox(width: 4),
                          _miniBtn('+1', const Color(0xFF0F3460), () => c.setBotwShrines(1)),
                        ],
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Row(
                        children: [
                          Text('Koroks: ${c.botwKoroks}/900',
                              style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600)),
                          const Spacer(),
                          _miniBtn('-1', const Color(0xFF23294E), () => c.setBotwKoroks(-1)),
                          const SizedBox(width: 4),
                          _miniBtn('+1', const Color(0xFF0F3460), () => c.setBotwKoroks(1)),
                        ],
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
                      return FilterChip(
                        label: Text(beasts[i], style: const TextStyle(fontSize: 11)),
                        selected: on,
                        selectedColor: const Color(0xFF1B5E20),
                        backgroundColor: const Color(0xFF101324),
                        onSelected: (_) => c.toggleBotwBeast(i),
                      );
                    }),
                    FilterChip(
                      label: const Text('Master Sword', style: TextStyle(fontSize: 11)),
                      selected: c.botwMasterSword,
                      selectedColor: const Color(0xFF0F3460),
                      backgroundColor: const Color(0xFF101324),
                      onSelected: (_) => c.toggleBotwMasterSword(),
                    ),
                    FilterChip(
                      label: const Text('Hylian Shield', style: TextStyle(fontSize: 11)),
                      selected: c.botwHylianShield,
                      selectedColor: const Color(0xFF0F3460),
                      backgroundColor: const Color(0xFF101324),
                      onSelected: (_) => c.toggleBotwHylianShield(),
                    ),
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
      const regions = [
        ('Great Plateau', 'Inicio • Shrine of Resurrection / 4 Shrines'),
        ('Central Hyrule', 'Hyrule Castle • Hylian Shield / Guardians'),
        ('Kakariko & Hateno', 'Impa, Great Fairy, Casa do Link & Tech Lab'),
        ('Zora\'s Domain (Leste)', 'Vah Ruta • Mipha\'s Grace'),
        ('Eldin / Goron City (NE)', 'Vah Rudania • Daruk\'s Protection'),
        ('Hebra / Rito Village (NO)', 'Vah Medoh • Revali\'s Gale'),
        ('Gerudo Desert (SO)', 'Vah Naboris • Urbosa\'s Fury'),
        ('Korok Forest (Norte)', 'Lost Woods • Master Sword (13 Coracoes)'),
      ];
      return ListView.separated(
        itemCount: regions.length,
        separatorBuilder: (_, _) => const SizedBox(height: 6),
        itemBuilder: (_, i) {
          final r = regions[i];
          return Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: th.cardBg,
              borderRadius: BorderRadius.circular(th.cardRadius),
              border: Border.all(color: th.cardBorder),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(r.$1, style: TextStyle(color: th.accent, fontWeight: FontWeight.w700, fontSize: 13)),
                const SizedBox(height: 2),
                Text(r.$2, style: const TextStyle(color: Colors.white70, fontSize: 11)),
              ],
            ),
          );
        },
      );
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
                      final res = await EmuiiboFtpService.searchApi(_amiiboSearchCtrl.text);
                      setState(() {
                        _amiiboCatalog = res;
                        _amiiboBusy = false;
                        _amiiboMsg = '${res.length} encontrado(s)';
                      });
                    }),
                  ],
                ),
                if (_amiiboMsg.isNotEmpty) ...[
                  const SizedBox(height: 6),
                  Text(_amiiboMsg, style: TextStyle(color: th.accent, fontSize: 11)),
                ],
                const SizedBox(height: 6),
                ..._amiiboCatalog.take(10).map((item) => Container(
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

  Widget _buildFooter(SysBotbaseClient c, HudThemeConfig th) {
    return Container(
      height: 24,
      padding: const EdgeInsets.symmetric(horizontal: 10),
      color: th.topBg,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(
            c.lastLog.isNotEmpty ? c.lastLog : '[sys-botbase] ${c.switchIp}:${c.switchPort}',
            style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA)),
          ),
          Text('Tema: ${th.name}', style: const TextStyle(fontSize: 10, color: Color(0xFF8888AA))),
        ],
      ),
    );
  }
}
