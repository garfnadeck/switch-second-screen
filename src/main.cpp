#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#if defined(ARDUINO_ARCH_RP2040)
    #include "hardware/watchdog.h"   // reset_usb_boot (BOOTSEL)
#endif
#include "wifi_config.h"
#include "core/game_state.h"
#include "core/asset_manager.h"
#include "providers/provider_router.h"

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
    json += "}";
    return json;
}

// Mantem USB (timing das macros) e, se houver tela, o LVGL rodando
static void pump() {
    router.update(state);
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

static void loadSettings() {
    snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", WIFI_SSID);
    snprintf(cfg.pass, sizeof(cfg.pass), "%s", WIFI_PASSWORD);
    snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", SWITCH_IP);
    cfg.sw_port = SWITCH_PORT;
    File f = LittleFS.open("/config.txt", "r");
    if (!f) return;
    while (f.available()) {
        String l = f.readStringUntil('\n'); l.trim();
        int e = l.indexOf('='); if (e < 0) continue;
        String k = l.substring(0, e), v = l.substring(e + 1);
        if (k == "ssid")           snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", v.c_str());
        else if (k == "pass")      snprintf(cfg.pass, sizeof(cfg.pass), "%s", v.c_str());
        else if (k == "switch_ip") { if (validIp(v.c_str())) snprintf(cfg.sw_ip, sizeof(cfg.sw_ip), "%s", v.c_str()); }
        else if (k == "port")      { int p = v.toInt(); if (p > 0 && p < 65536) cfg.sw_port = (uint16_t)p; }
    }
    f.close();
}

static bool saveSettings() {
    File f = LittleFS.open("/config.txt", "w");
    if (!f) return false;
    f.print("ssid=");      f.print(cfg.ssid);  f.print("\n");
    f.print("pass=");      f.print(cfg.pass);  f.print("\n");
    f.print("switch_ip="); f.print(cfg.sw_ip); f.print("\n");
    f.print("port=");      f.print((int)cfg.sw_port); f.print("\n");
    f.close();
    return true;
}

static void rebootBoard() {
    delay(200);
#if defined(ARDUINO_ARCH_RP2040)
    rp2040.reboot();
#else
    ESP.restart();
#endif
}

void setup() {
    // 1. USB HID sobe primeiro (o Switch precisa enumerar o controle logo)
    router.setBootMode(SwitchMode::LOCKED_USB_MACRO);
    router.init(state);

    LittleFS.begin();
    loadSettings();

    // 2. Tela fisica + UI (so ESP32-S3 com tela)
#ifdef HAS_DISPLAY
    init_hardware_display_touch();
    ui.buildUI();
#endif

    // 3. Wi-Fi (pagina web de teste / BOOTSEL)
    //    Diagnostico sem Serial (o CDC atrapalharia o HID):
    //      LED piscando rapido = tentando conectar | LED aceso fixo = conectou na sua rede
    //      LED piscando lento  = falhou: criou a rede propria "SwitchHUD" (senha: switchhud1)
    pinMode(LED_BUILTIN, OUTPUT);
    unsigned long start = millis(), lastBlink = 0;
    bool led = false;
    if (cfg.ssid[0]) {                       // sem SSID salvo -> vai direto para a rede propria (uso na rua)
        WiFi.begin(cfg.ssid, cfg.pass);
        while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
            pump();
            if (millis() - lastBlink >= 150) {
                lastBlink = millis();
                led = !led;
                digitalWrite(LED_BUILTIN, led);
            }
            delay(5);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, HIGH);
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
            if (slotVal >= 0 && slotVal < MACRO_COUNT) {
                router.triggerAction(static_cast<ActionMacro>(slotVal), state);
            }
        }
        server.send(200, "application/json", statusJson(true));
    });

    server.on("/api/macro_status", HTTP_GET, []() {
        server.send(200, "application/json", statusJson(false));
    });

    // Time (HP/status) e zona. Sem sys-botbase (modo USB) os campos vem vazios -> a pagina mostra "sem dados"
    server.on("/api/party", HTTP_GET, []() {
        auto clean = jsonClean;
        String j = "{\"zone\":\"" + clean(state.map.zone_name) + "\",\"x\":" + String((int)state.map.x) +
                   ",\"y\":" + String((int)state.map.y) + ",\"link\":\"" + clean(state.link_status) + "\",\"party\":[";
        for (int i = 0; i < 6; i++) {
            const PokemonSlot &p = state.party[i];
            if (i) j += ",";
            j += "{\"n\":\"" + clean(p.name) + "\",\"lv\":" + String(p.level) + ",\"hp\":" + String(p.current_hp) +
                 ",\"max\":" + String(p.max_hp) + ",\"st\":\"" + clean(p.status) + "\"}";
        }
        j += "]}";
        server.send(200, "application/json", j);
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

    server.serveStatic("/maps_config.json", LittleFS, "/maps_config.json", "max-age=3600");
    server.serveStatic("/m", LittleFS, "/m", "max-age=86400");
    server.serveStatic("/assets", LittleFS, "/assets", "max-age=86400");

    server.begin();
}

void loop() {
    server.handleClient();
    pump();   // router.update a cada volta (timing das macros) + LVGL se houver tela

    if (apMode) {   // LED piscando lento = modo AP de emergencia
        static unsigned long t = 0;
        static bool on = false;
        if (millis() - t >= 1000) {
            t = millis();
            on = !on;
            digitalWrite(LED_BUILTIN, on);
        }
    }
}