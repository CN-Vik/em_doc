可以。你这次重新装，按我这个流程走。你现在的目标是：**先把 Ubuntu Server 22.04 成功装进台式机，网络/WiFi 后面再慢慢搞**。

下面是新手安装路线，尽量不要改高级选项。

---

# 一、重新开始前先记住几个原则

你这台机器目前没有网线，所以安装时：

```text
网络：可以跳过
Proxy：留空
Ubuntu Pro：跳过
第三方驱动：这次先不要勾选
Create bond / bridge / VLAN：都不要选
```

最重要的是磁盘页面：

```text
只安装到 INTEL 512GB SSD
不要动 58GB 的 U盘/读卡器
确认硬盘里没有重要资料
```

---

# 二、从 U 盘重新启动安装

如果现在卡在 `Rebooting...`，直接：

```text
长按电源键 5~10 秒关机
等几秒
再开机
```

开机后狂按：

```text
Dell 台式机一般按 F12
```

进入启动菜单后，选择你的 U 盘启动。

一般名字类似：

```text
UEFI: USB
UEFI: Kingston
UEFI: SanDisk
UEFI: Prolific Technology
```

优先选带 `UEFI` 的 U 盘启动项。

---

# 三、进入 Ubuntu Server 安装界面后的完整步骤

## 1. 语言选择

如果有语言选择，建议选：

```text
English
```

虽然你是中文用户，但 Server 版用英文更稳，报错也更容易搜索。

选好后：

```text
Enter
```

---

## 2. Keyboard configuration 键盘布局

一般保持默认：

```text
Layout: English (US)
Variant: English (US)
```

直接：

```text
Done
```

操作：

```text
Tab 切到 Done
Enter
```

---

## 3. Choose the type of installation 安装类型

这个页面你之前拍过。

选择：

```text
(X) Ubuntu Server
( ) Ubuntu Server (minimized)
```

这次重点：**取消第三方驱动搜索**。

也就是：

```text
[ ] Search for third-party drivers
```

不要勾选它。

原因是你刚才安装失败，有可能和没网络时安装器处理驱动/软件包有关。现在先保证系统装成功。

所以这一页最终应该是：

```text
(X) Ubuntu Server
( ) Ubuntu Server (minimized)

[ ] Search for third-party drivers
```

然后：

```text
Done
```

---

## 4. Network connections 网络配置

这里可能会看到：

```text
eno1
enp2s0
wlan0
wlp...
Create bond
Create bridge
Create VLAN
```

你这样判断：

### 情况 A：只有 `eno1` / `enp...`

这是有线网卡。你没网线，不用管。

直接：

```text
Done
```

如果提示：

```text
Continue without network?
```

选择：

```text
Continue without network
```

### 情况 B：看到 `wlan0` / `wlp...` / `wlx...`

这是无线网卡。

但是新手安装阶段我仍然建议你先别折腾，直接跳过也可以。
如果你想试着连 WiFi，也可以选无线网卡输入 WiFi 名和密码。

不过为了这次顺利安装，我建议：

```text
先跳过网络
系统装完后再配 WiFi
```

### 情况 C：看到 `Create bond`

不要选。

`Create bond` 是多网卡绑定，服务器机房用的，不是 WiFi。

这一页你只需要：

```text
找 Done
Enter
Continue without network
```

---

## 5. Proxy address 代理

这个页面如果出现：

```text
Proxy address:
```

直接留空。

不要填：

```text
127.0.0.1
localhost
```

直接：

```text
Done
```

---

## 6. Mirror address 镜像源

你之前看到：

```text
http://cn.archive.ubuntu.com/ubuntu/
The mirror location passed tests
```

保持默认即可。

直接：

```text
Done
```

如果你没网络，它可能测试失败，也没关系，继续即可。

---

# 四、磁盘分区步骤，非常重要

## 7. Guided storage configuration

这个页面是选择安装到哪个硬盘。

你应该选择：

```text
(X) Use an entire disk
```

磁盘选择你的 Intel SSD：

```text
INTEL SSDPEKNW512G8
476.939G
```

不要选 58GB 那个设备。

58GB 那个大概率是你的 U 盘或者读卡器：

```text
Prolific_Technology_Inc._SD_Card_Reader
58.242G
```

那个不要动。

---

## 8. LVM 怎么选？

页面上可能有：

```text
[X] Set up this disk as an LVM group
[ ] Encrypt the LVM group with LUKS
```

建议你这样：

```text
[X] Set up this disk as an LVM group
[ ] Encrypt the LVM group with LUKS
```

也就是：

```text
LVM 可以勾选
LUKS 加密不要勾选
```

原因：

```text
LVM：服务器默认常用，后面扩容方便
LUKS：磁盘加密，每次开机要输入密码，新手服务器不建议
```

然后：

```text
Done
```

---

## 9. Storage configuration 磁盘分区确认

你之前看到类似：

```text
/boot/efi    1.049G   fat32
/boot        2.000G   ext4
/            100.000G ext4
ubuntu-vg    LVM volume group
free space   373.886G
```

这个可以接受。

意思是：

```text
/boot/efi：UEFI 启动分区
/boot：Linux 内核启动文件
/：系统根目录
free space：LVM 里面剩余空间，后面可以扩容
```

新手不要乱改。

直接：

```text
Done
```

---

## 10. Confirm destructive action

这一步会警告你：

```text
确认要清空磁盘并写入分区
```

如果你确认 Intel 512GB SSD 里面没有重要数据，就选：

```text
Continue
```

注意：这一步之后，原硬盘数据基本就没了。

---

# 五、创建用户

## 11. Profile setup 用户信息

会让你填写：

```text
Your name:
Your server's name:
Pick a username:
Choose a password:
Confirm your password:
```

建议这样填，例如：

```text
Your name: vik
Your server's name: ubuntu-server
Pick a username: vik
Choose a password: 自己设置一个密码
Confirm your password: 再输入一次
```

注意：

```text
username 建议用小写英文，比如 vik
密码输入时屏幕不会显示，这是正常的
```

填完：

```text
Done
```

---

# 六、Ubuntu Pro 页面

## 12. Upgrade to Ubuntu Pro

这个页面你之前也见过。

直接跳过：

```text
Skip for now
```

不要去输入 code，不要绑定账号。

---

# 七、SSH 设置

## 13. SSH Setup

这个页面很重要。

如果看到：

```text
[ ] Install OpenSSH server
```

建议你勾选：

```text
[X] Install OpenSSH server
```

这样以后你可以从 Windows 电脑远程连接 Ubuntu Server。

但是因为你现在没网络，即使勾选了也暂时连不上。等 WiFi 配好后就能用。

下面如果有：

```text
Import SSH identity
```

选择：

```text
No
```

也就是：

```text
[X] Install OpenSSH server
Import SSH identity: No
```

然后：

```text
Done
```

如果它因为没网络装不了 SSH，也没关系，系统装完后可以再装。

---

# 八、Featured Server Snaps

## 14. 软件推荐页面

可能会出现一堆选项，比如：

```text
docker
microk8s
nextcloud
prometheus
```

你现在全部不要选。

直接：

```text
Done
```

原因：新手先把系统装好，后面需要什么再装。

---

# 九、开始安装

## 15. Installing system

然后会进入安装界面。

这一步要等。

正常会看到：

```text
Installing system
Configuring apt
Installing kernel
Installing grub
```

你就等它跑。

如果它成功，会出现类似：

```text
Install complete!
Reboot Now
```

然后选择：

```text
Reboot Now
```

---

# 十、重启时拔 U 盘

按下 `Reboot Now` 后，系统可能提示你：

```text
Please remove the installation medium, then press ENTER
```

意思是：

```text
拔掉 U 盘
按 Enter
```

你就：

```text
1. 拔掉安装 U 盘
2. 按 Enter
```

如果没有提示，也可以在电脑黑屏重启时拔掉 U 盘。

---

# 十一、第一次进入系统

正常启动后，你会看到登录界面：

```text
ubuntu-server login:
```

输入你刚才创建的用户名，比如：

```text
vik
```

然后输入密码。

注意：输入密码时不会显示星号，这是正常的。

登录成功后，应该看到类似：

```text
vik@ubuntu-server:~$
```

这就说明安装成功了。

---

# 十二、装完后第一件事：检查系统和网络

登录后执行：

```bash
ip link
```

看有哪些网卡。

可能看到：

```text
lo
eno1
wlp3s0
wlx001122334455
```

无线网卡一般是：

```text
wlan0
wlp...
wlx...
```

然后看 USB 无线网卡：

```bash
lsusb
```

如果是 PCIe 无线网卡：

```bash
lspci | grep -i network
```

---

# 十三、装完后配置 WiFi，大概流程

如果系统识别到了无线网卡，比如叫 `wlp3s0`，就编辑 netplan：

```bash
ls /etc/netplan/
```

假设文件叫：

```text
00-installer-config.yaml
```

编辑：

```bash
sudo nano /etc/netplan/00-installer-config.yaml
```

写成类似这样：

```yaml
network:
  version: 2
  renderer: networkd
  wifis:
    wlp3s0:
      dhcp4: true
      optional: true
      access-points:
        "你的WiFi名字":
          password: "你的WiFi密码"
```

注意把：

```text
wlp3s0
你的WiFi名字
你的WiFi密码
```

换成你自己的。

保存：

```text
Ctrl + O
Enter
Ctrl + X
```

应用：

```bash
sudo netplan generate
sudo netplan apply
```

测试：

```bash
ip addr
ping -c 4 8.8.8.8
ping -c 4 baidu.com
```

---

# 十四、你这次重装时的推荐选择汇总

你可以照着这个抄：

```text
Language:
English

Keyboard:
English US

Installation type:
(X) Ubuntu Server
( ) Ubuntu Server minimized
[ ] Search for third-party drivers

Network:
没网线就 Continue without network
不要选 Create bond / bridge / VLAN

Proxy:
留空

Mirror:
默认

Storage:
(X) Use an entire disk
选择 INTEL 512GB SSD
[X] Set up this disk as an LVM group
[ ] Encrypt with LUKS

Storage summary:
Done

Confirm destructive action:
Continue

Profile:
用户名 vik
主机名 ubuntu-server
密码自己设置

Ubuntu Pro:
Skip for now

SSH:
[X] Install OpenSSH server
Import SSH identity: No

Featured snaps:
全部不选
Done

Install complete:
Reboot Now
拔 U 盘
Enter
```

---

# 十五、如果又报错怎么办？

这次如果又出现：

```text
An error occurred during installation
```

不要马上重启，先选：

```text
View error report
```

拍清楚错误页面给我。

重点拍包含这些字样的地方：

```text
ERROR
Traceback
CurtinInstallError
apt
kernel
grub
```

我就能判断是 U 盘问题、ISO 问题、磁盘问题，还是网络/软件包问题。

你现在就按这个流程重新装：**第三方驱动不勾选，网络跳过，Ubuntu Pro 跳过，snaps 不选。**
