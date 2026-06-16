# STM32 UART+DMA 完整使用注意事项（含开关启停配置要点）
## 一、基础硬件&时钟前置注意
1. **时钟必须同时开启 UART、对应DMA、GPIO**
   - UART外设时钟：`__HAL_RCC_USARTx_CLK_ENABLE()`
   - DMA通道时钟：`__HAL_RCC_DMAx_CLK_ENABLE()`
   - TX/RX引脚GPIO时钟，TX推挽复用、RX浮空/上拉复用
2. **DMA通道映射不能冲突**
   不同型号（F1/F4/F103/L4/H7）DMA通道绑定固定：
   - F1：USART1_TX=DMA1_CH4，USART1_RX=DMA1_CH5；USART2_TX=DMA1_CH7、RX=DMA1_CH6
   - F4/H7支持DMA映射重映射，可手动选择流/通道，避免多个外设抢占同一DMA流
   > 同一DMA流同一时间只能给一个外设使用，多串口DMA必须分配不同流

3. **引脚复用模式正确**
   TX：GPIO_AF_PP 复用推挽；RX：GPIO_AF_FLOATING / GPIO_AF_PULLUP
   禁止TX设成开漏，高速波特率会丢包、波形畸变。

## 二、DMA结构体关键配置易错点（TX/RX分开）
### 通用DMA配置规范
1. **数据宽度匹配 UART DR寄存器（8bit）**
   `DMA_PERIPHERALDATAWIDTH_BYTE`、`DMA_MEMORYDATAWIDTH_BYTE`
   若设半字/字，收发数据错乱。
2. **外设地址固定、内存地址自增**
   - TX DMA：外设地址固定（&huart.Instance->DR），内存自增 `DMA_MINC_ENABLE`
     传输方向：`DMA_MEMORY_TO_PERIPH`
   - RX DMA：外设地址固定，内存自增；方向 `DMA_PERIPH_TO_MEMORY`
3. **循环模式 vs 正常模式（单次传输）**
   - 单次发送（普通数据包）：`DMA_NORMAL`，发完自动停止DMA，需重新启动
   - 循环接收（持续串口监听）：`DMA_CIRCULAR`，缓冲区满自动从头覆盖，适合不定长接收
4. **DMA优先级**
   多DMA同时工作时，串口DMA优先级建议中等/高，避免电机、ADC高优先级抢占导致丢字节。

### TX-DMA专属坑
1. DMA只搬运数据，**不会自动发送停止位、不会触发TC传输完成标志**，必须配合中断判断发送结束。
2. 发送缓冲区不能局部栈数组！DMA搬运时函数退出栈销毁，内存非法，必须全局/静态/堆内存。
3. 禁止中途直接覆盖发送缓冲区，DMA正在搬运会出现乱码。

### RX-DMA专属高频问题
1. **空闲中断 IDLE 必开（不定长接收核心）**
   DMA只能计数固定长度，不定长报文靠IDLE中断识别一帧结束；
   流程：IDLE中断 → 停止DMA → 计算接收长度 → 重置DMA计数器。
2. 循环模式下缓冲区溢出覆盖风险，必须做双缓冲/数据拷贝，不能直接原地处理数据。
3. 不要关闭DMA后不清空NDTR计数器，下次接收长度错乱。

## 三、UART结构体配置要点（配合DMA）
1. `huart.Init.Mode` 必须开启收发DMA模式
   ```c
   huart.Init.HwFlowCtl = UART_HWCONTROL_NONE; // 无硬件流控就关
   huart.Init.OverSampling = UART_OVERSAMPLING_16;
   // 开启DMA收发
   HAL_UART_Init(&huart);
   __HAL_UART_ENABLE_IT(&huart, UART_IT_IDLE); // 空闲中断
   ```
2. 波特率误差：高波特率（115200+）优先外部晶振，内部RC误差大会DMA丢包、校验错误。
3. 校验位、停止位要和上位机严格一致，否则DMA搬运数据全部无效。

## 四、中断配置注意（DMA+UART双中断）
1. 两类中断分开配置，优先级合理分组：
   - DMA中断：传输完成TC、半传输HT、错误TE
   - UART中断：IDLE空闲中断、奇偶错误PE、帧错误FE
2. 中断服务函数标准写法：
   - DMA回调：`HAL_UART_TxCpltCallback` / `HAL_UART_RxCpltCallback`
   - UART空闲中断：手动清除IDLE标志（SR寄存器，读SR+DR清标志，HAL库封装函数 `__HAL_UART_CLEAR_IDLEFLAG`）
3. 禁止在中断里长耗时处理，仅标记标志位，业务逻辑丢主循环。
4. DMA错误中断一定要开启：传输溢出、总线错误会触发TE，直接停止串口避免死机。

## 五、DMA收发启停开关操作规范（重点）
### 1. DMA发送启停（HAL库标准API）
#### 启动DMA发送
```c
// 单次DMA发送，阻塞/非阻塞二选一，DMA方式用异步
HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart, buf, len);
if(ret != HAL_OK) { /* 发送失败，DMA忙 */ }
```
**启动前检查条件**：
- 确认DMA流不在工作：`__HAL_DMA_GET_FLAG(&hdma_tx, DMA_FLAG_TCIF)` 判断传输完成
- 若上一次DMA未发完，直接调用会返回HAL_BUSY，必须等待TC中断再发下一包

#### 停止DMA发送（中途终止发送）
禁止直接关闭DMA时钟，标准流程：
```c
// 1. 关闭UART TX DMA请求
__HAL_UART_DISABLE_DMA(&huart, UART_DMA_TX);
// 2. 关闭DMA外设使能
__HAL_DMA_DISABLE(&hdma_tx);
// 3. 清除DMA所有标志位
__HAL_DMA_CLEAR_FLAG(&hdma_tx, DMA_FLAG_TCIF | DMA_FLAG_HTIF | DMA_FLAG_TEIF);
// 4. 重置NDTR计数
hdma_tx.Instance->NDTR = 0;
```
> 坑：只关DMA不关UART DMA使能，UART仍会发起DMA请求，总线报错HardFault。

### 2. DMA接收启停（循环/单次两种场景）
#### 开启循环DMA接收（常驻监听）
上电初始化一次即可持续接收：
```c
// 开启循环DMA接收
HAL_UART_Receive_DMA(&huart, rx_buf, RX_BUF_LEN);
```
#### 停止DMA接收（读取一帧数据后重置）
收到IDLE中断后标准关闭重置流程：
```c
// 1. 关闭DMA通道
__HAL_DMA_DISABLE(&hdma_rx);
// 2. 计算本次接收长度：RX_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_rx)
uint16_t rx_len = RX_BUF_LEN - hdma_rx.Instance->NDTR;
// 3. 清空DMA中断标志
__HAL_DMA_CLEAR_FLAG(&hdma_rx, DMA_FLAG_TCIF | DMA_FLAG_HTIF | DMA_FLAG_TEIF);
// 4. 重新赋值接收长度，开启DMA继续接收
hdma_rx.Instance->NDTR = RX_BUF_LEN;
__HAL_DMA_ENABLE(&hdma_rx);
__HAL_UART_ENABLE_DMA(&huart, UART_DMA_RX);
```
**禁止操作**：
1. 不关闭DMA直接修改NDTR，计数错乱、数据覆盖；
2. 循环模式下不清标志直接重启，TC中断重复触发；
3. 处理数据时不拷贝缓冲区，DMA后台覆盖数据。

### 3. 彻底关闭整个UART+DMA（休眠/关机场景）
完整关闭顺序（逆初始化，防止总线异常）：
1. 停止DMA收发，关闭DMA通道
2. 关闭UART DMA收发请求
3. 关闭UART外设 `__HAL_UART_DISABLE(&huart)`
4. 关闭UART时钟、DMA时钟（低功耗省电）
恢复开启顺序相反：开时钟 → 初始化GPIO → 初始化DMA → 初始化UART → 使能UART → 开启DMA收发。

## 六、高频踩坑&稳定性注意事项
1. **内存对齐**
   全局接收/发送数组建议4字节对齐 `__attribute__((aligned(4)))`，H7/F7等32位DMA架构不对齐会传输异常。
2. **禁止动态局部数组做DMA缓冲区**
   函数内 `uint8_t buf[128]` 栈内存，DMA后台搬运时函数退出，内存失效，HardFault乱码。
3. DMA半传输HT中断妙用
   大缓冲区开启HT中断，实现双缓冲，避免循环缓冲区数据被覆盖。
4. 多线程/多任务操作保护（RTOS下）
   发送、启停DMA加互斥锁，多任务同时调用`HAL_UART_Transmit_DMA`会产生BUSY冲突。
5. 错误处理
   - DMA TE错误：立即关闭DMA，复位串口，排查地址/时钟冲突；
   - UART PE/FE错误：上位机电平异常、波特率不匹配，清空错误标志；
6. 低功耗休眠注意
   DMA外设时钟关闭后DMA停止，唤醒后必须重新初始化DMA并启动接收；
7. 软件复位DMA不要直接修改寄存器，优先使用HAL库函数，寄存器操作前后同步状态标志。

## 七、极简操作口诀总结
### 配置阶段
时钟全开，通道不冲突；
内存自增，外设地址定；
RX开循环，IDLE中断必启；
缓冲区全局，栈数组禁用。

### 开关启停阶段
启动先判忙，忙则等完成；
停止先关UART DMA请求，再关DMA；
重置NDTR，清全部中断标志；
循环收帧后，重新开DMA监听。


---------
----------
--------
# 一、修改DMA发送数据长度（Transmit_DMA改len）所有注意事项
## 1. 核心前提：**必须等上一次DMA发送完全结束，才能修改长度、重新启动**
DMA发送分为两种模式：`DMA_NORMAL`（单次）、`DMA_CIRCULAR`（循环，TX几乎不用）
### （1）单次模式（99%串口发送场景）
HAL库 `HAL_UART_Transmit_DMA` 内部逻辑：
- 会先判断DMA是否正在运行，忙则直接返回 `HAL_BUSY`
- 内部自动写入 `hdma_tx.Instance->NDTR = len`

错误操作：
DMA还在发送，直接改NDTR / 直接调用Transmit_DMA
→ 数据半截错乱、丢字节、TC中断乱跳。

正确判断发送完成两种方式：
```c
// 方式1：查询标志
if(__HAL_DMA_GET_FLAG(&hdma_tx, DMA_FLAG_TCIF) == SET || huart.gState == HAL_UART_STATE_READY)
{
    // 可修改长度、发起新发送
}

// 方式2：用发送完成回调标记（推荐，无轮询）
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    tx_done_flag = 1;
}
```

### （2）循环DMA TX（极少用，不推荐串口发送）
循环模式下NDTR不会自动清零，**不支持中途动态改长度**；
如果强行改NDTR，传输边界完全混乱，只能先关闭DMA再重新配置长度。

## 2. 手动直接修改NDTR寄存器的硬性规则（不调用HAL库时）
如果你不使用`HAL_UART_Transmit_DMA`，直接操作寄存器改发送长度：
1. 先关闭DMA通道：`__HAL_DMA_DISABLE(&hdma_tx)`
2. 清除TC/HT/TE全部中断标志
3. 修改NDTR：`hdma_tx.Instance->NDTR = new_len;`
4. 重新开启DMA：`__HAL_DMA_ENABLE(&hdma_tx);`
5. 确保UART TX DMA请求已打开 `__HAL_UART_ENABLE_DMA(&huart, UART_DMA_TX)`

> 严禁DMA使能状态下直接写NDTR，DMA运行时NDTR为只读，写入无效，传输长度不变。

## 3. 缓冲区与长度匹配问题
1. 新长度不能超过数组实际大小，否则数组越界，HardFault乱码；
2. DMA搬运期间**不能修改缓冲区内容**，如需更新数据，必须等发送完成再填充新数据；
3. 缓冲区建议全局/static，禁止栈数组；4字节对齐（H7/F7必加）：
```c
uint8_t tx_buf[256] __attribute__((aligned(4)));
```

## 4. 中断配套注意
1. 新长度对应的TC中断会在**NDTR计数到0**时触发，长度变了回调触发时机同步变化；
2. 如果开了HT半传输中断，半传输点会随长度改变，大分包场景需留意；
3. 若上次发送遗留TC标志未清除，改长度重启后会立刻进一次虚假发送完成中断。

## 5. RTOS多任务场景额外注意
多任务同时调用发送/修改长度：
- 加互斥锁保护发送流程；
- 上锁后再判断是否发送完成、修改长度、启动DMA；
- 防止任务打断导致一半长度、一半旧数据混合发送。

## 6. 中途强制停止再发新包流程（需要打断当前发送）
业务需要立刻终止当前发送、换长度发新数据标准步骤：
```c
// 1. 关闭UART侧DMA请求，停止外设触发DMA
__HAL_UART_DISABLE_DMA(&huart, UART_DMA_TX);
// 2. 关闭DMA通道
__HAL_DMA_DISABLE(&hdma_tx);
// 3. 清空所有DMA中断标志
__HAL_DMA_CLEAR_FLAG(&hdma_tx, DMA_FLAG_TCIF | DMA_FLAG_HTIF | DMA_FLAG_TEIF);
// 4. 填充新数据、设置新长度
uint16_t new_len = 64;
// 5. 重新启动DMA发送（HAL方式最简单，自动处理NDTR）
HAL_UART_Transmit_DMA(&huart, tx_buf, new_len);
```

---

# 二、UART + DMA 完整初始化配置顺序（含时钟、DMA、UART、DMA使能、中断）
## 标准正确初始化顺序（不可颠倒，颠倒会DMA无响应、HardFault、中断失效）
### 阶段1：开启所有外设时钟（最先做）
1. 开启GPIO时钟（TX/RX引脚）
2. 开启USART外设时钟
3. 开启对应DMA外设时钟
> 原理：外设寄存器访问必须先开时钟，后配置，否则寄存器写不生效。

### 阶段2：GPIO初始化
TX：复用推挽 AF_PP
RX：复用浮空/上拉 AF_FLOATING / AF_PULLUP

### 阶段3：DMA句柄初始化（hdma_tx / hdma_rx）
1. 填充DMA结构体参数：
   - 外设地址、内存地址、数据宽度、MINC、PINC、模式NORMAL/CIRCULAR、传输方向、优先级
2. 调用 `HAL_DMA_Init(&hdma_tx);`
3. 将DMA句柄挂载到UART句柄：
```c
huart.hdmatx = &hdma_tx;
huart.hdmarx = &hdma_rx;
```
> 关键：必须先初始化DMA，再初始化UART，HAL_UART_Init内部会读取hdmatx配置。

### 阶段4：UART句柄初始化
1. 配置波特率、数据位、校验、停止位、收发模式
2. 调用 `HAL_UART_Init(&huart);`
内部自动完成：
- UART寄存器基础配置
- 绑定DMA请求通道映射（内部设置CR3寄存器DMAT/DMAR位）

### 阶段5：配置NVIC中断（DMA中断 + UART中断）
顺序无严格先后，但必须放在外设初始化之后：
1. DMA Tx/Rx中断NVIC配置：`HAL_NVIC_EnableIRQ(DMAx_Channelx_IRQn)`，设置抢占优先级
2. UART中断NVIC配置（IDLE、错误中断）
3. 在中断服务函数中调用HAL库分发函数：
```c
void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_tx);
}
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart);
}
```

### 阶段6：使能各类中断位（寄存器层使能，顺序无关）
1. DMA内部中断使能（TC/HT/TE）
```c
__HAL_DMA_ENABLE_IT(&hdma_tx, DMA_IT_TC | DMA_IT_TE);
__HAL_DMA_ENABLE_IT(&hdma_rx, DMA_IT_TC | DMA_IT_TE | DMA_IT_HT);
```
2. UART空闲中断使能（RX不定长接收必备）
```c
__HAL_UART_ENABLE_IT(&huart, UART_IT_IDLE);
```

### 阶段7：使能UART外设 + 开启DMA收发（最后执行）
1. 使能UART外设内核：`__HAL_UART_ENABLE(&huart);`
2. 启动DMA接收（常驻循环接收场景，上电初始化一次）
```c
HAL_UART_Receive_DMA(&huart, rx_buf, RX_BUF_LEN);
```
3. 发送DMA不用初始化时开启，业务需要时再调用`Transmit_DMA`

## 绝对不能颠倒的关键节点（高频踩坑）
1. ❌ 先初始化UART，再初始化DMA
   → HAL_UART_Init找不到有效hdmatx，DMA映射失效，串口DMA完全不工作
2. ❌ 未开DMA时钟就配置DMA寄存器
   → DMA寄存器写入全部无效，收发无数据
3. ❌ 先使能DMA通道，再初始化UART
   → UART CR3 DMAT位未配置，UART不会产生DMA请求，DMA空转无数据
4. ❌ 未挂载huart.hdmatx = &hdma_tx 直接初始化UART
   → UART和DMA完全断开，DMA不参与串口传输
5. ❌ 先开NVIC中断，再初始化外设
   → 外设寄存器未配置完成，中断乱触发，进入HardFault

## 精简标准初始化流程清单（复制可用）
1. RCC：GPIO → USART → DMA 时钟打开
2. GPIO_Init TX/RX
3. 填充hdma_tx/hdma_rx结构体 → HAL_DMA_Init
4. huart.hdmatx = &hdma_tx; huart.hdmarx = &hdma_rx;
5. 填充huart结构体 → HAL_UART_Init
6. NVIC配置DMA中断、USART中断
7. 使能DMA内部中断（TC/TE/HT）
8. 使能UART IDLE中断
9. __HAL_UART_ENABLE(&huart)
10. 按需启动Receive_DMA（接收常驻），发送业务动态启动Transmit_DMA

---

# 三、补充：关闭/重启整套DMA+UART再初始化顺序（休眠复位场景）
关闭（逆初始化）：
1. 停止DMA收发，关闭DMA通道
2. 关闭UART DMA请求
3. __HAL_UART_DISABLE(&huart)
4. HAL_DMA_DeInit(&hdma_tx) / HAL_UART_DeInit(&huart)
5. 关闭RCC时钟

重新开启：严格遵循上面「标准初始化顺序」从头走一遍，不能跳过时钟、DMA挂载步骤。