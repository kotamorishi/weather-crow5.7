import os
import cairosvg
from PIL import Image, ImageDraw, ImageFont
from io import BytesIO
from collections import defaultdict
import json
import argparse
import math

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
        self.supported_extensions = ['.svg', '.png']  # Add PNG support

        # Initialize from config if provided
        self.config = config or {}
        self.category_configs = {cfg["category"]: cfg for cfg in self.config.get("categories", [])}

    def scan_image_files(self):
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
                file_lower = file.lower()
                if any(file_lower.endswith(ext) for ext in self.supported_extensions):
                    full_path = os.path.join(root, file)
                    category_files[category].append(full_path)

                    # Analyze original proportions
                    buffer = BytesIO()
                    file_ext = os.path.splitext(file_lower)[1]

                    if file_ext == '.svg':
                        cairosvg.svg2png(url=full_path, output_width=128*4, write_to=buffer)
                    else:  # PNG file
                        with Image.open(full_path) as img:
                            img.save(buffer, format="PNG")

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

    def convert_image_to_png(self, image_path, output_filename, target_width, target_height, custom_scale=None):
        """Convert either SVG or PNG to the target PNG"""
        category = os.path.basename(os.path.dirname(image_path))
        buffer = BytesIO()
        file_ext = os.path.splitext(image_path.lower())[1]

        # Handle file based on its extension
        if file_ext == '.svg':
            # SVG conversion path
            cairosvg.svg2png(url=image_path, output_width=target_width*4, write_to=buffer)
        else:  # PNG path
            with Image.open(image_path) as img:
                img.save(buffer, format="PNG")

        buffer.seek(0)

        with Image.open(buffer) as original:
            original = original.convert('RGBA')
            bbox = original.getbbox()
            if bbox:
                cropped = original.crop(bbox)

                # If custom scale is provided in the config, use it directly
                if custom_scale is not None:
                    category_scale = custom_scale
                    print(f"Using custom scale: {category_scale} for {os.path.basename(image_path)}")
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
        category_files = self.scan_image_files()

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
                    # Use the new method that handles both SVG and PNG
                    self.convert_image_to_png(file_path, "tmp.png", target_width, target_height, custom_scale)
                    # Get actual dimensions of generated image
                    with Image.open("tmp.png") as img:
                        actual_width, actual_height = img.size

                    # Get the original filename without extension
                    base_name_original = os.path.splitext(os.path.basename(file_path))[0]
                    # Get the C-compatible name (replace hyphens with underscores)
                    base_name = base_name_original.replace('-', '_')

                    arr = self.image_to_bmp_array("tmp.png", debug=False)
                    array_name = f"{base_name}_{label}"  # Use label instead of width
                    icon_key = f"{base_name}_{label}"  # Create key for the icon map

                    # Store both the C array name and the original filename
                    self.generated_icons.append((icon_key, array_name, base_name_original, category))

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
        for icon_key, array_name, orig_name, category in self.generated_icons:
            map_content.append(f"  {{\"{icon_key}\", {array_name}}},")

        # Remove trailing comma from the last entry if there are any entries
        if self.generated_icons:
            map_content[-1] = map_content[-1][:-1]  # Remove trailing comma

        map_content.append("};")
        return "\n".join(map_content)

    def generate_category_previews(self):
        """Generate a single preview image showing all categories and sizes"""
        print("\n=== Generating comprehensive preview image ===")

        # Background color (blue #51709C)
        bg_color = (81, 112, 156)
        header_color = (45, 65, 95)  # Darker blue for section headers

        # First scan and collect information about all categories
        category_files_map = self.scan_image_files()

        print(f"Found {len(category_files_map)} categories with SVGs: {list(category_files_map.keys())}")

        # Structure to hold all preview data
        preview_sections = []

        # Calculate total height and maximum width needed
        total_height = 50  # Start with space for title
        max_width = 800  # Minimum width
        max_icons_per_row = 8  # Maximum icons per row for any section

        # Get all categories from configuration
        all_categories = list(self.category_configs.keys())
        print(f"Configured categories: {all_categories}")

        # Check if generated icons exist
        if not self.generated_icons:
            print("WARNING: No icons have been generated yet. Running header generation first.")
            self.generate_header_content()  # Generate icons if not already done

        print(f"Total generated icons: {len(self.generated_icons)}")
        # Group icons by category and size label directly using the stored category
        icons_by_category_size = {}
        for icon_key, array_name, orig_name, category in self.generated_icons:
            # Extract size label from icon_key
            parts = icon_key.rsplit('_', 1)
            if len(parts) == 2:
                base_name, size_label = parts

                # Use the stored category directly
                key = (category, size_label)
                if key not in icons_by_category_size:
                    icons_by_category_size[key] = []

                # Store the original name for display
                icons_by_category_size[key].append((orig_name, icon_key, array_name))

        # Print debug info about icons by category
        print("\nIcons by category and size:")
        for (cat, size_label), icons in icons_by_category_size.items():
            print(f"  {cat}/{size_label}: {len(icons)} icons")
            # Print first few icon names for debugging
            for i, (orig_name, icon_key, array_name) in enumerate(icons[:3]):
                print(f"    - {orig_name} -> {icon_key}")
            if len(icons) > 3:
                print(f"    - ... and {len(icons) - 3} more")

        # Process all configured categories and their sizes
        for category, config in self.category_configs.items():
            # Get category files
            category_files = category_files_map.get(category, [])
            if not category_files:
                print(f"No SVG files found for category '{category}', skipping")
                continue

            # Get configured sizes for this category
            sizes = config.get("sizes", [])
            if not sizes:
                print(f"No size configurations for category '{category}', skipping")
                continue

            # Process each size
            for size_config in sizes:
                size = size_config.get("width", 128)
                label = size_config.get("label", "md")

                # Get the target dimensions
                target_width, target_height = self.get_category_size(category, size)

                # Get icons for this category and size
                category_icons = icons_by_category_size.get((category, label), [])

                if not category_icons:
                    print(f"No icons found for {category}/{label}, skipping this section")
                    continue

                print(f"Adding section for {category}/{label} with {len(category_icons)} icons")

                # Calculate grid dimensions
                icon_count = len(category_icons)
                grid_columns = min(max_icons_per_row, icon_count)
                grid_rows = (icon_count + grid_columns - 1) // grid_columns

                section_width = grid_columns * (target_width + 20)
                section_height = grid_rows * (target_height + 60) + 60  # Space for section header

                max_width = max(max_width, section_width + 40)  # Add margin

                # Store section info for later rendering
                preview_sections.append({
                    'category': category,
                    'size': size,
                    'label': label,
                    'target_width': target_width,
                    'target_height': target_height,
                    'icons': category_icons,
                    'grid_columns': grid_columns,
                    'grid_rows': grid_rows,
                    'section_height': section_height,
                    'y_offset': total_height
                })

                total_height += section_height + 20  # Add margin between sections

        print(f"Created {len(preview_sections)} preview sections")

        if not preview_sections:
            print("No preview sections to render. Check your SVG files and configuration.")
            return

        # Create the single large preview canvas
        preview = Image.new('RGB', (max_width, total_height + 50), bg_color)
        draw = ImageDraw.Draw(preview)

        # Add main title
        try:
            font_title = ImageFont.truetype("Arial", 30)
            draw.text((20, 10), "Weather Icon Preview - All Categories and Sizes",
                     fill="white", font=font_title)
        except Exception as e:
            print(f"Font error: {e}")
            draw.text((20, 10), "Weather Icon Preview - All Categories and Sizes",
                     fill="white")

        # Render each section
        for section in preview_sections:
            # Draw section header background
            draw.rectangle(
                [(10, section['y_offset']),
                 (max_width - 10, section['y_offset'] + 40)],
                fill=header_color
            )

            # Draw section header text
            try:
                font_header = ImageFont.truetype("Arial", 24)
                draw.text(
                    (20, section['y_offset'] + 5),
                    f"Category: {section['category']}, Size: {section['size']}px ({section['label']})",
                    fill="white", font=font_header
                )
            except:
                draw.text(
                    (20, section['y_offset'] + 5),
                    f"Category: {section['category']}, Size: {section['size']}px ({section['label']})",
                    fill="white"
                )

            # Place each icon in this section
            x, y = 20, section['y_offset'] + 50

            # Get the custom scale for this specific size configuration
            category_config = self.category_configs.get(section['category'], {})
            sizes = category_config.get("sizes", [])
            custom_scale = None
            for size_cfg in sizes:
                if size_cfg.get("label") == section['label']:
                    custom_scale = size_cfg.get("scale")
                    break

            if custom_scale is not None:
                print(f"Using custom scale {custom_scale} for {section['category']}/{section['label']}")

            for i, (orig_name, icon_key, array_name) in enumerate(section['icons']):
                # Find the file with supported extensions
                found_file = None
                for ext in self.supported_extensions:
                    test_file = os.path.join(self.svg_dir, section['category'], f"{orig_name}{ext}")
                    if os.path.exists(test_file):
                        found_file = test_file
                        break

                if found_file:
                    # Generate the PNG temporarily
                    temp_png = f"temp_{orig_name}_{section['label']}.png"
                    self.convert_image_to_png(
                        found_file, temp_png,
                        section['target_width'], section['target_height'],
                        custom_scale
                    )

                    # Open and paste the icon
                    try:
                        with Image.open(temp_png) as icon_img:
                            # Draw a light border to see icon boundaries
                            icon_with_border = Image.new('RGBA', icon_img.size, (255, 255, 255, 30))
                            icon_with_border.paste(icon_img, (0, 0), icon_img.split()[3])
                            preview.paste(icon_with_border, (x, y), icon_with_border.split()[3])

                            # Add icon name below using the original filename
                            try:
                                font_sm = ImageFont.truetype("Arial", 12)
                                draw.text((x, y + section['target_height'] + 5), orig_name,
                                         fill="white", font=font_sm)
                            except:
                                draw.text((x, y + section['target_height'] + 5), orig_name,
                                         fill="white")

                            # Remove temp file
                            os.remove(temp_png)
                    except Exception as e:
                        print(f"Error processing icon {orig_name}: {e}")
                else:
                    print(f"Warning: No image file found for {orig_name} in {section['category']}")

                # Move to next position
                x += section['target_width'] + 20
                if (i + 1) % section['grid_columns'] == 0:
                    x = 20
                    y += section['target_height'] + 60

        # Save the comprehensive preview image
        preview_dir = os.path.join(os.path.dirname(__file__), "previews")
        os.makedirs(preview_dir, exist_ok=True)
        preview_file = os.path.join(preview_dir, "weather_icons_complete_preview.png")
        preview.save(preview_file)
        print(f"Saved comprehensive preview image: {preview_file}")

    def convert_and_save(self):
        """Convert SVGs and save to the configured output file"""
        content = self.generate_header_content()
        icon_map = self.generate_icon_map()
        self.generate_header_file(content, icon_map, self.output_file)

        # Generate preview images after header generation
        self.generate_category_previews()

        print(f"\nGenerated header file at: {self.output_file}")
        print(f"Preview images saved in {os.path.join(os.path.dirname(__file__), 'previews')} directory")

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
    print("Supported file formats: SVG, PNG")
