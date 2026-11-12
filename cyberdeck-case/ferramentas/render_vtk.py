"""Render offscreen com VTK. Referencial TERM: X largura, Y espessura (face +), Z profundidade (borda sup. -)."""
import numpy as np, trimesh, vtk
from vtk.util.numpy_support import numpy_to_vtk, numpy_to_vtkIdTypeArray

def _poly(mesh):
    pts = vtk.vtkPoints(); pts.SetData(numpy_to_vtk(np.ascontiguousarray(mesh.vertices, dtype=np.float64), deep=True))
    f = np.hstack([np.full((len(mesh.faces), 1), 3), mesh.faces]).astype(np.int64).ravel()
    cells = vtk.vtkCellArray(); cells.SetCells(len(mesh.faces), numpy_to_vtkIdTypeArray(f, deep=True))
    p = vtk.vtkPolyData(); p.SetPoints(pts); p.SetPolys(cells)
    n = vtk.vtkPolyDataNormals(); n.SetInputData(p); n.SetFeatureAngle(35); n.SplittingOn(); n.Update()
    return n.GetOutput()

def render(items, path, views, size=(900, 700), titles=None, parallel=True, bg=(1, 1, 1)):
    """items: [(mesh, (r,g,b), opacity?)]; views: [(position_dir, viewup, zoom)] - direção da câmera a partir do centro."""
    allv = np.vstack([it[0].vertices for it in items]); c = (allv.min(0) + allv.max(0)) / 2
    imgs = []
    for vi, (d, up, zoom) in enumerate(views):
        ren = vtk.vtkRenderer(); ren.SetBackground(*bg)
        for it in items:
            m, col = it[0], it[1]; op = it[2] if len(it) > 2 else 1.0
            mp = vtk.vtkPolyDataMapper(); mp.SetInputData(_poly(m))
            a = vtk.vtkActor(); a.SetMapper(mp)
            pr = a.GetProperty(); pr.SetColor(*col); pr.SetOpacity(op); pr.SetSpecular(0.2); pr.SetDiffuse(0.85); pr.SetAmbient(0.18)
            ren.AddActor(a)
        cam = ren.GetActiveCamera(); d = np.array(d, float); d /= np.linalg.norm(d)
        cam.SetFocalPoint(*c); cam.SetPosition(*(c + d * 400)); cam.SetViewUp(*up)
        if parallel: cam.ParallelProjectionOn()
        ren.ResetCamera(); cam.Zoom(zoom)
        if titles:
            t = vtk.vtkTextActor(); t.SetInput(titles[vi]); t.GetTextProperty().SetFontSize(20); t.GetTextProperty().SetColor(0, 0, 0); t.SetPosition(10, 10); ren.AddActor2D(t)
        rw = vtk.vtkRenderWindow(); rw.SetOffScreenRendering(1); rw.AddRenderer(ren); rw.SetSize(*size); rw.Render()
        w2i = vtk.vtkWindowToImageFilter(); w2i.SetInput(rw); w2i.Update()
        im = w2i.GetOutput(); W, H, _ = im.GetDimensions()
        arr = np.frombuffer(im.GetPointData().GetScalars(), dtype=np.uint8).reshape(H, W, -1)[::-1]
        imgs.append(arr[..., :3])
    from PIL import Image
    Image.fromarray(np.hstack(imgs)).save(path)

# vistas padrão (direção da câmera, view-up)
ISO_FRONT = ((0.6, 0.9, 0.9), (0, 1, 0), 1.0)   # acima do teclado, do lado inferior
ISO_BACK  = ((-0.6, -0.9, -0.7), (0, 1, 0), 1.0)
TOP_EDGE  = ((0, 0.25, -1), (0, 1, 0), 1.0)
SIDE      = ((1, 0, 0), (0, 1, 0), 1.0)
FACE      = ((0, 1, 0), (0, 0, -1), 1.0)
BACK      = ((0, -1, 0), (0, 0, -1), 1.0)
