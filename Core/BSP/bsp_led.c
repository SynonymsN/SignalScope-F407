/**
  ******************************************************************************
  * @file    bsp_led.c
  * @brief   LED驱动实现 (BSP层 - 纯HAL库操作，不含RTOS)
  ******************************************************************************
  */

#include "bsp_led.h"

/**
  * @brief  LED GPIO初始化
  * @note   正点原子探索者：LED低电平点亮
  */
void BSP_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 使能GPIOF时钟 */
    __HAL_RCC_GPIOF_CLK_ENABLE();
    
    /* 默认熄灭LED (高电平) */
    HAL_GPIO_WritePin(LED0_PORT, LED0_PIN | LED1_PIN, GPIO_PIN_SET);
    
    /* 配置LED引脚为推挽输出 */
    GPIO_InitStruct.Pin   = LED0_PIN | LED1_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED0_PORT, &GPIO_InitStruct);
}

/* LED0 操作函数 */
void BSP_LED0_On(void)
{
    HAL_GPIO_WritePin(LED0_PORT, LED0_PIN, GPIO_PIN_RESET);
}

void BSP_LED0_Off(void)
{
    HAL_GPIO_WritePin(LED0_PORT, LED0_PIN, GPIO_PIN_SET);
}

void BSP_LED0_Toggle(void)
{
    HAL_GPIO_TogglePin(LED0_PORT, LED0_PIN);
}

/* LED1 操作函数 */
void BSP_LED1_On(void)
{
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
}

void BSP_LED1_Off(void)
{
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
}

void BSP_LED1_Toggle(void)
{
    HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
}
