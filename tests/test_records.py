"""Parse the shipped records independently of their generator."""
from pathlib import Path
import struct
import sys

raw = Path(sys.argv[1]).read_bytes()


def fields(record):
    out = []
    pos = 0
    while pos < len(record):
        tag, n = struct.unpack_from("<4sH", record, pos)
        pos += 6
        out.append((tag, record[pos:pos+n]))
        pos += n
    assert pos == len(record)
    return out


def records(lo, end):
    while lo < end:
        tag, size = struct.unpack_from("<4sI", raw, lo)
        if tag == b"GRUP":
            yield (tag, 0, [])
            yield from records(lo + 24, lo + size)
            lo += size
        else:
            yield tag, struct.unpack_from("<I", raw, lo + 12)[0], fields(raw[lo + 24:lo + 24 + size])
            lo += 24 + size
    assert lo == end


all_records = list(records(0, len(raw)))
header = dict(all_records[0][2])
assert header[b"MAST"] == b"FalloutNV.esm\0"
assert struct.unpack("<fII", header[b"HEDR"])[1:] == (len(all_records) - 1, 0x954)
forms = {i: (t, f) for t, i, f in all_records if i}
assert len(forms) == 66
assert forms[0x01000832][0] == b"FACT"
for tier in range(5):
    item, armed = 0x01000800+tier, 0x01000810+tier
    tag, data = forms[item]
    assert tag == b"MISC" and struct.unpack("<If",dict(data)[b"DATA"])[1] == 10.0
    assert dict(data)[b"MODL"] == b"TTWVRClutter\\Portable.nif\0"
    if tier:
        tag, data = forms[armed]
        assert dict(data)[b"MODL"] == b"Traps\\BearTrap01.NIF\0"
        assert tag == b"ACTI" and dict(data)[b"SCRI"] == struct.pack("<I",0x2cba1)
    if tier < 4:
        tag, data = forms[0x01000821+tier]
        values = [(t, int.from_bytes(v,"little")) for t,v in data if t in (b"RCIL",b"RCOD",b"RCQY")]
        assert values == [(b"RCIL",item),(b"RCQY",1),(b"RCIL",0x31944),(b"RCQY",1),(b"RCOD",item+1),(b"RCQY",1)]
tag, data = forms[0x01000820]
assert tag == b"RCPE" and struct.unpack("<4I",dict(data)[b"DATA"]) == (39,0,0x13b2c1,0x13e11b)
assert dict(data)[b"RCOD"] == struct.pack("<I",0x01000804)

tag, data = forms[0x01000830]
assert tag == b"WEAP" and dict(data)[b"WNAM"] == struct.pack("<I", 0x01000831)
assert struct.unpack("<IIfHB", dict(data)[b"DATA"])[2:] == (0., 0, 0)
assert dict(data)[b"MODL"] == dict(forms[0x01000831][1])[b"MODL"] == b"TTWVRClutter\\Held.nif\0"
stock = [int.from_bytes(v, "little") for t, v in forms[0x01000833][1] if t == b"LNAM"]
assert len(stock) == len(set(stock)) == 21
for tier, item in enumerate(stock):
    assert dict(forms[item][1])[b"FULL"] == f"Portable Bear Trap ({tier * 5}%)".encode() + b"\0"
    if tier % 5:
        assert forms[0x01000920 + tier][0] == b"ACTI"
        assert dict(forms[0x01000940 + tier][1])[b"RCOD"] == struct.pack("<I", stock[min(20, tier + 5)])
print("Trap records, legacy IDs, condition tiers, recipes and holster stock passed")
