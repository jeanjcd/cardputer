"""Lista os sólidos (com nomes) de um STEP de montagem e exporta cada um como STL na posição montada."""
import sys, os
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.XCAFDoc import XCAFDoc_DocumentTool
from OCP.TDF import TDF_LabelSequence
from OCP.TDataStd import TDataStd_Name
from OCP.TopLoc import TopLoc_Location
import cadquery as cq

def name_of(label):
    attr = TDataStd_Name()
    if label.FindAttribute(TDataStd_Name.GetID_s(), attr):
        return attr.Get().ToExtString()
    return "?"

def walk(st, label, loc, out, depth=0):
    if st.IsReference_s(label):
        ref = label.__class__()
        st.GetReferredShape_s(label, ref)
        loc = loc.Multiplied(st.GetLocation_s(label))
        nm = name_of(label)
        walk(st, ref, loc, out, depth)
        return
    if st.IsAssembly_s(label):
        comps = TDF_LabelSequence()
        st.GetComponents_s(label, comps)
        for i in range(1, comps.Length()+1):
            walk(st, comps.Value(i), loc, out, depth+1)
    else:
        shp = st.GetShape_s(label).Moved(loc)
        out.append((name_of(label), shp))

def read(path):
    doc = TDocStd_Document(TCollection_ExtendedString("doc"))
    r = STEPCAFControl_Reader(); r.SetNameMode(True)
    r.ReadFile(path); r.Transfer(doc)
    st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    roots = TDF_LabelSequence(); st.GetFreeShapes(roots)
    out = []
    for i in range(1, roots.Length()+1):
        walk(st, roots.Value(i), TopLoc_Location(), out)
    return out

if __name__ == "__main__":
    path = sys.argv[1]; outdir = sys.argv[2] if len(sys.argv) > 2 else None
    parts = read(path)
    seen = {}
    for nm, shp in parts:
        seen[nm] = seen.get(nm, 0) + 1
        if seen[nm] > 1: nm = f"{nm}_{seen[nm]}" 
        s = cq.Shape.cast(shp); bb = s.BoundingBox()
        print(f"{nm:30s} min=({bb.xmin:7.2f},{bb.ymin:7.2f},{bb.zmin:7.2f}) max=({bb.xmax:7.2f},{bb.ymax:7.2f},{bb.zmax:7.2f}) size=({bb.xlen:6.2f},{bb.ylen:6.2f},{bb.zlen:6.2f}) solids={len(s.Solids())}")
        if outdir:
            os.makedirs(outdir, exist_ok=True)
            safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in nm)
            cq.exporters.export(cq.Workplane().add(s), os.path.join(outdir, safe + ".stl"), tolerance=0.02, angularTolerance=0.1)
