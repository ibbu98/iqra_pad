import os
import sys
import fitz
from PIL import Image

# =========================
# SETTINGS
# =========================
ZOOM = 2.0
START_PAGE_INDEX = 0   # start from page 1

TARGET_W, TARGET_H = 400, 300

# neat crop tuning
CROP_BG_THRESH = 245
CROP_PAD = 8

# B/W conversion (NO dithering)
BW_THRESHOLD = 200
WHITE_IS_1 = True
INVERT_FOR_GXEPD2 = True

# ✅ Main output folder name
MAIN_OUT = "quran_processed"

PNG_DIR = "png_400x300"
BIN_DIR = "bin_1bit"
CPP_DIR = "cpp_arrays"

# =========================
# PDF RENDER
# =========================
def render_page(doc, idx):
    page = doc.load_page(idx)
    pix = page.get_pixmap(matrix=fitz.Matrix(ZOOM, ZOOM), alpha=False)
    return Image.frombytes("RGB", (pix.width, pix.height), pix.samples)

# =========================
# NEAT CROP
# =========================
def neat_crop(img):
    g = img.convert("L")
    px = g.load()
    w, h = g.size

    left, right = w, 0
    top, bottom = h, 0

    for y in range(h):
        for x in range(w):
            if px[x, y] < CROP_BG_THRESH:
                if x < left: left = x
                if x > right: right = x
                if y < top: top = y
                if y > bottom: bottom = y

    if right <= left or bottom <= top:
        return img

    left = max(0, left - CROP_PAD)
    top = max(0, top - CROP_PAD)
    right = min(w-1, right + CROP_PAD)
    bottom = min(h-1, bottom + CROP_PAD)

    return img.crop((left, top, right+1, bottom+1))

# =========================
# MERGE RTL
# =========================
def pad_h(img, h):
    if img.height == h:
        return img
    bg = Image.new("RGB", (img.width, h), (255,255,255))
    bg.paste(img, (0,0))
    return bg

def merge_rtl(a, b):
    h = max(a.height, b.height)
    a = pad_h(a, h)
    b = pad_h(b, h)
    out = Image.new("RGB", (a.width+b.width, h), (255,255,255))
    out.paste(b, (0,0))
    out.paste(a, (b.width,0))
    return out

# =========================
# 1-BIT PACK
# =========================
def to_1bit_bytes(img):
    g = img.convert("L")
    bw = g.point(lambda p: 255 if p >= BW_THRESHOLD else 0)

    px = bw.load()
    w,h = bw.size

    data = bytearray()
    for y in range(h):
        byte=0; bits=0
        for x in range(w):
            is_white = (px[x,y]==255)
            bit = 1 if is_white else 0
            if not WHITE_IS_1: bit ^= 1

            byte = (byte<<1)|bit
            bits+=1
            if bits==8:
                data.append(byte)
                byte=0; bits=0

        if bits:
            data.append(byte<<(8-bits))

    return bytes(data)

# =========================
# CPP WRITER
# =========================
def write_cpp(data, name, path):
    if INVERT_FOR_GXEPD2:
        data = bytes([b^0xFF for b in data])

    with open(path,"w") as f:
        f.write('#include "quran_pages.h"\n\n')
        f.write(f"const uint8_t {name}[] PROGMEM = {{\n")
        for i in range(0,len(data),16):
            chunk=data[i:i+16]
            f.write("  "+", ".join(f"0x{b:02X}" for b in chunk)+",\n")
        f.write("};\n")

# =========================
# MAIN
# =========================
def main():
    if len(sys.argv)<2:
        print("Usage: py all_in_1.py file.pdf")
        return

    pdf = sys.argv[1]
    doc = fitz.open(pdf)

    # ✅ Create folder tree
    base = os.path.join(os.getcwd(), MAIN_OUT)
    png_dir = os.path.join(base, PNG_DIR)
    bin_dir = os.path.join(base, BIN_DIR)
    cpp_dir = os.path.join(base, CPP_DIR)

    os.makedirs(png_dir, exist_ok=True)
    os.makedirs(bin_dir, exist_ok=True)
    os.makedirs(cpp_dir, exist_ok=True)

    print("Saving into:", base)

    vars=[]
    idx_out=1

    for i in range(START_PAGE_INDEX, doc.page_count, 2):
        a = render_page(doc,i)
        b = render_page(doc,i+1) if i+1<doc.page_count else Image.new("RGB",a.size,(255,255,255))

        a = neat_crop(a)
        b = neat_crop(b)

        merged = merge_rtl(a,b)
        merged = neat_crop(merged)

        final = merged.resize((TARGET_W,TARGET_H), Image.Resampling.LANCZOS)

        png_path = os.path.join(png_dir,f"page_{idx_out:04d}.png")
        final.save(png_path)

        data = to_1bit_bytes(final)

        bin_path = os.path.join(bin_dir,f"page_{idx_out:04d}.bin")
        open(bin_path,"wb").write(data)

        var=f"quran_page_{idx_out:04d}"
        cpp_path = os.path.join(cpp_dir,var+".cpp")
        write_cpp(data,var,cpp_path)
        vars.append(var)

        print("Saved page", idx_out)
        idx_out+=1

    # header
    with open(os.path.join(cpp_dir,"quran_pages.h"),"w") as h:
        h.write("#pragma once\n#include <Arduino.h>\n\n")
        for v in vars:
            h.write(f"extern const uint8_t {v}[];\n")

    print("\nDONE ✅ All files inside folder:", base)

if __name__=="__main__":
    main()
