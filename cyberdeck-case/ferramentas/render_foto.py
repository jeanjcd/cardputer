"""Render "fotográfico" do z3ck 031 montado (VTK PBR + IBL sintético + sombras + SSAO + bloom).

Uso:  python -I -X utf8 ferramentas/render_foto.py [TypeA|TypeB]
Saída: versao_z3ck031/renders/foto_<paleta>_<vista>.png

Referencial das peças montadas = frame TERM: X largura, Y espessura (+ = teclado), Z profundidade (- = borda do Cap).
O conjunto é apoiado na mesa pela própria geometria (o Cap fica ~15 mm abaixo do fundo -> cunha de digitação).
"""
import sys, os, glob
import numpy as np, trimesh, vtk
from vtk.util.numpy_support import numpy_to_vtk, numpy_to_vtkIdTypeArray
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy.spatial import ConvexHull

sys.path.insert(0, os.path.dirname(__file__))
import gerar_z3ck031 as G

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIPO = sys.argv[1] if len(sys.argv) > 1 else "TypeA"
OUT = os.path.join(ROOT, "versao_z3ck031", "renders")
TAMPA_GRAUS = 112          # abertura da tampa (0 = fechada, 135 = batente)
SS = 2                      # supersampling (renderiza 2x e reduz)
W, H = 1600, 1100

def srgb(h):
    h = h.lstrip("#"); return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))

# ---------------------------------------------------------------- paletas (cores de filamentos comuns)
PALETAS = {
    "magenta": {
        "nome": "Preto fosco + Magenta neon + Ciano",
        "base": srgb("#1b1b1f"), "acento": srgb("#ff1fa8"), "detalhe": srgb("#00d9ff"),
        "luz_a": (1.0, 0.12, 0.65), "luz_b": (0.0, 0.85, 1.0),
    },
    "amarelo": {
        "nome": "Preto fosco + Amarelo neon",
        "base": srgb("#1b1b1f"), "acento": srgb("#f2ff00"), "detalhe": srgb("#f2ff00"), "logo": srgb("#1b1b1f"),
        "luz_a": (1.0, 0.75, 0.05), "luz_b": (0.0, 0.85, 1.0),
    },
}
BRANCO_ADV = srgb("#ecebe6")
CINZA_CAP = srgb("#7d8087")

# papel de cada peça -> (cor da paleta, rugosidade)
PAPEL = {
    "CaseS1L": "base", "CaseB1_z3ck": "base", "CaseK1": "base", "CaseD1": "base", "CaseC3": "base",
    "CaseD2": "base", "CaseD3": "base",
    "CaseL1L": "acento", "CaseR1L": "acento", "CaseC2_z3ck031": "acento", "CaseK2": "acento",
    "CaseT1": "detalhe", "CaseT1_2": "detalhe",
    "CaseD1R": "base", "CaseD2R_z3ck031": "acento", "CaseC3R": "base",   # TypeR (tela vermelha)
}
TAMPA = ["CaseD1", "CaseC2_z3ck031", "CaseC3", "CaseD2", "CaseD3", "Frame__1_", "PCB", "SOLID",
         "CaseD1R", "CaseD2R_z3ck031", "CaseC3R", "TFT_painel", "TFT_pcb", "TFT_sd", "LOGO_fundo"]

# ---------------------------------------------------------------- geometria
def carregar():
    d = os.path.join(ROOT, "versao_z3ck031", "montagem", TIPO)
    A = {os.path.basename(f)[:-4]: trimesh.load(f) for f in glob.glob(os.path.join(d, "*.stl"))}
    cap = G.load_m5("Cap_LoRa-1262.stl"); cp = G.load_m5("Cardputer-Adv.stl")
    if "CaseD2R_z3ck031" in A:
        # fundo do logo gravado na D2R (mostra o efeito da troca de filamento a 0,6 mm)
        import gerar_tampa_vermelha as RV
        txt, _ = RV.logo_2d()
        f = G.to_tm(RV.extrude_y(txt, RV.Y_FRENTE + RV.GRAVACAO - 0.05, RV.Y_FRENTE + RV.GRAVACAO + 0.01))
        f.apply_transform(np.linalg.inv(T_tampa(0))); A["LOGO_fundo"] = f
    return A, cap, cp

def T_tampa(ang):
    return trimesh.transformations.rotation_matrix(np.radians(135 - ang), [1, 0, 0], [0, G.PIVO[0], G.PIVO[1]])

def quad_tela(glass, ang):
    """Quad texturizado sobre a face visível do vidro da TFT (calculado com a tampa fechada)."""
    T0 = T_tampa(0); g = glass.copy(); g.apply_transform(T0)
    (x0, y0, z0), (x1, y1, z1) = g.bounds
    y = y0 - 0.06                                  # tampa fechada: a tela olha para o teclado (-Y)
    V = np.array([[x0, y, z0], [x1, y, z0], [x1, y, z1], [x0, y, z1]], float)
    UV = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], float)   # v=1 longe da dobradiça (topo da tela)
    M = T_tampa(ang) @ np.linalg.inv(T0)
    V = trimesh.transformations.transform_points(V, M)
    return V, UV, (x1 - x0) / (z1 - z0)

def quad_aa(ang):
    """Quad da área ativa da TFT vermelha (TypeR), a partir dos parâmetros do gerador."""
    import gerar_tampa_vermelha as RV
    _, aa, y_vidro = RV.tela_envelope()
    x0, x1, z0, z1 = aa; y = y_vidro - 0.06
    V = np.array([[x0, y, z0], [x1, y, z0], [x1, y, z1], [x0, y, z1]], float)
    UV = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], float)
    V = trimesh.transformations.transform_points(V, T_tampa(ang) @ np.linalg.inv(T_tampa(0)))
    return V, UV, (x1 - x0) / (z1 - z0)

def antena(lid_meshes):
    """Antena 'rubber duck' com articulação: base RP-SMA (eixo -Z), dobra para cima/fora sem tocar a tampa."""
    p0 = np.array([G.SMA[0], G.SMA[1], -9.11])
    hexa = trimesh.creation.cylinder(radius=4.2, height=7, sections=6)
    hexa.apply_transform(trimesh.geometry.align_vectors([0, 0, 1], [0, 0, -1])); hexa.apply_translation(p0 + [0, 0, -3.5])
    knuckle_c = p0 + [0, 0, -7 - 9]
    base = trimesh.creation.cylinder(radius=4.6, height=18, sections=48)
    base.apply_translation(p0 + [0, 0, -7 - 9])
    lid = trimesh.util.concatenate(lid_meshes)
    best = None
    for a in np.radians(np.arange(0, 91, 3)):          # elevação em direção a +Y (para "cima" do teclado)
        dvec = np.array([np.sin(np.radians(12)), np.sin(a), -np.cos(a)]); dvec /= np.linalg.norm(dvec)
        pts = knuckle_c + np.outer(np.linspace(4, 100, 40), dvec)
        dist = np.abs(trimesh.proximity.signed_distance(lid, pts))
        if dist.min() > 7.0:
            best = dvec
    whip = trimesh.creation.cone(radius=4.8, height=95, sections=48)  # afunila
    whip = trimesh.creation.cylinder(radius=4.3, height=88, sections=48)
    whip.apply_translation([0, 0, 44 + 3])
    tip = trimesh.creation.icosphere(subdivisions=3, radius=4.3); tip.apply_translation([0, 0, 91])
    joint = trimesh.creation.icosphere(subdivisions=3, radius=5.2)
    w = trimesh.util.concatenate([whip, tip])
    w.apply_transform(trimesh.geometry.align_vectors([0, 0, 1], best)); w.apply_translation(knuckle_c)
    joint.apply_translation(knuckle_c)
    return {"metal": hexa, "borracha": trimesh.util.concatenate([base, joint, w])}

def pose_apoio(meshes_peso):
    """Rotação em X que apoia o conjunto na mesa (aresta do casco convexo YZ sob o centro de massa)."""
    P = np.vstack([m.vertices for m, _ in meshes_peso])
    com = sum(w * m.bounds.mean(0) for m, w in meshes_peso) / sum(w for _, w in meshes_peso)
    q = P[:, [2, 1]]; hull = ConvexHull(q)
    c = com[[2, 1]]; best = None
    for i, j in hull.simplices:
        a, b = q[i], q[j]; e = b - a; n = np.array([e[1], -e[0]]); n /= np.linalg.norm(n)
        if np.dot(n, q[hull.vertices].mean(0) - a) > 0: n = -n
        if n[1] > -0.3: continue                                    # só arestas por baixo
        t = np.dot(c - a, e) / np.dot(e, e)
        if 0 <= t <= 1 and (best is None or n[1] < best[1][1]): best = (e, n)
    n = best[1]; ang = np.arctan2(n[0], -n[1])                      # gira n para (0,-1) no plano (z,y)
    R = trimesh.transformations.rotation_matrix(ang, [1, 0, 0])
    print("inclinacao na mesa: %.1f graus" % np.degrees(ang))
    Pz = trimesh.transformations.transform_points(P, R)
    R[:3, 3] -= [0, Pz[:, 1].min(), 0]
    return R

# ---------------------------------------------------------------- textura da TFT (UI cyberpunk, 320x240 -> 4x)
def textura_ui(aspect, acento, detalhe):
    S = 4; w, h = 320 * S, int(320 * S / aspect)
    im = Image.new("RGB", (w, h), (4, 6, 12)); d = ImageDraw.Draw(im)
    f = lambda s, b=False: ImageFont.truetype("C:/Windows/Fonts/consolab.ttf" if b else "C:/Windows/Fonts/consola.ttf", s * S)
    ca = tuple(int(255 * v) for v in acento); cd = tuple(int(255 * v) for v in detalhe)
    verde = (60, 255, 140)
    for x in range(0, w, 16 * S): d.line([(x, 0), (x, h)], fill=(10, 22, 30))
    for y in range(0, h, 16 * S): d.line([(0, y), (w, y)], fill=(10, 22, 30))
    d.rectangle([0, 0, w, 22 * S], fill=ca); d.text((6 * S, 3 * S), "z3ck 031 // MESH NODE", font=f(15, True), fill=(5, 5, 10))
    d.text((w - 70 * S, 4 * S), "23:47", font=f(14, True), fill=(5, 5, 10))
    d.text((8 * S, 30 * S), "GNSS  FIX 3D  SAT 11", font=f(12), fill=verde)
    d.text((8 * S, 46 * S), "LAT  --.----  LON  --.----", font=f(12), fill=cd)
    d.text((8 * S, 62 * S), "LoRa 915MHz SF9  RSSI -87dBm", font=f(12), fill=cd)
    # radar
    cx, cy, r = 70 * S, 150 * S, 52 * S
    for k in (1, 2, 3): d.ellipse([cx - r * k / 3, cy - r * k / 3, cx + r * k / 3, cy + r * k / 3], outline=(0, 120, 90), width=S)
    d.line([cx - r, cy, cx + r, cy], fill=(0, 90, 70), width=S); d.line([cx, cy - r, cx, cy + r], fill=(0, 90, 70), width=S)
    d.pieslice([cx - r, cy - r, cx + r, cy + r], 300, 340, fill=(0, 90, 60))
    for (dx, dy, c) in [(20, -25, ca), (-30, 12, verde), (34, 22, cd)]:
        d.ellipse([cx + dx * S - 4 * S, cy + dy * S - 4 * S, cx + dx * S + 4 * S, cy + dy * S + 4 * S], fill=c)
    # lista de nós
    x0 = 140 * S
    d.text((x0, 92 * S), "NODES", font=f(12, True), fill=ca)
    for i, (n, s) in enumerate([("!a3f2 r0gue", "-71"), ("!19c0 n3tr", "-88"), ("!77de d4wn", "-102")]):
        d.text((x0, (110 + 16 * i) * S), f"{n:<12}{s}", font=f(12), fill=verde if i == 0 else cd)
    # espectro
    rng = np.random.default_rng(7); y0 = h - 14 * S
    for i in range(40):
        v = int((10 + 40 * abs(np.sin(i * 0.45)) * rng.uniform(0.5, 1)) * S)
        d.rectangle([x0 + i * 4 * S, y0 - v, x0 + i * 4 * S + 2 * S, y0], fill=ca if i % 7 == 0 else (0, 200, 160))
    d.text((8 * S, h - 16 * S), "> ping --mesh_", font=f(12), fill=verde)
    return im

# ---------------------------------------------------------------- VTK
def poly(mesh, uv=None, faces=None):
    pts = vtk.vtkPoints(); pts.SetData(numpy_to_vtk(np.ascontiguousarray(mesh if uv is not None else mesh.vertices, dtype=np.float64), deep=True))
    F = faces if faces is not None else mesh.faces
    f = np.hstack([np.full((len(F), 1), 3), F]).astype(np.int64).ravel()
    cells = vtk.vtkCellArray(); cells.SetCells(len(F), numpy_to_vtkIdTypeArray(f, deep=True))
    p = vtk.vtkPolyData(); p.SetPoints(pts); p.SetPolys(cells)
    if uv is not None:
        t = numpy_to_vtk(np.ascontiguousarray(uv, dtype=np.float32), deep=True); t.SetName("TCoords"); p.GetPointData().SetTCoords(t)
        return p
    n = vtk.vtkPolyDataNormals(); n.SetInputData(p); n.SetFeatureAngle(32); n.SplittingOn(); n.ConsistencyOn(); n.Update()
    return n.GetOutput()

def ator(mesh, cor, rug=0.6, met=0.0, R=None):
    m = mesh.copy()
    if R is not None: m.apply_transform(R)
    mp = vtk.vtkPolyDataMapper(); mp.SetInputData(poly(m))
    a = vtk.vtkActor(); a.SetMapper(mp); pr = a.GetProperty()
    pr.SetInterpolationToPBR(); pr.SetColor(*cor); pr.SetRoughness(rug); pr.SetMetallic(met)
    return a

def ambiente():
    """Mapa equiretangular sintético: estúdio escuro com softbox branca e tiras neon (reflexos nos plásticos)."""
    w, h = 1024, 512
    u = np.linspace(0, 1, w)[None, :]; v = np.linspace(0, 1, h)[:, None]
    img = np.zeros((h, w, 3)) + (0.015 + 0.03 * (1 - v))[..., None] * np.array([0.6, 0.7, 1.0])
    def faixa(u0, u1, v0, v1, cor, k):
        m = ((u > u0) & (u < u1) & (v > v0) & (v < v1)).astype(float)
        img[...] += m[..., None] * np.array(cor) * k
    faixa(0.40, 0.60, 0.05, 0.22, (1, 0.97, 0.92), 1.0)     # softbox no alto
    faixa(0.10, 0.16, 0.25, 0.60, (1, 0.1, 0.6), 0.9)       # tira magenta
    faixa(0.82, 0.88, 0.25, 0.60, (0, 0.8, 1), 0.9)         # tira ciano
    img = np.clip(img, 0, 1) ** (1 / 2.2)
    a = (img * 255).astype(np.uint8)[::-1].copy()
    vi = vtk.vtkImageData(); vi.SetDimensions(w, h, 1)
    arr = numpy_to_vtk(a.reshape(-1, 3), deep=True); arr.SetNumberOfComponents(3); vi.GetPointData().SetScalars(arr)
    t = vtk.vtkTexture(); t.SetInputData(vi); t.SetColorModeToDirectScalars(); t.MipmapOn(); t.InterpolateOn(); t.SetUseSRGBColorSpace(True)
    return t

def tex_pil(im):
    a = np.asarray(im.convert("RGB"))[::-1].copy()
    vi = vtk.vtkImageData(); vi.SetDimensions(a.shape[1], a.shape[0], 1)
    arr = numpy_to_vtk(a.reshape(-1, 3), deep=True); arr.SetNumberOfComponents(3); vi.GetPointData().SetScalars(arr)
    t = vtk.vtkTexture(); t.SetInputData(vi); t.InterpolateOn(); t.MipmapOn(); t.SetColorModeToDirectScalars()
    return t

def luz(pos, foco, cor, inten, cone=45, sombra=True):
    L = vtk.vtkLight(); L.SetLightTypeToSceneLight(); L.SetPositional(True)
    L.SetPosition(*pos); L.SetFocalPoint(*foco); L.SetColor(*cor); L.SetIntensity(inten); L.SetConeAngle(cone)
    L.SetShadowAttenuation(1.0 if sombra else 0.0)
    return L

def bloom(a):
    x = a.astype(np.float32) / 255
    lum = x.max(-1, keepdims=True)
    br = np.clip(x - 0.72, 0, 1) * (lum > 0.72)
    im = Image.fromarray((np.clip(br * 3, 0, 1) * 255).astype(np.uint8))
    g1 = np.asarray(im.filter(ImageFilter.GaussianBlur(6))).astype(np.float32) / 255
    g2 = np.asarray(im.filter(ImageFilter.GaussianBlur(28))).astype(np.float32) / 255
    x = x + 0.35 * g1 + 0.45 * g2
    hh, ww = x.shape[:2]; yy, xx = np.mgrid[0:hh, 0:ww]
    r = np.sqrt(((xx - ww / 2) / (ww / 2)) ** 2 + ((yy - hh / 2) / (hh / 2)) ** 2)
    x *= (1 - 0.35 * np.clip(r - 0.45, 0, 1))[..., None]                    # vinheta
    return (np.clip(x, 0, 1) * 255).astype(np.uint8)

def cena(A, cap, cp, pal, vista, path):
    R0 = T_tampa(TAMPA_GRAUS)
    lid = {k: A[k].copy().apply_transform(R0) for k in TAMPA if k in A}
    ant = antena([lid[k] for k in ("CaseD1", "CaseC2_z3ck031", "CaseD1R", "TFT_pcb", "TFT_sd") if k in lid])
    base_keys = ["CaseS1L", "CaseL1L", "CaseR1L", "CaseB1_z3ck", "CaseK1", "CaseK2", "CaseT1", "CaseT1_2"]
    peso = [(cp, 0.55), (cap, 0.25), (A["CaseS1L"], 0.09), (A["CaseB1_z3ck"], 0.12)] + [(m, 0.05) for m in lid.values()] \
           + [(A[k], 0.01) for k in base_keys[1:3] + base_keys[4:]] + [(ant["borracha"], 0.02)]
    R = pose_apoio(peso)

    ren = vtk.vtkRenderer()
    ren.SetBackground(*srgb("#07080d")); ren.SetBackground2(*srgb("#141826")); ren.GradientBackgroundOn()
    ren.UseImageBasedLightingOn(); ren.SetEnvironmentTexture(ambiente(), True)
    ren.SetEnvironmentUp(0, 1, 0); ren.SetEnvironmentRight(1, 0, 0)

    for b in cp.split(only_watertight=False):
        ren.AddActor(ator(b, BRANCO_ADV, 0.42, R=R))
    for b in cap.split(only_watertight=False):
        ren.AddActor(ator(b, CINZA_CAP, 0.5, R=R))
    for k in base_keys:
        ren.AddActor(ator(A[k], pal[PAPEL[k]], 0.62, R=R))
    for k, m in lid.items():
        if k in PAPEL: ren.AddActor(ator(m, pal[PAPEL[k]], 0.62, R=R))
        elif k == "Frame__1_": ren.AddActor(ator(m, srgb("#d9d9d9"), 0.3, 0.6, R=R))
        elif k == "TFT_pcb": ren.AddActor(ator(m, srgb("#c8141c"), 0.35, R=R))
        elif k == "TFT_sd": ren.AddActor(ator(m, srgb("#d5d8dd"), 0.45, 0.35, R=R))
        elif k == "LOGO_fundo": ren.AddActor(ator(m, pal.get("logo", pal["detalhe"]), 0.6, R=R))
        else: ren.AddActor(ator(m, srgb("#0c3d2a") if k == "PCB" else srgb("#101010"), 0.5, R=R))
    ren.AddActor(ator(ant["metal"], srgb("#d4af6a"), 0.25, 1.0, R=R))
    ren.AddActor(ator(ant["borracha"], srgb("#151517"), 0.55, R=R))

    # tela (emissiva: sem iluminação)
    V, UV, asp = quad_tela(A["Glass"], TAMPA_GRAUS) if "Glass" in A else quad_aa(TAMPA_GRAUS)
    V = trimesh.transformations.transform_points(V, R)
    q = poly(V, UV, faces=np.array([[0, 1, 2], [0, 2, 3]]))
    mp = vtk.vtkPolyDataMapper(); mp.SetInputData(q)
    nq = vtk.vtkPolyDataNormals(); nq.SetInputData(q); nq.Update(); mp.SetInputData(nq.GetOutput())
    tela = vtk.vtkActor(); tela.SetMapper(mp)
    et = tex_pil(textura_ui(asp, pal["acento"], pal["detalhe"])); et.SetUseSRGBColorSpace(True)
    tp = tela.GetProperty(); tp.SetInterpolationToPBR(); tp.SetColor(0.01, 0.01, 0.01); tp.SetRoughness(0.12)
    tp.SetEmissiveTexture(et); tp.SetEmissiveFactor(1.25, 1.25, 1.25)
    ren.AddActor(tela)

    # mesa
    allv = np.vstack([trimesh.transformations.transform_points(A[k].vertices, R) for k in base_keys])
    c = (allv.min(0) + allv.max(0)) / 2
    pl = vtk.vtkPlaneSource(); pl.SetOrigin(c[0] - 1500, 0, c[2] - 1500); pl.SetPoint1(c[0] + 1500, 0, c[2] - 1500); pl.SetPoint2(c[0] - 1500, 0, c[2] + 1500)
    pl.SetXResolution(1); pl.SetYResolution(1)
    pm = vtk.vtkPolyDataMapper(); pm.SetInputConnection(pl.GetOutputPort())
    mesa = vtk.vtkActor(); mesa.SetMapper(pm); mp_ = mesa.GetProperty()
    mp_.SetInterpolationToPBR(); mp_.SetColor(*srgb("#0a0b0f")); mp_.SetRoughness(0.62); mp_.SetMetallic(0.0)
    ren.AddActor(mesa)

    # luzes: chave quente + contraluzes neon + brilho da tela sobre o teclado
    ren.AutomaticLightCreationOff()
    ren.AddLight(luz(c + [160, 380, 260], c, (1.0, 0.95, 0.88), 0.85, 60))
    ren.AddLight(luz(c + [-330, 260, -260], c, pal["luz_a"], 1.1, 60))
    ren.AddLight(luz(c + [330, 220, -280], c, pal["luz_b"], 0.9, 60))
    ren.AddLight(luz(c + [0, 60, 300], c, (0.6, 0.7, 1.0), 0.25, 40, sombra=False))

    # passes: sombras -> SSAO -> tone mapping
    seq = vtk.vtkSequencePass(); passes = vtk.vtkRenderPassCollection()
    sh = vtk.vtkShadowMapPass(); sh.GetShadowMapBakerPass().SetResolution(4096)
    passes.AddItem(sh.GetShadowMapBakerPass()); passes.AddItem(sh)
    ov = vtk.vtkOverlayPass(); passes.AddItem(ov)
    seq.SetPasses(passes); camp = vtk.vtkCameraPass(); camp.SetDelegatePass(seq)
    ssao = vtk.vtkSSAOPass(); ssao.SetDelegatePass(camp); ssao.SetRadius(6.0); ssao.SetBias(0.05); ssao.SetKernelSize(128); ssao.BlurOn()
    tm = vtk.vtkToneMappingPass(); tm.SetDelegatePass(ssao); tm.SetToneMappingType(vtk.vtkToneMappingPass.GenericFilmic)
    tm.SetGenericFilmicDefaultPresets(); tm.SetExposure(1.35)
    ren.SetPass(tm)

    cam = ren.GetActiveCamera()
    dvec, foco_off, dist, ang = vista
    dvec = np.array(dvec, float); dvec /= np.linalg.norm(dvec)
    f = c + np.array(foco_off)
    cam.SetFocalPoint(*f); cam.SetPosition(*(f + dvec * dist)); cam.SetViewUp(0, 1, 0); cam.SetViewAngle(ang)
    ren.ResetCameraClippingRange()

    rw = vtk.vtkRenderWindow(); rw.SetOffScreenRendering(1); rw.AddRenderer(ren); rw.SetSize(W * SS, H * SS)
    rw.SetMultiSamples(0); rw.Render()
    w2i = vtk.vtkWindowToImageFilter(); w2i.SetInput(rw); w2i.ReadFrontBufferOff(); w2i.Update()
    im = w2i.GetOutput(); ww, hh, _ = im.GetDimensions()
    arr = np.frombuffer(im.GetPointData().GetScalars(), dtype=np.uint8).reshape(hh, ww, -1)[::-1, :, :3]
    img = Image.fromarray(arr).resize((W, H), Image.LANCZOS)
    img = Image.fromarray(bloom(np.asarray(img)))
    d = ImageDraw.Draw(img)
    d.text((28, H - 46), f"z3ck 031  ·  {pal['nome']}", font=ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", 24), fill=(200, 205, 215))
    img.save(path); print("ok", path)

VISTAS = {
    "frente": ((0.55, 0.6, 1.0), (8, 30, -10), 330, 30),
    "tras":   ((-0.32, 0.5, -1.0), (2, 26, -16), 270, 30),
}

if __name__ == "__main__":
    A, cap, cp = carregar()
    pals = sys.argv[2].split(",") if len(sys.argv) > 2 else list(PALETAS)
    vistas = sys.argv[3].split(",") if len(sys.argv) > 3 else list(VISTAS)
    for p in pals:
        for v in vistas:
            pref = "foto_vermelha_fechada" if TIPO == "TypeR" else "foto"
            cena(A, cap, cp, PALETAS[p], VISTAS[v], os.path.join(OUT, f"{pref}_{p}_{v}.png"))
