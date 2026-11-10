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

## Formato dos dados

- Cada Pokémon: bloco de `0x148` bytes no formato PA9 (cifrado e embaralhado, como no PKHeX).
- HP atual e status: fora do bloco de caixa; tamanho da extensão de time a confirmar no PKHeX.
- Posição: três float32 seguidos, sem cifra.

## Fontes

- https://github.com/kwsch/SysBot.NET — `SysBot.Pokemon/LZA/Vision/PokeDataOffsetsLZA.cs`
- https://github.com/berichan/ZAShinyWarper — `ZAShinyWarper/Bot/ZAVision.cs`, `ZAWarpWindow.cs`
