# STM32 定时器触发ADC + DMA 完整配置流程（保姆级步骤）
我直接给你**最标准、工程上最常用**的配置流程：
**TIM 定时器 → 硬件触发 ADC → DMA 自动搬运数据到内存数组**
全程**CPU不参与、不阻塞、采样精准、连续采集不掉帧**。

我用 **STM32F103（通用所有STM32逻辑一样）** 举例，**库函数/HAL库都讲清楚**，你直接照着配就行。

---

## 一、整体工作流程（先看懂逻辑）
1. **定时器** 每隔固定时间（如1ms）输出一个触发信号
2. **ADC** 收到触发信号 → 自动采样转换
3. **DMA** 自动把ADC结果搬到数组里
4. 数组满了 → 通知CPU处理（中断/标志位）
5. CPU只需要读数组做滤波、算电压，**不用管采集过程**

---

## 二、必须配置的 4 大模块（按顺序来）
1. **GPIO 模拟输入**
2. **ADC 配置（硬件触发模式）**
3. **定时器 配置（输出触发信号）**
4. **DMA 配置（搬运ADC数据）**

---

## 三、详细配置步骤（一步一步跟着做）
### 1. 开启时钟（GPIO、ADC1、TIM1、DMA1）
所有外设先开时钟，这是第一步！
```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1 | RCC_APB2Periph_TIM1, ENABLE);
RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
```

---

### 2. 配置 GPIO 为模拟输入
ADC 引脚必须设 **ANALOG 模拟模式**
```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;    // ADC_IN0
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;// 模拟输入
GPIO_Init(GPIOA, &GPIO_InitStructure);
```

---

### 3. 配置 ADC（关键：设为**外部触发模式**）
这里是**核心**：必须让ADC等待定时器触发！
```c
ADC_InitTypeDef ADC_InitStructure;

// ADC 时钟分频（最大14MHz，必须配）
RCC_ADCCLKConfig(RCC_PCLK2_Div6);

ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
ADC_InitStructure.ADC_ScanConvMode = DISABLE;       // 单通道
ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; // 关闭连续转换（等待硬件触发）
ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1; // 定时器1通道1触发
ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
ADC_InitStructure.ADC_NbrOfChannel = 1;
ADC_Init(ADC1, &ADC_InitStructure);

// 配置通道采样时间
ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);

// 开启 ADC DMA
ADC_DMACmd(ADC1, ENABLE); // 必须开！

// 使能 ADC
ADC_Cmd(ADC1, ENABLE);
```

---

### 4. 配置定时器（用来定时触发 ADC）
目标：让定时器**每隔 X ms 产生一个触发信号**
```c
TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
TIM_OCInitTypeDef TIM_OCInitStructure;

// 定时器基本配置：72MHz / 72 / 1000 = 1ms 中断一次
TIM_TimeBaseStructure.TIM_Period = 1000-1;
TIM_TimeBaseStructure.TIM_Prescaler = 72-1;
TIM_TimeBaseStructure.TIM_ClockDivision = 0;
TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

// 配置通道1为 PWM 模式，用于触发 ADC
TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
TIM_OCInitStructure.TIM_Pulse = 500;
TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
TIM_OC1Init(TIM1, &TIM_OCInitStructure);

// 使能定时器
TIM_Cmd(TIM1, ENABLE);
```
✅ **现在定时器每 1ms 自动触发一次 ADC 转换**

---

### 5. 配置 DMA（自动搬运 ADC 数据到数组）
```c
DMA_InitTypeDef DMA_InitStructure;

// 定义一个数组存采样数据（比如存10个）
uint16_t ADC_ConvertedValue[10];

DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR; // ADC数据寄存器地址
DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADC_ConvertedValue; // 数组地址
DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC; // 外设作为源
DMA_InitStructure.DMA_BufferSize = 10; // 数组大小
DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; // 内存地址自增
DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; // 循环模式（一直采集）
DMA_InitStructure.DMA_Priority = DMA_Priority_High;
DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
DMA_Init(DMA1_Channel1, &DMA_InitStructure);

// 使能 DMA
DMA_Cmd(DMA1_Channel1, ENABLE);
```

---

## 四、最终效果（你完全不用管采集）
- 定时器 **每1ms** 触发一次ADC
- ADC自动转换
- DMA自动把值放进数组 `ADC_ConvertedValue[]`
- 数组循环覆盖，**连续采集永不停止**
- **CPU 完全解放**

你只需要：
```c
// 读数组里的最新值
uint16_t val = ADC_ConvertedValue[0];

// 转成电压
float voltage = val * 3.3f / 4096;
```

---

## 五、最关键的 3 个配置要点（新手必看）
1. **ADC 必须关闭连续转换**，选择**外部硬件触发**
2. **定时器必须配置为 PWM 触发模式**
3. **DMA 必须设为循环模式 Circular Mode**

---

## 六、我可以直接给你
你告诉我：
1. 你用的是 **标准库** 还是 **HAL库**？
2. 你要 **单通道** 还是 **多通道**？

我直接给你**完整可编译的工程代码**，复制就能跑。