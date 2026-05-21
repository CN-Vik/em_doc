# 拉代码的方式：git clone https
>C:\Users\ufo28\Desktop>git clone https://github.com/Vik-W001/em_doc.git
Cloning into 'em_doc'...
remote: Enumerating objects: 149, done.
remote: Counting objects: 100% (149/149), done.
remote: Compressing objects: 100% (98/98), done.
remote: Total 149 (delta 25), reused 129 (delta 9), pack-reused 0 (from 0)
Receiving objects: 100% (149/149), 30.33 MiB | 4.19 MiB/s, done.
Resolving deltas: 100% (25/25), done.


-----
----
---
不用重新拉！你说得对，**本地一大堆提交，千万别删目录、别重新 clone**。

你现在只需要做一件事：**保留本地仓库，把远程地址从 HTTPS 改成 SSH，然后 push。**

---

## 先确认：本地提交都还在

在你的仓库目录执行：

```powershell
cd C:\Users\ufo28\Desktop\em_doc

git status
git log --oneline --decorate --graph -10
```

只要 `git log` 能看到你那些提交，就说明本地提交都在。

---

## 核心操作：只改远程地址，不动你的代码和提交

你现在是 HTTPS：

```text
https://github.com/Vik-W001/em_doc.git
```

改成 SSH：

```powershell
git remote set-url origin git@github.com:Vik-W001/em_doc.git
```

这个命令**不会改你的代码、不会删提交、不会重新拉代码**。

它只是改 `.git/config` 里的远程仓库地址。

然后看一下：

```powershell
git remote -v
```

应该变成：

```text
origin  git@github.com:Vik-W001/em_doc.git (fetch)
origin  git@github.com:Vik-W001/em_doc.git (push)
```

然后直接推：

```powershell
git push -u origin HEAD
```

---

## 你现在完整执行这一套

```powershell
cd C:\Users\ufo28\Desktop\em_doc

git status
git log --oneline --decorate --graph -10

git remote -v
git remote set-url origin git@github.com:Vik-W001/em_doc.git
git remote -v

git push -u origin HEAD
```

---

## 如果 push 报这个

```text
Permission denied (publickey)
```

说明你本机 SSH key 没配好，不是提交问题。

执行：

```powershell
ssh -T git@github.com
```

如果失败，再生成 SSH key：

```powershell
ssh-keygen -t ed25519 -C "ufo20240502@gmail.com"
```

一路回车。

然后查看公钥：

```powershell
Get-Content $env:USERPROFILE\.ssh\id_ed25519.pub
```

把这一整行复制到 GitHub：

```text
GitHub 头像 -> Settings -> SSH and GPG keys -> New SSH key
```

添加后再测试：

```powershell
ssh -T git@github.com
```

成功后回到项目：

```powershell
cd C:\Users\ufo28\Desktop\em_doc
git push -u origin HEAD
```

---

## 为了防止误操作，先打个本地备份分支

你本地提交多的话，建议先执行：

```powershell
git branch backup-local-commits
```

这个会在当前提交位置创建一个备份分支。就算后面你乱切分支，提交也不会丢。

也可以再看所有分支：

```powershell
git branch
```

---

## 重点记住

你现在不要执行这些危险操作：

```powershell
# 不要删目录
rm -rf em_doc

# 不要重新 clone 覆盖
git clone ...

# 不要乱 reset
git reset --hard origin/main
```

你要做的只是：

```powershell
git remote set-url origin git@github.com:Vik-W001/em_doc.git
git push -u origin HEAD
```

你的本地提交不会因为改 HTTPS/SSH 地址而丢。
