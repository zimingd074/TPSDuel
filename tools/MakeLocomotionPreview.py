"""Create a looping preview from eight real consecutive game screenshots.

Optional documentation tool; requires Pillow. Does not generate game artwork.
"""
from pathlib import Path
from datetime import datetime
import re
from PIL import Image

folder = Path(__file__).resolve().parents[1] / "docs/screenshots/locomotion-v0.5.0"
frames = []
for index in range(8):
    with Image.open(folder / f"Motion-{index:02d}.png") as source:
        frames.append(source.convert("RGB").resize((960, 443), Image.Resampling.LANCZOS))
palette = frames[0].quantize(colors=256)
output = [palette] + [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames[1:]]
log = folder.parents[2] / "Saved/locomotion-v050.log"
times = {}
for line in log.read_text(encoding="utf-8-sig").splitlines():
    match = re.search(r"^\[([\d.\-:]+)\].*MOTION_SAMPLE frame=(\d+)", line)
    if match and int(match[2]) < 8:
        times[int(match[2])] = datetime.strptime(match[1], "%Y.%m.%d-%H.%M.%S:%f")
if len(times) != 8:
    raise RuntimeError("Missing matching screenshot timestamps")
durations = [max(20, round((times[index + 1] - times[index]).total_seconds() * 1000)) for index in range(7)]
durations.append(durations[-1])
output[0].save(folder / "跑动连续预览.gif", save_all=True, append_images=output[1:], duration=durations, loop=0, optimize=True)
print(folder / "跑动连续预览.gif")
