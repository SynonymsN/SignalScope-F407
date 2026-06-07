#include "bsp_scope_adc.h"

#include <string.h>

#include "main.h"

ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_adc1;

static uint16_t s_adc_raw[BSP_SCOPE_ADC_BUFFER_SIZE];
static uint8_t s_adc_ready;
static volatile uint32_t s_ready_mask;
static volatile uint32_t s_event_count;
static volatile uint32_t s_overrun_count;
static BspScopeAdcNotifyCallback_t s_notify_callback;

#define ADC_BLOCK_FIRST_MASK  (1UL << 0)
#define ADC_BLOCK_SECOND_MASK (1UL << 1)

static uint16_t raw_to_mv(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * 3300UL + 2047UL) / 4095UL);
}

static void adc_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void dma_init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    hdma_adc1.Instance = DMA2_Stream0;
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

static void adc_init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_5;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

static void tim3_init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1749;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
}

void BSP_SCOPE_ADC_Init(void)
{
    memset(s_adc_raw, 0, sizeof(s_adc_raw));
    s_ready_mask = 0UL;
    s_event_count = 0UL;
    s_overrun_count = 0UL;

    adc_gpio_init();
    dma_init();
    adc_init();
    tim3_init();

    if (HAL_ADC_Start_DMA(&hadc1,
                          (uint32_t *)s_adc_raw,
                          BSP_SCOPE_ADC_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) {
        Error_Handler();
    }

    s_adc_ready = 1U;
}

uint8_t BSP_SCOPE_ADC_IsReady(void)
{
    return s_adc_ready;
}

void BSP_SCOPE_ADC_SetNotifyCallback(BspScopeAdcNotifyCallback_t callback)
{
    s_notify_callback = callback;
}

uint8_t BSP_SCOPE_ADC_TakeReadyBlock(BspScopeAdcBlock_t *block)
{
    uint32_t primask;
    uint32_t mask;

    if (block == 0) {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    mask = s_ready_mask;
    if ((mask & ADC_BLOCK_FIRST_MASK) != 0UL) {
        s_ready_mask &= ~ADC_BLOCK_FIRST_MASK;
        *block = BSP_SCOPE_ADC_BLOCK_FIRST;
    } else if ((mask & ADC_BLOCK_SECOND_MASK) != 0UL) {
        s_ready_mask &= ~ADC_BLOCK_SECOND_MASK;
        *block = BSP_SCOPE_ADC_BLOCK_SECOND;
    } else {
        *block = BSP_SCOPE_ADC_BLOCK_NONE;
    }
    if (primask == 0UL) {
        __enable_irq();
    }

    return (*block != BSP_SCOPE_ADC_BLOCK_NONE) ? 1U : 0U;
}

void BSP_SCOPE_ADC_CopyBlockMv(BspScopeAdcBlock_t block, uint16_t *dst, uint32_t count)
{
    uint32_t start_index;

    if ((dst == 0) || (count == 0U)) {
        return;
    }

    if (count > BSP_SCOPE_ADC_HALF_SIZE) {
        count = BSP_SCOPE_ADC_HALF_SIZE;
    }

    if (block == BSP_SCOPE_ADC_BLOCK_SECOND) {
        start_index = BSP_SCOPE_ADC_HALF_SIZE;
    } else {
        start_index = 0U;
    }

    for (uint32_t i = 0; i < count; i++) {
        dst[i] = raw_to_mv(s_adc_raw[start_index + i]);
    }
}

void BSP_SCOPE_ADC_CopyLatestMv(uint16_t *dst, uint32_t count)
{
    uint32_t ndtr;
    uint32_t write_index;
    uint32_t start_index;
    uint32_t stable_base;

    if ((dst == 0) || (count == 0U)) {
        return;
    }

    if (count > BSP_SCOPE_ADC_HALF_SIZE) {
        count = BSP_SCOPE_ADC_HALF_SIZE;
    }

    ndtr = __HAL_DMA_GET_COUNTER(&hdma_adc1);
    write_index = (BSP_SCOPE_ADC_BUFFER_SIZE - ndtr) % BSP_SCOPE_ADC_BUFFER_SIZE;
    stable_base = (write_index < BSP_SCOPE_ADC_HALF_SIZE) ? BSP_SCOPE_ADC_HALF_SIZE : 0U;
    start_index = stable_base + BSP_SCOPE_ADC_HALF_SIZE - count;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t src = start_index + i;
        dst[i] = raw_to_mv(s_adc_raw[src]);
    }
}

uint32_t BSP_SCOPE_ADC_GetEventCount(void)
{
    return s_event_count;
}

uint32_t BSP_SCOPE_ADC_GetOverrunCount(void)
{
    return s_overrun_count;
}

void BSP_SCOPE_ADC_ResetCounters(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_ready_mask = 0UL;
    s_event_count = 0UL;
    s_overrun_count = 0UL;
    if (primask == 0UL) {
        __enable_irq();
    }
}

void BSP_SCOPE_ADC_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

static void mark_ready_from_isr(uint32_t ready_bit)
{
    if ((s_ready_mask & ready_bit) != 0UL) {
        s_overrun_count++;
    }

    s_ready_mask |= ready_bit;
    s_event_count++;

    if (s_notify_callback != 0) {
        s_notify_callback();
    }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if ((hadc != 0) && (hadc->Instance == ADC1)) {
        mark_ready_from_isr(ADC_BLOCK_FIRST_MASK);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if ((hadc != 0) && (hadc->Instance == ADC1)) {
        mark_ready_from_isr(ADC_BLOCK_SECOND_MASK);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if ((hadc != 0) && (hadc->Instance == ADC1)) {
        s_overrun_count++;
    }
}
