# SwitchHUD (NS Companion) — v1.1.2

> [🇧🇷 Português](README.md) | 🇺🇸 **English**

Real-time second-screen HUD and utility companion for **Nintendo Switch (Atmosphère + `sys-botbase`)**, available as a **Mobile App (Flutter — iOS/Android)** and **ESP32-S3 Firmware (LVGL Display + Responsive Web Dashboard)**.

[![Release v1.1.2](https://img.shields.io/badge/Release-v1.1.2-00B4D8?style=for-the-badge)](https://github.com/garfnadeck/switch-second-screen/releases/tag/v1.1.2)

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

## General Features (App & ESP32-S3)
- **Auto-Reconnect:** automatically reconnects when returning from background or if Wi-Fi/Switch drops.
- **Android Foreground Service:** keeps the TCP connection and Auto-Boost running in the background on Android with a persistent notification.
- **iOS:** iOS suspends background sockets after a few seconds by OS policy, and the app automatically reconnects as soon as you return to it.
- **ESP32-S3 + Responsive Web Dashboard (`http://<BOARD_IP>/`):**
  - Full multi-column web dashboard for Desktop, Tablet, and Mobile (no longer locked to `480x320`), featuring live BotW v1.6.0 telemetry (`Matrix34f` GPS, Hearts, Stamina, Rupees), **Auto-Boost**, **Pouch & Durability** editor (`PauseMenuDataMgr`), **12 Armor Sets**, **Pokémon Party**, **Macros/USB HID**, and **FAQ & Credits**.
  - **Wireless OTA Updates:** flash firmware and LittleFS over Wi-Fi via `espota.py` or the **Config & Rede** tab (`/api/ota`).
  - **Smooth LVGL UI:** low-timeout non-blocking `sys-botbase` polling keeps touch and display responsive even when the Switch is offline.
- **Built-in FAQ & Credits:** step-by-step guide available right inside the **Config** tab (app) and **FAQ & Créditos** (Web).

---

## Quick Start

1. **Prepare your Nintendo Switch:**
   - Install [`sys-botbase`](https://github.com/olliz0r/sys-botbase) on your SD card under `atmosphere/contents/430000000000000B/`.
   - Connect your Switch and phone/tablet to the **same Wi-Fi network**.
2. **Install SwitchHUD:**
   - Download `app-release.apk` (Android) or `SwitchHUD-ios-unsigned.ipa` (iOS via AltStore/Sideloadly/Xcode) from **[Releases](https://github.com/garfnadeck/switch-second-screen/releases/tag/v1.1.2)**.
3. **Connect:**
   - Open the **Rede** tab, enter your Switch IP (default port `6000`) or tap **Buscar Rede**, and hit **Conectar**.

### What persists in `.sav`?
- **Saved to `.sav` when saving in-game:** Armor, Weapons, Bows, Shields, and added/edited pouch items.
- **Re-applied by Auto-Boost on connect:** Max Hearts (`30❤`), Max Stamina (`3 wheels`), **999,999 Rupees**, and Hestu Max Slots (`20/14/20`) — when restarting the game, these values reset to your save state and are automatically re-applied by the app upon connecting.

---

## Build from Source

### Mobile App (Flutter)
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

### ESP32-S3 Firmware (PlatformIO)
```bash
# Build firmware + LittleFS image (Web Dashboard)
pio run -e esp32-s3-hud
pio run -e esp32-s3-hud -t buildfs

# Flash over USB
pio run -e esp32-s3-hud -t upload -t uploadfs

# Or update over Wi-Fi (OTA — replace with your board IP)
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py -i 192.168.15.11 -s -f .pio/build/esp32-s3-hud/littlefs.bin
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py -i 192.168.15.11 -f .pio/build/esp32-s3-hud/firmware.bin
```

---

## Credits
- **Created & Developed by:** [garfnadeck](https://github.com/garfnadeck)
- **Repository:** https://github.com/garfnadeck/switch-second-screen
- **Homebrew Protocol:** [`sys-botbase`](https://github.com/olliz0r/sys-botbase) (`olliz0r`)
