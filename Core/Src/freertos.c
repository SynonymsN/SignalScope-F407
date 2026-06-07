/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : FreeRTOS task creation for SignalScope-F407
  ******************************************************************************
  */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_key.h"
#include "bsp_lcd.h"
#include "bsp_led.h"
#include "bsp_scope_adc.h"
#include "lv_port_disp.h"
#include "lvgl.h"

#include "app_scope.h"
#include "app_scope_ui.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static osThreadId_t scopeAcqTaskHandle;
static osThreadId_t scopeCtrlTaskHandle;
static osThreadId_t scopeUiTaskHandle;

static const osThreadAttr_t scopeAcqTask_attr = {
    .name = "ScopeAcq",
    .stack_size = 1536,
    .priority = (osPriority_t) osPriorityAboveNormal,
};

static const osThreadAttr_t scopeCtrlTask_attr = {
    .name = "ScopeCtrl",
    .stack_size = 768,
    .priority = (osPriority_t) osPriorityNormal,
};

static const osThreadAttr_t scopeUiTask_attr = {
    .name = "ScopeUI",
    .stack_size = 4096,
    .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE END Variables */

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void Task_ScopeAcq(void *argument);
static void Task_ScopeCtrl(void *argument);
static void Task_ScopeUI(void *argument);
static void ScopeAdcNotifyFromIsr(void);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

void MX_FREERTOS_Init(void)
{
  /* USER CODE BEGIN Init */
  BSP_LED_Init();
  BSP_KEY_Init();
  BSP_LCD_Init();

  lv_init();
  lv_port_disp_init();

  AppScope_Init();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  scopeAcqTaskHandle = osThreadNew(Task_ScopeAcq, NULL, &scopeAcqTask_attr);
  scopeCtrlTaskHandle = osThreadNew(Task_ScopeCtrl, NULL, &scopeCtrlTask_attr);
  scopeUiTaskHandle = osThreadNew(Task_ScopeUI, NULL, &scopeUiTask_attr);
  (void)scopeAcqTaskHandle;
  (void)scopeCtrlTaskHandle;
  (void)scopeUiTaskHandle;
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */
}

void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  (void)argument;

  for (;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

static void Task_ScopeAcq(void *argument)
{
    (void)argument;

    BSP_SCOPE_ADC_ResetCounters();
    BSP_SCOPE_ADC_SetNotifyCallback(ScopeAdcNotifyFromIsr);

    for (;;)
    {
        uint32_t now;
        uint8_t processed = 0U;

        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        now = osKernelGetTickCount();
        while (AppScope_AcquireReady(now) != 0U) {
            processed = 1U;
        }

        if (processed == 0U) {
            AppScope_AcquireTick(now);
        }

        AppScope_RecordTaskHealth(APP_SCOPE_TASK_ACQ,
                                  now,
                                  (uint16_t)uxTaskGetStackHighWaterMark(NULL));
    }
}

static void Task_ScopeCtrl(void *argument)
{
    uint8_t key0_prev = KEY_RELEASED;
    uint8_t key1_prev = KEY_RELEASED;
    uint8_t key2_prev = KEY_RELEASED;
    uint8_t wkup_prev = KEY_RELEASED;

    (void)argument;

    for (;;)
    {
        uint8_t key0_now = BSP_KEY0_Read();
        uint8_t key1_now = BSP_KEY1_Read();
        uint8_t key2_now = BSP_KEY2_Read();
        uint8_t wkup_now = BSP_WKUP_Read();

        if ((key0_now == KEY_PRESSED) && (key1_now == KEY_PRESSED) &&
            ((key0_prev == KEY_RELEASED) || (key1_prev == KEY_RELEASED))) {
            AppScope_ClearStats();
            AppScopeUi_ShowAction("Counters cleared");
        } else if ((key2_now == KEY_PRESSED) && (wkup_now == KEY_PRESSED) &&
                   ((key2_prev == KEY_RELEASED) || (wkup_prev == KEY_RELEASED))) {
            AppScope_SingleCapture();
            AppScopeUi_ShowAction("Single frame captured");
        } else if ((key0_now == KEY_PRESSED) && (key0_prev == KEY_RELEASED)) {
            AppScope_ToggleRun();
            AppScopeUi_ShowAction("Run state toggled");
        } else if ((key1_now == KEY_PRESSED) && (key1_prev == KEY_RELEASED)) {
            AppScope_AutoTrigger();
            AppScopeUi_ShowAction("Auto trigger set from PA5");
        } else if ((key2_now == KEY_PRESSED) && (key2_prev == KEY_RELEASED)) {
            AppScope_AdjustTrigger(-100);
            AppScopeUi_ShowAction("Trigger level -100mV");
        } else if ((wkup_now == KEY_PRESSED) && (wkup_prev == KEY_RELEASED)) {
            AppScope_AdjustTrigger(100);
            AppScopeUi_ShowAction("Trigger level +100mV");
        }

        key0_prev = key0_now;
        key1_prev = key1_now;
        key2_prev = key2_now;
        wkup_prev = wkup_now;
        AppScope_RecordTaskHealth(APP_SCOPE_TASK_CTRL,
                                  osKernelGetTickCount(),
                                  (uint16_t)uxTaskGetStackHighWaterMark(NULL));
        osDelay(20);
    }
}

static void Task_ScopeUI(void *argument)
{
    uint32_t last_ui_update = 0UL;

    (void)argument;

    AppScopeUi_Create();

    for (;;)
    {
        uint32_t now = osKernelGetTickCount();

        if ((uint32_t)(now - last_ui_update) >= 40UL) {
            AppScopeSnapshot_t snapshot;

            AppScope_RecordTaskHealth(APP_SCOPE_TASK_UI,
                                      now,
                                      (uint16_t)uxTaskGetStackHighWaterMark(NULL));
            AppScope_GetSnapshot(&snapshot);
            AppScopeUi_Update(&snapshot);
            last_ui_update = now;
        }

        lv_timer_handler();
        osDelay(5);
    }
}

static void ScopeAdcNotifyFromIsr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (scopeAcqTaskHandle != NULL) {
        vTaskNotifyGiveFromISR((TaskHandle_t)scopeAcqTaskHandle,
                               &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

/* USER CODE END Application */
