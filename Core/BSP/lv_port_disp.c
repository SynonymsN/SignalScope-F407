/**
 * @file lv_port_disp.c
 * @brief LVGL display port for the FSMC LCD on the STM32F407 board.
 */

#include "lv_port_disp.h"
#include "bsp_lcd.h"

#define DISP_BUF_LINES             10U
#define DISP_BUF_MAX_WIDTH         480U
#define DISP_FORCE_LANDSCAPE       1U

static lv_disp_draw_buf_t draw_buf_dsc;
static lv_color_t draw_buf[DISP_BUF_MAX_WIDTH * DISP_BUF_LINES];

static lv_disp_drv_t disp_drv;
static uint8_t s_rotate_landscape;
static uint16_t s_phys_w;
static uint16_t s_phys_h;

static void lcd_set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD->LCD_REG = lcddev.setxcmd;
    LCD->LCD_RAM = (uint16_t)(x1 >> 8);
    LCD->LCD_RAM = (uint16_t)(x1 & 0xFFU);
    LCD->LCD_RAM = (uint16_t)(x2 >> 8);
    LCD->LCD_RAM = (uint16_t)(x2 & 0xFFU);

    LCD->LCD_REG = lcddev.setycmd;
    LCD->LCD_RAM = (uint16_t)(y1 >> 8);
    LCD->LCD_RAM = (uint16_t)(y1 & 0xFFU);
    LCD->LCD_RAM = (uint16_t)(y2 >> 8);
    LCD->LCD_RAM = (uint16_t)(y2 & 0xFFU);

    LCD->LCD_REG = lcddev.wramcmd;
}

static void flush_portrait(const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t total = (uint32_t)(area->x2 - area->x1 + 1) *
                     (uint32_t)(area->y2 - area->y1 + 1);

    lcd_set_addr_window((uint16_t)area->x1,
                        (uint16_t)area->y1,
                        (uint16_t)area->x2,
                        (uint16_t)area->y2);

    for (uint32_t i = 0; i < total; i++) {
        LCD->LCD_RAM = color_p[i].full;
    }
}

static void flush_landscape_clockwise(const lv_area_t *area, lv_color_t *color_p)
{
    uint16_t virtual_w = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t phys_x1 = (uint16_t)area->y1;
    uint16_t phys_x2 = (uint16_t)area->y2;
    uint16_t phys_y1 = (uint16_t)(s_phys_h - 1U - (uint16_t)area->x2);
    uint16_t phys_y2 = (uint16_t)(s_phys_h - 1U - (uint16_t)area->x1);

    lcd_set_addr_window(phys_x1, phys_y1, phys_x2, phys_y2);

    for (uint16_t py = phys_y1; py <= phys_y2; py++) {
        uint16_t vx = (uint16_t)(s_phys_h - 1U - py);

        for (uint16_t px = phys_x1; px <= phys_x2; px++) {
            uint16_t vy = px;
            uint32_t src_index = (uint32_t)(vy - (uint16_t)area->y1) * virtual_w +
                                 (uint32_t)(vx - (uint16_t)area->x1);

            LCD->LCD_RAM = color_p[src_index].full;
        }
    }
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)drv;

    if (s_rotate_landscape != 0U) {
        flush_landscape_clockwise(area, color_p);
    } else {
        flush_portrait(area, color_p);
    }

    lv_disp_flush_ready(drv);
}

void lv_port_disp_init(void)
{
    uint16_t hor_res;
    uint16_t ver_res;
    uint32_t buf_size;

    s_phys_w = lcddev.width;
    s_phys_h = lcddev.height;
    s_rotate_landscape = ((DISP_FORCE_LANDSCAPE != 0U) && (s_phys_h > s_phys_w)) ? 1U : 0U;

    if (s_rotate_landscape != 0U) {
        hor_res = s_phys_h;
        ver_res = s_phys_w;
    } else {
        hor_res = s_phys_w;
        ver_res = s_phys_h;
    }

    buf_size = (uint32_t)hor_res * DISP_BUF_LINES;
    if (buf_size > (sizeof(draw_buf) / sizeof(draw_buf[0]))) {
        buf_size = sizeof(draw_buf) / sizeof(draw_buf[0]);
    }

    lv_disp_draw_buf_init(&draw_buf_dsc, draw_buf, NULL, buf_size);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = hor_res;
    disp_drv.ver_res = ver_res;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    lv_disp_drv_register(&disp_drv);
}
