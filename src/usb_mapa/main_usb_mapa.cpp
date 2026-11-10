// Teste de mapeamento manual: cada celula da grade (bit 0 a 15) envia o bit ao ser tocada.
// O LCD destaca o ultimo bit enviado e o terminal (USB OTG CDC) registra o mesmo bit.

#include <Arduino.h>
#include <string.h>
#include <lvgl.h>
#include "USB.h"
#include "USBHID.h"
#include "USBCDC.h"
#include "hardware/es3c35p_lvgl.h"

// Descritor igual ao do firmware de teste (gamepad de 16 botoes, direcional, 2 analogicos)
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

class ControleMapa : public USBHIDDevice {
public:
    uint16_t _onGetDescriptor(uint8_t *buffer) override {
        memcpy(buffer, DESCRITOR, sizeof(DESCRITOR));
        return sizeof(DESCRITOR);
    }
};

static USBHID hid;
static ControleMapa controle;
static USBCDC usbSerial;
static RelatorioSwitch rel = { 0, 8, 128, 128, 128, 128, 0 };

static const int PINO_BACKLIGHT = 41;
static const uint32_t DURACAO_PRESSAO_MS = 150;
static lv_obj_t * s_celulas[16];
static lv_obj_t * s_status = NULL;
static volatile bool s_soltarEm = false;
static uint32_t s_pressionadoEm = 0;

// Envia o bit tocado (pressao) e agenda a soltura
static void enviarBit(int bit)
{
    rel.botoes = (uint16_t)(1u << bit);
    hid.SendReport(0, &rel, sizeof(rel));
    usbSerial.printf("bit %d\n", bit);
    lv_label_set_text_fmt(s_status, "enviado: bit %d", bit);
    for (int i = 0; i < 16; i++) {
        lv_obj_set_style_bg_color(s_celulas[i], lv_color_hex(i == bit ? 0xE00000 : 0x333333), 0);
    }
    s_pressionadoEm = millis();
    s_soltarEm = true;
}

static void aoTocar(lv_event_t * e)
{
    int bit = (int)(intptr_t)lv_event_get_user_data(e);
    enviarBit(bit);
}

static void criarTela()
{
    lv_obj_t * tela = lv_scr_act();
    lv_obj_set_style_bg_color(tela, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(tela, LV_OPA_COVER, 0);

    lv_obj_t * titulo = lv_label_create(tela);
    lv_label_set_text(titulo, "Toque no bit para enviar");
    lv_obj_set_style_text_color(titulo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(titulo, 10, 6);

    for (int i = 0; i < 16; i++) {
        int c = i % 4, r = i / 4;
        lv_obj_t * cel = lv_obj_create(tela);
        lv_obj_set_size(cel, 110, 62);
        lv_obj_set_pos(cel, 10 + c * 118, 36 + r * 68);
        lv_obj_set_style_bg_color(cel, lv_color_hex(0x333333), 0);
        lv_obj_set_style_bg_opa(cel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cel, 0, 0);
        lv_obj_clear_flag(cel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(cel, aoTocar, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t * txt = lv_label_create(cel);
        lv_label_set_text_fmt(txt, "bit %d", i);
        lv_obj_set_style_text_color(txt, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(txt);
        s_celulas[i] = cel;
    }

    s_status = lv_label_create(tela);
    lv_label_set_text(s_status, "aguardando toque...");
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_pos(s_status, 10, 300);
}

void setup()
{
    pinMode(PINO_BACKLIGHT, OUTPUT);
    digitalWrite(PINO_BACKLIGHT, HIGH);

    usbSerial.begin();

    es3c35p_lvgl_init();
    criarTela();

    hid.addDevice(&controle, sizeof(DESCRITOR));
    hid.begin();
    USB.VID(0x0F0D);
    USB.PID(0x0092);
    USB.productName("HORI Pro Pad (teste)");
    USB.manufacturerName("HORI");
    USB.begin();
}

void loop()
{
    static uint32_t ultimoPisca = 0;
    static uint32_t ultimoLog = 0;
    uint32_t agora = millis();

    lv_timer_handler();

    if (agora - ultimoPisca >= 500) {
        ultimoPisca = agora;
        digitalWrite(PINO_BACKLIGHT, !digitalRead(PINO_BACKLIGHT));
    }
    if (agora - ultimoLog >= 1000) {
        ultimoLog = agora;
        usbSerial.printf("mapa rodando: %lu ms\n", (unsigned long)agora);
    }

    // Solta o botao depois de DURACAO_PRESSAO_MS
    if (s_soltarEm && agora - s_pressionadoEm >= DURACAO_PRESSAO_MS) {
        s_soltarEm = false;
        rel.botoes = 0;
        hid.SendReport(0, &rel, sizeof(rel));
    }
    delay(5);
}
