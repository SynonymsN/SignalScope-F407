#ifndef BSP_SCOPE_ADC_H
#define BSP_SCOPE_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_SCOPE_ADC_SAMPLE_RATE_HZ 48000UL
#define BSP_SCOPE_ADC_BUFFER_SIZE    256U
#define BSP_SCOPE_ADC_HALF_SIZE      (BSP_SCOPE_ADC_BUFFER_SIZE / 2U)

typedef enum
{
    BSP_SCOPE_ADC_BLOCK_NONE = 0,
    BSP_SCOPE_ADC_BLOCK_FIRST,
    BSP_SCOPE_ADC_BLOCK_SECOND,
} BspScopeAdcBlock_t;

typedef void (*BspScopeAdcNotifyCallback_t)(void);

void BSP_SCOPE_ADC_Init(void);
uint8_t BSP_SCOPE_ADC_IsReady(void);
void BSP_SCOPE_ADC_SetNotifyCallback(BspScopeAdcNotifyCallback_t callback);
uint8_t BSP_SCOPE_ADC_TakeReadyBlock(BspScopeAdcBlock_t *block);
void BSP_SCOPE_ADC_CopyBlockMv(BspScopeAdcBlock_t block, uint16_t *dst, uint32_t count);
void BSP_SCOPE_ADC_CopyLatestMv(uint16_t *dst, uint32_t count);
uint32_t BSP_SCOPE_ADC_GetEventCount(void);
uint32_t BSP_SCOPE_ADC_GetOverrunCount(void);
void BSP_SCOPE_ADC_ResetCounters(void);
void BSP_SCOPE_ADC_DMA_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SCOPE_ADC_H */
