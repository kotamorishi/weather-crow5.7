import os
import cairosvg
from PIL import Image
from io import BytesIO
from collections import defaultdict

# Define target output image widths
imageSize = [
    (64,'small'),
    (128,'midium'),
    (256,'large')
]

# this scripte will genraate a temporary PNG file which is totall unnessary. just I want to see the output of the image.

class SvgToBmpConverter:
    def __init__(self, width=128, threshold=128):
        self.width = width
        self.threshold = threshold
        # Calculate max_bytes based on width: (width * width / 8) + 4 bytes for header
        self.max_bytes = ((width * width) // 8) + 4
        self.svg_dir = os.path.join(os.path.dirname(__file__), "../svg")
        self.output_file = os.path.join(os.path.dirname(__file__), "weatherIcons.h")
        self.category_base_sizes = {}  # Store base sizes for each category

    def scan_svg_files(self):
        """Scan SVG files and determine base aspect ratios for each category"""
        category_files = defaultdict(list)
        category_dimensions = defaultdict(list)

        # First pass: collect files and analyze dimensions
        for root, dirs, files in os.walk(self.svg_dir):
            category = os.path.basename(root)
            if category == os.path.basename(self.svg_dir):
                continue

            for file in files:
                if file.lower().endswith('.svg'):
                    full_path = os.path.join(root, file)
                    category_files[category].append(full_path)

                    # Analyze original proportions
                    buffer = BytesIO()
                    cairosvg.svg2png(url=full_path, output_width=128*4, write_to=buffer)
                    buffer.seek(0)
                    with Image.open(buffer) as img:
                        bbox = img.getbbox()
                        if bbox:
                            orig_width = bbox[2] - bbox[0]
                            orig_height = bbox[3] - bbox[1]
                            aspect = orig_height / orig_width
                            category_dimensions[category].append((orig_width, orig_height, aspect))

        # Calculate base proportions for each category
        for category, dimensions in category_dimensions.items():
            avg_aspect = sum(d[2] for d in dimensions) / len(dimensions)
            self.category_base_sizes[category] = avg_aspect
            print(f"Category {category} base aspect ratio: {avg_aspect:.2f}")

        return category_files

    def get_category_size(self, category, target_width):
        """Calculate category-specific dimensions for given target width"""
        aspect = self.category_base_sizes[category]
        if aspect > 1:  # Taller than wide
            width = int(target_width / aspect)
            height = target_width
        else:  # Wider than tall
            width = target_width
            height = int(target_width * aspect)

        # Round to nearest multiple of 8
        width = ((width + 7) // 8) * 8
        height = ((height + 7) // 8) * 8
        return width, height

    def convert_svg_to_png(self, svg_path, output_filename, target_width, target_height):
        """Modified to use target width and height"""
        buffer = BytesIO()
        cairosvg.svg2png(url=svg_path, output_width=target_width*4, write_to=buffer)
        buffer.seek(0)

        with Image.open(buffer) as original:
            original = original.convert('RGBA')
            bbox = original.getbbox()
            if bbox:
                cropped = original.crop(bbox)

                # Scale to target size while maintaining aspect ratio within bounds
                crop_width, crop_height = cropped.size
                scale_width = target_width / crop_width
                scale_height = target_height / crop_height
                scale = min(scale_width, scale_height)

                new_width = target_width
                new_height = target_height

                # Resize with padding to maintain aspect ratio
                intermediate = cropped.resize(
                    (int(crop_width * scale), int(crop_height * scale)),
                    Image.Resampling.LANCZOS
                )

                final = Image.new('RGBA', (new_width, new_height), (255, 255, 255, 0))
                paste_x = (new_width - intermediate.width) // 2
                paste_y = 0  # Align to top
                final.paste(intermediate, (paste_x, paste_y), intermediate)
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
        """Generate header content for all sizes and categories"""
        content = []
        category_files = self.scan_svg_files()

        for size_tuple in sorted(imageSize):  # size_tuple is now (width, label)
            size, label = size_tuple  # Unpack the tuple
            content.append(f"\n// === Size variant: {size}px ({label}) ===")
            for category, files in category_files.items():
                target_width, target_height = self.get_category_size(category, size)
                content.append(f"\n// Category: {category} ({target_width}x{target_height})")

                for file_path in files:
                    print(f"Processing {file_path} at {target_width}x{target_height}")
                    self.convert_svg_to_png(file_path, "tmp.png", target_width, target_height)
                    # Get actual dimensions of generated image
                    with Image.open("tmp.png") as img:
                        actual_width, actual_height = img.size
                        print(f"  Actual size: {actual_width}x{actual_height} pixels")

                    arr = self.image_to_bmp_array("tmp.png", debug=False)
                    base_name = os.path.splitext(os.path.basename(file_path))[0].replace('-', '_')
                    array_name = f"{base_name}_{label}"  # Use label instead of width
                    content.append(f"// {array_name}: {actual_width}x{actual_height} pixels")
                    content.append(f"const unsigned char {array_name}[] = {{")

                    width_in_bytes = (target_width + 7) // 8  # Calculate actual bytes per row
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
    converter = SvgToBmpConverter(width=128, threshold=100)
    content = converter.generate_header_content()
    SvgToBmpConverter.generate_header_file(content)
    print("\nGenerated weatherIcons.h with all size variants")
