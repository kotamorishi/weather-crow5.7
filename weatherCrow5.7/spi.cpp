#include "spi.h"

void EPD_GPIOInit(void)
{
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(RES, OUTPUT);
    pinMode(DC, OUTPUT);
    pinMode(CS, OUTPUT);
    pinMode(BUSY, INPUT);
}

/**
 * @brief       IO simulated SPI send a byte of data
 * @param       dat: the byte of data to be sent
 * @retval      None
 */
void EPD_WR_Bus(uint8_t dat)
{
    uint8_t i;
    EPD_CS_Clr();
    for (i = 0; i < 8; i++)
    {
        EPD_SCK_Clr();
        if (dat & 0x80)
        {
            EPD_MOSI_Set();
        }
        else
        {
            EPD_MOSI_Clr();
        }
        EPD_SCK_Set();
        dat <<= 1;
    }
    EPD_CS_Set();
}

/**
 * @brief       Write a register command to the LCD
 * @param       reg: the command to be written
 * @retval      None
 */
void EPD_WR_REG(uint8_t reg)
{
    EPD_DC_Clr();
    EPD_WR_Bus(reg);
    EPD_DC_Set();
}

/**
 * @brief       Write a byte of data to the LCD
 * @param       dat: the data to be written
 * @retval      None
 */
void EPD_WR_DATA8(uint8_t dat)
{
    EPD_DC_Set();
    EPD_WR_Bus(dat);
    EPD_DC_Set();
}
