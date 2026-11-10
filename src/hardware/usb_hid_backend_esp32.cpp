#include "hardware/usb_hid_backend_esp32.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(USB_HID_ESP32)

#include <string.h>
#include "USB.h"
#include "USBHID.h"

// Um so USBHID para o firmware inteiro (o TinyUSB do RP2040 tambem e unico)
static USBHID s_hid;
static bool s_usbPronto = false;

TinyUSBDeviceEsp32 TinyUSBDevice;

bool TinyUSBDeviceEsp32::mounted() const { return s_usbPronto; }

Adafruit_USBD_HID_Esp32::Adafruit_USBD_HID_Esp32(const uint8_t *desc, uint16_t len, uint8_t, uint8_t, bool)
    : _desc(desc), _len(len), _pronto(false) {}

void Adafruit_USBD_HID_Esp32::setReportDescriptor(const uint8_t *desc, uint16_t len)
{
    _desc = desc;
    _len = len;
}

bool Adafruit_USBD_HID_Esp32::begin()
{
    if (_pronto) return true;
    s_hid.addDevice(this, _len);
    s_hid.begin();
    // Identidade de controle HORI, a mesma usada no teste que o NS2 reconheceu
    USB.VID(0x0F0D);
    USB.PID(0x0092);
    USB.productName("HORI Pro Pad");
    USB.manufacturerName("HORI");
    USB.begin();
    _pronto = true;
    s_usbPronto = true;
    return true;
}

bool Adafruit_USBD_HID_Esp32::ready() const { return _pronto && s_usbPronto; }

bool Adafruit_USBD_HID_Esp32::sendReport(uint8_t id, const void *data, uint16_t len)
{
    if (!ready()) return false;
    s_hid.SendReport(id, data, len);
    return true;
}

uint16_t Adafruit_USBD_HID_Esp32::_onGetDescriptor(uint8_t *buffer)
{
    memcpy(buffer, _desc, _len);
    return _len;
}

#endif
