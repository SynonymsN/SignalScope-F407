/**
  ******************************************************************************
  * @file    bsp_lcd.h
  * @brief   LCD驱动头文件 - 正点原子探索者F407 (FSMC接口)
  * @note    参考正点原子官方例程
  ******************************************************************************
  */

#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdio.h>

/* FSMC参数定义 */
#define LCD_FSMC_NEX    4   /* 使用NE4 */
#define LCD_FSMC_AX     6   /* 使用A6作为RS */

/* LCD地址定义
 * NE4基地址 = 0x6C000000
 * A6作为RS信号，16位模式下对应地址bit7
 * 命令地址(A6=0): 0x6C000000
 * 数据地址(A6=1): 0x6C000080
 */
typedef struct
{
    volatile uint16_t LCD_REG;      /* 偏移0x00 - 命令 */
    uint16_t reserved[63];          /* 填充 */
    volatile uint16_t LCD_RAM;      /* 偏移0x80 - 数据 */
} LCD_TypeDef;

#define LCD_BASE    ((uint32_t)0x6C000000)
#define LCD         ((LCD_TypeDef *)LCD_BASE)

/* 常用颜色定义 */
#define WHITE           0xFFFF
#define BLACK           0x0000
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define YELLOW          0xFFE0
#define CYAN            0x07FF
#define MAGENTA         0xF81F

/* LCD参数 */
typedef struct {
    uint16_t width;     /* LCD宽度 */
    uint16_t height;    /* LCD高度 */
    uint16_t id;        /* LCD ID */
    uint8_t  dir;       /* 横屏/竖屏: 0竖屏, 1横屏 */
    uint16_t wramcmd;   /* 写GRAM命令 */
    uint16_t setxcmd;   /* 设置X坐标命令 */
    uint16_t setycmd;   /* 设置Y坐标命令 */
} LCD_DevTypeDef;

extern LCD_DevTypeDef lcddev;

/* 背光控制 PB15 */
#define LCD_BL(x)   do{ (x) ? \
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET); \
                    }while(0)

/* 函数声明 */
void BSP_LCD_Init(void);
void BSP_LCD_Clear(uint16_t color);
void BSP_LCD_SetCursor(uint16_t x, uint16_t y);
void BSP_LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void BSP_LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void BSP_LCD_ShowChar(uint16_t x, uint16_t y, char c, uint16_t fc, uint16_t bc, uint8_t size);
void BSP_LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc, uint8_t size);
void BSP_LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t size);
void BSP_LCD_Printf(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LCD_H */
