import binascii
import os
import sys

def bin_to_c_array(bin_file, array_name):
    with open(bin_file, 'rb') as f:
        data = f.read()
    hex_data = binascii.hexlify(data).decode('utf-8')  # decode to string
    c_array = f'const unsigned char {array_name}[] = {{\n'
    for i in range(0, len(hex_data), 2):
        c_array += f'  0x{hex_data[i:i+2]},'
        if (i + 2) % 16 == 0 and i + 2 != len(hex_data):  # add newline every 8 bytes
            c_array += '\n'
        elif i + 2 == len(hex_data):
            c_array = c_array.rstrip(',') + '\n'
    c_array += '};\n'
    return c_array

def main():
    if len(sys.argv) != 2:
        print("Usage: python script.py <input_bin_file>")
        sys.exit(1)

    input_bin_file = sys.argv[1]
    if not os.path.isfile(input_bin_file):
        print("Error: Input file not found.")
        sys.exit(1)

    if not input_bin_file.endswith('.bin'):
        print("Error: Input file must be a .bin file.")
        sys.exit(1)

    output_c_file = os.path.splitext(input_bin_file)[0] + '.c'
    array_name = os.path.splitext(os.path.basename(input_bin_file))[0]

    c_array = bin_to_c_array(input_bin_file, array_name)

    with open(output_c_file, 'w') as f:
        f.write(c_array)

    print(f"Successfully generated {output_c_file}")

if __name__ == "__main__":
    main()