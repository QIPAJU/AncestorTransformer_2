================================================================
 AncestorTransformer_2 —— 电位器电压分档显示小项目
================================================================

一、项目简介
----------------------------------------------------------------
本项目基于 STM32C011F4P6，通过 ADC 采集电位器（或可调电压源）
的分压电压，把 0 ~ 4095 的采样值划分成 7 个档位，并在 SSD1306
OLED 屏上显示对应的中文称号。

1 秒内多次循环刷新，转动电位器即可看到称号实时变化。

二、硬件平台
----------------------------------------------------------------
MCU        : STM32C011F4P6 (Cortex-M0+, TSSOP20)
主频       : HSI 48MHz 4 分频 -> 12MHz (HSIDiv = DIV4)
显示       : SSD1306 OLED, 128x64, I2C 接口, 器件地址 0x78
输入       : 电位器中心抽头 -> ADC1_IN13
开发方式   : STM32CubeMX 生成 + CMake + Ninja + arm-none-eabi-gcc
调试/烧录  : ST-Link / SWD (CubeProgrammer CLI)

三、引脚分配
----------------------------------------------------------------
PA4   GPIO 输出   板载 LED（低电平点亮，上电亮起作指示）
PA13  ADC1_IN13   电位器输入（模拟输入，无上下拉）
PB6   I2C1_SCL    OLED 时钟线（复用功能 AF6）
PB7   I2C1_SDA    OLED 数据线（复用功能 AF6）

四、档位说明（12 位 ADC，参考电压按 3.3V 估算）
----------------------------------------------------------------
  ADC 采样值        估算电压        显示文字
  >= 4070           >= 3.28 V       梁祖
  >= 3460           >= 2.79 V       梁神
  >= 2825           >= 2.28 V       梁圣
  >= 2190           >= 1.76 V       梁文峰
  >= 1555           >= 1.25 V       梁子
  >= 920            >= 0.74 V       牢梁
  >= 250            >= 0.20 V       梁嘻皮
  <  250            <  0.20 V       梁嘻皮

  (档位阈值定义在 Core/Src/main.c 的 while 循环中，可按需修改)

五、程序流程
----------------------------------------------------------------
1. HAL_Init() + SystemClock_Config()
2. MX_GPIO_Init() / MX_ADC1_Init() / MX_I2C1_Init()
3. 延时 20ms（OLED 上电比 MCU 慢）后 OLED_Init()
4. 点亮 LED 作上电指示
5. HAL_ADCEx_Calibration_Start() 校准 ADC1，启动连续转换
6. 主循环:
   - 读取 HAL_ADC_GetValue() 采样值
   - 查表得到对应文字
   - OLED_NewFrame() -> OLED_PrintString() -> OLED_ShowFrame()

六、目录结构
----------------------------------------------------------------
Core/Src/main.c              主程序、档位判断、时钟配置
Core/Src/oled.c              波特律动 OLED 驱动 (SSD1306, MIT)
Core/Src/font.c              字库（ASCII + 16x16 中文点阵）
Core/Src/adc.c               ADC1 初始化 (连续转换, 采样时间 160.5 周期)
Core/Src/i2c.c               I2C1 初始化 (7 位地址, 模拟+数字滤波)
Core/Src/gpio.c              GPIO 初始化
Core/Src/stm32c0xx_it.c      中断服务函数
Core/Inc/*.h                 对应头文件
Drivers/                     CMSIS + STM32C0xx HAL 驱动
cmake/                       工具链与 CubeMX 源文件列表
CMakeLists.txt               顶层构建脚本（用户源文件在此添加）
CMakePresets.json            Debug / Release 预设
STM32C011xx_FLASH.ld         链接脚本
AncestorTransformer_2.ioc    CubeMX 工程文件

七、编译与烧录
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
  （自动先执行 CMake: build，再用 SWD 1MHz 下载并复位运行）

注意:
  用 CubeMX 重新生成代码时，main.c 中新增的用户代码请写在
  /* USER CODE BEGIN */ 与 /* USER CODE END */ 之间，否则会被覆盖。
  新增 .c 源文件需手动加到 CMakeLists.txt 的 target_sources() 中
  （font.c 和 oled.c 已经加好）。

八、编码与显示注意事项
----------------------------------------------------------------
- 源文件保存为 UTF-8，编译器字符集也需为 UTF-8，否则中文显示异常。
- OLED 初始化前务必留出约 20ms 延时，等待屏幕上电稳定。
- ADC 使用连续转换模式，主循环直接取值；如需更稳的读数可做多次
  采样取平均或加软件滤波。

================================================================
