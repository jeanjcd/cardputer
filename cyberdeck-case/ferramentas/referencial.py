"""Transformação do referencial dos STLs oficiais M5Stack para o referencial de montagem do TERM512.
M5:  X = profundidade (-27 borda superior/EXT .. +27), Y = largura (-42..42), Z = espessura (-9 traseira .. +10.6 teclado)
TERM: X = largura (0..84), Y = espessura (-0.23 traseira .. 19.47 teclado), Z = profundidade (0 borda superior .. 54)
"""
import numpy as np, trimesh
M5_TO_TERM = np.array([[0,1,0,42.0],
                       [0,0,1,8.77],
                       [1,0,0,26.93],
                       [0,0,0,1]], float)
def m5(path):
    m = trimesh.load(path, force='mesh')
    bodies = m.split(only_watertight=False)
    return bodies
def assembled(path):
    """Somente os corpos na posição montada (|Y_m| < 50)."""
    bodies = [b for b in m5(path) if b.bounds[1][1] < 50 and b.bounds[0][1] > -50 and b.bounds[1][2] < 12 and b.bounds[0][2] > -25]
    out = trimesh.util.concatenate(bodies); out.apply_transform(M5_TO_TERM)
    return out

def envelope(mesh, pitch=0.3):
    """Envelope sólido (preenche cavidades internas fechadas) via voxelização + marching cubes."""
    from scipy import ndimage
    vg = mesh.voxelized(pitch, max_iter=30)
    mat = vg.matrix
    filled = ndimage.binary_fill_holes(mat)
    # fecha pequenas frestas (aberturas de conectores) antes de preencher
    closed = ndimage.binary_closing(np.pad(filled, 2), iterations=2)[2:-2, 2:-2, 2:-2]
    filled = ndimage.binary_fill_holes(closed | filled)
    from trimesh.voxel import VoxelGrid
    out = VoxelGrid(filled).marching_cubes
    out.apply_transform(vg.transform)
    return out
