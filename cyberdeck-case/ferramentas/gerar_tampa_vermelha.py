"""Tampa para a TFT 2,8" vermelha cortada (MSP2807 / "2.8\" TFT SPI 240x320 V1.2", 81 x 51 mm) -> kit TypeR.

Uso:  python -I -X utf8 ferramentas/gerar_tampa_vermelha.py            (padrão: tampa FECHADA com a C2 "z3ck 031")
      python -I -X utf8 ferramentas/gerar_tampa_vermelha.py --aberta   (variante sem fundo, sem C2/C3)
Pré-requisito: versao_z3ck031/montagem/TypeA e STL_TypeA (gerados por gerar_z3ck031.py).

Versão FECHADA (padrão) - mesma espessura e mesmo visual do TypeA, a placa traseira C2 "z3ck 031" não muda:
  - CaseD1R: D1 original com o rebaixo da tela refeito para a pilha de 5,6 mm da placa vermelha
    (os apoios antigos recuam até o verso da PCB), alívios rasos no fundo para os componentes do verso
    (chip do touch, transistor, regulador, resistores), 2 furos-guia para M2x4 (entram pela frente,
    como no TERM512) e uma passagem de fios para a fenda da dobradiça. Fora disso é idêntica à D1.
  - CaseC3R: C3 com o mesmo alívio para os componentes que ficam atrás da janela do fundo.
  - CaseD2R_z3ck031: moldura do lado dos pinos (substitui D2) com 2 furos rebaixados para M2x4, canal para os fios soldados
    na FRENTE da placa (descem até a fenda da dobradiça esquerda) e "z3ck 031" gravado (opcional).
  - Saem do kit: D2, D3 e o módulo AMS1117 (a placa vermelha tem regulador 3,3 V). O slot SD precisa ser removido.

Referencial "fechado" (tampa fechada sobre o teclado), o mesmo de gerar_z3ck031 depois de girar 135°:
  X = largura (0..84), Y = espessura (frente do vidro em Y=20,09, fundo da D1 em Y=27,29), Z = comprimento
  (dobradiça em Z≈-8, borda das travas em Z≈58). Visto de frente com a tampa aberta: X para a direita, Z para cima.

Medidas da placa (desenho oficial LCDwiki MSP2807 + foto/medidas do usuário): PCB 81 x 51 mm depois do corte,
painel (vidro+touch) 50 x 69,2 mm, pilha vidro->verso da PCB 5,6 mm, AA 43,2 x 57,6 mm.
"""
import os, sys, json, shutil
import numpy as np
import trimesh
import manifold3d as mf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gerar_z3ck031 as G           # utilidades (to_mf, to_tm, box, ok, export_ok, PIVO)
import logo_z3ck as LOGO

ROOT, OUT = G.ROOT, G.OUT
ASM_A = os.path.join(OUT, "montagem", "TypeA")
STL_A = os.path.join(OUT, "STL_TypeA")
ASM_R = os.path.join(OUT, "montagem", "TypeR")
STL_R = os.path.join(OUT, "STL_TypeR")

# ------------------------------------------------------------------ parâmetros (referencial fechado, mm)
BOLSO = dict(x=(1.30, 82.70), z=(-4.07, 52.13))   # bolso da placa na D1 original (medido)
Y_FRENTE = 20.09          # frente do vidro / face da D2 (igual ao TypeA: mesma folga para as teclas)
Y_FUNDO = 27.30           # plano traseiro (D1 original: 27,286)
Y_FUNDO_INT = 26.286      # face interna da parede do fundo da D1 (1,0 mm de espessura)
ESP_PAINEL = 4.0          # touch 1,2 + LCD 2,3 + fita 0,5 (desenho MSP2807)
ESP_PCB = 1.6
FOLGA_Y = 0.0             # vidro exatamente no plano do vidro original (TypeA)
Y_APOIO = Y_FRENTE + ESP_PAINEL + ESP_PCB + FOLGA_Y   # onde o verso da PCB encosta (apoios/abas)
PCB = dict(x0=1.45, L=81.0, z0=1.03, W=51.0)          # placa cortada; lado dos pinos em X baixo (como o TypeA)
MARGEM_PAINEL = (10.4, 11.4)   # borda dos pinos -> início do vidro: desenho (86 mm) / placa de 87 mm do usuário
AA_DO_PAINEL = 8.7             # início do vidro (lado dos pinos) -> início da imagem
AA = (57.6, 43.2)
FUROS_PCB = [(7.5, 44.9)]   # (borda dos pinos -> centro do furo, distância entre furos) medidos na foto da placa do usuário
# (desenho oficial da placa de 86 mm: 6,92 / 44,0). CONFIRMAR com paquímetro antes de imprimir.
FURO_X = PCB["x0"] + 7.55                      # eixo dos parafusos M2 (furo da placa Ø3,2 -> ±0,55 mm de tolerância)
FURO_DZ = 22.25
Z_MEIO = PCB["z0"] + PCB["W"] / 2
FUROS = [(FURO_X, Z_MEIO - FURO_DZ), (FURO_X, Z_MEIO + FURO_DZ)]
PINOS = dict(x=(PCB["x0"] + 0.4, PCB["x0"] + 4.2), z=(9.3, 45.8))   # fileira de 14 pinos
D2 = dict(x=(1.40, PCB["x0"] + MARGEM_PAINEL[0] - 0.25), z=(BOLSO["z"][0] + 0.10, PCB["z0"] + PCB["W"]))
GRAVACAO = 0.6            # profundidade do logo na face da D2R (troca de filamento nessa altura = logo bicolor)
PILOTO = (0.85, 3.2)      # furo-guia M2 (raio; profundidade na D2R da variante aberta)
PARAF_L = 4.0             # (fechada) M2x4 cabeça cilíndrica pela frente: D2R -> furo da placa -> rosca na D1R
Y_PONTA = 26.85           # ponta do parafuso: deixa 0,39 mm (2 camadas) de fundo inteiro na D1
REBAIXO = (2.2, Y_PONTA - PARAF_L)   # raio e fundo (Y) do rebaixo da cabeça na frente da D2R
PASSAGEM = dict(x=(1.6, 4.0), y=(Y_FRENTE, 22.3), z=(-5.3, -3.9))  # janela na parede fina entre o canto e a fenda
# componentes no verso da placa (posições estimadas da foto, com margem de ~2 mm; alturas de catálogo)
COMP = [dict(nome="U2_touch_Q1_U1", x=(14.0, 35.0), z=(7.0, 38.0), h=1.30),   # TSSOP-16, SOT-23, LDO, R/C
        dict(nome="R1_R3", x=(42.0, 57.0), z=(19.0, 29.0), h=0.60)]            # 0603
ALIVIO_GERAL = 26.60       # fundo rebaixado 0,31 mm sob toda a placa (folga de 0,8 mm para qualquer 0603/0805)
ALIVIO_FUNDO = 27.00       # alívio dos componentes altos (pele de 0,29 mm no fundo da D1, coberta pela C2)
FENDA_ESQ = dict(x=(BOLSO["x"][0], 5.6), z=(BOLSO["z"][0], PCB["z0"]))   # canto livre: fios -> fenda da dobradiça
SD = dict(x=(35.8, 62.5), z=(32.6, 50.7), h=3.0)          # slot SD (só na variante aberta; na fechada é removido)
# variante aberta
ABA = dict(x=(BOLSO["x"][0], 12.0), z_baixo=(BOLSO["z"][0], 7.6), z_alto=(46.9, BOLSO["z"][1]))
TRAVESSA = dict(x=(79.6, BOLSO["x"][1]))
FAIXA_DOBRADICA = dict(x=(12.0, BOLSO["x"][1]), z=(BOLSO["z"][0], PCB["z0"] - 0.10))

def T_fechar():
    """STEP (tampa a 135°) -> tampa fechada."""
    return trimesh.transformations.rotation_matrix(np.radians(135), [1, 0, 0], [0, G.PIVO[0], G.PIVO[1]])

def cyl_y(x, z, r, y0, y1, n=40):
    return mf.Manifold.cylinder(y1 - y0, r, r, n).rotate([-90, 0, 0]).translate([x, y0, z])

def cone_y(x, z, r0, r1, y0, y1, n=40):
    return mf.Manifold.cylinder(y1 - y0, r0, r1, n).rotate([-90, 0, 0]).translate([x, y0, z])

def extrude_y(geom, y0, y1):
    """Polígonos shapely no plano (X, Z) extrudados de y0 a y1."""
    res = None
    for p in (geom.geoms if hasattr(geom, "geoms") else [geom]):
        rings = [np.array(p.exterior.coords)[:-1]] + [np.array(i.coords)[:-1] for i in p.interiors]
        e = mf.Manifold.extrude(mf.CrossSection(rings, mf.FillRule.EvenOdd), y1 - y0)
        e = e.transform([[1, 0, 0, 0], [0, 0, 1, y0], [0, 1, 0, 0]])
        res = e if res is None else res + e
    return res

# ------------------------------------------------------------------ logo
def logo_2d():
    """'z3ck 031' lido de cima para baixo na faixa da D2R (topo das letras voltado para a tela, como o '512')."""
    from shapely import affinity
    from shapely.ops import unary_union
    c2 = os.path.join(ROOT, "versao_AREA512", "term512", "Model", "TypeA", "STL", "CaseC2.stl")
    L = LOGO.original_letters(c2)
    big = affinity.rotate(LOGO.big_word(L["E_top"], L["E_low"]), -90, origin=(0, 0))   # lê para baixo, topo -> +X
    small = LOGO.small_word(L["one"])                                                  # já lê para baixo
    u0, u1 = D2["x"][0] + 0.9, D2["x"][1] - 0.9           # faixa útil em X
    z0, z1 = FUROS[0][1] + 2.6, FUROS[1][1] - 2.6          # entre os rebaixos dos parafusos
    gap = 2.0
    bb, sb = big.bounds, small.bounds
    Lb, Ls, Hb = bb[3] - bb[1], sb[3] - sb[1], bb[2] - bb[0]
    s = min((u1 - u0) / Hb, (z1 - z0 - gap - Ls) / Lb)
    big = affinity.scale(big, s, s, origin=(0, 0))
    bb = big.bounds
    total = (bb[3] - bb[1]) + gap + Ls
    top = (z0 + z1) / 2 + total / 2
    big = affinity.translate(big, u0 - bb[0], top - bb[3])
    small = affinity.translate(small, u0 - sb[0], (top - (bb[3] - bb[1]) - gap) - sb[3])
    return unary_union([big, small]), s

# ------------------------------------------------------------------ peças: versão FECHADA
E, S = 0.02, 0.30   # o STL da D1 tem ruído de ~0,05 mm: cortes 0,02 além das paredes, adições 0,3 sobrepostas

def make_D1F(D1c):
    m = G.to_mf(D1c)
    bx, bz = BOLSO["x"], BOLSO["z"]
    # 1) rebaixo da tela: tudo à frente do verso da PCB sai (os apoios antigos recuam para Y_APOIO)
    m = m - G.box(bx[0] - E, bx[1] + E, 19.95, Y_APOIO, bz[0] - E, bz[1] + E)
    # 2) alívio raso sob a placa, sem tocar os apoios das pontas (X<8,3 / X>76,9) e da borda longa (Z>46,13)
    alivio = G.box(8.6, 76.6, Y_APOIO - 0.01, ALIVIO_GERAL, PCB["z0"], 45.9)
    for (x, z) in FUROS:                      # fundo inteiro em volta dos parafusos
        alivio = alivio - cyl_y(x, z, 3.0, 0, 40)
    m = m - alivio
    # 3) alívios fundos para os componentes altos
    for c in COMP:
        m = m - G.box(c["x"][0], c["x"][1], Y_APOIO - 0.01, min(ALIVIO_FUNDO, Y_APOIO + c["h"] + 0.35), c["z"][0], c["z"][1])
    # 4) alívio para as pontas dos pinos aparadas / soldas no verso (sem tocar os cantos da C2)
    m = m - G.box(PINOS["x"][0] - 0.3, PINOS["x"][1] + 0.3, Y_APOIO - 0.01, ALIVIO_GERAL, PINOS["z"][0], PINOS["z"][1])
    # 5) ressaltos atrás dos furos da placa (fecham o vão até o fundo, abaixo das abas da C2) + furo-guia M2
    for (x, z) in FUROS:
        m = m + cyl_y(x, z, 3.0, Y_APOIO, Y_FUNDO_INT - 0.005)
    for (x, z) in FUROS:
        m = m - cyl_y(x, z, PILOTO[0], Y_APOIO - 1, Y_PONTA + 0.05)               # cego: o fundo não é furado
    # 6) passagem dos fios do canto livre para a fenda da dobradiça esquerda
    m = m - G.box(PASSAGEM["x"][0], PASSAGEM["x"][1], PASSAGEM["y"][0], PASSAGEM["y"][1], PASSAGEM["z"][0], PASSAGEM["z"][1])
    parts = m.decompose()
    m = max(parts, key=lambda p: p.volume())
    return G.ok(m, "D1R fechada"), len(parts)

def make_C3F(C3c):
    m = G.to_mf(C3c)
    m = m - G.box(8.6, 76.6, 20, ALIVIO_GERAL, PCB["z0"], 45.9)
    for c in COMP:
        m = m - G.box(c["x"][0], c["x"][1], 20, Y_APOIO + c["h"] + 0.35 + 0.2, c["z"][0], c["z"][1])
    return G.ok(m, "C3R"), None

def make_D2R(fechada=True):
    x0, x1 = D2["x"]; z0, z1 = D2["z"]
    y_pcb = Y_APOIO - ESP_PCB                                 # face da frente da PCB (placa puxada contra os apoios)
    m = G.box(x0, x1, Y_FRENTE, y_pcb, PCB["z0"], z1)
    if fechada:
        # ponta além da PCB só fora do canto dos fios (o canto X<5,6 leva os fios até a fenda da dobradiça)
        m = m + G.box(FENDA_ESQ["x"][1], x1, Y_FRENTE, y_pcb, z0, PCB["z0"] + 0.01)
        # canal dos fios soldados na frente dos furos dos pinos, aberto até a ponta da dobradiça
        m = m - G.box(x0 - 1, PINOS["x"][1], y_pcb - 1.8, y_pcb + 1, z0 - 1, PINOS["z"][1])
    else:
        m = m + G.box(x0, x1, Y_FRENTE, Y_APOIO - 0.10, z0, PCB["z0"] + 0.01)
        m = m - G.box(x0 - 1, PINOS["x"][1], y_pcb - 1.8, y_pcb + 1, PINOS["z"][0], PINOS["z"][1])
    m = m - G.box(x1 - 1.0, x1 + 1, Y_FRENTE + 1.5, y_pcb + 1, FUROS[0][1] + 2.6, FUROS[1][1] - 2.6)   # cabo flexível do LCD
    for (x, z) in FUROS:
        if fechada:   # passagem + rebaixo da cabeça pela frente
            m = m - cyl_y(x, z, 1.2, Y_FRENTE - 1, y_pcb + 1) - cyl_y(x, z, REBAIXO[0], Y_FRENTE - 1, REBAIXO[1])
        else:
            m = m - cyl_y(x, z, PILOTO[0], y_pcb - PILOTO[1], y_pcb + 1)
    txt, escala = logo_2d()
    m = m - G.ok(extrude_y(txt, Y_FRENTE - 0.2, Y_FRENTE + GRAVACAO), "logo")
    return G.ok(m, "D2R"), txt, escala

# ------------------------------------------------------------------ peças: variante ABERTA
def make_D1A(D1c):
    m = G.to_mf(D1c)
    m = m - G.box(BOLSO["x"][0] - E, BOLSO["x"][1] + E, 19.95, 40, BOLSO["z"][0] - E, BOLSO["z"][1] + E)
    add = G.box(FAIXA_DOBRADICA["x"][0], FAIXA_DOBRADICA["x"][1] + S, Y_FRENTE, Y_FUNDO, FAIXA_DOBRADICA["z"][0] - S, FAIXA_DOBRADICA["z"][1])
    add = add + G.box(ABA["x"][0] - S, ABA["x"][1], Y_APOIO, Y_FUNDO, ABA["z_baixo"][0] - S, ABA["z_baixo"][1])
    add = add + G.box(ABA["x"][0] - S, ABA["x"][1], Y_APOIO, Y_FUNDO, ABA["z_alto"][0], ABA["z_alto"][1] + S)
    add = add + G.box(TRAVESSA["x"][0], TRAVESSA["x"][1] + S, Y_APOIO, Y_FUNDO, BOLSO["z"][0] - S, BOLSO["z"][1] + S)
    m = m + add
    for (x, z) in FUROS:
        m = m - cyl_y(x, z, 1.2, Y_APOIO - 1, Y_FUNDO + 1)
    parts = m.decompose()
    m = max(parts, key=lambda p: p.volume())
    return G.ok(m, "D1R aberta"), len(parts)

# ------------------------------------------------------------------ envelope da tela (para testes e renders)
def tela_envelope(margem=MARGEM_PAINEL[1], com_sd=False):
    """Sólidos da TFT na posição final (placa puxada contra os apoios): painel, PCB, componentes do verso (e slot SD)."""
    y_pcb = Y_APOIO - ESP_PCB
    px0 = PCB["x0"] + margem
    pz0 = PCB["z0"] + (PCB["W"] - 50.0) / 2
    out = {"painel": G.box(px0, px0 + 69.2, y_pcb - ESP_PAINEL, y_pcb, pz0, pz0 + 50.0),
           "pcb": G.box(PCB["x0"], PCB["x0"] + PCB["L"], y_pcb, Y_APOIO, PCB["z0"], PCB["z0"] + PCB["W"])}
    # componentes reais estimados (menores que os alívios, que têm ~2 mm de margem em X/Z)
    out["comp"] = (G.box(17.0, 32.0, Y_APOIO, Y_APOIO + 1.20, 9.5, 18.5)        # U2 TSSOP-16 (máx. 1,20) atrás da C3
                   + G.box(17.0, 32.0, Y_APOIO, Y_APOIO + 1.12, 18.5, 35.5)      # Q1/U1 SOT-23 (máx. 1,12)
                   + G.box(44.5, 54.5, Y_APOIO, Y_APOIO + 0.55, 21.5, 26.5)
                   + G.box(PINOS["x"][0], PINOS["x"][1], Y_APOIO, Y_APOIO + 0.45, PINOS["z"][0] + 0.7, PINOS["z"][1] - 0.7))
    if com_sd:
        out["sd"] = G.box(SD["x"][0], SD["x"][1], Y_APOIO, Y_APOIO + SD["h"], SD["z"][0], SD["z"][1])
    aa_x0 = px0 + AA_DO_PAINEL
    aa = (aa_x0, aa_x0 + AA[0], pz0 + (50 - AA[1]) / 2, pz0 + (50 + AA[1]) / 2)
    return out, aa, y_pcb - ESP_PAINEL

def caminho_fio():
    """Linha central de um fio de Ø0,65: frente do pino 7 -> canal da D2R -> canto livre -> fenda da dobradiça."""
    yc = Y_APOIO - ESP_PCB - 0.9
    return [(3.5, yc, 27.0), (3.5, yc, -1.5), (2.8, 21.2, -2.5), (2.8, 21.2, -6.0), (-0.5, 21.2, -6.5)]

# ------------------------------------------------------------------ principal
def main(aberta=False):
    Tc = T_fechar(); Ts = np.linalg.inv(Tc)
    D1c = trimesh.load(os.path.join(ASM_A, "CaseD1.stl")); D1c.apply_transform(Tc)
    os.makedirs(ASM_R, exist_ok=True); os.makedirs(STL_R, exist_ok=True)
    for d in (ASM_R, STL_R):                       # limpa só as peças da tampa (as da base são recopiadas)
        for f in os.listdir(d):
            if f.startswith(("CaseD1R", "CaseD2R", "CaseC2", "CaseC3", "TFT_")):
                os.remove(os.path.join(d, f))
    if aberta:
        D1R, nD1 = make_D1A(D1c); pecas = {"CaseD1R": D1R}
    else:
        D1R, nD1 = make_D1F(D1c)
        C3c = trimesh.load(os.path.join(ASM_A, "CaseC3.stl")); C3c.apply_transform(Tc)
        C3R, _ = make_C3F(C3c)
        pecas = {"CaseD1R": D1R, "CaseC3R": C3R}
    D2R, txt, escala = make_D2R(fechada=not aberta)
    pecas["CaseD2R_z3ck031"] = D2R
    tela, aa, y_vidro = tela_envelope(com_sd=aberta)
    rel = {"variante": "aberta" if aberta else "fechada", "escala_logo": escala, "d1r_pedacos_descartados": nD1 - 1,
           "aa_fechado": aa, "y_vidro": y_vidro,
           "parametros": {k: v for k, v in globals().items() if k.isupper() and isinstance(v, (int, float, tuple, list, dict))}}

    # montagem (referencial do STEP, tampa a 135°) - mesmo referencial dos outros tipos
    asm = {k: G.to_tm(M) for k, M in pecas.items()}
    for k, M in tela.items():
        asm[f"TFT_{k}"] = G.to_tm(M)
    for k, m in asm.items():
        mm = m.copy(); mm.apply_transform(Ts)
        (G.export_ok if k.startswith("Case") else lambda a, p: a.export(p))(mm, os.path.join(ASM_R, k + ".stl"))
    base = ["CaseS1L", "CaseL1L", "CaseR1L", "CaseB1_z3ck", "CaseT1", "CaseT1_2", "CaseK1", "CaseK2"]
    if not aberta:
        base.append("CaseC2_z3ck031")
    for k in base:
        shutil.copy2(os.path.join(ASM_A, k + ".stl"), os.path.join(ASM_R, k + ".stl"))

    # impressão
    nome_d1 = "CaseD1R_aberta.stl" if aberta else "CaseD1R.stl"
    d1p = asm["CaseD1R"].copy(); d1p.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0]))
    d1p.apply_translation(-d1p.bounds[0]); G.export_ok(d1p, os.path.join(STL_R, nome_d1))                # fundo na mesa
    d2p = asm["CaseD2R_z3ck031"].copy(); d2p.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
    d2p.apply_translation(-d2p.bounds[0]); G.export_ok(d2p, os.path.join(STL_R, "CaseD2R_z3ck031.stl"))  # face com logo na mesa
    if not aberta:
        c3p = asm["CaseC3R"].copy(); c3p.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0]))
        c3p.apply_translation(-c3p.bounds[0]); G.export_ok(c3p, os.path.join(STL_R, "CaseC3R.stl"))      # face externa na mesa
        shutil.copy2(os.path.join(STL_A, "CaseC2_z3ck031.stl"), os.path.join(STL_R, "CaseC2_z3ck031.stl"))
    for k in ["CaseS1L_capa_cap", "CaseL1L", "CaseR1L", "CaseB1_z3ck", "CaseT1", "CaseK1", "CaseK2"]:
        shutil.copy2(os.path.join(STL_A, k + ".stl"), os.path.join(STL_R, k + ".stl"))
    json.dump(rel, open(os.path.join(OUT, "relatorio_tampa_vermelha.json"), "w", encoding="utf-8"), indent=1, default=str, ensure_ascii=False)
    print("ok ->", STL_R, "| variante", rel["variante"], "| escala do logo %.3f" % escala)

if __name__ == "__main__":
    main(aberta="--aberta" in sys.argv)
