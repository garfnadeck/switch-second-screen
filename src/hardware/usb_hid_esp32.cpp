#include "hardware/usb_hid_esp32.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(USB_HID_ESP32)

#include <string.h>
#include "USB.h"
#include "USBHID.h"

// Mesmo descritor de controle do Pico (usb_hid_provider.h): 16 botoes, direcional, 2 analogicos
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
    uint8_t  chapeu;          // 0..7 direcoes, 8 = solto
    uint8_t  lx, ly, rx, ry;  // analogicos, 128 = centro
    uint8_t  vendor;
};

class ControleSwitch : public USBHIDDevice {
public:
    uint16_t _onGetDescriptor(uint8_t * buffer) override {
        memcpy(buffer, DESCRITOR, sizeof(DESCRITOR));
        return sizeof(DESCRITOR);
    }
};

static USBHID hid;
static ControleSwitch controle;
static RelatorioSwitch rel = { 0, 8, 128, 128, 128, 128, 0 };

void usbHidEsp32Init()
{
    hid.addDevice(&controle, sizeof(DESCRITOR));
    hid.begin();
    USB.productName("Switch Second Screen");
    USB.manufacturerName("garfnadeck");
    USB.begin();
}

void usbHidEsp32Press(uint16_t botoes)
{
    rel.botoes = botoes;
    hid.SendReport(0, &rel, sizeof(rel));
}

void usbHidEsp32Release()
{
    rel.botoes = 0;
    hid.SendReport(0, &rel, sizeof(rel));
}

#else

void usbHidEsp32Init() {}
void usbHidEsp32Press(uint16_t) {}
void usbHidEsp32Release() {}

#endif
