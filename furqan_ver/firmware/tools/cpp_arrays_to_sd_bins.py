"""
cpp_arrays_to_sd_bins.py
Convert quran_page_XXXX.cpp C-array files → raw .bin files for the SD card.

Usage:
    python tools/cpp_arrays_to_sd_bins.py --src src --out output/13line
    python tools/cpp_arrays_to_sd_bins.py --src src_15line --out output/15line

Then copy the output folder contents to your SD card:
    SD:/13line/0001.bin  …  SD:/13line/0604.bin
    SD:/15line/0001.bin  …  SD:/15line/0604.bin

Each .bin file is exactly 15,000 bytes (400 × 300 ÷ 8 — raw 1bpp bitmap, MSB-first).
"""

import os
import re
import argparse

EXPECTED_BYTES = 15_000   # 400 × 300 / 8

def convert(cpp_path: str, bin_path: str) -> bool:
    with open(cpp_path, "r") as f:
        content = f.read()

    # Pull every 0xNN hex literal out of the file
    raw = re.findall(r"0x([0-9A-Fa-f]{2})", content)
    if not raw:
        print(f"  WARNING: no hex data found in {cpp_path}")
        return False

    data = bytes(int(b, 16) for b in raw)

    if len(data) != EXPECTED_BYTES:
        print(f"  WARNING: {cpp_path} has {len(data)} bytes (expected {EXPECTED_BYTES})")

    with open(bin_path, "wb") as f:
        f.write(data)

    print(f"  {os.path.basename(cpp_path)} → {os.path.basename(bin_path)}  ({len(data)} bytes)")
    return True


def main():
    ap = argparse.ArgumentParser(description="Convert Quran page .cpp arrays to .bin files")
    ap.add_argument("--src", default="src",    help="Folder containing quran_page_XXXX.cpp files")
    ap.add_argument("--out", default="output", help="Output folder for .bin files (create if needed)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    files = sorted(
        f for f in os.listdir(args.src)
        if f.startswith("quran_page_") and f.endswith(".cpp")
    )

    if not files:
        print(f"No quran_page_XXXX.cpp files found in '{args.src}'")
        return

    print(f"Converting {len(files)} page(s)  {args.src} → {args.out}")
    ok = sum(
        convert(
            os.path.join(args.src, f),
            os.path.join(args.out, f[len("quran_page_"):-len(".cpp")] + ".bin"),
        )
        for f in files
    )
    print(f"Done — {ok}/{len(files)} converted.")
    print(f"\nCopy '{args.out}/' to your SD card root as-is.")
    print("Expected path: SD:/13line/0001.bin  (or /15line/0001.bin)")


if __name__ == "__main__":
    main()
