#ifndef USB_HID_PROVIDER_H
#define USB_HID_PROVIDER_H

#include "../core/i_game_provider.h"
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
    #include <Arduino.h>
    #include <Adafruit_TinyUSB.h>
#endif

enum SwitchButton : uint16_t {
    SW_BTN_NONE  = 0x0000,
    SW_BTN_Y     = 0x0001,
    SW_BTN_B     = 0x0002,
    SW_BTN_A     = 0x0004,
    SW_BTN_X     = 0x0008,
    SW_BTN_L     = 0x0010,
    SW_BTN_R     = 0x0020,
    SW_BTN_ZL    = 0x0040,
    SW_BTN_ZR    = 0x0080,
    SW_BTN_MINUS = 0x0100,
    SW_BTN_PLUS  = 0x0200,
    SW_BTN_LCLK  = 0x0400,
    SW_BTN_RCLK  = 0x0800,
    SW_BTN_HOME  = 0x1000,
    SW_BTN_CAPT  = 0x2000
};

struct HidStep {
    uint16_t buttons;
    uint8_t dpad;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
    uint16_t hold_ms;
};

static const uint8_t switch_hid_report_desc[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x75, 0x01,
    0x95, 0x10, 0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x81, 0x02, 0x05, 0x01, 0x25, 0x07, 0x46, 0x3B,
    0x01, 0x75, 0x04, 0x95, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81,
    0x01, 0x26, 0xFF, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x75,
    0x08, 0x95, 0x04, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x21,
    0x26, 0x95, 0x08, 0x91, 0x02, 0xC0
};

struct __attribute__((packed)) CustomSwitchReport {
    uint16_t buttons;
    uint8_t  hat;
    uint8_t  lx, ly, rx, ry;
    uint8_t  vendor;
};

class UsbHidProvider : public IGameProvider {
private:
    HidStep queue[32];
    uint8_t q_head = 0;
    uint8_t q_tail = 0;
    unsigned long step_start_ms = 0;
    bool step_in_progress = false;

    int active_slot = -1;
    bool loop_mode = false;
    unsigned long last_loop_ms = 0;
    uint8_t loop_phase = 0;
    unsigned long last_keepalive_ms = 0;

#ifdef ARDUINO
    Adafruit_USBD_HID usb_hid;
#endif

    void clearQueue() {
        q_head = q_tail = 0;
        step_in_progress = false;
    }

    void enqueueStep(uint16_t btns, uint8_t dpad, uint16_t hold_ms,
                     uint8_t lx = 128, uint8_t ly = 128, uint8_t rx = 128, uint8_t ry = 128) {
        uint8_t next = (q_tail + 1) % 32;
        if (next != q_head) {
            queue[q_tail] = {btns, dpad, lx, ly, rx, ry, hold_ms};
            q_tail = next;
        }
    }

    // Aperta (hold_ms) e solta (gap_ms): 2 passos da fila
    void tap(uint16_t btns, uint16_t hold_ms, uint16_t gap_ms) { tap(btns, 0x08, hold_ms, gap_ms); }
    void tap(uint16_t btns, uint8_t dpad, uint16_t hold_ms, uint16_t gap_ms) {
        enqueueStep(btns, dpad, hold_ms);
        enqueueStep(SW_BTN_NONE, 0x08, gap_ms);
    }

    void sendUsbReport(uint16_t btns, uint8_t dpad,
                       uint8_t lx = 128, uint8_t ly = 128, uint8_t rx = 128, uint8_t ry = 128) {
#ifdef ARDUINO
        CustomSwitchReport rep;
        rep.buttons = btns;
        rep.hat     = dpad;
        rep.lx      = lx;
        rep.ly      = ly;
        rep.rx      = rx;
        rep.ry      = ry;
        rep.vendor  = 0;

        if (TinyUSBDevice.mounted() && usb_hid.ready()) {
            usb_hid.sendReport(0, &rep, sizeof(rep));
        }
#endif
    }

public:
    UsbHidProvider()
#ifdef ARDUINO
        : usb_hid(switch_hid_report_desc, sizeof(switch_hid_report_desc), HID_ITF_PROTOCOL_NONE, 1, false)
#endif
    {}

    int getActiveSlot() const { return active_slot; }

    void init(GameState &state) override {
        state.mode = SwitchMode::LOCKED_USB_MACRO;
        state.profile = GameProfile::POKEMON_LEGENDS_ZA;
        state.connected = true;
        clearQueue();

#ifdef ARDUINO
        TinyUSBDevice.setID(0x0F0D, 0x0092);
        TinyUSBDevice.setManufacturerDescriptor("HORI CO.,LTD.");
        TinyUSBDevice.setProductDescriptor("POKKEN CONTROLLER");

        usb_hid.setPollInterval(1);
        usb_hid.setReportDescriptor(switch_hid_report_desc, sizeof(switch_hid_report_desc));
        usb_hid.begin();

        if (TinyUSBDevice.mounted()) {
            TinyUSBDevice.detach();
            delay(150);
            TinyUSBDevice.attach();
        }
#endif
    }

    void setProfile(GameState &state, GameProfile new_profile) override {
        state.profile = new_profile;
        clearQueue();
        loop_mode = false;
        active_slot = -1;
        sendUsbReport(SW_BTN_NONE, 0x08);
    }

    void update(GameState &state) override {
        state.mode = SwitchMode::LOCKED_USB_MACRO;
        unsigned long now = millis();
        state.active_macro_slot = static_cast<int8_t>(active_slot);
#ifdef ARDUINO
        state.connected = TinyUSBDevice.mounted();   // cabo USB-C ligado e enumerado
#endif

        if (q_head != q_tail) {
            HidStep &step = queue[q_head];
            if (!step_in_progress) {
                step_in_progress = true;
                step_start_ms = now;
                sendUsbReport(step.buttons, step.dpad, step.lx, step.ly, step.rx, step.ry);
            } else if (now - step_start_ms >= step.hold_ms) {
                step_in_progress = false;
                q_head = (q_head + 1) % 32;
                if (q_head == q_tail) {
                    sendUsbReport(SW_BTN_NONE, 0x08);
                    if (!loop_mode) active_slot = -1;
                }
            }
        } else if (loop_mode) {
            switch (active_slot) {
                case 2:  // Turbo A
                case 8:  // Spam Y
                case 9:  // Spam B
                    if (now - last_loop_ms >= 85) {
                        last_loop_ms = now;
                        loop_phase = !loop_phase;
                        uint16_t b = (active_slot == 2) ? SW_BTN_A : (active_slot == 8) ? SW_BTN_Y : SW_BTN_B;
                        sendUsbReport(loop_phase ? b : SW_BTN_NONE, 0x08);
                    }
                    break;
                case 3:  // Circulo Perfeito com Analogico (Farm / Hatch de Ovos)
                    if (now - last_loop_ms >= 45) {
                        last_loop_ms = now;
                        loop_phase = (loop_phase + 1) % 12;
                        const uint8_t lx_circ[12] = {128, 195, 240, 255, 240, 195, 128, 60, 15, 0, 15, 60};
                        const uint8_t ly_circ[12] = {0, 15, 60, 128, 195, 240, 255, 240, 195, 128, 60, 15};
                        sendUsbReport(SW_BTN_B, 0x08, lx_circ[loop_phase], ly_circ[loop_phase], 128, 128);
                    }
                    break;
                case 10: // Andar reto (analogico para frente)
                case 11: // Correr (analogico para frente + B)
                    if (now - last_loop_ms >= 40) {
                        last_loop_ms = now;
                        sendUsbReport(active_slot == 11 ? SW_BTN_B : SW_BTN_NONE, 0x08, 128, 0, 128, 128);
                    }
                    break;
                default: break;
            }
        } else {
            if (now - last_keepalive_ms >= 50) {
                last_keepalive_ms = now;
                sendUsbReport(SW_BTN_NONE, 0x08);
            }
        }
    }

    void triggerAction(ActionMacro action, GameState &state) override {
        int clicked_slot = static_cast<int>(action);

        if (active_slot == clicked_slot) {
            clearQueue();
            loop_mode = false;
            active_slot = -1;
            state.generic.turbo_enabled = false;
            state.active_macro_slot = -1;
            sendUsbReport(SW_BTN_NONE, 0x08);
            return;
        }

        clearQueue();
        loop_mode = false;
        active_slot = clicked_slot;
        state.active_macro_slot = static_cast<int8_t>(clicked_slot);
        state.generic.macro_executions++;

        switch (clicked_slot) {
            case 0: // Parear Switch (L+R -> A)
                enqueueStep(SW_BTN_L | SW_BTN_R, 0x08, 300);
                enqueueStep(SW_BTN_NONE, 0x08, 150);
                enqueueStep(SW_BTN_A, 0x08, 200);
                enqueueStep(SW_BTN_NONE, 0x08, 100);
                break;

            case 1: // Combo de Batalha (Esquiva + Y carregado)
                enqueueStep(SW_BTN_B, 0x08, 120, 128, 255, 128, 128); // esquiva recuando (analogico para baixo)
                enqueueStep(SW_BTN_NONE, 0x08, 80);
                enqueueStep(SW_BTN_Y, 0x08, 400, 128, 128, 128, 128); // ataque pesado mantendo Y
                enqueueStep(SW_BTN_NONE, 0x08, 150);
                break;

            case 2: case 8: case 9: case 3: case 10: case 11: // toggles (loop em update())
                loop_mode = true;
                loop_phase = 0;
                if (clicked_slot == 2) state.generic.turbo_enabled = true;
                break;

            case 4: // Abrir Mapa (+ -> A)
                enqueueStep(SW_BTN_PLUS, 0x08, 200);
                enqueueStep(SW_BTN_NONE, 0x08, 350);
                enqueueStep(SW_BTN_A, 0x08, 150);
                enqueueStep(SW_BTN_NONE, 0x08, 100);
                break;

            case 5: // Cancelar / Voltar (2x B)
                tap(SW_BTN_B, 120, 100);
                tap(SW_BTN_B, 120, 100);
                break;

            case 6: // Auto-Catch (ZL+ZR)
                enqueueStep(SW_BTN_ZL | SW_BTN_ZR, 0x08, 400);
                enqueueStep(SW_BTN_NONE, 0x08, 150);
                break;

            case 7: // Esquiva simples (B + analogico para tras)
                enqueueStep(SW_BTN_B, 0x08, 120, 128, 255, 128, 128);
                enqueueStep(SW_BTN_NONE, 0x08, 100);
                break;

            case 12: // Menu rapido (X)
                tap(SW_BTN_X, 150, 200);
                break;

            case 13: // Home
                tap(SW_BTN_HOME, 150, 200);
                break;

            case 14: // Reset da camera (R3)
                tap(SW_BTN_RCLK, 120, 150);
                break;

            // ---- CURA (PRECISAM SER CALIBRADAS no seu jogo: ordem/posicao dos itens no menu) ----
            // Sequencia assumida: X abre o menu -> A entra em Itens -> A usa o item sob o cursor
            //                     -> A escolhe o 1o Pokemon -> A confirma -> B x3 fecha tudo.
            case 15: // Curar HP (item de cura na 1a posicao da lista)
                tap(SW_BTN_X, 150, 600);
                tap(SW_BTN_A, 150, 500);
                tap(SW_BTN_A, 150, 400);
                tap(SW_BTN_A, 150, 400);
                tap(SW_BTN_A, 150, 300);
                tap(SW_BTN_B, 120, 200);
                tap(SW_BTN_B, 120, 200);
                tap(SW_BTN_B, 120, 200);
                break;

            case 16: // Curar Status (cura-tudo na 2a posicao: 1x para baixo antes de usar)
                tap(SW_BTN_X, 150, 600);
                tap(SW_BTN_A, 150, 500);
                tap(SW_BTN_NONE, 0x04, 100, 250);   // D-pad para baixo (hat 4)
                tap(SW_BTN_A, 150, 400);
                tap(SW_BTN_A, 150, 400);
                tap(SW_BTN_A, 150, 300);
                tap(SW_BTN_B, 120, 200);
                tap(SW_BTN_B, 120, 200);
                tap(SW_BTN_B, 120, 200);
                break;

            default: break;
        }
    }
};

#endif // USB_HID_PROVIDER_H