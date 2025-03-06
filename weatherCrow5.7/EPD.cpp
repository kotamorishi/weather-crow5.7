#include "EPD.h"
#include "EPDfont.h"
#include "fonts.h"
#include "string.h"

PAINT Paint;


/*******************************************************************
	Function Description: Create image buffer array
	Interface Description: *image  The image array to be passed in
						   Width  Image width
						   Height Image height
						   Rotate Screen display orientation
						   Color  Display color
	Return Value:  None
*******************************************************************/
void Paint_NewImage(uint8_t *image,uint16_t Width,uint16_t Height,uint16_t Rotate,uint16_t Color)
{
	Paint.Image = 0x00;
	Paint.Image = image;
	Paint.color = Color;
	Paint.widthMemory = Width;
	Paint.heightMemory = Height;
	Paint.widthByte = (Width % 8 == 0)? (Width / 8 ): (Width / 8 + 1);
	Paint.heightByte = Height;
	Paint.rotate = Rotate;
	if(Rotate==0||Rotate==180)
	{
		Paint.width=Height;
		Paint.height=Width;
	}
	else
	{
		Paint.width = Width;
		Paint.height = Height;
	}
}

/*******************************************************************
	Function Description: Clear the buffer
	Interface Description: Color  Pixel color parameter
	Return Value:  None
*******************************************************************/
void Paint_Clear(uint8_t Color)
{
	uint16_t X,Y;
	uint32_t Addr;
  for(Y=0;Y<Paint.heightByte;Y++)
	{
    for(X=0;X<Paint.widthByte;X++)
		{
      Addr=X+Y*Paint.widthByte;//8 pixel =  1 byte
      Paint.Image[Addr]=Color;
    }
  }
}


/*******************************************************************
	Function Description: Set a pixel
	Interface Description: Xpoint Pixel x-coordinate parameter
						   Ypoint Pixel y-coordinate parameter
						   Color  Pixel color parameter
	Return Value:  None
*******************************************************************/
void Paint_SetPixel(uint16_t Xpoint,uint16_t Ypoint,uint16_t Color)
{
	uint16_t X, Y;
	uint32_t Addr;
	uint8_t Rdata;
    switch(Paint.rotate)
		{
				case 0:
					if(Xpoint>=396)
					{
						Xpoint+=8;
					}
					X=Xpoint;
					Y=Ypoint;
					break;
			case 90:
					if(Ypoint>=396)
					{
						Ypoint+=8;
					}
					X=Paint.widthMemory-Ypoint-1;
					Y=Xpoint;
					break;
			case 180:
				  if(Xpoint>=396)
					{
						Xpoint+=8;
					}
					X=Paint.widthMemory-Xpoint-1;
					Y=Paint.heightMemory-Ypoint-1;
					break;

			case 270:
					if(Ypoint>=396)
					{
						Ypoint+=8;
					}
					X=Ypoint;
					Y=Paint.heightMemory-Xpoint-1;
					break;
				default:
						return;
    }
		Addr=X/8+Y*Paint.widthByte;
    Rdata=Paint.Image[Addr];
    if(Color==BLACK)
    {
			Paint.Image[Addr]=Rdata&~(0x80>>(X % 8)); // Set the corresponding data bit to 0
		}
    else
		{
	Paint.Image[Addr]=Rdata|(0x80>>(X % 8));   // Set the corresponding data bit to 1
		}
}


/*******************************************************************
	Function Description: Draw line function
	Interface Description: Xstart Pixel x start coordinate parameter
						   Ystart Pixel Y start coordinate parameter
						   Xend   Pixel x end coordinate parameter
						   Yend   Pixel Y end coordinate parameter
						   Color  Pixel color parameter
	Return Value:  None
*******************************************************************/
void EPD_DrawLine(uint16_t Xstart,uint16_t Ystart,uint16_t Xend,uint16_t Yend,uint16_t Color)
{
	uint16_t Xpoint, Ypoint;
	int dx, dy;
	int XAddway,YAddway;
	int Esp;
	char Dotted_Len;
  Xpoint = Xstart;
  Ypoint = Ystart;
  dx = (int)Xend - (int)Xstart >= 0 ? Xend - Xstart : Xstart - Xend;
  dy = (int)Yend - (int)Ystart <= 0 ? Yend - Ystart : Ystart - Yend;
  XAddway = Xstart < Xend ? 1 : -1;
  YAddway = Ystart < Yend ? 1 : -1;
  Esp = dx + dy;
  Dotted_Len = 0;
  for (;;) {
        Dotted_Len++;
            Paint_SetPixel(Xpoint, Ypoint, Color);
        if (2 * Esp >= dy) {
            if (Xpoint == Xend)
                break;
            Esp += dy;
            Xpoint += XAddway;
        }
        if (2 * Esp <= dx) {
            if (Ypoint == Yend)
                break;
            Esp += dx;
            Ypoint += YAddway;
        }
    }
}
/*******************************************************************
	Function Description: Draw rectangle function
	Interface Description: Xstart Rectangle x start coordinate parameter
						   Ystart Rectangle Y start coordinate parameter
						   Xend   Rectangle x end coordinate parameter
						   Yend   Rectangle Y end coordinate parameter
						   Color  Pixel color parameter
						   mode   Whether the rectangle is filled
	Return Value:  None
*******************************************************************/
void EPD_DrawRectangle(uint16_t Xstart,uint16_t Ystart,uint16_t Xend,uint16_t Yend,uint16_t Color,uint8_t mode)
{
	uint16_t i;
    if (mode)
			{
        for(i = Ystart; i < Yend; i++)
				{
          EPD_DrawLine(Xstart,i,Xend,i,Color);
        }
      }
		else
		 {
        EPD_DrawLine(Xstart, Ystart, Xend, Ystart, Color);
        EPD_DrawLine(Xstart, Ystart, Xstart, Yend, Color);
        EPD_DrawLine(Xend, Yend, Xend, Ystart, Color);
        EPD_DrawLine(Xend, Yend, Xstart, Yend, Color);
		 }
}
/*******************************************************************
	Function Description: Draw circle function
	Interface Description: X_Center Circle center x-coordinate parameter
						   Y_Center Circle center y-coordinate parameter
						   Radius   Circle radius parameter
						   Color    Pixel color parameter
						   mode     Whether the circle is filled
	Return Value:  None
*******************************************************************/
void EPD_DrawCircle(uint16_t X_Center,uint16_t Y_Center,uint16_t Radius,uint16_t Color,uint8_t mode)
{
	int Esp, sCountY;
	uint16_t XCurrent, YCurrent;
  XCurrent = 0;
  YCurrent = Radius;
  Esp = 3 - (Radius << 1 );
    if (mode) {
        while (XCurrent <= YCurrent ) { //Realistic circles
            for (sCountY = XCurrent; sCountY <= YCurrent; sCountY ++ ) {
                Paint_SetPixel(X_Center + XCurrent, Y_Center + sCountY, Color);//1
                Paint_SetPixel(X_Center - XCurrent, Y_Center + sCountY, Color);//2
                Paint_SetPixel(X_Center - sCountY, Y_Center + XCurrent, Color);//3
                Paint_SetPixel(X_Center - sCountY, Y_Center - XCurrent, Color);//4
                Paint_SetPixel(X_Center - XCurrent, Y_Center - sCountY, Color);//5
                Paint_SetPixel(X_Center + XCurrent, Y_Center - sCountY, Color);//6
                Paint_SetPixel(X_Center + sCountY, Y_Center - XCurrent, Color);//7
                Paint_SetPixel(X_Center + sCountY, Y_Center + XCurrent, Color);
            }
            if ((int)Esp < 0 )
                Esp += 4 * XCurrent + 6;
            else {
                Esp += 10 + 4 * (XCurrent - YCurrent );
                YCurrent --;
            }
            XCurrent ++;
        }
    } else { //Draw a hollow circle
        while (XCurrent <= YCurrent ) {
            Paint_SetPixel(X_Center + XCurrent, Y_Center + YCurrent, Color);//1
            Paint_SetPixel(X_Center - XCurrent, Y_Center + YCurrent, Color);//2
            Paint_SetPixel(X_Center - YCurrent, Y_Center + XCurrent, Color);//3
            Paint_SetPixel(X_Center - YCurrent, Y_Center - XCurrent, Color);//4
            Paint_SetPixel(X_Center - XCurrent, Y_Center - YCurrent, Color);//5
            Paint_SetPixel(X_Center + XCurrent, Y_Center - YCurrent, Color);//6
            Paint_SetPixel(X_Center + YCurrent, Y_Center - XCurrent, Color);//7
            Paint_SetPixel(X_Center + YCurrent, Y_Center + XCurrent, Color);//0
            if ((int)Esp < 0 )
                Esp += 4 * XCurrent + 6;
            else {
                Esp += 10 + 4 * (XCurrent - YCurrent );
                YCurrent --;
            }
            XCurrent ++;
        }
    }
}


/*******************************************************************
    Function Description: Display a single character using variable-width font
    Interface Description:
        x         Character x-coordinate parameter
        y         Character y-coordinate parameter
        chr       Character to be displayed (ASCII 0x20 to 0x7E)
        font_size Font height (e.g., 8, 12, 24)
        color     Pixel color parameter (1 for black, 0 for white)
    Return Value: None
*******************************************************************/
void EPD_ShowChar(uint16_t x, uint16_t y, uint16_t chr, uint16_t font_size, uint16_t color) {
    const FontSet *font;

    switch (font_size) {
        case 8:
		    font = &font_8;
			break;
        case 12:
			font = &font_12;
			break;
        case 24:
			font = &font_24;
			break;
		case 36:
			font = &font_36;
			break;
        default:
            Serial.println("ERROR : Font size not supported!! You can add more fonts with ttfToEPD tool.");
            return;
    }

    if (!font || !font->chars) {
        Serial.println("ERROR : Font does not initialized.");
        return;
    }

    uint8_t chr_index = chr - font->char_start;
    if (chr_index >= font->char_count) {
        Serial.println("ERROR : Character out of range");
        return;
    }


    const FontChar *char_data = font->chars[chr_index];

    // Print sizes of variables
    Serial.print("[CHAR ");
	Serial.print(chr);
	Serial.print('/');
	Serial.print((char)char_data->char_code);
    Serial.print("] width: ");
    Serial.print(char_data->width);
	Serial.print(" height: ");
    Serial.print(char_data->height);
    Serial.print(", bytes_per_row: ");
    Serial.print(char_data->bytes_per_row);
    Serial.println();

    // Calculate expected bytes per row based on width
    uint8_t expected_bytes = (char_data->width + 7) / 8;

    // Validate bytes_per_row
    if (char_data->bytes_per_row == 0 || char_data->bytes_per_row > expected_bytes) {
        Serial.println("Invalid bytes_per_row");
        return;
    }

    if (!char_data->bitmap) {
        Serial.println("No bitmap data");
        return;
    }

    // Draw character with bounds checking
    uint16_t current_y = y + (uint16_t)char_data->vertical_offset;
    for (uint16_t row = 0; row < char_data->height && current_y <= Paint.heightMemory; row++) {
		Serial.println();
		Serial.print("Processing x: ");
		Serial.print(x);
		Serial.print(", y: ");
		Serial.print(current_y);
		Serial.print(" ==> ");
        uint16_t current_x = x + (uint16_t)char_data->horizontal_offset;
        for (uint16_t byte_idx = 0; byte_idx < char_data->bytes_per_row && current_x < Paint.widthMemory; byte_idx++) {
            uint8_t byte = char_data->bitmap[row * char_data->bytes_per_row + byte_idx];
            for (uint8_t bit = 0; bit < 8 && (byte_idx * 8 + bit) < char_data->width; bit++) {
                if (current_x >= Paint.widthMemory) break;
				if (current_y >= Paint.heightMemory) break;
                Paint_SetPixel(current_x++, current_y, (byte & (0x80 >> bit)) ? color : !color);

            }
        }
        current_y++;
    }
}

/*******************************************************************
    Function Description: Display a string using variable-width font
    Interface Description:
        x       String x-coordinate parameter
        y       String y-coordinate parameter
        chr     String to be displayed (null-terminated)
        fontSetSize   Font height (e.g., 8, 12, 24)
        color   Pixel color parameter
    Return Value: None
*******************************************************************/
void EPD_ShowString(uint16_t x, uint16_t y, const char *chr, uint16_t fontSetSize, uint16_t color) {
    const FontSet *font;

    switch (fontSetSize) {
        case 8:  font = &font_8;  break;
        case 12: font = &font_12; break;
        case 24: font = &font_24; break;
		case 36: font = &font_36; break;
        default: return;
    }

    uint16_t x_pos = x;
    while (*chr != '\0') {
        uint16_t chr_index = *chr - font->char_start;
        if (chr_index < font->char_count) {
            const FontChar *current_char = font->chars[chr_index];
            EPD_ShowChar(x_pos, y, current_char->char_code, fontSetSize, color);
            x_pos += current_char->width + 1;
        }
        chr++;
    }
}


/*******************************************************************
	Function Description: Exponential operation
	Interface Description: m Base
						  n Exponent
	Return Value:  m raised to the power of n
*******************************************************************/
uint32_t EPD_Pow(uint16_t m,uint16_t n)
{
	uint32_t result=1;
	while(n--)
	{
	  result*=m;
	}
	return result;
}

/*******************************************************************
	Function Description: Display a floating-point number
	Interface Description: x     Number x-coordinate parameter
						   y     Number y-coordinate parameter
						   num   Floating-point number to be displayed
						   len   Number of digits
						   pre   Precision of the floating-point number
						   size1 Display string font size
						   Color Pixel color parameter
	Return Value:  None
*******************************************************************/

void EPD_ShowFloatNum1(uint16_t x,uint16_t y,float num,uint8_t len,uint8_t pre,uint8_t sizey,uint8_t color)
{
	uint8_t t,temp,sizex;
	uint16_t num1;
	sizex=sizey/2;
	num1=num*EPD_Pow(10,pre);
	for(t=0;t<len;t++)
	{
		temp=(num1/EPD_Pow(10,len-t-1))%10;
		if(t==(len-pre))
		{
			EPD_ShowChar(x+(len-pre)*sizex,y,'.',sizey,color);
			t++;
			len+=1;
		}
	 	EPD_ShowChar(x+t*sizex,y,temp+48,sizey,color);
	}
}


// GUI display stopwatch
void EPD_ShowWatch(uint16_t x,uint16_t y,float num,uint8_t len,uint8_t pre,uint8_t sizey,uint8_t color)
{
	uint8_t t,temp,sizex;
	uint16_t num1;
	sizex=sizey/2;
	num1=num*EPD_Pow(10,pre);
	for(t=0;t<len;t++)
	{
		temp=(num1/EPD_Pow(10,len-t-1))%10;
		if(t==(len-pre))
		{
			EPD_ShowChar(x+(len-pre)*sizex+(sizex/2-2),y-6,':',sizey,color);
			t++;
			len+=1;
		}
	 	EPD_ShowChar(x+t*sizex,y,temp+48,sizey,color);
	}
}



void EPD_ShowPicture(uint16_t x,uint16_t y,uint16_t sizex,uint16_t sizey,const uint8_t BMP[],uint16_t Color)
{
	uint16_t j=0,t;
	uint16_t i,temp,x0,TypefaceNum=sizey*(sizex/8+((sizex%8)?1:0));
	x0=x;
  for(i=0;i<TypefaceNum;i++)
	{
		temp=BMP[j];
		for(t=0;t<8;t++)
		{
		 if(temp&0x80)
		 {
			 Paint_SetPixel(x,y,!Color);
		 }
		 else
		 {
			 Paint_SetPixel(x,y,Color);
		 }
		 x++;
		 temp<<=1;
		}
		if((x-x0)==sizex)
		{
			x=x0;
			y++;
		}
		j++;
//    delayMicroseconds(10);
	}
}


void EPD_drawImage(uint16_t drawPositionX, uint16_t drawPositionY, const uint8_t *bmp)
{
    // first two bytes are width and height
    uint16_t width = bmp[0] | (bmp[1] << 8);
    uint16_t height = bmp[2] | (bmp[3] << 8);

    uint16_t bytesPerRow = (width + 7) / 8;  // Round up to nearest byte
    uint16_t baseXpos = drawPositionX;
    uint8_t lastByteMask = 0xFF >> ((bytesPerRow * 8) - width); // Mask for last byte in row

    uint32_t idx = 4; // skip the first 4 bytes(width and height)

    for(uint16_t row = 0; row < height; row++) {
        drawPositionX = baseXpos;  // Reset X position at the start of each row
        for(uint16_t b = 0; b < bytesPerRow; b++) {
            uint8_t temp = bmp[idx++];
            // For the last byte in row, mask out unused bits
            if (b == bytesPerRow - 1 && width % 8 != 0) {
                temp &= lastByteMask;
            }

            // Calculate how many bits to process for this byte
            uint8_t bitsThisByte = (b == bytesPerRow - 1 && width % 8 != 0) ?
                                  (width % 8) : 8;

            for(uint8_t bit = 0; bit < bitsThisByte; bit++) {
                if(temp & 0x80) {
                    Paint_SetPixel(drawPositionX, drawPositionY, BLACK);
                } else {
                    Paint_SetPixel(drawPositionX, drawPositionY, WHITE);
                }
                drawPositionX++;
                temp <<= 1;
            }
        }
        drawPositionY++;
    }
}
