import os
import cairosvg
from PIL import Image
from io import BytesIO

# Define target output image widths
imageSize = [32, 64, 128, 256]

# this scripte will genraate a temporary PNG file which is totall unnessary. just I want to see the output of the image.

class SvgToBmpConverter:
    generatedList = []
    def __init__(self, width=128, threshold=128):
        self.width = width
        self.threshold = threshold
        # Calculate max_bytes based on width: (width * width / 8) + 4 bytes for header
        self.max_bytes = ((width * width) // 8) + 4
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

                # Calculate scaling to fit width while maintaining aspect ratio
                crop_width, crop_height = cropped.size
                scale = self.width / crop_width

                # Calculate new dimensions
                new_width = self.width
                new_height = int(crop_height * scale)

                # Resize the cropped image
                resized = cropped.resize((new_width, new_height), Image.Resampling.LANCZOS)

                # Create output image with the same height as resized image
                final = Image.new('RGBA', (new_width, new_height), (255, 255, 255, 0))
                # Paste at the top (y=0)
                final.paste(resized, (0, 0), resized)
                final.save(output_filename, format="PNG")

    def image_to_bmp_array(self, png_path, debug=False):
        img = Image.open(png_path)
        bg = Image.new("RGB", img.size, (255, 255, 255))
        bg.paste(img, mask=img.split()[3])
        img = bg.convert("L")
        width, height = img.size

        # Calculate max_bytes based on actual dimensions
        self.max_bytes = ((width * height) // 8) + 4

        # Start with width and height as first 4 bytes (little endian)
        bit_array = [
            width & 0xFF,         # width low byte
            (width >> 8) & 0xFF,  # width high byte
            height & 0xFF,        # height low byte
            (height >> 8) & 0xFF  # height high byte
        ]

        # Add line break for better readability after header
        # bit_array.append(0x00)  # Add padding byte for alignment

        for y in range(height):
            row_byte = 0
            bit_count = 0
            for x in range(width):
                val = img.getpixel((x, y))
                pixel = 1 if val < self.threshold else 0
                row_byte = (row_byte << 1) | pixel
                bit_count += 1
                if debug:
                    print(pixel, end="")
                if bit_count == 8:
                    bit_array.append(row_byte)
                    row_byte = 0
                    bit_count = 0
            if debug:
                print()

        # Calculate required padding to reach max_bytes
        if len(bit_array) < self.max_bytes:
            bit_array += [0] * (self.max_bytes - len(bit_array))
        else:
            bit_array = bit_array[:self.max_bytes]
        return bit_array

    def generate_header_content(self):
        """Generate header content for current size without writing to file"""
        content = []
        for f in os.listdir(self.svg_dir):
            if f.lower().endswith(".svg"):
                full_path = os.path.join(self.svg_dir, f)
                print(f"Processing {full_path} at target width {self.width}")
                self.convert_svg_to_png(full_path, "tmp.png")
                # Get actual dimensions of generated image
                with Image.open("tmp.png") as img:
                    actual_width, actual_height = img.size
                    print(f"  Actual size: {actual_width}x{actual_height} pixels")

                arr = self.image_to_bmp_array("tmp.png", debug=False)
                base_name = os.path.splitext(f)[0].replace('-', '_')
                array_name = f"{base_name}_{self.width}"
                content.append(f"// {array_name}: {actual_width}x{actual_height} pixels")
                content.append(f"const unsigned char {array_name}[] = {{")
                self.generatedList.append(array_name + ", ")

                width_in_bytes = (self.width + 7) // 8  # Calculate actual bytes per row
                hex_values = []
                for i, val in enumerate(arr):
                    hex_values.append(f"0x{val:02X}")
                    if i == 3:  # Add extra line break after header
                        content.append("  " + ", ".join(hex_values) + ",  // width, height")
                        hex_values = []
                    elif i > 3 and ((i - 4) % width_in_bytes == width_in_bytes - 1):  # Align with actual image width
                        content.append("  " + ", ".join(hex_values) + ",")
                        hex_values = []
                if hex_values:
                    content.append("  " + ", ".join(hex_values) + ",")
                content.append("};\n")


        return "\n".join(content)

    @staticmethod
    def generate_header_file(all_content):
        """Write complete header file with all sizes"""
        output_file = os.path.join(os.path.dirname(__file__), "weatherIcons.h")
        with open(output_file, "w") as out:
            guard_name = "WEATHER_ICONS_H"
            out.write("// weatherIcons.h\n")
            out.write(f"#ifndef {guard_name}\n")
            out.write(f"#define {guard_name}\n\n")
            out.write("#ifdef __cplusplus\n")
            out.write('extern "C" {\n')
            out.write("#endif\n\n")

            out.write("// Structure of the image\n")
            out.write("//  uint16_t width; // firtst byte is width\n")
            out.write("//  uint16_t height; // second byte is height\n")
            out.write("//  const unsigned char *data; vary\n")
            out.write("//};\n\n")

            # Write all size variants
            out.write(all_content)

            # Close guards
            out.write("#ifdef __cplusplus\n")
            out.write("}\n")
            out.write("#endif\n\n")
            out.write(f"#endif /* {guard_name} */\n")


if __name__ == "__main__":
    all_content = []
    for size in sorted(imageSize):  # Sort sizes for consistent order
        print(f"\nGenerating {size}x{size} icons...")
        converter = SvgToBmpConverter(width=size, threshold=100)
        content = converter.generate_header_content()
        all_content.append(content)

    # Generate single header file with all sizes
    SvgToBmpConverter.generate_header_file("\n".join(all_content))
    print(f"\nGenerated weatherIcons.h with all size variants")
