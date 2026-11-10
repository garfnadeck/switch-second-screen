#pragma once

// Controle USB (HID, perfil Switch) pelo USB nativo do ESP32-S3 (IO19/IO20).
// Etapa 1: so enumerar e enviar botoes. Ativo so com -DUSB_HID_ESP32 (senao, nada faz).
// Atencao: o USB nativo passa a ser o USB HID; para gravar, segure BOOT ao conectar.

#include <stdint.h>

// Bits de botao (mesma ordem do relatorio do Switch)
#define USB_BTN_A       (1u << 0)
#define USB_BTN_B       (1u << 1)

void usbHidEsp32Init();
void usbHidEsp32Press(uint16_t botoes);   // botoes: mascara de 16 bits, o resto fica solto
void usbHidEsp32Release();
