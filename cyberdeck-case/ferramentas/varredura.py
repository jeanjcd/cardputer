"""Varredura da tampa em torno do eixo da dobradiça (X, Y=16.88, Z=-5.07). Ângulo 135° = posição do STEP."""
import numpy as np, trimesh
PIVOT = np.array([0, 16.88, -5.07])
LID = ["CaseD1", "CaseD2", "CaseD3", "CaseC2", "CaseC3", "PCB", "Frame__1_", "SOLID", "Glass"]
def lid_transform(angle_deg, step_angle=135.0):
    """Matriz que leva a tampa da posição do STEP (135°) para 'angle_deg' de abertura (0 = fechada)."""
    d = np.radians(step_angle - angle_deg)
    return trimesh.transformations.rotation_matrix(d, [1, 0, 0], PIVOT)
