# Teste de bancada: tela ST77922

Teste isolado da tela 3.5" ESP32-S3 (320×480, QSPI) e do PSRAM, em ESP-IDF via PlatformIO.
Não faz parte do firmware principal.

## Como rodar (no Mac)

```
cp -R bringup/tela_st77922 ~/projetos/teste-tela   # ou abrir a pasta direto
cd ~/projetos/teste-tela
pio run -t upload --upload-port /dev/cu.usbmodem112301
pio device monitor -b 115200
```

Se o upload falhar, segure **BOOT**, aperte **RESET**, solte o BOOT e rode de novo.

## O que esperar no monitor

1. `PSRAM encontrada: 8192 KB` (ou próximo). Se aparecer `NAO encontrada`, trocar `CONFIG_SPIRAM_MODE_OCT` por `CONFIG_SPIRAM_MODE_QUAD` em `sdkconfig.defaults`.
2. `Backlight ligado (IO41)`.
3. `Tela inicializada (320x480)`.
4. A tela alterna vermelho, verde, azul e branco a cada 2 s.

## Se a tela ficar preta ou com cor errada

- Preta: confira o backlight (IO41) e os pinos QSPI. O init do componente pode precisar do exemplo oficial (`init_cmds`).
- Cores trocadas (vermelho aparece azul): trocar `LCD_RGB_ELEMENT_ORDER_RGB` por `LCD_RGB_ELEMENT_ORDER_BGR`.
- Imagem deslocada ou espelhada: depende do componente e do MADCTL. Anotar e ajustar depois.

## Pendências desta fase

- Confirmar o nome exato do header (`esp_lcd_st77922.h`) e a configuração do io QSPI. A documentação do componente usa uma macro `ST77922_PANEL_IO_QSPI_CONFIG`, que não consegui ver a definição. Se o build reclamar, copiar a configuração do exemplo do componente.
- Confirmar que o init padrão do componente basta para a tela.
- Confirmar o modo do PSRAM (OCT ou QUAD).
- Touch (FT6336G, IO38/39/47/48) fica para o próximo teste.
