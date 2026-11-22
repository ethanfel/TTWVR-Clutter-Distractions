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
assert struct.unpack("<fII", header[b"HEDR"])[1:] == (len(all_records) - 1, 0x825)
forms = {i: (t, f) for t, i, f in all_records if i}
assert len(forms) == 14
for tier in range(5):
    item, armed = 0x01000800+tier, 0x01000810+tier
    tag, data = forms[item]
    assert tag == b"MISC" and struct.unpack("<If",dict(data)[b"DATA"])[1] == 10.0
    if tier:
        tag, data = forms[armed]
        assert tag == b"ACTI" and dict(data)[b"SCRI"] == struct.pack("<I",0x2cba1)
    if tier < 4:
        tag, data = forms[0x01000821+tier]
        values = [(t, int.from_bytes(v,"little")) for t,v in data if t in (b"RCIL",b"RCOD",b"RCQY")]
        assert values == [(b"RCIL",item),(b"RCQY",1),(b"RCIL",0x31944),(b"RCQY",1),(b"RCOD",item+1),(b"RCQY",1)]
tag, data = forms[0x01000820]
assert tag == b"RCPE" and struct.unpack("<4I",dict(data)[b"DATA"]) == (39,0,0x13b2c1,0x13e11b)
assert dict(data)[b"RCOD"] == struct.pack("<I",0x01000804)
print("Trap records, recipes and master links passed")
