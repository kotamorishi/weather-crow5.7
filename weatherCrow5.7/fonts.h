#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>


typedef struct {
    uint8_t char_code;      // Character itself
    uint8_t width;          // Character width in pixels (max 255)
    uint8_t height;         // Character width in pixels (max 255)
    int8_t vertical_offset;   // Offset from top of the line to the top of the character (could be negative)
    int8_t horizontal_offset; // Offset from the left of the line to the left of the character (could be negative)
    uint8_t bytes_per_row;  // Number of bytes per row (max 255)
    const uint8_t *bitmap;  // Pointer to bitmap data
} FontChar;


typedef struct {
    uint8_t height;         // Font height in pixels
    uint8_t char_start;     // First character code (usually 0x20 for space)
    uint8_t char_count;     // Number of characters in the font
    const FontChar * const *chars;  // Pointer to array of pointers to FontChar structs
} FontSet;

// Declare external font variables
extern const FontSet font_8;
extern const FontSet font_12;
extern const FontSet font_24;
extern const FontSet font_36;

#endif // FONTS_H