"""Deterministic, engine-owned L0 metallic (rows) / roughness (columns) fixture."""
import json
import math
import struct
from pathlib import Path

folder = Path(__file__).resolve().parent.parent / 'content' / 'samples'
positions, indices = [], []
slices, stacks = 64, 32
for y in range(stacks + 1):
    for x in range(slices + 1):
        phi, theta = y / stacks * math.pi, x / slices * math.tau
        positions.append((math.sin(phi) * math.cos(theta), math.cos(phi), math.sin(phi) * math.sin(theta)))
for y in range(stacks):
    for x in range(slices):
        a = y * (slices + 1) + x
        b = a + slices + 1
        indices.extend([a, a + 1, b, a + 1, b + 1, b])
vertices = b''.join(struct.pack('<3f', *p) for p in positions)
binary = vertices + b''.join(struct.pack('<I', i) for i in indices)
scene = {
    'asset': {'version': '2.0', 'generator': 'Engine L0 lighting grid'},
    'scene': 0, 'scenes': [{'nodes': list(range(49))}],
    'buffers': [{'uri': 'lighting_grid.bin', 'byteLength': len(binary)}],
    'bufferViews': [{'buffer': 0, 'byteLength': len(vertices)},
                    {'buffer': 0, 'byteOffset': len(vertices), 'byteLength': len(binary) - len(vertices)}],
    'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': len(positions), 'type': 'VEC3', 'min': [-1, -1, -1], 'max': [1, 1, 1]},
                  {'bufferView': 1, 'componentType': 5125, 'count': len(indices), 'type': 'SCALAR'}],
    'nodes': [], 'meshes': [], 'materials': []
}
for row in range(7):
    for col in range(7):
        i = row * 7 + col
        name = f'Metallic {row / 6:.3f}, roughness {max(.045, col / 6):.3f}'
        scene['materials'].append({'name': name, 'pbrMetallicRoughness': {
            'baseColorFactor': [.8, .32, .12, 1], 'metallicFactor': row / 6, 'roughnessFactor': max(.045, col / 6)}})
        scene['meshes'].append({'name': name, 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 0}, 'indices': 1, 'material': i}]})
        scene['nodes'].append({'name': name, 'mesh': i, 'translation': [(col - 3) * 2.5, row * 2.5 + 1, 0]})
folder.mkdir(parents=True, exist_ok=True)
(folder / 'lighting_grid.bin').write_bytes(binary)
(folder / 'lighting_grid.gltf').write_text(json.dumps(scene, indent=2))
print('Prepared', folder / 'lighting_grid.gltf')
