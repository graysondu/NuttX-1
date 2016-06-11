/****************************************************************************
 * config/sam4l-xplained/src/sam_ug2832hsweg04.c
 *
 *   Copyright (C) 2013, 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/* OLED1 Connector:
 *
 *  OLED1             EXT1                 EXT2                 Other use of either pin
 *  ----------------- -------------------- -------------------- ------------------------------------
 *  1  ID             1                    1
 *  2  GND            2       GND          2
 *  3  BUTTON2        3  PA04 ADCIFE/AD0   3  PA07 ADCIFE/AD2
 *  4  BUTTON3        4  PA05 ADCIFE/AD1   4  PB02 ADCIFE/AD3
 *  5  DATA_CMD_SEL   5  PB12 GPIO         5  PC08 GPIO         PB12 and PC8 on EXT5
 *  6  LED3           6  PC02 GPIO         6  PB10 GPIO         PB10 on EXT5
 *  7  LED1           7  PC00 TC/1/A0      7  PC04 TC/1/A2
 *  8  LED2           8  PC01 TC/1/B0      8  PC05 TC/1/B2      PC05 on EXT5
 *  9  BUTTON1        9  PC25 EIC/EXTINT2  9  PC06 EIC/EXTINT8  PC25 on EXT5
 *  10 DISPLAY_RESET  10 PB13 SPI/NPCS1    10 PC09 GPIO         PB13 on EXT5
 *  11 N/C            11 PA23 TWIMS/0/TWD  11 PB14 TWIMS/3/TWD  PB14 on EXT3&4, PA23 and PB14 on EXT5
 *  12 N/C            12 PA24 TWIMS/0/TWCK 12 PB15 TWIMS/3/TWCK PB15 on EXT3&4, PA24 and PB15 on EXT5
 *  13 N/C            13 PB00 USART/0/RXD  13 PC26 USART/1/RXD  PB00 on EXT4, PC26 on EXT3&5
 *  14 N/C            14 PB01 USART/0/TXD  14 PC27 USART/1/TXD  PB01 on EXT4, PC27 on EXT3&5
 *  15 DISPLAY_SS     15 PC03 SPI/NPCS0    15 PB11 SPI/NPCS2    PB11 on EXT5
 *  16 SPI_MOSI       16 PA22 SPI/MOSI     16 PA22 SPI/MOSI     PA22 on EXT5
 *  17 N/C            17 PA21 SPI/MISO     17 PA21 SPI/MISO     PA21 on EXT5
 *  18 SPI_SCK        18 PC30 SPI/SCK      18 PC30 SPI/SCK      PC30 on EXT5
 *  19 GND            19      GND             GND
 *  20 VCC            20      VCC             VCC
 *
 * OLED1 signals
 *
 * DATA_CMD_SEL - Data/command select. Used to choose whether the
 *   communication is data to the display memory or a command to the LCD
 *   controller. High = data, low = command
 * DISPLAY_RESET - Reset signal to the OLED display, active low. Used during
 *   initialization of the display.
 * DISPLAY_SS - SPI slave select signal, must be held low during SPI
 *   communication.
 * SPI_MOSI - SPI master out, slave in signal. Used to write data to the
 *   display
 * SPI_SCK SPI - clock signal, generated by the master.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/board.h>
#include <nuttx/spi/spi.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/lcd/ssd1306.h>

#include "sam_gpio.h"
#include "sam_spi.h"

#include "sam4l-xplained.h"

#ifdef CONFIG_SAM4L_XPLAINED_OLED1MODULE

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* Configuration ************************************************************/
/* The pin configurations here require that SPI1 is selected */

#ifndef CONFIG_LCD_SSD1306
#  error "The OLED driver requires CONFIG_LCD_SSD1306 in the configuration"
#endif

#ifndef CONFIG_LCD_UG2832HSWEG04
#  error "The OLED driver requires CONFIG_LCD_UG2832HSWEG04 in the configuration"
#endif

#ifndef CONFIG_SAM34_SPI0
#  error "The OLED driver requires CONFIG_SAM34_SPI0 in the configuration"
#endif

#ifndef CONFIG_SPI_CMDDATA
#  error "The OLED driver requires CONFIG_SPI_CMDDATA in the configuration"
#endif

/* Debug ********************************************************************/

#ifdef CONFIG_DEBUG_LCD
#  define lcderr(format, ...)   err(format, ##__VA_ARGS__)
#  define lcdinfo(format, ...)  info(format, ##__VA_ARGS__)
#else
#  define lcderr(x...)
#  define lcdinfo(x...)
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_graphics_setup
 *
 * Description:
 *   Called by NX initialization logic to configure the OLED.
 *
 ****************************************************************************/

FAR struct lcd_dev_s *board_graphics_setup(unsigned int devno)
{
  FAR struct spi_dev_s *spi;
  FAR struct lcd_dev_s *dev;

  /* Configure the OLED GPIOs. This initial configuration is RESET low,
   * putting the OLED into reset state.
   */

  (void)sam_configgpio(GPIO_OLED_RST);

  /* Wait a bit then release the OLED from the reset state */

  up_mdelay(20);
  sam_gpiowrite(GPIO_OLED_RST, true);

  /* Get the SPI1 port interface */

  spi = sam_spibus_initialize(OLED_CSNO);
  if (!spi)
    {
      lcderr("Failed to initialize SPI port 1\n");
    }
  else
    {
      /* Bind the SPI port to the OLED */

      dev = ssd1306_initialize(spi, devno);
      if (!dev)
        {
          lcderr("Failed to bind SPI port 1 to OLED %d: %d\n", devno);
        }
     else
        {
          lcdinfo("Bound SPI port 1 to OLED %d\n", devno);

          /* And turn the OLED on */

          (void)dev->setpower(dev, CONFIG_LCD_MAXPOWER);
          return dev;
        }
    }

  return NULL;
}
#endif /* CONFIG_SAM4L_XPLAINED_OLED1MODULE */
