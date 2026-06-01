我直接给你**最简单、一步到位的 ADB 推送命令**，你复制粘贴就能用，不用改任何东西！

# 一、直接执行这 3 条命令（最稳妥）
确保你的电脑已经连接开发板，且 **ADB 能识别设备**。

在你**电脑上存放这 3 个 .ko 文件的文件夹**里打开终端/CMD，执行：

```bash
adb push bmi160_core.ko /tmp/kernel_drv_mode/
adb push bmi160_spi.ko /tmp/kernel_drv_mode/
adb push bmi160_spi_board.ko /tmp/kernel_drv_mode/
```

执行完就全部推送到开发板的 `/tmp/kernel_drv_mode/` 目录下了。

---

# 二、如果你想**一条命令全部推送**
```bash
adb push bmi160*.ko /tmp/kernel_drv_mode/
```
`bmi160*.ko` 会自动匹配所有以 `bmi160` 开头、`.ko` 结尾的文件。

---

# 三、推送完验证是否成功
```bash
adb shell ls /tmp/kernel_drv_mode/
```
能看到那 3 个文件就说明成功了。

---

# 四、常见小问题
1. **提示目录不存在**
   先创建目录：
   ```bash
   adb shell mkdir -p /tmp/kernel_drv_mode
   ```
2. **权限不足**
   先获取 root：
   ```bash
   adb root
   adb remount
   ```
   然后再推送。

---

### 总结
- **单文件推送**：`adb push 文件名.ko 目标路径`
- **批量推送**：`adb push *.ko 目标路径`
- **目标目录**：`/tmp/kernel_drv_mode/`

你直接复制我给的命令就能用！