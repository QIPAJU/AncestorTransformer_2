================================================================
 Type_mini  v2.0  —— 滑动变祖器（电位器分档显示小项目）
 工程：AncestorTransformer_2          MCU：STM32C011F4P6
================================================================

一、项目简介
----------------------------------------------------------------
本项目基于 STM32C011F4P6，通过 ADC 采集电位器的分压电压，把
0 ~ 4095 的采样值划分成 6 个档位，在 SSD1306 OLED 屏上显示
对应的中文称号 + 全屏图案（stage1Img ~ stage6Img）。

v2.0 改成“事件驱动”刷新：只有档位真正变化时才重绘屏幕，其余时间
以 20Hz 单次采样 + Sleep 待机；超过 60 秒无人操作自动进入 Shutdown
关机（只能按复位键重新开机），面向纽扣电池供电场景。

版本：Type_mini v2.0   (2026-10-01)

二、硬件平台
----------------------------------------------------------------
MCU        : STM32C011F4P6 (Cortex-M0+, TSSOP20, Flash 16KB / RAM 6KB)
主频       : HSI 48MHz 再 4 分频 -> SYSCLK 12MHz (HSIDiv = DIV4)
显示       : SSD1306 OLED, 128x64, I2C 接口, 器件地址 0x78
输入       : 电位器中心抽头 -> PA12 / ADC1_IN12（单次转换, 20Hz）
供电       : CR2450 纽扣电池 3V（调试时也可用 ST-Link 的 3.3V）
开发方式   : STM32CubeMX 生成 + CMake + Ninja + arm-none-eabi-gcc
调试/烧录  : ST-Link / SWD (CubeProgrammer CLI, 500kHz)
固件体积   : FLASH 13292B / 16KB (81.1%)，RAM 2920B / 6KB (47.5%)
编译选项   : Debug 预设 -Os -g3 + LTO（不开 LTO 会撑爆 16KB Flash）

三、引脚分配
----------------------------------------------------------------
PA4   GPIO 输出   LED（低电平点亮，仅上电亮 0.5 秒作指示）
PA12  ADC1_IN12   电位器中心抽头（模拟输入，无上下拉）
PB6   I2C1_SCL    OLED 时钟线（复用功能 AF6，开漏）
PB7   I2C1_SDA    OLED 数据线（复用功能 AF6，开漏）
PA13  SWDIO       SWD 调试数据（v1.0 曾用此脚做 ADC 输入，见第十节）
PA14  SWCLK       SWD 调试时钟

四、档位说明（12 位 ADC，VREF+ = VDD ≈ 3.05V 实测）
----------------------------------------------------------------
  索引  ADC 采样值    估算电压     显示文字   图案
  0     >= 4070       >= 3.03 V    梁祖       stage6Img
  1     >= 3730       >= 2.78 V    梁神       stage5Img
  2     >= 2032       >= 1.51 V    梁圣       stage4Img
  3     >= 395        >= 0.29 V    梁子       stage3Img
  4     >= 25         >= 0.02 V    牢梁       stage2Img
  5     <  25         <  0.02 V    小难梁     stage1Img

  说明:
  - 阈值/图案/文字三张表在 Core/Src/main.c 中分别是 kThresh[] / kImg[]
    / kTxt[]，下标一一对应；增删档位时要同时改三个数组的长度，以及
    StageFromValue() 的循环上界与兜底返回值（见 main.c 的 USER CODE 0）。
  - 电位器两端各有约 20~25% 机械盲区，阈值按实测有效行程标定。
  - 电压列只是按 3.05V 参考估算；测量是比例式的（ADC 参考就是 VDD），
    所以电池电压跌落不会影响档位判断。

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
1. ADC 用单次转换（CubeMX: Continuous Conversion Mode = Disabled），
   20Hz 下 ADC 占空比约 0.03%，平均电流可忽略。
2. 帧间用 __WFI() 进 Sleep 模式，CPU 不再 Run 空转（Sleep 下 SysTick 仍在走）。
3. 事件驱动刷新：只有跨档才把 1KB 显存写到 OLED，平时不发 I2C。
4. OLED 对比度降到 0x4F（驱动默认 0xDF 几乎拉满），面板点亮电流大致按比例下降。
5. LED 只在开机亮 0.5 秒。
6. 60 秒无操作 -> 先 OLED_DisPlay_Off() 关电荷泵，再进 Shutdown
   （SRAM 丢失、只能按复位键唤醒，关机电流 µA 级）。
7. 关键: OLED 是独立供电的，进 Shutdown 前必须先关屏，
   否则屏幕继续耗 10~25mA，关机等于没关。

纽扣电池续航参考（CR2450 标称 620mAh，但那是按 0.2mA 放电标定的）:
  - 亮屏时整机约 15~25mA（OLED 是大头），实际只够几个小时；
  - 关机后 µA 级；若电位器是跨在 VDD-GND 上的分压器，还要额外
    加上它的分流电流（例如 10kΩ 约 300µA，可考虑用 GPIO 给电位器供电）。

七、目录结构
----------------------------------------------------------------
Core/Src/main.c              主程序：档位表、单次采样、判档、低功耗与 Shutdown
Core/Src/oled.c              波特律动 OLED 驱动 (SSD1306, MIT) + OLED_SetContrast()
Core/Src/font.c              字库 font12x12 等 + stage1~6Img 六张 128x64 图（约 6KB）
Core/Src/adc.c               ADC1 初始化（单次转换、采样时间 160.5 周期、PCLK 12MHz）
Core/Src/i2c.c               I2C1 初始化（7 位地址、模拟+数字滤波、Timing 0x00402D41）
Core/Src/gpio.c              GPIO 初始化（PA4 LED，低电平点亮）
Core/Src/stm32c0xx_it.c      中断服务函数
Core/Inc/*.h                 对应头文件
Drivers/                     CMSIS + STM32C0xx HAL 驱动
cmake/                       工具链与 CubeMX 源文件列表
CMakeLists.txt               顶层构建脚本（用户源文件在此添加）
CMakePresets.json            Debug / Release 预设
STM32C011xx_FLASH.ld         链接脚本
AncestorTransformer_2.ioc    CubeMX 工程文件
README.md                    本文件

八、编译与烧录
----------------------------------------------------------------
依赖:
  - arm-none-eabi-gcc (需加入 PATH)
  - CMake >= 3.22、Ninja
  - STM32CubeProgrammer (含 STM32_Programmer_CLI.exe)

命令行:
  cmake --preset Debug
  cmake --build --preset Debug

VS Code 中:
  直接运行任务 "Flash STM32 (CubeProg)" 即可
  （自动先执行 CMake: build，再用 SWD 500kHz 下载并复位运行）

链接结果参考:
  Memory region         Used Size  Region Size  %age Used
               RAM:        2920 B         6 KB     47.53%
             FLASH:       13292 B        16 KB     81.13%
  （16KB Flash 很紧张：改代码后请留意 --print-memory-usage 输出，
    Debug 预设靠 -Os 和 -flto 才装得下，别轻易关掉优化。）

注意:
  用 CubeMX 重新生成代码时，main.c 中新增的用户代码请写在
  /* USER CODE BEGIN */ 与 /* USER CODE END */ 之间，否则会被覆盖。
  ADC 的连续/单次转换、采样时间等参数请在 CubeMX 里改（会自动更新
  adc.c），手改 adc.c 会在下次生成时被覆盖。
  新增 .c 源文件需手动加到 CMakeLists.txt 的 target_sources() 中
  （font.c 和 oled.c 已经加好）。

九、编码与显示注意事项
----------------------------------------------------------------
- 源文件保存为 UTF-8，编译器字符集也需为 UTF-8，否则中文显示异常。
- OLED 初始化前务必留出约 20ms 延时，等待屏幕上电稳定。
- 绘制顺序：OLED_DrawImage()/OLED_PrintString() 走的是 OLED_SetBlock，
  属于覆盖式写入（先清后写，没有透明模式）。所以必须“先画 128x64 的图，
  再在图上面写字”，顺序反了字会被图擦掉。
- 判档不要用“相邻两次采样差值累加”的写法，ADC 的 1~2 码噪声会把它
  迅速撑满，导致“无操作计时”永远刷新、自动关机失效。本工程用的是
  “固定参考点 + 迟滞”方案（见第六节与 main.c 的 USER CODE 0）。
- 改档位阈值后，记得同步检查 kImg / kTxt 与 StageFromValue()。
- 字库与图片可用“波特律动取模助手”(https://led.baud-dance.com) 生成，
  图片数据为列优先(column-major)：index = x + page * w，bit0 为该页顶行。

十、已知问题与注意事项
----------------------------------------------------------------
1. PA13/PA14 是 SWD 引脚。v1.0 曾用 PA13(ADC1_IN13) 采电位器，接上 ST-Link 后
   会被探针的 SWDIO 上拉偏置：万用表量到 0V，ADC 却读到约 280 码，顶端还会
   顶死在 4095。现已改用 PA12 / ADC1_IN12。
2. 把 SWD 引脚配置成模拟输入后 SWD 就失效了，只能以复位方式（-rst）重新烧录。
3. OLED 用细杜邦线连接时，点亮瞬间会造成约 70mV 的轨压跌落；
   换短而粗的线 + 星形接地可以缓解。
4. 进入 Shutdown 后 ST-Link 会掉线，属正常现象。
5. CR2450 的最大推荐连续电流只有 1~3mA，不适合长时间亮屏供电；
   要长时间续航建议换 2×AA / 小锂电，或大幅降低亮屏占空比。

================================================================
 Type_mini v2.0  ——  功能：6 档位称号/图案显示 + 20Hz 采样
                     + 事件驱动刷新 + 60 秒无操作自动关机
================================================================
