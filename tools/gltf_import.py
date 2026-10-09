"""Small standard-library glTF metadata reader for offline asset cataloguing."""
import itertools
import json
import math
import struct
from pathlib import Path


def read_gltf(path):
    path = Path(path)
    if path.suffix.lower() != '.glb':
        return json.loads(path.read_text()), None
    raw = path.read_bytes()
    magic, version, length = struct.unpack_from('<III', raw)
    if magic != 0x46546C67 or version != 2 or length != len(raw):
        raise ValueError(f'Invalid GLB header: {path}')
    offset, document, binary = 12, None, None
    while offset < length:
        size, kind = struct.unpack_from('<II', raw, offset)
        offset += 8
        if offset + size > length:
            raise ValueError(f'Truncated GLB chunk: {path}')
        chunk = raw[offset:offset + size]
        if kind == 0x4E4F534A:
            document = json.loads(chunk)
        elif kind == 0x004E4942:
            binary = chunk
        offset += size
    if document is None:
        raise ValueError(f'Missing GLB JSON: {path}')
    return document, binary


def multiply(a, b):
    return [[sum(a[row][k] * b[k][column] for k in range(4))
             for column in range(4)] for row in range(4)]


def node_matrix(node):
    if 'matrix' in node:
        return [[node['matrix'][column * 4 + row] for column in range(4)] for row in range(4)]
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = node.get('scale', [1, 1, 1])
    tx, ty, tz = node.get('translation', [0, 0, 0])
    return [[(1-2*y*y-2*z*z)*sx, (2*x*y-2*z*w)*sy, (2*x*z+2*y*w)*sz, tx],
            [(2*x*y+2*z*w)*sx, (1-2*x*x-2*z*z)*sy, (2*y*z-2*x*w)*sz, ty],
            [(2*x*z-2*y*w)*sx, (2*y*z+2*x*w)*sy, (1-2*x*x-2*y*y)*sz, tz],
            [0, 0, 0, 1]]


def scene_bounds(document):
    low, high = [math.inf] * 3, [-math.inf] * 3
    identity = [[int(row == column) for column in range(4)] for row in range(4)]
    nodes = document.get('nodes', [])

    def visit(index, parent, ancestors):
        if index in ancestors:
            raise ValueError('Cyclic glTF nodes')
        node = nodes[index]
        matrix = multiply(parent, node_matrix(node))
        if 'mesh' in node:
            for primitive in document['meshes'][node['mesh']]['primitives']:
                accessor = document['accessors'][primitive['attributes']['POSITION']]
                for corner in itertools.product(*zip(accessor['min'], accessor['max'])):
                    for axis in range(3):
                        value = sum(matrix[axis][k] * corner[k] for k in range(3)) + matrix[axis][3]
                        low[axis] = min(low[axis], value)
                        high[axis] = max(high[axis], value)
        for child in node.get('children', []):
            visit(child, matrix, ancestors | {index})

    scenes = document.get('scenes', [])
    roots = scenes[document.get('scene', 0)]['nodes'] if scenes else range(len(nodes))
    for index in roots:
        visit(index, identity, set())
    if not all(math.isfinite(value) for value in low + high):
        raise ValueError('No finite mesh bounds')
    return low, high
