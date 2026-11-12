"""Cortes 2D (plano Y-Z, X fixo) de várias malhas sobrepostas."""
import numpy as np, trimesh, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import PathPatch
from matplotlib.path import Path as MPath

def section_polys(mesh, x):
    s = mesh.section(plane_origin=[x, 0, 0], plane_normal=[1, 0, 0])
    if s is None: return []
    # projeta em (Z, Y): eixo horizontal = Z (profundidade, borda sup. à esquerda), vertical = Y (espessura)
    out = []
    for e in s.discrete:
        out.append(e[:, [2, 1]])
    return out

def plot_sections(items, xs, path, xlim=None, ylim=None, ncols=None):
    ncols = ncols or len(xs)
    nrows = int(np.ceil(len(xs) / ncols))
    fig, axs = plt.subplots(nrows, ncols, figsize=(6.5*ncols, 4.8*nrows), squeeze=False)
    for k, x in enumerate(xs):
        ax = axs[k // ncols][k % ncols]
        for it in items:
            name, m, col = it[0], it[1], it[2]
            polys = section_polys(m, x)
            verts, codes = [], []
            for p in polys:
                verts += p.tolist() + [p[0].tolist()]
                codes += [MPath.MOVETO] + [MPath.LINETO]*(len(p)-1) + [MPath.CLOSEPOLY]
            if verts:
                ax.add_patch(PathPatch(MPath(verts, codes), facecolor=col, edgecolor="k", lw=0.4, alpha=0.55, label=name))
        ax.set_aspect("equal"); ax.grid(alpha=0.3)
        if xlim: ax.set_xlim(*xlim)
        if ylim: ax.set_ylim(*ylim)
        ax.set_title(f"corte X = {x} mm"); ax.set_xlabel("Z (mm)  ← borda superior | teclado →"); ax.set_ylabel("Y (mm)  traseira ↓ | face ↑")
        if k == 0: ax.legend(fontsize=7, loc="lower right")
    plt.tight_layout(); plt.savefig(path, dpi=100); plt.close(fig)
