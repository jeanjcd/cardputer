import sys, glob, trimesh, numpy as np
np.set_printoptions(precision=2, suppress=True)
for pat in sys.argv[1:]:
    for f in sorted(glob.glob(pat)):
        m = trimesh.load(f, force='mesh')
        b = m.bounds
        print(f"{f.split('/')[-1]:28s} faces={len(m.faces):7d} wt={m.is_watertight!s:5s} "
              f"min={b[0]} max={b[1]} size={b[1]-b[0]} vol={m.volume if m.is_watertight else float('nan'):.0f} bodies={len(m.split(only_watertight=False))}")
