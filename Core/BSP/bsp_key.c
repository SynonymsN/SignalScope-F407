#include "bsp_key.h"

void BSP_KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin  = KEY0_PIN | KEY1_PIN | KEY2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = WKUP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(WKUP_PORT, &GPIO_InitStruct);
}

void BSP_KEY_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin  = KEY0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY0_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(KEY0_EXTI_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(KEY0_EXTI_IRQn);
}

uint8_t BSP_KEY0_Read(void)
{
    return (HAL_GPIO_ReadPin(KEY0_PORT, KEY0_PIN) == GPIO_PIN_RESET) ? KEY_PRESSED : KEY_RELEASED;
}

uint8_t BSP_KEY1_Read(void)
{
    return (HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN) == GPIO_PIN_RESET) ? KEY_PRESSED : KEY_RELEASED;
}

uint8_t BSP_KEY2_Read(void)
{
    return (HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN) == GPIO_PIN_RESET) ? KEY_PRESSED : KEY_RELEASED;
}

uint8_t BSP_WKUP_Read(void)
{
    return (HAL_GPIO_ReadPin(WKUP_PORT, WKUP_PIN) == GPIO_PIN_SET) ? KEY_PRESSED : KEY_RELEASED;
}
