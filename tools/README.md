# Tools for this project

When you are working on this project, you may want to use your graphics or fonts.
Here are some tools that I made:

## SVG image to BMP image converter
This tool converts SVG vector data to BMP header file.
It can be drawn on the screen using the `EPD_drawImage()` function.

- converts the all files under `svg` directory to `weatherIcons.h` file.
- the output size is defined in the script, you can change it.
```python
imageSize = [
    (64,'small'),
    (128,'midium'),
    (256,'large')
]
```

ussage:
```bash
python3 svgToBmp.py
```

Note: The BMP file is not compressed and stored it as C++ header file.
So if you have a large image, it may not fit in the memory.

If it does not fit within the runtime memory, e-paper will not start and keep rebooting.
when it happens you can see the error message on the serial monitor. it may say `Heap error`.
**Watch out serial monitor carefully** when you add a new image.


## TTF font to EPD font converter
This tool converts TTF font to EPD font.
All the fonts should include `fonts.h` which has the font data structure.
Once you add the new font on your project, add the font data to the `fonts.h` file.
add the font file under the `fonts` directory and run the script.

Known issue:
- The font size is not accurate.
- The font kerning is not accurate.
- Wide fonts are not rendered correctly, some parts are cut off.

```c
typedef struct {
    uint8_t char_code;      // Character itself
    uint8_t width;          // Character width in pixels (max 255)
    uint8_t height;         // Character width in pixels (max 255)
    int8_t vertical_offset;   // Offset from top of the line to the top of the character (could be negative)
    int8_t horizontal_offset; // Offset from the left of the line to the left of the character (could be negative) AKA kerning
    uint8_t bytes_per_row;  // Number of bytes per row (max 255)
    const uint8_t *bitmap;  // Pointer to bitmap data
} FontChar;

typedef struct {
    uint8_t height;         // Font height in pixels
    uint8_t char_start;     // First character code (usually 0x20 for space)
    uint8_t char_count;     // Number of characters in the font
    int8_t space_width;     // Width of the space character
    const FontChar * const *chars;  // Pointer to array of pointers to FontChar structs
} FontSet;

// Declare external font variables
extern const FontSet font_8;
extern const FontSet font_16;
extern const FontSet font_36;
extern const FontSet font_38;
extern const FontSet font_92;

typedef enum {
    FONT_SIZE_8 = 8,
    FONT_SIZE_16 = 16,
    FONT_SIZE_36 = 36,
    FONT_SIZE_38 = 38,
    FONT_SIZE_92 = 92
} FontSize;
```

Generate example:
```bash
python ttfToEPD.py ../fonts/8bit_wonder/8-BIT\ WONDER.TTF ../weatherCrow5.7/font8.cpp 8
python ttfToEPD.py ../fonts/rotorcap_neue12/ROTORcapNeue-Regular.ttf ../weatherCrow5.7/font12.cpp 12
python ttfToEPD.py ../fonts/rotorcap_neue12/ROTORcapNeue-Bold.ttf ../weatherCrow5.7/font24.cpp 24
```

Note : These tools are not perfect, but it just works for me.
It may not works for some fonts or svg images. so you may need to modify the code.
Pull requests are very welcome.