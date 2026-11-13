import 'package:flutter/material.dart';

enum GameProfile { pokemonZa, pokemonFr }

enum PokeEditAction {
  healHp,
  healStatus,
  restorePp,
  fullRestore,
  toggleShiny,
  maxIvs,
  levelPlus1,
  levelPlus10,
  level100,
  evAtkSpe,
  evSpaSpe,
  evHpTank,
  evReset,
  toggleAlpha,
  healPartyAll,
}

class PokemonSlot {
  int species = 0;
  String name = '';
  int level = 0;
  int currentHp = 0;
  int maxHp = 0;
  String status = '';
  int heldItem = 0;
  List<int> pp = [0, 0, 0, 0];
  int nature = 0;
  int evTotal = 0;
  bool isShiny = false;
  bool isMaxIv = false;
  bool isAlpha = false;
  bool active = false;

  void clear() {
    species = 0;
    name = '';
    level = 0;
    currentHp = 0;
    maxHp = 0;
    status = '';
    heldItem = 0;
    pp = [0, 0, 0, 0];
    nature = 0;
    evTotal = 0;
    isShiny = false;
    isMaxIv = false;
    isAlpha = false;
    active = false;
  }

  PokemonSlot copy() {
    final c = PokemonSlot();
    c.species = species;
    c.name = name;
    c.level = level;
    c.currentHp = currentHp;
    c.maxHp = maxHp;
    c.status = status;
    c.heldItem = heldItem;
    c.pp = List<int>.from(pp);
    c.nature = nature;
    c.evTotal = evTotal;
    c.isShiny = isShiny;
    c.isMaxIv = isMaxIv;
    c.isAlpha = isAlpha;
    c.active = active;
    return c;
  }
}

class FavWarp {
  final String name;
  final double x;
  final double y;
  final double z;
  final bool valid;

  const FavWarp({
    this.name = '',
    this.x = 0,
    this.y = 0,
    this.z = 0,
    this.valid = false,
  });

  Map<String, dynamic> toJson() => {
        'name': name,
        'x': x,
        'y': y,
        'z': z,
        'valid': valid,
      };

  factory FavWarp.fromJson(Map<String, dynamic> m) => FavWarp(
        name: (m['name'] ?? '') as String,
        x: (m['x'] as num?)?.toDouble() ?? 0,
        y: (m['y'] as num?)?.toDouble() ?? 0,
        z: (m['z'] as num?)?.toDouble() ?? 0,
        valid: (m['valid'] as bool?) ?? false,
      );
}

class HudThemeConfig {
  final int id;
  final String name;
  final bool isSidebar;
  final Color bg;
  final Color topBg;
  final Color barBg;
  final Color cardBg;
  final Color cardBorder;
  final Color accent;
  final Color titleColor;
  final Color tabActiveBg;
  final double cardRadius;

  const HudThemeConfig({
    required this.id,
    required this.name,
    required this.isSidebar,
    required this.bg,
    required this.topBg,
    required this.barBg,
    required this.cardBg,
    required this.cardBorder,
    required this.accent,
    required this.titleColor,
    required this.tabActiveBg,
    required this.cardRadius,
  });

  static const List<HudThemeConfig> themes = [
    HudThemeConfig(
      id: 0,
      name: 'Tatico',
      isSidebar: true,
      bg: Color(0xFF111626),
      topBg: Color(0xFF0B0F1A),
      barBg: Color(0xFF0B0F1A),
      cardBg: Color(0xFF171E34),
      cardBorder: Color(0xFF2A385C),
      accent: Color(0xFF00F5D4),
      titleColor: Color(0xFF00F5D4),
      tabActiveBg: Color(0xFF16223D),
      cardRadius: 8,
    ),
    HudThemeConfig(
      id: 1,
      name: 'Lumiose',
      isSidebar: false,
      bg: Color(0xFF0F1117),
      topBg: Color(0xFF0B0D14),
      barBg: Color(0xFF131722),
      cardBg: Color(0xFF161A26),
      cardBorder: Color(0xFFFFD54F),
      accent: Color(0xFF00E676),
      titleColor: Color(0xFFFFD54F),
      tabActiveBg: Color(0xFF1A2233),
      cardRadius: 10,
    ),
    HudThemeConfig(
      id: 2,
      name: 'Rotom',
      isSidebar: false,
      bg: Color(0xFF070D1A),
      topBg: Color(0xFF1A0B16),
      barBg: Color(0xFF120E20),
      cardBg: Color(0xFF0E172B),
      cardBorder: Color(0xFF00E5FF),
      accent: Color(0xFF00E5FF),
      titleColor: Color(0xFFFF3B5C),
      tabActiveBg: Color(0xFF132238),
      cardRadius: 6,
    ),
    HudThemeConfig(
      id: 3,
      name: 'OLED',
      isSidebar: false,
      bg: Color(0xFF08090D),
      topBg: Color(0xFF08090D),
      barBg: Color(0xFF0E1017),
      cardBg: Color(0xFF131620),
      cardBorder: Color(0xFF222738),
      accent: Color(0xFFFFFFFF),
      titleColor: Color(0xFFFFFFFF),
      tabActiveBg: Color(0xFF171A26),
      cardRadius: 8,
    ),
  ];

  static HudThemeConfig byIndex(int idx) =>
      themes[((idx % themes.length) + themes.length) % themes.length];
}
