import os
import sys
from PIL import Image

def image_to_1bit_bytes(img: Image.Image, dither=True) -> bytes:
    # Convert to 1-bit (mode "1") with optional dithering (Floyd-Steinberg)
    img_l = img.convert("L")
    img_1 = img_l.convert("1", dither=Image.FLOYDSTEINBERG if dither else Image.NONE)

    w, h = img_1.size
    pixels = img_1.load()

    # Pack bits: 1 byte = 8 pixels, MSB first (common for e-ink libs)
    out = bytearray()
    for y in range(h):
        byte = 0
        bit_count = 0
        for x in range(w):
            # In mode "1": pixel is 0 (black) or 255 (white)
            is_white = (pixels[x, y] != 0)
            bit = 1 if is_white else 0  # 1=white, 0=black (adjust if your library expects opposite)

            byte = (byte << 1) | bit
            bit_count += 1

            if bit_count == 8:
                out.append(byte)
                byte = 0
                bit_count = 0

        if bit_count != 0:
            # pad remaining bits in row to full byte
            byte = byte << (8 - bit_count)
            out.append(byte)

    return bytes(out)

def main():
    if len(sys.argv) < 3:
        print("Usage: py png_to_1bit_bin.py <input_folder> <output_folder>")
        sys.exit(1)

    in_dir = sys.argv[1]
    out_dir = sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    files = sorted([f for f in os.listdir(in_dir) if f.lower().endswith(".png")])

    for f in files:
        path = os.path.join(in_dir, f)
        img = Image.open(path).convert("RGB")

        if img.size != (400, 300):
            print(f"Skipping {f} (not 400x300)")
            continue

        data = image_to_1bit_bytes(img, dither=True)

        base = os.path.splitext(f)[0]
        out_path = os.path.join(out_dir, base + ".bin")
        with open(out_path, "wb") as fp:
            fp.write(data)

        print("Wrote:", out_path, "bytes=", len(data))

    print("DONE")

if __name__ == "__main__":
    main()
