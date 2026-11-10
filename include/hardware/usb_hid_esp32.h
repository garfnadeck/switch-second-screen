#pragma once

// Controle USB (HID, perfil Switch) pelo USB nativo do ESP32-S3 (IO19/IO20).
// Etapa 1: so enumerar e enviar botoes. Ativo so com -DUSB_HID_ESP32 (senao, nada faz).
// Atencao: o USB nativo passa a ser o USB HID; para gravar, segure BOOT ao conectar.

#include <stdint.h>

// Bits de botao: ver include/hardware/usb_hid_mapa.h (mapeamento medido no NS2)
#include "hardware/usb_hid_mapa.h"
#define USB_BTN_A  BOTAO(BIT_A)
#define USB_BTN_B  BOTAO(BIT_B)

void usbHidEsp32Init();
void usbHidEsp32Press(uint16_t botoes);   // botoes: mascara de 16 bits, o resto fica solto
void usbHidEsp32Release();
