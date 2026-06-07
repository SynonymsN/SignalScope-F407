#include "app_scope.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_scope_adc.h"

#define SCOPE_DEFAULT_TRIGGER_MV 1650U
#define SCOPE_DEFAULT_SAMPLE_HZ  48000UL
#define SCOPE_TRIGGER_MIN_MV     100U
#define SCOPE_TRIGGER_MAX_MV     3200U
#define SCOPE_SIGNAL_MIN_VPP_MV  150U
#define SCOPE_CLIP_LOW_MV        60U
#define SCOPE_CLIP_HIGH_MV       3240U

static AppScopeSnapshot_t s_scope;
static AppScopeSnapshot_t s_build_snapshot;
static AppScopeSnapshot_t s_publish_snapshot[3];
static volatile uint8_t s_front_snapshot;
static uint8_t s_write_snapshot = 1U;
static volatile int8_t s_read_snapshot = -1;
static uint32_t s_phase;
static uint32_t s_noise = 0x13572468UL;
static uint8_t s_single_armed;
static uint8_t s_prev_logic[APP_SCOPE_LOGIC_CHANNELS];

static const int16_t s_sine_lut[32] = {
    0, 195, 383, 556, 707, 831, 924, 981,
    1000, 981, 924, 831, 707, 556, 383, 195,
    0, -195, -383, -556, -707, -831, -924, -981,
    -1000, -981, -924, -831, -707, -556, -383, -195
};

static void publish_snapshot(const AppScopeSnapshot_t *snapshot)
{
    uint8_t written = s_write_snapshot;

    memcpy(&s_publish_snapshot[written], snapshot, sizeof(*snapshot));

    taskENTER_CRITICAL();
    s_front_snapshot = written;
    for (uint8_t i = 0U; i < 3U; i++) {
        if ((i != s_front_snapshot) && ((int8_t)i != s_read_snapshot)) {
            s_write_snapshot = i;
            break;
        }
    }
    taskEXIT_CRITICAL();
}

static uint32_t source_freq(AppScopeSource_t source)
{
    switch (source) {
    case APP_SCOPE_SOURCE_PWM:
        return 1000UL;
    case APP_SCOPE_SOURCE_SINE:
        return 2500UL;
    case APP_SCOPE_SOURCE_STEP:
        return 200UL;
    case APP_SCOPE_SOURCE_ADC:
    case APP_SCOPE_SOURCE_NOISE:
    default:
        return 0UL;
    }
}

static uint16_t clamp_mv(int32_t value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > 3300) {
        return 3300U;
    }
    return (uint16_t)value;
}

static int32_t next_noise_mv(void)
{
    s_noise = (s_noise * 1664525UL) + 1013904223UL;
    return (int32_t)((s_noise >> 24) & 0x7FU) - 64;
}

static uint16_t generate_sample(AppScopeSource_t source, uint32_t phase)
{
    uint32_t local = phase & 0x3FFUL;
    int32_t mv;

    switch (source) {
    case APP_SCOPE_SOURCE_PWM:
        if (local < 512UL) {
            mv = 2440 + next_noise_mv();
        } else {
            mv = 860 + next_noise_mv();
        }
        if ((local < 36UL) || ((local > 512UL) && (local < 548UL))) {
            mv += 180 - (int32_t)(local & 0x1FUL) * 8;
        }
        break;

    case APP_SCOPE_SOURCE_SINE:
        mv = 1650 + s_sine_lut[(local >> 5) & 0x1FU] + next_noise_mv();
        break;

    case APP_SCOPE_SOURCE_STEP:
        if (local < 210UL) {
            mv = 620 + next_noise_mv();
        } else if (local < 260UL) {
            mv = 620 + (int32_t)((local - 210UL) * 42UL) + next_noise_mv();
        } else if (local < 768UL) {
            mv = 2720 + next_noise_mv();
        } else {
            mv = 1220 + next_noise_mv();
        }
        break;

    case APP_SCOPE_SOURCE_NOISE:
    default:
        mv = 1650 + (next_noise_mv() * 9) + (int32_t)((local & 0x3FUL) * 3);
        break;
    }

    return clamp_mv(mv);
}

static void update_logic(AppScopeSource_t source, uint32_t phase, uint8_t level[APP_SCOPE_LOGIC_CHANNELS])
{
    uint32_t local = phase & 0x3FFUL;

    level[0] = (local < 512UL) ? 1U : 0U;
    level[1] = ((phase >> 9) & 0x01UL) ? 1U : 0U;
    level[2] = ((phase >> 7) & 0x01UL) ? 1U : 0U;
    level[3] = (source == APP_SCOPE_SOURCE_STEP) ? ((local > 210UL) && (local < 768UL)) : ((phase >> 8) & 0x01UL);
}

static uint32_t estimate_freq_hz(const uint16_t *wave, uint16_t trigger_mv)
{
    uint32_t first_edge = 0UL;
    uint32_t last_edge = 0UL;
    uint32_t edge_count = 0UL;

    for (uint32_t i = 1; i < APP_SCOPE_WAVE_POINTS; i++) {
        if ((wave[i - 1U] < trigger_mv) && (wave[i] >= trigger_mv)) {
            if (edge_count == 0UL) {
                first_edge = i;
            }
            last_edge = i;
            edge_count++;
        }
    }

    if ((edge_count < 2UL) || (last_edge <= first_edge)) {
        return 0UL;
    }

    return (BSP_SCOPE_ADC_SAMPLE_RATE_HZ * (edge_count - 1UL)) / (last_edge - first_edge);
}

void AppScope_Init(void)
{
    memset(&s_scope, 0, sizeof(s_scope));
    memset(&s_build_snapshot, 0, sizeof(s_build_snapshot));
    memset(s_publish_snapshot, 0, sizeof(s_publish_snapshot));
    s_scope.run_mode = APP_SCOPE_MODE_RUN;
    s_scope.source = APP_SCOPE_SOURCE_ADC;
    s_scope.sample_rate_hz = SCOPE_DEFAULT_SAMPLE_HZ;
    s_scope.source_freq_hz = source_freq(s_scope.source);
    s_scope.trigger_mv = SCOPE_DEFAULT_TRIGGER_MV;
    s_scope.duty_permille = 500U;
    memcpy(&s_publish_snapshot[0], &s_scope, sizeof(s_scope));
    s_front_snapshot = 0U;
    s_write_snapshot = 1U;
    s_read_snapshot = -1;
}

static void process_acquisition(uint32_t now_ms, BspScopeAdcBlock_t block, uint8_t use_block)
{
    uint16_t local_wave[APP_SCOPE_WAVE_POINTS];
    AppScopeSnapshot_t *next = &s_build_snapshot;
    uint8_t logic[APP_SCOPE_LOGIC_CHANNELS];
    AppScopeRunMode_t mode;
    AppScopeSource_t source;
    uint32_t phase;
    uint16_t trigger_mv;
    uint8_t single_armed;
    uint16_t min_mv = 3300U;
    uint16_t max_mv = 0U;
    uint32_t sum = 0UL;
    uint32_t high_count = 0UL;
    uint32_t triggers = 0UL;
    uint8_t trigger_locked = 0U;
    uint8_t signal_present;
    uint8_t signal_clipped;
    uint8_t freq_valid;
    uint16_t vpp_mv;
    uint32_t measured_freq_hz;

    memset(next, 0, sizeof(*next));

    taskENTER_CRITICAL();
    mode = s_scope.run_mode;
    source = s_scope.source;
    phase = s_phase;
    trigger_mv = s_scope.trigger_mv;
    single_armed = s_single_armed;
    taskEXIT_CRITICAL();

    if ((mode == APP_SCOPE_MODE_STOP) && (single_armed == 0U)) {
        return;
    }

    if ((source == APP_SCOPE_SOURCE_ADC) && (BSP_SCOPE_ADC_IsReady() != 0U)) {
        if (use_block != 0U) {
            BSP_SCOPE_ADC_CopyBlockMv(block, local_wave, APP_SCOPE_WAVE_POINTS);
        } else {
            BSP_SCOPE_ADC_CopyLatestMv(local_wave, APP_SCOPE_WAVE_POINTS);
        }
    } else {
        for (uint32_t i = 0; i < APP_SCOPE_WAVE_POINTS; i++) {
            uint32_t sample_phase = phase + (i * 26UL);
            local_wave[i] = generate_sample(source, sample_phase);
        }
    }

    for (uint32_t i = 0; i < APP_SCOPE_WAVE_POINTS; i++) {
        uint16_t mv = local_wave[i];

        if (mv < min_mv) {
            min_mv = mv;
        }
        if (mv > max_mv) {
            max_mv = mv;
        }
        sum += mv;

        if (mv >= trigger_mv) {
            high_count++;
        }

        if ((i > 0U) &&
            (local_wave[i - 1U] < trigger_mv) &&
            (mv >= trigger_mv)) {
            triggers++;
        }
    }

    vpp_mv = (uint16_t)(max_mv - min_mv);
    signal_present = (vpp_mv >= SCOPE_SIGNAL_MIN_VPP_MV) ? 1U : 0U;
    signal_clipped = ((min_mv <= SCOPE_CLIP_LOW_MV) || (max_mv >= SCOPE_CLIP_HIGH_MV)) ? 1U : 0U;
    trigger_locked = ((signal_present != 0U) && (triggers > 0UL)) ? 1U : 0U;
    measured_freq_hz = (source == APP_SCOPE_SOURCE_ADC) ? estimate_freq_hz(local_wave, trigger_mv) : source_freq(source);
    freq_valid = ((signal_present != 0U) && (measured_freq_hz > 0UL)) ? 1U : 0U;

    if (source == APP_SCOPE_SOURCE_ADC) {
        logic[0] = (local_wave[APP_SCOPE_WAVE_POINTS - 1U] >= trigger_mv) ? 1U : 0U;
        logic[1] = trigger_locked;
        logic[2] = signal_present;
        logic[3] = signal_clipped;
    } else {
        update_logic(source, phase, logic);
    }

    memcpy(next->waveform_mv, local_wave, sizeof(local_wave));
    next->min_mv = min_mv;
    next->max_mv = max_mv;
    next->avg_mv = (uint16_t)(sum / APP_SCOPE_WAVE_POINTS);
    next->vpp_mv = vpp_mv;
    next->trigger_mv = trigger_mv;
    next->duty_permille = (uint16_t)((high_count * 1000UL) / APP_SCOPE_WAVE_POINTS);
    next->sample_rate_hz = SCOPE_DEFAULT_SAMPLE_HZ;
    next->source_freq_hz = measured_freq_hz;
    next->overrun_count = BSP_SCOPE_ADC_GetOverrunCount();
    next->adc_event_count = BSP_SCOPE_ADC_GetEventCount();
    next->last_update_ms = now_ms;
    next->trigger_locked = trigger_locked;
    next->signal_present = signal_present;
    next->signal_clipped = signal_clipped;
    next->freq_valid = freq_valid;
    next->source = source;
    next->run_mode = mode;
    memcpy(next->logic_level, logic, sizeof(logic));

    taskENTER_CRITICAL();
    next->frame_count = s_scope.frame_count + 1UL;
    next->trigger_hits = s_scope.trigger_hits + triggers;

    s_scope.min_mv = min_mv;
    s_scope.max_mv = max_mv;
    s_scope.avg_mv = next->avg_mv;
    s_scope.vpp_mv = vpp_mv;
    s_scope.duty_permille = next->duty_permille;
    s_scope.source_freq_hz = measured_freq_hz;
    s_scope.last_update_ms = now_ms;
    s_scope.frame_count = next->frame_count;
    s_scope.trigger_hits += triggers;
    s_scope.overrun_count = next->overrun_count;
    s_scope.adc_event_count = next->adc_event_count;
    s_scope.trigger_locked = trigger_locked;
    s_scope.signal_present = signal_present;
    s_scope.signal_clipped = signal_clipped;
    s_scope.freq_valid = freq_valid;

    for (uint32_t ch = 0; ch < APP_SCOPE_LOGIC_CHANNELS; ch++) {
        s_scope.logic_level[ch] = logic[ch];
        if (logic[ch] != s_prev_logic[ch]) {
            s_scope.logic_edges[ch]++;
            s_prev_logic[ch] = logic[ch];
        }
        next->logic_edges[ch] = s_scope.logic_edges[ch];
    }

    for (uint32_t i = 0; i < APP_SCOPE_TASK_COUNT; i++) {
        next->task_heartbeat[i] = s_scope.task_heartbeat[i];
        next->task_stack_min_words[i] = s_scope.task_stack_min_words[i];
    }

    if (s_single_armed != 0U) {
        s_single_armed = 0U;
        s_scope.run_mode = APP_SCOPE_MODE_STOP;
        next->run_mode = APP_SCOPE_MODE_STOP;
    }

    if (source != APP_SCOPE_SOURCE_ADC) {
        s_phase += 37UL;
    }
    taskEXIT_CRITICAL();

    publish_snapshot(next);
}

void AppScope_AcquireTick(uint32_t now_ms)
{
    process_acquisition(now_ms, BSP_SCOPE_ADC_BLOCK_NONE, 0U);
}

uint8_t AppScope_AcquireReady(uint32_t now_ms)
{
    BspScopeAdcBlock_t block;

    if (BSP_SCOPE_ADC_TakeReadyBlock(&block) == 0U) {
        return 0U;
    }

    process_acquisition(now_ms, block, 1U);
    return 1U;
}

void AppScope_GetSnapshot(AppScopeSnapshot_t *snapshot)
{
    uint8_t index;

    if (snapshot == 0) {
        return;
    }

    taskENTER_CRITICAL();
    index = s_front_snapshot;
    s_read_snapshot = (int8_t)index;
    taskEXIT_CRITICAL();

    memcpy(snapshot, &s_publish_snapshot[index], sizeof(*snapshot));

    taskENTER_CRITICAL();
    if (s_read_snapshot == (int8_t)index) {
        s_read_snapshot = -1;
    }
    taskEXIT_CRITICAL();
}

void AppScope_RecordTaskHealth(AppScopeTaskId_t task_id, uint32_t now_ms, uint16_t stack_free_words)
{
    if ((uint32_t)task_id >= APP_SCOPE_TASK_COUNT) {
        return;
    }

    taskENTER_CRITICAL();
    s_scope.task_heartbeat[task_id] = now_ms;
    if ((s_scope.task_stack_min_words[task_id] == 0U) ||
        (stack_free_words < s_scope.task_stack_min_words[task_id])) {
        s_scope.task_stack_min_words[task_id] = stack_free_words;
    }
    taskEXIT_CRITICAL();
}

void AppScope_ToggleRun(void)
{
    taskENTER_CRITICAL();
    if (s_scope.run_mode == APP_SCOPE_MODE_RUN) {
        s_scope.run_mode = APP_SCOPE_MODE_STOP;
    } else {
        s_scope.run_mode = APP_SCOPE_MODE_RUN;
        s_single_armed = 0U;
    }
    taskEXIT_CRITICAL();
}

void AppScope_NextSource(void)
{
    taskENTER_CRITICAL();
    s_scope.source = APP_SCOPE_SOURCE_ADC;
    s_scope.source_freq_hz = source_freq(s_scope.source);
    taskEXIT_CRITICAL();
}

void AppScope_SingleCapture(void)
{
    taskENTER_CRITICAL();
    s_scope.run_mode = APP_SCOPE_MODE_SINGLE;
    s_single_armed = 1U;
    taskEXIT_CRITICAL();
}

void AppScope_ClearStats(void)
{
    taskENTER_CRITICAL();
    s_scope.frame_count = 0UL;
    s_scope.trigger_hits = 0UL;
    s_scope.overrun_count = 0UL;
    s_scope.adc_event_count = 0UL;
    for (uint32_t i = 0; i < APP_SCOPE_LOGIC_CHANNELS; i++) {
        s_scope.logic_edges[i] = 0UL;
    }
    taskEXIT_CRITICAL();
    BSP_SCOPE_ADC_ResetCounters();
}

uint8_t AppScope_IsRunning(void)
{
    uint8_t running;

    taskENTER_CRITICAL();
    running = (s_scope.run_mode == APP_SCOPE_MODE_RUN) ? 1U : 0U;
    taskEXIT_CRITICAL();

    return running;
}

void AppScope_AdjustTrigger(int16_t delta_mv)
{
    int32_t next;

    taskENTER_CRITICAL();
    next = (int32_t)s_scope.trigger_mv + (int32_t)delta_mv;
    if (next < (int32_t)SCOPE_TRIGGER_MIN_MV) {
        next = (int32_t)SCOPE_TRIGGER_MIN_MV;
    } else if (next > (int32_t)SCOPE_TRIGGER_MAX_MV) {
        next = (int32_t)SCOPE_TRIGGER_MAX_MV;
    }
    s_scope.trigger_mv = (uint16_t)next;
    taskEXIT_CRITICAL();
}

void AppScope_AutoTrigger(void)
{
    uint32_t midpoint;

    taskENTER_CRITICAL();
    if (s_scope.vpp_mv >= SCOPE_SIGNAL_MIN_VPP_MV) {
        midpoint = ((uint32_t)s_scope.min_mv + (uint32_t)s_scope.max_mv) / 2UL;
        if (midpoint < SCOPE_TRIGGER_MIN_MV) {
            midpoint = SCOPE_TRIGGER_MIN_MV;
        } else if (midpoint > SCOPE_TRIGGER_MAX_MV) {
            midpoint = SCOPE_TRIGGER_MAX_MV;
        }
        s_scope.trigger_mv = (uint16_t)midpoint;
    } else {
        s_scope.trigger_mv = SCOPE_DEFAULT_TRIGGER_MV;
    }
    taskEXIT_CRITICAL();
}
