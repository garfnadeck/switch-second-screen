#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "hardware/watchdog.h"
#include "wifi_config.h"
#include "core/game_state.h"
#include "core/asset_manager.h"
#include "providers/provider_router.h"
#include "providers/usb_hid_provider.h"

WebServer server(80);
GameState state;
ProviderRouter router;
UsbHidProvider usbProvider;

GameProfile mapStringToProfile(const String &gameStr) {
    if (gameStr == "pkmn_za")    return GameProfile::POKEMON_LEGENDS_ZA;
    if (gameStr == "pkmn_sv")    return GameProfile::POKEMON_SV;
    if (gameStr == "zelda_totk") return GameProfile::ZELDA_TOTK;
    if (gameStr == "zelda_botw") return GameProfile::ZELDA_BOTW;
    if (gameStr == "mk8")        return GameProfile::MARIOKART_8;
    return GameProfile::GENERIC_HUB;
}

void setup() {
    // 1. Inicializa o USB HID imediatamente como a primeira instrução
    usbProvider.init(state);

    LittleFS.begin();
    router.init(state);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        usbProvider.update(state);
        delay(20);
    }

    // Serve a página web estática diretamente do LittleFS (data/index.html)
    server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/index.html", "r");

    if (!file) {
        server.send(500, "text/plain",
                    "Erro ao carregar index.html");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
});

    server.serveStatic("/css", LittleFS, "/css");
    server.serveStatic("/js", LittleFS, "/js");
    server.serveStatic("/games", LittleFS, "/games");

    server.on("/api/profile", HTTP_GET, []() {
        if (server.hasArg("game")) {
            GameProfile prof = mapStringToProfile(server.arg("game"));
            router.setProfile(state, prof);
            usbProvider.setProfile(state, prof);
        }
        String json = "{\"ok\":true,\"active_slot\":" + String(usbProvider.getActiveSlot()) +
                      ",\"executions\":" + String(state.generic.macro_executions) + "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/macro", HTTP_GET, []() {
        if (server.hasArg("slot")) {
            int slotVal = server.arg("slot").toInt();
            ActionMacro macro = static_cast<ActionMacro>(slotVal);
            usbProvider.triggerAction(macro, state);
        }
        String json = "{\"ok\":true,\"active_slot\":" + String(usbProvider.getActiveSlot()) +
                      ",\"executions\":" + String(state.generic.macro_executions) + "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/macro_status", HTTP_GET, []() {
        String json = "{\"active_slot\":" + String(usbProvider.getActiveSlot()) +
                      ",\"executions\":" + String(state.generic.macro_executions) + "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/reboot", HTTP_GET, []() {
        server.send(200, "application/json", "{\"rebooting\":true}");
        delay(100);
        reset_usb_boot(0, 0);
    });

    server.serveStatic("/maps_config.json", LittleFS, "/maps_config.json", "max-age=3600");
    server.serveStatic("/m", LittleFS, "/m", "max-age=86400");
    server.serveStatic("/assets", LittleFS, "/assets", "max-age=86400");

    server.begin();
}

void loop() {
    server.handleClient();
    usbProvider.update(state);
}