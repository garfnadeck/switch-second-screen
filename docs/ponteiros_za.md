# Ponteiros de memória — Pokémon Legends Z-A

Pesquisa de 10/11/2026. Nenhum valor foi testado no console ainda.
Título: `0100F43008C44000`. Cadeias a partir do `main` (comando `pointerPeek` do sys-botbase).

## Publicados, por versão do jogo

| Versão | Base "save" | Time (PartyPointer) | Posição do jogador (+0x90 = x,y,z float) | Fonte |
|---|---|---|---|---|
| 1.0.0 / 1.0.1 (não rotulado, out/2025) | `0x5F0B250` | não publicado | `0x41EC340, 0x248, 0x00, 0x138` | ZAShinyWarper, commit inicial 5d3696c |
| 1.0.2 | `0x5F0C250` | `0x5F0C250, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0` | `0x41ED340, 0x248, 0x00, 0x138` | SysBot.NET e675d1e; ZAShinyWarper 86674aa |
| 1.0.3 | `0x5F0E250` | `0x5F0E250, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0` | (ver ZAShinyWarper c01c080) | SysBot.NET ae0acb6 |
| 2.0.0 | `0x6105710` | (ver SysBot.NET 839d99c) | `0x47D21A0, 0x248, 0x00, 0x138` | ZAShinyWarper fa2e03b |
| 2.0.2 | `0x610A710` | `0x610A710, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0` | — | SysBot.NET 8621130 |

## Hipótese para o jogo base (a testar)

Na 1.0.2 o time e o "shiny stash" usam a mesma base (`0x5F0C250`). Na versão de outubro o stash usa
`0x5F0B250`, então o time provavelmente é `0x5F0B250, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0`.
Isso é inferência, não valor publicado. Não se sabe se os valores de outubro são da 1.0.0 (cartucho)
ou da 1.0.1 (atualização do dia do lançamento).

## Teste no console (10/11/2026)

- sys-botbase instalado (pasta `430000000000000B`), porta 6000 respondendo em 192.168.15.9.
- `getBuildID` do jogo do usuário: `7222E13ECF6ADB32` (versão a identificar; jogo base, sem updates).
- O comando correto é `getTitleID` (com ID maiúsculo). `getTitleId` não é reconhecido e fica sem resposta.

### Posição achada no jogo do usuário (build `7222E13ECF6ADB32`, sem updates)

`0x41E9D20, 0x248, 0x0, 0x138`, depois `+0x90` (x, y, z float32). Achado com `tools/za_scan_pos.py`:
único candidato, e os valores mudaram ao andar (-876,10 / 0 / -757,38 → -903,71 / 0 / -487,93).
A base ficou `0x2620` abaixo da publicada para outubro/2025.

### Base do save achada no jogo do usuário

`0x5F0B250` (igual à publicada para outubro/2025). Confirmada com `tools/za_scan_base.py`:
`[0x5F0B250, 0xA0, 0x40]` devolve o bloco do treinador, com o nome em texto puro (`0x10`, UTF-16).
`[0x5F0B250, 0xB0, 0x978, 0x0]` (caixa 1) resolve e devolve um espaço vazio válido.
A cadeia do time da 1.0.2 (`0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0`) NÃO resolve nesta versão.

Campos do bloco decifrado (PKHeX `PA9.cs`): HP atual `0x8A`, status `0x90`, nível `0x148`,
HP máximo `0x14A` (os dois últimos na extensão de time, total `0x158`).

## Formato dos dados

- Cada Pokémon: bloco de `0x148` bytes no formato PA9 (cifrado e embaralhado, como no PKHeX).
- HP atual e status: fora do bloco de caixa; tamanho da extensão de time a confirmar no PKHeX.
- Posição: três float32 seguidos, sem cifra.

## Fontes

- https://github.com/kwsch/SysBot.NET — `SysBot.Pokemon/LZA/Vision/PokeDataOffsetsLZA.cs`
- https://github.com/berichan/ZAShinyWarper — `ZAShinyWarper/Bot/ZAVision.cs`, `ZAWarpWindow.cs`
