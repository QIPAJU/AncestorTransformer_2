/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <string.h>
#include <oled.h>
#include <stdio.h>
#include <sys/_intsup.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MOVE_THRES   20U        /* 活动判定：脱离参考点 ±20 码才算"有人在动" */
#define HYS          10U        /* 换档迟滞(码)，防止停在阈值上抖动 */
#define IDLE_MS      60000UL    /* 无动作关机时间 (60 s) */
#define SAMPLE_MS    50U        /* 采样周期 (50 ms = 20 Hz) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//阈值表
static const uint16_t kThresh[6] = { 4070, 3730, 2032, 395, 25, 0 };
//图片表
static const Image *const kImg[6] = {
    &stage6Img, &stage5Img, &stage4Img, &stage3Img, &stage2Img, &stage1Img };
//文字表
    static const char *const kTxt[6] = {
    "梁祖", "梁神", "梁圣", "梁子", "牢梁", "小难梁" };

/* 无迟滞判档：返回 0..5 */
static uint8_t StageFromValue(uint16_t v)
{
    for (uint8_t s = 0; s < 5; s++) {
        if (v >= kThresh[s]) return s;   /* 0..4 */
    }
    return 5;                            /* 最低档，兜底 */
}

/* 带迟滞判档：cur = 0xFF 表示还没有档位（首帧，直接返回）
   档位 s 的区间是 [kThresh[s], kThresh[s-1])：
     往高档走 -> 要越过上边界 kThresh[cur-1] + HYS
     往低档走 -> 要越过下边界 kThresh[cur]   - HYS            */
static uint8_t StageFromValueHyst(uint16_t v, uint8_t cur) //v:当前值 cur:上一次的档位
{
    uint8_t s = StageFromValue(v);
    if (cur == 0xFFU || s == cur) return s;                 /* 首帧 / 没跨档 */
    if (s < cur)                                            /* 往高档 */
        return (v >= (uint16_t)(kThresh[cur - 1U] + HYS)) ? s : cur;
    return (v <= (uint16_t)(kThresh[cur] - HYS)) ? s : cur; /* 往低档 */
}

//单次ADC采样
static uint16_t AdcReadOnce(void)          /* 单次转换：启动 -> 等完成 -> 取值 -> 关 ADC */
{
    uint16_t v = 0xFFFFU;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK) {
        v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);                  /* ADEN=0，两次采样之间 ADC 完全断电 */
    return v;
}

/* 睡 ms 毫秒：Sleep 模式，CPU 停但 SysTick 还在走 */
static void SleepMs(uint32_t ms)
{
    uint32_t t = HAL_GetTick() + ms;
    while ((int32_t)(t - HAL_GetTick()) > 0) {
        __WFI();
    }
}

/* 关机：不复返，只能按复位键回来 */
static void PowerOff(void)
{
    OLED_DisPlay_Off();                  /* ★ 必须先关 OLED，否则它继续吃 10~25 mA */
    HAL_ADC_Stop(&hadc1);

    HAL_SuspendTick();                   /* 停掉 1 ms 中断 */
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;  /* 清掉可能已挂起的 SysTick */
    __disable_irq();

    while (1) {
        HAL_PWREx_EnterSHUTDOWNMode();   /* 正常不复返；万一返回就再进一次 */
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint16_t value   = 0;       /* 当前 ADC 值 */
  uint8_t  stage   = 0xFFU;   /* 当前显示档位，0xFF = 未绘制 */
  uint16_t ref     = 0;       /* 活动判定参考点 */
  uint32_t lastAct = 0;       /* 最后一次"有人在动"的时刻 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
    /* ---- LED 上电指示 0.5 s（低电平点亮）---- */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);   /* 先确保灭 */
  HAL_Delay(20);
  OLED_Init();
  OLED_SetContrast(0x4F); /* 降低对比度省电 */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); /* 亮 */
  HAL_Delay(500);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);   /* 灭 */

  /* ---- ADC 校准 ---- */
  HAL_ADC_Stop(&hadc1);
  HAL_ADCEx_Calibration_Start(&hadc1);

  /* ---- 状态初值 ---- */
  value = AdcReadOnce();
  if (value == 0xFFFFU) value = 0;   /* 采样失败也不影响启动 */
  ref     = value;                   /* 参考点 = 开机位置 */
  stage   = 0xFFU;                   /* 强制首帧绘制 */
  lastAct = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint16_t v, d;
    uint8_t  now;

    /* ---------- ① 采样 ---------- */
    v = AdcReadOnce();
    if (v == 0xFFFFU) {           /* 采样失败：跳过这一拍，不污染状态 */
        SleepMs(SAMPLE_MS);
        continue;
    }
    value = v;

    /* ---------- ② 有人在动吗？（喂关机计时器）---------- */
    d = (value > ref) ? (uint16_t)(value - ref) : (uint16_t)(ref - value);
    if (d > MOVE_THRES) {
        ref     = value;                 /* ★ 参考点只在这里更新 ★ */
        lastAct = HAL_GetTick();         /* 刷新"无动作"计时 */
    }

    /* ---------- ③ 显示：每拍都判档，只在跨档时重绘 ---------- */
    now = StageFromValueHyst(value, stage);
    if (now != stage) {
        stage = now;
        OLED_NewFrame();
        OLED_DrawImage(0, 0, kImg[stage], OLED_COLOR_NORMAL);
        OLED_PrintString(0, 0, kTxt[stage], &font12x12, OLED_COLOR_NORMAL);
        OLED_ShowFrame();                /* 约 23 ms，只在跨档时发生 */
    }

    /* ---------- ④ 1 分钟无动作 -> Shutdown（复位键唤醒）---------- */
    if ((uint32_t)(HAL_GetTick() - lastAct) > IDLE_MS) {
        PowerOff();                      /* 不复返 */
    }

    /* ---------- ⑤ 节拍：睡 50 ms ---------- */
    SleepMs(SAMPLE_MS);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_0);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
