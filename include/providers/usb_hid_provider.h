#ifndef USB_HID_PROVIDER_H
#define USB_HID_PROVIDER_H

#include "../core/i_game_provider.h"
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
    #include <Arduino.h>
    #if defined(ARDUINO_ARCH_RP2040)
        #include <Adafruit_TinyUSB.h>
    #elif defined(USB_HID_ESP32)
        #include "hardware/usb_hid_backend_esp32.h"   // ESP32-S3: USB HID real pelo USB OTG
    #else
        #include "usb_tinyusb_shim.h"   // ESP32-S3 sem o backend USB HID: stub, nao enumera
    #endif
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

// ---- Calibracao da Caca FireRed (tempos medidos no video do usuario; ajuste aqui se o ciclo errar) ----
// Esperar demais e SEGURO (as telas ficam paradas esperando o botao); apertar cedo demais e que desalinha.
static constexpr uint16_t FR_BOOT_WAIT_MS   = 9000;  // apos o soft reset ate o titulo "PRESS START" estar na tela (~7 s)
static constexpr uint16_t FR_AFTER_START_MS = 1500;  // titulo -> menu CONTINUE
static constexpr uint16_t FR_LOAD_WAIT_MS   = 7500;  // CONTINUE -> resumo "Previously on your quest" -> laboratorio (~6 s)
static constexpr uint16_t FR_BALL_TEXT_MS   = 5000;  // A na Pokebola -> "Ah! CHARMANDER is your choice..." terminar de digitar
static constexpr uint16_t FR_YESNO_MS       = 3500;  // texto -> caixa "So, ... claiming CHARMANDER?" Sim/Nao
static constexpr uint16_t FR_RECEIVE_MS     = 8000;  // "energetic" -> fanfarra "received" -> pergunta do apelido (~7 s)
static constexpr uint8_t  FR_AFTER_NICK_B   = 8;     // toques de B (1,5 s) apos o apelido: Nao + texto do Gary ate o controle
static constexpr uint16_t FR_SHINY_CHECK_MS = 10000; // pausa no resumo para conferir se e shiny

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
    // ---- Macros do usuario (state.um) ----
    int8_t   um_idx = -1;               // macro em execucao (-1 = nenhum)
    uint8_t  um_pos = 0;                // proximo passo a executar
    bool     um_started = false;        // passo atual ja enviado
    bool     um_step_mode = false;      // passo a passo: pausa depois de cada passo
    bool     um_wait = false;           // pausado esperando "proximo"
    uint16_t um_limit = 1;              // ciclos a executar (0 = infinito)
    uint32_t um_cycles = 0;
    unsigned long um_t0 = 0;
    // ---- Controle ao vivo (gravador da pagina) ----
    bool     pad_active = false;
    unsigned long pad_until = 0;
    bool hunt_once = false;             // FireRed: Caca Reset roda so 1 ciclo
    unsigned long last_hunt_tick = 0;   // FireRed: contador de segundos de macro ativa
    unsigned long last_step_cnt  = 0;   // FireRed: contador de passos ao correr

#ifdef ARDUINO
    Adafruit_USBD_HID usb_hid;
#endif

    void clearQueue() {
        um_idx = -1; um_started = false; um_wait = false; pad_active = false;
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


    // ======================= POKEMON FIRERED =======================
    // Slots FR (0..24). As posicoes de menu sao ASSUMIDAS e precisam de calibracao no seu jogo.
    //  Caca: 0 SoftReset 1 CacaReset(loop) 2 Grama(loop)
    //  Dialogo: 3 SpamA(loop) 4 SpamB(loop) 5 AvancarTexto
    //  Menu: 6 Menu 7 Bolsa 8 Salvar 9 Fechar
    //  Cura: 10 Pocao 11 CentroPokemon
    //  Movimento: 12..15 Correr cima/baixo/esq/dir (loop) 16 Patrulha <-> (loop) 17 Patrulha ^v (loop)
    //  Batalha: 18..21 Golpe 1..4  22 Trocar  23 Fugir  24 Atacar rapido (A x3)
    static constexpr uint8_t DIR_UP = 0, DIR_RIGHT = 2, DIR_DOWN = 4, DIR_LEFT = 6;

    void frWalk(uint8_t dir, uint8_t tiles, GameState &state) {
        for (uint8_t i = 0; i < tiles; i++) tap(SW_BTN_NONE, dir, 270, 30);
        state.fr.steps_est += tiles;
        state.fr.dirty = true;
    }

    void frTrigger(int slot, GameState &state) {
        if (slot != 24) hunt_once = false;
        state.fr.runs++;
        state.fr.dirty = true;
        switch (slot) {
            case 0: // Soft Reset (+ - A B)
                enqueueStep(SW_BTN_PLUS | SW_BTN_MINUS | SW_BTN_A | SW_BTN_B, 0x08, 500);
                enqueueStep(SW_BTN_NONE, 0x08, 200);
                state.fr.soft_resets++;
                break;
            case 24: // Testar 1 ciclo da caca: roda reset -> resumo e para sozinho
                hunt_once = true; loop_mode = true; loop_phase = 1;   // teste: parte da Pokebola ate o resumo, sem reset
                break;
            case 1: case 2: case 3: case 4: case 12: case 13: case 14: case 15: case 16: case 17:
                loop_mode = true; loop_phase = (slot == 1) ? 1 : 0; last_step_cnt = millis();   // Caca Reset comeca JA na frente da Pokebola (sem reset)
                break;
            case 5: for (uint8_t i = 0; i < 8; i++) tap(SW_BTN_A, 90, 260); break;
            case 6: tap(SW_BTN_PLUS, 120, 300); break;
            case 7: // Menu -> Bolsa (3o item)
                tap(SW_BTN_PLUS, 120, 350);
                tap(SW_BTN_NONE, DIR_DOWN, 100, 150);
                tap(SW_BTN_NONE, DIR_DOWN, 100, 150);
                tap(SW_BTN_A, 120, 300);
                break;
            case 8: // Menu -> Salvar (5o item) -> confirma
                tap(SW_BTN_PLUS, 120, 350);
                for (uint8_t i = 0; i < 4; i++) tap(SW_BTN_NONE, DIR_DOWN, 100, 150);
                tap(SW_BTN_A, 120, 400);
                tap(SW_BTN_A, 120, 600);
                break;
            case 9: for (uint8_t i = 0; i < 4; i++) tap(SW_BTN_B, 110, 200); break;
            case 10: // Bolsa -> 1o item (Pocao) -> Usar -> 1o Pokemon
                tap(SW_BTN_PLUS, 120, 350);
                tap(SW_BTN_NONE, DIR_DOWN, 100, 150);
                tap(SW_BTN_NONE, DIR_DOWN, 100, 150);
                tap(SW_BTN_A, 120, 450);
                tap(SW_BTN_A, 120, 350);
                tap(SW_BTN_A, 120, 350);
                tap(SW_BTN_A, 120, 900);
                tap(SW_BTN_B, 110, 250);
                tap(SW_BTN_B, 110, 250);
                break;
            case 11: // Centro Pokemon: fale com a enfermeira (de frente) e va confirmando
                for (uint8_t i = 0; i < 4; i++) tap(SW_BTN_A, 100, 700);
                enqueueStep(SW_BTN_NONE, 0x08, 3500);      // animacao das Pokebolas
                for (uint8_t i = 0; i < 3; i++) tap(SW_BTN_A, 100, 600);
                tap(SW_BTN_B, 110, 300);
                break;
            case 18: tap(SW_BTN_A, 110, 400); tap(SW_BTN_A, 110, 250); break;
            case 19: tap(SW_BTN_A, 110, 400); tap(SW_BTN_NONE, DIR_RIGHT, 90, 150); tap(SW_BTN_A, 110, 250); break;
            case 20: tap(SW_BTN_A, 110, 400); tap(SW_BTN_NONE, DIR_DOWN, 90, 150); tap(SW_BTN_A, 110, 250); break;
            case 21: tap(SW_BTN_A, 110, 400); tap(SW_BTN_NONE, DIR_DOWN, 90, 150);
                     tap(SW_BTN_NONE, DIR_RIGHT, 90, 150); tap(SW_BTN_A, 110, 250); break;
            case 22: tap(SW_BTN_NONE, DIR_DOWN, 90, 150); tap(SW_BTN_A, 110, 400); break;   // Pokemon (baixo-esq)
            case 23: tap(SW_BTN_NONE, DIR_RIGHT, 90, 150); tap(SW_BTN_NONE, DIR_DOWN, 90, 150);
                     tap(SW_BTN_A, 110, 400); tap(SW_BTN_A, 110, 500); break;               // Fugir (baixo-dir)
            default: break;
        }
    }

    // Caca do inicial (slots 1 e 24). Fases (sequenciais): 0 soft reset+carregar | 1 Pokebola |
    // 2 apelido NAO + texto do Gary | 3 menu->Pokemon->Resumo + pausa | 4 conta e decide o proximo ciclo.
    // Obs: B tambem avanca texto, e numa caixa Sim/Nao escolhe "Nao". Por isso, DEPOIS de confirmar
    // o Charmander so usamos B: nao depende de a pergunta do apelido aparecer num momento exato.
    void frHunt(GameState &state) {
#ifdef FR_DEBUG
        Serial.print("[FR] fase "); Serial.println(loop_phase);   // compile com -DFR_DEBUG para acompanhar no monitor serial
#endif
        if (loop_phase == 0) {                 // soft reset -> titulo -> CONTINUE -> laboratorio carregado
            tap(SW_BTN_PLUS | SW_BTN_MINUS | SW_BTN_A | SW_BTN_B, 500, 200);
            enqueueStep(SW_BTN_NONE, 0x08, FR_BOOT_WAIT_MS);
            tap(SW_BTN_A, 150, FR_AFTER_START_MS);            // PRESS START
            tap(SW_BTN_A, 150, FR_LOAD_WAIT_MS);              // CONTINUE (o resumo pula sozinho)
            loop_phase = 1;
        } else if (loop_phase == 1) {          // Pokebola: cada A so depois do texto terminar de digitar
            tap(SW_BTN_A, 150, FR_BALL_TEXT_MS);              // interagir -> "Ah! CHARMANDER is your choice..."
            tap(SW_BTN_A, 150, FR_YESNO_MS);                  // avanca -> "So, ... claiming CHARMANDER?" SIM/NAO
            tap(SW_BTN_A, 150, FR_YESNO_MS);                  // SIM -> "This POKeMON is really quite energetic!"
            tap(SW_BTN_A, 150, FR_RECEIVE_MS);                // avanca -> recebeu -> pergunta do apelido
            loop_phase = 2;
        } else if (loop_phase == 2) {          // B: responde NAO ao apelido e passa o texto do Gary
            for (uint8_t i = 0; i < FR_AFTER_NICK_B; i++) tap(SW_BTN_B, 150, 1500);
            loop_phase = 3;
        } else if (loop_phase == 3) {          // menu (POKeMON e o 1o item, sem Pokedex) -> Charmander -> Resumo
            tap(SW_BTN_PLUS, 150, 900);                       // menu
            tap(SW_BTN_A, 150, 1500);                         // POKeMON
            tap(SW_BTN_A, 150, 900);                          // Charmander -> SUMMARY / ITEM / CANCEL
            tap(SW_BTN_A, 150, 1200);                         // SUMMARY
            enqueueStep(SW_BTN_NONE, 0x08, FR_SHINY_CHECK_MS);   // PAUSA: veja se e shiny (PARE a macro se for!)
            loop_phase = 4;
        } else {                               // fase 4: conta a tentativa e decide
            state.fr.soft_resets++; state.fr.encounters++;
            state.fr.dirty = true; state.fr.urgent = true;
            loop_phase = 0;
            if (hunt_once) { hunt_once = false; loop_mode = false; active_slot = -1; state.active_macro_slot = -1; sendUsbReport(SW_BTN_NONE, 0x08); }
        }
    }

    // Chamado quando a fila esvazia e o modo loop esta ativo (perfil FireRed)
    void frLoop(unsigned long now, GameState &state) {
        switch (active_slot) {
            case 1: case 24: frHunt(state); break;
            case 3: case 4: // Spam A / B
                if (now - last_loop_ms >= 85) {
                    last_loop_ms = now; loop_phase = !loop_phase;
                    sendUsbReport(loop_phase ? (active_slot == 3 ? SW_BTN_A : SW_BTN_B) : SW_BTN_NONE, 0x08);
                }
                break;
            case 12: case 13: case 14: case 15: // Correr: direcao + B
                if (now - last_loop_ms >= 40) {
                    last_loop_ms = now;
                    static const uint8_t d[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};
                    sendUsbReport(SW_BTN_B, d[active_slot - 12]);
                }
                if (now - last_step_cnt >= 133) { last_step_cnt = now; state.fr.steps_est++; state.fr.dirty = true; }
                break;
            case 2: frWalk(DIR_LEFT, 4, state); frWalk(DIR_RIGHT, 4, state); break;   // Grama
            case 16: frWalk(DIR_LEFT, 3, state); frWalk(DIR_RIGHT, 3, state); break;
            case 17: frWalk(DIR_UP, 3, state);   frWalk(DIR_DOWN, 3, state);  break;
            default: break;
        }
    }


    // ======================= MACROS DO USUARIO =======================
    void umFinish(GameState &state) {
        um_idx = -1; um_started = false; um_wait = false;
        loop_mode = false; active_slot = -1; state.active_macro_slot = -1;
        sendUsbReport(SW_BTN_NONE, 0x08);
    }

    void umUpdate(unsigned long now, GameState &state) {
        const UserMacro &u = state.um.m[um_idx];
        if (u.n == 0) { umFinish(state); return; }
        if (!um_started) {
            if (um_step_mode && um_wait) return;              // passo a passo: espera o "proximo"
            const UMStep &st = u.s[um_pos];
            sendUsbReport(st.btns, st.hat);
            um_t0 = now; um_started = true;
            return;
        }
        if (now - um_t0 < u.s[um_pos].ms) return;
        um_started = false;
        um_pos++;
        if (um_pos >= u.n) {
            um_pos = 0; um_cycles++;
            if (um_limit != 0 && um_cycles >= um_limit) { umFinish(state); return; }
        }
        if (um_step_mode) { um_wait = true; sendUsbReport(SW_BTN_NONE, 0x08); }
    }

public:
    // mode: 0 = executa com a repeticao salva, 1 = testa 1 vez, 2 = passo a passo
    bool startUser(uint8_t idx, uint8_t mode, GameState &state) {
        if (idx >= UM_COUNT || state.um.m[idx].n == 0) return false;
        clearQueue(); loop_mode = false;
        um_idx = (int8_t)idx; um_pos = 0; um_cycles = 0; um_started = false;
        um_step_mode = (mode == 2); um_wait = um_step_mode;     // passo a passo comeca pausado
        um_limit = (mode == 0) ? state.um.m[idx].repeat : 1;
        active_slot = USER_MACRO_BASE + idx;
        state.active_macro_slot = (int8_t)active_slot;
        state.generic.macro_executions++;
        return true;
    }
    void userNext() { if (um_idx >= 0 && um_step_mode) um_wait = false; }
    int  userIdx() const { return um_idx; }
    uint8_t userPos() const { return um_pos; }
    uint32_t userCycles() const { return um_cycles; }
    bool userWaiting() const { return um_idx >= 0 && um_step_mode && um_wait && !um_started; }

    // Controle ao vivo: a pagina manda os botoes apertados (mascara) + direcional; vale ~2,5 s sem novas chamadas
    void livePad(uint16_t btns, uint8_t hat, GameState &state) {
        if (!pad_active) { clearQueue(); loop_mode = false; active_slot = -1; state.active_macro_slot = -1; }
        if (btns == 0 && hat == 0x08) { pad_active = false; sendUsbReport(SW_BTN_NONE, 0x08); return; }
        pad_active = true; pad_until = millis() + 2500;
        sendUsbReport(btns, hat);
    }
private:

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
        state.usb_online = TinyUSBDevice.mounted();
        state.mode = SwitchMode::LOCKED_USB_MACRO;
        unsigned long now = millis();
        state.active_macro_slot = static_cast<int8_t>(active_slot);
#ifdef ARDUINO
        state.connected = TinyUSBDevice.mounted();   // cabo USB-C ligado e enumerado
#endif

        if (state.profile == GameProfile::POKEMON_FIRERED && active_slot >= 0 && now - last_hunt_tick >= 1000) {
            last_hunt_tick = now; state.fr.hunt_s++; state.fr.dirty = true;
        } else if (active_slot < 0) {
            last_hunt_tick = now;
        }

        if (pad_active) {                       // gravador: a pagina manda o estado dos botoes ao vivo
            if (now > pad_until) { pad_active = false; sendUsbReport(SW_BTN_NONE, 0x08); }
            return;
        }
        if (um_idx >= 0) { umUpdate(now, state); return; }

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
        } else if (loop_mode && state.profile == GameProfile::POKEMON_FIRERED) {
            frLoop(now, state);
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

    // PARAR TUDO: cancela a fila e os toggles e solta todos os botoes/analogicos
    void stopAll(GameState &state) {
        clearQueue();
        loop_mode = false;
        active_slot = -1;
        state.generic.turbo_enabled = false;
        state.active_macro_slot = -1;
        sendUsbReport(SW_BTN_NONE, 0x08);
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

        if (clicked_slot >= USER_MACRO_BASE) {     // macros do usuario (slots 100..105)
            if (!startUser((uint8_t)(clicked_slot - USER_MACRO_BASE), 0, state)) { active_slot = -1; state.active_macro_slot = -1; }
            return;
        }
        if (state.profile == GameProfile::POKEMON_FIRERED) { frTrigger(clicked_slot, state); return; }

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