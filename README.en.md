# SwitchHUD (NS Companion) — v1.1.1

> [🇧🇷 Português](README.md) | 🇺🇸 **English**

Real-time second-screen HUD and utility companion for **Nintendo Switch (Atmosphère + `sys-botbase`)**, built with **Flutter** for **iOS** and **Android**.

[![Release v1.1.1](https://img.shields.io/badge/Release-v1.1.1-00B4D8?style=for-the-badge)](https://github.com/garfnadeck/switch-second-screen/releases/tag/v1.1.1)

---

## Supported Games

### 1. The Legend of Zelda: Breath of the Wild (`v1.6.0` — BuildID `8E9978D50BDD20B4`)
- **Auto-Boost on Connect:** as soon as a loaded save is detected in RAM, automatically applies:
  - **30 Hearts** (`120` quarter-hearts)
  - **3 Stamina Wheels** (`3000.0`) — automatically restored when entering/exiting Shrines or fast-traveling
  - **999,999 Rupees**
  - **Max Pouch Slots (`20` Weapons / `14` Bows / `20` Shields)** via `SaveMgr::TriggerParam`
- **Inventory & Durability (`PauseMenuDataMgr`):**
  - **`+ Add Item`:** spawns new items directly into your pouch (including **Master Sword**, **Hylian Shield**, bows, shields, food, and materials)
  - **`Reparar Tudo (999)`:** restores full durability on all weapons, bows, and shields
  - **`999 Flechas` / `999 Materiais`:** maxes out arrow and material stacks
  - **`Trocar`:** converts any existing slot into another item ID
- **Armor Sets Tab (12 Sets):** adds new armor pieces without overwriting your existing collection and marks them equipped in the pouch (select a piece in the in-game `+` menu or Save & Load / Fast Travel to refresh Link's 3D model).
- **Live GPS Map:** tracks Link's live position with **Shrines**, **Towers**, **Koroks**, and custom **Pins**.
  > **Note on Teleport:** BotW's Havok physics engine overwrites Link's position every frame; therefore the map acts as a live GPS tracker while fast travel is done through the in-game (`-`) map.
- **Hyrule Progress:** automatic sync of Shrines, Koroks, and Divine Beasts.

### 2. Pokémon Legends: Z-A
- **Live Party HUD:** HP, status, level, IVs/EVs, and **Shiny** alerts with optional **Auto-Stop**.
- **Live Map:** **Lumiose**, **Lysandre**, and **Sewers** layers with real-time coordinate tracking.
- **Macros & Automation:** shortcuts for farming, catching, quick healing, and virtual controller input.

---

## General Features
- **Auto-Reconnect:** automatically reconnects when returning from background or if Wi-Fi/Switch drops.
- **Android Foreground Service:** keeps the TCP connection and Auto-Boost running in the background on Android with a persistent notification.
- **iOS:** iOS suspends background sockets after a few seconds by OS policy, and the app automatically reconnects as soon as you return to it.
- **Built-in FAQ & Credits:** step-by-step guide available right inside the **Config** tab.

---

## Quick Start

1. **Prepare your Nintendo Switch:**
   - Install [`sys-botbase`](https://github.com/olliz0r/sys-botbase) on your SD card under `atmosphere/contents/430000000000000B/`.
   - Connect your Switch and phone/tablet to the **same Wi-Fi network**.
2. **Install SwitchHUD:**
   - Download `app-release.apk` (Android) or `SwitchHUD-ios-unsigned.ipa` (iOS via AltStore/Sideloadly/Xcode) from **[Releases](https://github.com/garfnadeck/switch-second-screen/releases/tag/v1.1.1)**.
3. **Connect:**
   - Open the **Rede** tab, enter your Switch IP (default port `6000`) or tap **Buscar Rede**, and hit **Conectar**.

### What persists in `.sav`?
- **Saved to `.sav` when saving in-game:** Rupees, Armor, Weapons, Bows, Shields, and added/edited pouch items.
- **Re-applied by Auto-Boost on connect:** Max Hearts, Max Stamina (3 wheels), and Hestu Max Slots (`20/14/20`).

---

## Build from Source

```bash
cd flutter_app
flutter pub get
flutter analyze
flutter test

# Android APK
flutter build apk --release

# iOS (macOS + Xcode)
flutter build ios --release
```

---

## Credits
- **Created & Developed by:** [garfnadeck](https://github.com/garfnadeck)
- **Repository:** https://github.com/garfnadeck/switch-second-screen
- **Homebrew Protocol:** [`sys-botbase`](https://github.com/olliz0r/sys-botbase) (`olliz0r`)
