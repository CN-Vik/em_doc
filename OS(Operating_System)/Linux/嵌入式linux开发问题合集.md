嵌入式 Linux **不是绝对不能用 root 权限编译**，而是**强烈不建议用 root 编译**。尤其是 Buildroot、kernel、uboot、SDK 这种大工程，很多都会明确禁止或警告：

```bash
sudo make
```

原因不是“编译器不能用 root”，而是 **root 权限太大，容易把工程、系统、rootfs 权限搞乱，甚至误删宿主机文件**。

---

# 1. 先说结论

平时编译应该用普通用户：

```bash
make
./build.sh
./buildall.sh
```

不要这样：

```bash
sudo make
sudo ./build.sh
sudo ./buildall.sh
```

只有这些场景才可能需要 `sudo`：

```bash
sudo apt install xxx        # 安装 Ubuntu 软件包
sudo mount xxx              # 挂载镜像
sudo umount xxx             # 卸载镜像
sudo dd if=xxx of=/dev/sdX  # 烧录 SD 卡，危险
sudo fastboot flash xxx     # 有些烧录工具需要权限
```

**编译本身不应该用 sudo。**

---

# 2. 为啥不能用 root 编译？

## 原因一：会把工程文件变成 root 拥有

你如果执行：

```bash
sudo make
```

编译出来的文件可能变成：

```bash
-rw-r--r-- 1 root root xxx.o
-rwxr-xr-x 1 root root app
drwxr-xr-x 2 root root output
```

之后你再用普通用户编译，就会遇到：

```text
Permission denied
cannot remove xxx.o
cannot create directory
cannot write file
```

比如：

```bash
rm: cannot remove 'xxx.o': Permission denied
make: *** Permission denied
```

因为这些文件属于 root，你普通用户删不了、改不了。

---

## 原因二：清理工程会出问题

比如你之前用 root 编译过：

```bash
sudo make
```

然后你想清理：

```bash
make clean
```

可能失败：

```text
rm: cannot remove 'output/build/xxx': Permission denied
```

因为 `output/` 里面很多文件是 root 创建的。

这时候你只能修复权限：

```bash
sudo chown -R $USER:$USER ~/project/x2600
```

比如你当前用户是 `vik`，实际就是：

```bash
sudo chown -R vik:vik ~/project/x2600
```

这条命令的意思是：

> 把 `~/project/x2600` 目录下所有文件的所有者改回当前用户。

---

## 原因三：Buildroot 自己就不希望你用 root

Buildroot 里面会制作 rootfs。

rootfs 里确实需要很多文件属于 root，比如：

```text
/bin/busybox       root:root
/etc/passwd        root:root
/dev/console       root:root
/sbin/init         root:root
```

但 Buildroot 不是靠你真的用 root 编译来实现的。

它会用类似：

```text
fakeroot
```

的机制来“假装 root 权限”，在打包 rootfs 镜像时设置文件属主、权限、设备节点。

也就是说：

```text
宿主机编译：普通用户
生成 rootfs 权限：fakeroot 模拟 root
最终板子上：文件看起来属于 root
```

所以你不需要真的 `sudo make`。

---

# 3. 你可能会疑惑：rootfs 里文件不是要 root 权限吗？

对，目标板 rootfs 里很多文件应该是 root 拥有的。

比如板子上：

```bash
ls -l /sbin/init
```

可能是：

```text
-rwxr-xr-x 1 root root /sbin/init
```

但是这个 `root:root` 是在**制作 rootfs 镜像时设置进去的**，不是你在 Ubuntu 上用 root 编译出来的。

Buildroot 的流程大概是：

```text
普通用户编译 busybox、库、应用
        ↓
安装到 output/target
        ↓
fakeroot 阶段模拟 root 权限
        ↓
设置 rootfs 内部文件权限、属主、设备节点
        ↓
生成 rootfs.ext4 / rootfs.squashfs / rootfs.ubifs
```

所以正确逻辑是：

```text
编译时不要 root
打包 rootfs 时由工具处理 root 权限
```

---

# 4. root 编译最危险的地方：脚本可能误操作宿主机

嵌入式 SDK 里面经常有大量脚本：

```text
build.sh
buildall.sh
post-build.sh
post-image.sh
mkimage.sh
install.sh
clean.sh
```

这些脚本里面可能有：

```bash
rm -rf xxx
cp -a xxx yyy
mkdir -p xxx
install xxx yyy
```

如果脚本写错路径，比如变量为空：

```bash
rm -rf $TARGET_DIR/*
```

如果 `$TARGET_DIR` 意外为空，可能变成危险操作。

普通用户权限下，最多毁掉你自己的工程目录。

root 权限下，可能破坏宿主机系统目录。

这就是为啥很多构建系统不让你用 root 编译。

---

# 5. `sudo make` 和 `make` 的区别

普通编译：

```bash
make
```

权限大概是：

```text
只能操作当前用户有权限的文件
比较安全
文件属于当前用户 vik
```

root 编译：

```bash
sudo make
```

权限大概是：

```text
可以操作整个系统
生成文件属于 root
脚本出错影响更大
后续普通用户编译容易 Permission denied
```

所以嵌入式 Linux 开发里，原则是：

```text
能不用 sudo 就不用 sudo
编译绝对不要随手 sudo
```

---

# 6. 那内核能不能用 root 编译？

Linux kernel 本身技术上可以用 root 编译，但不推荐。

比如：

```bash
sudo make ARCH=mips
```

不推荐。

正确是：

```bash
make ARCH=mips
```

如果需要指定交叉编译器：

```bash
make ARCH=mips CROSS_COMPILE=mips-linux-gnu-
```

如果你用的是君正 SDK，通常是厂商脚本封装：

```bash
./buildall.sh
```

或者：

```bash
make
```

也应该普通用户执行。

---

# 7. 哪些操作可以用 sudo？

## 安装依赖可以用 sudo

```bash
sudo apt update
sudo apt install build-essential libncurses-dev bison flex
```

这是操作 Ubuntu 系统软件包，需要 root，正常。

---

## 烧录 SD 卡可能需要 sudo

比如：

```bash
sudo dd if=sdcard.img of=/dev/sdb bs=4M status=progress
```

这个需要访问块设备，所以需要 root。

但这个很危险，设备名写错会覆盖电脑硬盘。

---

## 挂载镜像可能需要 sudo

```bash
sudo mount rootfs.ext4 /mnt/rootfs
sudo umount /mnt/rootfs
```

这个也正常。

---

## 修改 USB 设备权限可以用 sudo

比如烧录工具访问 USB 失败时，可能需要：

```bash
sudo chmod
sudo udevadm
```

但这是配置设备权限，不是编译。

---

# 8. 哪些操作不该用 sudo？

这些一般不要用 sudo：

```bash
sudo make
sudo make menuconfig
sudo make clean
sudo make distclean
sudo ./build.sh
sudo ./buildall.sh
sudo cmake ..
sudo ninja
```

正确是：

```bash
make
make menuconfig
make clean
./build.sh
./buildall.sh
cmake ..
ninja
```

---

# 9. 如果你已经用 root 编译过怎么办？

如果你之前执行过：

```bash
sudo make
sudo ./buildall.sh
```

然后现在普通用户编译报权限错误，可以修复：

```bash
cd ~/project
sudo chown -R $USER:$USER x2600
```

或者直接：

```bash
sudo chown -R vik:vik ~/project/x2600
```

再检查：

```bash
ls -l ~/project/x2600
```

如果看到大部分文件是：

```text
vik vik
```

就正常了。

也可以查找 root 拥有的文件：

```bash
find ~/project/x2600 -user root | head
```

如果还有很多 root 文件，继续：

```bash
sudo chown -R $USER:$USER ~/project/x2600
```

---

# 10. 一个很常见的新手误区

你可能会想：

> 既然板子 Linux 里 root 权限最大，那我在 Ubuntu 上用 root 编译不是更好吗？

不是。

这里有两个 root：

```text
Ubuntu 宿主机的 root
目标板 rootfs 里的 root
```

它们不是一回事。

你在 Ubuntu 上 `sudo make`，是用 **宿主机 root** 编译。

你最终生成的 rootfs 里有 root 用户，是 **目标板系统里的 root**。

Buildroot 会帮你设置目标板 rootfs 的权限，不需要你用宿主机 root 去编译。

---

# 11. 推荐你以后这样操作

进入工程：

```bash
cd ~/project/x2600
```

确认目录属于自己：

```bash
ls -ld .
```

应该类似：

```text
drwxr-xr-x vik vik x2600
```

设置工具链环境：

```bash
export PATH=/home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin:$PATH
```

然后普通用户编译：

```bash
./buildall.sh
```

或者根据 SDK 要求：

```bash
make
```

不要加 `sudo`。

---

# 12. 总结

嵌入式 Linux 不推荐 root 编译，主要原因是：

```text
1. 编译产物会变成 root 拥有
2. 普通用户后续无法清理/修改
3. Buildroot 会用 fakeroot 处理 rootfs 权限，不需要 sudo
4. root 权限运行脚本风险很大
5. 工具链、缓存、环境变量容易被搞乱
6. 多人协作或 repo/git 工程容易出现权限混乱
```

你只要记住：

```text
安装系统依赖、挂载镜像、烧录设备：可以 sudo
编译 kernel、buildroot、uboot、app：不要 sudo
```

如果已经搞乱权限，用：

```bash
sudo chown -R $USER:$USER ~/project/x2600
```

把工程权限改回来。
