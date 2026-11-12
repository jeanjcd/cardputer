"""Exporta o kit montado (Cardputer-Adv + Cap LoRa + case + TFT) como modelo 3D colorido (.glb).

Uso:  python -I -X utf8 ferramentas/exportar_montagem.py [TypeR|TypeA|TypeB] [paleta]
Saída: versao_z3ck031/modelo_3d/z3ck031_<tipo>_aberto.glb e _fechado.glb
       (abre no Visualizador 3D do Windows, Blender, https://gltf-viewer.donmccurdy.com etc.)

Mesmas peças e posições dos testes (pasta montagem/<tipo>), tampa girada no eixo real da dobradiça.
Cardputer, Cap e antena são os modelos de referência (M5Stack); a TFT é o envelope usado nos testes.
"""
import os, sys
import numpy as np
import trimesh
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gerar_z3ck031 as G
import render_foto as RF

TIPO = sys.argv[1] if len(sys.argv) > 1 else "TypeR"
PAL = RF.PALETAS[sys.argv[2] if len(sys.argv) > 2 else "magenta"]
OUT = os.path.join(G.OUT, "modelo_3d")

def material(rgb, rug=0.6, met=0.0, emis=None):
    return trimesh.visual.material.PBRMaterial(
        baseColorFactor=[int(255 * c) for c in rgb] + [255], roughnessFactor=rug, metallicFactor=met,
        emissiveFactor=emis)

def com_cor(m, rgb, rug=0.6, met=0.0):
    m = m.copy()
    m.visual = trimesh.visual.TextureVisuals(material=material(rgb, rug, met))
    return m

def tela_texturizada(ang):
    """Quad da área ativa com a interface fictícia (textura emissiva)."""
    if TIPO == "TypeR":
        V, UV, asp = RF.quad_aa(ang)
    else:
        A = trimesh.load(os.path.join(G.OUT, "montagem", TIPO, "Glass.stl"))
        V, UV, asp = RF.quad_tela(A, ang)
    img = RF.textura_ui(asp, PAL["acento"], PAL["detalhe"])
    q = trimesh.Trimesh(V, [[0, 1, 2], [0, 2, 3]], process=False)
    mat = trimesh.visual.material.PBRMaterial(baseColorTexture=img, emissiveTexture=img, emissiveFactor=[1, 1, 1],
                                              roughnessFactor=0.15, metallicFactor=0.0, doubleSided=True)
    q.visual = trimesh.visual.TextureVisuals(uv=UV, material=mat)
    return q

def parafusos(Tl):
    """2 parafusos M2x4 cabeça cilíndrica (TypeR), na posição real."""
    import gerar_tampa_vermelha as RV
    out = []
    Ts = np.linalg.inv(RV.T_fechar())
    for (x, z) in RV.FUROS:
        cab = trimesh.creation.cylinder(radius=1.9, height=2.0, sections=24)
        cab.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0])); cab.apply_translation([x, RV.REBAIXO[1] - 1.0, z])
        has = trimesh.creation.cylinder(radius=1.0, height=RV.PARAF_L, sections=16)
        has.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0])); has.apply_translation([x, RV.REBAIXO[1] + RV.PARAF_L / 2, z])
        p = trimesh.util.concatenate([cab, has]); p.apply_transform(Ts); p.apply_transform(Tl)
        out.append(p)
    return out

def cena(ang):
    d = os.path.join(G.OUT, "montagem", TIPO)
    A, cap, cp = RF.carregar() if RF.TIPO == TIPO else (None, None, None)
    Tl = RF.T_tampa(ang)
    S = trimesh.Scene()
    add = lambda nome, m: S.add_geometry(m, node_name=nome, geom_name=nome)
    for i, b in enumerate(cp.split(only_watertight=False)):
        add(f"Cardputer_ADV_{i}", com_cor(b, RF.BRANCO_ADV, 0.45))
    for i, b in enumerate(cap.split(only_watertight=False)):
        add(f"Cap_LoRa1262_{i}", com_cor(b, RF.CINZA_CAP, 0.5))
    for k in ["CaseS1L", "CaseL1L", "CaseR1L", "CaseB1_z3ck", "CaseK1", "CaseK2", "CaseT1", "CaseT1_2"]:
        add(k, com_cor(A[k], PAL[RF.PAPEL[k]], 0.6))
    lid_meshes = []
    for k in RF.TAMPA:
        if k not in A or k == "TFT_sd":
            continue
        m = A[k].copy(); m.apply_transform(Tl); lid_meshes.append(m)
        if k in RF.PAPEL:
            add(k, com_cor(m, PAL[RF.PAPEL[k]], 0.6))
        elif k == "LOGO_fundo":
            add("Logo_z3ck031_cor2", com_cor(m, PAL.get("logo", PAL["detalhe"]), 0.6))
        elif k == "TFT_pcb":
            add("TFT_PCB_vermelha", com_cor(m, RF.srgb("#c8141c"), 0.35))
        elif k == "Frame__1_":
            add("TFT_moldura", com_cor(m, RF.srgb("#d9d9d9"), 0.3, 0.6))
        else:
            add(f"TFT_{k}", com_cor(m, RF.srgb("#0c3d2a") if k == "PCB" else RF.srgb("#101010"), 0.5))
    add("TFT_imagem", tela_texturizada(ang))
    if TIPO == "TypeR":
        for i, p in enumerate(parafusos(Tl)):
            add(f"Parafuso_M2x4_{i + 1}", com_cor(p, RF.srgb("#2a2a2e"), 0.35, 1.0))
    ant = RF.antena([m for m in lid_meshes])
    add("Antena_SMA", com_cor(ant["metal"], RF.srgb("#d4af6a"), 0.25, 1.0))
    add("Antena", com_cor(ant["borracha"], RF.srgb("#151517"), 0.55))
    S.apply_scale(0.001)          # glTF usa metros
    return S

if __name__ == "__main__":
    RF.TIPO = TIPO
    os.makedirs(OUT, exist_ok=True)
    for nome, ang in [("aberto", 112), ("fechado", 0)]:
        S = cena(ang)
        p = os.path.join(OUT, f"z3ck031_{TIPO}_{nome}.glb")
        S.export(p)
        print("ok", p, "%.1f MB" % (os.path.getsize(p) / 1e6), len(S.geometry), "peças")
