"""Render simples (matplotlib) de malhas com sombreamento, para revisão visual."""
import numpy as np, trimesh, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

def _shade(mesh, rgb, light):
    n = mesh.face_normals
    k = np.clip(n @ light, 0, 1) * 0.65 + 0.35
    c = np.array(rgb)[None, :] * k[:, None]
    return np.clip(c, 0, 1)

def render(items, path, views=((25, -60),), titles=None, size=(7, 6), maxfaces=60000, zoom=1.0, center=None, span=None):
    """items: lista de (mesh, (r,g,b)) ou (mesh,(r,g,b),alpha)."""
    fig = plt.figure(figsize=(size[0]*len(views), size[1]))
    allv = np.vstack([it[0].vertices for it in items])
    lo, hi = allv.min(0), allv.max(0)
    c = (lo+hi)/2 if center is None else np.array(center)
    r = (hi-lo).max()/2/zoom if span is None else span/2
    for i, (el, az) in enumerate(views):
        ax = fig.add_subplot(1, len(views), i+1, projection="3d")
        ax.set_proj_type("ortho")
        light = np.array([0.4, -0.5, 0.75]); light /= np.linalg.norm(light)
        for it in items:
            m, rgb = it[0], it[1]; a = it[2] if len(it) > 2 else 1.0
            if len(m.faces) > maxfaces:
                m = m.simplify_quadric_decimation(face_count=maxfaces) if hasattr(m, "simplify_quadric_decimation") else m
            # mapeia TERM (X larg, Y esp, Z prof) para eixos de plot (x, -z, y) => "de pé"
            V = m.vertices[:, [0, 2, 1]] * np.array([1, -1, 1])
            pc = Poly3DCollection(V[m.faces], facecolors=_shade(m, rgb, light), edgecolors="none", alpha=a)
            ax.add_collection3d(pc)
        cc = c[[0, 2, 1]] * np.array([1, -1, 1])
        ax.set_xlim(cc[0]-r, cc[0]+r); ax.set_ylim(cc[1]-r, cc[1]+r); ax.set_zlim(cc[2]-r, cc[2]+r)
        ax.view_init(elev=el, azim=az); ax.set_box_aspect((1, 1, 1))
        ax.set_xlabel("X largura"); ax.set_ylabel("-Z (borda sup.)"); ax.set_zlabel("Y (teclado)")
        if titles: ax.set_title(titles[i])
    plt.tight_layout(); plt.savefig(path, dpi=110); plt.close(fig)
