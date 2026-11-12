"""Gera o kit "z3ck 031": adaptação do TERM512 (AREA512) para Cardputer-Adv + Cap LoRa-1262 (GNSS) + TFT 2.8" ILI9341.

Peças NOVAS / MODIFICADAS (as demais são reaproveitadas sem alteração):
  CaseS1L  - capa-suporte em "C" que desliza sobre o Cap LoRa-1262 (substitui S1 + C1 + header 2x7)
  CaseL1L  - L1 original cortado acima do Cap + 2 furos-guia M2 + canal de fios
  CaseR1L  - R1 original cortado acima do Cap + 2 furos-guia M2
  CaseB1   - base original recortada para liberar Cap e capa
  CaseC2   - placa com o texto "z3ck 031" no lugar de "AREA 512" (letras vazadas na chapa de 1 mm, como o original)

Referencial de montagem = o do STEP do TERM512: X largura (0..84), Y espessura (+ = teclado), Z profundidade (- = borda superior).

Uso:  python -I -X utf8 ferramentas/gerar_z3ck031.py
"""
import os, sys, json, shutil, hashlib, time
import numpy as np
import trimesh
import manifold3d as mf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "versao_z3ck031")
CACHE = os.path.join(ROOT, ".cache_z3ck031")   # ~400 MB de voxels; pode apagar (regenera em ~25 min)

from referencial import M5_TO_TERM, envelope
from voxels import Grid
import logo_z3ck as LOGO

# ------------------------------------------------------------------ parâmetros de projeto
FOLGA = 0.30            # folga entre peça impressa e Cap/Cardputer (mm)
CAP_TOPO_Y = 8.66       # face superior do Cap (medida no STL M5Stack)
CAP_FACE_Z = -9.11      # face "topo" do Cap (lado da antena)
CAP_TRAS_Y = -15.34     # face traseira do Cap
PAREDE = 1.6            # espessura das paredes da capa
LAT = 2.0               # espessura das paredes laterais (recebem parafusos M2 escareados)
PLACA_Y_MAX = 7.0       # a placa superior para aqui: a tampa nunca desce abaixo de Y=8,2 (verificado)
GANCHO_Y_MAX = -6.0     # gancho sob o ressalto do Cap termina antes do kickstand K1 (Y>=-4,23)
L_CORTE_Y = CAP_TOPO_Y + FOLGA   # L1/R1 são cortados abaixo deste plano
SMA = (76.41, -7.74)    # centro do conector RP-SMA (X, Y)
SMA_R = 5.75            # raio do furo da antena na capa (rebaixo do Cap tem r=5.29)
PARAF_CAP = [(12.0, 4.75), (12.0, -11.40)]   # parafusos da carcaça do Cap (acesso)
PORTA_A = dict(y=(-9.1, 2.2), z=(-9.6, -2.9))  # janela do HY2.0 do Cap (ponta esquerda)
FENDA_TRAS = dict(x=(35.0, 46.0), z=(-2.9, -0.7))  # fenda/LED na traseira do Cap
FUROS_L = [(11.2, -3.2), (13.8, -6.4)]       # (Y, Z) dos parafusos M2 que prendem L1L/R1L na capa
CANAL = dict(y=(L_CORTE_Y - 0.05, 10.8), z=(-7.6, -4.6))  # canal de fios no fundo do L1L
CAP_FACE_TRAS_Z = -0.65 # face do Cap voltada para o teclado, atrás da traseira (meio; -0,11 nas pontas)
PIVO = (16.88, -5.07)   # eixo da dobradiça (Y, Z) - não muda
# capa VAZADA: janelas octogonais (cantos a 45°: imprimem sem suporte com a parede traseira na mesa)
CAPA_VAZADA = True
JANELAS_ANTENA = dict(x=[(18.5, 32.5), (35.5, 49.5), (52.5, 66.5)], y=(-14.5, 4.5), k=4.5)   # placa do lado da antena (plano XY)
JANELAS_BAIXO = dict(x=[(4.0, 19.0), (22.0, 33.0), (48.0, 63.0), (66.0, 80.0)], z=(-8.2, -3.4), k=1.5)  # placa de baixo (plano XZ)

LID = ["CaseD1", "CaseD2", "CaseD3", "CaseC2", "CaseC3", "PCB", "Frame__1_", "SOLID", "Glass"]


# ------------------------------------------------------------------ utilidades
def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)

def to_mf(m):
    m = m.copy(); m.merge_vertices()
    M = mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32), tri_verts=np.asarray(m.faces, np.uint32)))
    if M.status() != mf.Error.NoError:
        raise ValueError(f"malha não-manifold: {M.status()}")
    return M

def to_tm(M):
    g = M.to_mesh()
    v = np.asarray(g.vert_properties)[:, :3].astype(np.float64)
    f = np.asarray(g.tri_verts).astype(np.int64)
    mfrom, mto = np.asarray(g.merge_from_vert), np.asarray(g.merge_to_vert)
    if len(mfrom):                                     # costuras entre malhas de origem diferente
        idx = np.arange(len(v)); idx[mfrom] = mto; f = idx[f]
    m = trimesh.Trimesh(v, f, process=False)
    m.remove_unreferenced_vertices()
    return m

def ok(M, what=""):
    """Falha cedo se o manifold estiver vazio ou em estado de erro (o erro se propaga em booleanos)."""
    if M.status() != mf.Error.NoError or M.is_empty():
        raise RuntimeError(f"manifold inválido/vazio: {what} {M.status()}")
    return M

def box(x0, x1, y0, y1, z0, z1):
    return mf.Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])

def CAP_CAIXA():
    """Caixa do corpo principal do Cap + folga (sem as paredes laterais, sem orelhas/placas das pontas)."""
    return box(0.3, 83.7, CAP_TRAS_Y - FOLGA, CAP_TOPO_Y + FOLGA, CAP_FACE_Z - FOLGA, CAP_FACE_TRAS_Z + FOLGA)

def teardrop_z(cx, cy, r, z0, z1, tip=(0, 1)):
    """Furo cilíndrico ao longo de Z com ponta a 45° (gota) apontando em 'tip' = direção de crescimento na impressão."""
    from shapely.geometry import MultiPoint
    a = np.linspace(0, 2 * np.pi, 72, endpoint=False)
    t = np.array(tip, float); t /= np.linalg.norm(t)
    pts = np.vstack([np.c_[np.cos(a), np.sin(a)] * r, t * r * np.sqrt(2)])
    from shapely.geometry.polygon import orient
    hull = np.array(orient(MultiPoint(pts).convex_hull, 1.0).exterior.coords)[:-1] + [cx, cy]   # anti-horário
    return ok(mf.Manifold.extrude(mf.CrossSection([hull]), z1 - z0).translate([0, 0, z0]), "gota")

def cyl_x(y, z, r, x0, x1):
    return mf.Manifold.cylinder(x1 - x0, r, r, 48).rotate([0, 90, 0]).translate([x0, y, z])

def cone_x(y, z, r0, r1, x0, x1):
    return mf.Manifold.cylinder(x1 - x0, r0, r1, 48).rotate([0, 90, 0]).translate([x0, y, z])

def export_ok(m, path):
    """Grava STL e confere ida-e-volta (float32): fechado, 1 corpo, mesmo volume."""
    m.export(path)
    r = trimesh.load(path)
    if not (r.is_watertight and len(r.split(only_watertight=False)) == 1 and abs(r.volume - m.volume) < 0.01 * abs(m.volume)):
        raise RuntimeError(f"STL inválido após gravação: {path} wt={r.is_watertight}")
    return r

def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()[:16]


# ------------------------------------------------------------------ carga das referências
def load_term(tipo):
    """Peças do STEP do TERM512 na posição de montagem (cache em STL)."""
    d = os.path.join(CACHE, f"term512_{tipo}")
    step = os.path.join(ROOT, "versao_AREA512", "term512", "Model", f"Type{tipo}", f"CapTFT_512_{tipo}.step")
    if not os.path.isdir(d) or not os.listdir(d):
        log("lendo STEP", step)
        import step_info, cadquery as cq
        os.makedirs(d, exist_ok=True)
        seen = {}
        for nm, shp in step_info.read(step):
            seen[nm] = seen.get(nm, 0) + 1
            if seen[nm] > 1: nm = f"{nm}_{seen[nm]}"
            safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in nm)
            cq.exporters.export(cq.Workplane().add(cq.Shape.cast(shp)), os.path.join(d, safe + ".stl"), tolerance=0.02, angularTolerance=0.1)
    return {os.path.splitext(f)[0]: trimesh.load(os.path.join(d, f)) for f in os.listdir(d) if f.endswith(".stl")}

def load_m5(name):
    m = trimesh.load(os.path.join(ROOT, "referencias_m5stack", name), force="mesh")
    bodies = [b for b in m.split(only_watertight=False)
              if b.bounds[1][1] < 50 and b.bounds[0][1] > -50 and b.bounds[1][2] < 12 and b.bounds[0][2] > -25]
    out = trimesh.util.concatenate(bodies); out.apply_transform(M5_TO_TERM)
    return out

def cap_envelope():
    p = os.path.join(CACHE, "cap_envelope_015.stl")
    if not os.path.exists(p):
        log("envelope sólido do Cap (voxel 0,15 mm)")
        envelope(load_m5("Cap_LoRa-1262.stl"), pitch=0.15).export(p)
    return trimesh.load(p)


# ------------------------------------------------------------------ registro rígido (frame do STL original <-> montagem)
def _rot24():
    mats = []
    import itertools
    for perm in itertools.permutations(range(3)):
        for signs in itertools.product([1, -1], repeat=3):
            R = np.zeros((3, 3))
            for i, (p, s) in enumerate(zip(perm, signs)): R[i, p] = s
            if np.linalg.det(R) > 0: mats.append(R)
    return mats

def register(src, dst):
    """Transformação rígida 4x4 que leva 'src' (mesma peça, outro frame) até 'dst'.
    Inicializa pelos eixos principais de inércia (4 combinações de sinal com det=+1) e refina com ICP."""
    from scipy.spatial import cKDTree
    ps = trimesh.sample.sample_surface(src, 6000, seed=7)[0]      # semente fixa: registro reprodutível
    pd = trimesh.sample.sample_surface(dst, 60000, seed=11)[0]
    def frame(m):
        v = m.principal_inertia_vectors.copy()            # linhas = eixos
        return v
    Fs, Fd = frame(src), frame(dst)
    cs, cd = src.center_mass, dst.center_mass
    best = (np.inf, None)
    for sx in (1, -1):
        for sy in (1, -1):
            S = np.diag([sx, sy, sx * sy])
            R = Fd.T @ S @ Fs
            if np.linalg.det(R) < 0: continue
            T = np.eye(4); T[:3, :3] = R; T[:3, 3] = cd - R @ cs
            M, _, cost = trimesh.registration.icp(trimesh.transform_points(ps, T), pd, max_iterations=80, reflection=False, scale=False)
            T2 = M @ T
            # critério final: volume da diferença simétrica (distingue placa virada frente/verso, que o ICP não vê)
            try:
                a = to_mf(src.copy().apply_transform(T2)); b = to_mf(dst)
                score = (a - b).volume() + (b - a).volume()
            except ValueError:
                score = cost * 1e3
            if score < best[0]: best = (score, T2)
    # refino final: ICP contra a SUPERFÍCIE exata do destino (ponto mais próximo na malha), mais pontos
    pf = trimesh.sample.sample_surface(src, 20000, seed=3)[0]
    M, _, _ = trimesh.registration.icp(trimesh.transform_points(pf, best[1]), dst, max_iterations=60, reflection=False, scale=False, threshold=1e-7)
    T3 = M @ best[1]
    a = to_mf(src.copy().apply_transform(T3)); b = to_mf(dst)
    score3 = (a - b).volume() + (b - a).volume()
    if score3 < best[0]: best = (score3, T3)
    return best[1], best[0]


# ------------------------------------------------------------------ zonas de exclusão
def keepouts(P, cap_env):
    """Retorna dicionário de malhas de exclusão (já com folga), no referencial de montagem."""
    path = os.path.join(CACHE, "keepouts.npz")
    G = Grid(lo=(-5, -22, -16), hi=(90, 26, 10), pitch=0.15)
    names = ["cap_static", "cap_sweepY", "cp_static", "cp_sweepY", "k_sweepY"]
    files = {n: os.path.join(CACHE, f"ko_{n}.stl") for n in names}
    if all(os.path.exists(f) for f in files.values()):
        return {n: trimesh.load(f) for n, f in files.items()}, G
    log("voxelizando Cap / Cardputer / kickstand")
    cap_o = G.voxelize(cap_env)
    cp_o = G.voxelize(P["CardputerADV"]) | G.voxelize(load_m5("Cardputer-Adv.stl"), fill=False)
    k_o = G.voxelize(P["CaseK1"]) | G.voxelize(P["CaseK2"])
    log("dilatação de folga e varredura de montagem (+Y)")
    cap_d = G.dilate(cap_o, FOLGA); cp_d = G.dilate(cp_o, FOLGA); k_d = G.dilate(k_o, FOLGA)
    occ = {"cap_static": cap_d, "cap_sweepY": G.sweep(cap_d, 1, +1),
           "cp_static": cp_d, "cp_sweepY": G.sweep(cp_d, 1, +1), "k_sweepY": G.sweep(k_d, 1, +1)}
    out = {}
    for n, o in occ.items():
        m = G.to_mesh(o); m.export(files[n]); out[n] = m
    return out, G


# ------------------------------------------------------------------ peças novas
def octogono(a, b, c, d, k):
    from shapely.geometry import Polygon
    from shapely.geometry.polygon import orient
    k = min(k, (b - a) / 2 - 0.01, (d - c) / 2 - 0.01)
    P = Polygon([(a, c + k), (a + k, c), (b - k, c), (b, c + k), (b, d - k), (b - k, d), (a + k, d), (a, d - k)])
    return np.array(orient(P, 1.0).exterior.coords)[:-1]

def janelas_capa():
    """Sólidos das janelas: placa do lado da antena (atravessa em Z) e placa de baixo (atravessa em Y)."""
    z_top0 = CAP_FACE_Z - FOLGA - PAREDE
    y_back0 = CAP_TRAS_Y - FOLGA - PAREDE
    W = None
    for (a, b) in JANELAS_ANTENA["x"]:
        cs = mf.CrossSection([octogono(a, b, *JANELAS_ANTENA["y"], JANELAS_ANTENA["k"])])
        e = mf.Manifold.extrude(cs, (CAP_FACE_Z - FOLGA + 0.4) - (z_top0 - 1)).translate([0, 0, z_top0 - 1])
        W = e if W is None else W + e
    for (a, b) in JANELAS_BAIXO["x"]:
        cs = mf.CrossSection([octogono(a, b, *JANELAS_BAIXO["z"], JANELAS_BAIXO["k"])])
        h = (CAP_TRAS_Y - FOLGA + 0.4) - (y_back0 - 1)
        e = mf.Manifold.extrude(cs, h).transform([[1, 0, 0, 0], [0, 0, 1, y_back0 - 1], [0, 1, 0, 0]])
        W = W + e
    return ok(W, "janelas")

def make_capa(KO, vazada=False):
    """CaseS1L: capa em "C" sobre o Cap (placa no topo + paredes laterais + parede traseira + gancho)."""
    x0, x1 = -FOLGA - LAT, 84.0 + FOLGA + LAT
    z_top0 = CAP_FACE_Z - FOLGA - PAREDE          # face externa da placa
    y_back0 = CAP_TRAS_Y - FOLGA - PAREDE         # face externa da parede traseira
    z_hook1 = 0.0 + FOLGA + PAREDE + 0.05         # face externa do gancho
    body = (box(x0, x1, y_back0, PLACA_Y_MAX, z_top0, -8.6)                 # placa superior (o excesso é removido pelo keep-out)
            + box(x0, x1, y_back0, GANCHO_Y_MAX, z_top0, z_hook1)            # parede traseira + gancho
            + box(x0, -FOLGA, y_back0, 19.77, z_top0, -0.40)                 # parede lateral esquerda
            + box(84.0 + FOLGA, x1, y_back0, 19.77, z_top0, -0.40))          # parede lateral direita
    # cantos externos verticais arredondados (estética / impressão)
    # As faces laterais do Cap/Cardputer são planos exatos em X=0 e X=84; o voxel "transborda" até ~0,2 mm.
    # Recorta as exclusões em X∈[-0,25; 84,25] para não gerar lâminas de espessura zero na face interna das paredes.
    clip = box(-FOLGA + 0.05, 84.0 + FOLGA - 0.05, -60, 60, -60, 60)
    for name in ("cap_sweepY", "cp_sweepY", "cp_static", "k_sweepY"):
        body = body - (to_mf(KO[name]) ^ clip)
    # corpo principal do Cap como caixa: fecha fendas/cavidades abertas que o envelope voxel não fecha
    body = body - CAP_CAIXA()
    # furos e janelas
    holes = teardrop_z(SMA[0], SMA[1], SMA_R, z_top0 - 1, -5, tip=(0, 1))
    for (x, y) in PARAF_CAP:
        holes = holes + teardrop_z(x, y, 2.7, z_top0 - 1, -5, tip=(0, 1))
    holes = holes + box(x0 - 1, 1.0, PORTA_A["y"][0], PORTA_A["y"][1], PORTA_A["z"][0], PORTA_A["z"][1])
    holes = holes + box(FENDA_TRAS["x"][0], FENDA_TRAS["x"][1], y_back0 - 1, CAP_TRAS_Y, FENDA_TRAS["z"][0], FENDA_TRAS["z"][1])
    for (y, z) in FUROS_L:                       # M2 escareado nas duas paredes laterais
        holes = holes + cyl_x(y, z, 1.2, x0 - 1, x1 + 1)
        holes = holes + cone_x(y, z, 2.1, 1.0, x0 - 0.01, x0 + 1.1) + cone_x(y, z, 1.0, 2.1, x1 - 1.1, x1 + 0.01)
    # saída dos fios na parede esquerda, alinhada ao canal do L1L
    holes = holes + cyl_x(sum(CANAL["y"]) / 2, sum(CANAL["z"]) / 2, 1.5, x0 - 1, 0.5)
    body = ok(body - ok(holes, "furos da capa"), "capa")
    if vazada:
        body = ok(body - janelas_capa(), "capa vazada")
    parts = body.decompose()
    body = max(parts, key=lambda p: p.volume())   # descarta lascas soltas (se houver)
    return ok(body.simplify(0.01), "capa simplificada"), len(parts)

def make_L(P, which, KO):
    src = to_mf(P["CaseL1" if which == "L" else "CaseR1"])
    keep = box(-5, 90, L_CORTE_Y, 40, -20, 10)
    m = (src ^ keep) - to_mf(KO["cap_static"])
    xs = (0.0, 6.0) if which == "L" else (78.0, 84.0)   # furos-guia (Ø1,7) a partir da face externa
    for (y, z) in FUROS_L:
        m = m - cyl_x(y, z, 0.85, xs[0] - 0.5, xs[1] + 0.5)
    if which == "L":
        m = m - box(-1, 9.5, CANAL["y"][0], CANAL["y"][1], CANAL["z"][0], CANAL["z"][1])
    parts = m.decompose()
    return max(parts, key=lambda p: p.volume()), len(parts)

def make_B1(P, capa_tm, G, cap_sweep):
    """B1 recortada: remove tudo que conflita com Cap e capa (com folga), na direção de montagem +Y."""
    path = os.path.join(CACHE, "ko_b1.stl")
    key = hashlib.sha256(np.ascontiguousarray(np.round(capa_tm.vertices, 4)).tobytes()).hexdigest()[:16]
    if os.path.exists(path) and open(path + ".key").read() == key:
        ko_m = trimesh.load(path)
    else:
        occ = G.voxelize(capa_tm)
        ko_m = G.to_mesh(G.sweep(G.dilate(occ, FOLGA), 1, +1))
        ko_m.export(path); open(path + ".key", "w").write(key)
    b1 = to_mf(P["CaseB1"]) - to_mf(ko_m) - to_mf(cap_sweep)
    parts = sorted(b1.decompose(), key=lambda p: -p.volume())
    return ok(parts[0].simplify(0.01), "B1"), [round(p.volume(), 2) for p in parts], parts[1:]


# ------------------------------------------------------------------ C2 com novo texto
def make_C2(tipo):
    """Constrói C2 no frame do STL original (face visível = +Y, letras em baixo-relevo Y 0..1)."""
    src_path = os.path.join(ROOT, "versao_AREA512", "term512", "Model", f"Type{tipo}", "STL", "CaseC2.stl")
    orig = trimesh.load(src_path)
    L = LOGO.original_letters(src_path)
    big = LOGO.big_word(L["E_top"], L["E_low"])
    small = LOGO.small_word(L["one"])
    # preenche os bolsões antigos (região das letras) e escava o texto novo
    # chapa de 1 mm (Y 0..1) com letras vazadas; preenche exatamente essa camada (não invade a D1 em Y<0)
    # Na região do texto a peça original é só a chapa Y 0..1 (sem ressaltos): troca a região inteira por uma chapa nova.
    reg = (204 - 76.0, 204 - 10.5, 26.0, 45.2)                     # X0, X1, Z0, Z1
    M = to_mf(orig) - box(reg[0], reg[1], -1.5, 1.5, reg[2], reg[3])
    chapa = box(reg[0], reg[1], 0.0, 1.0, reg[2], reg[3])
    from shapely import affinity
    txt = affinity.affine_transform(big.union(small), [-1, 0, 0, 1, 204, 0])   # (u,v) -> (X = 204-u, Z = v)
    def extrude_y(geom, y0, y1):
        res = None
        for p in (geom.geoms if hasattr(geom, "geoms") else [geom]):
            rings = [np.array(p.exterior.coords)[:-1]] + [np.array(i.coords)[:-1] for i in p.interiors]
            e = mf.Manifold.extrude(mf.CrossSection(rings, mf.FillRule.EvenOdd), y1 - y0)   # (X, Z, t)
            e = e.transform([[1, 0, 0, 0], [0, 0, 1, y0], [0, 1, 0, 0]])                     # -> (X, y0+t, Z)
            res = e if res is None else res + e
        return res
    cut = ok(extrude_y(txt, -0.2, 1.2), "texto")      # vazado na chapa de 1 mm
    new = ok((M + (chapa - cut)).simplify(0.001), "C2")
    return new, orig, big, small


# ------------------------------------------------------------------ principal
def main():
    os.makedirs(CACHE, exist_ok=True)
    rel = {"parametros": {k: v for k, v in globals().items() if k.isupper() and isinstance(v, (int, float, tuple, list, dict))}}
    P = load_term("A")
    cap_env = cap_envelope()
    KO, G = keepouts(P, cap_env)
    log("capa CaseS1L")
    capa_cheia, n_capa = make_capa(KO)                       # a B1 é recortada pela capa cheia (não muda)
    to_tm(capa_cheia).export(os.path.join(CACHE, "capa_cheia_ref.stl"))   # referência para o teste da capa vazada
    capa = make_capa(KO, vazada=True)[0] if CAPA_VAZADA else capa_cheia
    log("L1L / R1L")
    L1L, nL = make_L(P, "L", KO); R1L, nR = make_L(P, "R", KO)
    capa_tm = to_tm(capa)
    log("B1 recortada")
    B1n, b1_parts, b1_soltas = make_B1(P, to_tm(capa_cheia), G, KO["cap_sweepY"])
    for i, p in enumerate(b1_soltas):
        to_tm(p).export(os.path.join(CACHE, f"b1_lasca_{i}.stl"))
    asm = {"CaseS1L": capa_tm, "CaseL1L": to_tm(L1L), "CaseR1L": to_tm(R1L), "CaseB1_z3ck": to_tm(B1n)}
    rel["pecas_soltas_descartadas"] = {"capa": n_capa - 1, "L1L": nL - 1, "R1L": nR - 1, "B1": b1_parts[1:]}

    # ---- exportação: montagem (referencial TERM) e impressão
    for tipo in ("A", "B"):
        PT = P if tipo == "A" else load_term("B")
        d_stl = os.path.join(OUT, f"STL_Type{tipo}")
        d_asm = os.path.join(OUT, "montagem", f"Type{tipo}")
        os.makedirs(d_stl, exist_ok=True); os.makedirs(d_asm, exist_ok=True)
        log(f"C2 Type{tipo}")
        C2n, C2o, big, small = make_C2(tipo)
        C2n_tm = to_tm(C2n)
        # C2: frame STL -> montagem
        T, cost = register(C2o, PT["CaseC2"])
        rel[f"registro_C2_{tipo}_mm3_diferenca"] = float(cost)
        C2_asm = C2n_tm.copy(); C2_asm.apply_transform(T)
        for k, m in list(asm.items()) + [("CaseC2_z3ck031", C2_asm)]:
            export_ok(m, os.path.join(d_asm, k + ".stl"))
        # peças reaproveitadas na montagem
        for k in ["CaseD1", "CaseD2", "CaseD3", "CaseC3", "CaseT1", "CaseT1_2", "CaseK1", "CaseK2", "PCB", "Frame__1_", "SOLID", "Glass"]:
            if k in PT: PT[k].export(os.path.join(d_asm, k + ".stl"))
        # ---- impressão
        export_ok(C2n_tm, os.path.join(d_stl, "CaseC2_z3ck031.stl"))                         # mesmo frame do STL original
        Tb, costb = register(trimesh.load(os.path.join(ROOT, "versao_AREA512", "term512", "Model", f"Type{tipo}", "STL", "CaseB1.stl")), P["CaseB1"])
        rel[f"registro_B1_{tipo}_mm3_diferenca"] = float(costb)
        b1p = asm["CaseB1_z3ck"].copy(); b1p.apply_transform(np.linalg.inv(Tb))          # volta ao frame/orientação do STL original
        export_ok(b1p, os.path.join(d_stl, "CaseB1_z3ck.stl"))
        c = asm["CaseS1L"].copy(); c.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
        c.apply_translation(-c.bounds[0]); export_ok(c, os.path.join(d_stl, "CaseS1L_capa_cap.stl"))   # parede traseira na mesa
        l = asm["CaseL1L"].copy(); l.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [0, 1, 0]))
        l.apply_translation(-l.bounds[0]); export_ok(l, os.path.join(d_stl, "CaseL1L.stl"))             # face externa na mesa
        r = asm["CaseR1L"].copy(); r.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [0, 1, 0]))
        r.apply_translation(-r.bounds[0]); export_ok(r, os.path.join(d_stl, "CaseR1L.stl"))
        src = os.path.join(ROOT, "versao_AREA512", "term512", "Model", f"Type{tipo}", "STL")
        for k in ["CaseD1", "CaseD2", "CaseD3", "CaseC3", "CaseT1", "CaseK1", "CaseK2"]:
            shutil.copy2(os.path.join(src, k + ".stl"), os.path.join(d_stl, k + ".stl"))
    json.dump(rel, open(os.path.join(OUT, "relatorio_geracao.json"), "w", encoding="utf-8"), indent=1, default=str, ensure_ascii=False)
    log("ok ->", OUT)

if __name__ == "__main__":
    main()
