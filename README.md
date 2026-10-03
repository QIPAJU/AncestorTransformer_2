================================================================
 Type_mini  v2.0  —— 滑动变祖器（电位器分档显示小项目）
 工程：AncestorTransformer_2          MCU：STM32C011F4P6
================================================================

一、项目简介
----------------------------------------------------------------
本项目基于 STM32C011F4P6，通过 ADC 采集电位器的分压电压，把
0 ~ 4095 的采样值划分成 6 个档位，在 SSD1306 OLED 屏上显示
对应文字 + 全屏图案（stage1Img ~ stage6Img）。

版本：Type_mini v2.0   (2026-10-01)

二、硬件平台
----------------------------------------------------------------
MCU        : STM32C011F4P6 (Cortex-M0+, TSSOP20, Flash 16KB / RAM 6KB)
显示       : SSD1306 OLED, 128x64, I2C 接口, 器件地址 0x78
输入       : 电位器中心抽头 -> PA12 / ADC1_IN12（单次转换, 20Hz）
供电       : CR2450 纽扣电池 3V / TypeC有线供电
固件体积   : FLASH 13292B / 16KB (81.1%)，RAM 2920B / 6KB (47.5%)
编译选项   : Debug 预设 -Os -g3 + LTO（不开 LTO 会撑爆 16KB Flash）

三、引脚分配
----------------------------------------------------------------
PA4   GPIO 输出   LED（低电平点亮，仅上电亮 0.5 秒作指示）
PA12  ADC1_IN12   电位器中心抽头
PB6   I2C1_SCL    OLED 时钟线
PB7   I2C1_SDA    OLED 数据线
PA13  SWDIO       SWD 调试数据
PA14  SWCLK       SWD 调试时钟

四、档位说明
----------------------------------------------------------------
  索引  ADC 采样值    大致电压       图案
  0     >= 4070       >= 3.03 V     stage6Img
  1     >= 3730       >= 2.78 V     stage5Img
  2     >= 2032       >= 1.51 V     stage4Img
  3     >= 395        >= 0.29 V     stage3Img
  4     >= 25         >= 0.02 V     stage2Img
  5     <  25         <  0.02 V     stage1Img

五、程序流程
----------------------------------------------------------------
1. HAL_Init() + SystemClock_Config()  （SYSCLK 12MHz）
2. MX_GPIO_Init() / MX_ADC1_Init() / MX_I2C1_Init()
3. LED 亮 0.5 秒作上电指示
4. 延时 20ms（OLED 上电比 MCU 慢）后 OLED_Init()，再 OLED_SetContrast(0x4F)
5. HAL_ADCEx_Calibration_Start() 校准 ADC1（不需要再启动转换）
6. 主循环（每拍 50ms = 20Hz）:
   - AdcReadOnce(): Start -> 等转换完成 -> 取值 -> Stop（帧间 ADC 完全断电）
   - 活动检测: |value - ref| > 20 码 则更新 ref 并刷新 lastAct
   - 判档: StageFromValueHyst(value, stage)（带 ±10 码迟滞，防边界抖动）
   - 只有跨档时才 OLED_NewFrame() -> OLED_DrawImage() -> OLED_PrintString()
     -> OLED_ShowFrame()（一次约 23ms）
   - lastAct 超过 60 秒 -> PowerOff(): 关 OLED 电荷泵 -> 停 SysTick -> Shutdown
   - SleepMs(50): __WFI() 进 Sleep 等待下一拍

六、低功耗设计要点
----------------------------------------------------------------
1. ADC 用单次转换，20Hz 下 ADC 占空比约 0.03%，平均电流可忽略。
2. 帧间用 __WFI() 进 Sleep 模式，CPU 不再 Run 空转（Sleep 下 SysTick 仍在走）。
3. 事件驱动刷新：只有跨档才把 1KB 显存写到 OLED，平时不发 I2C。
4. OLED 对比度降到 0x4F（驱动默认 0xDF 几乎拉满），面板点亮电流大致按比例下降。
5. LED 只在开机亮 0.5 秒。
6. 60 秒无操作 -> 先 OLED_DisPlay_Off() 关电荷泵，再进 Shutdown。

================================================================
 Type_mini v2.0  ——  功能：6 档位称号/图案显示 + 20Hz 采样
                     + 事件驱动刷新 + 60 秒无操作自动关机
================================================================
