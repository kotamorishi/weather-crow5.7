from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont
import sys

def ttf_to_c_font(ttf_file, output_file, font_height):
    # TTFフォントを読み込む
    ttfont = TTFont(ttf_file)
    font = ImageFont.truetype(ttf_file, font_height)

    chars = []
    for codepoint in range(0x20, 0x7F):
        # 文字をレンダリング
        char = chr(codepoint)
        img = Image.new('1', (font_height * 2, font_height * 2), 0)  # 余裕を持たせる
        draw = ImageDraw.Draw(img)
        draw.text((0, 0), char, font=font, fill=1)

        # バウンディングボックスを取得して幅を確定
        bbox = img.getbbox()
        if not bbox:
            continue
        left, top, right, bottom = bbox
        width = right - left
        height = bottom - top

        if height > font_height:
            height = font_height  # 高さを制限

        # ビットマップを切り出し
        bitmap = img.crop((left, top, left + width, top + height))
        pixels = list(bitmap.getdata())
        bytes_per_row = (width + 7) // 8
        bitmap_bytes = []

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
            'bytes_per_row': bytes_per_row,
            'bitmap': bitmap_bytes
        })

    # Cコード生成（BDFとほぼ同じ）
    with open(output_file, 'w') as f:
        header_guard = f'FONT_{font_height}_H'.upper()
        f.write(f'#ifndef {header_guard}\n')
        f.write(f'#define {header_guard}\n\n')
        f.write('#include <stdint.h>\n')
        f.write('#include "fonts.h">\n\n')
        # f.write('typedef struct {\n')
        # f.write('    uint8_t width;\n')
        # f.write('    uint8_t bytes_per_row;\n')
        # f.write('    const uint8_t *bitmap;\n')
        # f.write('} FontChar;\n\n')
        # f.write('typedef struct {\n')
        # f.write('    uint8_t height;\n')
        # f.write('    uint8_t char_start;\n')
        # f.write('    uint8_t char_count;\n')
        # f.write('    const FontChar *chars;\n')
        # f.write('} FontSet;\n\n')

        for char_data in chars:
            f.write(f'// {char_data["char"]} (U+{char_data["code"]:04X})\n')
            f.write(f'const uint8_t font_{font_height}_{ord(char_data["char"]):02x}[] = {{\n')
            for i, byte in enumerate(char_data['bitmap']):
                f.write(f'    0b{byte:08b}' + (',' if i < len(char_data['bitmap']) - 1 else '') + '\n')
            f.write('};\n\n')

        for char_data in chars:
            f.write(f'// {char_data["char"]} (U+{char_data["code"]:04X})\n')
            f.write(f'const FontChar char_{font_height}_{ord(char_data["char"]):02x} = {{\n')
            f.write(f'    .char_code = \'{char_data["char"].replace("\\", "\\\\").replace("\'", "\\\'").replace("\"", "\\\"")}\',\n')
            f.write(f'    .width = (uint8_t){char_data["width"]},\n')
            f.write(f'    .bytes_per_row = (uint8_t){char_data["bytes_per_row"]},\n')
            f.write(f'    .bitmap = font_{font_height}_{ord(char_data["char"]):02x}\n')
            f.write('};\n\n')

        f.write(f'const FontChar * const font_{font_height}_chars[] = {{\n')
        for i, char_data in enumerate(chars):
            # f.write(f'    &char_{font_height}_{ord(char_data["char"]):02x}' + (',' if i < len(chars) - 1 else '') +  + '\n')
            f.write(f'    &char_{font_height}_{ord(char_data["char"]):02x}' + (',' if i < len(chars) - 1 else '') + ' // ' + char_data["char"].replace("\\", "\\\\").replace("\'", "\\\'").replace("\"", "\\\"") + '\n')
        f.write('};\n\n')
        f.write(f'const FontSet font_{font_height} = {{\n')
        f.write(f'    .height = (uint8_t){font_height},\n')
        f.write(f'    .char_start = 0x20,\n')
        f.write(f'    .char_count = (uint8_t)95,\n')
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