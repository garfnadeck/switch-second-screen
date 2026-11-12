# Ponteiros de memória — Pokémon Legends Z-A

Pesquisa e testes de 10/11/2026. Na coluna da posição, os quatro números são saltos seguidos; o `+0x90` vem depois.
Título: `0100F43008C44000`. Cadeias a partir do `main` (comando `pointerPeek` do sys-botbase).

## Publicados, por versão do jogo

| Versão | Base "save" | Time (PartyPointer) | Posição do jogador (+0x90 = x,y,z float) | Fonte |
|---|---|---|---|---|
| 1.0.0 / 1.0.1 (não rotulado, out/2025) | `0x5F0B250` | não publicado | `0x41EC340, 0x248, 0x00, 0x138` | ZAShinyWarper, commit inicial 5d3696c |
| 1.0.2 | `0x5F0C250` | `0x5F0C250, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0` | `0x41ED340, 0x248, 0x00, 0x138` | SysBot.NET e675d1e; ZAShinyWarper 86674aa |
| 1.0.3 | `0x5F0E250` | `0x5F0E250, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0` | (ver ZAShinyWarper c01c080) | SysBot.NET ae0acb6 |
| 2.0.0 | `0x6105710` | (ver SysBot.NET 839d99c) | `0x47D21A0, 0x248, 0x00, 0x138` | ZAShinyWarper fa2e03b |
| 2.0.2 — **atual do usuário** (`b1f12fd919eae86a`) | `0x610A710` | `0x610A710, 0x18, 0x1B0, 0xF0+i*8, 0x50, 0x30/0x50, 0x0` | `0x47D71A0, 0x248, 0x00, 0x138` | SysBot.NET 8621130 + validado ao vivo (12/11/2026) |

## Hipótese para o jogo base (a testar)

Na 1.0.2 o time e o "shiny stash" usam a mesma base (`0x5F0C250`). Na versão de outubro o stash usa
`0x5F0B250`, então o time provavelmente é `0x5F0B250, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0`.
Isso é inferência, não valor publicado. Não se sabe se os valores de outubro são da 1.0.0 (cartucho)
ou da 1.0.1 (atualização do dia do lançamento).

## Teste no console (10/11/2026)

- sys-botbase instalado (pasta `430000000000000B`), porta 6000 respondendo em 192.168.15.9.
- `getBuildID` do jogo do usuário: `7222E13ECF6ADB32` (versão a identificar; jogo base, sem updates).
- O comando correto é `getTitleID` (com ID maiúsculo). `getTitleId` não é reconhecido e fica sem resposta.

### Posição do jogador no jogo do usuário (build `7222E13ECF6ADB32`, sem updates)

**`pointerPeek 12 0x41EC340 0x248 0x0 0x138 0x90`** → x, y, z em float32. É o ponteiro publicado
pelo ZAShinyWarper para outubro/2025, e foi conferido ao vivo com `tools/za_pos_ao_vivo.py`: os
valores acompanham o personagem a cada passo.

Atenção à forma: são QUATRO saltos seguidos (`[[[[main+0x41EC340]+0x248]+0x0]+0x138]`) e só então
`+0x90`. No `pointerPeek` o último número é somado sem ser seguido, então o `0x90` vai separado.

**Erro registrado (10/11):** a primeira tentativa juntou `0x138 + 0x90 = 0x1C8`, o que pula um
salto. Com isso o ponteiro publicado devolveu zeros e concluiu-se, errado, que a versão do jogo
era outra. A busca `za_scan_pos.py` então achou `0x41E9D20`, que NÃO é a posição: é um valor da
interface que alterna entre dois pontos fixos ao abrir o menu. O jogo do usuário usa os mesmos
endereços publicados para outubro/2025 (a base do save também bate).

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

### Versão do jogo do usuário: 1.0.1

O banco público de cheats (HamletDuFromage/switch-cheats-db, `cheats/0100F43008C44000.json`) rotula
o build `7222E13ECF6ADB32` como **Pokémon Legends: Z-A 1.0.1**. Os cheats desse build são remendos
de código (dinheiro, itens, mega, pulo); nenhum aponta para o time ou para o HP.
Os forks de SysBot para o Z-A (PokeBot_ZA_Compatable, ZA_Sysbot.net) não leem o time.
PokeBot (1.0.2) traz `MenuOffset = 0x5F2E880` (0 = mundo, 1 = menu X, 4 = caixa), útil depois.

### Objeto de Pokémon na memória (confirmado em 10/11/2026)

`[assinatura (ponteiro de código)][ponteiro da extensão 0x10][ponteiro do bloco 0x148]`.
`tools/za_bfs_pokemon.py` achou os dois em branco por essa assinatura (`[BASE, 0x18, 0x110]` e
`[BASE, 0x18, 0x1B0]`). `[BASE, 0x18]` é o começo de uma área geral de memória, não um objeto.
Bloco, extensão e objeto saem do mesmo alocador (`[BASE, 0x18, 0x8]`).

### Time ACHADO na memória (10/11/2026, 23h)

`tools/za_scan_assinatura.py` varreu os 189 MB do alocador e achou 11 Pokémon com dados e 29 em
branco. Entre eles a Chikorita do time: nível 9, HP 23/29, batendo com o jogo. Os outros são
selvagens carregados em volta (Fletchling, Bunnelby, Scatterbug, Weedle, Mareep, Snover).
Confirmado com Pokémon reais: a decifragem (`tools/za_pa9.py`), os campos (HP `0x8A`, nível
`0x148`, HP máximo `0x14A`, apelido `0x58`) e o formato do objeto. Na memória os blocos ficam
CIFRADOS com a soma certa. A "espécie interna" lida bateu com o número da Pokédex nesses casos.
Assinatura do objeto: `main+0x39E5750` (build 1.0.1). Os objetos ficam poucos MB depois do
Pokémon em branco de `[BASE, 0x18, 0x1B0]`.
Falta: a cadeia fixa até o objeto. Ferramentas: `za_dump_mem.py` (copia a memória) e
`za_cadeias.py` (busca reversa de ponteiros, sem rede).

### 11/11/2026: onde o time NÃO está, e o mapa de regiões

- Os objetos de Pokémon achados em 10/11 eram os Pokémon presentes no cenário (inclusive a
  Chikorita andando ao lado do jogador). São descartados quando o cenário muda: na cópia de
  288 MB feita minutos depois (`za_dump_mem.py`) só restava o último Pokémon enfrentado.
- Blocos do save: `[BASE,0x0]`, `[BASE,0x70]`, `[BASE,0xB0,0x978]` (caixas), `[BASE,0xC8,0x40]`,
  `[BASE,0xD0]`, `[BASE,0x100]`, `[BASE,0x38,0x1C0]`, `[BASE,0x38,0x2C0]` apontam para áreas
  vizinhas (página-alinhadas). `za_scan_save.py` varreu 4 MB ali em qualquer alinhamento: só achou
  o Pokémon da caixa 1 (leitura das caixas validada: `pointerPeek 344 0x5F0B250 0xB0 0x978 0x0`,
  com a extensão contígua). O time não fica ali como bloco cifrado.
- Regiões apontadas fora do heap principal (`za_regioes.py`): `0x5CE3..0x5CE5` (32 MB, 661 mil
  ponteiros: objetos vivos), `0x59F8..0x5A2D` (848 MB), `0x5C89..0x5C9B` (288 MB),
  `0x5CA7..0x5CB1` (160 MB). Endereços da sessão de 10-11/11; mudam ao reabrir o jogo.

## Validado ao vivo na 2.0.2 (12/11/2026 — build `b1f12fd919eae86a`)

Com o jogo atualizado para a versão **2.0.2**, todos os ponteiros do time (6 slots) e da posição do
jogador foram validados ao vivo no console (`192.168.15.5`) e integrados ao firmware
(`include/providers/za_memory_map.h` e `include/providers/sysbotbase_provider.h`):

1. **Time (6 slots, `i = 0..5`, `SLOT_OFF = 0xF0 + i * 8` → `0xF0, 0xF8, 0x100, 0x108, 0x110, 0x118`):**
   - **Bloco principal PA9 (`0x148` = 328 bytes, cifrado):**
     `pointerPeek 328 0x610A710 0x18 0x1B0 0x<SLOT_OFF> 0x50 0x30 0x0`
   - **Extensão de time (`0x10` = 16 bytes, cifrada com a mesma semente `chave` dos primeiros 4 bytes do bloco):**
     `pointerPeek 16 0x610A710 0x18 0x1B0 0x<SLOT_OFF> 0x50 0x50 0x0`
   - Validado com os 6 Pokémon reais do time (OT `Laila`):
     - Slot 1 (`0xF0`): Chikorita (`#152`), Nv 10, HP 10/32
     - Slot 2 (`0xF8`): Fletchling (`#661`), Nv 7, HP 24/24
     - Slot 3 (`0x100`): Bunnelby (`#659`), Nv 7, HP 24/24
     - Slot 4 (`0x108`): Scatterbug (`#664`), Nv 6, HP 22/22
     - Slot 5 (`0x110`): Scatterbug (`#664`), Nv 5, HP 20/20
     - Slot 6 (`0x118`): Fletchling (`#661`), Nv 6, HP 22/22

2. **Posição do jogador (`x, y, z` float32):**
   - **`pointerPeek 12 0x47D71A0 0x248 0x0 0x138 0x90`** (`0x47D71A0 = 0x47D21A0 + 0x5000`, mesmo
     deslocamento `+0x5000` da base do save entre a 2.0.0 e a 2.0.2).

## Formato dos dados

- Cada Pokémon: bloco de `0x148` bytes no formato PA9 (cifrado e embaralhado, como no PKHeX).
- Extensão de time: `0x10` bytes em ponteiro separado (`+0x50` do registro do slot), cifrada com o
  mesmo LCG semeado pela `chave` (`uint32_t` nos bytes `0..3` do bloco PA9). Contém nível em `+0x00`
  (`0x148`) e HP máximo em `+0x02` (`0x14A`).
- Posição: três float32 seguidos, sem cifra.

## Fontes

- https://github.com/kwsch/SysBot.NET — `SysBot.Pokemon/LZA/Vision/PokeDataOffsetsLZA.cs`
- https://github.com/berichan/ZAShinyWarper — `ZAShinyWarper/Bot/ZAVision.cs`, `ZAWarpWindow.cs`

## Erros registrados (12/11/2026)

- A cadeia `0x41EC340, 0x248, 0x0` (1.0.1) não resolve na 2.0.2. O ponteiro `0x41EC340` ainda aponta para memória válida, mas o campo `+0x248` é um inteiro (`0x207713`), não um ponteiro. Use a cadeia publicada `0x47D71A0`.
- O objeto `0x4B4A861C40` não é o personagem: 0 de 8 blocos de 64 bytes mudaram ao andar.
- Formato dos comandos: `peekMain` e `peekAbsolute` recebem endereço primeiro e tamanho depois. Com a ordem invertida, o tamanho vira um valor enorme e o jogo trava.
- A busca inicial na web não encontrou a referência SysBot.NET (commit 8621130), que tinha os ponteiros da 2.0.2.

