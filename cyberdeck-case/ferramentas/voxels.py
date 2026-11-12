"""Grade de voxels global para zonas de exclusão (keep-outs) com folga e varredura de montagem.

Uso típico:
    G = Grid(lo=(-4,-21,-16), hi=(88,24,9), pitch=0.15)
    occ = G.voxelize(mesh)               # sólido preenchido
    occ = G.dilate(occ, 0.3)             # folga de 0,3 mm (distância euclidiana)
    occ = G.sweep(occ, axis=1, sign=+1)  # obstáculo varrido para +Y (peça montada deslizando em +Y)
    mesh = G.to_mesh(occ)                # malha fechada (marching cubes)
"""
import numpy as np
import trimesh
from scipy import ndimage


class Grid:
    def __init__(self, lo, hi, pitch=0.15):
        self.p = float(pitch)
        self.i0 = np.floor(np.asarray(lo, float) / self.p).astype(int)     # índice inteiro global da origem
        self.i1 = np.ceil(np.asarray(hi, float) / self.p).astype(int)
        self.shape = tuple(self.i1 - self.i0 + 1)

    def empty(self):
        return np.zeros(self.shape, bool)

    def centers(self, idx):
        return (np.asarray(idx) + self.i0) * self.p

    # ---------------------------------------------------------------- voxelização
    def voxelize(self, mesh, fill=True):
        """Voxeliza superfície (subdivisão) e preenche o interior; recorta na grade."""
        out = self.empty()
        if mesh is None or len(mesh.faces) == 0:
            return out
        vg = mesh.voxelized(self.p, method="subdivide", max_iter=30)
        if fill:
            vg = vg.copy(); vg.fill(method="holes")
        mat = vg.matrix
        org = np.round(vg.transform[:3, 3] / self.p).astype(int)            # índice global do voxel (0,0,0) local
        off = org - self.i0
        self._paste(out, mat, off)
        return out

    def _paste(self, out, mat, off):
        src = [slice(0, s) for s in mat.shape]; dst = []
        for a in range(3):
            d0, d1 = off[a], off[a] + mat.shape[a]
            c0, c1 = max(d0, 0), min(d1, self.shape[a])
            if c1 <= c0:
                return
            src[a] = slice(c0 - d0, c1 - d0); dst.append(slice(c0, c1))
        out[tuple(dst)] |= mat[tuple(src)]

    def voxelize_points(self, pts, out=None):
        """Marca os voxels que contêm os pontos (usado para varredura rotacional)."""
        out = self.empty() if out is None else out
        idx = np.round(pts / self.p).astype(int) - self.i0
        ok = np.all((idx >= 0) & (idx < np.array(self.shape)), axis=1)
        idx = idx[ok]
        out[idx[:, 0], idx[:, 1], idx[:, 2]] = True
        return out

    # ---------------------------------------------------------------- operações
    def dilate(self, occ, dist):
        """Dilatação euclidiana exata (EDT) de 'dist' mm, calculada só na caixa envolvente do objeto."""
        if not occ.any():
            return occ.copy()
        m = int(np.ceil(dist / self.p)) + 1
        nz = np.argwhere(occ); a = np.maximum(nz.min(0) - m, 0); b = np.minimum(nz.max(0) + m + 1, self.shape)
        sub = occ[a[0]:b[0], a[1]:b[1], a[2]:b[2]]
        d = ndimage.distance_transform_edt(~sub, sampling=self.p)
        out = occ.copy()
        out[a[0]:b[0], a[1]:b[1], a[2]:b[2]] = d <= dist + 1e-9
        return out

    @staticmethod
    def sweep(occ, axis, sign=+1):
        """Varre o volume ao longo do eixo: sign=+1 => cobre tudo a partir do obstáculo no sentido +eixo."""
        if sign > 0:
            return np.logical_or.accumulate(occ, axis=axis)
        flip = [slice(None)] * 3; flip[axis] = slice(None, None, -1)
        return np.logical_or.accumulate(occ[tuple(flip)], axis=axis)[tuple(flip)]

    def to_mesh(self, occ):
        """Malha fechada da região ocupada (marching cubes com borda zerada)."""
        from skimage import measure
        pad = np.pad(occ, 1).astype(np.float32)
        v, f, _, _ = measure.marching_cubes(pad, level=0.5, spacing=(self.p,) * 3)
        v = v + (self.i0 - 1) * self.p
        m = trimesh.Trimesh(v, f, process=True)
        if m.volume < 0:
            m.invert()
        return m

    def volume(self, occ):
        return occ.sum() * self.p ** 3
