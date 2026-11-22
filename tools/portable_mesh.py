# SPDX-License-Identifier: GPL-3.0-only
"""Bake Fallout's closed trap pose for transport. Input: two extracted vanilla NIFs."""
import math
import sys
import time
from pathlib import Path
if not hasattr(time, "clock"):
    time.clock = time.perf_counter
from pyffi.formats.nif import NifFormat as N


def read(path):
    data = N.Data()
    with path.open("rb") as f:
        data.read(f)
    return data


def portable(trap, clutter, out):
    scene = read(trap)
    root = scene.roots[0]
    seq = next(b for b in scene.blocks if isinstance(b, N.NiControllerSequence) and b.name == b"Forward")
    for track in seq.controlled_blocks:
        node = root.find(block_name=track.get_node_name())
        data = track.interpolator.data
        if data.translations.keys:
            node.translation = data.translations.keys[-1].value
        # Each vanilla track moves about a single local axis.
        angles = [keys.keys[-1].value for keys in data.xyz_rotations]
        nonzero = [i for i, a in enumerate(angles) if abs(math.sin(a / 2)) > 1e-5]
        assert len(nonzero) <= 1
        rot = N.Matrix33()
        rot.set_identity()
        if nonzero:
            i = nonzero[0]
            j, k = (i + 1) % 3 + 1, (i + 2) % 3 + 1
            cosine, sine = math.cos(angles[i]), math.sin(angles[i])
            setattr(rot, f'm_{j}{j}', cosine)
            setattr(rot, f'm_{k}{k}', cosine)
            setattr(rot, f'm_{j}{k}', sine)
            setattr(rot, f'm_{k}{j}', -sine)
        node.rotation = rot
    root.controller = None
    for obj in list(root.tree()):
        if isinstance(obj, N.NiAVObject):
            obj.collision_object = None
    visual = root.children[0]
    visual.remove_child(root.find(block_name=b'Trigger'))
    visual.name = b'TrapCarryMesh'
    root.set_extra_datas([])
    root.name = b'TTWVRPortableTrap'
    out.mkdir(parents=True, exist_ok=True)
    # Carry by the end of the frame. The entire folded tool extends from the palm.
    visual.translation.x = 39
    with (out/'Held.nif').open('wb') as f:
        scene.write(f)
    visual.translation.x = 0
    # One movable root body. All rendered parts are rigid children of this body.
    donor = read(clutter).roots[0]
    root.set_extra_datas([next(e for e in donor.extra_data_list if isinstance(e, N.BSXFlags))])
    collision = donor.collision_object
    root.collision_object = collision
    collision.target = root
    body = collision.body
    shape = N.bhkBoxShape()
    shape.material.material = 5  # HAV_MAT_METAL
    shape.radius = .04
    shape.dimensions.x, shape.dimensions.y, shape.dimensions.z = 6.9, 1.1, .40
    body.shape = shape
    body.translation.x = body.translation.y = body.translation.z = 0
    body.rotation.x = body.rotation.y = body.rotation.z = 0
    body.rotation.w = 1
    body.update_mass_center_inertia(mass=10)
    with (out/'Portable.nif').open('wb') as f:
        scene.write(f)
    for name, count in [('Portable',1),('Held',0)]:
        saved = read(out/(name+'.nif'))
        assert sum(isinstance(b,N.bhkWorldObject) for b in saved.blocks) == count
        assert not any(isinstance(b,N.NiControllerManager) for b in saved.blocks)
        assert saved.roots[0].find(block_name=b'TrapCarryMesh')


if __name__ == '__main__':
    portable(*(Path(p) for p in sys.argv[1:]))
