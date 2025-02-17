import os
import cairosvg
from PIL import Image
from io import BytesIO

class SvgToBmpConverter:
    def __init__(self, width=128, threshold=128, max_bytes=2048):
        self.width = width
        self.threshold = threshold
        self.max_bytes = max_bytes
        self.svg_dir = os.path.join(os.path.dirname(__file__), "svg")
        self.output_file = os.path.join(os.path.dirname(__file__), "weatherIcons.h")

    def convert_svg_to_png(self, svg_path, output_filename):
        # First render large enough to get good quality
        buffer = BytesIO()
        cairosvg.svg2png(url=svg_path, output_width=self.width*4, write_to=buffer)
        buffer.seek(0)

        with Image.open(buffer) as original:
            # Convert to RGBA if not already
            original = original.convert('RGBA')

            # Get the bounding box of actual content
            bbox = original.getbbox()
            if bbox:
                # Crop to content
                cropped = original.crop(bbox)

                # Calculate scaling to fit within bounds while maintaining aspect ratio
                crop_width, crop_height = cropped.size
                scale_width = self.width / crop_width
                scale_height = self.width / crop_height
                # Use minimum scale to ensure it fits in both dimensions
                scale = min(scale_width, scale_height)

                # Calculate new dimensions
                new_width = int(crop_width * scale)
                new_height = int(crop_height * scale)

                # Resize the cropped image
                resized = cropped.resize((new_width, new_height), Image.Resampling.LANCZOS)

                # Create output image and center the resized content
                final = Image.new('RGBA', (self.width, self.width), (255, 255, 255, 0))
                paste_x = (self.width - new_width) // 2
                paste_y = (self.width - new_height) // 2
                final.paste(resized, (paste_x, paste_y), resized)
                final.save(output_filename, format="PNG")

    def image_to_bmp_array(self, png_path):
        img = Image.open(png_path)
        bg = Image.new("RGB", img.size, (255, 255, 255))
        bg.paste(img, mask=img.split()[3])
        img = bg.convert("L")
        width, height = img.size
        bit_array = []
        for y in range(height):
            row_byte = 0
            bit_count = 0
            for x in range(width):
                val = img.getpixel((x, y))
                pixel = 1 if val < self.threshold else 0
                row_byte = (row_byte << 1) | pixel
                print(pixel, end="")
                bit_count += 1
                if bit_count == 8:
                    bit_array.append(row_byte)
                    row_byte = 0
                    bit_count = 0
            print()
        if len(bit_array) < self.max_bytes:
            bit_array += [0] * (self.max_bytes - len(bit_array))
        else:
            bit_array = bit_array[:self.max_bytes]
        print()
        return bit_array

    def generate_header_file(self):
        with open(self.output_file, "w") as out:
            # Add header guards
            guard_name = "WEATHER_ICONS_H"
            out.write("// weatherIcons.h\n")
            out.write(f"#ifndef {guard_name}\n")
            out.write(f"#define {guard_name}\n\n")
            out.write("#ifdef __cplusplus\n")
            out.write('extern "C" {\n')
            out.write("#endif\n\n")

            # Write bitmap arrays
            for f in os.listdir(self.svg_dir):
                if f.lower().endswith(".svg"):
                    full_path = os.path.join(self.svg_dir, f)
                    print(f"Processing {full_path}")
                    self.convert_svg_to_png(full_path, "tmp.png")
                    arr = self.image_to_bmp_array("tmp.png")
                    array_name = os.path.splitext(f)[0].replace('-', '_')
                    out.write(f"const unsigned char {array_name}[] = {{\n")
                    width_in_bytes = 16
                    for i, val in enumerate(arr):
                        out.write(f"0x{val:02X}, ")
                        if (i + 1) % width_in_bytes == 0:
                            out.write("\n")
                    out.write("};\n\n")

            # Close header guards
            out.write("#ifdef __cplusplus\n")
            out.write("}\n")
            out.write("#endif\n\n")
            out.write(f"#endif /* {guard_name} */\n")

if __name__ == "__main__":
    converter = SvgToBmpConverter(width=128, threshold=100)
    converter.generate_header_file()
