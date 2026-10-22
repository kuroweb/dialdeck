// Guition JC3636K718C pinout (manufacturer demo JC3636K718_knob_EN).
#pragma once

#define BOARD_LCD_H_RES        360
#define BOARD_LCD_V_RES        360

#define BOARD_LCD_SCK          11
#define BOARD_LCD_CS           12
#define BOARD_LCD_D0           13
#define BOARD_LCD_D1           14
#define BOARD_LCD_D2           15
#define BOARD_LCD_D3           16
#define BOARD_LCD_RST          17
#define BOARD_LCD_BL           21
#define BOARD_LCD_PCLK_HZ      (80 * 1000 * 1000)

#define BOARD_TOUCH_SDA        9
#define BOARD_TOUCH_SCL        10
#define BOARD_TOUCH_INT        7
#define BOARD_TOUCH_RST        8

// Not quadrature: each detent pulses one pin LOW (left -> A, right -> B).
#define BOARD_KNOB_LEFT        2
#define BOARD_KNOB_RIGHT       1
