#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ALIENTEK Explorer STM32F407 key map.
 * KEY0/KEY1/KEY2 are active-low. WK_UP is active-high.
 */
#define KEY0_PIN          GPIO_PIN_4
#define KEY0_PORT         GPIOE
#define KEY1_PIN          GPIO_PIN_3
#define KEY1_PORT         GPIOE
#define KEY2_PIN          GPIO_PIN_2
#define KEY2_PORT         GPIOE
#define WKUP_PIN          GPIO_PIN_0
#define WKUP_PORT         GPIOA

#define KEY0_EXTI_IRQn    EXTI4_IRQn

#define KEY_PRESSED       1U
#define KEY_RELEASED      0U

void BSP_KEY_Init(void);
void BSP_KEY_EXTI_Init(void);
uint8_t BSP_KEY0_Read(void);
uint8_t BSP_KEY1_Read(void);
uint8_t BSP_KEY2_Read(void);
uint8_t BSP_WKUP_Read(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
