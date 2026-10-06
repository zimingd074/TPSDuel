"""Validate the acquired full Fab ZIP and extract only runtime PBR maps."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("archive", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / "Assets/Source/Fab/Quantum"
root.mkdir(parents=True, exist_ok=True)
files = []
with zipfile.ZipFile(args.archive) as archive:
    bad = archive.testzip()
    if bad:
        raise RuntimeError("ZIP CRC failed: " + bad)
    for entry in archive.infolist():
        if entry.is_dir() or not any(key in entry.filename for key in ("_BaseColor.", "_Normal.", "_OcclusionRoughnessMetallic.")):
            continue
        if "Unity_Normal" in entry.filename:
            continue
        target = (root / entry.filename).resolve()
        if root.resolve() not in target.parents or entry.file_size > 128 * 1024 * 1024:
            raise RuntimeError("Unexpected archive entry: " + entry.filename)
        raw = archive.read(entry)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        files.append({"file": entry.filename, "bytes": len(raw), "crc32": f"{entry.CRC:08x}", "sha256": hashlib.sha256(raw).hexdigest()})
report = {"complete_pack": True, "source": "https://www.fab.com/listings/8e200050-3158-4762-b297-f785b5b1533d", "license": "Fab Standard", "archive_bytes": args.archive.stat().st_size, "archive_sha256": hashlib.file_digest(args.archive.open("rb"), "sha256").hexdigest(), "files": files}
(root / "FullTextures.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print("Validated full archive; extracted", len(files), "PBR maps")
