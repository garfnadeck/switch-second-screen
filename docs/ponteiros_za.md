# Ponteiros de memória — Pokémon Legends Z-A

Pesquisa e testes de 10/11/2026. Na coluna da posição, os quatro números são saltos seguidos; o `+0x90` vem depois.
Título: `0100F43008C44000`. Cadeias a partir do `main` (comando `pointerPeek` do sys-botbase).

## Publicados, por versão do jogo

| Versão | Base "save" | Time (PartyPointer) | Posição do jogador (+0x90 = x,y,z float) | Fonte |
|---|---|---|---|---|
| 1.0.0 / 1.0.1 (não rotulado, out/2025) — **é a do usuário** | `0x5F0B250` | não publicado | `0x41EC340, 0x248, 0x00, 0x138` | ZAShinyWarper, commit inicial 5d3696c |
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

## Formato dos dados

- Cada Pokémon: bloco de `0x148` bytes no formato PA9 (cifrado e embaralhado, como no PKHeX).
- HP atual e status: fora do bloco de caixa; tamanho da extensão de time a confirmar no PKHeX.
- Posição: três float32 seguidos, sem cifra.

## Fontes

- https://github.com/kwsch/SysBot.NET — `SysBot.Pokemon/LZA/Vision/PokeDataOffsetsLZA.cs`
- https://github.com/berichan/ZAShinyWarper — `ZAShinyWarper/Bot/ZAVision.cs`, `ZAWarpWindow.cs`
