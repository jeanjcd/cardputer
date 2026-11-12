"""Carrega a cena combinada: peças TERM512 (do STEP) + Cardputer-Adv + Cap LoRa-1262 oficiais, no referencial TERM."""
import sys, os, glob, trimesh
sys.path.insert(0, os.path.dirname(__file__))
from referencial import assembled
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def term_parts(asm_dir):
    out = {}
    for f in glob.glob(os.path.join(asm_dir, "*.stl")):
        out[os.path.splitext(os.path.basename(f))[0]] = trimesh.load(f)
    return out
def cardputer(): return assembled(os.path.join(ROOT, "referencias_m5stack", "Cardputer-Adv.stl"))
def cap():       return assembled(os.path.join(ROOT, "referencias_m5stack", "Cap_LoRa-1262.stl"))
