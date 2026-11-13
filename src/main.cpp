#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#if defined(ARDUINO_ARCH_RP2040)
    #include "hardware/watchdog.h"   // reset_usb_boot (BOOTSEL)
#endif
#include "wifi_config.h"
#ifdef TOUCH_ISOLADO
    #include "hardware/touch_isolado.h"
#endif
#include "hardware/usb_hid_esp32.h"
#include "core/game_state.h"
#include "core/asset_manager.h"
#include "providers/provider_router.h"
#include "../include/core/user_macros.h"
#include "net/wifi_scanner.h"
#include "hardware/es3c35p_display.h"
#include "hardware/es3c35p_extra.h"

// Tela fisica (LVGL) so existe na placa ESP32-S3 3.5" -> build com -DHAS_DISPLAY.
// No Pico W de teste (sem tela) o MVP roda so com USB HID + pagina web.
#ifdef HAS_DISPLAY
    #include "hardware/hardware_hal.h"
    #include "ui/ui_manager.h"
#endif

// Identificacao do firmware (aparece em /api/version e no rodape da pagina)
#define FW_BUILD __DATE__ " " __TIME__

WebServer server(80);
static bool apMode = false;     // true = Wi-Fi do roteador falhou, placa em modo AP "SwitchHUD"
GameState state;
ProviderRouter router;          // unico dono do provider USB HID (router.usb())
#ifdef HAS_DISPLAY
UIManager ui(state, router);    // tela fisica 480x320 (abas Status/Mapa/Macros/Config)
#endif

GameProfile mapStringToProfile(const String &gameStr) {
    if (gameStr == "pkmn_za")    return GameProfile::POKEMON_LEGENDS_ZA;
    if (gameStr == "pkmn_fr")    return GameProfile::POKEMON_FIRERED;
    if (gameStr == "pkmn_sv")    return GameProfile::POKEMON_SV;
    if (gameStr == "zelda_totk") return GameProfile::ZELDA_TOTK;
    if (gameStr == "zelda_botw") return GameProfile::ZELDA_BOTW;
    if (gameStr == "mk8")        return GameProfile::MARIOKART_8;
    return GameProfile::GENERIC_HUB;
}

static String statusJson(bool withOk) {
    String json = "{";
    if (withOk) json += "\"ok\":true,";
    json += "\"active_slot\":" + String(router.usb().getActiveSlot());
    json += ",\"executions\":" + String(state.generic.macro_executions);
    json += ",\"usb\":" + String(state.connected ? "true" : "false");
    json += ",\"um_idx\":" + String(router.usb().userIdx()) + ",\"um_pos\":" + String(router.usb().userPos()) +
            ",\"um_cyc\":" + String((unsigned int)router.usb().userCycles()) + ",\"um_wait\":" + String(router.usb().userWaiting() ? "true" : "false");
    json += ",\"game\":\"" + String(state.profile == GameProfile::POKEMON_FIRERED ? "pkmn_fr" : state.profile == GameProfile::POKEMON_LEGENDS_ZA ? "pkmn_za" : "other") + "\"";
    json += "}";
    return json;
}

// Mantem USB (timing das macros) e, se houver tela, o LVGL rodando
static void pump() {
    router.update(state);
    wifiScanner.poll();
#ifdef HAS_DISPLAY
    #if !LV_TICK_CUSTOM
    static uint32_t last_tick = millis();
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;
    #endif
    lv_timer_handler();
#endif
}

// ---------------- Configuracoes salvas na placa (LittleFS /config.txt) ----------------
// A pagina web grava aqui o IP do Switch e o Wi-Fi; wifi_config.h vira so o valor padrao.
// ATENCAO: "uploadfs" apaga o LittleFS inteiro -> as configuracoes voltam ao padrao do wifi_config.h.
struct Settings { char ssid[33]; char pass[64]; char sw_ip[16]; uint16_t sw_port; };
static Settings cfg;

static bool validIp(const char * s) {
    int a, b, c, d; char tail;
    return sscanf(s, "%d.%d.%d.%d%c", &a, &b, &c, &d, &tail) == 4 &&
           a >= 0 && a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255;
}

static String jsonClean(const char * in) {
    String o;
    for (const char * c = in; *c && o.length() < 40; c++) if (*c >= 32 && *c != '"' && *c != '\\') o += *c;
    return o;
}

static bool saveSettings() {
    File f = LittleFS.open("/config.txt", "w");
    if (!f) return false;
    f.print("ssid=");        f.print(cfg.ssid);  f.print("\n");
    f.print("pass=");        f.print(cfg.pass);  f.print("\n");
    f.print("switch_ip=");   f.print(cfg.sw_ip); f.print("\n");
    f.print("compiled_ip="); f.print(SWITCH_IP); f.print("\n");
    f.print("port=");        f.print((int)cfg.sw_port); f.print("\n");
    f.print("flip=");        f.print(state.screen_flip ? 1 : 0); f.print("\n");
    f.print("bright=");      f.print((int)state.brightness); f.print("\n");
    f.print("sound=");       f.print(state.sound_enabled ? 1 : 0); f.print("\n");
    f.print("autostop=");    f.print(state.auto_stop_shiny ? 1 : 0); f.print("\n");
    f.print("autoheal=");    f.print(state.auto_heal ? 1 : 0); f.print("\n");
    f.close();
    return true;
}

static void loadSettings() {
    snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", WIFI_SSID);
    snprintf(cfg.pass, sizeof(cfg.pass), "%s", WIFI_PASSWORD);
    snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", SWITCH_IP);
    cfg.sw_port = SWITCH_PORT;
    File f = LittleFS.open("/config.txt", "r");
    if (!f) return;
    String file_sw_ip = "", compiled_sw_ip = "";
    while (f.available()) {
        String l = f.readStringUntil('\n'); l.trim();
        int e = l.indexOf('='); if (e < 0) continue;
        String k = l.substring(0, e), v = l.substring(e + 1);
        if (k == "ssid")              snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", v.c_str());
        else if (k == "pass")         snprintf(cfg.pass, sizeof(cfg.pass), "%s", v.c_str());
        else if (k == "switch_ip")    file_sw_ip = v;
        else if (k == "compiled_ip")  compiled_sw_ip = v;
        else if (k == "port")         { int p = v.toInt(); if (p > 0 && p < 65536) cfg.sw_port = (uint16_t)p; }
        else if (k == "flip")         { state.screen_flip = (v.toInt() != 0); }
        else if (k == "bright")       { int b = v.toInt(); if (b >= 15 && b <= 100) state.brightness = (uint8_t)b; }
        else if (k == "sound")        { state.sound_enabled = (v.toInt() != 0); }
        else if (k == "autostop")     { state.auto_stop_shiny = (v.toInt() != 0); }
        else if (k == "autoheal")     { state.auto_heal = (v.toInt() != 0); }
    }
    f.close();
    if (compiled_sw_ip == SWITCH_IP && validIp(file_sw_ip.c_str())) {
        snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", file_sw_ip.c_str());
    } else {
        saveSettings();   // SWITCH_IP do wifi_config.h mudou nesta compilacao: atualiza /config.txt
    }
}

#ifdef HAS_DISPLAY
static void saveWarps() {
    File f = LittleFS.open("/warps.txt", "w");
    if (!f) return;
    for (uint8_t i = 0; i < 3; i++) {
        const WarpPoint &wp = ui.getFavWarp(i);
        if (!wp.valid) continue;
        f.printf("%u|%s|%.2f|%.2f|%.2f\n", (unsigned)i, wp.name, wp.x, wp.y_alt, wp.z);
    }
    f.close();
}

static void loadWarps() {
    File f = LittleFS.open("/warps.txt", "r");
    if (!f) return;
    while (f.available()) {
        String l = f.readStringUntil('\n'); l.trim();
        int p1 = l.indexOf('|'), p2 = l.indexOf('|', p1 + 1), p3 = l.indexOf('|', p2 + 1), p4 = l.indexOf('|', p3 + 1);
        if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) continue;
        uint8_t idx = (uint8_t)l.substring(0, p1).toInt();
        String nm = l.substring(p1 + 1, p2);
        float wx = l.substring(p2 + 1, p3).toFloat();
        float wy = l.substring(p3 + 1, p4).toFloat();
        float wz = l.substring(p4 + 1).toFloat();
        ui.setFavWarp(idx, nm.c_str(), wx, wy, wz);
    }
    f.close();
}
#endif

// ---------------- Estatisticas do FireRed (LittleFS /fr_stats.txt) ----------------
// Gravacao espacada (a cada 20 s, ou na hora quando o usuario toca num botao) para poupar a flash.
static void loadFrStats() {
    File f = LittleFS.open("/fr_stats.txt", "r");
    if (!f) return;
    FireRedStats &s = state.fr;
    while (f.available()) {
        String l = f.readStringUntil('\n'); l.trim();
        int e = l.indexOf('='); if (e < 0) continue;
        String k = l.substring(0, e); uint32_t v = (uint32_t)l.substring(e + 1).toInt();
        if (k == "enc") s.encounters = v; else if (k == "resets") s.soft_resets = v;
        else if (k == "steps") s.steps_est = v; else if (k == "hunt") s.hunt_s = v;
        else if (k == "runs") s.runs = v; else if (k == "badges") s.badges = (uint8_t)v;
        else if (k == "ach") s.ach_mask = v;
    }
    f.close();
}

static void saveFrStats() {
    File f = LittleFS.open("/fr_stats.txt", "w");
    if (!f) return;
    const FireRedStats &s = state.fr;
    f.print("enc=");    f.print((unsigned long)s.encounters);   f.print("\n");
    f.print("resets="); f.print((unsigned long)s.soft_resets); f.print("\n");
    f.print("steps=");  f.print((unsigned long)s.steps_est);    f.print("\n");
    f.print("hunt=");   f.print((unsigned long)s.hunt_s);       f.print("\n");
    f.print("runs=");   f.print((unsigned long)s.runs);         f.print("\n");
    f.print("badges="); f.print((int)s.badges);                 f.print("\n");
    f.print("ach=");    f.print((unsigned long)s.ach_mask);     f.print("\n");
    f.close();
}

static void frPersistTick() {
    static unsigned long last_save = 0;
    FireRedStats &s = state.fr;
    if (!s.dirty) return;
    unsigned long now = millis();
    if (s.urgent || now - last_save >= 20000) {
        last_save = now; s.dirty = false; s.urgent = false;
        saveFrStats();
    }
}

static String frJson() {
    const FireRedStats &s = state.fr;
    char shiny[16]; snprintf(shiny, sizeof(shiny), "%.4f", s.shinyChance());
    String j = "{\"enc\":" + String((unsigned int)s.encounters) + ",\"resets\":" + String((unsigned int)s.soft_resets) +
               ",\"steps\":" + String((unsigned int)s.steps_est) + ",\"hunt\":" + String((unsigned int)s.hunt_s) +
               ",\"runs\":" + String((unsigned int)s.runs) + ",\"badges\":" + String((int)s.badges) +
               ",\"ach\":" + String((unsigned int)s.ach_mask) + ",\"last\":" + String((int)s.last_ach) +
               ",\"shiny\":" + String(shiny) + ",\"names\":[";
    for (uint8_t i = 0; i < FireRedStats::ACH_COUNT; i++) { if (i) j += ","; j += "\"" + String(FireRedStats::achName(i)) + "\""; }
    j += "]}";
    return j;
}

// ---------------- Macros do usuario (LittleFS /macros.txt) ----------------
// Uma linha por macro: indice|nome|repeticoes|passos   (passos = "botoes:direcional:ms;...")
static void loadUserMacros() {
    File f = LittleFS.open("/macros.txt", "r");
    if (!f) return;
    while (f.available()) {
        String l = f.readStringUntil('\n'); l.trim();
        char line[UM_MAX_STEPS * 20 + 64];
        snprintf(line, sizeof(line), "%s", l.c_str());
        char * p1 = strchr(line, '|');           if (!p1) continue;
        char * p2 = strchr(p1 + 1, '|');         if (!p2) continue;
        char * p3 = strchr(p2 + 1, '|');         if (!p3) continue;
        *p1 = 0; *p2 = 0; *p3 = 0;
        state.um.set((uint8_t)atoi(line), p1 + 1, (uint16_t)atoi(p2 + 1), p3 + 1);
    }
    f.close();
}

static bool saveUserMacros() {
    File f = LittleFS.open("/macros.txt", "w");
    if (!f) return false;
    static char buf[UM_MAX_STEPS * 20];
    for (uint8_t i = 0; i < UM_COUNT; i++) {
        if (state.um.m[i].n == 0 && !state.um.m[i].name[0]) continue;
        state.um.stepsText(i, buf, sizeof(buf));
        f.print((int)i); f.print("|"); f.print(state.um.m[i].name); f.print("|");
        f.print((int)state.um.m[i].repeat); f.print("|"); f.print(buf); f.print("\n");
    }
    f.close();
    return true;
}

static String umJson() {
    static char buf[UM_MAX_STEPS * 20];
    String j = "{\"max_steps\":" + String(UM_MAX_STEPS) + ",\"slots\":[";
    for (uint8_t i = 0; i < UM_COUNT; i++) {
        const UserMacro &u = state.um.m[i];
        state.um.stepsText(i, buf, sizeof(buf));
        if (i) j += ",";
        j += "{\"i\":" + String(i) + ",\"name\":\"" + String(u.name) + "\",\"repeat\":" + String(u.repeat) +
             ",\"n\":" + String(u.n) + ",\"steps\":\"" + String(buf) + "\"}";
    }
    j += "]}";
    return j;
}

static void rebootBoard() {
    delay(200);
#if defined(ARDUINO_ARCH_RP2040)
    rp2040.reboot();
#else
    ESP.restart();
#endif
}


// LED de status. No ESP32-S3 DevKitC o LED_BUILTIN e o GPIO 48, que e o RST do touch da ES3C35P:
// piscar o LED resetava o touch sem parar. Na placa ESP32 o LED fica desligado (sem LED de usuario).
#if defined(ARDUINO_ARCH_ESP32)
    static inline void ledInit() {}
    static inline void ledWrite(bool) {}
#else
    static inline void ledInit() { pinMode(LED_BUILTIN, OUTPUT); }
    static inline void ledWrite(bool on) { digitalWrite(LED_BUILTIN, on ? HIGH : LOW); }
#endif

void setup() {
#ifdef USB_HID_ESP32_TESTE
    usbHidEsp32Init();   // teste: aparece como controle USB e aperta A a cada 2 s (ver loop)
#endif
#ifdef TOUCH_ISOLADO
    touchIsoladoRun();   // teste isolado do toque: nao retorna
#endif
    // 1. USB HID sobe primeiro (o Switch precisa enumerar o controle logo)
    router.setBootMode(SwitchMode::LOCKED_USB_MACRO);
    router.init(state);

    LittleFS.begin();
    loadSettings();
    loadFrStats();
    loadUserMacros();

    // 2. MicroSD (SDIO 4-bit 16GB) + Tela fisica + UI (so ESP32-S3 com tela)
    es3c35p_extra::init();
    state.battery_mv = es3c35p_extra::readBatteryMv(state.battery_pct);
    AssetManager::initSD();
#ifdef HAS_DISPLAY
    init_hardware_display_touch();
    #if defined(ARDUINO_ARCH_ESP32)
    es3c35p_lvgl_set_flip(state.screen_flip);
    es3c35p_display::setBrightness(state.brightness);
    es3c35p_extra::initAudio();
    es3c35p_extra::setSoundEnabled(state.sound_enabled);
    #endif
    ui.buildUI();
    loadWarps();
#endif

    // Teste de bancada da tela ST77922 (so com -DES3C35P_TELA_TESTE): mostra "TESTE" e segue o firmware
#ifdef ES3C35P_TELA_TESTE
    if (es3c35p_display::init()) es3c35p_display::drawTestText("TESTE");
#endif

    // 3. Wi-Fi (pagina web de teste / BOOTSEL)
    //    Diagnostico sem Serial (o CDC atrapalharia o HID):
    //      LED piscando rapido = tentando conectar | LED aceso fixo = conectou na sua rede
    //      LED piscando lento  = falhou: criou a rede propria "SwitchHUD" (senha: switchhud1)
    ledInit();
    unsigned long start = millis(), lastBlink = 0;
    bool led = false;
    if (cfg.ssid[0]) {                       // sem SSID salvo -> vai direto para a rede propria (uso na rua)
        WiFi.begin(cfg.ssid, cfg.pass);
        while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
            pump();
            if (millis() - lastBlink >= 150) {
                lastBlink = millis();
                led = !led;
                ledWrite(led);
            }
            delay(5);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        ledWrite(true);
    } else {
        // Sem roteador: a placa cria a rede "SwitchHUD". Ligue o Switch (Configuracoes > Internet)
        // e o celular nela; a pagina fica em http://192.168.42.1/  (IP padrao do modo AP no arduino-pico)
        WiFi.disconnect();
        WiFi.softAP("SwitchHUD", "switchhud1");
        apMode = true;
    }

    // Leitura de RAM do Switch (Atmosphere + sys-botbase) em paralelo ao USB HID.
    // IP/porta: salvos pela pagina (aba Rede) ou, na falta, os de wifi_config.h.
    if (validIp(cfg.sw_ip)) router.setSwitchTarget(state, cfg.sw_ip, cfg.sw_port);

    // Endereco da pagina no monitor serial (so ESP32; no Pico o CDC conflita com o HID)
#if !defined(ARDUINO_ARCH_RP2040)
    Serial.begin(115200);
    delay(300);
    if (apMode) Serial.printf("[rede] SwitchHUD (senha switchhud1): pagina em http://%s/\n", WiFi.softAPIP().toString().c_str());
    else        Serial.printf("[rede] Wi-Fi conectado: pagina em http://%s/\n", WiFi.localIP().toString().c_str());
#endif

    server.on("/", HTTP_GET, []() {
        File file = LittleFS.open("/index.html", "r");
        if (!file) {
            server.send(500, "text/plain", "Erro ao carregar index.html (rode uploadfs)");
            return;
        }
        server.sendHeader("Cache-Control", "no-store");   // sempre a pagina que esta na placa
        server.streamFile(file, "text/html");
        file.close();
    });

    // Confere qual firmware esta rodando e se a tela esta habilitada
    server.on("/api/version", HTTP_GET, []() {
        String json = "{\"fw\":\"" FW_BUILD "\",\"display\":";
#ifdef HAS_DISPLAY
        json += "true";
#else
        json += "false";
#endif
        json += ",\"net\":\"" + String(apMode ? "ap" : "sta") + "\"";
        json += ",\"ip\":\"" + (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\"";
        json += "}";
        server.send(200, "application/json", json);
    });

    server.serveStatic("/css", LittleFS, "/css");
    server.serveStatic("/js", LittleFS, "/js");
    server.serveStatic("/games", LittleFS, "/games");

    server.on("/api/profile", HTTP_GET, []() {
        if (server.hasArg("game")) {
            router.setProfile(state, mapStringToProfile(server.arg("game")));
        }
        server.send(200, "application/json", statusJson(true));
    });

    server.on("/api/macro", HTTP_GET, []() {
        if (server.hasArg("slot")) {
            int slotVal = server.arg("slot").toInt();
            if (slotVal >= 0 && (slotVal < MACRO_COUNT || (slotVal >= USER_MACRO_BASE && slotVal < USER_MACRO_BASE + UM_COUNT))) {
                router.triggerAction(static_cast<ActionMacro>(slotVal), state);
            }
        }
        server.send(200, "application/json", statusJson(true));
    });

    // FireRed: estatisticas e operacoes do contador / insignias
    server.on("/api/fr", HTTP_GET, []() {
        if (server.hasArg("op")) {
            String op = server.arg("op");
            FireRedStats &f = state.fr;
            if (op == "inc") f.encounters++;
            else if (op == "dec") { if (f.encounters) f.encounters--; }
            else if (op == "reset") f.encounters = 0;
            else if (op == "badge" && server.hasArg("i")) { int i = server.arg("i").toInt(); if (i >= 0 && i < 8) f.badges ^= (1u << i); }
            f.dirty = true; f.urgent = true;
            f.evaluate();
        }
        server.send(200, "application/json", frJson());
    });

    // ---- Macros do usuario ----
    server.on("/api/um", HTTP_GET, []() { server.send(200, "application/json", umJson()); });

    server.on("/api/um", HTTP_POST, []() {
        if (!server.hasArg("i")) { server.send(400, "application/json", "{\"ok\":false,\"err\":\"sem indice\"}"); return; }
        int i = server.arg("i").toInt();
        if (i < 0 || i >= UM_COUNT) { server.send(400, "application/json", "{\"ok\":false,\"err\":\"indice\"}"); return; }
        if (server.hasArg("clear")) {
            state.um.clear((uint8_t)i);
        } else {
            int rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 1;
            if (rep < 0) rep = 0; if (rep > 65000) rep = 65000;
            if (!state.um.set((uint8_t)i, server.arg("name").c_str(), (uint16_t)rep, server.arg("steps").c_str())) {
                server.send(400, "application/json", "{\"ok\":false,\"err\":\"passos invalidos\"}"); return;
            }
        }
        bool ok = saveUserMacros();
        server.send(ok ? 200 : 500, "application/json", umJson());
    });

    // run = executa com a repeticao salva | test = 1 vez | step = passo a passo | next = proximo passo | (stop = /api/stop)
    server.on("/api/um_run", HTTP_GET, []() {
        String mode = server.hasArg("mode") ? server.arg("mode") : "run";
        if (mode == "next") router.usb().userNext();
        else if (server.hasArg("i")) {
            int i = server.arg("i").toInt();
            uint8_t m = (mode == "test") ? 1 : (mode == "step") ? 2 : 0;
            if (i >= 0 && i < UM_COUNT) router.usb().startUser((uint8_t)i, m, state);
        }
        server.send(200, "application/json", statusJson(true));
    });

    // Controle ao vivo (gravador): b = mascara de botoes, h = direcional (0..7, 8 = neutro)
    server.on("/api/pad", HTTP_GET, []() {
        uint16_t b = (uint16_t)(server.arg("b").toInt() & 0x3FFF);
        int h = server.hasArg("h") ? server.arg("h").toInt() : 8;
        if (h < 0 || h > 8) h = 8;
        router.usb().livePad(b, (uint8_t)h, state);
        server.send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/stop", HTTP_GET, []() {
        router.stopAll(state);
        server.send(200, "application/json", statusJson(true));
    });

    server.on("/api/macro_status", HTTP_GET, []() {
        server.send(200, "application/json", statusJson(false));
    });

    // Time (HP/status) e zona. Sem sys-botbase (modo USB) os campos vem vazios -> a pagina mostra "sem dados"
    server.on("/api/party", HTTP_GET, []() {
        auto clean = jsonClean;
        String j = "{\"zone\":\"" + clean(state.map.zone_name) + "\",\"x\":" + String((int)state.map.x) +
                   ",\"y\":" + String((int)state.map.y) + ",\"z\":" + String((int)state.map.z) +
                   ",\"shiny_stash\":" + (state.shiny_stash_found ? "true" : "false") +
                   ",\"shiny_name\":\"" + clean(state.shiny_stash_name) + "\"" +
                   ",\"bat_pct\":" + String(state.battery_pct) +
                   ",\"bat_mv\":" + String(state.battery_mv) +
                   ",\"link\":\"" + clean(state.link_status) + "\",\"party\":[";
        for (int i = 0; i < 6; i++) {
            const PokemonSlot &p = state.party[i];
            if (i) j += ",";
            j += "{\"sp\":" + String(p.species) + ",\"n\":\"" + clean(p.name) + "\",\"lv\":" + String(p.level) +
                 ",\"hp\":" + String(p.current_hp) + ",\"max\":" + String(p.max_hp) + ",\"st\":\"" + clean(p.status) + "\"" +
                 ",\"item\":" + String(p.held_item) + ",\"shiny\":" + (p.is_shiny ? "true" : "false") +
                 ",\"alpha\":" + (p.is_alpha ? "true" : "false") + ",\"ev\":" + String(p.ev_total) +
                 ",\"iv\":" + (p.is_max_iv ? "true" : "false") + "}";
        }
        j += "]}";
        server.send(200, "application/json", j);
    });

    // ---- MicroSD de 16 GB: status, upload de assets via Wi-Fi e reload na PSRAM ----
    server.on("/api/sd/status", HTTP_GET, []() {
        String j = String("{\"ok\":true,\"sd_ready\":") + (AssetManager::isSDReady() ? "true" : "false") +
                   ",\"total_mb\":" + String((unsigned long)AssetManager::sdTotalMB()) +
                   ",\"used_mb\":" + String((unsigned long)AssetManager::sdUsedMB()) + "}";
        server.send(200, "application/json", j);
    });

    static bool sdUploadOk = false;
    static String sdUploadPath = "";
    server.on("/api/sd/upload", HTTP_POST, []() {
        server.send(sdUploadOk ? 200 : 500, "application/json",
                    String("{\"ok\":") + (sdUploadOk ? "true" : "false") + ",\"path\":\"" + sdUploadPath + "\"}");
    }, []() {
        HTTPUpload &up = server.upload();
        if (up.status == UPLOAD_FILE_START) {
            sdUploadPath = server.hasArg("path") ? server.arg("path") : ("/" + up.filename);
            if (!sdUploadPath.startsWith("/")) sdUploadPath = "/" + sdUploadPath;
            sdUploadOk = AssetManager::writeFile(sdUploadPath.c_str(), nullptr, 0, false);
        } else if (up.status == UPLOAD_FILE_WRITE) {
            if (sdUploadOk && up.currentSize > 0) {
                sdUploadOk = AssetManager::writeFile(sdUploadPath.c_str(), up.buf, up.currentSize, true);
            }
        }
    });

    server.on("/api/sd/reload", HTTP_POST, []() {
#ifdef HAS_DISPLAY
        ui.reloadAssets();
#endif
        server.send(200, "application/json", "{\"ok\":true}");
    });

    // ---- Rede: IP do Switch + Wi-Fi (gravados na placa) ----
    server.on("/api/config", HTTP_GET, []() {
        String j = "{\"ssid\":\"" + jsonClean(cfg.ssid) + "\",\"has_pass\":" + String(cfg.pass[0] ? "true" : "false") +
                   ",\"switch_ip\":\"" + String(cfg.sw_ip) + "\",\"switch_port\":" + String(cfg.sw_port) +
                   ",\"net\":\"" + String(apMode ? "ap" : "sta") + "\",\"ip\":\"" +
                   (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\"}";
        server.send(200, "application/json", j);
    });

    server.on("/api/config", HTTP_POST, []() {
        bool reboot = false;
        if (server.hasArg("switch_ip")) {
            String ip = server.arg("switch_ip"); ip.trim();
            if (!validIp(ip.c_str())) { server.send(400, "application/json", "{\"ok\":false,\"err\":\"IP invalido\"}"); return; }
            snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", ip.c_str());
            if (server.hasArg("switch_port")) { int p = server.arg("switch_port").toInt(); if (p > 0 && p < 65536) cfg.sw_port = (uint16_t)p; }
            router.setSwitchTarget(state, cfg.sw_ip, cfg.sw_port);   // aplica na hora, sem reiniciar
        }
        if (server.hasArg("ssid")) {
            String ssid = server.arg("ssid"), pass = server.arg("pass");
            bool changed = (ssid != String(cfg.ssid));
            if (changed || pass.length()) {
                snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", ssid.c_str());
                snprintf(cfg.pass, sizeof(cfg.pass), "%s", pass.c_str());
                reboot = true;                                       // Wi-Fi novo so vale apos reiniciar
            }
        }
        bool ok = saveSettings();
        server.send(200, "application/json", String("{\"ok\":") + (ok ? "true" : "false") + ",\"reboot\":" + (reboot ? "true" : "false") + "}");
        if (reboot) rebootBoard();
    });

    // ---- Busca de redes Wi-Fi ao redor (a pagina escolhe a rede e grava a senha em /api/config) ----
    server.on("/api/wifi/scan", HTTP_POST, []() {
        bool started = wifiScanner.begin();
        server.send(200, "application/json", String("{\"ok\":") + (started ? "true" : "false") + "}");
    });

    server.on("/api/wifi/results", HTTP_GET, []() {
        server.send(200, "application/json", wifiScanner.toJson());
    });

    // Procura o Switch (porta do sys-botbase aberta) na propria sub-rede da placa
    server.on("/api/scan", HTTP_POST, []() {
        IPAddress ip = apMode ? WiFi.softAPIP() : WiFi.localIP();
        char prefix[20];
        snprintf(prefix, sizeof(prefix), "%d.%d.%d.", ip[0], ip[1], ip[2]);
        router.scanForSwitch(state, prefix);
        server.send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/scan_status", HTTP_GET, []() {
        uint8_t st = router.scanState();
        if (st == 2 && strcmp(cfg.sw_ip, state.switch_ip) != 0) {      // achou: grava o IP encontrado
            snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", state.switch_ip);
            saveSettings();
        }
        const char * names[] = {"idle", "running", "found", "none"};
        server.send(200, "application/json", String("{\"state\":\"") + names[st] + "\",\"progress\":" +
                    String(router.scanProgress()) + ",\"ip\":\"" + String(state.switch_ip) + "\"}");
    });

    server.on("/api/reboot", HTTP_GET, []() {
        server.send(200, "application/json", "{\"rebooting\":true}");
        delay(100);
#if defined(ARDUINO_ARCH_RP2040)
        reset_usb_boot(0, 0);
#else
        ESP.restart();
#endif
    });

    server.on("/api/pokemon/edit", HTTP_POST, []() {
        uint8_t slot = server.hasArg("slot") ? (uint8_t)server.arg("slot").toInt() : 0;
        uint8_t act  = server.hasArg("action") ? (uint8_t)server.arg("action").toInt() : 0;
        uint16_t itm = server.hasArg("item") ? (uint16_t)server.arg("item").toInt() : 0;
        if (slot < 6 && act <= 16) {
            router.editPokemon(state, slot, static_cast<PokeEditAction>(act), itm);
            server.send(200, "application/json", "{\"ok\":true}");
        } else {
            server.send(400, "application/json", "{\"ok\":false}");
        }
    });

    server.on("/api/teleport", HTTP_POST, []() {
        if (!server.hasArg("wx") || !server.hasArg("wz")) {
            server.send(400, "application/json", "{\"ok\":false}");
            return;
        }
        float wx = server.arg("wx").toFloat();
        float wz = server.arg("wz").toFloat();
        float wy = server.hasArg("wy") ? server.arg("wy").toFloat() : state.map.z;
        router.teleportPlayer(state, wx, wy, wz);
        server.send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/display/flip", HTTP_POST, []() {
        state.screen_flip = server.hasArg("flip") ? (server.arg("flip").toInt() != 0) : !state.screen_flip;
        state.screen_flip_changed = true;
#if defined(HAS_DISPLAY) && defined(ARDUINO_ARCH_ESP32)
        es3c35p_lvgl_set_flip(state.screen_flip);
#endif
        saveSettings();
        server.send(200, "application/json", String("{\"ok\":true,\"flip\":") + (state.screen_flip ? "true" : "false") + "}");
    });

    server.serveStatic("/maps_config.json", LittleFS, "/maps_config.json", "max-age=3600");
    server.serveStatic("/m", LittleFS, "/m", "max-age=86400");
    server.serveStatic("/assets", LittleFS, "/assets", "max-age=86400");

    // Atualizacao de firmware sem fio (HTTP POST /api/ota + ArduinoOTA)
    static bool otaOk = false;
    server.on("/api/ota", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(otaOk ? 200 : 500, "application/json", otaOk ? "{\"ok\":true}" : "{\"ok\":false,\"err\":\"Update falhou\"}");
        if (otaOk) {
            delay(300);
            ESP.restart();
        }
    }, []() {
        HTTPUpload &up = server.upload();
        if (up.status == UPLOAD_FILE_START) {
            otaOk = Update.begin(UPDATE_SIZE_UNKNOWN);
        } else if (up.status == UPLOAD_FILE_WRITE) {
            if (otaOk && Update.write(up.buf, up.currentSize) != up.currentSize) {
                otaOk = false;
            }
        } else if (up.status == UPLOAD_FILE_END) {
            if (otaOk && !Update.end(true)) {
                otaOk = false;
            }
        }
    });

    ArduinoOTA.setHostname("switch-hud");
    ArduinoOTA.begin();

    server.begin();
}

void loop() {
    ArduinoOTA.handle();
    {
        static uint32_t ultimoIp = 0;
        if (millis() - ultimoIp >= 2000) {
            ultimoIp = millis();
            state.battery_mv = es3c35p_extra::readBatteryMv(state.battery_pct);
            IPAddress ip = apMode ? WiFi.softAPIP() : WiFi.localIP();
            snprintf(state.board_ip, sizeof(state.board_ip), "%s", ip.toString().c_str());
            // IP digitado no teclado da tela, ou achado pela busca disparada na tela: grava na flash
            bool digitado = state.switch_ip_changed;
            bool girou = state.screen_flip_changed;
            bool cfg_mudou = state.brightness_changed;
            state.switch_ip_changed = false;
            state.screen_flip_changed = false;
            state.brightness_changed = false;
            if ((digitado || router.scanState() == 2) && strcmp(cfg.sw_ip, state.switch_ip) != 0) {
                snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", state.switch_ip);
                saveSettings();
            } else if (girou || cfg_mudou) {
                saveSettings();
            }
#ifdef HAS_DISPLAY
            if (ui.consumeWarpsDirty()) saveWarps();
#endif
        }
    }

    // Automacao Shiny Stash + Audio + Auto-Heal + LED RGB + Auto-Dim
    {
        static bool prevShiny = false;
        if (state.shiny_stash_found && !prevShiny) {
            es3c35p_extra::playBeep(true);
            if (state.auto_stop_shiny && state.active_macro_slot >= 0) {
                router.stopAll(state);
            }
        }
        prevShiny = state.shiny_stash_found;

        static uint32_t lastAutoHeal = 0;
        if (state.auto_heal && state.connected && millis() - lastAutoHeal >= 5000) {
            lastAutoHeal = millis();
            for (uint8_t i = 0; i < 6; i++) {
                const PokemonSlot &p = state.party[i];
                if (p.max_hp > 0 && p.current_hp > 0 && ((uint32_t)p.current_hp * 100 < (uint32_t)p.max_hp * 25)) {
                    router.editPokemon(state, i, PokeEditAction::HEAL_HP, 0);
                }
            }
        }

        if (state.shiny_stash_found) {
            bool on = ((millis() / 400) % 2) == 0;
            es3c35p_extra::setRgb(on ? 255 : 40, on ? 180 : 20, 0);
        } else if (state.active_macro_slot >= 0) {
            es3c35p_extra::setRgb(60, 0, 120);
        } else if (state.connected) {
            es3c35p_extra::setRgb(0, 40, 10);
        } else {
            es3c35p_extra::setRgb(0, 10, 40);
        }

#ifdef HAS_DISPLAY
        static bool dimmed = false;
        static uint8_t last_bright = 100;
        bool should_dim = (lv_disp_get_inactive_time(NULL) > 120000UL);
        if (should_dim && !dimmed) {
            dimmed = true;
            es3c35p_display::setBrightness(15);
        } else if (!should_dim && (dimmed || last_bright != state.brightness)) {
            dimmed = false;
            last_bright = state.brightness;
            es3c35p_display::setBrightness(state.brightness);
        }
#endif
    }
#ifdef USB_HID_ESP32_TESTE
    {
        static uint32_t ultimo = 0;
        uint32_t agora = millis();
        if (agora - ultimo >= 2000) {
            ultimo = agora;
            usbHidEsp32Press(USB_BTN_A);
        } else if (agora - ultimo >= 150) {
            usbHidEsp32Release();
        }
    }
#endif
    server.handleClient();
    frPersistTick();
    pump();   // router.update a cada volta (timing das macros) + LVGL se houver tela

    if (apMode) {   // LED piscando lento = modo AP de emergencia
        static unsigned long t = 0;
        static bool on = false;
        if (millis() - t >= 1000) {
            t = millis();
            on = !on;
            ledWrite(on);
        }
    }
}