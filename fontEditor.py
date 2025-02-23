import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import re
import numpy as np
import os


class FontEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("EPD Font Editor")
        self.font_data = {}
        self.current_char = None
        self.current_size = None
        self.canvas_pixels = []

        self.font_file_path = 'EPDfont.h'

        # GUIレイアウト
        self.main_frame = ttk.Frame(self.root, padding="10")
        self.main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # フォント選択
        ttk.Label(self.main_frame, text="Font Size:").grid(row=0, column=0, sticky=tk.W)
        self.size_var = tk.StringVar()
        self.size_combo = ttk.Combobox(self.main_frame, textvariable=self.size_var,
                                     values=["0806", "1206", "1608", "2412", "4824"])
        self.size_combo.grid(row=0, column=1, sticky=tk.W)
        self.size_combo.bind("<<ComboboxSelected>>", self.load_font)

        # ファイルメニュー
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="Load Font File", command=self.load_file)
        file_menu.add_command(label="Save Font File", command=self.save_file)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)

        # サムネイル表示
        self.thumbnail_frame = ttk.LabelFrame(self.main_frame, text="Thumbnails", padding="5")
        self.thumbnail_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E))
        self.thumbnail_canvas = tk.Canvas(self.thumbnail_frame, width=300, height=200)
        self.thumbnail_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.scrollbar = ttk.Scrollbar(self.thumbnail_frame, orient=tk.VERTICAL,
                                     command=self.thumbnail_canvas.yview)
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.thumbnail_canvas.configure(yscrollcommand=self.scrollbar.set)

        # 編集エリア
        self.edit_frame = ttk.LabelFrame(self.main_frame, text="Edit Character", padding="5")
        self.edit_frame.grid(row=1, column=2, sticky=(tk.N, tk.S))
        self.edit_canvas = tk.Canvas(self.edit_frame, width=200, height=200)
        self.edit_canvas.pack()
        self.edit_canvas.bind("<Button-1>", self.toggle_pixel)

        # 保存ボタン
        ttk.Button(self.edit_frame, text="Update Character", command=self.update_char).pack(pady=5)

        # 読み込みボタン
        ttk.Button(self.main_frame, text="Load Font File", command=self.load_file).grid(row=2, column=0, sticky=tk.W)


    def load_file(self):
        with open(self.font_file_path, 'r') as f:
            content = f.read()
        self.parse_font_data(content)
        self.size_combo.set("0806")  # デフォルトで0806を選択
        self.load_font()

    def parse_font_data(self, content):
        patterns = {
            "0806": r"const\s+unsigned\s+char\s+ascii_0806\[\]\[6\]\s*=\s*{([^;]+)};",
            "1206": r"const\s+unsigned\s+char\s+ascii_1206\[\]\[12\]\s*=\s*{([^;]+)};",
            "1608": r"const\s+unsigned\s+char\s+ascii_1608\[\]\[16\]\s*=\s*{([^;]+)};",
            "2412": r"const\s+unsigned\s+char\s+ascii_2412\[\]\[36\]\s*=\s*{([^;]+)};",
            "4824": r"const\s+unsigned\s+char\s+ascii_4824\[\]\[144\]\s*=\s*{([^;]+)};"
        }

        bytes_per_char = {
            "0806": 6,
            "1206": 12,
            "1608": 16,
            "2412": 36,
            "4824": 144
        }

        width_by_size = {
            "0806": 8,
            "1206": 12,
            "1608": 16,
            "2412": 24,
            "4824": 48
        }

        for size, pattern in patterns.items():
            print(f"Searching for font size: {size}")
            match = re.search(pattern, content, re.DOTALL | re.MULTILINE)
            if match:
                print(f"Found font data for size: {size}")
                data_str = match.group(1).strip()
                char_entries = [entry.strip() for entry in re.split(r'\},\s*(?://[^\n]*\n)?', data_str) if entry.strip()]
                self.font_data[size] = {}

                bytes_count = bytes_per_char[size]
                width = width_by_size[size]

                for i, entry in enumerate(char_entries):
                    entry = re.sub(r'//.*$', '', entry, flags=re.MULTILINE)
                    byte_str = entry.replace('{', '').replace('}', '').strip()
                    try:
                        bytes_list = []
                        for b in byte_str.split(','):
                            if b.strip():
                                try:
                                    # 16進数文字列を正規化
                                    hex_val = b.strip().replace('0x', '').strip()
                                    if hex_val:
                                        bytes_list.append(int(hex_val, 16))
                                except ValueError as e:
                                    print(f"Error parsing hex value: {b.strip()} - {e}")

                        char = chr(32 + i) if i < 95 else chr(i - 95 + 127)

                        # バイト数が足りない場合は0で埋める
                        while len(bytes_list) < bytes_count:
                            bytes_list.append(0)

                        if len(bytes_list) == bytes_count:
                            self.font_data[size][char] = bytes_list
                            print(f"Loaded character '{char}' for size {size} with {len(bytes_list)} bytes")
                        else:
                            print(f"Warning: Invalid data length for char '{char}' in size {size}: {len(bytes_list)} != {bytes_count}")
                    except Exception as e:
                        print(f"Error parsing data at index {i}: {e}")
            else:
                print(f"No match found for size {size}")

    def load_font(self, event=None):
        size = self.size_var.get()
        if not size or size not in self.font_data:
            return

        self.current_size = size
        self.thumbnail_canvas.delete("all")
        self.canvas_pixels = []

        # サイズ文字列からwidthとheightを取得
        width = int(size[:2])  # 最初の2桁
        height = int(size[2:]) # 残りの2桁

        x, y = 0, 0
        for char, data in sorted(self.font_data[size].items()):
            bitmap = self.bytes_to_bitmap(data, width, height)
            thumb_id = self.thumbnail_canvas.create_rectangle(x, y, x+width*2, y+height*2,
                                                            fill="white", outline="black")
            # ビットマップの描画
            for i in range(height):
                for j in range(width):
                    if bitmap[i][j]:
                        self.thumbnail_canvas.create_rectangle(x+j*2, y+i*2, x+j*2+2, y+i*2+2,
                                                             fill="black")

            # 文字のラベルを追加
            self.thumbnail_canvas.create_text(x+width, y+height*2+10, text=char)
            self.thumbnail_canvas.tag_bind(thumb_id, "<Button-1>",
                                         lambda e, c=char: self.select_char(c))

            # 次の文字の位置を計算
            x += width*2 + 10
            if x > 280:  # 行の端に達したら改行
                x = 0
                y += height*2 + 20

        # スクロール領域を更新
        self.thumbnail_canvas.configure(scrollregion=self.thumbnail_canvas.bbox("all"))

    def bytes_to_bitmap(self, bytes_data, width, height):
        bitmap = np.zeros((height, width), dtype=bool)
        bytes_per_row = (width + 7) // 8  # 切り上げ除算

        for row in range(height):
            for byte_offset in range(bytes_per_row):
                if row * bytes_per_row + byte_offset >= len(bytes_data):
                    continue

                byte = bytes_data[row * bytes_per_row + byte_offset]
                for bit in range(8):
                    col = byte_offset * 8 + bit
                    if col >= width:
                        break
                    # MSBファースト (Most Significant Bit first)
                    bitmap[row][col] = bool(byte & (0x80 >> bit))

        return bitmap

    def select_char(self, char):
        self.current_char = char
        self.edit_canvas.delete("all")
        self.canvas_pixels = []

        # サイズの解析を一貫した方法に修正
        width = int(self.current_size[:2])
        height = int(self.current_size[2:])

        # キャンバスのサイズを取得
        canvas_width = self.edit_canvas.winfo_width()
        canvas_height = self.edit_canvas.winfo_height()

        # スケールを計算（余白を考慮）
        scale = min((canvas_width - 20) // width, (canvas_height - 20) // height)

        # 中央揃えのためのオフセットを計算
        x_offset = (canvas_width - (width * scale)) // 2
        y_offset = (canvas_height - (height * scale)) // 2

        # ビットマップデータを取得
        bitmap = self.bytes_to_bitmap(self.font_data[self.current_size][char], width, height)

        # ピクセルを描画
        for i in range(height):
            row = []
            for j in range(width):
                color = "black" if bitmap[i][j] else "white"
                pixel = self.edit_canvas.create_rectangle(
                    x_offset + j*scale,
                    y_offset + i*scale,
                    x_offset + (j+1)*scale,
                    y_offset + (i+1)*scale,
                    fill=color,
                    outline="gray"
                )
                row.append(pixel)
            self.canvas_pixels.append(row)

        self.edit_frame.config(text=f"Edit Character: '{char}'")

    def toggle_pixel(self, event):
        if not self.current_char:
            return

        # キャンバスのサイズとオフセットを取得
        width = int(self.current_size[:2])
        height = int(self.current_size[2:])
        canvas_width = self.edit_canvas.winfo_width()
        canvas_height = self.edit_canvas.winfo_height()
        scale = min((canvas_width - 20) // width, (canvas_height - 20) // height)
        x_offset = (canvas_width - (width * scale)) // 2
        y_offset = (canvas_height - (height * scale)) // 2

        # クリック位置をグリッド座標に変換
        x = (event.x - x_offset) // scale
        y = (event.y - y_offset) // scale

        # 有効な範囲内のクリックかチェック
        if 0 <= x < width and 0 <= y < height:
            current_color = self.edit_canvas.itemcget(self.canvas_pixels[y][x], "fill")
            new_color = "black" if current_color == "white" else "white"
            self.edit_canvas.itemconfig(self.canvas_pixels[y][x], fill=new_color)

    def update_char(self):
        if not self.current_char:
            return
        width, height = map(int, self.current_size[:4]), int(self.current_size[4:])
        bitmap = np.zeros((height, width), dtype=bool)
        for i in range(height):
            for j in range(width):
                bitmap[i][j] = self.edit_canvas.itemcget(self.canvas_pixels[i][j], "fill") == "black"
        bytes_data = self.bitmap_to_bytes(bitmap, width, height)
        self.font_data[self.current_size][self.current_char] = bytes_data
        self.load_font()

    def bitmap_to_bytes(self, bitmap, width, height):
        bytes_per_row = (width + 7) // 8  # 切り上げ除算
        bytes_data = []

        for row in range(height):
            for byte_offset in range(bytes_per_row):
                byte = 0
                for bit in range(8):
                    col = byte_offset * 8 + bit
                    if col >= width:
                        break
                    if bitmap[row][col]:
                        # MSBファースト (Most Significant Bit first)
                        byte |= (0x80 >> bit)
                bytes_data.append(byte)

        return bytes_data

    def save_file(self):
        with open(self.font_file_path, 'w') as f:
            f.write("#ifndef _EPDFONT_H_\n#define _EPDFONT_H_\n\n")
            for size in self.font_data:
                bytes_per_char = {"0806": 6, "1206": 12, "1608": 16, "2412": 36, "4824": 144}[size]
                f.write(f"const unsigned char ascii_{size}[][bytes_per_char] = {{\n")
                for i, (char, data) in enumerate(self.font_data[size].items()):
                    byte_str = ", ".join(f"0x{b:02x}" for b in data)
                    comment = f"// {char}" if i < 95 else f"// {chr(i-95+127)}"
                    f.write(f"{{{byte_str}}},{comment}\n")
                f.write("};\n\n")
            f.write("#endif\n")
        messagebox.showinfo("Save", "Font file saved successfully!")

if __name__ == "__main__":
    root = tk.Tk()
    app = FontEditor(root)
    root.mainloop()