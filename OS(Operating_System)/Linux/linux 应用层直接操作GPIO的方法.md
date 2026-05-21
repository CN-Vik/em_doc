了解！你使用的是 **Rockchip RK3588S** 平台，需要操作 `GPIO3_D5_d` 这个引脚（即 `MCU_JTAG_TMS_M1/.../GPIO3_D5_d`）。以下是基于你提供的系统信息的**具体操作步骤**：

---

### 🔍 **第一步：确认 GPIO 系统编号**
在 RK3588 芯片中，GPIO 编号规则为：  
**GPIO 全局编号 = bank编号 × 32 + 组内序号**  

根据引脚 `GPIO3_D5`：
- **bank 编号**：3（对应 GPIO3）
- **组内序号**：D5 = 5 + 24 = 29（D组偏移24）  
  （*RK3588 GPIO分组：A=0~7, B=8~15, C=16~23, D=24~31*）

👉 **计算 GPIO 全局编号**：  
`3 × 32 + 29 = 125`  
**你需要操作的 GPIO 编号是 `125`**

---

### ⚙️ **第二步：操作 GPIO（两种方法）**

#### **方法1：通过 sysfs 文件系统（直接操作）**
```bash
# 1. 导出 GPIO125（激活控制权限）
echo 125 > /sys/class/gpio/export

# 2. 查看生成的 GPIO 目录（确保出现 gpio125）
ls /sys/class/gpio

# 3. 进入 GPIO125 目录
cd /sys/class/gpio/gpio125

# 4. 设置为输出模式
echo out > direction

# 5. 拉高电平
echo 1 > value

# 6. 拉低电平
echo 0 > value

# 完成后取消导出（可选）
echo 125 > /sys/class/gpio/unexport
```

#### **方法2：使用 libgpiod 工具（推荐更可靠的方式）**
```bash
# 1. 安装工具包
sudo apt update
sudo apt install gpiod

# 2. 查找 GPIO 对应的芯片编号（GPIO3 属于 gpiochip96）
gpiodetect  # 输出示例：
            # gpiochip0 [......]
            # gpiochip96 [......]   # ← GPIO3 在这里

# 3. 操作 GPIO：
# 语法：gpioset <芯片名> <组内序号>=<值>
gpioset gpiochip96 29=1  # 拉高（D5 = 组内偏移29）
gpioset gpiochip96 29=0  # 拉低
```

---

### ⚠️ **关键注意事项**
1. **复用功能冲突**：  
   如果该引脚被设备树配置为其他功能（如 `UART9_TX_M2`），操作会失败。  
   **解决方案**：
   ```bash
   # 查看当前引脚复用状态（需安装 io 工具）
   sudo apt install rockchip-io
   rockchip_io -p PD5   # PD5 对应 GPIO3_D5
   ```
   若显示 `MUX: UART9_TX`，需修改设备树将其设为 `GPIO`。

2. **权限问题**：
   - 操作 `/sys/class/gpio` 需要 root 权限
   - 可将用户加入 `gpio` 组避免 sudo：
     ```bash
     sudo usermod -aG gpio $(whoami)
     # 重启后生效
     ```

3. **快速电平切换限制**：  
   sysfs 最大频率约 100Hz，如需高速控制需：
   - 编写内核驱动
   - 使用 `/dev/mem` 直接映射寄存器（高风险，需 root）

---

### 📌 **操作结果验证**
用万用表测量引脚电压：
- 执行 `echo 1 > value` → 应显示 3.3V（或 SOC 工作电压）
- 执行 `echo 0 > value` → 应显示 0V

> 💡 **提示**：若操作失败，检查 `dmesg | grep gpio` 查看内核报错信息，通常原因包括复用冲突或物理驱动未启用。