@[toc]

---
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/f9ae5fa0e52348748d281e18b7adf567.png#pic_center)

> **常见的stm32开发都是直接使用keil-MDK工具的，这是个集成开发环境，包含了代码编辑，编译，下载，调试，等功能，而且keil还是个图形化操作工具，直接可以点击图标案件就可以实现编译下载啥的工具了，但是在linux系统上没有这种图形化的集成开发环境，需要自己搭建开发环境，配置代码编辑器+编译器+下载器+调试器等等。下边介绍Linux(基于ubuntu)上配置stm32的开发环境，基于vscode。**

## **1.下载arm版本的gcc编译器**

> sudo apt-get install gcc-arm-none-eabi  

## **2.下载Openocd下载器和GDB调试器**

> sudo apt-get install openocd
sudo apt-get install gdb-multiarch

## **3.在vscode里边下载Cortex-Debug插件**

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/835a2d047d4f4e549747c4eacb8ecdee.png)
这个Cortex-Debug 插件可以直接实现类似于keil的图形化界面的调试功能。还可以一键下载程序，省的去敲超级麻烦的GDB命令去调试下载了
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/ccc0cfbd61aa4a0699b34dfaaaeea508.png)
## 4.获取stm32 makefile版例程
> git@gitee.com:free-people-in-time-and-space/linux_stm32f103_gcc_makefile.git
> https://gitee.com/free-people-in-time-and-space/linux_stm32f103_gcc_makefile.git

stm32 makefile版工程可以去找找stm32 cubemx 生成教程，直接用这个工具生成就可以了，不过我这也有例程可以直接下载下来用，省事儿了
## 5.coretex-Debug插件文件配置
要想使用Coretex-debug插件实现图形化debug功能还得配置文件，你上述工作全做好了之后会出现下边情况，生成了个.vscode 文件夹，里边有两个配置文件一个是launch一个是task的，只需要关注launch的就可以了
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/5d488b37d5c3478ea6523131b0eb2f99.png)
打开launch.json文件 按我文中说的配置就可以了
```javascript
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Cortex Debug",
            "cwd": "${workspaceRoot}",
            "executable": "./build/led.elf", //mcu程序可执行文件路径
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "openocd",
            "configFiles": [
                "interface/cmsis-dap.cfg",//调试器配置文件 我用的是CMSIS-DAP所以选的这个配置文件,
                                            // 绝对路径是/usr/share/openocd/scripts/interface

                "target/stm32f1x.cfg" //对应MCU系列的配置文件，绝对路径/usr/share/openocd/scripts/target
            ],
            "gdbPath": "/usr/bin/gdb-multiarch", //gdb-multiarch 默认路径，按照sudo apt install安装的应该改都是这个路径
            "armToolchainPath": "/usr/bin", //arm-none-eabi-gcc安装路径和gdb-multiarch同理
            "svdFile": "/home/wls/wls/linux_stm32f103_gcc_makefile/led/stm32f103x.svd",//看你具体的文件位置了
            "preLaunchTask": "Build"
        }
    ]
}

```

## 6.下载调试
上述所有配置都做好之后，就可以点击图片中的红色按钮了
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/211d4aace45e447ab93a91657f2bc7ea.png)
然后就进入了调试阶段，
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e8969c22cbee4d4ba1d165a9cc50d4af.png)
点击下图的红色方框关闭调试程序就下载到MCU里边去了
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/141de4ca1e0740d48c2fc83440ee6b93.png)

## 7. openocd 下载命令

#定义固件文件路径
path_elf = D:\windows10_file\Desktop\linux_stm32f103_gcc_makefile\led\build\led.elf

#定义 OpenOCD 命令
OPENOCD_CMD = openocd -c "gdb_port 50000" \
				-c "tcl_port 50001" \
				-c "telnet_port 50002" \
				-s /home/wls/wls/linux_stm32f103_gcc_makefile/led \
				-f /home/wls/.vscode-server/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl \
				-f interface/cmsis-dap.cfg -f target/stm32f1x.cfg \
				-c "program $(path_elf) verify reset exit"

download: 
	$(OPENOCD_CMD) 

 auto compile and download
rebuild_and_download:clean \
					all \
					download


## 8. make加速编译指令
>**make -j$(nproc)**  
>+ 根据当前系统的 CPU 核心数，自动设置并行编译的任务数。
>+ 例如，如果你的 CPU 有 8 个核心，make -j$(nproc) 相当于 make -j8