/**
 * @file lv_port_disp.h
 * @brief LVGL显示接口 - 适配正点原子探索者F407 FSMC LCD
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_H */
