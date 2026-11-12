"""Cálculo de interferência volumétrica entre malhas (manifold3d)."""
import numpy as np, trimesh, manifold3d as mf

def to_mf(m):
    m = m.copy(); m.merge_vertices()
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32), tri_verts=np.asarray(m.faces, np.uint32)))

def to_tm(M):
    g = M.to_mesh(); return trimesh.Trimesh(g.vert_properties[:, :3], g.tri_verts, process=True)

def vol(M): return M.volume()

def inter(a, b):
    """volume (mm³) da interseção a∩b; a, b = trimesh ou Manifold."""
    A = a if isinstance(a, mf.Manifold) else to_mf(a)
    B = b if isinstance(b, mf.Manifold) else to_mf(b)
    return (A ^ B).volume()

def offset_check(a, b, gap):
    """Interseção de a com b dilatado ~gap (aprox. via Minkowski com esfera não disponível: usa escala local)."""
    raise NotImplementedError
