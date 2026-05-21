你这个仓库才 **149 个对象**，不算大，但速度只有 **17 KiB/s**，基本就是 **GitHub 网络链路慢**，不是仓库太大。

你现在用的是：

```bash
git clone git@github.com:Vik-W001/em_doc.git
```

这是 **SSH 方式克隆**。注意：
**SSH 克隆不一定走 Git 的 http 代理配置**，所以很多人开了代理还是慢。

---

# 最推荐方案：改用 HTTPS + Git 代理

先中断当前 clone：

```bash
Ctrl + C
```

删掉没下完的目录：

```bat
rmdir /s /q em_doc
```

然后设置 Git 代理。假设你的代理端口是 `127.0.0.1:7897`，可以这样：

```bat
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897
```

然后改用 HTTPS 克隆：

```bat
git clone https://github.com/Vik-W001/em_doc.git
```

这个通常比 SSH 稳很多。

---

# 如果你用的是 SOCKS5 代理

有些代理软件端口是 SOCKS5，不是 HTTP。那就这样配：

```bat
git config --global http.proxy socks5h://127.0.0.1:7897
git config --global https.proxy socks5h://127.0.0.1:7897
```

然后：

```bat
git clone https://github.com/Vik-W001/em_doc.git
```

如果不知道自己是 HTTP 还是 SOCKS5，先用 HTTP 方式试，失败再换 SOCKS5。

---

# 如果只想要最新版，不要历史记录

这个更快：

```bat
git clone --depth 1 https://github.com/Vik-W001/em_doc.git
```

`--depth 1` 的意思是：

```text
只下载最新一次提交，不下载完整历史记录
```

如果你只是看文档、写代码、同步最新版，一般够用。

以后如果想补全历史：

```bat
git fetch --unshallow
```

---

# 如果你坚持用 SSH 克隆

你现在这个命令：

```bash
git clone git@github.com:Vik-W001/em_doc.git
```

走的是 SSH。可以先让 GitHub SSH 改走 443 端口。

在 Windows 里创建或编辑这个文件：

```text
C:\Users\ufo28\.ssh\config
```

写入：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ServerAliveInterval 60
    ServerAliveCountMax 3
```

然后测试：

```bat
ssh -T git@github.com
```

再 clone：

```bat
git clone git@github.com:Vik-W001/em_doc.git
```

这个方法适合 **22 端口被干扰或不稳定** 的情况。

---

# SSH 也想走代理的话

如果你用的是 Git for Windows，一般可以用 `connect` 做 SSH 代理。

编辑：

```text
C:\Users\ufo28\.ssh\config
```

HTTP 代理写法：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ProxyCommand connect -H 127.0.0.1:7897 %h %p
    ServerAliveInterval 60
```

SOCKS5 代理写法：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ProxyCommand connect -S 127.0.0.1:7897 %h %p
    ServerAliveInterval 60
```

然后测试：

```bat
ssh -T git@github.com
```

成功后再：

```bat
git clone git@github.com:Vik-W001/em_doc.git
```

---

# 查看当前 Git 代理配置

```bat
git config --global --get http.proxy
git config --global --get https.proxy
```

取消代理：

```bat
git config --global --unset http.proxy
git config --global --unset https.proxy
```

---

# 我建议你现在直接这样做

你这个仓库不大，最省事的是：

```bat
Ctrl + C
rmdir /s /q em_doc

git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897

git clone --depth 1 https://github.com/Vik-W001/em_doc.git
```

一句话总结：

```text
GitHub 慢，优先用 HTTPS + Git 代理 + --depth 1。
SSH 克隆要另外配 SSH 代理，普通 Git http.proxy 不一定管用。
```

-------
-------
------
方便，**push 本身没问题**。只是不同 clone 方式，push 认证方式不一样。

## 你现在有两个选择

### 方案 A：HTTPS clone，HTTPS push

你如果这样 clone：

```bat
git clone https://github.com/Vik-W001/em_doc.git
```

那后面 push 就是：

```bat
cd em_doc
git add .
git commit -m "update docs"
git push
```

第一次 push 时可能会让你输入 GitHub 用户名和密码。注意：**GitHub 命令行里已经不能用账号密码直接认证了，要用 Personal Access Token，也就是 PAT，代替密码**。GitHub 官方文档也说明，HTTPS Git 操作需要用 token 或 Git Credential Manager 缓存凭据。([GitHub Docs][1])

也就是说：

```text
Username: 你的 GitHub 用户名
Password: 粘贴 GitHub Token，不是登录密码
```

Windows 上 Git for Windows 通常自带 **Git Credential Manager**，登录一次后，后面 push 一般就不用重复输入了。([GitHub Docs][2])

---

### 方案 B：SSH clone，SSH push

你如果这样 clone：

```bat
git clone git@github.com:Vik-W001/em_doc.git
```

那后面 push 也是：

```bat
cd em_doc
git add .
git commit -m "update docs"
git push
```

只要 SSH key 配好了，就不需要输 GitHub token。长期用起来更舒服。

如果普通 SSH 端口慢或者不稳定，可以把 GitHub SSH 改成走 443 端口：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
```

GitHub 官方也支持这种方式，用来解决普通 SSH 连接不通或不稳定的问题。([GitHub Docs][3])

---

## 我建议你怎么选？

你是自己的 GitHub 仓库，**长期建议用 SSH**：

```text
clone / pull / push 都统一用 SSH
不用每次管 token
配好一次后最省心
```

但是你刚才的问题是 **clone 太慢**。所以可以这样：

### 推荐做法

先把 SSH 配成 443：

```text
C:\Users\ufo28\.ssh\config
```

写入：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ServerAliveInterval 60
    ServerAliveCountMax 3
```

测试：

```bat
ssh -T git@github.com
```

然后继续用：

```bat
git clone git@github.com:Vik-W001/em_doc.git
```

以后 push：

```bat
cd em_doc
git add .
git commit -m "update"
git push
```

---

## 如果你已经用 HTTPS 克隆了，后面又想改成 SSH

进入仓库目录：

```bat
cd em_doc
```

查看当前远程地址：

```bat
git remote -v
```

如果看到：

```text
origin  https://github.com/Vik-W001/em_doc.git
```

可以改成 SSH：

```bat
git remote set-url origin git@github.com:Vik-W001/em_doc.git
```

再看一下：

```bat
git remote -v
```

以后 push 就走 SSH 了。

---

## `--depth 1` 会不会影响 push？

一般不会影响你正常提交和 push。

比如你这样 clone：

```bat
git clone --depth 1 https://github.com/Vik-W001/em_doc.git
```

然后修改文件：

```bat
git add .
git commit -m "update docs"
git push
```

通常没问题。

但是 `--depth 1` 是浅克隆，只下载最近一次提交历史，所以它对这些操作有限制：

```text
看完整历史记录    不方便
切很老的分支      可能不行
回溯很早的 commit  不行
普通修改 push      可以
```

你的 `em_doc` 看起来像文档仓库，`--depth 1` 完全可以用。

---

## 最省事结论

你要是只想现在赶紧能 clone、能 push：

```bat
git clone --depth 1 https://github.com/Vik-W001/em_doc.git
cd em_doc
git add .
git commit -m "update"
git push
```

第一次 push 的时候，用 **GitHub Token** 当密码。

你要是想长期最方便：

```bat
用 SSH key + git@github.com:Vik-W001/em_doc.git
```

一句话：

```text
HTTPS 适合临时方便，但 push 要 Token；
SSH 适合长期开发，配好 key 后 push 最舒服。
```

[1]: https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/about-authentication-to-github?utm_source=chatgpt.com "About authentication to GitHub"
[2]: https://docs.github.com/en/get-started/git-basics/caching-your-github-credentials-in-git?utm_source=chatgpt.com "Caching your GitHub credentials in Git"
[3]: https://docs.github.com/en/authentication/troubleshooting-ssh/using-ssh-over-the-https-port?utm_source=chatgpt.com "Using SSH over the HTTPS port"

