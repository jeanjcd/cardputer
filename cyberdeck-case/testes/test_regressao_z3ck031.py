"""Testes de regressão do kit z3ck 031 (Cardputer-Adv + Cap LoRa-1262 + TFT 2.8" ILI9341).

Rodar (depois de ferramentas/gerar_z3ck031.py):
    python -I -X utf8 -m pytest testes -v

Cada teste compara as peças geradas com: (a) os modelos oficiais M5Stack (Cardputer-Adv e Cap LoRa-1262),
(b) as peças originais do TERM512 que são reaproveitadas, e (c) as regras do projeto original.
Vários testes também rodam sobre o TERM512 ORIGINAL para provar que detectam o problema (controle negativo).
"""
import os, sys, glob, hashlib
import numpy as np
import pytest
import trimesh
import manifold3d as mf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "ferramentas"))
import gerar_z3ck031 as GEN                      # noqa: E402

OUT = GEN.OUT
ORIG = os.path.join(ROOT, "versao_AREA512", "term512", "Model")
TIPOS = ["A", "B"]
NOVAS = ["CaseS1L", "CaseL1L", "CaseR1L", "CaseB1_z3ck"]
FOLGA_MIN = 0.20                                  # folga mínima aceitável peça x hardware (mm)


# ------------------------------------------------------------------ fixtures
def mfm(m):
    return GEN.to_mf(m)

def ko(hw, name):
    """Zona de exclusão recortada em X∈[-0,25; 84,25] (faces laterais reais do Cap/Cardputer são planos em X=0/84)."""
    return mfm(hw[name]) ^ GEN.box(-GEN.FOLGA + 0.05, 84.0 + GEN.FOLGA - 0.05, -60, 60, -60, 60)

@pytest.fixture(scope="session")
def term():
    return {t: GEN.load_term(t) for t in TIPOS}

@pytest.fixture(scope="session")
def hw():
    """Hardware real: Cap (envelope sólido) e Cardputer (sólido do TERM512 ∪ casca oficial M5)."""
    cap_env = GEN.cap_envelope()
    ko = {n: trimesh.load(os.path.join(GEN.CACHE, f"ko_{n}.stl")) for n in ["cap_static", "cp_static", "cap_sweepY", "cp_sweepY", "k_sweepY"]}
    return {"cap": cap_env, "cap_raw": GEN.load_m5("Cap_LoRa-1262.stl"), "cp_raw": GEN.load_m5("Cardputer-Adv.stl"), **ko}

@pytest.fixture(scope="session")
def asm():
    out = {}
    for t in TIPOS:
        d = os.path.join(OUT, "montagem", f"Type{t}")
        out[t] = {os.path.splitext(os.path.basename(f))[0]: trimesh.load(f) for f in glob.glob(os.path.join(d, "*.stl"))}
    return out

def lid_parts(a):
    """Peças da tampa convertíveis em sólido; D1, D2, D3, C2 e C3 são obrigatórias (PCB/vidro do STEP B não são manifold)."""
    out = []
    for k in ["CaseD1", "CaseD2", "CaseD3", "CaseC2_z3ck031", "CaseC3", "PCB", "Frame__1_", "SOLID", "Glass"]:
        if k not in a: continue
        try:
            out.append(a[k]); GEN.to_mf(a[k])
        except ValueError:
            out.pop()
            assert not k.startswith("Case"), f"peça estrutural {k} não é sólida"
    return out

def GEN_lid_T(angle):
    d = np.radians(135.0 - angle)
    return trimesh.transformations.rotation_matrix(d, [1, 0, 0], [0, GEN.PIVO[0], GEN.PIVO[1]])


# ================================================================== 1. integridade das malhas
@pytest.mark.parametrize("tipo", TIPOS)
def test_stls_de_impressao_validos(tipo):
    """Todo STL de impressão é fechado (watertight), manifold, volume positivo e corpo único."""
    d = os.path.join(OUT, f"STL_Type{tipo}")
    files = sorted(glob.glob(os.path.join(d, "*.stl")))
    assert len(files) == 12, f"esperado 12 STL (5 novos/modificados + 7 reaproveitados), achei {len(files)}"
    for f in files:
        m = trimesh.load(f)
        assert m.is_watertight, f"{os.path.basename(f)} não é fechado"
        assert m.volume > 50, f"{os.path.basename(f)} volume suspeito {m.volume}"
        assert mfm(m).status() == mf.Error.NoError
        assert len(m.split(only_watertight=False)) == 1, f"{os.path.basename(f)} tem mais de um corpo"

@pytest.mark.parametrize("tipo", TIPOS)
def test_pecas_reaproveitadas_identicas(tipo):
    """D1, D2, D3, C3, T1, K1, K2 são cópias byte-a-byte dos originais (nenhuma regressão acidental)."""
    for k in ["CaseD1", "CaseD2", "CaseD3", "CaseC3", "CaseT1", "CaseK1", "CaseK2"]:
        a = open(os.path.join(ORIG, f"Type{tipo}", "STL", k + ".stl"), "rb").read()
        b = open(os.path.join(OUT, f"STL_Type{tipo}", k + ".stl"), "rb").read()
        assert hashlib.sha256(a).digest() == hashlib.sha256(b).digest(), k

def test_pecas_eliminadas_ausentes():
    """S1, C1 e o header 2x7 saem do kit (o Cap LoRa ocupa o conector EXT)."""
    for t in TIPOS:
        nomes = {os.path.basename(f) for f in glob.glob(os.path.join(OUT, f"STL_Type{t}", "*.stl"))}
        assert "CaseS1.stl" not in nomes and "CaseC1.stl" not in nomes


# ================================================================== 2. controle negativo: o original NÃO cabe
def test_original_conflita_com_cap(term, hw):
    """Prova que o teste de interferência funciona: S1/C1/L1/R1/B1 originais colidem com o Cap."""
    cap = mfm(hw["cap"])
    for k, minimo in [("CaseS1", 1000), ("CaseC1", 300), ("CaseL1", 100), ("CaseR1", 100), ("CaseB1", 100)]:
        v = (mfm(term["A"][k]) ^ cap).volume()
        assert v > minimo, f"{k}: esperado conflito > {minimo} mm³, obtido {v:.1f}"


# ================================================================== 3. encaixe estático das peças novas
@pytest.mark.parametrize("tipo", TIPOS)
def test_sem_interferencia_com_hardware(asm, hw, term, tipo):
    """Peças novas não invadem o Cap (com folga). Contra o Cardputer, nenhuma piora em relação à peça original
    correspondente (L1/R1 apoiam no topo do Cardputer e a B1 tem pinos LEGO, por projeto)."""
    a = asm[tipo]
    cap_ko, cp_ko = ko(hw, "cap_static"), ko(hw, "cp_static")
    origem = {"CaseS1L": None, "CaseL1L": "CaseL1", "CaseR1L": "CaseR1", "CaseB1_z3ck": "CaseB1"}
    for k in NOVAS:
        p = mfm(a[k])
        assert (p ^ cap_ko).volume() < 0.5, f"{k} invade Cap+folga"
        ref = 0.0 if origem[k] is None else (mfm(term["A"][origem[k]]) ^ cp_ko).volume()
        assert (p ^ cp_ko).volume() <= ref + 0.5, f"{k} invade mais o Cardputer que o original"

def test_b1_nao_piora_contato_com_cardputer(term, asm, hw):
    """A B1 modificada não aumenta a sobreposição com o Cardputer (só os pinos LEGO, como no original)."""
    ko = mfm(hw["cp_static"])
    v_orig = (mfm(term["A"]["CaseB1"]) ^ ko).volume()
    v_new = (mfm(asm["A"]["CaseB1_z3ck"]) ^ ko).volume()
    assert v_new <= v_orig + 0.5, (v_orig, v_new)

@pytest.mark.parametrize("tipo", TIPOS)
def test_folga_minima_cap(asm, hw, tipo):
    """Distância real (min_gap) entre capa/L1L/R1L e o Cap >= 0,2 mm."""
    cap = mfm(hw["cap"])
    for k in ["CaseS1L", "CaseL1L", "CaseR1L"]:
        g = mfm(asm[tipo][k]).min_gap(cap, 2.0)
        assert g >= FOLGA_MIN - 0.08, f"{k}: folga {g:.3f} mm"   # tolerância do voxel de 0,15 mm

@pytest.mark.parametrize("tipo", TIPOS)
def test_pecas_novas_nao_se_cruzam(asm, tipo):
    a = asm[tipo]
    ks = NOVAS + ["CaseK1", "CaseK2", "CaseT1", "CaseT1_2"]
    for i, x in enumerate(ks):
        for y in ks[i + 1:]:
            if {x, y} <= {"CaseB1_z3ck", "CaseT1", "CaseT1_2", "CaseK1", "CaseK2"}:
                continue                                 # pares do original (encaixes por pressão)
            v = (mfm(a[x]) ^ mfm(a[y])).volume()
            assert v < 0.5, f"{x} x {y}: {v:.2f} mm³"


# ================================================================== 4. tampa / dobradiça
@pytest.mark.parametrize("tipo", TIPOS)
def test_eixo_da_dobradica_preservado(asm, term, tipo):
    """O furo do pivô em L1L/R1L continua exatamente onde a D1 espera (Y=16,88 Z=-5,07, r=1,65)."""
    for k, x in [("CaseL1L", 4.0), ("CaseR1L", 83.0)]:
        s = asm[tipo][k].section(plane_origin=[x, 0, 0], plane_normal=[1, 0, 0])
        found = False
        for e in s.discrete:
            c = e.mean(0); r = np.linalg.norm(e - c, axis=1)
            if len(e) > 10 and r.std() < 0.05 * r.mean() and abs(r.mean() - 1.65) < 0.05:
                assert abs(c[1] - GEN.PIVO[0]) < 0.02 and abs(c[2] - GEN.PIVO[1]) < 0.02
                found = True
        assert found, f"furo do pivô não encontrado em {k}"

@pytest.mark.parametrize("tipo", TIPOS)
def test_varredura_da_tampa_0_a_135(asm, hw, tipo):
    """A tampa (com a C2 nova) abre de 0° a 135° sem tocar Cap, capa, L1L/R1L (exceto batente), B1 e Cardputer."""
    a = asm[tipo]
    lid = [mfm(m) for m in lid_parts(a)]
    obst = {"Cap": mfm(hw["cap"]), "CaseS1L": mfm(a["CaseS1L"]), "CaseB1_z3ck": mfm(a["CaseB1_z3ck"]),
            "Cardputer": mfm(hw["cp_static"]),
            "CaseL1L": mfm(a["CaseL1L"]), "CaseR1L": mfm(a["CaseR1L"])}
    piores = {}
    for ang in list(range(0, 136, 3)) + [135]:
        T = GEN_lid_T(ang); TT = [[float(T[r][c]) for c in range(4)] for r in range(3)]
        L = [m.transform(TT) for m in lid]
        for k, O in obst.items():
            if k == "Cardputer" and ang <= 0:
                continue
            v = sum((l ^ O).volume() for l in L)
            piores[k] = max(piores.get(k, 0), v)
    # Cardputer: o envelope com folga de 0,3 mm toca a tampa fechada/encostada; permitido < 25 mm³ (= original)
    for k, v in piores.items():
        lim = 25.0 if k == "Cardputer" else 0.5
        assert v < lim, f"tampa colide com {k}: {v:.2f} mm³"

@pytest.mark.parametrize("tipo", TIPOS)
def test_batente_de_135_graus_preservado(asm, tipo):
    """Acima de ~136° a tampa encosta em L1L/R1L (mesmo batente do TERM512)."""
    a = asm[tipo]
    lid = [mfm(m) for m in lid_parts(a)]
    stop = mfm(a["CaseL1L"]) + mfm(a["CaseR1L"])
    T = GEN_lid_T(140); TT = [[float(T[r][c]) for c in range(4)] for r in range(3)]
    assert sum((m.transform(TT) ^ stop).volume() for m in lid) > 5.0


# ================================================================== 5. montagem e retenção
def test_capa_nao_entra_no_corpo_do_cap(asm):
    """Nenhum material da capa dentro do corpo do Cap (inclui fendas abertas que o envelope voxel não fecha)."""
    v = (mfm(asm["A"]["CaseS1L"]) ^ GEN.CAP_CAIXA()).volume()
    assert v < 0.05, f"{v:.2f} mm³ da capa dentro do Cap"

def test_capa_montavel_deslizando_em_Y(asm, hw):
    """A capa entra deslizando em +Y por trás sem cruzar Cap/Cardputer/kickstand em nenhum ponto do curso."""
    p = mfm(asm["A"]["CaseS1L"])
    for k in ["cap_sweepY", "cp_sweepY", "k_sweepY"]:
        assert (p ^ ko(hw, k)).volume() < 0.5, k

def test_capa_retida_em_todas_as_direcoes(asm, hw):
    """Com L1L/R1L parafusados, o conjunto não pode se mover 1 mm em nenhum dos 6 sentidos sem colidir com o Cap."""
    grupo = mfm(asm["A"]["CaseS1L"]) + mfm(asm["A"]["CaseL1L"]) + mfm(asm["A"]["CaseR1L"])
    cap = mfm(hw["cap"])
    for d in [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)]:
        v = (grupo.translate(list(np.array(d, float) * 1.0)) ^ cap).volume()
        assert v > 1.0, f"conjunto livre na direção {d} (colisão {v:.2f} mm³)"

def test_l1l_r1l_apoiados_no_cap(asm):
    """L1L/R1L são cortados exatamente no plano de apoio (Y = topo do Cap + folga)."""
    for k in ["CaseL1L", "CaseR1L"]:
        assert abs(asm["A"][k].bounds[0][1] - GEN.L_CORTE_Y) < 0.05


# ================================================================== 6. acessos funcionais
def _ray_free(mesh, origin, direction, length):
    loc, _, _ = mesh.ray.intersects_location([origin], [direction])
    if len(loc) == 0: return True
    return np.min(np.linalg.norm(loc - origin, axis=1)) > length

def test_antena_sma_acessivel(asm):
    """Furo da antena concêntrico ao RP-SMA e com Ø >= 11 mm (porca/base da antena)."""
    c = asm["A"]["CaseS1L"]
    x, y = GEN.SMA
    for ang in np.linspace(0, 2 * np.pi, 24, endpoint=False):
        o = np.array([x + 5.5 * np.cos(ang), y + 5.5 * np.sin(ang), -20.0])
        assert _ray_free(c, o, np.array([0, 0, 1.0]), 11.5), "furo da antena obstruído"   # de Z=-20 até Z=-8,5 (placa)

def test_parafusos_do_cap_acessiveis(asm):
    c = asm["A"]["CaseS1L"]
    for (x, y) in GEN.PARAF_CAP:
        for dx, dy in [(0, 0), (1.9, 0), (-1.9, 0), (0, 1.9), (0, -1.9)]:
            assert _ray_free(c, np.array([x + dx, y + dy, -20.0]), np.array([0, 0, 1.0]), 11.5)

def test_porta_hy2_do_cap_livre(asm):
    """Janela na parede esquerda deixa o conector HY2.0 do Cap (Y -8,5..1,6 / Z -8,9..-3,5) livre."""
    c = asm["A"]["CaseS1L"]
    for y in np.linspace(-8.3, 1.4, 6):
        for z in np.linspace(-8.7, -3.7, 4):
            assert _ray_free(c, np.array([-10.0, y, z]), np.array([1.0, 0, 0]), 10.2)

def test_saida_de_fios(asm):
    """Canal do L1L alinhado com o furo de saída na parede esquerda (fios TFT -> Grove)."""
    y = sum(GEN.CANAL["y"]) / 2; z = sum(GEN.CANAL["z"]) / 2
    o = np.array([-10.0, y, z])
    assert _ray_free(asm["A"]["CaseS1L"], o, np.array([1.0, 0, 0]), 20)
    assert _ray_free(asm["A"]["CaseL1L"], o, np.array([1.0, 0, 0]), 18)


# ================================================================== 7. placa C2 / texto
@pytest.mark.parametrize("tipo", TIPOS)
def test_c2_preserva_tudo_fora_do_texto(tipo):
    """Fora da região do logotipo a C2 nova é idêntica à original (furos, janela, chanfros, contorno)."""
    o = mfm(trimesh.load(os.path.join(ORIG, f"Type{tipo}", "STL", "CaseC2.stl")))
    n = mfm(trimesh.load(os.path.join(OUT, f"STL_Type{tipo}", "CaseC2_z3ck031.stl")))
    regiao = GEN.box(204 - 76.5, 204 - 10.0, -2, 2, 25.5, 45.7)
    fora_o, fora_n = o - regiao, n - regiao
    assert abs(fora_o.volume() - fora_n.volume()) < 0.05
    assert (fora_o ^ fora_n).volume() == pytest.approx(fora_o.volume(), abs=0.05)

@pytest.mark.parametrize("tipo", TIPOS)
def test_c2_texto_novo(tipo):
    """Letras antigas sumiram e o texto novo está vazado na chapa de 1 mm (a D1 preta aparece por trás)."""
    import logo_z3ck as LOGO
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    n = trimesh.load(os.path.join(OUT, f"STL_Type{tipo}", "CaseC2_z3ck031.stl"))
    src = os.path.join(ORIG, f"Type{tipo}", "STL", "CaseC2.stl")
    L = LOGO.original_letters(src)
    novo = unary_union([LOGO.big_word(L["E_top"], L["E_low"]), LOGO.small_word(L["one"])])
    antigo = unary_union(L["all_letters"])
    s = n.section(plane_origin=[0, 0.5, 0], plane_normal=[0, 1, 0])
    vazios = None                                    # regra par-ímpar: a ilha do "0" é material apoiado no piso
    for e in s.discrete:
        pg = Polygon(np.c_[204 - e[:, 0], e[:, 2]])
        if pg.bounds[1] > 25.5 and pg.bounds[3] < 45.7:
            vazios = pg if vazios is None else vazios.symmetric_difference(pg)
    # o conjunto de bolsões no meio da profundidade deve coincidir com o texto novo
    assert vazios.symmetric_difference(novo).area < 0.02 * novo.area
    assert antigo.difference(novo).intersection(vazios).area < 0.5     # nenhuma letra antiga remanescente
    # metade interna (Y -1..0) continua sendo só os 4 ressaltos dos cantos com seus furos (= original)
    s2 = n.section(plane_origin=[0, -0.5, 0], plane_normal=[0, 1, 0])
    s2o = trimesh.load(src).section(plane_origin=[0, -0.5, 0], plane_normal=[0, 1, 0])
    assert len(s2.discrete) == len(s2o.discrete) == 8

def test_c2_sem_ilhas_soltas():
    """Letras vazadas: nenhum pedaço da chapa pode ficar solto (ex.: miolo do '0')."""
    for t in TIPOS:
        n = trimesh.load(os.path.join(OUT, f"STL_Type{t}", "CaseC2_z3ck031.stl"))
        camada = GEN.to_tm(mfm(n) ^ GEN.box(100, 220, 0.0, 1.0, 25.5, 45.7))     # só a chapa na faixa do texto
        assert len(camada.split(only_watertight=False)) == 1, "ilha solta no texto"

def test_c2_tracos_imprimiveis():
    """Bico 0,4 mm: traços do texto >= 0,8 mm e pontes de material entre letras >= 0,8 mm."""
    import logo_z3ck as LOGO
    L = LOGO.original_letters(os.path.join(ORIG, "TypeA", "STL", "CaseC2.stl"))
    novo = LOGO.big_word(L["E_top"], L["E_low"]).union(LOGO.small_word(L["one"]))
    assert novo.buffer(-0.4).buffer(0.4).area > 0.97 * novo.area        # nenhum traço < 0,8 mm desaparece
    from shapely.geometry import box as sbox
    fundo = sbox(10, 26, 76, 45).difference(novo)
    assert fundo.buffer(-0.4).buffer(0.4).area > 0.97 * fundo.area       # nenhuma ponte < 0,8 mm

def test_c2_encaixa_na_tampa(asm, term):
    """A C2 nova ocupa a mesma posição da original na D1: bbox igual (±0,05 mm), contato com a D1 só por película
    de registro (< 3 mm³ em ~1600 mm² de contato = < 0,002 mm) e, fora do texto, mesma geometria da C2 do STEP."""
    for t in TIPOS:
        a, b = asm[t]["CaseC2_z3ck031"], term[t]["CaseC2"]
        assert np.allclose(a.bounds, b.bounds, atol=0.05)
        assert (mfm(a) ^ mfm(term[t]["CaseD1"])).volume() < 3.0
        n = trimesh.load(os.path.join(OUT, f"STL_Type{t}", "CaseC2_z3ck031.stl"))
        o = trimesh.load(os.path.join(ORIG, f"Type{t}", "STL", "CaseC2.stl"))
        T, _ = GEN.register(o, b)                                   # mesmo registro (determinístico) do gerador
        reg = GEN.box(204 - 76.5, 204 - 10.0, -2, 2, 25.5, 45.7)
        fora_n = (mfm(n) - reg).transform([[float(T[r][c]) for c in range(4)] for r in range(3)])
        fora_o = mfm(b) - (reg.transform([[float(T[r][c]) for c in range(4)] for r in range(3)]))
        dif = (fora_n - fora_o).volume() + (fora_o - fora_n).volume()
        assert dif < 15.0, f"Type{t}: diferença fora do texto {dif:.1f} mm³ (tesselação STEP x STL)"


# ================================================================== 8. imprimibilidade básica das peças novas
def test_espessura_minima_capa(asm):
    """Paredes da capa >= 1,2 mm (3 perímetros de 0,4) medidas por raios internos."""
    c = asm["A"]["CaseS1L"]
    pts, idx = trimesh.sample.sample_surface_even(c, 4000)
    n = c.face_normals[idx]
    o = pts - n * 0.01
    loc, ri, _ = c.ray.intersects_location(o, -n, multiple_hits=False)
    d = np.linalg.norm(loc - o[ri], axis=1)
    frac_fina = np.mean(d < 1.2)
    assert frac_fina < 0.03, f"{frac_fina:.1%} da superfície com parede < 1,2 mm"

def test_capa_vazada(asm, hw):
    """Capa vazada: o Cap aparece pelas janelas; paredes laterais (parafusos da dobradiça), gancho e furo da antena
    continuam com material; janelas da placa da antena têm topo a 45° (imprimem sem suporte com a parede traseira na mesa)."""
    if not GEN.CAPA_VAZADA:
        pytest.skip("capa cheia")
    c = asm["A"]["CaseS1L"]; cap = hw["cap_raw"]
    for (a, b) in GEN.JANELAS_ANTENA["x"]:
        o = np.array([(a + b) / 2, sum(GEN.JANELAS_ANTENA["y"]) / 2, -20.0])
        assert _ray_free(c, o, np.array([0, 0, 1.0]), 15), "janela da placa da antena fechada"
        assert len(cap.ray.intersects_location([o], [[0, 0, 1.0]])[0]) > 0, "Cap não aparece pela janela"
    for (a, b) in GEN.JANELAS_BAIXO["x"]:
        o = np.array([(a + b) / 2, -25.0, sum(GEN.JANELAS_BAIXO["z"]) / 2])
        assert _ray_free(c, o, np.array([0, 1.0, 0]), 9.5), "janela da placa de baixo fechada"
    # nada removido das paredes laterais, do gancho e em volta do furo da antena / parafusos do Cap
    p = mfm(c)
    for zona in [GEN.box(-3, 0, -20, 21, -12, 2), GEN.box(84, 87, -20, 21, -12, 2),            # paredes laterais
                 GEN.box(-3, 87, -18, -14, -0.4, 2.0),                                       # gancho
                 GEN.box(GEN.SMA[0] - 8, GEN.SMA[0] + 8, GEN.SMA[1] - 8, GEN.SMA[1] + 8, -12, -9)]:
        assert (zona ^ p).volume() > 0.9 * (zona ^ mfm(trimesh.load(os.path.join(GEN.CACHE, "capa_cheia_ref.stl")))).volume()
    # topo das janelas da placa da antena: aresta horizontal (ponte) curta
    for (a, b) in GEN.JANELAS_ANTENA["x"]:
        assert (b - a) - 2 * GEN.JANELAS_ANTENA["k"] <= 6.0
