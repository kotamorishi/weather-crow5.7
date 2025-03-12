from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont
import sys

def ttf_to_c_font(ttf_file, output_file, font_height):
    ttfont = TTFont(ttf_file)
    font = ImageFont.truetype(ttf_file, font_height)

    # Get font metrics with proper scaling
    units_per_em = ttfont['head'].unitsPerEm
    ascent = ttfont['hhea'].ascent * font_height / units_per_em
    descent = abs(ttfont['hhea'].descent * font_height / units_per_em)

    # Calculate baseline position from bottom
    baseline_from_bottom = descent
    total_height = ascent + descent
    scale_factor = font_height / total_height

    # First pass: measure all characters to find the consistent baseline
    test_chars = []
    max_height = 0
    max_ascent = 0

    for codepoint in range(0x20, 0x7F):
        char = chr(codepoint)
        img = Image.new('1', (font_height * 2, font_height * 2), 0)
        draw = ImageDraw.Draw(img)
        draw.text((font_height//2, font_height//2), char, font=font, fill=1, anchor="mm")

        bbox = img.getbbox()
        if bbox:
            left, top, right, bottom = bbox
            height = bottom - top
            max_height = max(max_height, height)
            max_ascent = max(max_ascent, font_height//2 - top)
            test_chars.append((char, top, bottom, height))

    # Calculate the optimal baseline position
    optimal_baseline = int(max_ascent * scale_factor)

    chars = []
    for codepoint in range(0x20, 0x7F):
        char = chr(codepoint)
        img = Image.new('1', (font_height * 2, font_height * 2), 0)
        draw = ImageDraw.Draw(img)

        # Draw text at the calculated baseline position
        draw.text((0, optimal_baseline), char, font=font, fill=1)

        bbox = img.getbbox()
        if not bbox:
            continue

        left, top, right, bottom = bbox
        width = right - left
        height = bottom - top

        # Calculate vertical offset from the optimal baseline
        vertical_offset = optimal_baseline - (top + height)
        vertical_offset = max(-128, min(127, vertical_offset))

        # Rest of the bitmap processing
        bitmap = img.crop((left, top, left + width, top + height))
        pixels = list(bitmap.getdata())
        vertical_offset = top - font_height
        bytes_per_row = (width + 7) // 8
        bitmap_bytes = []

        # print(f'{char} (U+{codepoint:04X}): width={width}, height={height}, vertical_offset={vertical_offset}, baseline={optimal_baseline}, bytes_per_row={bytes_per_row}')
        for y in range(height):
            byte_row = 0
            for x in range(width):
                if pixels[y * width + x]:
                    byte_row |= (1 << (7 - (x % 8)))
                if (x + 1) % 8 == 0 or x == width - 1:
                    bitmap_bytes.append(byte_row)
                    byte_row = 0

        chars.append({
            'char': char,
            'code': codepoint,
            'width': width,
            'height': height,
            'vertical_offset': vertical_offset,
            'horizontal_offset': 0,
            'bytes_per_row': bytes_per_row,
            'bitmap': bitmap_bytes
        })

    with open(output_file, 'w') as f:
        header_guard = f'FONT_{font_height}_H'.upper()
        f.write(f'#ifndef {header_guard}\n')
        f.write(f'#define {header_guard}\n\n')
        f.write('#include <stdint.h>\n')
        f.write('#include "fonts.h">\n\n')

        for char_data in chars:
            f.write(f'// {char_data["char"]} (U+{char_data["code"]:04X})\n')
            f.write(f'const uint8_t font_{font_height}_{ord(char_data["char"]):02x}[] = {{\n')
            for i, byte in enumerate(char_data['bitmap']):
                f.write(f'  0b{byte:08b}' + (',' if (i + 1) % char_data['bytes_per_row'] != 0 else ',\n'))
            f.write('};\n\n')

        for char_data in chars:
            f.write(f'// {char_data["char"]} (U+{char_data["code"]:04X})\n')
            f.write(f'const FontChar char_{font_height}_{ord(char_data["char"]):02x} = {{\n')
            f.write(f'    .char_code = \'{char_data["char"].replace("\\", "\\\\").replace("\'", "\\\'").replace("\"", "\\\"")}\',\n')
            f.write(f'    .width = (uint8_t){char_data["width"]},\n')
            f.write(f'    .height = (uint8_t){char_data["height"]},\n')
            f.write(f'    .vertical_offset = (int8_t){char_data["vertical_offset"]},\n')
            f.write(f'    .horizontal_offset = (int8_t){char_data["horizontal_offset"]},\n')
            f.write(f'    .bytes_per_row = (uint8_t){char_data["bytes_per_row"]},\n')
            f.write(f'    .bitmap = font_{font_height}_{ord(char_data["char"]):02x}\n')
            f.write('};\n\n')
            # print(f'{char_data["char"]} (U+{char_data["code"]:04X}): width={char_data["width"]}, height={char_data["height"]}, vertical_offset={char_data["vertical_offset"]}, bytes_per_row={char_data["bytes_per_row"]}')


        f.write(f'const FontChar * const font_{font_height}_chars[] = {{\n')
        for i, char_data in enumerate(chars):
            f.write(f'    &char_{font_height}_{ord(char_data["char"]):02x}' + (',' if i < len(chars) - 1 else '') + ' \n')
        f.write('};\n\n')
        f.write(f'const FontSet font_{font_height} = {{\n')
        f.write(f'    .height = (uint8_t){font_height},\n')
        f.write(f'    .char_start = 0x21,\n')
        f.write(f'    .char_count = (uint8_t)95,\n')
        space_between_chars = int(font_height * 0.1)
        space_between_chars = max(1, space_between_chars)
        f.write(f'    .space_width = (int8_t){ space_between_chars },\n')
        f.write(f'    .chars = font_{font_height}_chars\n')
        f.write('};\n')

        f.write(f'#endif // {header_guard}\n')

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python script.py <ttf_file> <output_file> <font_height>")
        sys.exit(1)
    ttf_file = sys.argv[1]
    output_file = sys.argv[2]
    font_height = int(sys.argv[3])
    ttf_to_c_font(ttf_file, output_file, font_height)