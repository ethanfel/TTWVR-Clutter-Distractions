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


def record(tag, form, fields, flags=0):
    body = b"".join(struct.pack("<4sH", key, len(value)) + value for key, value in fields)
    return struct.pack("<4sIIIIHH", tag, len(body), flags, form, 0, 15, 0) + body


def recipe(form, name, ingredients, output):
    fields = [(b"EDID", string(f"TTWVRTrapRecipe{form:X}")), (b"FULL", string(name)),
              (b"DATA", struct.pack("<4I", 39, 0, 0x13B2C1, 0x13E11B))]
    for item, count in ingredients:
        fields.extend([(b"RCIL", u32(item)), (b"RCQY", u32(count))])
    fields.extend([(b"RCOD", u32(output)), (b"RCQY", u32(1))])
    return record(b"RCPE", PLUGIN + form, fields)


def tier_id(tier, legacy, extended):
    return legacy + tier // 5 if tier % 5 == 0 else extended + tier


def build():
    groups = {b"FACT": [], b"MGEF": [], b"SPEL": [], b"ACTI": [], b"MISC": [], b"STAT": [], b"WEAP": [], b"FLST": [], b"RCPE": []}
    # No actors belong to this faction. TTW's native trigger skips the owner.
    groups[b"FACT"].append(record(b"FACT", PLUGIN+0x832, [
        (b"EDID", string("TTWVRTrapNeutralOwner")), (b"DATA", u32(1))]))
    # A timed, recoverable SpeedMult modifier. The engine removes it on expiry,
    # including after save/load. It never disables AI or controller tracking.
    speed_mult = 21
    effect = bytearray(72)
    struct.pack_into("<I", effect, 0, 0x16)  # recover + detrimental + self
    struct.pack_into("<ii", effect, 12, -1, -1)
    struct.pack_into("<f", effect, 28, 1.)
    struct.pack_into("<I", effect, 68, speed_mult)
    groups[b"MGEF"].append(record(b"MGEF", PLUGIN+0x834, [
        (b"EDID", string("TTWVRTrapCaughtSpeed")), (b"FULL", string("Caught in bear trap")),
        (b"DATA", bytes(effect))]))
    for form, name, magnitude, seconds in (
        (0x835, "TTWVRTrapBriefCatch", 100, 1),
        (0x836, "TTWVRTrapActorCatch", 1000, 4),
    ):
        groups[b"SPEL"].append(record(b"SPEL", PLUGIN+form, [
            (b"EDID", string(name)), (b"FULL", string("Caught in bear trap")),
            (b"SPIT", bytes(16)), (b"EFID", u32(PLUGIN+0x834)),
            (b"EFIT", struct.pack("<5I", magnitude, 0, seconds, 0, speed_mult))]))
    bounds = struct.pack("<6h", -49, -32, -2, 49, 32, 16)
    # A stable, weightless hotkey represents the player's stock across condition tiers.
    held_model = string(r"TTWVRClutter\Held.nif")
    portable_model = string(r"TTWVRClutter\Portable.nif")
    groups[b"STAT"].append(record(b"STAT", PLUGIN+0x831, [
        (b"EDID", string("TTWVRFirstPersonTrap")), (b"OBND", bounds), (b"MODL", held_model)]))
    tuning = bytearray(204)
    struct.pack_into("<IffBBBB", tuning, 0, 1, 1., 0., 8, 255, 0, 0)
    tuning[41] = 255
    struct.pack_into("<IIf", tuning, 52, 3, 0x41, 1.)
    struct.pack_into("<i", tuning, 104, 38)
    struct.pack_into("<fi", tuning, 116, 1., -1)
    groups[b"WEAP"].append(record(b"WEAP", PLUGIN+0x830, [
        (b"EDID", string("TTWVRTrapCarrier")), (b"FULL", string("Portable Bear Trap (Place)")),
        (b"OBND", bounds), (b"MODL", held_model),
        (b"ICON", string(r"Interface\Icons\PipboyImages\Items\item_junk.dds")),
        (b"ETYP", u32(3)), (b"WNAM", u32(PLUGIN+0x831)),
        (b"DATA", struct.pack("<IIfHB", 0, 100, 0., 0, 0)), (b"DNAM", tuning),
        (b"CRDT", bytes(16)), (b"VNAM", u32(2))], flags=0x400))
    stock = [PLUGIN + tier_id(t, CLOSED, 0x900) for t in range(21)]
    groups[b"FLST"].append(record(b"FLST", PLUGIN+0x833, [
        (b"EDID", string("TTWVRTrapStock")), *((b"LNAM", u32(form)) for form in stock)]))
    for tier, closed in enumerate(stock):
        suffix = str(tier // 5) if tier % 5 == 0 else f"Pct{tier * 5}"
        groups[b"MISC"].append(record(b"MISC", closed, [
            (b"EDID", string(f"TTWVRPortableBearTrap{suffix}")),
            (b"OBND", bounds), (b"FULL", string(f"Portable Bear Trap ({tier * 5}%)")),
            (b"MODL", portable_model), (b"ICON", string(r"Interface\Icons\PipboyImages\Items\item_junk.dds")),
            (b"DATA", struct.pack("<If", 5 + tier, 10.0))]))
        if tier:
            groups[b"ACTI"].append(record(b"ACTI", PLUGIN + tier_id(tier, ARMED, 0x920), [
                (b"EDID", string(f"TTWVRArmedBearTrap{suffix}")), (b"OBND", bounds),
                (b"FULL", string("Portable Bear Trap (armed)")), (b"MODL", string(MODEL)),
                (b"SCRI", u32(0x2CBA1))]))
        if tier < 20:
            restored = min(20, tier + 5)
            groups[b"RCPE"].append(recipe(tier_id(tier, 0x821, 0x940),
                f"Repair Bear Trap ({tier * 5}% to {restored * 5}%)",
                [(closed, 1), (SCRAP, 1)], stock[restored]))
    groups[b"RCPE"].append(recipe(0x820, "Portable Bear Trap", [(SCRAP, 4)], PLUGIN + CLOSED + 4))
    total = sum(len(records) + 1 for records in groups.values())
    header = record(b"TES4", 0, [(b"HEDR", struct.pack("<fII", 1.34, total, 0x954)),
                                 (b"CNAM", string("TTWVR Clutter Distractions")),
                                 (b"MAST", string("FalloutNV.esm")), (b"DATA", bytes(8))])
    for tag, records in groups.items():
        data = b"".join(records)
        header += struct.pack("<4sI4sIII", b"GRUP", 24 + len(data), tag, 0, 0, 0) + data
    return header


if __name__ == "__main__":
    Path(sys.argv[1]).write_bytes(build())
