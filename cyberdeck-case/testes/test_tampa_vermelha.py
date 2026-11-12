"""Testes do kit TypeR: tampa FECHADA (com a C2 "z3ck 031") para a TFT 2,8" vermelha cortada (81 x 51 mm).

Rodar depois de ferramentas/gerar_tampa_vermelha.py:
    python -I -X utf8 -m pytest testes/test_tampa_vermelha.py -v
"""
import os, sys, glob, hashlib
import numpy as np
import pytest
import trimesh
import manifold3d as mf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "ferramentas"))
import gerar_z3ck031 as GEN          # noqa: E402
import gerar_tampa_vermelha as R     # noqa: E402

TC = R.T_fechar()                                      # STEP (135°) -> fechado
TAMPA_R = ["CaseD1R", "CaseD2R_z3ck031", "CaseC2_z3ck031", "CaseC3R"]

def mfm(m):
    return GEN.to_mf(m)

def fechado(m):
    m = m.copy(); m.apply_transform(TC); return m

def lid_T(angle):
    return trimesh.transformations.rotation_matrix(np.radians(135.0 - angle), [1, 0, 0], [0, GEN.PIVO[0], GEN.PIVO[1]])

def mat(T):
    return [[float(T[r][c]) for c in range(4)] for r in range(3)]

@pytest.fixture(scope="module")
def a():
    d = os.path.join(GEN.OUT, "montagem", "TypeR")
    return {os.path.splitext(os.path.basename(f))[0]: trimesh.load(f) for f in glob.glob(os.path.join(d, "*.stl"))}

@pytest.fixture(scope="module")
def orig():
    d = os.path.join(GEN.OUT, "montagem", "TypeA")
    return {k: trimesh.load(os.path.join(d, k + ".stl")) for k in ["CaseD1", "CaseD2", "CaseD3", "CaseC2_z3ck031", "CaseC3", "Frame__1_", "SOLID", "Glass", "PCB"]}

@pytest.fixture(scope="module")
def hw():
    return {"cap": GEN.cap_envelope(), "cp_static": trimesh.load(os.path.join(GEN.CACHE, "ko_cp_static.stl"))}

def tela(margem=R.MARGEM_PAINEL[1]):
    t, aa, _ = R.tela_envelope(margem)
    return t, aa

# ================================================================== integridade
def test_stls_typeR_validos():
    d = os.path.join(GEN.OUT, "STL_TypeR")
    files = sorted(glob.glob(os.path.join(d, "*.stl")))
    nomes = {os.path.basename(f) for f in files}
    assert nomes == {"CaseD1R.stl", "CaseD2R_z3ck031.stl", "CaseC2_z3ck031.stl", "CaseC3R.stl", "CaseS1L_capa_cap.stl",
                     "CaseL1L.stl", "CaseR1L.stl", "CaseB1_z3ck.stl", "CaseT1.stl", "CaseK1.stl", "CaseK2.stl"}, nomes
    for f in files:
        m = trimesh.load(f)
        assert m.is_watertight and m.volume > 50, os.path.basename(f)
        assert len(m.split(only_watertight=False)) == 1, os.path.basename(f)
        assert mfm(m).status() == mf.Error.NoError

def test_c2_logo_e_base_identicas_ao_typeA():
    """Placa traseira C2 "z3ck 031", capa, L1L, R1L, B1, T1, K1, K2: exatamente as do TypeA."""
    for k in ["CaseC2_z3ck031", "CaseS1L_capa_cap", "CaseL1L", "CaseR1L", "CaseB1_z3ck", "CaseT1", "CaseK1", "CaseK2"]:
        x = open(os.path.join(GEN.OUT, "STL_TypeA", k + ".stl"), "rb").read()
        y = open(os.path.join(GEN.OUT, "STL_TypeR", k + ".stl"), "rb").read()
        assert hashlib.sha256(x).digest() == hashlib.sha256(y).digest(), k

def test_orientacao_de_impressao():
    """D1R e C3R com a face externa (fundo) na mesa; D2R com a face do logo na mesa."""
    for f, area_min in [("CaseD1R.stl", 2500), ("CaseD2R_z3ck031.stl", 400), ("CaseC3R.stl", 600)]:
        m = trimesh.load(os.path.join(GEN.OUT, "STL_TypeR", f))
        assert abs(m.bounds[0][2]) < 1e-3
        base = m.area_faces[(m.face_normals[:, 2] < -0.999) & (m.triangles_center[:, 2] < 0.01)].sum()
        assert base > area_min, f"{f}: só {base:.0f} mm² apoiados na mesa"

# ================================================================== mesma tampa por fora
def test_d1r_igual_a_d1_fora_do_bolso(a, orig):
    """Dobradiça, batente, engates das travas, bordas e face externa do fundo: idênticos à D1 (exceto a passagem dos fios)."""
    keep = mfm(trimesh.creation.box(bounds=[[-5, 0, -20], [90, 40, 70]])) - GEN.box(R.BOLSO["x"][0] - 0.4, R.BOLSO["x"][1] + 0.4, 19.9, R.Y_FUNDO_INT + 0.75, R.BOLSO["z"][0] - 0.4, R.BOLSO["z"][1] + 0.4)
    P = R.PASSAGEM                                            # passagem dos fios: única mudança fora do bolso
    keep = keep - GEN.box(P["x"][0] - 0.1, P["x"][1] + 0.1, P["y"][0] - 0.1, P["y"][1] + 0.1, P["z"][0] - 0.1, P["z"][1] + 0.1)
    for (x, z) in R.FUROS:                                    # furos-guia cegos (testados à parte)
        keep = keep - R.cyl_y(x, z, 1.0, 0, 40)
    d1 = mfm(fechado(orig["CaseD1"])) ^ keep
    d1r = mfm(fechado(a["CaseD1R"])) ^ keep
    dif = (d1 - d1r).volume() + (d1r - d1).volume()
    assert dif < 0.5, f"D1R difere da D1 fora do bolso em {dif:.2f} mm³"

def test_mesma_espessura_do_typeA(a, orig):
    """A tampa fechada TypeR tem exatamente o mesmo envelope externo da tampa do TypeA (nada sai para trás)."""
    def env(ms):
        b = np.array([fechado(m).bounds for m in ms]); return b[:, 0].min(0), b[:, 1].max(0)
    lo0, hi0 = env([orig[k] for k in ["CaseD1", "CaseC2_z3ck031", "CaseC3", "CaseD2", "CaseD3"]])
    lo1, hi1 = env([a[k] for k in TAMPA_R + ["TFT_painel", "TFT_pcb", "TFT_comp"]])
    assert np.all(lo1 >= lo0 - 0.15) and np.all(hi1 <= hi0 + 0.02), (lo0, hi0, lo1, hi1)

def test_c2_no_mesmo_lugar_e_sem_conflito(a, orig):
    c2a, c2r = mfm(orig["CaseC2_z3ck031"]), mfm(a["CaseC2_z3ck031"])
    assert (c2a - c2r).volume() + (c2r - c2a).volume() < 0.01
    for k in ["CaseD1R", "CaseC3R", "CaseD2R_z3ck031"]:
        v = (c2r ^ mfm(a[k])).volume()
        ref = (c2a ^ mfm(orig["CaseD1"])).volume() if k == "CaseD1R" else (c2a ^ mfm(orig["CaseC3"])).volume() if k == "CaseC3R" else 0.0
        assert v <= ref + 0.05, f"C2 x {k}: {v:.3f} mm³ (original {ref:.3f})"

# ================================================================== a tela cabe e fica presa
@pytest.mark.parametrize("margem", R.MARGEM_PAINEL)
def test_tela_cabe_sem_interferencia(a, margem):
    t, _ = tela(margem)
    for k in TAMPA_R:
        p = mfm(fechado(a[k]))
        for nome, M in t.items():
            v = (p ^ M).volume()
            assert v < 0.01, f"{k} x {nome} (margem {margem}): {v:.3f} mm³"

def test_folga_dos_componentes_do_verso(a):
    """Componentes do verso (alturas máximas de catálogo) têm >= 0,15 mm de folga para D1R/C3R."""
    t, _ = tela()
    for k in ["CaseD1R", "CaseC3R"]:
        g = mfm(fechado(a[k])).min_gap(t["comp"], 1.0)
        assert g >= 0.15, f"{k}: folga {g:.3f} mm"

def test_pcb_apoiada_nas_pontas_e_na_borda(a):
    """Verso da PCB encosta nos apoios da D1R: lado dos pinos, lado cortado e borda longa."""
    t, _ = tela()
    d1r = mfm(fechado(a["CaseD1R"]))
    pcb_mais = t["pcb"].translate([0, 0.15, 0])
    for (x0, x1, z0, z1) in [(5.0, 12.0, 0, 52), (77.0, 82.7, 0, 52), (10, 76, 46.2, 52.1)]:
        zona = GEN.box(x0, x1, 0, 40, z0, z1)
        assert (d1r ^ pcb_mais ^ zona).volume() > 0.5, f"sem apoio em X {x0}..{x1} Z {z0}..{z1}"

def test_d2r_encosta_na_frente_da_pcb(a):
    t, _ = tela()
    assert (mfm(fechado(a["CaseD2R_z3ck031"])) ^ t["pcb"].translate([0, -0.05, 0])).volume() > 5.0

@pytest.mark.parametrize("ddx,ddist", [(0, 0), (-0.4, 0), (0.4, 0), (0, -0.6), (0, 0.6)])
def test_parafusos_passam_nos_furos_da_placa(ddx, ddist):
    """M2 (Ø2) passa no furo Ø3,2 da placa na medida estimada e com erro de ±0,4 mm (posição) / ±0,6 mm (distância)."""
    dx, dist = R.FUROS_PCB[0]
    cx = R.PCB["x0"] + dx + ddx
    zs = [R.Z_MEIO - (dist + ddist) / 2, R.Z_MEIO + (dist + ddist) / 2]
    for (x, z), zc in zip(R.FUROS, zs):
        assert np.hypot(x - cx, z - zc) <= 1.6 - 1.0 - 0.05, (x, z, cx, zc)

def test_parafusos_pela_frente(a):
    """M2x4 cabeça cilíndrica pela frente: cabeça rebaixada na D2R, passa livre pela D2R e pela placa,
    rosca >= 1,1 mm na D1R e a ponta não atravessa o fundo (não encosta na C2)."""
    d1r = mfm(fechado(a["CaseD1R"])); d2 = mfm(fechado(a["CaseD2R_z3ck031"])); c2 = mfm(fechado(a["CaseC2_z3ck031"]))
    for (x, z) in R.FUROS:
        cabeca = R.cyl_y(x, z, 1.9, R.Y_FRENTE - 0.5, R.REBAIXO[1] - 0.02)          # cabeça Ø3,8 x 2,0
        haste_livre = R.cyl_y(x, z, 1.0, R.REBAIXO[1], R.Y_APOIO)
        for p in (d2, d1r):
            assert ((cabeca + haste_livre) ^ p).volume() < 0.01, "cabeça/haste obstruída"
        assert R.REBAIXO[1] - R.Y_FRENTE >= 2.0, "cabeça saltaria da face"
        rosca = R.cyl_y(x, z, 1.0, R.Y_APOIO, R.Y_PONTA) - R.cyl_y(x, z, R.PILOTO[0] + 0.01, 0, 40)
        assert (rosca ^ d1r).volume() > 0.95 * rosca.volume() and R.Y_PONTA - R.Y_APOIO >= 1.1, "pouca rosca na D1R"
        parede = R.cyl_y(x, z, R.PILOTO[0] + 0.7, R.Y_APOIO, R.Y_PONTA) - R.cyl_y(x, z, R.PILOTO[0] + 0.01, 0, 40)   # >= 0,7 mm (aba da C2 ao lado)
        assert (parede ^ d1r).volume() > 0.95 * parede.volume(), "parede < 0,7 mm em volta da rosca"
        assert (R.cyl_y(x, z, 1.0, R.Y_PONTA, R.Y_PONTA + 0.01) ^ c2).volume() < 1e-6 and R.Y_PONTA < 27.286 - 0.1, "ponta encosta na C2"
        assert (R.cyl_y(x, z, R.PILOTO[0], R.Y_PONTA + 0.1, 27.25) ^ d1r).volume() > 0.95 * np.pi * R.PILOTO[0] ** 2 * (27.15 - R.Y_PONTA), "furo-guia atravessa o fundo"

def test_d2r_nao_cobre_a_imagem(a):
    for margem in R.MARGEM_PAINEL:
        _, aa = tela(margem)
        assert fechado(a["CaseD2R_z3ck031"]).bounds[1][0] < aa[0] - 5.0

def test_vidro_no_mesmo_plano_do_original(orig):
    _, _, y_vidro = R.tela_envelope()
    assert abs(y_vidro - fechado(orig["Frame__1_"]).bounds[0][1]) < 0.05

# ================================================================== fios
def test_caminho_dos_fios_livre(a):
    """Fio Ø0,65 da frente dos pinos -> canal da D2R -> canto livre -> fenda da dobradiça esquerda, sem obstrução."""
    pts = np.array(R.caminho_fio())
    obst = [mfm(fechado(a[k])) for k in TAMPA_R] + [M for M in tela()[0].values()]
    for p, q in zip(pts[:-1], pts[1:]):
        L = np.linalg.norm(q - p); d = (q - p) / L
        seg = mf.Manifold.cylinder(L, 0.33, 0.33, 16)
        rot = trimesh.geometry.align_vectors([0, 0, 1], d)
        seg = seg.transform(mat(trimesh.transformations.translation_matrix(p) @ rot))
        for o in obst:
            assert (seg ^ o).volume() < 0.005, f"fio bloqueado entre {p} e {q}"
    assert pts[-1][0] < 0, "o fio precisa sair pela face externa esquerda"

# ================================================================== dobradiça / varredura
def _lid(a):
    t, _ = tela()
    TS = np.linalg.inv(TC)
    return [mfm(a[k]) for k in TAMPA_R] + [M.transform(mat(TS)) for M in t.values()]

def test_varredura_0_a_135_sem_colisao(a, hw):
    lid = _lid(a)
    obst = {"Cap": mfm(hw["cap"]), "CaseS1L": mfm(a["CaseS1L"]), "CaseB1_z3ck": mfm(a["CaseB1_z3ck"]),
            "CaseL1L": mfm(a["CaseL1L"]), "CaseR1L": mfm(a["CaseR1L"]), "Cardputer": mfm(hw["cp_static"])}
    pior = {}
    for ang in list(range(0, 136, 3)) + [135]:
        L = [m.transform(mat(lid_T(ang))) for m in lid]
        for k, O in obst.items():
            if k == "Cardputer" and ang <= 0:
                continue
            pior[k] = max(pior.get(k, 0), sum((l ^ O).volume() for l in L))
    for k, v in pior.items():
        assert v < (25.0 if k == "Cardputer" else 0.5), f"tampa colide com {k}: {v:.2f} mm³"

def test_tampa_fechada_nao_piora_contato_com_cardputer(a, orig, hw):
    cp = mfm(hw["cp_static"])
    L0 = mat(lid_T(0))
    v_new = sum((m.transform(L0) ^ cp).volume() for m in _lid(a))
    v_old = 0.0
    for k in ["CaseD1", "CaseD2", "CaseD3", "CaseC2_z3ck031", "CaseC3", "Frame__1_", "SOLID", "Glass", "PCB"]:
        try:
            v_old += (mfm(orig[k]).transform(L0) ^ cp).volume()
        except ValueError:
            pass
    assert v_new <= v_old + 0.5, (v_new, v_old)

def test_batente_135_preservado(a):
    stop = mfm(a["CaseL1L"]) + mfm(a["CaseR1L"])
    assert (mfm(a["CaseD1R"]).transform(mat(lid_T(140))) ^ stop).volume() > 5.0
    assert (mfm(a["CaseD1R"]).transform(mat(lid_T(135))) ^ stop).volume() < 0.5

# ================================================================== logo da frente (D2R)
def test_logo_gravado_legivel(a):
    """'z3ck 031' gravado 0,6 mm na D2R: >= 6 partes, traços >= 0,84 mm (bico 0,4), dentro da face."""
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    d2 = fechado(a["CaseD2R_z3ck031"])
    s = d2.section(plane_origin=[0, R.Y_FRENTE + 0.3, 0], plane_normal=[0, 1, 0])
    polys = sorted([Polygon(e[:, [0, 2]]) for e in s.discrete], key=lambda p: -p.area)
    perto = lambda p: min(np.hypot(p.centroid.x - x, p.centroid.y - z) for (x, z) in R.FUROS) < 0.5
    txt = unary_union([p for p in polys[1:] if polys[0].contains(p.representative_point()) and not perto(p)])
    assert len(getattr(txt, "geoms", [txt])) >= 6 and txt.area > 40
    aberto = txt.buffer(-0.42).buffer(0.42)
    assert aberto.area > 0.85 * txt.area, f"traços finos demais ({aberto.area / txt.area:.0%})"
    b = txt.bounds
    assert b[0] > R.D2["x"][0] + 0.5 and b[2] < R.D2["x"][1] - 0.5
