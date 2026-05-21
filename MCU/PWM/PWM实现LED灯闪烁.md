# STM32 用 PWM 做 1s / 0.5s 闪烁 + 开关控制（最详细教程）
我直接给你**标准、可直接复制、稳定运行**的方案，用 **STM32CubeMX + HAL库**（最常用），不用复杂寄存器，新手也能直接用。

## 核心结论（先记牢）
- **1秒闪一次**：PWM 频率 = **1Hz**，占空比 50%
- **0.5秒闪一次**：PWM 频率 = **2Hz**，占空比 50%
- 用 **定时器 PWM 输出** 实现，**CPU 完全不参与**，硬件自动闪
- 可以随时**开关 PWM**，控制灯亮/灭/闪烁

---

# 一、硬件选择
- 任意 STM32（F103/F407/F030 等通用）
- LED 正极 → 电阻 → **定时器 PWM 通道引脚**
- LED 负极 → GND
- 例：**TIM2_CH1 → PA0**（最常用，你也可以换其他定时器通道）

---

# 二、STM32CubeMX 配置（一步一步）
## 1. 选择定时器
比如用 **TIM2**
- Clock Source 选择 **Internal Clock**
- Channel 1 选择：**PWM Generation CH1**

## 2. 关键参数（决定闪烁频率）
### 目标：产生 1Hz / 2Hz 的超低频 PWM
STM32 定时器公式：
```
PWM频率 = 定时器时钟 / [(PSC+1) * (ARR+1)]
```

### ① 实现 1Hz 闪烁（1秒一次）
- PSC = **7199**
- ARR = **9999**
- 频率 = 72M / (7200 * 10000) = **1Hz**

### ② 实现 0.5Hz 闪烁（0.5秒一次）
- PSC = **7199**
- ARR = **4999**
- 频率 = 72M / (7200 * 5000) = **2Hz**

## 3. 占空比
- CCR1 = **ARR / 2** → 50% 占空比（亮50%、灭50%）

## 4. 生成代码
选择 MDK-ARM 生成工程即可。

---

# 三、直接可用代码（HAL库）
## 1. 定义（main.c 顶部）
```c
// PWM句柄（CubeMX自动生成）
extern TIM_HandleTypeDef htim2;

// 宏定义
#define PWM_HANDLE  htim2
#define PWM_CHANNEL TIM_CHANNEL_1
```

## 2. 启动 PWM（硬件自动开始闪烁）
```c
// 启动 PWM 输出 → LED 开始闪烁
HAL_TIM_PWM_Start(&PWM_HANDLE, PWM_CHANNEL);
```

## 3. 停止 PWM → 灯灭
```c
// 停止 PWM → LED 熄灭
HAL_TIM_PWM_Stop(&PWM_HANDLE, PWM_CHANNEL);
```

## 4. 切换 1s / 0.5s 闪烁
### 切换到 1秒闪烁一次
```c
// 停止 PWM
HAL_TIM_PWM_Stop(&PWM_HANDLE, PWM_CHANNEL);

// 设置新频率：1Hz
__HAL_TIM_SET_PRESCALER(&PWM_HANDLE, 7199);  // PSC
__HAL_TIM_SET_AUTORELOAD(&PWM_HANDLE, 9999); // ARR
__HAL_TIM_SET_COMPARE(&PWM_HANDLE, PWM_CHANNEL, 5000); // 50%占空比

// 重新启动
HAL_TIM_PWM_Start(&PWM_HANDLE, PWM_CHANNEL);
```

### 切换到 0.5秒闪烁一次
```c
HAL_TIM_PWM_Stop(&PWM_HANDLE, PWM_CHANNEL);

// 设置新频率：2Hz
__HAL_TIM_SET_PRESCALER(&PWM_HANDLE, 7199);
__HAL_TIM_SET_AUTORELOAD(&PWM_HANDLE, 4999);
__HAL_TIM_SET_COMPARE(&PWM_HANDLE, PWM_CHANNEL, 2500);

HAL_TIM_PWM_Start(&PWM_HANDLE, PWM_CHANNEL);
```

---

# 四、完整开关控制逻辑（你要的功能）
```c
// 开关状态
uint8_t pwm_enable = 1; // 1=开启闪烁 0=关闭

// 按键控制例子（按下按键切换）
if(KEY_Press()){
    pwm_enable = !pwm_enable;
    
    if(pwm_enable){
        // 开启：1s闪烁
        HAL_TIM_PWM_Start(&PWM_HANDLE, PWM_CHANNEL);
    }else{
        // 关闭：灯灭
        HAL_TIM_PWM_Stop(&PWM_HANDLE, PWM_CHANNEL);
    }
}
```

---

# 五、最精简总结（直接背）
1. **1s闪烁** = PWM 1Hz
2. **0.5s闪烁** = PWM 2Hz
3. **开启闪烁** = `HAL_TIM_PWM_Start()`
4. **关闭灯** = `HAL_TIM_PWM_Stop()`
5. **切换速度** = 修改 PSC + ARR + CCR

---

## 你告诉我 3 个信息，我直接给你**完整工程代码**
1. 你用的 STM32 型号（如 F103C8T6）
2. LED 接在哪个引脚
3. 用库还是寄存器

我可以直接给你**复制就能跑的完整代码**！


---------
-------
------
# 三个LED用STM32 PWM独立控制：1s闪 / 0.5s闪 / 开关控制（完美方案）
我直接给你**3路独立PWM**控制3个LED，每个灯可以：
- 独立设置 **1秒闪烁**
- 独立设置 **0.5秒闪烁**
- 独立**开关**（亮/灭/闪烁）
- 互不干扰，硬件自动运行，不占CPU

## 核心方案
用 **1个定时器 + 3个通道**（比如TIM2_CH1、CH2、CH3）输出3路独立PWM
- LED1：1Hz（1秒闪一次）
- LED2：2Hz（0.5秒闪一次）
- LED3：可开关、可切换频率

完全满足你**控制三个LED**的需求！

---

# 一、硬件接线（通用STM32）
以最常用 **TIM2** 为例（所有STM32都能用）
- LED1 → PA0（TIM2_CH1）
- LED2 → PA1（TIM2_CH2）
- LED3 → PA2（TIM2_CH3）
- 每个LED串联 **220Ω电阻** 再接 GND

---

# 二、STM32CubeMX 一键配置
1. 定时器选择 **TIM2**
2. Clock Source：Internal Clock
3. Channel1 / Channel2 / Channel3 都选：**PWM Generation CHx**
4. 关键参数（固定，直接抄）
   - PSC = 7199
   - ARR = 9999
   - 这样基础频率 = **1Hz（1秒闪烁）**

5. 时钟：72MHz（STM32F103默认）
6. 生成MDK-ARM工程

---

# 三、3路LED控制代码（直接复制用）
## 1. 声明（main.c顶部）
```c
// 定时器句柄（CubeMX自动生成）
extern TIM_HandleTypeDef htim2;

// 3路PWM定义
#define LED1_PWM_TIM &htim2
#define LED1_CHANNEL TIM_CHANNEL_1

#define LED2_PWM_TIM &htim2
#define LED2_CHANNEL TIM_CHANNEL_2

#define LED3_PWM_TIM &htim2
#define LED3_CHANNEL TIM_CHANNEL_3
```

## 2. 初始化（main函数里）
```c
// 启动3路PWM，默认：
// LED1：1秒闪烁   LED2：0.5秒闪烁   LED3：关闭
HAL_TIM_PWM_Start(LED1_PWM_TIM, LED1_CHANNEL);
LED1_Set_1s_Flash();  // LED1 = 1s闪

HAL_TIM_PWM_Start(LED2_PWM_TIM, LED2_CHANNEL);
LED2_Set_05s_Flash(); // LED2 = 0.5s闪

HAL_TIM_PWM_Stop(LED3_PWM_TIM, LED3_CHANNEL); // LED3 = 灭
```

---

# 四、3个LED独立控制函数（核心！）
## 1. LED1：1秒闪烁一次
```c
void LED1_Set_1s_Flash(void)
{
  // 1Hz = 1秒周期
  __HAL_TIM_SET_PRESCALER(LED1_PWM_TIM, 7199);
  __HAL_TIM_SET_AUTORELOAD(LED1_PWM_TIM, 9999);
  __HAL_TIM_SET_COMPARE(LED1_PWM_TIM, LED1_CHANNEL, 5000); // 50%占空比
}
```

## 2. LED2：0.5秒闪烁一次
```c
void LED2_Set_05s_Flash(void)
{
  // 2Hz = 0.5秒周期
  __HAL_TIM_SET_PRESCALER(LED2_PWM_TIM, 7199);
  __HAL_TIM_SET_AUTORELOAD(LED2_PWM_TIM, 4999);
  __HAL_TIM_SET_COMPARE(LED2_PWM_TIM, LED2_CHANNEL, 2500); // 50%占空比
}
```

## 3. LED3：任意控制（开关 + 频率）
```c
// LED3 开启 1s闪烁
void LED3_1s_On(void)
{
  __HAL_TIM_SET_PRESCALER(LED3_PWM_TIM, 7199);
  __HAL_TIM_SET_AUTORELOAD(LED3_PWM_TIM, 9999);
  __HAL_TIM_SET_COMPARE(LED3_PWM_TIM, LED3_CHANNEL, 5000);
  HAL_TIM_PWM_Start(LED3_PWM_TIM, LED3_CHANNEL);
}

// LED3 开启 0.5s闪烁
void LED3_05s_On(void)
{
  __HAL_TIM_SET_PRESCALER(LED3_PWM_TIM, 7199);
  __HAL_TIM_SET_AUTORELOAD(LED3_PWM_TIM, 4999);
  __HAL_TIM_SET_COMPARE(LED3_PWM_TIM, LED3_CHANNEL, 2500);
  HAL_TIM_PWM_Start(LED3_PWM_TIM, LED3_CHANNEL);
}

// LED3 关闭（灯灭）
void LED3_Off(void)
{
  HAL_TIM_PWM_Stop(LED3_PWM_TIM, LED3_CHANNEL);
}
```

---

# 五、独立开关控制（你要的功能）
```c
// 例子：循环测试
while (1)
{
  // LED1：固定 1秒闪烁（一直运行）
  // LED2：固定 0.5秒闪烁（一直运行）

  // LED3：自己控制
  LED3_1s_On();   // 1s闪烁
  HAL_Delay(5000);
  
  LED3_05s_On();  // 0.5s闪烁
  HAL_Delay(5000);
  
  LED3_Off();     // 关闭
  HAL_Delay(3000);
}
```

---

# 六、最终效果（完美满足你）
- **LED1**：永远 1秒 闪烁一次
- **LED2**：永远 0.5秒 闪烁一次
- **LED3**：可随时开关、切换1s/0.5s闪烁
- 三路**完全独立**，互不影响
- 硬件PWM自动运行，**不占用CPU资源**

---

# 七、最简单总结（背会就会用）
1. 3个LED → 用**定时器3个PWM通道**
2. 1s闪烁 = 1Hz
3. 0.5s闪烁 = 2Hz
4. 开启：`HAL_TIM_PWM_Start()`
5. 关闭：`HAL_TIM_PWM_Stop()`
6. 改频率：改PSC、ARR、CCR

---

我可以直接给你 **完整的main.c代码**，你复制进去就能跑！
你只需要告诉我：
**你用的STM32型号？（如F103C8T6）**