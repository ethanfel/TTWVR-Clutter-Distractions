# SPDX-License-Identifier: GPL-3.0-only
"""Build the optional ghoul hearing room and its ready-to-install test ZIP."""
import struct
from math import pi
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from trap_records import record, string, u32

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
CORE = "TTWVRClutterDistractions"
STAGE = "TTWVRAudioLab"
VERSION = "0.2.7"
OWN = 0x02000000  # FalloutNV.esm, the clutter plug-in, then this plug-in
CELL = OWN + 0x800
MESH = OWN + 0x801


def group(label, kind, records):
    body = b"".join(records)
    return struct.pack("<4sI4sIHHI", b"GRUP", len(body) + 24, label, kind, 0, 0, 0) + body


def navigation():
    # Leave the table and the two solid crates outside the walkable surface.
    xs = (-336, -208, -136, -24, 0, 136, 208, 336)
    ys = (-332, -210, -99, -76, 76, 210, 366, 574)
    vertices = [(x, y, 1.) for y in ys for x in xs]
    faces = []
    for row in range(len(ys) - 1):
        for col in range(len(xs) - 1):
            x, y = xs[col], ys[row]
            if y == -76 and -136 <= x < 136:
                continue
            if y == -210 and -208 <= x < -24:
                continue
            a = row * len(xs) + col
            b, c, d = a + 1, a + len(xs), a + len(xs) + 1
            faces.extend(((a, b, d), (a, d, c)))

    edges = {(a, b): i for i, face in enumerate(faces)
             for a, b in zip(face, face[1:] + face[:1])}
    triangles = []
    for face in faces:
        neighbours = [edges.get((b, a), -1) for a, b in zip(face, face[1:] + face[:1])]
        triangles.append(struct.pack("<6hHH", *face, *neighbours, 0x800, 0))
    # A single spatial lookup bucket is sufficient for this small interior.
    bounds = (xs[0], ys[0], 1., xs[-1], ys[-1], 1.)
    grid = struct.pack("<I8fH", 1, xs[-1]-xs[0], ys[-1]-ys[0], *bounds, len(faces))
    grid += struct.pack(f"<{len(faces)}H", *range(len(faces)))
    mesh = record(b"NAVM", MESH, [
        (b"NVER", u32(11)), (b"DATA", struct.pack("<6I", CELL, len(vertices), len(faces), 0, 0, 0)),
        (b"NVVX", b"".join(struct.pack("<3f", *v) for v in vertices)),
        (b"NVTR", b"".join(triangles)), (b"NVGD", grid)])
    # Fallout merges per-plugin entries into its one navigation index, as the DLCs do.
    index = record(b"NAVI", 0x14B92, [
        (b"NVER", u32(11)), (b"NVMI", struct.pack("<4I3fI", 0, MESH, CELL, 0, 0., 121., 1., 0)),
        (b"NVCI", struct.pack("<4I", MESH, 0, 0, 0))])
    return mesh, index


def build():
    mesh, index = navigation()
    permanent, temporary = [], [mesh]
    panel_base = OWN + 0x803
    panel = record(b"ACTI", panel_base, [
        (b"EDID", string("TTWVRAudioLabSwitch")), (b"OBND", struct.pack("<6h", -6,-6,-12,6,3,12)),
        (b"FULL", string("Ghoul test - start / stop")), (b"MODL", string(r"clutter\genelecswitch02.nif"))])
    next_id = OWN + 0x820

    def place(base, x, y, z, yaw=0, *, key=None, editor=None, actor=False):
        nonlocal next_id
        if key is None:
            form = next_id
            next_id += 1
        else:
            form = OWN + key
        items = [(b"EDID", string(editor))] if editor else []
        items += [(b"NAME", u32(base)), (b"DATA", struct.pack("<6f", x, y, z, 0, 0, yaw))]
        flags = (0x400 if editor else 0) | (0x800 if actor else 0)
        (permanent if editor else temporary).append(record(b"ACRE" if actor else b"REFR", form, items, flags))

    # Office kit tiles have built-in floors, ceilings and perimeter walls.
    for ix in range(3):
        for iy in range(4):
            x, y = (ix-1)*256, (iy-1)*256
            west, east, south, north = ix == 0, ix == 2, iy == 0, iy == 3
            if (west or east) and (south or north):
                base = 0x159DD
                yaw = {(True, True): pi, (True, False): 1.5*pi,
                       (False, True): 2.5*pi, (False, False): 2*pi}[west, south]
            elif west or east or south or north:
                base = 0x159E0
                yaw = 1.5*pi if west else .5*pi if east else pi if south else 2*pi
            else:
                base, yaw = 0x159DF, 0.
            place(base, x, y, 0., yaw)

    place(0x32, 0, -176, 4, key=0x810, editor="TTWVRAudioArrival")
    place(panel_base, 0, -56, 91, key=0x811, editor="TTWVRAudioControls")
    for x in (-53, 53):
        place(0x10AC4B, x, 0, 0)  # opaque water crates, 156 units high
    place(0xF21B3, -116, -155, 0)
    for col, (base, height) in enumerate([(0x22104, 7), (0x5B6CA, 8), (0x9AA4E, 12),
                                          (0x5B6CE, 1), (0x3406C, 9)]):
        for y in (-168, -144):
            place(base, -171 + col*24, y, 55 + height)
    place(0x4328, -57, -168, 57)  # tire iron for tapping held objects
    place(0x01000804, 86, -117, 5)
    place(0x01000804, 95, -222, 5)
    place(0x01000814, 233, 162, 2.25)
    for i, (x, y, base) in enumerate(((-72, 250, 0x9FAFA), (68, 335, 0x1CF7A))):
        place(0x34, x, y, 4, key=0x816+i, editor=f"TTWVRAudioGhoulHome{i+1}")
        place(base, x, y, 4, key=0x814+i, editor=f"TTWVRAudioGhoul{i+1}", actor=True)

    lighting = bytes((100,100,94,0, 150,145,122,0, 46,48,48,0)) + struct.pack("<ffiifff", 5000,8000,40,45,.5,0,1)
    cell = record(b"CELL", CELL, [
        (b"EDID", string(STAGE)), (b"FULL", string("Ghoul hearing range")), (b"DATA", b"\x21"),
        (b"XCLL", lighting), (b"LTMP", u32(0)), (b"LNAM", u32(0)), (b"XCLW", struct.pack("<f",-2147483648.)),
        (b"XNAM", b"\0")])
    children = group(u32(CELL), 6, [group(u32(CELL), 8, permanent), group(u32(CELL), 9, temporary)])
    block = group(u32(8), 2, [group(u32(4), 3, [cell, children])])  # 0x800 = 2048
    groups = [group(b"ACTI", 0, [panel]), group(b"NAVI", 0, [index]), group(b"CELL", 0, [block])]
    header = record(b"TES4", 0, [
        (b"HEDR", struct.pack("<fII", 1.34, 3 + len(temporary) + len(permanent) + 8, next_id - OWN)),
        (b"CNAM", string("TTWVR Clutter Distractions")),
        (b"MAST", string("FalloutNV.esm")), (b"DATA", bytes(8)),
        (b"MAST", string(CORE + ".esp")), (b"DATA", bytes(8))])
    return header + b"".join(groups)


if __name__ == "__main__":
    output = BUILD / f"TTWVR-Clutter-Audio-Test-{VERSION}.zip"
    with ZipFile(BUILD / f"{CORE}-{VERSION}.zip") as mod, ZipFile(output.with_suffix(".tmp"), "w", ZIP_DEFLATED) as archive:
        for entry in mod.namelist():
            # The DLL supplies its defaults. Preserve an existing user's test settings.
            if not entry.lower().endswith(".ini"):
                archive.writestr(entry, mod.read(entry))
        archive.writestr(STAGE + ".esp", build())
        for path in sorted((ROOT / "test-scene").rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(ROOT / "test-scene").as_posix())
    output.with_suffix(".tmp").replace(output)
    print(output)
