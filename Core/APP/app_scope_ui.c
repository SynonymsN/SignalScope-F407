#include "app_scope_ui.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"

#define ACTION_TEXT_LEN 96U

static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_source;
static lv_obj_t *s_lbl_status;
static lv_obj_t *s_lbl_rate;
static lv_obj_t *s_lbl_freq;
static lv_obj_t *s_lbl_vpp;
static lv_obj_t *s_lbl_avg;
static lv_obj_t *s_lbl_minmax;
static lv_obj_t *s_lbl_duty;
static lv_obj_t *s_lbl_action;
static lv_obj_t *s_lbl_ctrl_run;
static lv_obj_t *s_lbl_ctrl_source;
static lv_obj_t *s_lbl_ctrl_trig_down;
static lv_obj_t *s_lbl_ctrl_trig_up;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_series;
static lv_obj_t *s_logic_dot[APP_SCOPE_LOGIC_CHANNELS];
static lv_obj_t *s_logic_label[APP_SCOPE_LOGIC_CHANNELS];
static char s_pending_action[ACTION_TEXT_LEN];
static uint8_t s_action_dirty;
static TickType_t s_action_started_at;
static uint8_t s_action_visible;

static const char *mode_text(AppScopeRunMode_t mode)
{
    switch (mode) {
    case APP_SCOPE_MODE_RUN:
        return "RUN";
    case APP_SCOPE_MODE_SINGLE:
        return "SINGLE";
    case APP_SCOPE_MODE_STOP:
    default:
        return "STOP";
    }
}

static const char *source_text(AppScopeSource_t source)
{
    switch (source) {
    case APP_SCOPE_SOURCE_PWM:
        return "SIM PWM";
    case APP_SCOPE_SOURCE_SINE:
        return "SIM SINE";
    case APP_SCOPE_SOURCE_STEP:
        return "SIM STEP";
    case APP_SCOPE_SOURCE_NOISE:
        return "SIM NOISE";
    case APP_SCOPE_SOURCE_ADC:
        return "EXT PA5";
    default:
        return "SIM NOISE";
    }
}

static lv_color_t mode_color(AppScopeRunMode_t mode)
{
    switch (mode) {
    case APP_SCOPE_MODE_RUN:
        return lv_color_hex(0x22C55E);
    case APP_SCOPE_MODE_SINGLE:
        return lv_color_hex(0xF59E0B);
    case APP_SCOPE_MODE_STOP:
    default:
        return lv_color_hex(0xEF4444);
    }
}

static lv_obj_t *make_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *panel = lv_obj_create(parent);

    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x151D2F), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x314056), 0);
    lv_obj_set_style_pad_all(panel, 5, 0);

    return panel;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

    return label;
}

static lv_obj_t *make_metric_card(lv_obj_t *parent,
                                  lv_coord_t x,
                                  lv_coord_t y,
                                  lv_coord_t w,
                                  lv_coord_t h,
                                  const char *caption,
                                  lv_color_t value_color,
                                  lv_obj_t **value_label)
{
    lv_obj_t *card = make_panel(parent, x, y, w, h);
    lv_obj_t *cap;

    lv_obj_set_style_bg_color(card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x26364B), 0);
    lv_obj_set_style_pad_all(card, 3, 0);

    cap = make_label(card, caption, &lv_font_montserrat_14, lv_color_hex(0x7DD3FC));
    lv_obj_set_pos(cap, 0, -1);
    lv_obj_set_width(cap, w - 6);

    *value_label = make_label(card, "--", &lv_font_montserrat_14, value_color);
    lv_obj_set_pos(*value_label, 0, 16);
    lv_obj_set_width(*value_label, w - 6);

    return card;
}

static lv_obj_t *make_control_box(lv_obj_t *parent,
                                  lv_coord_t x,
                                  lv_coord_t w,
                                  const char *text,
                                  lv_color_t color)
{
    lv_obj_t *box = make_panel(parent, x, 3, w, 21);
    lv_obj_t *label;

    lv_obj_set_style_bg_color(box, lv_color_hex(0x101827), 0);
    lv_obj_set_style_border_color(box, color, 0);
    lv_obj_set_style_pad_all(box, 2, 0);

    label = make_label(box, text, &lv_font_montserrat_14, color);
    lv_obj_set_width(label, w - 4);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);

    return label;
}

static void format_hz_value(char *buf, size_t len, uint32_t hz)
{
    if (hz == 0UL) {
        (void)snprintf(buf, len, "rand");
    } else if (hz >= 1000UL) {
        (void)snprintf(buf, len,
                       "%lu.%luk",
                       (unsigned long)(hz / 1000UL),
                       (unsigned long)((hz % 1000UL) / 100UL));
    } else {
        (void)snprintf(buf, len, "%luHz", (unsigned long)hz);
    }
}

static void format_signal_hz_value(char *buf, size_t len, const AppScopeSnapshot_t *snapshot)
{
    if ((snapshot->source == APP_SCOPE_SOURCE_ADC) && (snapshot->signal_present == 0U)) {
        (void)snprintf(buf, len, "--");
    } else if ((snapshot->source == APP_SCOPE_SOURCE_ADC) && (snapshot->freq_valid == 0U)) {
        (void)snprintf(buf, len, "wait");
    } else {
        format_hz_value(buf, len, snapshot->source_freq_hz);
    }
}

static void format_mv_value(char *buf, size_t len, uint16_t mv)
{
    (void)snprintf(buf, len,
                   "%lu.%02luV",
                   (unsigned long)(mv / 1000U),
                   (unsigned long)((mv % 1000U) / 10U));
}

static void format_minmax_value(char *buf, size_t len, uint16_t min_mv, uint16_t max_mv)
{
    (void)snprintf(buf, len,
                   "%lu.%lu/%lu.%lu",
                   (unsigned long)(min_mv / 1000U),
                   (unsigned long)((min_mv % 1000U) / 100U),
                   (unsigned long)(max_mv / 1000U),
                   (unsigned long)((max_mv % 1000U) / 100U));
}

static void format_diag_action(char *buf, size_t len, const AppScopeSnapshot_t *snapshot)
{
    uint32_t hb_age[APP_SCOPE_TASK_COUNT];
    uint16_t stack_acq = snapshot->task_stack_min_words[APP_SCOPE_TASK_ACQ];
    uint16_t stack_ctrl = snapshot->task_stack_min_words[APP_SCOPE_TASK_CTRL];
    uint16_t stack_ui = snapshot->task_stack_min_words[APP_SCOPE_TASK_UI];

    for (uint32_t i = 0; i < APP_SCOPE_TASK_COUNT; i++) {
        if (snapshot->last_update_ms >= snapshot->task_heartbeat[i]) {
            hb_age[i] = snapshot->last_update_ms - snapshot->task_heartbeat[i];
        } else {
            hb_age[i] = 0UL;
        }
    }

    (void)snprintf(buf,
                   len,
                   "EV%lu OV%lu STK%u/%u/%u HB%lu/%lu/%lu",
                   (unsigned long)snapshot->adc_event_count,
                   (unsigned long)snapshot->overrun_count,
                   stack_acq,
                   stack_ctrl,
                   stack_ui,
                   (unsigned long)hb_age[APP_SCOPE_TASK_ACQ],
                   (unsigned long)hb_age[APP_SCOPE_TASK_CTRL],
                   (unsigned long)hb_age[APP_SCOPE_TASK_UI]);
}

static uint8_t apply_pending_action(void)
{
    char text[ACTION_TEXT_LEN];
    uint8_t dirty;
    TickType_t now_tick = xTaskGetTickCount();

    taskENTER_CRITICAL();
    dirty = s_action_dirty;
    if (dirty != 0U) {
        (void)strncpy(text, s_pending_action, sizeof(text) - 1U);
        text[sizeof(text) - 1U] = '\0';
        s_action_dirty = 0U;
    }
    taskEXIT_CRITICAL();

    if ((dirty != 0U) && (s_lbl_action != 0)) {
        lv_label_set_text(s_lbl_action, text);
        s_action_started_at = now_tick;
        s_action_visible = 1U;
        return 1U;
    }

    if ((s_lbl_action != 0) && (s_action_visible != 0U)) {
        if ((TickType_t)(now_tick - s_action_started_at) < pdMS_TO_TICKS(1200)) {
            return 1U;
        }
        s_action_visible = 0U;
        return 0U;
    }

    return 0U;
}

void AppScopeUi_Create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    lv_coord_t h = lv_disp_get_ver_res(NULL);
    lv_coord_t margin = (h <= 260) ? 6 : 8;
    lv_coord_t header_h = (h <= 260) ? 36 : 44;
    lv_coord_t footer_h = 28;
    lv_coord_t logic_h = (h <= 260) ? 28 : 34;
    lv_coord_t right_w = (w >= 430) ? 150 : 126;
    lv_coord_t chart_x = margin;
    lv_coord_t chart_y = header_h + margin;
    lv_coord_t chart_w = w - (3 * margin) - right_w;
    lv_coord_t chart_h = h - chart_y - logic_h - footer_h - (3 * margin);
    lv_coord_t stats_x = chart_x + chart_w + margin;
    lv_coord_t logic_y = chart_y + chart_h + margin;
    lv_coord_t footer_y = logic_y + logic_h + margin;
    lv_obj_t *chart_panel;
    lv_obj_t *stats_panel;
    lv_obj_t *logic_panel;
    lv_obj_t *footer_panel;
    lv_obj_t *title;
    lv_coord_t card_gap = 4;
    lv_coord_t card_w;
    lv_coord_t card_h;
    lv_coord_t ctrl_w = (w >= 430) ? 62 : 48;
    lv_coord_t ctrl_gap = 4;
    lv_coord_t action_x = 2 + (ctrl_w * 4) + (ctrl_gap * 4);

    if (chart_w < 150) {
        chart_w = w - (2 * margin);
        right_w = chart_w;
        stats_x = margin;
        chart_h = (h - chart_y - logic_h - footer_h - (4 * margin)) / 2;
        logic_y = chart_y + chart_h + margin + chart_h + margin;
        footer_y = logic_y + logic_h + margin;
    }

    lv_obj_clean(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    title = make_label(scr,
                       "SignalScope-F407",
                       (h <= 260) ? &lv_font_montserrat_16 : &lv_font_montserrat_24,
                       lv_color_hex(0xE5E7EB));
    lv_obj_set_pos(title, margin, 3);
    lv_obj_set_width(title, w - 92);

    s_lbl_mode = lv_label_create(scr);
    lv_label_set_text(s_lbl_mode, "RUN");
    lv_obj_set_size(s_lbl_mode, 62, 22);
    lv_obj_align(s_lbl_mode, LV_ALIGN_TOP_RIGHT, -margin, 5);
    lv_obj_set_style_radius(s_lbl_mode, 5, 0);
    lv_obj_set_style_bg_color(s_lbl_mode, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_bg_opa(s_lbl_mode, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_mode, lv_color_hex(0x06120A), 0);
    lv_obj_set_style_text_align(s_lbl_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_lbl_mode, 3, 0);

    s_lbl_source = make_label(scr, "EXT PA5", &lv_font_montserrat_14, lv_color_hex(0x38BDF8));
    lv_obj_set_pos(s_lbl_source, margin, (h <= 260) ? 22 : 30);
    lv_obj_set_width(s_lbl_source, 92);

    s_lbl_status = make_label(scr, "Lock T1650 | Frm 0", &lv_font_montserrat_14, lv_color_hex(0xA7F3D0));
    lv_obj_set_pos(s_lbl_status, margin + 96, (h <= 260) ? 22 : 30);
    lv_obj_set_width(s_lbl_status, w - 170);

    chart_panel = make_panel(scr, chart_x, chart_y, chart_w, chart_h);
    s_chart = lv_chart_create(chart_panel);
    lv_obj_set_size(s_chart, chart_w - 12, chart_h - 12);
    lv_obj_center(s_chart);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, APP_SCOPE_WAVE_POINTS);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3300);
    lv_chart_set_div_line_count(s_chart, 4, 7);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x243244), LV_PART_MAIN);
    lv_obj_set_style_size(s_chart, 0, LV_PART_INDICATOR);
    s_series = lv_chart_add_series(s_chart, lv_color_hex(0x2DD4BF), LV_CHART_AXIS_PRIMARY_Y);

    stats_panel = make_panel(scr, stats_x, chart_y, right_w, chart_h);
    lv_obj_set_style_bg_color(stats_panel, lv_color_hex(0x101827), 0);

    card_w = (right_w - 14 - card_gap) / 2;
    card_h = (chart_h - 14 - (2 * card_gap)) / 3;
    if (card_h < 34) {
        card_h = 34;
    }

    (void)make_metric_card(stats_panel, 2, 2, card_w, card_h, "Fs", lv_color_hex(0xE5E7EB), &s_lbl_rate);
    (void)make_metric_card(stats_panel, 2 + card_w + card_gap, 2, card_w, card_h, "Sig", lv_color_hex(0xE5E7EB), &s_lbl_freq);
    (void)make_metric_card(stats_panel, 2, 2 + card_h + card_gap, card_w, card_h, "Vpp", lv_color_hex(0xFDBA74), &s_lbl_vpp);
    (void)make_metric_card(stats_panel, 2 + card_w + card_gap, 2 + card_h + card_gap, card_w, card_h, "Avg", lv_color_hex(0xE5E7EB), &s_lbl_avg);
    (void)make_metric_card(stats_panel, 2, 2 + (card_h + card_gap) * 2, card_w, card_h, "Range", lv_color_hex(0xCBD5E1), &s_lbl_minmax);
    (void)make_metric_card(stats_panel, 2 + card_w + card_gap, 2 + (card_h + card_gap) * 2, card_w, card_h, "Duty", lv_color_hex(0xA7F3D0), &s_lbl_duty);

    logic_panel = make_panel(scr, margin, logic_y, w - (2 * margin), logic_h);
    lv_obj_set_style_bg_color(logic_panel, lv_color_hex(0x101827), 0);

    for (uint32_t i = 0; i < APP_SCOPE_LOGIC_CHANNELS; i++) {
        lv_coord_t slot_w = (w - (2 * margin) - 12) / APP_SCOPE_LOGIC_CHANNELS;
        lv_coord_t x = (lv_coord_t)(i * slot_w);

        s_logic_dot[i] = lv_obj_create(logic_panel);
        lv_obj_set_pos(s_logic_dot[i], x, 7);
        lv_obj_set_size(s_logic_dot[i], 9, 9);
        lv_obj_clear_flag(s_logic_dot[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(s_logic_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_logic_dot[i], 0, 0);
        lv_obj_set_style_bg_color(s_logic_dot[i], lv_color_hex(0x334155), 0);
        lv_obj_set_style_bg_opa(s_logic_dot[i], LV_OPA_COVER, 0);

        s_logic_label[i] = make_label(logic_panel, "D0 L E0", &lv_font_montserrat_14, lv_color_hex(0xCBD5E1));
        lv_obj_set_pos(s_logic_label[i], x + 13, 2);
        lv_obj_set_width(s_logic_label[i], slot_w - 16);
    }

    footer_panel = make_panel(scr, margin, footer_y, w - (2 * margin), footer_h);
    lv_obj_set_style_bg_color(footer_panel, lv_color_hex(0x0F172A), 0);
    s_lbl_ctrl_run = make_control_box(footer_panel, 2, ctrl_w, "K0 STOP", lv_color_hex(0x22C55E));
    s_lbl_ctrl_source = make_control_box(footer_panel,
                                         2 + ctrl_w + ctrl_gap,
                                         ctrl_w,
                                         "K1 AUTO",
                                         lv_color_hex(0x38BDF8));
    s_lbl_ctrl_trig_down = make_control_box(footer_panel,
                                            2 + (ctrl_w + ctrl_gap) * 2,
                                            ctrl_w,
                                            "K2 TR-",
                                            lv_color_hex(0xF59E0B));
    s_lbl_ctrl_trig_up = make_control_box(footer_panel,
                                          2 + (ctrl_w + ctrl_gap) * 3,
                                          ctrl_w,
                                          "WK TR+",
                                          lv_color_hex(0xA7F3D0));

    s_lbl_action = make_label(footer_panel, "EXT PA5: F103 PA1, common GND", &lv_font_montserrat_14, lv_color_hex(0x94A3B8));
    lv_obj_set_pos(s_lbl_action, action_x, 5);
    lv_obj_set_width(s_lbl_action, w - (2 * margin) - action_x - 6);
}

void AppScopeUi_Update(const AppScopeSnapshot_t *snapshot)
{
    char text[64];

    if ((snapshot == 0) || (s_chart == 0) || (s_series == 0)) {
        return;
    }

    lv_label_set_text(s_lbl_mode, mode_text(snapshot->run_mode));
    lv_obj_set_style_bg_color(s_lbl_mode, mode_color(snapshot->run_mode), 0);
    lv_label_set_text(s_lbl_source, source_text(snapshot->source));
    if (snapshot->source == APP_SCOPE_SOURCE_ADC) {
        lv_color_t status_color = lv_color_hex(0xA7F3D0);
        const char *signal_text = snapshot->signal_present ? "SIG" : "NO SIG";
        const char *lock_text = snapshot->trigger_locked ? "LOCK" : "WAIT";

        if ((snapshot->signal_present == 0U) && (snapshot->avg_mv >= 3000U)) {
            signal_text = "DC HIGH";
            status_color = lv_color_hex(0xFDBA74);
        } else if ((snapshot->signal_present == 0U) && (snapshot->avg_mv <= 300U)) {
            signal_text = "DC LOW";
            status_color = lv_color_hex(0x7DD3FC);
        } else if (snapshot->signal_clipped != 0U) {
            signal_text = "RAIL";
            status_color = lv_color_hex(0xFDBA74);
        } else if (snapshot->signal_present == 0U) {
            status_color = lv_color_hex(0x94A3B8);
        } else if (snapshot->trigger_locked == 0U) {
            status_color = lv_color_hex(0xFACC15);
        }

        lv_obj_set_style_text_color(s_lbl_status, status_color, 0);
        lv_label_set_text_fmt(s_lbl_status,
                              "%s %s T%u | F%lu",
                              signal_text,
                              lock_text,
                              snapshot->trigger_mv,
                              (unsigned long)snapshot->frame_count);
    } else {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xA7F3D0), 0);
        lv_label_set_text_fmt(s_lbl_status,
                              "%s T%u | F%lu",
                              snapshot->trigger_locked ? "LOCK" : "WAIT",
                              snapshot->trigger_mv,
                              (unsigned long)snapshot->frame_count);
    }

    format_hz_value(text, sizeof(text), snapshot->sample_rate_hz);
    lv_label_set_text(s_lbl_rate, text);
    format_signal_hz_value(text, sizeof(text), snapshot);
    lv_label_set_text(s_lbl_freq, text);
    format_mv_value(text, sizeof(text), snapshot->vpp_mv);
    lv_label_set_text(s_lbl_vpp, text);
    format_mv_value(text, sizeof(text), snapshot->avg_mv);
    lv_label_set_text(s_lbl_avg, text);
    format_minmax_value(text, sizeof(text), snapshot->min_mv, snapshot->max_mv);
    lv_label_set_text(s_lbl_minmax, text);
    lv_label_set_text_fmt(s_lbl_duty, "%u%%", (uint16_t)((snapshot->duty_permille + 5U) / 10U));

    for (uint32_t i = 0; i < APP_SCOPE_WAVE_POINTS; i++) {
        lv_chart_set_value_by_id(s_chart, s_series, i, (lv_coord_t)snapshot->waveform_mv[i]);
    }
    lv_chart_refresh(s_chart);

    for (uint32_t ch = 0; ch < APP_SCOPE_LOGIC_CHANNELS; ch++) {
        lv_obj_set_style_bg_color(s_logic_dot[ch],
                                  snapshot->logic_level[ch] ? lv_color_hex(0x22C55E) : lv_color_hex(0x334155),
                                  0);
        if (snapshot->source == APP_SCOPE_SOURCE_ADC) {
            static const char *adc_logic_name[APP_SCOPE_LOGIC_CHANNELS] = {"IN", "TR", "SG", "RL"};

            lv_label_set_text_fmt(s_logic_label[ch],
                                  "%s %s E%lu",
                                  adc_logic_name[ch],
                                  snapshot->logic_level[ch] ? "H" : "L",
                                  (unsigned long)snapshot->logic_edges[ch]);
        } else {
            lv_label_set_text_fmt(s_logic_label[ch],
                                  "D%lu %s E%lu",
                                  (unsigned long)ch,
                                  snapshot->logic_level[ch] ? "H" : "L",
                                  (unsigned long)snapshot->logic_edges[ch]);
        }
    }

    if (s_lbl_ctrl_run != 0) {
        lv_label_set_text(s_lbl_ctrl_run, snapshot->run_mode == APP_SCOPE_MODE_RUN ? "K0 STOP" : "K0 RUN");
    }
    if (s_lbl_ctrl_source != 0) {
        lv_label_set_text(s_lbl_ctrl_source, "K1 AUTO");
    }
    if (s_lbl_ctrl_trig_down != 0) {
        lv_label_set_text(s_lbl_ctrl_trig_down, "K2 TR-");
    }
    if (s_lbl_ctrl_trig_up != 0) {
        lv_label_set_text(s_lbl_ctrl_trig_up, "WK TR+");
    }

    if (apply_pending_action() == 0U) {
        format_diag_action(text, sizeof(text), snapshot);
        lv_label_set_text(s_lbl_action, text);
    }
}

void AppScopeUi_ShowAction(const char *text)
{
    if (text != 0) {
        taskENTER_CRITICAL();
        (void)strncpy(s_pending_action, text, sizeof(s_pending_action) - 1U);
        s_pending_action[sizeof(s_pending_action) - 1U] = '\0';
        s_action_dirty = 1U;
        taskEXIT_CRITICAL();
    }
}
