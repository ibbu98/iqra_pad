import sys, os

def bin_to_c_array(bin_path, var_name, out_path):
    with open(bin_path, "rb") as f:
        data = f.read()

    # GxEPD2 usually expects 0=white 1=black for drawBitmap usage,
    # but our bin might be opposite depending on your converter.
    # If your display shows inverted colors, enable invert=True below.
    invert = True
    if invert:
        data = bytes([b ^ 0xFF for b in data])

    with open(out_path, "w", newline="\n") as out:
        out.write('#include "quran_pages.h"\n\n')
        out.write(f"const uint8_t {var_name}[] PROGMEM = {{\n")

        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            out.write("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",\n")

        out.write("};\n")

if __name__ == "__main__":
    # Usage:
    # py bin_to_c_array.py page_0001.bin page_0001 quran_page_0001.cpp
    if len(sys.argv) != 4:
        print("Usage: py bin_to_c_array.py <input.bin> <var_name> <output.cpp>")
        sys.exit(1)

    bin_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])
    print("Wrote:", sys.argv[3])
