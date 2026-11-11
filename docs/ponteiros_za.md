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

### Estrutura vista em [BASE, 0x18] (dump de 10/11/2026)

O objeto `[0x5F0B250, 0x18]` tem registros de `0xA0` bytes (em `+0x0A0` e `+0x140`). Cada registro
anota três áreas com tamanho: `0x148` (alinhamento 4), `0x10` (2) e `0x28` (8). São os tamanhos do
bloco de Pokémon e da extensão de time, guardados em áreas SEPARADAS (por isso a busca por um
bloco contínuo de `0x158` não achou nada). Ponteiros: bloco em `registro+0x30`, extensão em
`registro+0x50`. A testar com `tools/za_ler_pokemon.py`.
A cadeia da 1.0.2 passa por `+0x1B0` (a área de `0x28` do segundo registro) e quebra em `+0xF0`.

### Time: o que já foi tentado e NÃO achou a Chikorita (10/11/2026)

Jogo: Chikorita nv 7, HP 25/25, única no time; treinador "Laila".

| Tentativa | Ferramenta | Resultado |
|---|---|---|
| Cadeia da 1.0.2 e variações de 1 e 2 saltos | `za_scan_time.py` | não resolve; quebra no elo 3 (`+0xF0` = 0), com menu aberto ou fechado |
| Blocos e ponteiros a 2 níveis do objeto do save | `za_scan_bloco.py` | nada |
| Soma de verificação em 40 MB em volta das âncoras | `za_scan_heap.py` | só coincidências (soma de 16 bits) |
| Chaves dos blocos do save (`0x3AA1A9AD` etc.) em 32 MB | `za_scan_chave.py` | nenhuma ocorrência |
| Registros de `[BASE, 0x18]` | `za_ler_pokemon.py` | dois Pokémon EM BRANCO (área de trabalho, não o time) |
| Blocos válidos no reservatório (6 MB) | `za_scan_core.py` | nada |
| Nome "Chikorita" em UTF-16 em 48 MB | `za_scan_nome.py` | nenhuma ocorrência |

Conclusão: o time não está nas regiões próximas aos dados do save. Varrer a memória inteira
pelo Wi-Fi é lento demais (~270 KB/s). Caminho recomendado: achar o endereço do HP com uma
ferramenta de busca no próprio console (Breeze / EdiZon SE) e depois montar a cadeia.

## Formato dos dados

- Cada Pokémon: bloco de `0x148` bytes no formato PA9 (cifrado e embaralhado, como no PKHeX).
- HP atual e status: fora do bloco de caixa; tamanho da extensão de time a confirmar no PKHeX.
- Posição: três float32 seguidos, sem cifra.

## Fontes

- https://github.com/kwsch/SysBot.NET — `SysBot.Pokemon/LZA/Vision/PokeDataOffsetsLZA.cs`
- https://github.com/berichan/ZAShinyWarper — `ZAShinyWarper/Bot/ZAVision.cs`, `ZAWarpWindow.cs`
