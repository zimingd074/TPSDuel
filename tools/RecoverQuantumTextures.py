"""Recover only complete, CRC-checked arm textures from an interrupted ZIP.

This does not treat a .crdownload as a complete resource pack. Other character
materials still need the full download; never extract an incomplete entry.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import zlib

parser = argparse.ArgumentParser()
parser.add_argument("archive", type=pathlib.Path)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
destination = root / "Assets/Source/Fab/Quantum"
data = args.archive.read_bytes()
position = 0
recovered = []
while data[position:position + 4] == b"PK\x03\x04":
    _, version, flags, method, _, _, crc, compressed, size, name_length, extra_length = struct.unpack_from("<IHHHHHIIIHH", data, position)
    name = data[position + 30:position + 30 + name_length].decode("utf-8" if flags & 2048 else "cp437")
    start = position + 30 + name_length + extra_length
    if flags & 9 or compressed == 0xffffffff or start + compressed > len(data):
        break
    if name.startswith("PBR_Textures/Arms/") and any(key in name for key in ("_BaseColor.", "_Normal.", "_OcclusionRoughnessMetallic.")):
        if size > 64 * 1024 * 1024:
            raise RuntimeError("Unexpected texture size")
        payload = data[start:start + compressed]
        raw = zlib.decompress(payload, -15) if method == 8 else payload if method == 0 else None
        if raw is None or len(raw) != size or zlib.crc32(raw) & 0xffffffff != crc:
            raise RuntimeError("Incomplete/corrupt entry: " + name)
        target = (destination / name).resolve()
        if destination.resolve() not in target.parents:
            raise RuntimeError("ZIP path outside destination")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        recovered.append({"file": name, "bytes": size, "crc32": f"{crc:08x}", "sha256": hashlib.sha256(raw).hexdigest()})
    position = start + compressed
if len(recovered) != 3:
    raise RuntimeError("Expected three complete arm textures; found " + str(len(recovered)))
report = {"complete_pack": False, "source": "https://www.fab.com/listings/8e200050-3158-4762-b297-f785b5b1533d", "license": "Fab Standard", "files": recovered}
(destination / "RecoveredTextures.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))
