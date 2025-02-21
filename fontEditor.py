import tkinter as tk
from tkinter import ttk, messagebox  # Added messagebox import
import re

class FontEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("Font Editor")

        # Font data
        self.current_font = []
        self.current_char = 0
        self.cell_size = 20
        self.grid_size = (8, 6)  # 8x6 for ascii_0806

        # Load font data
        self.load_font_data()

        # Create widgets
        self.create_widgets()

    def load_font_data(self):
        try:
            print("Loading font data...")
            # Fix the file path by removing the extra directory level
            with open('EPDfont_orig.h', 'r') as f:
                content = f.read()

                array_start = content.find('ascii_0806[][6]')
                if array_start == -1:
                    raise Exception("Font array not found")

                start_idx = content.find('{', array_start)
                end_idx = content.find('};', start_idx)

                if start_idx != -1 and end_idx != -1:
                    data = content[start_idx+1:end_idx].strip()
                    char_arrays = [x.strip() for x in data.split('},') if x.strip()]
                    self.current_font = []

                    print(f"Font data found : {len(char_arrays)} characters")

                    for char_array in char_arrays:
                        # Extract the hex values part before any comment
                        if '{' in char_array:
                            hex_part = char_array[char_array.find('{')+1:]
                            if '//' in hex_part:
                                hex_part = hex_part.split('//')[0]

                            # Split and clean hex values
                            hex_values = [x.strip() for x in hex_part.split(',') if x.strip() and '0x' in x]

                            if len(hex_values) == 6:  # Only process complete characters
                                try:
                                    bytes_data = [int(x, 16) for x in hex_values]
                                    self.current_font.append(bytes_data)
                                except ValueError as ve:
                                    print(f"Skipping invalid hex values: {hex_values}")
                                    continue

                    print(f"Successfully loaded {len(self.current_font)} characters")
                else:
                    raise Exception("Font data pattern not found in file")

        except Exception as e:
            print(f"Error loading font data: {e}")
            # Initialize empty font with 95 characters (ASCII 32-126)
            self.current_font = [[0x00] * 6 for _ in range(95)]

    def load_char(self):
        # realod specified font data from the file
        self.load_font_data()

        try:
            char = self.char_selector.get()
            if char and len(char) == 1:
                char_code = ord(char)
                if 32 <= char_code <= 126:  # Valid ASCII range
                    self.current_char = char_code - 32
                    if 0 <= self.current_char < len(self.current_font):
                        print(f"Loading character '{char}' (ASCII {char_code}, index {self.current_char})")
                        print(f"Character data: {[f'0x{byte:02X}' for byte in self.current_font[self.current_char]]}")
                        self.draw_grid()
                    else:
                        print(f"Character index {self.current_char} out of range (font has {len(self.current_font)} characters)")
                else:
                    print(f"Character '{char}' is outside valid ASCII range (32-126)")
        except Exception as e:
            print(f"Error loading character: {e}")

    def create_widgets(self):
        # Character selector
        selector_frame = ttk.Frame(self.root)
        selector_frame.pack(pady=10)

        self.char_selector = ttk.Entry(selector_frame, width=5)
        self.char_selector.pack(side=tk.LEFT, padx=5)
        self.char_selector.insert(0, "A")

        ttk.Button(selector_frame, text="Load", command=self.load_char).pack(side=tk.LEFT)
        ttk.Button(selector_frame, text="Save", command=self.save_font).pack(side=tk.LEFT, padx=5)

        # Grid editor
        self.canvas = tk.Canvas(self.root,
                            width=self.grid_size[1]*self.cell_size,   # Use height (6) for width
                            height=self.grid_size[0]*self.cell_size)  # Use width (8) for height
        self.canvas.pack(pady=10, padx=10)

        # Bind mouse events
        self.canvas.bind("<Button-1>", self.toggle_pixel)

        # Draw initial grid
        self.draw_grid()

    def draw_grid(self):
        self.canvas.delete("all")

        # Draw pixels
        char_data = self.current_font[self.current_char] if self.current_char < len(self.current_font) else [0]*6

        for y in range(self.grid_size[1]):
            for x in range(self.grid_size[0]):
                # For 90 degrees counter-clockwise rotation:
                # - new_x = y
                # - new_y = (width-1) - x
                rotated_x = y
                rotated_y = (self.grid_size[0]-1) - x

                # Calculate byte and bit position (using original coordinates for data access)
                byte_pos = y
                bit_pos = 7 - x

                # Check if pixel is set
                if byte_pos < len(char_data):
                    is_set = (char_data[byte_pos] & (1 << bit_pos)) != 0
                else:
                    is_set = False

                # Draw rectangle with rotated coordinates
                color = "black" if is_set else "white"
                self.canvas.create_rectangle(
                    rotated_x*self.cell_size, rotated_y*self.cell_size,
                    (rotated_x+1)*self.cell_size, (rotated_y+1)*self.cell_size,
                    fill=color, outline="gray"
                )

    def toggle_pixel(self, event):
        # Convert click coordinates back to original grid position
        rotated_x = event.x // self.cell_size
        rotated_y = event.y // self.cell_size

        # Convert rotated coordinates back to original positions
        x = (self.grid_size[0]-1) - rotated_y
        y = rotated_x

        if 0 <= x < self.grid_size[0] and 0 <= y < self.grid_size[1]:
            # Calculate byte and bit position
            byte_pos = y
            bit_pos = 7 - x

            # Toggle bit
            if byte_pos < len(self.current_font[self.current_char]):
                self.current_font[self.current_char][byte_pos] ^= (1 << bit_pos)

            self.draw_grid()


    def save_font(self):
        try:
            output_file = 'EPDfont_orig.h'

            # Read existing file content
            with open(output_file, 'r') as f:
                content = f.read()

            # Find the font array in the content - use more flexible pattern matching
            array_pattern = 'ascii_0806'
            array_start = content.find(array_pattern)
            if array_start == -1:
                raise Exception("Font array not found in file")

            # Find the opening brace of the array
            start_idx = content.find('{', array_start)
            if start_idx == -1:
                raise Exception("Array start not found")

            # Find the closing brace of the array
            end_idx = content.find('};', start_idx)
            if end_idx == -1:
                raise Exception("Array end not found")
            end_idx += 2  # Include the };

            # Generate new font data string
            new_font_data = "const unsigned char ascii_0806[][6]={\n"
            for i, char_data in enumerate(self.current_font):
                if i % 4 == 0:  # Add newline every 4 characters for readability
                    new_font_data += "    "
                new_font_data += "{" + ",".join(f"0x{byte:02X}" for byte in char_data) + "},"
                if i % 4 == 3:  # Add newline after every 4th character
                    new_font_data += "\n"
            new_font_data = new_font_data.rstrip(",\n") + "\n};"

            # Replace the old font data with new font data
            new_content = content[:start_idx] + new_font_data[new_font_data.find('{'):] + content[end_idx:]

            # Write the updated content back to file
            with open(output_file, 'w') as f:
                f.write(new_content)

            # Show success message
            tk.messagebox.showinfo("Success", "Font data saved successfully!")

        except Exception as e:
            messagebox.showerror("Error", f"Failed to save font data: {str(e)}")
            print(f"Error saving font: {e}")

# Add this at the end of the file
if __name__ == "__main__":
    root = tk.Tk()
    app = FontEditor(root)
    root.mainloop()