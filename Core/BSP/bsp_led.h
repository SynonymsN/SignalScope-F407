/**
  ******************************************************************************
  * @file    bsp_led.h
  * @brief   LED驱动头文件 (BSP层 - 不含RTOS逻辑)
  ******************************************************************************
  */

#ifndef __BSP_LED_H
#define __BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* LED引脚定义 - 正点原子探索者F407 */
#define LED0_PIN          GPIO_PIN_9
#define LED0_PORT         GPIOF

#define LED1_PIN          GPIO_PIN_10
#define LED1_PORT         GPIOF

/* 函数声明 */
void BSP_LED_Init(void);
void BSP_LED0_On(void);
void BSP_LED0_Off(void);
void BSP_LED0_Toggle(void);
void BSP_LED1_On(void);
void BSP_LED1_Off(void);
void BSP_LED1_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LED_H */
