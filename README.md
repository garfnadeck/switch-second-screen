# SwitchHUD (NS Companion) — v1.1.2

> 🇧🇷 **Português** | [🇺🇸 English](README.en.md)

Segunda tela em tempo real e painel de utilitários para **Nintendo Switch (Atmosphère + `sys-botbase`)**, feito em **Flutter** para **iOS** e **Android**.

[![Release v1.1.2](https://img.shields.io/badge/Release-v1.1.2-00B4D8?style=for-the-badge)](https://github.com/garfnadeck/switch-second-screen/releases/tag/v1.1.2)

---

## Jogos Suportados

### 1. The Legend of Zelda: Breath of the Wild (`v1.6.0` — BuildID `8E9978D50BDD20B4`)
- **Auto-Boost ao conectar:** assim que o save é detectado na RAM, aplica automaticamente:
  - **30 Corações** (`120` quartos)
  - **3 Rodas de Stamina** (`3000.0`) — com restauração automática ao entrar/sair de Shrines ou fazer viagem rápida
  - **999.999 Rupees**
  - **Max Slots (`20` Armas / `14` Arcos / `20` Escudos)** via `SaveMgr::TriggerParam`
- **Bolsa & Durabilidade (`PauseMenuDataMgr`):**
  - **`+ Add Item`:** cria novos itens direto na bolsa (incluindo **Master Sword**, **Hylian Shield**, arcos, escudos, comidas e materiais)
  - **`Reparar Tudo (999)`:** restaura a durabilidade de todas as armas, arcos e escudos equipáveis
  - **`999 Flechas` / `999 Materiais`:** maximiza stacks instantaneamente
  - **`Trocar`:** converte qualquer slot existente em outro item
- **Aba Roupas (12 Sets):** adiciona peças novas sem apagar sua coleção atual e equipa na bolsa (selecione no menu `+` ou faça Save & Load / Viagem Rápida para atualizar o modelo 3D no Link).
- **Mapa GPS ao vivo:** acompanha a posição do Link em tempo real com marcadores de **Shrines**, **Torres**, **Koroks** e **Pins personalizados**.
  > **Nota sobre Teleporte:** a engine de física Havok do BotW sobrescreve a posição do Link a cada frame; por isso o mapa funciona como GPS ao vivo, e a viagem rápida é feita pelo mapa (`-`) do próprio jogo.
- **Progresso em Hyrule:** leitura automática de Shrines, Koroks e Divine Beasts.

### 2. Pokémon Legends: Z-A
- **HUD da Party ao vivo:** HP, status, nível, IVs/EVs e alertas de **Shiny** com **Auto-Stop** opcional.
- **Mapa ao vivo:** camadas de **Lumiose**, **Lysandre** e **Sewers** com rastreamento de coordenadas.
- **Macros & Automação:** atalhos para farm, captura, cura rápida e controle virtual.

---

## Recursos Gerais do App
- **Auto-Reconexão Automática:** reconecta automaticamente ao voltar do segundo plano ou caso o Wi-Fi/Switch reinicie.
- **Android Foreground Service:** mantém a conexão TCP e o Auto-Boost ativos em segundo plano no Android com notificação fixa.
- **iOS:** pausa conexões em segundo plano por política do sistema, mas reconecta instantaneamente ao reabrir o app.
- **FAQ & Créditos integrados:** manual passo a passo dentro da aba **Config**.

---

## Como Usar (Passo a Passo)

1. **Prepare o Nintendo Switch:**
   - Instale o [`sys-botbase`](https://github.com/olliz0r/sys-botbase) no cartão SD em `atmosphere/contents/430000000000000B/`.
   - Conecte o Switch e o celular na **mesma rede Wi-Fi**.
2. **Instale o SwitchHUD:**
   - Baixe o `app-release.apk` (Android) ou o `SwitchHUD-ios-unsigned.ipa` (iOS via AltStore/Sideloadly/Xcode) na página de **[Releases](https://github.com/garfnadeck/switch-second-screen/releases/tag/v1.1.2)**.
3. **Conecte:**
   - Abra a aba **Rede**, informe o IP do Switch (porta padrão `6000`) ou toque em **Buscar Rede** e clique em **Conectar**.

### O que fica salvo no `.sav`?
- **Salva no `.sav` ao salvar o jogo:** Armaduras, Armas, Arcos, Escudos e Itens criados/editados na bolsa.
- **Reaplicado pelo Auto-Boost ao conectar:** Corações máximos (`30❤`), Stamina máxima (`3 rodas`), **999.999 Rupees** e Slots máximos do Hestu (`20/14/20`) — ao reiniciar o jogo, esses valores voltam ao estado do save e são reaplicados automaticamente pelo app ao conectar.

---

## Compilar pelo Código-Fonte

```bash
cd flutter_app
flutter pub get
flutter analyze
flutter test

# Android APK
flutter build apk --release

# iOS (macOS + Xcode)
flutter build ios --release
```

---

## Créditos
- **Idealização e Desenvolvimento:** [garfnadeck](https://github.com/garfnadeck)
- **Repositório:** https://github.com/garfnadeck/switch-second-screen
- **Protocolo Homebrew:** [`sys-botbase`](https://github.com/olliz0r/sys-botbase) (`olliz0r`)
