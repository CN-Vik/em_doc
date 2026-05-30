你这个不是单纯“GitHub 慢”，而是 **命令行没有稳定走 Clash** + **SSH 推送中途被断开**。

先按这个顺序搞。

## 1. 先修 Git 分支 upstream

你现在分支叫 `esp32_foc`，第一次推这个分支要带 `-u`：

```powershell
git push -u origin esp32_foc
```

以后再推就可以直接：

```powershell
git push
```

但你后面的 `Connection reset by peer` 才是主要问题。

---

## 2. 先确认你 GitHub remote 用的是 SSH 还是 HTTPS

在项目目录执行：

```powershell
git remote -v
```

如果看到类似：

```text
git@github.com:xxx/xxx.git
```

这就是 **SSH**。注意：`git config --global http.proxy ...` 对 SSH 基本没用。

如果看到：

```text
https://github.com/xxx/xxx.git
```

这就是 **HTTPS**，可以直接用 Git 的 http/https 代理配置。Git 官方配置里确实有 `http.proxy` 这种配置项。([Git][1])

---

## 3. 先查 Clash 端口

PowerShell 执行：

```powershell
netstat -ano | findstr LISTENING | findstr 789
```

你之前 WSL 里看到过 `127.0.0.1:7897`，所以大概率 Clash Verge 的 mixed port 是 `7897`。

测试代理通不通：

```powershell
curl.exe -x http://127.0.0.1:7897 https://github.com -I
```

能返回 `HTTP/1.1 200`、`301`、`302` 之类就说明这个端口能用。

---

## 4. 如果 remote 是 HTTPS，用这个方案

```powershell
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897

git config --global --get http.proxy
git config --global --get https.proxy
```

然后推：

```powershell
git push -u origin esp32_foc
```

如果你想只让 GitHub 走代理，而不是所有 Git 都走代理，可以这样：

```powershell
git config --global http.https://github.com.proxy http://127.0.0.1:7897
```

---

## 5. 如果 remote 是 SSH，推荐先改成 GitHub 的 443 SSH

GitHub 官方支持通过 `ssh.github.com:443` 走 SSH，常用于普通 22 端口被拦或不稳定的环境。([GitHub Docs][2])

先测试：

```powershell
ssh -T -p 443 git@ssh.github.com
```

如果能看到类似：

```text
Hi xxx! You've successfully authenticated, but GitHub does not provide shell access.
```

就说明 443 SSH 可用。

然后编辑 SSH 配置：

```powershell
notepad $env:USERPROFILE\.ssh\config
```

写入：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

如果你有指定密钥，比如 `id_ed25519`，可以加：

```sshconfig
    IdentityFile C:\Users\ufo28\.ssh\id_ed25519
    IdentitiesOnly yes
```

然后测试：

```powershell
ssh -T git@github.com
git push -u origin esp32_foc
```

---

## 6. 如果 SSH 443 还是断，就强制 SSH 也走 Clash

先找 `connect.exe`：

```powershell
Get-ChildItem "C:\Program Files\Git" -Recurse -Filter connect.exe -ErrorAction SilentlyContinue | Select-Object -First 5 FullName
```

一般可能在：

```text
C:\Program Files\Git\mingw64\bin\connect.exe
```

然后再次编辑：

```powershell
notepad $env:USERPROFILE\.ssh\config
```

改成这样：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ProxyCommand "C:/Program Files/Git/mingw64/bin/connect.exe" -H 127.0.0.1:7897 %h %p
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

测试：

```powershell
ssh -T git@github.com
git push -u origin esp32_foc
```

这个方案的意思是：**Git SSH -> connect.exe -> Clash 本地代理 -> GitHub SSH 443**。

---

## 7. ESP-IDF 下载失败也要单独设置代理

你这个：

```text
Download attempt failed ...
https://dl.espressif.cn/github_assets/...
error decoding response body
```

一般是 zip 下载中途坏了。ESP-IDF 的工具下载是 `idf_tools.py / install.ps1` 在干，官方文档也说明这些脚本会下载并安装对应版本工具链。([Espressif Systems][3])

PowerShell 当前窗口先设置：

```powershell
$env:HTTP_PROXY="http://127.0.0.1:7897"
$env:HTTPS_PROXY="http://127.0.0.1:7897"
$env:http_proxy="http://127.0.0.1:7897"
$env:https_proxy="http://127.0.0.1:7897"
```

再删除坏掉的半截包：

```powershell
Remove-Item "$env:USERPROFILE\.espressif\dist\riscv32-esp-elf-13.2.0_20250707-x86_64-w64-mingw32.zip" -ErrorAction SilentlyContinue
```

然后重新安装工具。比如你的 ESP-IDF 在这里：

```powershell
cd C:\Espressif\frameworks\esp-idf-v5.5.3
```

如果你只做普通 ESP32：

```powershell
.\install.ps1 esp32
```

如果你做 ESP32-S3：

```powershell
.\install.ps1 esp32s3
```

官方工具支持只安装特定芯片 target 的工具，不一定要所有工具都下。([Espressif Systems][3])

如果你要永久让 VS Code / ESP-IDF 插件也走代理，用：

```powershell
setx HTTP_PROXY "http://127.0.0.1:7897"
setx HTTPS_PROXY "http://127.0.0.1:7897"
```

然后 **重启 VS Code**。

---

## 8. ESP-IDF 镜像源也可以切一下

你现在用的是：

```text
dl.espressif.cn/github_assets
```

如果这个源不稳定，可以临时切成国际源：

```powershell
$env:IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
```

然后重新执行：

```powershell
.\install.ps1 esp32
```

Espressif 文档里说明，`IDF_GITHUB_ASSETS` 可以把 GitHub release 资源下载地址改写到 Espressif 下载服务器。([Espressif Systems][4])

如果你在国内网络环境，反过来也可以用：

```powershell
$env:IDF_GITHUB_ASSETS="dl.espressif.cn/github_assets"
```

---

## 我建议你现在直接这样做

你当前在 PowerShell，先执行：

```powershell
netstat -ano | findstr LISTENING | findstr 789
git remote -v
```

如果 remote 是 `git@github.com:...`，优先用 SSH 443：

```powershell
notepad $env:USERPROFILE\.ssh\config
```

写入：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

然后：

```powershell
ssh -T git@github.com
git push -u origin esp32_foc
```

如果还是 `Connection reset by peer`，再加 `ProxyCommand` 强制走 Clash。这个比瞎改 `postBuffer` 靠谱。

[1]: https://git-scm.com/docs/git-config?utm_source=chatgpt.com "Git - git-config Documentation"
[2]: https://docs.github.com/en/authentication/troubleshooting-ssh/using-ssh-over-the-https-port?utm_source=chatgpt.com "Using SSH over the HTTPS port"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-tools.html?utm_source=chatgpt.com "Downloadable IDF Tools - ESP32"
[4]: https://docs.espressif.com/projects/esp-idf/en/v4.4.5/esp32/api-guides/tools/idf-tools.html?utm_source=chatgpt.com "Downloadable Tools - ESP32 - — ESP-IDF Programming ..."

-----
-----
-----
看你这个日志，结论很明确：

1. **Clash 代理端口是通的**：`127.0.0.1:7897` 可以访问 GitHub。
2. **你的 Git remote 是 SSH**：`git@github.com:Vik-W001/vik_diy_bldc_driver.git`。
3. **GitHub SSH 443 也认证成功了**：`Hi Vik-W001! You've successfully authenticated...`。
4. 所以现在要做的不是继续折腾 Clash，而是让 `git@github.com` 默认走 `ssh.github.com:443`。你日志里的认证成功和远程地址都能说明这一点。

---

## 你现在直接这样做

### 第一步：编辑 SSH 配置

PowerShell 里执行：

```powershell
notepad $env:USERPROFILE\.ssh\config
```

如果文件不存在，Notepad 会问你是否创建，点“是”。

然后写入这个：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

保存退出。

---

## 第二步：测试是否生效

执行：

```powershell
ssh -T git@github.com
```

注意，这次不要写：

```powershell
ssh -T -p 443 git@ssh.github.com
```

而是必须测试：

```powershell
ssh -T git@github.com
```

如果出现：

```text
Hi Vik-W001! You've successfully authenticated, but GitHub does not provide shell access.
```

就说明配置生效了。

你后面的：

```text
channel_by_id: 0: bad id: channel free
Received window adjust for non-open channel 0.
```

这个先不用管，前面 `Hi Vik-W001!` 已经说明认证成功了。

---

## 第三步：重新 push

在你的项目目录执行：

```powershell
git push -u origin esp32_foc
```

或者：

```powershell
git push --set-upstream origin esp32_foc
```

---

## 第四步：确认是否真的推上去了

执行：

```powershell
git branch -vv
```

如果看到类似：

```text
* esp32_foc xxxxxxx [origin/esp32_foc] ...
```

说明本地分支已经跟远程 `origin/esp32_foc` 绑定成功。

也可以查远程有没有这个分支：

```powershell
git ls-remote --heads origin esp32_foc
```

如果能看到一串 commit hash，说明远程分支已经存在。

---

## 如果还是中途断开，再加 Clash 强制代理

如果配置了 443 还是报：

```text
Connection reset by peer
send-pack: unexpected disconnect
```

那就让 SSH 也强制走 Clash。

先找 `connect.exe`：

```powershell
Get-ChildItem "C:\Program Files\Git" -Recurse -Filter connect.exe -ErrorAction SilentlyContinue | Select-Object -First 5 FullName
```

大概率会找到：

```text
C:\Program Files\Git\mingw64\bin\connect.exe
```

然后重新编辑：

```powershell
notepad $env:USERPROFILE\.ssh\config
```

改成：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ProxyCommand "C:/Program Files/Git/mingw64/bin/connect.exe" -H 127.0.0.1:7897 %h %p
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

保存后测试：

```powershell
ssh -T git@github.com
git push -u origin esp32_foc
```

---

## 你这个 ESP-IDF 下载失败也要处理

你日志里的这个：

```text
error decoding response body
```

说明 zip 包下载到一半坏了。把坏包删掉，再让 PowerShell 当前窗口走 Clash：

```powershell
$env:HTTP_PROXY="http://127.0.0.1:7897"
$env:HTTPS_PROXY="http://127.0.0.1:7897"
$env:http_proxy="http://127.0.0.1:7897"
$env:https_proxy="http://127.0.0.1:7897"
```

删除坏包：

```powershell
Remove-Item "$env:USERPROFILE\.espressif\dist\riscv32-esp-elf-13.2.0_20250707-x86_64-w64-mingw32.zip" -ErrorAction SilentlyContinue
```

然后重新跑 ESP-IDF 安装。

---

## 最推荐你现在执行的最小命令顺序

```powershell
notepad $env:USERPROFILE\.ssh\config
```

写入：

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

然后：

```powershell
ssh -T git@github.com
git push -u origin esp32_foc
git branch -vv
```

如果这一步成功，以后你这个仓库直接：

```powershell
git push
git pull
```

就行了。

-----
-----
----
可以，**这两个可以同时设置**，不冲突，因为它们管的是两套东西：

| 配置                                               | 作用对象                                                |
| ------------------------------------------------ | --------------------------------------------------- |
| `~/.ssh/config` 里的 `ProxyCommand`                | 管 `git@github.com:xxx/xxx.git` 这种 **SSH Git**       |
| `git config --global http.proxy` / `https.proxy` | 管 `https://github.com/xxx/xxx.git` 这种 **HTTPS Git** |

你现在仓库 remote 是：

```text
git@github.com:Vik-W001/vik_diy_bldc_driver.git
```

所以你当前这个项目主要走的是 **SSH 配置**，不是 Git 的 `http.proxy`。

---

## 推荐你这样设置

### 1. SSH 走 Clash，保留这个

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ProxyCommand "C:/Program Files/Git/mingw64/bin/connect.exe" -H 127.0.0.1:7897 %h %p
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

这个是专门解决：

```text
git@github.com:xxx/xxx.git
```

这种 SSH 推拉慢、断线的问题。

---

### 2. Git HTTPS 代理可以设置，但建议只针对 GitHub

不要一上来全局所有网站都代理，容易影响你拉公司内网 Git / Gitea。

建议这样设置：

```powershell
git config --global http.https://github.com.proxy http://127.0.0.1:7897
```

查看：

```powershell
git config --global --get http.https://github.com.proxy
```

这样只有访问 `https://github.com` 的时候才走代理。

---

## 不太推荐你直接这样全局设置

```powershell
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897
```

不是不能用，而是它会影响所有 HTTPS Git 仓库，比如：

```text
https://github.com/...
https://gitee.com/...
https://公司内网git/...
```

如果你以后访问公司内网 Git，可能反而出问题。

---

## 如果你已经设置了全局代理，想取消

```powershell
git config --global --unset http.proxy
git config --global --unset https.proxy
```

然后只保留 GitHub 专用代理：

```powershell
git config --global http.https://github.com.proxy http://127.0.0.1:7897
```

---

## 你现在最推荐的最终配置

### SSH 配置

```sshconfig
Host github.com
    HostName ssh.github.com
    User git
    Port 443
    ProxyCommand "C:/Program Files/Git/mingw64/bin/connect.exe" -H 127.0.0.1:7897 %h %p
    ServerAliveInterval 30
    ServerAliveCountMax 3
```

### Git HTTPS 配置

```powershell
git config --global http.https://github.com.proxy http://127.0.0.1:7897
```

### 测试

```powershell
ssh -T git@github.com
git push -u origin esp32_foc
```

---

## ESP-IDF 下载还要单独设置

Git 的 `http.proxy` **不一定管 ESP-IDF 下载工具**。ESP-IDF 下载工具最好单独设置环境变量：

```powershell
$env:HTTP_PROXY="http://127.0.0.1:7897"
$env:HTTPS_PROXY="http://127.0.0.1:7897"
$env:http_proxy="http://127.0.0.1:7897"
$env:https_proxy="http://127.0.0.1:7897"
```

所以总结就是：

**GitHub SSH 推拉：靠 `~/.ssh/config`。**
**GitHub HTTPS 仓库：靠 `git config http.https://github.com.proxy`。**
**ESP-IDF 工具下载：靠 PowerShell 环境变量 `HTTP_PROXY/HTTPS_PROXY`。**
