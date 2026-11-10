#pragma once

// Backend USB HID do ESP32-S3 com a mesma interface do Adafruit_TinyUSB usada pelo provider.
// Assim o UsbHidProvider (macros, perfis) roda igual no RP2040 e no ESP32, sem mudar o provider.
// Ativo so com -DUSB_HID_ESP32 (ver platformio.ini, ambiente esp32-s3-hud).

#if defined(ARDUINO_ARCH_ESP32) && defined(USB_HID_ESP32)

#include <stdint.h>
#include "USBHID.h"

// HID_ITF_PROTOCOL_NONE vem do TinyUSB (USBHID.h); nao redeclarar aqui.

class Adafruit_USBD_HID_Esp32 : public USBHIDDevice {
public:
    Adafruit_USBD_HID_Esp32(const uint8_t *desc, uint16_t len, uint8_t protocolo, uint8_t poll, bool boot);
    void setPollInterval(uint8_t) {}
    void setReportDescriptor(const uint8_t *desc, uint16_t len);
    bool begin();
    bool ready() const;
    bool sendReport(uint8_t id, const void *data, uint16_t len);
    uint16_t _onGetDescriptor(uint8_t *buffer) override;

private:
    const uint8_t *_desc;
    uint16_t _len;
    bool _pronto;
};

typedef Adafruit_USBD_HID_Esp32 Adafruit_USBD_HID;

class TinyUSBDeviceEsp32 {
public:
    bool mounted() const;          // true depois que o USB OTG subiu
    void setID(uint16_t, uint16_t) {}
    void setManufacturerDescriptor(const char *) {}
    void setProductDescriptor(const char *) {}
    void attach() {}
    void detach() {}
};

extern TinyUSBDeviceEsp32 TinyUSBDevice;

#endif
