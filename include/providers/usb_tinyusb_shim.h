#ifndef USB_TINYUSB_SHIM_H
#define USB_TINYUSB_SHIM_H

// Substituto do Adafruit_TinyUSB para placas que nao sao RP2040 (ex.: ESP32-S3 nesta fase).
// O USB HID nao enumera: mounted() e ready() dao false, e as macros USB ficam paradas.
// O port real do USB no ESP32-S3 entra na fase F3 (ver docs/DECISOES.md).

#include <stdint.h>

enum { HID_ITF_PROTOCOL_NONE = 0 };

class ShimUsbHid {
public:
    ShimUsbHid() {}
    ShimUsbHid(const uint8_t *, uint16_t, uint8_t, uint8_t, bool) {}
    void setPollInterval(uint8_t) {}
    void setReportDescriptor(const uint8_t *, uint16_t) {}
    bool begin() { return false; }
    bool ready() const { return false; }
    bool sendReport(uint8_t, const void *, uint16_t) { return false; }
};

class ShimTinyUSBDevice {
public:
    bool mounted() const { return false; }
    void setID(uint16_t, uint16_t) {}
    void setManufacturerDescriptor(const char *) {}
    void setProductDescriptor(const char *) {}
    void attach() {}
    void detach() {}
};

typedef ShimUsbHid Adafruit_USBD_HID;
static ShimTinyUSBDevice TinyUSBDevice;

#endif
