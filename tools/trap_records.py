# SPDX-License-Identifier: GPL-3.0-only
"""FalloutNV.esm supplies the trap model, sound and armed activator script."""
import struct
import sys
from pathlib import Path

PLUGIN = 0x01000000
CLOSED = 0x800
ARMED = 0x810
SCRAP = 0x31944
MODEL = r"Traps\BearTrap01.NIF"


def u32(value):
    return struct.pack("<I", value)


def string(value):
    return value.encode("ascii") + b"\0"


def record(tag, form, fields):
    body = b"".join(struct.pack("<4sH", key, len(value)) + value for key, value in fields)
    return struct.pack("<4sIIIIHH", tag, len(body), 0, form, 0, 15, 0) + body


def recipe(form, name, ingredients, output):
    fields = [(b"EDID", string(f"TTWVRTrapRecipe{form:X}")), (b"FULL", string(name)),
              (b"DATA", struct.pack("<4I", 39, 0, 0x13B2C1, 0x13E11B))]
    for item, count in ingredients:
        fields.extend([(b"RCIL", u32(item)), (b"RCQY", u32(count))])
    fields.extend([(b"RCOD", u32(output)), (b"RCQY", u32(1))])
    return record(b"RCPE", PLUGIN + form, fields)


def build():
    groups = {b"ACTI": [], b"MISC": [], b"RCPE": []}
    bounds = struct.pack("<6h", -49, -32, -2, 49, 32, 16)
    for tier in range(5):
        closed = PLUGIN + CLOSED + tier
        groups[b"MISC"].append(record(b"MISC", closed, [
            (b"EDID", string(f"TTWVRPortableBearTrap{tier}")),
            (b"OBND", bounds), (b"FULL", string(f"Portable Bear Trap ({tier * 25}%)")),
            (b"MODL", string(MODEL)), (b"ICON", string(r"Interface\Icons\PipboyImages\Items\item_junk.dds")),
            (b"DATA", struct.pack("<If", 5 + 5 * tier, 10.0))]))
        if tier:
            groups[b"ACTI"].append(record(b"ACTI", PLUGIN + ARMED + tier, [
                (b"EDID", string(f"TTWVRArmedBearTrap{tier}")), (b"OBND", bounds),
                (b"FULL", string("Portable Bear Trap (armed)")), (b"MODL", string(MODEL)),
                (b"SCRI", u32(0x2CBA1))]))
        if tier < 4:
            groups[b"RCPE"].append(recipe(0x821 + tier, f"Repair Bear Trap ({tier * 25}% to {(tier + 1) * 25}%)",
                                           [(closed, 1), (SCRAP, 1)], closed + 1))
    groups[b"RCPE"].append(recipe(0x820, "Portable Bear Trap", [(SCRAP, 4)], PLUGIN + CLOSED + 4))
    total = sum(len(records) + 1 for records in groups.values())
    header = record(b"TES4", 0, [(b"HEDR", struct.pack("<fII", 1.34, total, 0x825)),
                                 (b"CNAM", string("TTWVR Clutter Distractions")),
                                 (b"MAST", string("FalloutNV.esm")), (b"DATA", bytes(8))])
    for tag, records in groups.items():
        data = b"".join(records)
        header += struct.pack("<4sI4sIII", b"GRUP", 24 + len(data), tag, 0, 0, 0) + data
    return header


if __name__ == "__main__":
    Path(sys.argv[1]).write_bytes(build())
