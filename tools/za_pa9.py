"""Decifragem do bloco de Pokemon do Legends Z-A (formato PA9, mesmo esquema do PK8/PK9 do PKHeX).

Bloco de caixa: 0x148 bytes = cabecalho de 8 + 4 blocos de 0x50, cifrados com XOR de um
gerador linear semeado pela chave (primeiros 4 bytes) e embaralhados conforme a chave.
A soma de verificacao (bytes 6..7) permite confirmar que um trecho de memoria e um Pokemon.
"""
import struct

TAM_CAIXA = 0x148
TAM_BLOCO = 0x50

# Ordem dos 4 blocos para cada valor de embaralhamento (0..31), como no PKHeX
_ORDEM = [
    0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1,
    1, 0, 2, 3, 1, 0, 3, 2, 2, 0, 1, 3, 3, 0, 1, 2, 2, 0, 3, 1, 3, 0, 2, 1,
    1, 2, 0, 3, 1, 3, 0, 2, 2, 1, 0, 3, 3, 1, 0, 2, 2, 3, 0, 1, 3, 2, 0, 1,
    1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1, 0,
    0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1,
    1, 0, 2, 3, 1, 0, 3, 2,
]


def _xor(dados, semente):
    saida = bytearray(dados)
    for i in range(0, len(saida) - 1, 2):
        semente = (semente * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
        chave = (semente >> 16) & 0xFFFF
        valor = struct.unpack_from("<H", saida, i)[0] ^ chave
        struct.pack_into("<H", saida, i, valor)
    return bytes(saida)


def decifrar(bruto):
    """Devolve (dados decifrados e desembaralhados, soma_ok). Aceita 0x148 bytes ou mais."""
    chave = struct.unpack_from("<I", bruto, 0)[0]
    corpo = _xor(bruto[8:TAM_CAIXA], chave)
    soma = sum(struct.unpack_from("<H", corpo, i)[0] for i in range(0, len(corpo), 2)) & 0xFFFF
    soma_ok = soma == struct.unpack_from("<H", bruto, 6)[0]

    sv = (chave >> 13) & 31
    blocos = [corpo[i * TAM_BLOCO:(i + 1) * TAM_BLOCO] for i in range(4)]
    ordenado = b"".join(blocos[_ORDEM[sv * 4 + i]] for i in range(4))

    extra = _xor(bruto[TAM_CAIXA:], chave) if len(bruto) > TAM_CAIXA else b""
    return bruto[:8] + ordenado + extra, soma_ok


def especie(dados):
    """Numero interno da especie (bloco A, deslocamento 0x08)."""
    return struct.unpack_from("<H", dados, 0x08)[0]


def validar(bruto):
    """Devolve (dados em claro, modo) se 'bruto' for um Pokemon valido, senao (None, None).

    modo = 'cifrado' (como no save) ou 'aberto' (ja decifrado na memoria).
    """
    if len(bruto) < TAM_CAIXA or bruto[4:6] != b"\0\0":
        return None, None
    claro, soma_ok = decifrar(bruto)
    if soma_ok and especie(claro) != 0:
        return claro, "cifrado"
    soma = sum(struct.unpack_from("<H", bruto, i)[0] for i in range(8, TAM_CAIXA, 2)) & 0xFFFF
    if soma == struct.unpack_from("<H", bruto, 6)[0] and soma != 0 and especie(bruto) != 0:
        return bytes(bruto), "aberto"
    return None, None
