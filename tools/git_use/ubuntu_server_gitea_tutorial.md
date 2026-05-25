# Ubuntu Server 搭建 Gitea 私有 Git 服务器详细教程

> 适合场景：个人学习、实验室/小团队内网代码托管、初创团队早期自建 Git 服务。  
> 本教程基于你当前的实际环境整理：Ubuntu Server + Docker + Gitea + 局域网访问。

---

## 0. 最终目标

搭建完成后，我们希望实现下面几个目标：

```text
1. Ubuntu Server 可以正常联网
2. 局域网其他电脑可以 SSH 登录 Ubuntu Server
3. Ubuntu Server 上运行 Gitea 服务
4. 浏览器可以访问 Gitea 网页
5. 可以创建 Git 仓库
6. 客户端可以 git clone / git push / git pull
7. Gitea 数据可以备份和恢复
```

本教程中假设服务器 IP 为：

```text
192.168.1.100
```

Gitea Web 访问地址为：

```text
http://192.168.1.100:3000
```

Gitea SSH Git 端口为：

```text
2222
```

你的 Gitea 用户名示例为：

```text
tiger
```

你的测试仓库示例为：

```text
tiger/temp_list.git
```

---

# 一、安装 Ubuntu Server

## 1.1 准备工作

需要准备：

```text
1. 一台闲置电脑 / 台式机 / 小主机
2. 一个 U 盘，建议 8GB 以上
3. Ubuntu Server 镜像
4. Rufus / Ventoy 等 U 盘启动盘制作工具
5. 有线网口或者可用 USB 网卡 / 手机 USB 共享网络
```

建议优先使用有线网口安装 Ubuntu Server。Wi-Fi 网卡在 Server 版上可能需要额外驱动，新手容易卡在联网这一步。

---

## 1.2 安装 Ubuntu Server 的大致流程

启动电脑，进入 BIOS/UEFI，选择 U 盘启动。

安装过程中一般会经过这些步骤：

```text
1. 选择语言
2. 选择键盘布局
3. 配置网络
4. 配置磁盘分区
5. 设置用户名和密码
6. 安装 OpenSSH Server
7. 等待安装完成
8. 重启进入系统
```

建议安装时勾选：

```text
Install OpenSSH Server
```

这样后面可以直接从 Windows / 笔记本通过 SSH 远程操作服务器，不需要一直接显示器和键盘。

---

## 1.3 登录 Ubuntu Server

安装完成后，登录服务器。

示例用户：

```text
tiger_vik
```

查看服务器 IP：

```bash
hostname -I
```

或者：

```bash
ip addr
```

如果看到：

```text
192.168.1.100
```

说明服务器 IP 是 `192.168.1.100`。

---

## 1.4 测试网络是否正常

测试能不能访问外网：

```bash
ping -c 4 www.baidu.com
```

或者：

```bash
ping -c 4 8.8.8.8
```

如果能收到回复，说明网络正常。

更新软件源：

```bash
sudo apt update
sudo apt upgrade -y
```

---

# 二、配置 SSH 远程登录

## 2.1 确认 SSH 服务是否安装

执行：

```bash
sudo systemctl status ssh
```

如果没有安装，执行：

```bash
sudo apt install -y openssh-server
```

启动 SSH：

```bash
sudo systemctl enable ssh
sudo systemctl start ssh
```

---

## 2.2 从 Windows / 其他 Linux 电脑连接服务器

在客户端执行：

```bash
ssh tiger_vik@192.168.1.100
```

这里：

```text
tiger_vik       是 Ubuntu Server 的系统用户名
192.168.1.100   是 Ubuntu Server 的 IP
```

注意区分：

```text
Ubuntu 系统用户：tiger_vik
Gitea 网页用户：tiger
Gitea Git SSH 用户：git
```

这三个不是一回事。

---

# 三、安装 Docker 和 Docker Compose

Gitea 可以直接二进制安装，也可以用 Docker 安装。个人和小团队更推荐 Docker，因为：

```text
1. 安装简单
2. 升级方便
3. 服务隔离好
4. 备份迁移方便
5. 不容易污染系统环境
```

---

## 3.1 安装 Docker

执行：

```bash
sudo apt update
sudo apt install -y docker.io docker-compose-plugin
```

启动 Docker：

```bash
sudo systemctl enable docker
sudo systemctl start docker
```

查看 Docker 版本：

```bash
sudo docker version
```

测试 Docker 是否可用：

```bash
sudo docker run hello-world
```

---

## 3.2 解决普通用户执行 docker 没权限的问题

如果执行：

```bash
docker ps
```

出现：

```text
permission denied while trying to connect to the docker API at unix:///var/run/docker.sock
```

说明当前用户没有权限访问 Docker。

临时解决：

```bash
sudo docker ps
```

长期解决：把当前用户加入 `docker` 组：

```bash
sudo usermod -aG docker $USER
```

让权限立即生效：

```bash
newgrp docker
```

或者退出 SSH 后重新登录。

验证：

```bash
docker ps
```

如果不再报错，说明权限解决了。

注意：加入 `docker` 组后，该用户权限会很高，不要随便把不可信用户加入 docker 组。

---

# 四、使用 Docker Compose 部署 Gitea

## 4.1 创建 Gitea 目录

在服务器上执行：

```bash
mkdir -p ~/gitea
cd ~/gitea
```

---

## 4.2 创建 docker-compose.yml

执行：

```bash
nano docker-compose.yml
```

写入下面内容：

```yaml
networks:
  gitea:
    external: false

volumes:
  gitea_data:
    driver: local

services:
  server:
    image: docker.gitea.com/gitea:latest
    container_name: gitea
    restart: always

    environment:
      - USER_UID=1000
      - USER_GID=1000

    networks:
      - gitea

    volumes:
      - gitea_data:/data
      - /etc/timezone:/etc/timezone:ro
      - /etc/localtime:/etc/localtime:ro

    ports:
      - "3000:3000"
      - "2222:22"
```

保存退出：

```text
Ctrl + O
回车
Ctrl + X
```

---

## 4.3 端口解释

```yaml
ports:
  - "3000:3000"
  - "2222:22"
```

含义：

```text
3000:3000  表示宿主机 3000 端口映射到 Gitea 容器 3000 端口
2222:22    表示宿主机 2222 端口映射到 Gitea 容器 22 端口
```

也就是说：

```text
浏览器访问：
http://192.168.1.100:3000

Git SSH 访问：
ssh://git@192.168.1.100:2222/用户名/仓库名.git
```

为什么不用宿主机 22 端口？

因为 Ubuntu Server 自己的 SSH 登录服务一般已经占用了 22 端口。为了避免冲突，让 Gitea 的 Git SSH 用宿主机 2222 端口。

---

## 4.4 启动 Gitea

在 `~/gitea` 目录执行：

```bash
docker compose up -d
```

查看容器状态：

```bash
docker ps
```

正常应该看到类似：

```text
CONTAINER ID   IMAGE                           STATUS      PORTS                                                                                  NAMES
25886c2e875e   docker.gitea.com/gitea:latest   Up 2 days   0.0.0.0:3000->3000/tcp, 0.0.0.0:2222->22/tcp   gitea
```

说明：

```text
Gitea 容器：正在运行
Web 端口：3000 已开放
Git SSH 端口：2222 已开放
容器名：gitea
```

查看日志：

```bash
docker logs gitea --tail=100
```

持续查看日志：

```bash
docker logs -f gitea
```

---

# 五、防火墙设置

如果 Ubuntu Server 开启了 UFW 防火墙，需要放行端口。

查看防火墙状态：

```bash
sudo ufw status
```

如果状态是 `active`，执行：

```bash
sudo ufw allow 22/tcp
sudo ufw allow 3000/tcp
sudo ufw allow 2222/tcp
sudo ufw reload
```

端口含义：

```text
22    Ubuntu Server 远程 SSH 登录
3000  Gitea Web 页面
2222  Gitea Git SSH
```

---

# 六、初始化 Gitea 网页

## 6.1 打开 Gitea 页面

在浏览器访问：

```text
http://192.168.1.100:3000
```

第一次打开会进入安装初始化页面。

---

## 6.2 数据库选择

个人学习、小团队内网使用，可以先选：

```text
SQLite3
```

优点：

```text
1. 不用额外安装 MySQL/PostgreSQL
2. 配置最简单
3. 数据直接保存在 Gitea 数据目录里
```

缺点：

```text
1. 不适合大量用户
2. 不适合高并发
3. 后期换数据库类型会比较麻烦
```

如果你以后准备作为公司长期代码平台，建议一开始就用 PostgreSQL 或 MySQL/MariaDB。

---

## 6.3 关键配置项建议

假设服务器 IP 是：

```text
192.168.1.100
```

建议填写：

```text
站点标题：Vik Gitea 或 Tiger Gitea
服务器域名：192.168.1.100
Gitea 基础 URL：http://192.168.1.100:3000/
HTTP 监听端口：3000
SSH 服务器域名：192.168.1.100
SSH 服务器端口：2222
```

这里最重要的是：

```text
Gitea 基础 URL 要填 http://192.168.1.100:3000/
SSH 服务器端口要填 2222
```

否则网页上显示的 clone 地址可能不正确。

---

## 6.4 创建管理员账号

初始化页面底部会让你创建管理员账号。

例如：

```text
用户名：tiger
邮箱：你的邮箱
密码：设置强密码
```

创建完成后，Gitea 就可以正常登录了。

---

# 七、创建第一个仓库

登录 Gitea 后，右上角点击：

```text
+ -> New Repository / 新建仓库
```

示例仓库名：

```text
temp_list
```

如果用户名是 `tiger`，仓库地址就是：

```text
http://192.168.1.100:3000/tiger/temp_list
```

仓库 Git 地址有两种：

HTTP：

```text
http://192.168.1.100:3000/tiger/temp_list.git
```

SSH：

```text
ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

---

# 八、客户端 Git 基本配置

## 8.1 配置 Git 用户名和邮箱

在客户端电脑执行：

```bash
git config --global user.name "vik"
git config --global user.email "vik@example.com"
```

查看配置：

```bash
git config --global --list
```

如果没有配置，提交时会报：

```text
作者身份未知
*** 请告诉我您是谁。
fatal: 不允许空的姓名
```

解决方法就是配置 `user.name` 和 `user.email`。

---

# 九、使用 HTTP 方式 clone 和 push

HTTP 方式比较容易理解，适合先测试服务是否正常。

## 9.1 克隆仓库

```bash
git clone http://192.168.1.100:3000/tiger/temp_list.git
```

进入仓库：

```bash
cd temp_list
```

新建 README：

```bash
echo "# temp_list" > README.md
```

提交：

```bash
git add .
git commit -m "init temp_list repo"
```

推送：

```bash
git push -u origin main
```

如果默认分支是 master，则执行：

```bash
git push -u origin master
```

查看当前分支：

```bash
git branch
```

---

## 9.2 HTTP push 需要输入什么账号密码？

如果提示：

```text
Username for 'http://192.168.1.100:3000':
Password for 'http://tiger@192.168.1.100:3000':
```

这里输入的是：

```text
Username：Gitea 网页用户名，例如 tiger
Password：Gitea 网页密码，或者 Access Token
```

注意：不是 Ubuntu Server 的登录密码，也不是 Linux 用户 `tiger_vik` 的密码。

有些版本或配置下，更推荐用 Access Token 代替网页登录密码。

---

# 十、使用 SSH 方式 clone 和 push

SSH 方式更适合长期使用，因为不用每次输入网页密码。

## 10.1 一定要注意端口

你的 Gitea SSH 端口是：

```text
2222
```

所以正确地址是：

```bash
ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

错误地址是：

```bash
git@192.168.1.100:tiger/temp_list.git
```

为什么错？

因为这种写法默认走 22 端口。

但是：

```text
22    是 Ubuntu Server 自己的 SSH 登录端口
2222  才是 Gitea 的 Git SSH 端口
```

所以你之前执行 `git push` 出现：

```text
git@192.168.1.100's password:
Permission denied, please try again.
```

本质原因是：你连到了 Ubuntu Server 的 22 端口，而不是 Gitea 的 2222 端口。

---

## 10.2 生成 SSH Key

在客户端电脑执行：

```bash
ssh-keygen -t ed25519 -C "vik@gitea"
```

一路回车即可。

查看公钥：

```bash
cat ~/.ssh/id_ed25519.pub
```

输出类似：

```text
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIxxxxxxxx vik@gitea
```

复制整行。

---

## 10.3 把 SSH 公钥添加到 Gitea

打开 Gitea 网页：

```text
http://192.168.1.100:3000
```

进入：

```text
头像 -> Settings -> SSH / GPG Keys -> Add Key
```

把刚才复制的公钥粘贴进去。

---

## 10.4 测试 SSH 是否连到 Gitea

执行：

```bash
ssh -p 2222 git@192.168.1.100
```

正常会看到类似：

```text
Hi there, tiger! You've successfully authenticated, but Gitea does not provide shell access.
```

这说明 SSH key 已经生效。

如果还是提示：

```text
git@192.168.1.100's password:
```

说明：

```text
1. 没有连到 Gitea 的 2222 端口
2. 公钥没有添加到 Gitea
3. 当前客户端用的不是对应私钥
4. Gitea SSH 配置的端口显示不正确
```

---

## 10.5 修改已有仓库的 remote 地址

查看当前 remote：

```bash
git remote -v
```

如果看到：

```text
origin  git@192.168.1.100:tiger/temp_list.git (fetch)
origin  git@192.168.1.100:tiger/temp_list.git (push)
```

说明它默认走 22 端口，需要改成 2222。

执行：

```bash
git remote set-url origin ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

再次查看：

```bash
git remote -v
```

应该变成：

```text
origin  ssh://git@192.168.1.100:2222/tiger/temp_list.git (fetch)
origin  ssh://git@192.168.1.100:2222/tiger/temp_list.git (push)
```

然后推送：

```bash
git push -u origin main
```

---

# 十一、本地已有项目推送到 Gitea

假设你本地已经有一个项目目录：

```bash
cd ~/project/temp_list
```

如果还不是 Git 仓库：

```bash
git init
```

添加文件：

```bash
git add .
```

提交：

```bash
git commit -m "init project"
```

添加远程仓库：

```bash
git remote add origin ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

如果 remote 已存在，就改地址：

```bash
git remote set-url origin ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

推送：

```bash
git push -u origin main
```

如果分支叫 master：

```bash
git push -u origin master
```

---

# 十二、如何判断 Gitea 是否真正搭建成功

服务端搭建成功的标志：

```bash
docker ps
```

能看到：

```text
gitea   Up   0.0.0.0:3000->3000/tcp, 0.0.0.0:2222->22/tcp
```

浏览器能打开：

```text
http://192.168.1.100:3000
```

能登录 Gitea 账号。

能创建仓库。

客户端能 clone：

```bash
git clone ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

客户端能 push：

```bash
git push
```

只要这些都通了，就可以认为：

```text
Ubuntu Server + Gitea 私有 Git 服务器搭建完成
```

---

# 十三、常用维护命令

进入 Gitea 部署目录：

```bash
cd ~/gitea
```

查看容器：

```bash
docker ps
```

查看 compose 状态：

```bash
docker compose ps
```

查看日志：

```bash
docker logs gitea --tail=100
```

持续查看日志：

```bash
docker logs -f gitea
```

停止 Gitea：

```bash
docker compose down
```

启动 Gitea：

```bash
docker compose up -d
```

重启 Gitea：

```bash
docker restart gitea
```

---

# 十四、备份 Gitea

## 14.1 为什么要备份

Gitea 里保存了：

```text
1. 用户账号
2. 仓库代码
3. issue
4. wiki
5. pull request
6. 配置文件
7. SSH key
8. 数据库
```

所以一定要备份。

---

## 14.2 查看 volume 名称

执行：

```bash
docker volume ls
```

可能看到：

```text
gitea_gitea_data
```

---

## 14.3 使用 tar 备份 Docker volume

进入备份目录：

```bash
mkdir -p ~/backup
cd ~/backup
```

执行备份：

```bash
docker run --rm \
  -v gitea_gitea_data:/data \
  -v $PWD:/backup \
  ubuntu \
  tar czf /backup/gitea_data_backup.tar.gz /data
```

备份完成后，会得到：

```text
gitea_data_backup.tar.gz
```

建议把这个文件复制到其他电脑、移动硬盘或者 NAS。

---

## 14.4 恢复 Docker volume

先停止 Gitea：

```bash
cd ~/gitea
docker compose down
```

恢复：

```bash
docker run --rm \
  -v gitea_gitea_data:/data \
  -v $PWD:/backup \
  ubuntu \
  tar xzf /backup/gitea_data_backup.tar.gz -C /
```

重新启动：

```bash
docker compose up -d
```

---

## 14.5 使用 Gitea dump 备份

也可以用 Gitea 自带的 dump 命令：

```bash
docker exec -u git gitea gitea dump -c /data/gitea/conf/app.ini
```

然后查看容器内生成的 zip 文件位置，或者把它复制出来。

例如：

```bash
docker cp gitea:/data/gitea-dump-xxxx.zip ./
```

实际文件名要根据命令输出确认。

---

# 十五、升级 Gitea

## 15.1 不建议长期使用 latest

你现在使用的是：

```yaml
image: docker.gitea.com/gitea:latest
```

学习阶段可以，但长期使用建议固定版本，例如：

```yaml
image: docker.gitea.com/gitea:1.26.2
```

这样可以避免某次自动拉取新镜像后，因为版本变化导致不可预期问题。

---

## 15.2 升级步骤

升级前先备份。

然后进入目录：

```bash
cd ~/gitea
```

拉取新镜像：

```bash
docker compose pull
```

重新启动：

```bash
docker compose up -d
```

查看日志：

```bash
docker logs -f gitea
```

---

# 十六、常见问题排查

## 16.1 docker ps 报 permission denied

现象：

```text
permission denied while trying to connect to the docker API at unix:///var/run/docker.sock
```

原因：当前用户没有 Docker 权限。

解决：

```bash
sudo usermod -aG docker $USER
newgrp docker
```

或者重新登录 SSH。

---

## 16.2 git commit 报“作者身份未知”

现象：

```text
作者身份未知
*** 请告诉我您是谁。
fatal: 不允许空的姓名
```

解决：

```bash
git config --global user.name "vik"
git config --global user.email "vik@example.com"
```

然后重新提交：

```bash
git commit -m "init repo"
```

---

## 16.3 git push 报 refs/heads/main 没有匹配

现象：

```text
error: 源引用规格 refs/heads/main 没有匹配
```

原因：本地还没有成功 commit。

解决：

```bash
git status
git add .
git commit -m "init repo"
git push -u origin main
```

---

## 16.4 SSH push 一直问 git@192.168.1.100 的密码

现象：

```text
git@192.168.1.100's password:
Permission denied, please try again.
```

原因通常是 remote 地址用了默认 22 端口：

```text
git@192.168.1.100:tiger/temp_list.git
```

这种写法默认连 22 端口，连到的是 Ubuntu Server 自己的 SSH，不是 Gitea。

解决：

```bash
git remote set-url origin ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

测试：

```bash
ssh -p 2222 git@192.168.1.100
```

---

## 16.5 浏览器打不开 Gitea

检查容器：

```bash
docker ps
```

检查日志：

```bash
docker logs gitea --tail=100
```

检查防火墙：

```bash
sudo ufw status
```

放行端口：

```bash
sudo ufw allow 3000/tcp
sudo ufw allow 2222/tcp
sudo ufw reload
```

检查服务器 IP：

```bash
hostname -I
```

确认浏览器访问的是正确 IP：

```text
http://192.168.1.100:3000
```

---

# 十七、建议的最终使用方式

对于你当前学习和自建服务器场景，建议这样用：

```text
部署方式：Docker Compose
数据库：SQLite 起步
Web 端口：3000
Git SSH 端口：2222
访问范围：局域网内访问
仓库推送方式：SSH
备份方式：定期备份 Docker volume + Gitea dump
```

推荐平时使用 SSH 地址：

```bash
ssh://git@192.168.1.100:2222/tiger/仓库名.git
```

不要使用：

```bash
git@192.168.1.100:tiger/仓库名.git
```

除非你把 Gitea SSH 真正配置到了宿主机 22 端口。

---

# 十八、完整验证清单

搭建完成后，按下面清单检查：

```text
[ ] Ubuntu Server 能联网
[ ] 可以从其他电脑 SSH 登录 Ubuntu Server
[ ] Docker 正常运行
[ ] docker ps 能看到 gitea 容器
[ ] 浏览器能打开 http://192.168.1.100:3000
[ ] 能登录 Gitea 网页账号
[ ] 能创建仓库
[ ] HTTP clone 正常
[ ] SSH key 已添加到 Gitea
[ ] ssh -p 2222 git@192.168.1.100 测试通过
[ ] git push 成功
[ ] 已做一次备份测试
```

如果这些都完成，就可以认为：

```text
Ubuntu Server + Gitea 私有 Git 服务器已经完整搭建完成
```

---

# 十九、你这次遇到的问题对应总结

## 问题 1：docker ps 没权限

你遇到：

```text
permission denied while trying to connect to the docker API at unix:///var/run/docker.sock
```

原因：当前用户 `tiger_vik` 不在 docker 组。

解决：

```bash
sudo usermod -aG docker $USER
newgrp docker
```

---

## 问题 2：commit 失败

你遇到：

```text
作者身份未知
fatal: 不允许空的姓名
```

原因：Git 没配置提交用户名和邮箱。

解决：

```bash
git config --global user.name "vik"
git config --global user.email "vik@example.com"
```

---

## 问题 3：push 提示 refs/heads/main 没有匹配

原因：commit 没成功，所以本地 main 分支还没有任何提交。

解决：

```bash
git add .
git commit -m "init test repo"
git push -u origin main
```

---

## 问题 4：SSH push 一直要 git@192.168.1.100 的密码

你当时的 remote 是：

```text
git@192.168.1.100:tiger/temp_list.git
```

它默认走 22 端口，连错了。

应该改成：

```bash
git remote set-url origin ssh://git@192.168.1.100:2222/tiger/temp_list.git
```

然后配置 SSH key，再 push。

---

# 二十、推荐目录和命名规范

服务器部署目录：

```text
~/gitea
```

备份目录：

```text
~/backup
```

仓库命名建议：

```text
em_doc
x2600_sdk_note
gd32_driver_demo
esp32_lvgl_demo
linux_driver_study
```

不要使用太随意的名字，例如：

```text
test1
111
abc
new
```

后面项目多了会很难管理。

---

# 二十一、后续可以继续完善的功能

后面有需要时，可以继续加：

```text
1. 固定服务器静态 IP
2. 使用域名访问，例如 http://git.local:3000
3. 配置 HTTPS
4. 配置 Nginx 反向代理
5. 配置邮件通知
6. 配置定时自动备份
7. 配置组织和团队权限
8. 配置 Git LFS
9. 配置 Gitea Actions
10. 迁移 GitHub / GitLab 仓库到 Gitea
```

当前阶段先把下面这个闭环跑通最重要：

```text
创建仓库 -> clone -> 修改文件 -> commit -> push -> 网页能看到代码
```

这个闭环跑通后，Gitea 服务器就真正可以用了。

