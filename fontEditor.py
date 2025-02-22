import tkinter as tk
from tkinter import ttk, messagebox
import re

class FontEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("Font Editor")

        # Font data
        self.font_groups = {
            'ascii_0806': {'data': [], 'dataSize': (8, 6), 'gridSize': (8, 6), 'blockSize': 1},
            'ascii_1206': {'data': [], 'dataSize': (12, 12), 'gridSize': (12, 6), 'blockSize': 2},
            'ascii_1608': {'data': [], 'dataSize': (16, 16), 'gridSize': (16, 8), 'blockSize': 2},
            'ascii_2412': {'data': [], 'dataSize': (36, 36), 'gridSize': (24, 12), 'blockSize': 2},
            'ascii_4824': {'data': [], 'dataSize': (48, 144), 'gridSize': (48, 24), 'blockSize': 3},
        }
        self.current_font_group = 'ascii_0806'
        self.current_char = 0
        self.cell_size = 20
        # Initialize grid_size from current font group
        self.grid_size = self.font_groups[self.current_font_group]['dataSize']

        # Load font data
        self.load_all_font_data()

        # Create widgets
        self.create_widgets()

    def load_all_font_data(self):
        try:
            with open('EPDfont_orig.h', 'r') as f:
                content = f.read()

            for font_name in self.font_groups.keys():
                print(f"Loading {font_name}...")
                array_start = content.find(f'{font_name}')
                if array_start == -1:
                    print(f"Font array {font_name} not found")
                    continue

                # Find the array declaration
                array_decl_start = content.rfind('\n', 0, array_start)
                array_decl = content[array_decl_start:array_start].strip()
                print(f"Array declaration: {array_decl}")

                start_idx = content.find('{', array_start)
                end_idx = content.find('};', start_idx)

                if start_idx != -1 and end_idx != -1:
                    data = content[start_idx:end_idx].strip()
                    char_arrays = []
                    height = self.font_groups[font_name]['dataSize'][1]

                    # Split the data into lines and process each line
                    lines = data.split('\n')
                    current_char = []

                    print('Loading data [', end='', flush=True)

                    for line in lines:
                        line = line.strip()

                        if not line or line == '{':
                            continue

                        # Extract hex values using regex
                        hex_values = re.findall(r'0x[0-9A-Fa-f]{2}', line)


                        if hex_values:
                            # If we find a complete character data in one line
                            if len(hex_values) == height:
                                print('.', end='', flush=True)
                                char_arrays.append([int(v, 16) for v in hex_values])
                            else:
                                print('hex_values and height is not matched (hex :', len(hex_values),' height :', height, ')')
                                #print('-', end='', flush=True)
                                # Add values to current character
                                current_char.extend([int(v, 16) for v in hex_values])

                                # If we have a complete character, add it to the array
                                if len(current_char) == height:
                                    char_arrays.append(current_char)
                                    current_char = []
                    print(']', flush=True)

                    self.font_groups[font_name]['data'] = char_arrays
                    print(f"Loaded {len(char_arrays)} characters for {font_name}")

        except Exception as e:
            print(f"Error loading font data: {e}")
            # Initialize empty fonts
            for font_name, font_info in self.font_groups.items():
                font_info['data'] = [[0x00] * font_info['size'][1] for _ in range(95)]

    def load_char(self):
        # realod specified font data from the file
        self.load_all_font_data()

        try:
            char = self.char_selector.get()
            if char and len(char) == 1:
                char_code = ord(char)
                if 32 <= char_code <= 126:  # Valid ASCII range
                    self.current_char = char_code - 32
                    if 0 <= self.current_char < len(self.font_groups[self.current_font_group]['data']):
                        print(f"Loading character '{char}' (ASCII {char_code}, index {self.current_char})")
                        print(f"Character data: {[f'0x{byte:02X}' for byte in self.font_groups[self.current_font_group]['data'][self.current_char]]}")
                        self.draw_grid()
                    else:
                        print(f"Character index {self.current_char} out of range (font has {len(self.font_groups[self.current_font_group]['data'])} characters)")
                else:
                    print(f"Character '{char}' is outside valid ASCII range (32-126)")
        except Exception as e:
            print(f"Error loading character: {e}")

    def create_widgets(self):
        # Font selector
        font_frame = ttk.Frame(self.root)
        font_frame.pack(pady=5)

        ttk.Label(font_frame, text="Font:").pack(side=tk.LEFT)
        self.font_selector = ttk.Combobox(font_frame,
                                        values=list(self.font_groups.keys()),
                                        state='readonly',
                                        width=10)
        self.font_selector.set(self.current_font_group)
        self.font_selector.pack(side=tk.LEFT, padx=5)
        self.font_selector.bind('<<ComboboxSelected>>', self.change_font_group)

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
                            width=self.grid_size[0]*self.cell_size * 1.4,   # Use width
                            height=self.grid_size[1]*self.cell_size * 1.4)  # Use height
        self.canvas.pack(pady=10, padx=10)

        # Bind mouse events
        self.canvas.bind("<Button-1>", self.toggle_pixel)

        # Draw initial grid
        self.draw_grid()

    def change_font_group(self, event=None):
        self.current_font_group = self.font_selector.get()
        self.grid_size = self.font_groups[self.current_font_group]['gridSize']
        grid_width, grid_height = self.grid_size

        blocks = self.font_groups[self.current_font_group]['blockSize']

        # キャンバスのサイズを更新（高さはブロック数を考慮）
        self.canvas.config(
            width=grid_width * self.cell_size + 2,  # 境界線の分を少し余分に
            height=grid_height * blocks * self.cell_size + 2  # 境界線の分を少し余分に
        )

        # グリッドを再描画
        self.draw_grid()

    def draw_grid(self):
        self.canvas.delete("all")
        grid_height, grid_width = self.font_groups[self.current_font_group]['gridSize']
        data_height, data_width = self.font_groups[self.current_font_group]['dataSize']

        font_data = self.font_groups[self.current_font_group]['data']
        char_data = [0] * data_height
        if self.current_char < len(font_data):
            char_data = font_data[self.current_char]

        blocks = self.font_groups[self.current_font_group]['blockSize']

        # Draw font grid with rotation correction
        for block in range(blocks):
            for row in range(grid_width):
                for col in range(grid_height):
                    byte_index = block * grid_height + row
                    bit_position = 7 - col

                    is_set = False
                    if byte_index < len(char_data):
                        is_set = (char_data[byte_index] & (1 << bit_position)) != 0

                    x1 = row * self.cell_size
                    y1 = (block * grid_height + (grid_height - col - 1)) * self.cell_size
                    y2 = (block * grid_height + (grid_height - col - 1) + 1) * self.cell_size
                    x2 = (row + 1) * self.cell_size

                    color = "black" if is_set else "white"
                    self.canvas.create_rectangle(
                        x1, y1, x2, y2,
                        fill=color, outline="gray"
                    )


    def toggle_pixel(self, event):
        grid_height, grid_width = self.font_groups[self.current_font_group]['gridSize']
        data_height, data_width = self.font_groups[self.current_font_group]['dataSize']
        blocks = data_height // grid_height

        col = event.x // self.cell_size
        row = event.y // self.cell_size
        block = row // grid_height
        data_row = row % grid_height

        if col >= grid_width or row >= grid_height * blocks:
            return

        byte_index = block * grid_height + data_row
        # Rotate bit position 90 degrees left from current state
        bit_position = col  # Changed from (7 - col) to col

        if byte_index < len(self.font_groups[self.current_font_group]['data'][self.current_char]):
            self.font_groups[self.current_font_group]['data'][self.current_char][byte_index] ^= (1 << bit_position)
            self.draw_grid()

    def save_font(self):
        try:
            output_file = 'EPDfont_orig.h'
            with open(output_file, 'r') as f:
                content = f.read()

            for font_name, font_info in self.font_groups.items():
                array_start = content.find(font_name)
                if array_start == -1:
                    continue

                start_idx = content.find('{', array_start)
                end_idx = content.find('};', start_idx)
                if start_idx == -1 or end_idx == -1:
                    continue
                end_idx += 2

                height = font_info['dataSize'][1]
                new_font_data = f"const unsigned char {font_name}[][{height}]={{\n"

                for i, char_data in enumerate(font_info['data']):
                    if i % 4 == 0:
                        new_font_data += "    "
                    # データはすでに正しい向きなので、回転処理は不要
                    new_font_data += "{" + ",".join(f"0x{byte:02X}" for byte in char_data) + "},"
                    if i % 4 == 3:
                        new_font_data += "\n"
                new_font_data = new_font_data.rstrip(",\n") + "\n};"

                content = content[:start_idx] + new_font_data[new_font_data.find('{'):] + content[end_idx:]

            with open(output_file, 'w') as f:
                f.write(content)

            tk.messagebox.showinfo("Success", "All font data saved successfully!")

        except Exception as e:
            messagebox.showerror("Error", f"Failed to save font data: {str(e)}")
            print(f"Error saving font: {e}")

# Add this at the end of the file
if __name__ == "__main__":
    root = tk.Tk()
    app = FontEditor(root)
    root.mainloop()