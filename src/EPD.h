#ifndef _EPD_GUI_H_
#define _EPD_GUI_H_

#include "EPD_Init.h"
#include "fonts.h"

typedef struct {
	uint8_t *Image;
	uint16_t width;
	uint16_t height;
	uint16_t widthMemory;
	uint16_t heightMemory;
	uint16_t color;
	uint16_t rotate;
	uint16_t widthByte;
	uint16_t heightByte;

}PAINT;
extern PAINT Paint;

// Define E-Paper display orientation
/*******************
Rotation: 0 - 0 degrees orientation
Rotation: 90 - 90 degrees orientation
Rotation: 180 - 180 degrees orientation
Rotation: 270 - 270 degrees orientation
*******************/
#define Rotation 180

void Paint_NewImage(uint8_t *image, uint16_t Width, uint16_t Height, uint16_t Rotate, uint16_t Color); // Create canvas and control display orientation
void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color); // Set pixel color
void Paint_Clear(uint8_t Color); // Clear the canvas with a color
void EPD_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color); // Draw a line
void EPD_DrawRectangle(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color, uint8_t mode); // Draw a rectangle
void EPD_DrawCircle(uint16_t X_Center, uint16_t Y_Center, uint16_t Radius, uint16_t Color, uint8_t mode); // Draw a circle
void EPD_ShowChar(uint16_t x, uint16_t y, uint16_t chr, FontSize font_size, uint16_t color); // Display a character
void EPD_ShowString(uint16_t x, uint16_t y, const char *chr, FontSize font_size, uint16_t color, bool disableLineBreak = false); // Display a string
void EPD_ShowStringRightAligned(uint16_t right_x, uint16_t y, const char *chr, FontSize font_size, uint16_t color); // Display a string with right alignment (no line breaks)
void EPD_ShowStringCenterAligned(uint16_t center_x, uint16_t y, const char *chr, FontSize font_size, uint16_t color); // Display a string with center alignment
void EPD_ShowPicture(uint16_t x, uint16_t y, uint16_t sizex, uint16_t sizey, const uint8_t BMP[], uint16_t Color); // Display a picture
void EPD_ClearWindows(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color); // Clear a window area with a color

void EPD_drawImage(uint16_t drawPositionX, uint16_t drawPositionY, const uint8_t *bmp);

#endif
