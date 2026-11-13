import 'dart:typed_data';

class Pa9Decoded {
  final Uint8List claro;
  final int ec;
  final int species;
  final int currentHp;
  final int status;
  final int item;
  final List<int> pp;
  final int nature;
  final int evTotal;
  final bool isShiny;
  final bool isMaxIv;
  final bool isAlpha;
  final String name;

  Pa9Decoded({
    required this.claro,
    required this.ec,
    required this.species,
    required this.currentHp,
    required this.status,
    required this.item,
    required this.pp,
    required this.nature,
    required this.evTotal,
    required this.isShiny,
    required this.isMaxIv,
    required this.isAlpha,
    required this.name,
  });
}

class Pa9Crypto {
  static const List<int> _order = [
    0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1,
    1, 0, 2, 3, 1, 0, 3, 2, 2, 0, 1, 3, 3, 0, 1, 2, 2, 0, 3, 1, 3, 0, 2, 1,
    1, 2, 0, 3, 1, 3, 0, 2, 2, 1, 0, 3, 3, 1, 0, 2, 2, 3, 0, 1, 3, 2, 0, 1,
    1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1, 0,
    0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1,
    1, 0, 2, 3, 1, 0, 3, 2,
  ];

  static int rd16(Uint8List p, int off) => p[off] | (p[off + 1] << 8);

  static int rd32(Uint8List p, int off) =>
      p[off] | (p[off + 1] << 8) | (p[off + 2] << 16) | (p[off + 3] << 24);

  static void wr16(Uint8List p, int off, int v) {
    p[off] = v & 0xFF;
    p[off + 1] = (v >> 8) & 0xFF;
  }

  static void wr32(Uint8List p, int off, int v) {
    p[off] = v & 0xFF;
    p[off + 1] = (v >> 8) & 0xFF;
    p[off + 2] = (v >> 16) & 0xFF;
    p[off + 3] = (v >> 24) & 0xFF;
  }

  static void xorCrypt(Uint8List data, int len, int seed) {
    var s = seed & 0xFFFFFFFF;
    for (int i = 0; i + 1 < len; i += 2) {
      s = (s * 0x41C64E6D + 0x6073) & 0xFFFFFFFF;
      final k = (s >> 16) & 0xFFFF;
      data[i] ^= k & 0xFF;
      data[i + 1] ^= (k >> 8) & 0xFF;
    }
  }

  static Pa9Decoded? decryptBlock(Uint8List raw) {
    if (raw.length < 328) return null;
    final ec = rd32(raw, 0);
    final sanity = rd16(raw, 4);
    final chkExpect = rd16(raw, 6);
    if ((ec == 0 && chkExpect == 0) || sanity != 0) return null;

    final body = Uint8List.fromList(raw.sublist(8, 328));
    xorCrypt(body, 320, ec);

    int sum = 0;
    for (int i = 0; i < 320; i += 2) {
      sum = (sum + rd16(body, i)) & 0xFFFF;
    }
    if (sum != chkExpect) return null;

    final sv = (ec >> 13) & 31;
    final claro = Uint8List(328);
    claro.setRange(0, 8, raw, 0);
    for (int b = 0; b < 4; b++) {
      final srcBlk = _order[sv * 4 + b];
      claro.setRange(8 + b * 80, 8 + (b + 1) * 80, body, srcBlk * 80);
    }

    final species = rd16(claro, 0x08);
    if (species == 0) return null;

    final hpCur = rd16(claro, 0x8A);
    final status = claro[0x90];
    final item = rd16(claro, 0x0A);
    final pp = [claro[0x7A], claro[0x7B], claro[0x7C], claro[0x7D]];
    final nature = claro[0x21];
    int evTot = 0;
    for (int e = 0; e < 6; e++) {
      evTot += claro[0x26 + e];
    }

    final tid = rd16(claro, 0x0C);
    final sid = rd16(claro, 0x0E);
    final pid = rd32(claro, 0x1C);
    final pLo = pid & 0xFFFF;
    final pHi = (pid >> 16) & 0xFFFF;
    final isShiny = (tid ^ sid ^ pLo ^ pHi) < 16;

    final iv32 = rd32(claro, 0x8C);
    final isMaxIv = (iv32 & 0x3FFFFFFF) == 0x3FFFFFFF;
    final isAlpha = claro[0x23] != 0 || claro[0x4A] >= 250;

    final buf = StringBuffer();
    for (int i = 0; i < 13; i++) {
      final ch = rd16(claro, 0x58 + i * 2);
      if (ch == 0) break;
      if (ch >= 0x20 && ch <= 0x7E) buf.writeCharCode(ch);
    }
    final name = buf.isEmpty ? '#$species' : buf.toString();

    return Pa9Decoded(
      claro: claro,
      ec: ec,
      species: species,
      currentHp: hpCur,
      status: status,
      item: item,
      pp: pp,
      nature: nature,
      evTotal: evTot,
      isShiny: isShiny,
      isMaxIv: isMaxIv,
      isAlpha: isAlpha,
      name: name,
    );
  }

  static Uint8List encryptBlock(Uint8List claro) {
    int sum = 0;
    for (int i = 8; i < 328; i += 2) {
      sum = (sum + rd16(claro, i)) & 0xFFFF;
    }
    wr16(claro, 4, 0);
    wr16(claro, 6, sum);

    final ec = rd32(claro, 0);
    final sv = (ec >> 13) & 31;
    final body = Uint8List(320);
    for (int b = 0; b < 4; b++) {
      final dstBlk = _order[sv * 4 + b];
      body.setRange(dstBlk * 80, (dstBlk + 1) * 80, claro, 8 + b * 80);
    }
    xorCrypt(body, 320, ec);

    final rawOut = Uint8List(328);
    rawOut.setRange(0, 8, claro, 0);
    rawOut.setRange(8, 328, body, 0);
    return rawOut;
  }

  static String statusName(int s) {
    switch (s) {
      case 1:
        return 'SLP';
      case 2:
        return 'PSN';
      case 3:
        return 'BRN';
      case 4:
        return 'FRZ';
      case 5:
        return 'PAR';
      case 6:
        return 'CNF';
      default:
        return '';
    }
  }

  static Uint8List? hexToBytes(String hex) {
    final clean = hex.trim();
    if (clean.length < 2) return null;
    final len = clean.length ~/ 2;
    final out = Uint8List(len);
    for (int i = 0; i < len; i++) {
      final byte = int.tryParse(clean.substring(i * 2, i * 2 + 2), radix: 16);
      if (byte == null) return null;
      out[i] = byte;
    }
    return out;
  }

  static String bytesToHex(Uint8List bytes) {
    final sb = StringBuffer();
    for (final b in bytes) {
      sb.write(b.toRadixString(16).padLeft(2, '0').toUpperCase());
    }
    return sb.toString();
  }
}
