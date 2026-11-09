# Pinagem da placa ES3C35P (ESP32-S3 N16R8, tela 3.5" 320x480)

Versão em HTML interativa: `docs/pinagem_es3c35p.html`. Este arquivo tem o mesmo conteúdo.
Header com os pinos: `include/hardware/es3c35p_pins.h`.

Fontes: esquema do fabricante (folhas ESP32-S3, LCD&TP, TF card, USB, botões, áudio) e o pacote ST77922 do fabricante (`bsp_display.c`, `bsp_touch.c`). Validação na bancada: repositório `teste-tela-st77922`.

## Tela ST77922 (QSPI)

| Sinal | GPIO | Status |
|---|---|---|
| CS | IO10 | Validado |
| CLK | IO12 | Validado |
| D0 / D1 / D2 / D3 | IO11 / IO13 / IO14 / IO9 | Validado |
| RST | EN | Validado (reset por software) |
| Backlight | IO41 | Validado (alto liga) |

Modo do módulo (conector QD1, HMX035CTFT-001): IM2 = GND, IM1 = 3,3 V, IM0 sem ligação. Modo QSPI = `0 1 1`. Funciona sem jumper com a sequência de init do fabricante (79 comandos, clock 80 MHz).

Formato: RGB565 com byte alto primeiro. O ESP32 é little-endian, então os bytes são trocados antes do envio.

## Touch capacitivo (I2C0)

| Sinal | GPIO | Status |
|---|---|---|
| SDA / SCL | IO38 / IO39 | Validado |
| RST | IO48 | Validado (nível baixo) |
| INT | IO47 | Não usado (não confiável) |

Endereço do chip: **0x55**, registradores de 16 bits (não é o FT6336G 0x38 citado em documentos antigos). Outros endereços no barramento: 0x18 (codec ES8311) e 0x28 (não identificado).

**Comportamento:** o primeiro toque é reportado; depois o chip para e o INT fica em 0. **Workaround validado:** se o INT ficar baixo por mais de 30 ms sem novos dados, pulso de RST (10 ms baixo, 100 ms alto). Cada reset leva ~110 ms, com toques perdidos nesse intervalo. Causa raiz não identificada.

## Cartão microSD (SDIO 4 bits)

| Sinal | GPIO | Status |
|---|---|---|
| CLK / CMD | IO5 / IO4 | Validado |
| D0 / D1 / D2 / D3 | IO6 / IO7 / IO2 / IO3 | Validado |

Cartão de 14,8 GB testado a 20 MHz. Não há pino de detecção de cartão (CD/DAT3 é o DATA3).

## Áudio I2S (não testado)

| Sinal | GPIO |
|---|---|
| Habilita amplificador (ativo baixo) | IO1 |
| MCLK / BCLK / LRCK | IO17 / IO18 / IO21 |
| DOUT / DIN | IO15 / IO16 |

## Outros

| Função | GPIO |
|---|---|
| LED RGB | IO40 |
| BOOT | IO0 |
| Bateria (ADC, divisor 100k/100k) | IO8 |
| UART0 RX / TX | IO43 / IO44 |
| Extensão livre | IO45, IO46 |

## Pinos a evitar

- IO26 a IO32: flash interna.
- IO33 a IO37: PSRAM octal.
- IO19 e IO20: USB nativo (conflita com a porta usada para gravar).
