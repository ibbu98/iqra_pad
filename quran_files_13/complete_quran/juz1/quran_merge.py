import fitz
from PIL import Image
import os
import sys

# Usage:
#   py quran_merge.py input.pdf
#
# Output:
#   output_400x300/page_0001.png, page_0002.png, ...

def render_page(doc, page_index, zoom=2.0):
    page = doc.load_page(page_index)
    pix = page.get_pixmap(matrix=fitz.Matrix(zoom, zoom), alpha=False)
    return Image.frombytes("RGB", (pix.width, pix.height), pix.samples)

def pad_to_height(img, target_h, bg=(255, 255, 255)):
    if img.height == target_h:
        return img
    canvas = Image.new("RGB", (img.width, target_h), bg)
    canvas.paste(img, (0, 0))
    return canvas

def main():
    if len(sys.argv) < 2:
        print("Usage: py quran_merge.py input.pdf")
        sys.exit(1)

    pdf_path = sys.argv[1]
    out_dir = "output_400x300"
    os.makedirs(out_dir, exist_ok=True)

    doc = fitz.open(pdf_path)
    n = doc.page_count

    # ✅ Skip FIRST page (index 0)
    start_page = 1

    out_index = 1
    bg = (255, 255, 255)
    zoom = 2.0

    # We take pairs: (start_page, start_page+1), (start_page+2, start_page+3), ...
    for i in range(start_page, n, 2):
        p_a = i           # earlier page in the pair
        p_b = i + 1       # next page in the pair

        img_a = render_page(doc, p_a, zoom=zoom)

        if p_b < n:
            img_b = render_page(doc, p_b, zoom=zoom)
        else:
            img_b = Image.new("RGB", img_a.size, bg)

        # Pad to same height
        h = max(img_a.height, img_b.height)
        img_a = pad_to_height(img_a, h, bg)
        img_b = pad_to_height(img_b, h, bg)

        # ✅ RTL FIX (Quran style):
        # earlier page should appear on the RIGHT side
        # so: LEFT side = img_b, RIGHT side = img_a
        merged = Image.new("RGB", (img_a.width + img_b.width, h), bg)
        merged.paste(img_b, (0, 0))                 # left
        merged.paste(img_a, (img_b.width, 0))       # right

        # Resize to exact 400x300
        final = merged.resize((400, 300), Image.LANCZOS)

        out_path = os.path.join(out_dir, f"page_{out_index:04d}.png")
        final.save(out_path, optimize=True)

        print(f"Saved: {out_path}  (PDF pages {p_a+1} & {min(p_b+1, n)})")
        out_index += 1

    doc.close()
    print("DONE. Output folder:", out_dir)

if __name__ == "__main__":
    main()
