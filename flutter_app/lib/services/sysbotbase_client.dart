import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/game_state.dart';
import 'pa9_crypto.dart';

class WildZoneRef {
  final int id;
  final double nx;
  final double ny;
  final double r;
  final String label;
  const WildZoneRef(this.id, this.nx, this.ny, this.r, this.label);
}

class BotwMemoryProfile {
  final String versionLabel;
  final List<int> heartsChain;
  final List<int> maxHeartsChain;
  final List<int> staminaChain;
  final List<int> rupeesChain;
  final List<int> posChain;
  const BotwMemoryProfile({
    required this.versionLabel,
    this.heartsChain = const [],
    this.maxHeartsChain = const [],
    this.staminaChain = const [],
    this.rupeesChain = const [],
    this.posChain = const [],
  });
}

class SysBotbaseClient extends ChangeNotifier {
  static const Map<String, BotwMemoryProfile> botwVersions = {
    '2094F152836683A3': BotwMemoryProfile(versionLabel: 'BotW 1.0.0'),
    '8E9978D50BDD20B4': BotwMemoryProfile(
      versionLabel: 'BotW 1.6.0',
      heartsChain: [0x02CA1A78, 0x80, 0x848],
      maxHeartsChain: [0x02C9FD70, 0x38, 0x2A8],
      staminaChain: [0x02C9FD70, 0x38, 0x2AC],
      rupeesChain: [0x02CC5FE0, 0xE40, 0x5B08, 0x5D90],
    ),
  };
  static const List<int> _partyBase = [0x610A710, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0];
  static const List<int> _posBase = [0x47D71A0, 0x248, 0x0, 0x138, 0x90];
  static const List<int> _stashBase = [0x610A710, 0x120, 0x168, 0x0];

  static const List<WildZoneRef> wildZones = [
    WildZoneRef(1, 0.585, 0.905, 0.085, 'Campo 1 (Zona Selvagem 1)'),
    WildZoneRef(2, 0.620, 0.660, 0.075, 'Campo 2 (Zona Selvagem 2)'),
    WildZoneRef(3, 0.500, 0.280, 0.070, 'Campo 3 (Zona Selvagem 3)'),
    WildZoneRef(4, 0.570, 0.150, 0.075, 'Campo 4 (Zona Selvagem 4)'),
    WildZoneRef(5, 0.410, 0.600, 0.075, 'Campo 5 (Zona Selvagem 5)'),
    WildZoneRef(6, 0.900, 0.530, 0.075, 'Campo 6 (Zona Selvagem 6)'),
    WildZoneRef(7, 0.290, 0.430, 0.070, 'Campo 7 (Zona Selvagem 7)'),
    WildZoneRef(8, 0.710, 0.420, 0.070, 'Campo 8 (Zona Selvagem 8)'),
    WildZoneRef(9, 0.130, 0.560, 0.075, 'Campo 9 (Zona Selvagem 9)'),
    WildZoneRef(10, 0.160, 0.660, 0.075, 'Campo 10 (Zona Selvagem 10)'),
    WildZoneRef(11, 0.820, 0.590, 0.075, 'Campo 11 (Zona Selvagem 11)'),
    WildZoneRef(12, 0.420, 0.820, 0.075, 'Campo 12 (Zona Selvagem 12)'),
    WildZoneRef(13, 0.400, 0.075, 0.075, 'Campo 13 (Zona Selvagem 13)'),
    WildZoneRef(14, 0.110, 0.430, 0.075, 'Campo 14 (Zona Selvagem 14)'),
    WildZoneRef(15, 0.810, 0.180, 0.075, 'Campo 15 (Zona Selvagem 15)'),
    WildZoneRef(16, 0.360, 0.680, 0.070, 'Campo 16 (Zona Selvagem 16)'),
    WildZoneRef(17, 0.760, 0.770, 0.070, 'Campo 17 (Zona Selvagem 17)'),
    WildZoneRef(18, 0.200, 0.180, 0.075, 'Campo 18 (Zona Selvagem 18)'),
    WildZoneRef(19, 0.800, 0.310, 0.075, 'Campo 19 (Zona Selvagem 19)'),
    WildZoneRef(20, 0.500, 0.500, 0.085, 'Campo 20 (Centro / Prisma)'),
  ];

  // State
  String switchIp = '192.168.15.10';
  int switchPort = 6000;
  GameProfile profile = GameProfile.pokemonZa;
  int uiTheme = 1; // 0=Tatico, 1=Lumiose, 2=Rotom, 3=OLED
  bool soundEnabled = true;
  bool autoStopShiny = true;
  bool autoHeal = false;

  bool connected = false;
  String linkStatus = 'desconectado';
  String titleId = '';
  String buildId = '';

  BotwMemoryProfile? get activeBotwProfile {
    if (botwSelectedVersion == '1.0.0') return botwVersions['2094F152836683A3'];
    if (botwSelectedVersion == '1.6.0') return botwVersions['8E9978D50BDD20B4'];
    final bid = buildId.toUpperCase();
    for (final entry in botwVersions.entries) {
      if (bid.startsWith(entry.key.toUpperCase())) return entry.value;
    }
    return null;
  }

  final List<PokemonSlot> party = List.generate(6, (_) => PokemonSlot());
  final List<Uint8List?> _slotClaro = List.filled(6, null);
  final List<Uint8List?> _slotExtra = List.filled(6, null);
  final List<int> _slotEc = List.filled(6, 0);
  final List<int> _slotExtAge = List.filled(6, 0);

  double posX = -830.0;
  double posY = -500.0; // Z do jogo (eixo vertical 2D)
  double posAlt = 0.0;  // Y do jogo (altitude)
  String zoneName = 'Lumiose City';
  final List<Offset> trail = [];

  bool shinyStashFound = false;
  int shinyStashSpecies = 0;
  String shinyStashName = '';
  bool testShinyActive = false;
  int _prevShinyCount = 0;
  bool _prevStash = false;

  int activeMacroSlot = -1;
  int macroExecutions = 0;
  String lastLog = '';

  bool scanning = false;
  int scanProgress = 0;
  String scanResult = '';
  bool autoDetectSwitch = true;
  bool autoDetectGame = true;
  DateTime _lastAutoScan = DateTime.fromMillisecondsSinceEpoch(0);
  int _titlePollCnt = 0;

  List<FavWarp> favWarps = List.generate(3, (_) => const FavWarp());

  // Zelda BotW progress & HUD stats
  int botwHeartsCur = 12; // em quartos de coracao (12 = 3 coracoes)
  int botwHeartsMax = 12;
  int botwStaminaPct = 100; // 100 = 1 roda, 300 = 3 rodas
  int botwRupees = 0;
  int botwShrines = 0;
  int botwKoroks = 0;
  int botwBeasts = 0; // bitmask 0..3: Ruta, Rudania, Medoh, Naboris
  bool botwMasterSword = false;
  bool botwHylianShield = false;
  String botwSelectedVersion = 'auto'; // 'auto', '1.0.0', '1.6.0'

  // TCP internals
  Socket? _socket;
  final StringBuffer _rxBuf = StringBuffer();
  Completer<String>? _pendingReply;
  bool _busy = false;
  Timer? _pollTimer;
  Timer? _macroTimer;
  int _partyIdx = 0;
  int _stashCnt = 0;
  double _circleAngle = 0;
  DateTime _lastAutoHeal = DateTime.fromMillisecondsSinceEpoch(0);
  DateTime _lastShinyAlert = DateTime.fromMillisecondsSinceEpoch(0);

  Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    switchIp = prefs.getString('switchIp') ?? switchIp;
    switchPort = prefs.getInt('switchPort') ?? switchPort;
    uiTheme = prefs.getInt('uiTheme') ?? 1;
    soundEnabled = prefs.getBool('soundEnabled') ?? true;
    autoStopShiny = prefs.getBool('autoStopShiny') ?? true;
    autoHeal = prefs.getBool('autoHeal') ?? false;
    autoDetectSwitch = prefs.getBool('autoDetectSwitch') ?? true;
    autoDetectGame = prefs.getBool('autoDetectGame') ?? true;
    botwShrines = prefs.getInt('botwShrines') ?? 0;
    botwKoroks = prefs.getInt('botwKoroks') ?? 0;
    botwBeasts = prefs.getInt('botwBeasts') ?? 0;
    botwMasterSword = prefs.getBool('botwMasterSword') ?? false;
    botwHylianShield = prefs.getBool('botwHylianShield') ?? false;
    botwSelectedVersion = prefs.getString('botwSelectedVersion') ?? 'auto';
    final fwRaw = prefs.getString('favWarps');
    if (fwRaw != null) {
      try {
        final list = jsonDecode(fwRaw) as List;
        favWarps = List.generate(
          3,
          (i) => i < list.length
              ? FavWarp.fromJson(Map<String, dynamic>.from(list[i]))
              : const FavWarp(),
        );
      } catch (_) {}
    }
    notifyListeners();
    connect();
  }

  Future<void> _savePrefs() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('switchIp', switchIp);
    await prefs.setInt('switchPort', switchPort);
    await prefs.setInt('uiTheme', uiTheme);
    await prefs.setBool('soundEnabled', soundEnabled);
    await prefs.setBool('autoStopShiny', autoStopShiny);
    await prefs.setBool('autoHeal', autoHeal);
    await prefs.setBool('autoDetectSwitch', autoDetectSwitch);
    await prefs.setBool('autoDetectGame', autoDetectGame);
    await prefs.setInt('botwShrines', botwShrines);
    await prefs.setInt('botwKoroks', botwKoroks);
    await prefs.setInt('botwBeasts', botwBeasts);
    await prefs.setBool('botwMasterSword', botwMasterSword);
    await prefs.setBool('botwHylianShield', botwHylianShield);
    await prefs.setString('botwSelectedVersion', botwSelectedVersion);
    await prefs.setString(
      'favWarps',
      jsonEncode(favWarps.map((e) => e.toJson()).toList()),
    );
  }

  void setTheme(int idx) {
    uiTheme = ((idx % 4) + 4) % 4;
    _savePrefs();
    notifyListeners();
  }

  void toggleSound() {
    soundEnabled = !soundEnabled;
    if (soundEnabled) {
      SystemSound.play(SystemSoundType.click);
    }
    _savePrefs();
    notifyListeners();
  }

  void toggleAutoStop() {
    autoStopShiny = !autoStopShiny;
    _savePrefs();
    notifyListeners();
  }

  void toggleAutoHeal() {
    autoHeal = !autoHeal;
    _savePrefs();
    notifyListeners();
  }

  void toggleAutoDetectSwitch() {
    autoDetectSwitch = !autoDetectSwitch;
    _savePrefs();
    notifyListeners();
  }

  void toggleAutoDetectGame() {
    autoDetectGame = !autoDetectGame;
    _savePrefs();
    notifyListeners();
  }

  void setBotwShrines(int delta) {
    botwShrines = (botwShrines + delta).clamp(0, 120);
    _savePrefs();
    notifyListeners();
  }

  void setBotwKoroks(int delta) {
    botwKoroks = (botwKoroks + delta).clamp(0, 900);
    _savePrefs();
    notifyListeners();
  }

  void toggleBotwBeast(int bit) {
    botwBeasts ^= (1 << bit);
    _savePrefs();
    notifyListeners();
  }

  void toggleBotwMasterSword() {
    botwMasterSword = !botwMasterSword;
    _savePrefs();
    notifyListeners();
  }

  void toggleBotwHylianShield() {
    botwHylianShield = !botwHylianShield;
    _savePrefs();
    notifyListeners();
  }

  void setBotwVersion(String v) {
    botwSelectedVersion = v;
    _savePrefs();
    notifyListeners();
  }

  void setProfile(GameProfile p) {
    profile = p;
    stopAllMacros();
    notifyListeners();
  }

  Future<void> updateTarget(String ip, int port) async {
    switchIp = ip.trim();
    switchPort = port;
    await _savePrefs();
    disconnect();
    await connect();
  }

  Future<void> connect() async {
    if (_socket != null) return;
    linkStatus = 'conectando...';
    notifyListeners();
    try {
      final s = await Socket.connect(
        switchIp,
        switchPort,
        timeout: const Duration(milliseconds: 2000),
      );
      s.setOption(SocketOption.tcpNoDelay, true);
      _socket = s;
      connected = true;
      linkStatus = 'conectado';
      _rxBuf.clear();

      s.listen(
        (Uint8List data) {
          final chunk = ascii.decode(data, allowInvalid: true);
          _rxBuf.write(chunk);
          final str = _rxBuf.toString();
          final idx = str.indexOf('\n');
          if (idx >= 0) {
            final line = str.substring(0, idx).replaceAll('\r', '').trim();
            final rest = str.substring(idx + 1);
            _rxBuf.clear();
            _rxBuf.write(rest);
            if (_pendingReply != null && !_pendingReply!.isCompleted) {
              _pendingReply!.complete(line);
            }
          }
        },
        onError: (e) {
          disconnect(status: 'erro TCP');
        },
        onDone: () {
          disconnect(status: 'desconectado');
        },
      );

      _titlePollCnt = 0;
      _startPolling();
      notifyListeners();
    } catch (e) {
      connected = false;
      linkStatus = 'sem conexao';
      notifyListeners();
      final now = DateTime.now();
      if (autoDetectSwitch &&
          !scanning &&
          now.difference(_lastAutoScan).inSeconds >= 20) {
        _lastAutoScan = now;
        Future.microtask(scanNetwork);
      } else {
        // Auto-retry in 5s
        Future.delayed(const Duration(seconds: 5), () {
          if (!connected && !scanning) connect();
        });
      }
    }
  }

  void disconnect({String status = 'desconectado'}) {
    _pollTimer?.cancel();
    _pollTimer = null;
    if (_pendingReply != null && !_pendingReply!.isCompleted) {
      _pendingReply!.completeError('disconnected');
    }
    _pendingReply = null;
    _busy = false;
    try {
      _socket?.destroy();
    } catch (_) {}
    _socket = null;
    connected = false;
    linkStatus = status;
    notifyListeners();
  }

  Future<String?> _sendAndWait(String cmd, {int timeoutMs = 800}) async {
    final s = _socket;
    if (s == null || _busy) return null;
    _busy = true;
    final comp = Completer<String>();
    _pendingReply = comp;
    try {
      s.write('$cmd\r\n');
      final res = await comp.future.timeout(Duration(milliseconds: timeoutMs));
      _busy = false;
      return res;
    } catch (_) {
      _busy = false;
      return null;
    }
  }

  void _sendFireAndForget(String cmd) {
    final s = _socket;
    if (s == null) return;
    try {
      s.write('$cmd\r\n');
    } catch (_) {}
  }

  void _applyDetectedTitle(String? tid, String? bid) {
    if (tid != null && tid.isNotEmpty) titleId = tid.trim().toUpperCase();
    if (bid != null && bid.isNotEmpty) buildId = bid.trim().toUpperCase();
    if (!autoDetectGame || titleId.isEmpty) return;

    GameProfile? detected;
    if (titleId == '01007EF00011E000') {
      detected = GameProfile.zeldaBotw;
    } else if (titleId == '0100F43008C44000') {
      detected = GameProfile.pokemonZa;
    } else if (titleId == '0100554023408000' || titleId == '010034D02340E000') {
      detected = GameProfile.pokemonFr;
    }

    if (detected != null && detected != profile) {
      profile = detected;
      final label = detected == GameProfile.zeldaBotw
          ? 'Zelda BOTW'
          : detected == GameProfile.pokemonFr
              ? 'Pokemon FireRed'
              : 'Pokemon Legends Z-A';
      lastLog = 'Auto-detectado: $label';
    }
  }

  void _startPolling() {
    _pollTimer?.cancel();
    _pollTimer = Timer.periodic(const Duration(milliseconds: 55), (_) async {
      if (!connected || _busy) return;
      final checkTitle = _titlePollCnt == 0 ||
          (profile != GameProfile.pokemonZa && _titlePollCnt % 10 == 0) ||
          (_titlePollCnt % 40 == 0);
      _titlePollCnt++;

      if (checkTitle) {
        final tid = await _sendAndWait('getTitleID');
        final bid = await _sendAndWait('getBuildID');
        _applyDetectedTitle(tid, bid);
        if (profile != GameProfile.pokemonZa) {
          if (tid != null || bid != null) {
            final ver = activeBotwProfile?.versionLabel ?? '';
            linkStatus = ver.isNotEmpty ? 'ok ($ver)' : 'ok ($titleId)';
            notifyListeners();
          }
          if (profile == GameProfile.zeldaBotw) {
            await _pollBotwStats();
          }
          return;
        }
      } else if (profile != GameProfile.pokemonZa) {
        if (profile == GameProfile.zeldaBotw && _titlePollCnt % 2 == 0) {
          await _pollBotwStats();
        }
        return;
      }

      // 1. Read current party slot
      final slot = _partyIdx;
      _partyIdx = (_partyIdx + 1) % 6;
      await _pollPartySlot(slot);

      // 2. Read position
      await _pollPosition();

      // 3. Every 12 cycles, read ShinyStash
      if (++_stashCnt >= 12) {
        _stashCnt = 0;
        await _pollShinyStash();
      }

      _checkShinyAndAutoHeal();
      notifyListeners();
    });
  }

  String _chainArgs(List<int> chain) =>
      chain.map((e) => '0x${e.toRadixString(16).toUpperCase()}').join(' ');

  Future<void> _pollBotwStats() async {
    final p = activeBotwProfile;
    if (p == null) return;
    bool changed = false;

    if (p.staminaChain.isNotEmpty) {
      final hex = await _sendAndWait('pointerPeek 8 ${_chainArgs(p.staminaChain)}');
      final raw = hex != null ? Pa9Crypto.hexToBytes(hex) : null;
      if (raw != null && raw.length >= 8) {
        final bd = ByteData.sublistView(raw);
        final cur = bd.getFloat32(0, Endian.little);
        final maxS = bd.getFloat32(4, Endian.little);
        if (maxS > 0 && cur >= 0 && cur <= maxS + 10) {
          final pct = ((cur / maxS) * 100.0).round().clamp(0, 100);
          if (pct != botwStaminaPct) {
            botwStaminaPct = pct;
            changed = true;
          }
        }
      }
    }

    if (p.heartsChain.isNotEmpty && _titlePollCnt % 4 == 0) {
      final hex = await _sendAndWait('pointerPeek 4 ${_chainArgs(p.heartsChain)}');
      final raw = hex != null ? Pa9Crypto.hexToBytes(hex) : null;
      if (raw != null && raw.length >= 4) {
        final curH = ByteData.sublistView(raw).getUint32(0, Endian.little);
        if (curH <= 160 && curH != botwHeartsCur) {
          botwHeartsCur = curH;
          if (botwHeartsMax < curH) botwHeartsMax = curH;
          changed = true;
        }
      }
    }

    if (p.maxHeartsChain.isNotEmpty && _titlePollCnt % 8 == 0) {
      final hex = await _sendAndWait('pointerPeek 4 ${_chainArgs(p.maxHeartsChain)}');
      final raw = hex != null ? Pa9Crypto.hexToBytes(hex) : null;
      if (raw != null && raw.length >= 4) {
        final maxHf = ByteData.sublistView(raw).getFloat32(0, Endian.little);
        final maxH = maxHf.round();
        if (maxH >= 12 && maxH <= 160 && maxH != botwHeartsMax) {
          botwHeartsMax = maxH;
          changed = true;
        }
      }
    }

    if (p.rupeesChain.isNotEmpty && _titlePollCnt % 8 == 4) {
      final hex = await _sendAndWait('pointerPeek 4 ${_chainArgs(p.rupeesChain)}');
      final raw = hex != null ? Pa9Crypto.hexToBytes(hex) : null;
      if (raw != null && raw.length >= 4) {
        final rup = ByteData.sublistView(raw).getUint32(0, Endian.little);
        if (rup <= 999999 && rup != botwRupees) {
          botwRupees = rup;
          changed = true;
        }
      }
    }

    if (changed) notifyListeners();
  }

  Future<void> _pollPartySlot(int slot) async {
    final c = List<int>.from(_partyBase);
    c[3] = _partyBase[3] + slot * 8;
    c[5] = 0x30;
    final hex = await _sendAndWait('pointerPeek 328 ${_chainArgs(c)}');
    if (hex == null) {
      linkStatus = 'timeout';
      return;
    }
    final raw = Pa9Crypto.hexToBytes(hex);
    if (raw == null || raw.length < 328) {
      _slotClaro[slot] = null;
      _slotEc[slot] = 0;
      party[slot].clear();
      return;
    }
    final dec = Pa9Crypto.decryptBlock(raw);
    if (dec == null) {
      _slotClaro[slot] = null;
      _slotEc[slot] = 0;
      party[slot].clear();
      return;
    }
    _slotClaro[slot] = dec.claro;
    final p = party[slot];
    final samePoke = (_slotEc[slot] == dec.ec && p.maxHp > 0 && p.level > 0);
    _slotEc[slot] = dec.ec;

    if (samePoke && ++_slotExtAge[slot] < 6) {
      p.species = dec.species;
      p.currentHp = dec.currentHp > p.maxHp ? p.maxHp : dec.currentHp;
      p.heldItem = dec.item;
      p.pp = dec.pp;
      p.nature = dec.nature;
      p.evTotal = dec.evTotal;
      p.isShiny = dec.isShiny;
      p.isMaxIv = dec.isMaxIv;
      p.isAlpha = dec.isAlpha;
      p.active = true;
      p.name = dec.name;
      p.status = Pa9Crypto.statusName(dec.status);
    } else {
      _slotExtAge[slot] = 0;
      c[5] = 0x50;
      final extHex = await _sendAndWait('pointerPeek 16 ${_chainArgs(c)}');
      if (extHex == null) return;
      final extRaw = Pa9Crypto.hexToBytes(extHex);
      if (extRaw == null || extRaw.length < 16) {
        party[slot].clear();
        return;
      }
      Pa9Crypto.xorCrypt(extRaw, 16, dec.ec);
      _slotExtra[slot] = extRaw;
      final lv = extRaw[0];
      final hpMax = Pa9Crypto.rd16(extRaw, 2);
      if (hpMax == 0 || hpMax > 9999 || lv == 0 || lv > 100) {
        party[slot].clear();
      } else {
        p.species = dec.species;
        p.maxHp = hpMax;
        p.currentHp = dec.currentHp > hpMax ? hpMax : dec.currentHp;
        p.level = lv;
        p.heldItem = dec.item;
        p.pp = dec.pp;
        p.nature = dec.nature;
        p.evTotal = dec.evTotal;
        p.isShiny = dec.isShiny;
        p.isMaxIv = dec.isMaxIv;
        p.isAlpha = dec.isAlpha;
        p.active = true;
        p.name = dec.name;
        p.status = Pa9Crypto.statusName(dec.status);
      }
    }
    linkStatus = 'ok (lendo RAM)';
  }

  Future<void> _pollPosition() async {
    final hex = await _sendAndWait('pointerPeek 12 ${_chainArgs(_posBase)}');
    if (hex == null) return;
    final raw = Pa9Crypto.hexToBytes(hex);
    if (raw == null || raw.length < 12) return;
    final bd = ByteData.sublistView(raw);
    posX = bd.getFloat32(0, Endian.little);
    posAlt = bd.getFloat32(4, Endian.little);
    posY = bd.getFloat32(8, Endian.little);
    zoneName = lumioseZoneName(posX, posY, posAlt);
    if (trail.isEmpty ||
        (trail.last.dx - posX).abs() > 4 ||
        (trail.last.dy - posY).abs() > 4) {
      trail.add(Offset(posX, posY));
      if (trail.length > 18) trail.removeAt(0);
    }
  }

  Future<void> _pollShinyStash() async {
    final hex = await _sendAndWait('pointerPeek 328 ${_chainArgs(_stashBase)}');
    if (hex == null) return;
    final raw = Pa9Crypto.hexToBytes(hex);
    if (raw == null || raw.length < 328) {
      shinyStashFound = false;
      shinyStashSpecies = 0;
      shinyStashName = '';
      return;
    }
    final dec = Pa9Crypto.decryptBlock(raw);
    if (dec != null) {
      shinyStashFound = true;
      shinyStashSpecies = dec.species;
      shinyStashName = dec.name;
    } else {
      shinyStashFound = false;
      shinyStashSpecies = 0;
      shinyStashName = '';
    }
  }

  void _checkShinyAndAutoHeal() {
    final shinyCnt = party.where((p) => p.active && p.isShiny).length;
    final newShiny = (shinyCnt > _prevShinyCount) || (shinyStashFound && !_prevStash);
    _prevShinyCount = shinyCnt;
    _prevStash = shinyStashFound;

    if (newShiny) {
      _triggerShinyAlert();
    }

    if (autoHeal &&
        DateTime.now().difference(_lastAutoHeal).inSeconds >= 3) {
      for (int i = 0; i < 6; i++) {
        final p = party[i];
        if (p.active && p.maxHp > 0 && p.currentHp < p.maxHp * 0.5) {
          _lastAutoHeal = DateTime.now();
          editPokemon(i, PokeEditAction.healHp);
          break;
        }
      }
    }
  }

  void _triggerShinyAlert() {
    final now = DateTime.now();
    if (now.difference(_lastShinyAlert).inSeconds < 3) return;
    _lastShinyAlert = now;
    if (autoStopShiny && activeMacroSlot >= 0) {
      stopAllMacros();
      lastLog = 'AUTO-STOP: Shiny detectado!';
    }
    if (soundEnabled) {
      SystemSound.play(SystemSoundType.alert);
    }
    HapticFeedback.heavyImpact();
  }

  Future<void> triggerTestShiny() async {
    testShinyActive = true;
    if (autoStopShiny && activeMacroSlot >= 0) {
      stopAllMacros();
    }
    lastLog = 'Teste Shiny (5s): Alerta + AutoStop';
    if (soundEnabled) SystemSound.play(SystemSoundType.alert);
    HapticFeedback.heavyImpact();
    notifyListeners();
    await Future.delayed(const Duration(seconds: 5));
    testShinyActive = false;
    notifyListeners();
  }

  static String lumioseZoneName(double x, double zMap, double altY) {
    if (altY < -8.0) return 'Lumiose (Subsolo)';
    final nx = 0.50 + (x - (-830.0)) / 980.0;
    final ny = 0.50 - (zMap - (-500.0)) / 980.0;
    int bestIdx = -1;
    double bestD2 = 999.0;
    for (int i = 0; i < wildZones.length; i++) {
      final dx = nx - wildZones[i].nx;
      final dy = ny - wildZones[i].ny;
      final d2 = dx * dx + dy * dy;
      if (d2 < bestD2) {
        bestD2 = d2;
        bestIdx = i;
      }
    }
    if (bestIdx >= 0 && bestD2 <= wildZones[bestIdx].r * wildZones[bestIdx].r) {
      return wildZones[bestIdx].label;
    }
    if (altY > 14.0) return 'Lumiose (Telhados)';
    if (ny < 0.35) {
      if (nx < 0.38) return 'Distrito Bleu / NO';
      if (nx > 0.62) return 'Distrito Jaune / NE';
      return 'Distrito Rouge / Norte';
    }
    if (ny > 0.65) {
      if (nx < 0.38) return 'Distrito Magenta / SO';
      if (nx > 0.62) return 'Bulevar Sul / SE';
      return 'Distrito Vert / Sul';
    }
    return nx < 0.50 ? 'Lumiose Oeste' : 'Lumiose Leste';
  }

  Future<void> teleportPlayer(double wx, double wyAlt, double wzMap) async {
    posX = wx;
    posAlt = wyAlt;
    posY = wzMap;
    zoneName = lumioseZoneName(wx, wzMap, wyAlt);
    final bd = ByteData(12);
    bd.setFloat32(0, wx, Endian.little);
    bd.setFloat32(4, wyAlt, Endian.little);
    bd.setFloat32(8, wzMap, Endian.little);
    final hex = Pa9Crypto.bytesToHex(bd.buffer.asUint8List());
    _sendFireAndForget('pointerPoke 0x$hex ${_chainArgs(_posBase)}');
    lastLog = 'Teleporte enviado!';
    notifyListeners();
  }

  void saveFavWarp(int idx, String name) {
    if (idx < 0 || idx >= 3) return;
    favWarps[idx] = FavWarp(name: name, x: posX, y: posAlt, z: posY, valid: true);
    _savePrefs();
    notifyListeners();
  }

  Future<void> editPokemon(int slot, PokeEditAction action) async {
    if (action == PokeEditAction.healPartyAll) {
      for (int i = 0; i < 6; i++) {
        await editPokemon(i, PokeEditAction.fullRestore);
      }
      lastLog = 'Time inteiro curado (RAM)';
      notifyListeners();
      return;
    }
    if (slot < 0 || slot >= 6) return;
    final p = party[slot];
    final claro = _slotClaro[slot];
    if (p.maxHp == 0 || claro == null) return;

    bool pokeExtra = false;
    if (action == PokeEditAction.healHp || action == PokeEditAction.fullRestore) {
      p.currentHp = p.maxHp;
      Pa9Crypto.wr16(claro, 0x8A, p.maxHp);
    }
    if (action == PokeEditAction.healStatus || action == PokeEditAction.fullRestore) {
      p.status = '';
      Pa9Crypto.wr32(claro, 0x90, 0);
    }
    if (action == PokeEditAction.restorePp || action == PokeEditAction.fullRestore) {
      for (int m = 0; m < 4; m++) {
        p.pp[m] = 40;
        if (Pa9Crypto.rd16(claro, 0x72 + m * 2) != 0) claro[0x7A + m] = 40;
      }
    }
    if (action == PokeEditAction.toggleShiny) {
      p.isShiny = !p.isShiny;
      final tid = Pa9Crypto.rd16(claro, 0x0C);
      final sid = Pa9Crypto.rd16(claro, 0x0E);
      final pid = Pa9Crypto.rd32(claro, 0x1C);
      final pLo = pid & 0xFFFF;
      final pHi = p.isShiny ? (tid ^ sid ^ pLo) : (tid ^ sid ^ pLo ^ 0x0100);
      Pa9Crypto.wr32(claro, 0x1C, ((pHi & 0xFFFF) << 16) | pLo);
    }
    if (action == PokeEditAction.maxIvs) {
      p.isMaxIv = true;
      final iv32 = Pa9Crypto.rd32(claro, 0x8C);
      Pa9Crypto.wr32(claro, 0x8C, (iv32 & 0xC0000000) | 0x3FFFFFFF);
    }
    if (action == PokeEditAction.levelPlus1 ||
        action == PokeEditAction.levelPlus10 ||
        action == PokeEditAction.level100) {
      final oldLv = p.level > 0 ? p.level : 1;
      final newLv = action == PokeEditAction.levelPlus1
          ? min(oldLv + 1, 100)
          : action == PokeEditAction.levelPlus10
              ? min(oldLv + 10, 100)
              : 100;
      var newMax = (p.maxHp * newLv) ~/ oldLv;
      if (newMax <= p.maxHp && newLv > oldLv) {
        newMax = p.maxHp + (newLv - oldLv) * 2;
      }
      p.level = newLv;
      p.maxHp = newMax;
      p.currentHp = newMax;
      Pa9Crypto.wr32(claro, 0x10, newLv * newLv * newLv);
      Pa9Crypto.wr16(claro, 0x8A, newMax);
      final ext = _slotExtra[slot];
      if (ext != null) {
        ext[0] = newLv;
        Pa9Crypto.wr16(ext, 2, newMax);
        pokeExtra = true;
      }
    }
    if (action == PokeEditAction.evAtkSpe) {
      p.evTotal = 510;
      claro.setRange(0x26, 0x2C, [6, 252, 0, 252, 0, 0]);
    } else if (action == PokeEditAction.evSpaSpe) {
      p.evTotal = 510;
      claro.setRange(0x26, 0x2C, [6, 0, 0, 252, 252, 0]);
    } else if (action == PokeEditAction.evHpTank) {
      p.evTotal = 510;
      claro.setRange(0x26, 0x2C, [252, 0, 128, 0, 0, 128]);
    } else if (action == PokeEditAction.evReset) {
      p.evTotal = 0;
      claro.setRange(0x26, 0x2C, [0, 0, 0, 0, 0, 0]);
    }
    if (action == PokeEditAction.toggleAlpha) {
      p.isAlpha = !p.isAlpha;
      claro[0x23] = p.isAlpha ? 1 : 0;
      claro[0x4A] = p.isAlpha ? 255 : 128;
      claro[0x4B] = p.isAlpha ? 255 : 128;
    }

    final enc = Pa9Crypto.encryptBlock(Uint8List.fromList(claro));
    final c = List<int>.from(_partyBase);
    c[3] = _partyBase[3] + slot * 8;
    c[5] = 0x30;
    _sendFireAndForget('pointerPoke 0x${Pa9Crypto.bytesToHex(enc)} ${_chainArgs(c)}');

    if (pokeExtra && _slotExtra[slot] != null) {
      final rawExt = Uint8List.fromList(_slotExtra[slot]!);
      Pa9Crypto.xorCrypt(rawExt, 16, _slotEc[slot]);
      c[5] = 0x50;
      _sendFireAndForget('pointerPoke 0x${Pa9Crypto.bytesToHex(rawExt)} ${_chainArgs(c)}');
    }
    lastLog = '${p.name} atualizado (RAM)';
    notifyListeners();
  }

  // ---- Wi-Fi Macros via sys-botbase ----
  void triggerMacro(int slot) {
    if (activeMacroSlot == slot) {
      stopAllMacros();
      return;
    }
    stopAllMacros();
    macroExecutions++;

    // Instant one-shot macros
    switch (slot) {
      case 0: // Parear L+R
        _sendFireAndForget('click L');
        _sendFireAndForget('click R');
        lastLog = 'Macro: Parear L+R';
        notifyListeners();
        return;
      case 1: // Combo Esq+Y
        _sendFireAndForget('click DLEFT');
        Future.delayed(const Duration(milliseconds: 80), () => _sendFireAndForget('click Y'));
        lastLog = 'Macro: Combo Esq+Y';
        notifyListeners();
        return;
      case 4: // Mapa (+)
        _sendFireAndForget('click PLUS');
        lastLog = 'Macro: Mapa (+)';
        notifyListeners();
        return;
      case 5: // Cancelar 2xB
        _sendFireAndForget('click B');
        Future.delayed(const Duration(milliseconds: 120), () => _sendFireAndForget('click B'));
        lastLog = 'Macro: Cancelar 2xB';
        notifyListeners();
        return;
      case 6: // Auto-Catch (ZL+ZR)
        _sendFireAndForget('click ZL');
        Future.delayed(const Duration(milliseconds: 90), () => _sendFireAndForget('click ZR'));
        lastLog = 'Macro: Auto-Catch';
        notifyListeners();
        return;
      case 7: // Esquiva
        _sendFireAndForget('click Y');
        lastLog = 'Macro: Esquiva';
        notifyListeners();
        return;
      case 12: // Menu (X)
        _sendFireAndForget('click X');
        lastLog = 'Macro: Menu (X)';
        notifyListeners();
        return;
      case 13: // Home
        _sendFireAndForget('click HOME');
        lastLog = 'Macro: Home';
        notifyListeners();
        return;
      case 14: // Reset Cam
        _sendFireAndForget('click L');
        lastLog = 'Macro: Reset Cam';
        notifyListeners();
        return;
      case 15: // Curar HP
        editPokemon(0, PokeEditAction.healPartyAll);
        return;
      case 16: // Curar Status
        for (int i = 0; i < 6; i++) {
          editPokemon(i, PokeEditAction.healStatus);
        }
        return;
      case 20: // FireRed Soft Reset (A+B+Start+Select)
        _sendFireAndForget('press A');
        _sendFireAndForget('press B');
        _sendFireAndForget('press PLUS');
        _sendFireAndForget('press MINUS');
        Future.delayed(const Duration(milliseconds: 300), () {
          _sendFireAndForget('release A');
          _sendFireAndForget('release B');
          _sendFireAndForget('release PLUS');
          _sendFireAndForget('release MINUS');
        });
        lastLog = 'Macro: Soft Reset';
        notifyListeners();
        return;
      case 40: // BotW: Runa / Amiibo (L)
        _sendFireAndForget('click L');
        lastLog = 'BotW: Runa / Amiibo (L)';
        notifyListeners();
        return;
      case 41: // BotW: Inventario (+)
        _sendFireAndForget('click PLUS');
        lastLog = 'BotW: Inventario (+)';
        notifyListeners();
        return;
      case 42: // BotW: Mapa Sheikah (-)
        _sendFireAndForget('click MINUS');
        lastLog = 'BotW: Mapa (-)';
        notifyListeners();
        return;
      case 43: // BotW: Scope (R3)
        _sendFireAndForget('click RSTICK');
        lastLog = 'BotW: Scope (R3)';
        notifyListeners();
        return;
      case 44: // BotW: Quick Save (+ -> R -> A)
        _sendFireAndForget('click PLUS');
        Future.delayed(const Duration(milliseconds: 280), () => _sendFireAndForget('click R'));
        Future.delayed(const Duration(milliseconds: 450), () => _sendFireAndForget('click A'));
        lastLog = 'BotW: Quick Save';
        notifyListeners();
        return;
    }

    // Continuous toggle macros
    activeMacroSlot = slot;
    lastLog = 'Macro ativa (#$slot)';
    notifyListeners();

    if (slot == 2 || slot == 23) {
      // Turbo A / Spam A
      _macroTimer = Timer.periodic(const Duration(milliseconds: 120), (_) {
        _sendFireAndForget('click A');
      });
    } else if (slot == 8) {
      // Spam Y
      _macroTimer = Timer.periodic(const Duration(milliseconds: 140), (_) {
        _sendFireAndForget('click Y');
      });
    } else if (slot == 9 || slot == 24) {
      // Spam B
      _macroTimer = Timer.periodic(const Duration(milliseconds: 140), (_) {
        _sendFireAndForget('click B');
      });
    } else if (slot == 3) {
      // AFK Circulo
      _macroTimer = Timer.periodic(const Duration(milliseconds: 120), (_) {
        _circleAngle += 0.35;
        final x = (cos(_circleAngle) * 32767).round();
        final y = (sin(_circleAngle) * 32767).round();
        _sendFireAndForget('setStick LEFT $x $y');
      });
    } else if (slot == 10) {
      // Andar reto
      _sendFireAndForget('setStick LEFT 0 32767');
    } else if (slot == 11) {
      // Correr
      _sendFireAndForget('setStick LEFT 0 32767');
      _sendFireAndForget('press B');
    } else if (slot == 22) {
      // FireRed Grama <->
      int dir = 1;
      _macroTimer = Timer.periodic(const Duration(milliseconds: 450), (_) {
        dir = -dir;
        _sendFireAndForget('setStick LEFT ${dir * 32767} 0');
      });
    } else if (slot == 45) {
      // BotW: Whistle Sprint (segura DDOWN + spam B + anda pra frente)
      _sendFireAndForget('press DDOWN');
      _sendFireAndForget('setStick LEFT 0 32767');
      _macroTimer = Timer.periodic(const Duration(milliseconds: 110), (_) {
        _sendFireAndForget('click B');
      });
    } else if (slot == 46) {
      // BotW: Spam A (Loot / Cozinhar)
      _macroTimer = Timer.periodic(const Duration(milliseconds: 120), (_) {
        _sendFireAndForget('click A');
      });
    } else if (slot == 47) {
      // BotW: Amiibo Loop (L a cada 1.5s)
      _macroTimer = Timer.periodic(const Duration(milliseconds: 1500), (_) {
        _sendFireAndForget('click L');
      });
    }
  }

  void sendButtonClick(String btn) {
    _sendFireAndForget('click $btn');
    lastLog = 'Botao: $btn';
    notifyListeners();
  }

  Future<void> openTeslaOverlay() async {
    _sendFireAndForget('press L');
    _sendFireAndForget('press DDOWN');
    _sendFireAndForget('press RSTICK');
    await Future.delayed(const Duration(milliseconds: 180));
    _sendFireAndForget('release L');
    _sendFireAndForget('release DDOWN');
    _sendFireAndForget('release RSTICK');
    lastLog = 'Tesla Overlay (L+▼+R3)';
    notifyListeners();
  }

  void stopAllMacros() {
    _macroTimer?.cancel();
    _macroTimer = null;
    if (activeMacroSlot >= 0) {
      _sendFireAndForget('setStick LEFT 0 0');
      _sendFireAndForget('release B');
      _sendFireAndForget('release DDOWN');
    }
    activeMacroSlot = -1;
    notifyListeners();
  }

  // ---- Subnet scanner ----
  Future<void> scanNetwork() async {
    if (scanning) return;
    scanning = true;
    scanProgress = 0;
    scanResult = 'Detectando sub-rede...';
    notifyListeners();

    String prefix = '';
    try {
      final ifaces = await NetworkInterface.list(type: InternetAddressType.IPv4);
      final candidates = <({int score, String prefix})>[];
      for (final iface in ifaces) {
        final name = iface.name.toLowerCase();
        if (name.startsWith('pdp_') ||
            name.startsWith('utun') ||
            name.startsWith('ipsec') ||
            name.startsWith('awdl') ||
            name.startsWith('llw')) {
          continue;
        }
        for (final addr in iface.addresses) {
          if (addr.isLoopback) continue;
          final ip = addr.address;
          if (ip.startsWith('192.0.0.') || ip.startsWith('169.254.')) continue;
          final parts = ip.split('.');
          if (parts.length != 4) continue;
          int score = 0;
          if (name == 'en0' || name == 'wlan0' || name == 'eth0') score += 100;
          if (ip.startsWith('192.168.')) {
            score += 50;
          } else if (ip.startsWith('10.')) {
            score += 40;
          } else if (ip.startsWith('172.')) {
            score += 30;
          }
          candidates.add((score: score, prefix: '${parts[0]}.${parts[1]}.${parts[2]}.'));
        }
      }
      if (candidates.isNotEmpty) {
        candidates.sort((a, b) => b.score.compareTo(a.score));
        prefix = candidates.first.prefix;
      }
    } catch (_) {}

    if (prefix.isEmpty) {
      final parts = switchIp.split('.');
      prefix = parts.length == 4 ? '${parts[0]}.${parts[1]}.${parts[2]}.' : '192.168.15.';
    }

    String? foundIp;
    for (int base = 1; base <= 254 && foundIp == null; base += 25) {
      final end = min(base + 24, 254);
      scanProgress = ((end * 100) / 254).round();
      scanResult = 'Buscando $prefix$base..$end ($scanProgress%)';
      notifyListeners();

      final futures = <Future<String?>>[];
      for (int h = base; h <= end; h++) {
        final ip = '$prefix$h';
        futures.add(() async {
          try {
            final s = await Socket.connect(
              ip,
              switchPort,
              timeout: const Duration(milliseconds: 450),
            );
            s.destroy();
            return ip;
          } catch (_) {
            return null;
          }
        }());
      }
      final results = await Future.wait(futures);
      for (final r in results) {
        if (r != null) {
          foundIp = r;
          break;
        }
      }
    }

    scanning = false;
    scanProgress = 100;
    if (foundIp != null) {
      scanResult = 'Switch encontrado: $foundIp';
      await updateTarget(foundIp, switchPort);
    } else {
      scanResult = 'Switch nao encontrado em $prefix*:$switchPort';
      Future.delayed(const Duration(seconds: 5), () {
        if (!connected && !scanning) connect();
      });
    }
    notifyListeners();
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}
