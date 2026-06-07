#ifndef APP_SCOPE_H
#define APP_SCOPE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SCOPE_WAVE_POINTS 128U
#define APP_SCOPE_LOGIC_CHANNELS 4U
#define APP_SCOPE_TASK_COUNT 3U

typedef enum
{
    APP_SCOPE_MODE_RUN = 0,
    APP_SCOPE_MODE_STOP,
    APP_SCOPE_MODE_SINGLE,
} AppScopeRunMode_t;

typedef enum
{
    APP_SCOPE_SOURCE_ADC = 0,
    APP_SCOPE_SOURCE_PWM,
    APP_SCOPE_SOURCE_SINE,
    APP_SCOPE_SOURCE_STEP,
    APP_SCOPE_SOURCE_NOISE,
    APP_SCOPE_SOURCE_COUNT,
} AppScopeSource_t;

typedef enum
{
    APP_SCOPE_TASK_ACQ = 0,
    APP_SCOPE_TASK_CTRL,
    APP_SCOPE_TASK_UI,
} AppScopeTaskId_t;

typedef struct
{
    uint16_t waveform_mv[APP_SCOPE_WAVE_POINTS];
    uint16_t min_mv;
    uint16_t max_mv;
    uint16_t avg_mv;
    uint16_t vpp_mv;
    uint16_t trigger_mv;
    uint16_t duty_permille;
    uint32_t sample_rate_hz;
    uint32_t source_freq_hz;
    uint32_t frame_count;
    uint32_t trigger_hits;
    uint32_t overrun_count;
    uint32_t adc_event_count;
    uint32_t last_update_ms;
    uint32_t task_heartbeat[APP_SCOPE_TASK_COUNT];
    uint32_t logic_edges[APP_SCOPE_LOGIC_CHANNELS];
    uint16_t task_stack_min_words[APP_SCOPE_TASK_COUNT];
    uint8_t logic_level[APP_SCOPE_LOGIC_CHANNELS];
    uint8_t trigger_locked;
    uint8_t signal_present;
    uint8_t signal_clipped;
    uint8_t freq_valid;
    AppScopeRunMode_t run_mode;
    AppScopeSource_t source;
} AppScopeSnapshot_t;

void AppScope_Init(void);
void AppScope_AcquireTick(uint32_t now_ms);
uint8_t AppScope_AcquireReady(uint32_t now_ms);
void AppScope_GetSnapshot(AppScopeSnapshot_t *snapshot);
void AppScope_RecordTaskHealth(AppScopeTaskId_t task_id, uint32_t now_ms, uint16_t stack_free_words);
void AppScope_ToggleRun(void);
void AppScope_NextSource(void);
void AppScope_SingleCapture(void);
void AppScope_ClearStats(void);
uint8_t AppScope_IsRunning(void);
void AppScope_AdjustTrigger(int16_t delta_mv);
void AppScope_AutoTrigger(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SCOPE_H */
