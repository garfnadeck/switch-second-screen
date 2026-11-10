// Teste isolado do USB HID no ESP32-S3: so USB.begin + HID, sem Wi-Fi, LittleFS, PSRAM ou HUD.
// Se enumerar como "Switch Minimo", o USB HID funciona na placa e o problema esta no firmware principal.

#include <Arduino.h>
#include <string.h>
#include "USB.h"
#include "USBHID.h"
#include "USBCDC.h"

// Serial virtual pelo mesmo USB OTG: aparece no Mac como /dev/cu.usbmodem* se o firmware subir.
static USBCDC usbSerial;

static const uint8_t DESCRITOR[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x75, 0x01,
    0x95, 0x10, 0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x81, 0x02, 0x05, 0x01, 0x25, 0x07, 0x46, 0x3B,
    0x01, 0x75, 0x04, 0x95, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81,
    0x01, 0x26, 0xFF, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x75,
    0x08, 0x95, 0x04, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x21,
    0x26, 0x95, 0x08, 0x91, 0x02, 0xC0
};

struct __attribute__((packed)) RelatorioSwitch {
    uint16_t botoes;
    uint8_t  chapeu;
    uint8_t  lx, ly, rx, ry;
    uint8_t  vendor;
};

class ControleMinimo : public USBHIDDevice {
public:
    uint16_t _onGetDescriptor(uint8_t *buffer) override {
        memcpy(buffer, DESCRITOR, sizeof(DESCRITOR));
        return sizeof(DESCRITOR);
    }
};

static USBHID hid;
static ControleMinimo controle;
static RelatorioSwitch rel = { 0, 8, 128, 128, 128, 128, 0 };

void setup() {
    usbSerial.begin();
    hid.addDevice(&controle, sizeof(DESCRITOR));
    hid.begin();
    // Identidade de controle HORI (VID 0x0F0D; PID 0x0092 = HORIPAD/Pokken Pro Pad, a confirmar).
    USB.VID(0x0F0D);
    USB.PID(0x0092);
    USB.productName("HORI Pro Pad (teste)");
    USB.manufacturerName("HORI");
    USB.begin();
}

void loop() {
    static uint32_t ultimo = 0;
    uint32_t agora = millis();
    static uint32_t ultimoLog = 0;
    if (agora - ultimoLog >= 1000) {
        ultimoLog = agora;
        usbSerial.printf("minimo rodando: %lu ms\n", (unsigned long)agora);
    }
    if (agora - ultimo >= 2000) {
        ultimo = agora;
        rel.botoes = 1;          // botao A
        hid.SendReport(0, &rel, sizeof(rel));
    } else if (agora - ultimo >= 150) {
        rel.botoes = 0;
        hid.SendReport(0, &rel, sizeof(rel));
    }
    delay(10);
}
