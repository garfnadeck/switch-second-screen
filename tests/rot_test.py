# Verifica o mapeamento de rotacao usado em es3c35p_lvgl.cpp (paisagem 480x320 -> retrato 320x480)
import random
PW = 320
def native_of(lx, ly):            # pixel logico -> pixel nativo
    return (PW - 1 - ly, lx)
def check(x1, y1, x2, y2):
    lw, lh = x2 - x1 + 1, y2 - y1 + 1
    nx1 = (PW - 1) - y2; nx2 = (PW - 1) - y1
    ny1 = x1; ny2 = x2
    nw, nh = nx2 - nx1 + 1, ny2 - ny1 + 1
    assert nw == lh and nh == lw
    seen = {}
    for j in range(nh):
        for i in range(nw):
            idx = (lh - 1 - i) * lw + j          # indice no buffer logico da area
            lx = x1 + idx % lw; ly = y1 + idx // lw
            nxp, nyp = native_of(lx, ly)
            assert (nxp, nyp) == (nx1 + i, ny1 + j), (lx, ly, nxp, nyp, i, j)
            assert idx not in seen; seen[idx] = 1
    assert len(seen) == lw * lh
for _ in range(2000):
    x1 = random.randint(0, 479); x2 = random.randint(x1, 479)
    y1 = random.randint(0, 319); y2 = min(319, y1 + random.randint(0, 24))
    check(x1, y1, x2, y2)
check(0, 0, 479, 24); check(0, 295, 479, 319); check(0, 0, 0, 0)
print("mapeamento OK (2000 areas aleatorias + bordas)")
