// Teste de mapeamento com tela: o LCD mostra os 16 bits do relatorio HID do controle HORI.
// A cada 1 s o bit seguinte e enviado e fica destacado na tela; o terminal (USB OTG CDC) mostra o mesmo bit.
// Assim, quando o botao do NS2 responder, da para ver na tela e no terminal qual bit era.

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
static lv_obj_t * s_celulas[16];
static lv_obj_t * s_titulo = NULL;
static lv_obj_t * s_status = NULL;

static void criarTela()
{
    lv_obj_t * tela = lv_scr_act();
    lv_obj_set_style_bg_color(tela, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(tela, LV_OPA_COVER, 0);

    s_titulo = lv_label_create(tela);
    lv_label_set_text(s_titulo, "Teste de botoes (bit)");
    lv_obj_set_style_text_color(s_titulo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_titulo, 10, 6);

    for (int i = 0; i < 16; i++) {
        int c = i % 4, r = i / 4;
        lv_obj_t * cel = lv_obj_create(tela);
        lv_obj_set_size(cel, 110, 62);
        lv_obj_set_pos(cel, 10 + c * 118, 36 + r * 68);
        lv_obj_set_style_bg_color(cel, lv_color_hex(0x333333), 0);
        lv_obj_set_style_bg_opa(cel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cel, 0, 0);
        lv_obj_clear_flag(cel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t * txt = lv_label_create(cel);
        lv_label_set_text_fmt(txt, "bit %d", i);
        lv_obj_set_style_text_color(txt, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(txt);
        s_celulas[i] = cel;
    }

    s_status = lv_label_create(tela);
    lv_label_set_text(s_status, "aguardando...");
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_pos(s_status, 10, 300);
}

// Destaca o bit atual (vermelho) e apaga o anterior
static void destacar(int atual, int anterior)
{
    if (anterior >= 0) lv_obj_set_style_bg_color(s_celulas[anterior], lv_color_hex(0x333333), 0);
    if (atual >= 0)    lv_obj_set_style_bg_color(s_celulas[atual], lv_color_hex(0xE00000), 0);
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
    static uint32_t ultimo = 0;
    static uint32_t inicio = 0;
    static bool pressionado = false;
    static uint8_t indice = 0;
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

    // Espera 30 s antes de enviar qualquer input, para dar tempo de abrir a tela de teste no console
    static const uint32_t ATRASO_INICIO_MS = 30000;
    if (agora < ATRASO_INICIO_MS) {
        static uint32_t ultimaContagem = 0xFFFFFFFF;
        uint32_t restante = (ATRASO_INICIO_MS - agora + 999) / 1000;
        if (restante != ultimaContagem) {
            ultimaContagem = restante;
            lv_label_set_text_fmt(s_status, "inicia em %lu s", (unsigned long)restante);
            usbSerial.printf("aguardando inicio: %lu s\n", (unsigned long)restante);
        }
        delay(5);
        return;
    }

    // Intervalo de 10 s entre um input e o proximo, para dar tempo de anotar
    if (!pressionado && agora - ultimo >= 10000) {
        rel.botoes = (uint16_t)(1u << indice);
        hid.SendReport(0, &rel, sizeof(rel));
        usbSerial.printf("bit %u\n", indice);
        lv_label_set_text_fmt(s_status, "enviado: bit %u", indice);
        destacar(indice, (indice + 15) % 16);
        pressionado = true;
        inicio = agora;
    } else if (pressionado && agora - inicio >= 700) {
        rel.botoes = 0;
        hid.SendReport(0, &rel, sizeof(rel));
        pressionado = false;
        indice = (uint8_t)((indice + 1) % 16);
        ultimo = agora;
    }
    delay(5);
}
