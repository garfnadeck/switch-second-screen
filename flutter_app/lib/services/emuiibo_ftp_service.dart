import 'dart:async';
import 'dart:convert';
import 'dart:io';
import '../models/game_state.dart';

class AmiiboCatalogItem {
  final String name;
  final String gameSeries;
  final String head;
  final String tail;
  final String image;

  const AmiiboCatalogItem({
    required this.name,
    required this.gameSeries,
    required this.head,
    required this.tail,
    this.image = '',
  });
}

class EmuiiboFtpService {
  static const List<AmiiboCatalogItem> botwCatalog = [
    AmiiboCatalogItem(
      name: 'Midna & Wolf Link',
      gameSeries: 'BotW: Invoca Wolf Link (aliado)',
      head: '01030000',
      tail: '024f0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01030000-024f0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Archer',
      gameSeries: 'BotW: Traveler Bow + Flechas',
      head: '01000000',
      tail: '03530902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-03530902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Rider',
      gameSeries: 'BotW: Traveler Bridle/Saddle + Espada',
      head: '01000000',
      tail: '03540902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-03540902.png',
    ),
    AmiiboCatalogItem(
      name: 'Zelda (BotW)',
      gameSeries: 'BotW: Radiant Shield + Gemas/Ervas',
      head: '01010000',
      tail: '03560902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01010000-03560902.png',
    ),
    AmiiboCatalogItem(
      name: 'Guardian',
      gameSeries: 'BotW: Pecas Ancestrais + Armas Guardian',
      head: '01400000',
      tail: '03550902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01400000-03550902.png',
    ),
    AmiiboCatalogItem(
      name: 'Bokoblin',
      gameSeries: 'BotW: Armas Boko + Carnes',
      head: '01410000',
      tail: '035c0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01410000-035c0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Mipha',
      gameSeries: 'BotW: Vah Ruta Divine Helm + Zora Spear',
      head: '01070000',
      tail: '035a0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01070000-035a0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Daruk',
      gameSeries: 'BotW: Vah Rudania Divine Helm + Cobble Crusher',
      head: '01050000',
      tail: '03580902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01050000-03580902.png',
    ),
    AmiiboCatalogItem(
      name: 'Revali',
      gameSeries: 'BotW: Vah Medoh Divine Helm + Falcon Bow',
      head: '01080000',
      tail: '035b0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01080000-035b0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Urbosa',
      gameSeries: 'BotW: Vah Naboris Divine Helm + Scimitar',
      head: '01060000',
      tail: '03590902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01060000-03590902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Ocarina of Time',
      gameSeries: 'BotW: Set of Time + Biggoron Sword',
      head: '01000000',
      tail: '034b0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-034b0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Majoras Mask',
      gameSeries: 'BotW: Fierce Deity Set + Sword',
      head: '01000000',
      tail: '034c0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-034c0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Twilight Princess',
      gameSeries: 'BotW: Epona + Set of Twilight',
      head: '01000000',
      tail: '034d0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-034d0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Skyward Sword',
      gameSeries: 'BotW: Set of the Sky + Goddess Sword',
      head: '01000000',
      tail: '034e0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-034e0902.png',
    ),
    AmiiboCatalogItem(
      name: '8-Bit Link',
      gameSeries: 'BotW: Set of the Hero + Sword',
      head: '01000000',
      tail: '034f0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-034f0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Toon Link - Wind Waker',
      gameSeries: 'BotW: Set of the Wind + Sea-Breeze Boomerang',
      head: '01000100',
      tail: '03500902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000100-03500902.png',
    ),
    AmiiboCatalogItem(
      name: 'Toon Zelda - Wind Waker',
      gameSeries: 'BotW: Hero Shield + Gemas',
      head: '01010000',
      tail: '03520902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01010000-03520902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link (Smash Bros)',
      gameSeries: 'BotW: Epona + Set of Twilight',
      head: '01000000',
      tail: '00040002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-00040002.png',
    ),
    AmiiboCatalogItem(
      name: 'Zelda (Smash Bros)',
      gameSeries: 'BotW: Twilight Bow + Gemas',
      head: '01010000',
      tail: '000e0002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01010000-000e0002.png',
    ),
    AmiiboCatalogItem(
      name: 'Sheik (Smash Bros)',
      gameSeries: 'BotW: Sheik Mask + Armas Sheikah',
      head: '01010100',
      tail: '00170002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01010100-00170002.png',
    ),
    AmiiboCatalogItem(
      name: 'Ganondorf (Smash Bros)',
      gameSeries: 'BotW: Sword of the Six Sages',
      head: '01020100',
      tail: '001b0002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01020100-001b0002.png',
    ),
    AmiiboCatalogItem(
      name: 'Toon Link (Smash Bros)',
      gameSeries: 'BotW: Set of the Wind + Boomerang',
      head: '01000100',
      tail: '00160002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000100-00160002.png',
    ),
    AmiiboCatalogItem(
      name: 'Young Link (Smash Bros)',
      gameSeries: 'BotW: Armas + Peixes/Ervas',
      head: '01000000',
      tail: '037c0002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-037c0002.png',
    ),
    AmiiboCatalogItem(
      name: 'Link - Links Awakening',
      gameSeries: 'BotW: Armas + Frutas/Baú',
      head: '01000000',
      tail: '03990902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-03990902.png',
    ),
  ];

  static List<AmiiboCatalogItem> catalogForGame(GameProfile profile) {
    if (profile == GameProfile.zeldaBotw) return botwCatalog;
    return quickCatalog;
  }

  static const List<AmiiboCatalogItem> quickCatalog = [
    AmiiboCatalogItem(
      name: 'Link (Tears of the Kingdom)',
      gameSeries: 'The Legend of Zelda',
      head: '01000000',
      tail: '04180902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-04180902.png',
    ),
    AmiiboCatalogItem(
      name: 'Zelda (Tears of the Kingdom)',
      gameSeries: 'The Legend of Zelda',
      head: '01010000',
      tail: '041a0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01010000-041a0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Ganondorf (TotK)',
      gameSeries: 'The Legend of Zelda',
      head: '01020000',
      tail: '041b0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01020000-041b0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Wolf Link',
      gameSeries: 'The Legend of Zelda',
      head: '01000200',
      tail: '00420902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000200-00420902.png',
    ),
    AmiiboCatalogItem(
      name: 'Guardian (BotW)',
      gameSeries: 'The Legend of Zelda',
      head: '01030000',
      tail: '03500902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01030000-03500902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link Archer (BotW)',
      gameSeries: 'The Legend of Zelda',
      head: '01000000',
      tail: '034e0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-034e0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Zelda (BotW)',
      gameSeries: 'The Legend of Zelda',
      head: '01010000',
      tail: '034f0902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01010000-034f0902.png',
    ),
    AmiiboCatalogItem(
      name: 'Link (Majora Mask)',
      gameSeries: 'The Legend of Zelda',
      head: '01000000',
      tail: '03570902',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_01000000-03570902.png',
    ),
    AmiiboCatalogItem(
      name: 'Mario (Smash)',
      gameSeries: 'Super Mario',
      head: '00000000',
      tail: '00000002',
      image: 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_00000000-00000002.png',
    ),
  ];

  static Future<List<AmiiboCatalogItem>> searchApi(String query) async {
    final q = query.trim();
    if (q.isEmpty) return quickCatalog;
    final client = HttpClient()..connectionTimeout = const Duration(seconds: 5);
    try {
      final uri = Uri.https('www.amiiboapi.com', '/api/amiibo/', {'name': q});
      final req = await client.getUrl(uri);
      final resp = await req.close();
      if (resp.statusCode != 200) return [];
      final body = await resp.transform(utf8.decoder).join();
      final json = jsonDecode(body) as Map<String, dynamic>;
      final list = (json['amiibo'] as List?) ?? [];
      return list.take(30).map((e) {
        final m = e as Map<String, dynamic>;
        return AmiiboCatalogItem(
          name: (m['name'] ?? '') as String,
          gameSeries: (m['gameSeries'] ?? '') as String,
          head: (m['head'] ?? '00000000') as String,
          tail: (m['tail'] ?? '00000002') as String,
          image: (m['image'] ?? '') as String,
        );
      }).toList();
    } catch (_) {
      return quickCatalog
          .where((a) => a.name.toLowerCase().contains(q.toLowerCase()))
          .toList();
    } finally {
      client.close();
    }
  }

  static String buildAmiiboJson(
    AmiiboCatalogItem item, {
    bool randomUuid = true,
  }) {
    final full = (item.head + item.tail).padRight(16, '0');
    final rawCharId = int.tryParse(full.substring(0, 4), radix: 16) ?? 0;
    final swappedCharId = ((rawCharId & 0xFF) << 8) | ((rawCharId >> 8) & 0xFF);
    final charVar = int.tryParse(full.substring(4, 6), radix: 16) ?? 0;
    final figType = int.tryParse(full.substring(6, 8), radix: 16) ?? 0;
    final modelNo = int.tryParse(full.substring(8, 12), radix: 16) ?? 0;
    final series = int.tryParse(full.substring(12, 14), radix: 16) ?? 0;
    final now = DateTime.now();

    final map = {
      'name': item.name.length > 10 ? item.name.substring(0, 10) : item.name,
      'write_counter': 0,
      'version': 0,
      'mii_charinfo_file': 'mii-charinfo.bin',
      'first_write_date': {'y': now.year, 'm': now.month, 'd': now.day},
      'last_write_date': {'y': now.year, 'm': now.month, 'd': now.day},
      'use_random_uuid': randomUuid,
      'id': {
        'game_character_id': swappedCharId,
        'character_variant': charVar,
        'figure_type': figType,
        'series': series,
        'model_number': modelNo,
      },
    };
    return const JsonEncoder.withIndent('  ').convert(map);
  }

  static String sanitizeFolder(String name) {
    final clean = name.replaceAll(RegExp(r'[^a-zA-Z0-9 _-]'), '').trim();
    return clean.isEmpty ? 'Amiibo' : clean;
  }

  // ---- Minimal Pure-Dart Passive FTP Client (sys-ftpd) ----
  static Future<T> _withFtp<T>(
    String host,
    int port,
    String user,
    String pass,
    Future<T> Function(_FtpConn ftp) fn,
  ) async {
    final ftp = _FtpConn();
    await ftp.connect(host, port, user: user, pass: pass);
    try {
      return await fn(ftp);
    } finally {
      await ftp.close();
    }
  }

  static Future<List<String>> listAmiibos(
    String host, {
    int port = 5000,
    String user = 'anonymous',
    String pass = 'anon@',
  }) {
    return _withFtp(host, port, user, pass, (ftp) async {
      await ftp.tryCmd('MKD /emuiibo');
      await ftp.tryCmd('MKD /emuiibo/amiibo');
      final raw = await ftp.dataText('NLST /emuiibo/amiibo');
      return raw
          .split(RegExp(r'\r?\n'))
          .map((e) => e.trim().split('/').last)
          .where((e) => e.isNotEmpty && e != '.' && e != '..')
          .toList();
    });
  }

  static Future<void> uploadVirtualAmiibo(
    String host,
    AmiiboCatalogItem item, {
    int port = 5000,
    String user = 'anonymous',
    String pass = 'anon@',
  }) {
    return _withFtp(host, port, user, pass, (ftp) async {
      final folder = sanitizeFolder(item.name);
      final base = '/emuiibo/amiibo/$folder';
      await ftp.tryCmd('MKD /emuiibo');
      await ftp.tryCmd('MKD /emuiibo/amiibo');
      await ftp.tryCmd('MKD $base');
      final jsonBytes = utf8.encode(buildAmiiboJson(item));
      await ftp.stor('$base/amiibo.json', jsonBytes);
      await ftp.stor('$base/amiibo.flag', const []);
    });
  }

  static Future<void> deleteAmiibo(
    String host,
    String folder, {
    int port = 5000,
    String user = 'anonymous',
    String pass = 'anon@',
  }) {
    return _withFtp(host, port, user, pass, (ftp) async {
      final base = '/emuiibo/amiibo/$folder';
      await ftp.tryCmd('DELE $base/amiibo.json');
      await ftp.tryCmd('DELE $base/amiibo.flag');
      await ftp.tryCmd('RMD $base');
    });
  }
}

class _FtpConn {
  Socket? _ctrl;
  final List<String> _lines = [];
  Completer<String>? _waiter;
  final StringBuffer _buf = StringBuffer();

  Future<void> connect(
    String host,
    int port, {
    required String user,
    required String pass,
  }) async {
    _ctrl = await Socket.connect(host, port, timeout: const Duration(seconds: 3));
    _ctrl!.listen((data) {
      _buf.write(utf8.decode(data, allowMalformed: true));
      while (true) {
        final s = _buf.toString();
        final idx = s.indexOf('\n');
        if (idx < 0) break;
        final line = s.substring(0, idx).replaceAll('\r', '');
        _buf.clear();
        _buf.write(s.substring(idx + 1));
        if (_waiter != null && !_waiter!.isCompleted) {
          if (line.length >= 4 &&
              RegExp(r'^\d{3} ').hasMatch(line)) {
            final w = _waiter!;
            _waiter = null;
            w.complete(line);
          }
        } else {
          _lines.add(line);
        }
      }
    });
    await _readReply(); // 220 welcome
    await cmd('USER $user');
    await cmd('PASS $pass');
    await cmd('TYPE I');
  }

  Future<String> _readReply() async {
    if (_lines.isNotEmpty) return _lines.removeAt(0);
    _waiter = Completer<String>();
    return _waiter!.future.timeout(const Duration(seconds: 5));
  }

  Future<String> cmd(String c) async {
    _ctrl!.write('$c\r\n');
    return _readReply();
  }

  Future<void> tryCmd(String c) async {
    try {
      await cmd(c);
    } catch (_) {}
  }

  Future<Socket> _openPasv() async {
    final resp = await cmd('PASV');
    final m = RegExp(r'\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\)').firstMatch(resp);
    if (m == null) throw Exception('PASV erro: $resp');
    final ip = '${m[1]}.${m[2]}.${m[3]}.${m[4]}';
    final port = (int.parse(m[5]!) << 8) + int.parse(m[6]!);
    return Socket.connect(ip, port, timeout: const Duration(seconds: 4));
  }

  Future<(ServerSocket, Future<Socket>)> _openPort() async {
    final localIp = _ctrl!.address.address;
    final srv = await ServerSocket.bind(InternetAddress.anyIPv4, 0);
    final p = srv.port;
    final ipParts = localIp.split('.');
    final r = await cmd('PORT ${ipParts.join(',')},${p >> 8},${p & 0xFF}');
    if (!r.startsWith('2')) {
      await srv.close();
      throw Exception('PORT erro: $r');
    }
    return (srv, srv.first.timeout(const Duration(seconds: 5)));
  }

  Future<String> dataText(String c) async {
    Socket? ds;
    ServerSocket? srv;
    Future<Socket>? acceptFut;
    try {
      ds = await _openPasv();
    } catch (_) {
      final res = await _openPort();
      srv = res.$1;
      acceptFut = res.$2;
    }

    final r = await cmd(c);
    if (!r.startsWith('1')) {
      await ds?.close();
      await srv?.close();
      return '';
    }
    ds ??= await acceptFut!;
    final chunks = <int>[];
    final done = Completer<void>();
    ds.listen(chunks.addAll, onDone: () {
      if (!done.isCompleted) done.complete();
    }, onError: (_) {
      if (!done.isCompleted) done.complete();
    });
    await done.future.timeout(const Duration(seconds: 5), onTimeout: () {});
    await srv?.close();
    await _readReply(); // 226
    return utf8.decode(chunks, allowMalformed: true);
  }

  Future<void> stor(String path, List<int> bytes) async {
    Socket? ds;
    ServerSocket? srv;
    Future<Socket>? acceptFut;
    try {
      ds = await _openPasv();
    } catch (_) {
      final res = await _openPort();
      srv = res.$1;
      acceptFut = res.$2;
    }

    _ctrl!.write('STOR $path\r\n');
    final r = await _readReply();
    if (!r.startsWith('1')) {
      await ds?.close();
      await srv?.close();
      throw Exception('STOR falhou: $r');
    }
    ds ??= await acceptFut!;
    ds.add(bytes);
    await ds.flush();
    await ds.close();
    await srv?.close();
    await _readReply(); // 226
  }

  Future<void> close() async {
    try {
      await tryCmd('QUIT');
      await _ctrl?.close();
    } catch (_) {}
  }
}
