"""Build-time only: render the game's pixel adventurer as PNG and Windows ICO."""
from io import BytesIO
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFilter

root = Path(__file__).resolve().parent
sprite = Image.new("RGBA", (32, 32))
draw = ImageDraw.Draw(sprite)
mint = (128, 237, 187, 255)
silver = (214, 227, 239, 255)
gold = (255, 205, 118, 255)
navy = (69, 81, 108, 255)
pixels = ("00111100", "01111110", "11011011", "11111111", "01111110", "00111100", "01100110", "11000011")
for row, line in enumerate(pixels):
    for column, bit in enumerate(line):
        if bit == "1":
            color = silver if row == 2 else navy if row > 4 else mint
            x, y = 6 + column * 2, 11 + row * 2
            draw.rectangle((x, y, x + 1, y + 1), fill=color)
# The same horned helmet and sword silhouette used by the world hero.
draw.rectangle((6, 9, 21, 12), fill=(177, 194, 190, 255))
draw.rectangle((3, 7, 5, 13), fill=silver)
draw.rectangle((22, 7, 24, 13), fill=silver)
draw.rectangle((8, 9, 18, 9), fill=(244, 247, 232, 255))
for step in range(8):
    draw.rectangle((21 + step, 17 - step, 22 + step, 18 - step), fill=silver)
    draw.point((22 + step, 18 - step), fill=mint)
draw.rectangle((19, 18, 25, 19), fill=gold)
draw.rectangle((21, 16, 22, 22), fill=gold)
draw.rectangle((18, 21, 20, 23), fill=(143, 107, 69, 255))
draw.rectangle((16, 21, 18, 23), fill=gold)
# A one-pixel outline reads clearly on light and dark File Explorer backgrounds.
mask = sprite.getchannel("A").filter(ImageFilter.MaxFilter(3))
outlined = Image.new("RGBA", sprite.size, (19, 30, 37, 0))
outlined.putalpha(mask)
outlined.alpha_composite(sprite)
png = outlined.resize((256, 256), Image.Resampling.NEAREST)
png.save(root / "adventurer.png", optimize=True)

# Store each icon size as crisp PNG pixels in a standard multi-image ICO.
sizes = (16, 24, 32, 48, 64, 128, 256)
payloads = []
for size in sizes:
    stream = BytesIO()
    outlined.resize((size, size), Image.Resampling.NEAREST).save(stream, format="PNG", optimize=True)
    payloads.append(stream.getvalue())
offset = 6 + 16 * len(sizes)
entries = []
for size, raw in zip(sizes, payloads):
    entries.append(struct.pack("<BBBBHHII", size if size < 256 else 0, size if size < 256 else 0, 0, 0, 1, 32, len(raw), offset))
    offset += len(raw)
(root / "adventurer.ico").write_bytes(struct.pack("<HHH", 0, 1, len(sizes)) + b"".join(entries) + b"".join(payloads))
assert png.getchannel("A").getextrema() == (0, 255)
assert Image.open(root / "adventurer.ico").ico.sizes() == {(s, s) for s in sizes}
print("Created transparent adventurer.png and 7-size adventurer.ico.")
