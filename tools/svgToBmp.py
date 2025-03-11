import os
import cairosvg
from PIL import Image
from io import BytesIO
from collections import defaultdict
import json
import argparse

# Define default image sizes for fallback
DEFAULT_IMAGE_SIZES = [
    (64,'sm'),
    (128,'md'),
    (200,'lg')
]

# Default config file path
CONFIG_FILE_PATH = os.path.join(os.path.dirname(__file__), "svgToBmp_config.json")

def load_or_create_config():
    """Load config from file or create default config file if not exists"""
    if os.path.exists(CONFIG_FILE_PATH):
        try:
            with open(CONFIG_FILE_PATH, 'r') as f:
                config = json.load(f)
                print(f"Loaded configuration from {CONFIG_FILE_PATH}")
                return config
        except json.JSONDecodeError:
            print(f"Error parsing config file {CONFIG_FILE_PATH}. Using defaults.")

    # Create default config with new structure
    default_config = {
        "categories": [
            {
                "category": "weather",
                "sizes": [
                    {"width": 64, "label": "sm"},
                    {"width": 128, "label": "md"},
                    {"width": 200, "label": "lg"}
                ]
            }
        ],
        "threshold": 100
    }

    # Save default config
    with open(CONFIG_FILE_PATH, 'w') as f:
        json.dump(default_config, f, indent=2)
        print(f"Created default configuration file at {CONFIG_FILE_PATH}")

    return default_config

class SvgToBmpConverter:
    def __init__(self, threshold=128, config=None):
        self.threshold = threshold
        self.svg_dir = os.path.join(os.path.dirname(__file__), "../svg")
        self.output_file = os.path.join(os.path.dirname(__file__), "../weatherCrow5.7/weatherIcons.h")
        self.category_base_sizes = {}  # Store base sizes for each category
        self.generated_icons = []  # Track all generated icon names

        # Initialize from config if provided
        self.config = config or {}
        self.category_configs = {cfg["category"]: cfg for cfg in self.config.get("categories", [])}

    def scan_svg_files(self):
        """Two-pass scan: first determine category dimensions, then scale consistently"""
        category_files = defaultdict(list)
        category_dimensions = defaultdict(list)

        print("\n=== First pass: Analyzing category dimensions ===")
        # First pass: collect files and analyze dimensions
        for root, dirs, files in os.walk(self.svg_dir):
            category = os.path.basename(root)
            if category == os.path.basename(self.svg_dir):
                continue

            # Skip categories not in config
            if category not in self.category_configs:
                print(f"Skipping category '{category}' - not in configuration")
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
                            file_name = os.path.basename(full_path)
                            print(f"  {category}/{file_name}: {orig_width}x{orig_height}, aspect: {aspect:.2f}")
                            category_dimensions[category].append({
                                'file': file_name,
                                'width': orig_width,
                                'height': orig_height,
                                'aspect': aspect
                            })

        # Calculate category metrics
        print("\n=== Category scaling analysis ===")
        for category, dimensions in category_dimensions.items():
            if dimensions:
                # Calculate average and max dimensions
                avg_width = sum(d['width'] for d in dimensions) / len(dimensions)
                avg_height = sum(d['height'] for d in dimensions) / len(dimensions)
                avg_aspect = sum(d['aspect'] for d in dimensions) / len(dimensions)
                max_width = max(d['width'] for d in dimensions)
                max_height = max(d['height'] for d in dimensions)

                # Store category metrics
                self.category_base_sizes[category] = {
                    'avg_aspect': avg_aspect,
                    'avg_width': avg_width,
                    'avg_height': avg_height,
                    'max_width': max_width,
                    'max_height': max_height,
                    'files': len(dimensions)
                }

                # Print category summary
                print(f"Category '{category}' ({len(dimensions)} files):")
                print(f"  Average dimensions: {avg_width:.1f}x{avg_height:.1f} px")
                print(f"  Max dimensions: {max_width}x{max_height} px")
                print(f"  Average aspect ratio: {avg_aspect:.2f}")

                # Find outliers for warning
                aspect_threshold = 0.2  # 20% deviation threshold
                outliers = [d for d in dimensions if
                           abs(d['aspect'] - avg_aspect) / avg_aspect > aspect_threshold]
                if outliers:
                    print(f"  Warning: {len(outliers)} files have significantly different aspect ratios:")
                    for o in outliers[:3]:  # Show first 3 outliers
                        print(f"    {o['file']}: aspect {o['aspect']:.2f} " +
                             f"(differs by {abs(o['aspect'] - avg_aspect) / avg_aspect * 100:.1f}%)")
                    if len(outliers) > 3:
                        print(f"    ... and {len(outliers) - 3} more")

        return category_files

    def get_category_size(self, category, target_width):
        """Calculate category-specific dimensions for given target width"""
        if category not in self.category_base_sizes:
            print(f"Warning: No metrics for category '{category}'. Using default aspect ratio.")
            aspect = 1.0
        else:
            metrics = self.category_base_sizes[category]
            aspect = metrics['avg_aspect']
            print(f"Using category '{category}' aspect ratio: {aspect:.2f}")

        if aspect > 1:  # Taller than wide
            width = int(target_width / aspect)
            height = target_width
        else:  # Wider than tall or square
            width = target_width
            height = int(target_width * aspect)

        # Round to nearest multiple of 8
        width = ((width + 7) // 8) * 8
        height = ((height + 7) // 8) * 8

        print(f"Target dimensions for category '{category}': {width}x{height} px")
        return width, height

    def convert_svg_to_png(self, svg_path, output_filename, target_width, target_height, custom_scale=None):
        """Modified to support custom scaling factor from config"""
        category = os.path.basename(os.path.dirname(svg_path))
        buffer = BytesIO()

        # Use high resolution for initial rendering
        cairosvg.svg2png(url=svg_path, output_width=target_width*4, write_to=buffer)
        buffer.seek(0)

        with Image.open(buffer) as original:
            original = original.convert('RGBA')
            bbox = original.getbbox()
            if bbox:
                cropped = original.crop(bbox)

                # If custom scale is provided in the config, use it directly
                if custom_scale is not None:
                    category_scale = custom_scale
                    print(f"Using custom scale: {category_scale} for {os.path.basename(svg_path)}")
                else:
                    # Otherwise use the category-based scaling calculation
                    if category in self.category_base_sizes:
                        metrics = self.category_base_sizes[category]
                        # Use the category's average dimensions as reference
                        ref_width = metrics['avg_width']
                        ref_height = metrics['avg_height']

                        # Calculate a consistent scaling factor based on category's average dimensions
                        if metrics['avg_aspect'] > 1:  # Taller than wide
                            category_scale = target_height / ref_height
                        else:  # Wider than tall or square
                            category_scale = target_width / ref_width
                    else:
                        # Fallback if no category metrics
                        print(f"Warning: No metrics for category '{category}'. Using default scaling.")
                        ref_width = cropped.width
                        ref_height = cropped.height
                        category_scale = min(target_width / ref_width, target_height / ref_height)

                # Apply the scaling to this image
                new_width = int(cropped.width * category_scale)
                new_height = int(cropped.height * category_scale)

                # Resize with the consistent category scale factor
                scaled = cropped.resize(
                    (new_width, new_height),
                    Image.Resampling.LANCZOS
                )

                # Center the scaled image on the target canvas
                final = Image.new('RGBA', (target_width, target_height), (255, 255, 255, 0))
                paste_x = (target_width - new_width) // 2
                paste_y = (target_height - new_height) // 2
                final.paste(scaled, (paste_x, paste_y), scaled)
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
        """Generate header content for categories and their specified sizes"""
        content = []
        self.generated_icons = []  # Reset the icon list
        category_files = self.scan_svg_files()

        # Process each category with its own size configuration
        for category, files in category_files.items():
            category_config = self.category_configs.get(category, {})
            sizes = category_config.get("sizes", [])

            # Skip empty categories or those without size configs
            if not files or not sizes:
                continue

            content.append(f"\n// === Category: {category} ===")

            # Process each size for this category
            for size_config in sizes:
                size = size_config.get("width", 128)
                label = size_config.get("label", "md")
                custom_scale = size_config.get("scale", None)  # Get custom scale if provided

                if custom_scale is not None:
                    print(f"Using custom scale factor: {custom_scale} for {category}/{label}")

                target_width, target_height = self.get_category_size(category, size)
                content.append(f"\n// Size variant: {size}px ({label}) - {target_width}x{target_height}")

                for file_path in files:
                    print(f"Processing {file_path} at {target_width}x{target_height}")
                    # Pass the custom_scale to the convert function
                    self.convert_svg_to_png(file_path, "tmp.png", target_width, target_height, custom_scale)
                    # Get actual dimensions of generated image
                    with Image.open("tmp.png") as img:
                        actual_width, actual_height = img.size

                    arr = self.image_to_bmp_array("tmp.png", debug=False)
                    base_name = os.path.splitext(os.path.basename(file_path))[0].replace('-', '_')
                    array_name = f"{base_name}_{label}"  # Use label instead of width
                    icon_key = f"{base_name}_{label}"  # Create key for the icon map
                    self.generated_icons.append((icon_key, array_name))  # Store for map generation

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

    def generate_icon_map(self):
        """Generate C++ map code for icon lookup"""
        map_content = []
        map_content.append("\n// Icon mapping for convenient lookup")
        map_content.append("std::map<std::string, const unsigned char*> icon_map = {")

        # Add each icon to the map with proper indentation
        for icon_key, array_name in self.generated_icons:
            map_content.append(f"  {{\"{icon_key}\", {array_name}}},")

        # Remove trailing comma from the last entry if there are any entries
        if self.generated_icons:
            map_content[-1] = map_content[-1][:-1]  # Remove trailing comma

        map_content.append("};")
        return "\n".join(map_content)

    def convert_and_save(self):
        """Convert SVGs and save to the configured output file"""
        content = self.generate_header_content()
        icon_map = self.generate_icon_map()
        self.generate_header_file(content, icon_map, self.output_file)
        print(f"\nGenerated header file at: {self.output_file}")

    def generate_header_file(self, all_content, icon_map_content, output_path):
        """Write complete header file with all sizes and icon map"""
        # Changed from static method to instance method
        # Use the provided output path parameter
        with open(output_path, "w") as out:
            guard_name = "WEATHER_ICONS_H"
            out.write("// weatherIcons.h\n")
            out.write(f"#ifndef {guard_name}\n")
            out.write(f"#define {guard_name}\n\n")
            out.write("#include <map>\n")
            out.write("#include <string>\n\n")
            out.write("#ifdef __cplusplus\n")
            out.write('extern "C" {\n')
            out.write("#endif\n\n")

            out.write("// Structure of the image\n")
            out.write("//  uint16_t width; // first byte is width\n")
            out.write("//  uint16_t height; // second byte is height\n")
            out.write("//  const unsigned char *data; vary\n")
            out.write("//};\n\n")

            # Write all size variants
            out.write(all_content)

            # Write icon map
            out.write(icon_map_content)

            # Close guards
            out.write("\n#ifdef __cplusplus\n")
            out.write("}\n")
            out.write("#endif\n\n")
            out.write(f"#endif /* {guard_name} */\n")


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description='Convert SVG files to BMP arrays in C++ header format.')
    parser.add_argument('--config', type=str, default=CONFIG_FILE_PATH,
                        help=f'Path to config file (default: {CONFIG_FILE_PATH})')
    return parser.parse_args()

if __name__ == "__main__":
    args = parse_arguments()

    # Load or create config file
    config = load_or_create_config()

    # Show configuration summary
    print(f"Using configuration:")
    print(f"- Threshold: {config.get('threshold', 100)}")
    print(f"- Categories:")
    for category_cfg in config.get("categories", []):
        cat_name = category_cfg.get("category", "unknown")
        sizes_info = [f"{s.get('width', 'unknown')}px ({s.get('label', 'unknown')})"
                      for s in category_cfg.get("sizes", [])]
        print(f"  - {cat_name}: {', '.join(sizes_info)}")

    converter = SvgToBmpConverter(
        threshold=config.get("threshold", 100),
        config=config
    )
    converter.convert_and_save()
    print("\nGenerated weatherIcons.h with category-specific size configurations")
