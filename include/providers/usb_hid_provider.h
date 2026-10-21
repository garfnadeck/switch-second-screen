#ifndef USB_HID_PROVIDER_H
#define USB_HID_PROVIDER_H

#include "../core/i_game_provider.h"
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
    #include <Arduino.h>
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
    SW_BTN_HOME  = 0x1000,
    SW_BTN_CAPT  = 0x2000
};

struct HidStep {
    uint16_t buttons;
    uint8_t dpad;
    uint16_t hold_ticks;
};

class UsbHidProvider : public IGameProvider {
private:
    HidStep queue[32];
    uint8_t q_head = 0;
    uint8_t q_tail = 0;
    uint16_t current_step_ticks = 0;
    bool turbo_a_active = false;

    void enqueueStep(uint16_t btns, uint8_t dpad, uint16_t ticks) {
        uint8_t next = (q_tail + 1) % 32;
        if (next != q_head) {
            queue[q_tail] = {btns, dpad, ticks};
            q_tail = next;
        }
    }

    void sendUsbReport(uint16_t btns, uint8_t dpad) {
        #ifdef ARDUINO
            Serial.printf("[USB-HID HW] Buttons: 0x%04X | DPad: %d\n", btns, dpad);
        #else
            if (btns != SW_BTN_NONE || dpad != 8) {
                printf("[USB-HID SIM] Enviando pacote USB -> Botoes: 0x%04X | DPad: %d\n", btns, dpad);
            }
        #endif
    }

public:
    void init(GameState &state) override {
        state.mode = SwitchMode::LOCKED_USB_MACRO;
        state.connected = true;
        q_head = q_tail = 0;
    }

    void setProfile(GameState &state, GameProfile new_profile) override {
        state.profile = new_profile;
        q_head = q_tail = 0;
    }

    void update(GameState &state) override {
        state.mode = SwitchMode::LOCKED_USB_MACRO;

        if (q_head != q_tail) {
            HidStep &step = queue[q_head];
            if (current_step_ticks == 0) {
                sendUsbReport(step.buttons, step.dpad);
            }
            current_step_ticks++;
            if (current_step_ticks >= step.hold_ticks) {
                current_step_ticks = 0;
                q_head = (q_head + 1) % 32;
                if (q_head == q_tail) {
                    sendUsbReport(SW_BTN_NONE, 8);
                }
            }
        } else if (turbo_a_active) {
            static bool toggle = false;
            toggle = !toggle;
            sendUsbReport(toggle ? SW_BTN_A : SW_BTN_NONE, 8);
        }
    }

    void triggerAction(ActionMacro action, GameState &state) override {
        turbo_a_active = false;
        if (state.profile == GameProfile::POKEMON_SV) {
            enqueueStep(SW_BTN_A, 8, 1);
        } else {
            enqueueStep(SW_BTN_A, 8, 1);
        }
    }
};

#endif // USB_HID_PROVIDER_H