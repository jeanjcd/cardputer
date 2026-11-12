"""Glifos "z3ck" (grande) e "031" (pequeno, vertical) no mesmo estilo geométrico do logo AREA 512.

Coordenadas 2D na VISTA FRONTAL da placa C2 (u: esquerda→direita, v: baixo→cima, mm),
idênticas às do STL original via  u = 204 - X,  v = Z  (frame do STL CaseC2 em Model/Type*/STL).

Regras de estilo extraídas do original (medidas no STL):
  - traço grande 2,0 mm; altura total 16 mm (v 27,4..43,4); altura-x 10 mm (v 27,4..37,4)
  - diagonais a ~59,6° (pernas do "A"), terminais chanfrados paralelos às diagonais
  - texto pequeno: traço 1,0 mm, glifo de 6 mm girado 90° (lido de cima para baixo), u 65,5..71,5
  - a C2 é uma chapa de 1 mm com as letras VAZADAS (a D1 preta aparece por trás): nenhum glifo pode
    criar ilha de material solta -> o "0" usa pontes de estêncil.
"""
import numpy as np
import trimesh
from shapely.geometry import Polygon, box, MultiPolygon
from shapely.ops import unary_union
from shapely import affinity

BASE, XH, TOP = 27.4, 37.4, 43.4          # linha de base, altura-x, topo
W = 2.0                                     # traço grande
ANG = np.radians(59.6)                      # ângulo das diagonais (pernas do A)
HT = W / np.sin(ANG)                        # largura horizontal de um traço diagonal

def original_letters(c2_stl_path):
    """Extrai os polígonos das letras do C2 original (corte no meio do bolsão, y=0.5)."""
    m = trimesh.load(c2_stl_path)
    s = m.section(plane_origin=[0, 0.5, 0], plane_normal=[0, 1, 0])
    polys = [Polygon(np.c_[204 - e[:, 0], e[:, 2]]) for e in s.discrete]
    def pick(u0, u1, v0, v1):
        for p in polys:
            b = p.bounds
            if abs(b[0] - u0) < 0.3 and abs(b[2] - u1) < 0.3 and abs(b[1] - v0) < 0.3 and abs(b[3] - v1) < 0.3:
                return p
        raise KeyError((u0, u1, v0, v1))
    return {
        "E_top": pick(36.95, 49.95, 40.40, 43.40),
        "E_low": pick(36.95, 47.15, 27.40, 37.40),   # = "c" minúsculo
        "one":   pick(65.50, 71.50, 34.80, 36.80),   # "1" pequeno do "512"
        "all_letters": [p for p in polys if p.bounds[1] > 26 and p.bounds[3] < 45],
    }

def _band(u_at_base, v0, v1, slope_sign=+1):
    """Faixa diagonal de traço W entre v0 e v1; borda esquerda passa por (u_at_base, v0)."""
    du = (v1 - v0) / np.tan(ANG) * slope_sign
    return Polygon([(u_at_base, v0), (u_at_base + HT, v0), (u_at_base + HT + du, v1), (u_at_base + du, v1)])

def glyph_z(u0):
    """z minúsculo (altura-x): barras com terminais chanfrados + diagonal a 59,6°."""
    h = XH - BASE
    dx = (h - 0) / np.tan(ANG)
    width = dx + HT
    top = Polygon([(u0 + (W / np.tan(ANG)), XH - W), (u0 + width, XH - W), (u0 + width, XH), (u0 + 0, XH)])
    # terminal esquerdo da barra superior chanfrado (paralelo à diagonal)
    top = Polygon([(u0 + W / np.tan(ANG), XH - W), (u0 + width - 0.01, XH - W), (u0 + width, XH), (u0, XH)])
    bot = Polygon([(u0, BASE), (u0 + width - W / np.tan(ANG), BASE), (u0 + width, BASE + W), (u0, BASE + W)])
    diag = _band(u0, BASE, XH)
    g = unary_union([top, bot, diag]).intersection(box(u0 - 1, BASE, u0 + width + 1, XH))
    return g, width

def glyph_3(u0, E_top, E_low):
    """3 = "E" do AREA espelhado horizontalmente (barra superior destacada + bojo inferior)."""
    g = unary_union([E_top, E_low])
    b = g.bounds
    g = affinity.scale(g, xfact=-1, yfact=1, origin=((b[0] + b[2]) / 2, 0))
    b = g.bounds
    # legibilidade: prolonga o gancho superior direito até 1 mm acima da barra do meio (corte estêncil)
    mt = affinity.scale(E_top, xfact=-1, yfact=1, origin=((b[0] + b[2]) / 2, 0))
    # largura real do traço do gancho medida logo acima da ponta dele (v = 40,6)
    from shapely.geometry import LineString
    cut = mt.intersection(LineString([(b[0] - 5, 40.6), (b[2] + 5, 40.6)])).bounds
    hook = box(cut[0], XH + 1.0, cut[2], 40.8)
    g = unary_union([g, hook])
    return affinity.translate(g, xoff=u0 - b[0]), b[2] - b[0]

def glyph_c(u0, E_low):
    """c = parte inferior do "E" original (já é um c minúsculo com altura-x 10 mm)."""
    b = E_low.bounds
    return affinity.translate(E_low, xoff=u0 - b[0]), b[2] - b[0]

def glyph_k(u0):
    """k: haste vertical inteira + braço (sobe a 59,6°) e perna (desce a -59,6°), terminais horizontais."""
    stem = box(u0, BASE, u0 + W, TOP)
    vj = BASE + 4.6                                   # altura do encontro braço/perna na haste
    arm = _band(u0 + W - HT * 0.55, vj - 1.0, XH, +1)  # braço superior até a altura-x
    leg_top = vj + 1.0
    du = (leg_top - BASE) / np.tan(ANG)
    leg = Polygon([(u0 + W - 0.2, leg_top), (u0 + W - 0.2 + HT, leg_top), (u0 + W - 0.2 + HT + du, BASE), (u0 + W - 0.2 + du, BASE)])
    g = unary_union([stem, arm, leg]).intersection(box(u0 - 1, BASE, u0 + 30, TOP))
    b = g.bounds
    return g, b[2] - u0

def big_word(E_top, E_low, gaps=(1.2, 2.3, 2.3), center_u=37.865):
    """Monta 'z3ck' centralizado na mesma posição horizontal do 'AREA' original (u 12,50..63,23).
    gaps = espaçamento (kerning) z-3, 3-c, c-k."""
    parts, u = [], 0.0
    gz, wz = glyph_z(u); parts.append(gz); u += wz + gaps[0]
    g3, w3 = glyph_3(u, E_top, E_low); parts.append(g3); u += w3 + gaps[1]
    gc, wc = glyph_c(u, E_low); parts.append(gc); u += wc + gaps[2]
    gk, wk = glyph_k(u); parts.append(gk); u += wk
    word = unary_union(parts)
    b = word.bounds
    return affinity.translate(word, xoff=center_u - (b[0] + b[2]) / 2)

# ---------------- texto pequeno vertical "031" ----------------
SW = 1.0                                     # traço pequeno
U0, GH = 65.5, 6.0                           # base (u) e altura do glifo (ao longo de u)

def _small_to_plate(g, v_top):
    """glifo local (x direita, y cima, altura GH) -> placa: u = U0 + y ; v = v_top - x (lido de cima p/ baixo)."""
    return affinity.affine_transform(g, [0, 1, -1, 0, U0, v_top])

PONTE = 1.0                                   # largura das pontes de estêncil (mm)

def small_0(gw=4.6):
    """0 estêncil: a C2 é chapa de 1 mm com letras VAZADAS, então o miolo precisa de pontes (em cima e embaixo)."""
    outer = box(0, 0, gw, GH).buffer(-1.4).buffer(1.4)
    inner = box(SW, SW, gw - SW, GH - SW).buffer(-0.4).buffer(0.4)
    ring = outer.difference(inner)
    pontes = unary_union([box(gw / 2 - PONTE / 2, -1, gw / 2 + PONTE / 2, SW + 0.01),
                          box(gw / 2 - PONTE / 2, GH - SW - 0.01, gw / 2 + PONTE / 2, GH + 1)])
    return ring.difference(pontes), gw

def small_3(gw=4.6):
    r = 1.3
    frame = box(0, 0, gw, GH).buffer(-r).buffer(r)               # cantos arredondados
    top = box(0, GH - SW, gw, GH)
    bot = box(0, 0, gw, SW)
    right = box(gw - SW, 0, gw, GH)
    mid = box(1.4, GH / 2 - SW / 2, gw, GH / 2 + SW / 2)
    g = unary_union([top, bot, right, mid]).intersection(frame)
    # remove o canto esquerdo arredondado (o 3 é aberto à esquerda): recompõe barras retas à esquerda
    g = unary_union([g, box(0, GH - SW, gw - r, GH), box(0, 0, gw - r, SW)])
    return g, gw

def small_word(one_poly, v_center=35.4, gap=1.0):
    """'031' lido de cima para baixo; reaproveita o '1' original."""
    g0, w0 = small_0(); g3, w3 = small_3()
    ob = one_poly.bounds; w1 = ob[3] - ob[1]                      # largura do "1" ao longo de v
    total = w0 + gap + w3 + gap + w1
    v_top = v_center + total / 2
    a = _small_to_plate(g0, v_top)
    b = _small_to_plate(g3, v_top - w0 - gap)
    one = affinity.translate(one_poly, yoff=(v_top - w0 - gap - w3 - gap) - ob[3])
    return unary_union([a, b, one])
