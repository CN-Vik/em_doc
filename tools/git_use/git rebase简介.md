
`git rebase` 核心作用：**把你当前分支上的提交，重新“搬到”另一个分支的最新提交后面。**

通俗说就是：
你本地写了几个 commit，但主分支 `master/main/dev` 已经被别人更新了。你想让自己的代码基于最新主分支继续开发，就可以用 `rebase`。

---

## 1. 先看普通开发场景

假设公司远程分支是：

```text
origin/dev:

A --- B --- C
```

你从 `B` 拉出来开发了两个提交：

```text
你的 feature 分支:

A --- B --- D --- E
```

但是别人已经把 `C` 合进了 `dev`：

```text
origin/dev:

A --- B --- C
```

现在整体是这样：

```text
        D --- E        你的 feature
       /
A --- B --- C          origin/dev
```

你执行：

```bash
git checkout feature
git fetch origin
git rebase origin/dev
```

rebase 之后变成：

```text
A --- B --- C --- D' --- E'
```

注意：`D`、`E` 会变成 `D'`、`E'`，因为它们被“重新应用”了一遍，commit id 会变。

---

## 2. `rebase` 和 `merge` 的区别

### 用 `merge`

```bash
git checkout feature
git merge origin/dev
```

结果可能是：

```text
        D --- E -------- M
       /                /
A --- B --- C ----------
```

会多一个合并提交 `M`。

### 用 `rebase`

```bash
git checkout feature
git rebase origin/dev
```

结果是：

```text
A --- B --- C --- D' --- E'
```

历史更直，看起来像你是基于最新 `dev` 开发的。

所以很多公司喜欢：

```bash
git pull --rebase
```

而不是普通：

```bash
git pull
```

因为普通 `pull` 默认可能会产生 merge commit。

---

## 3. 最常用命令

### 场景一：更新自己的开发分支

你在 `feature_xxx` 分支开发，想同步最新 `dev`：

```bash
git checkout feature_xxx
git fetch origin
git rebase origin/dev
```

这就是最常见用法。

---

### 场景二：拉代码时直接 rebase

普通拉取：

```bash
git pull
```

可能会自动 merge。

推荐写法：

```bash
git pull --rebase
```

等价于：

```bash
git fetch
git rebase origin/当前分支
```

例如你在 `dev` 分支：

```bash
git pull --rebase origin dev
```

---

## 4. rebase 遇到冲突怎么办？

执行：

```bash
git rebase origin/dev
```

如果冲突了，Git 会提示类似：

```text
CONFLICT (content): Merge conflict in main.c
```

这时候流程是：

### 第一步：看冲突文件

```bash
git status
```

你会看到哪些文件冲突了。

### 第二步：打开冲突文件

里面可能是这样：

```c
<<<<<<< HEAD
int speed = 100;
=======
int speed = 200;
>>>>>>> D
```

含义：

```text
<<<<<<< HEAD
这是目标分支 origin/dev 上的内容
=======
这是你自己提交里的内容
>>>>>>> 你的提交
```

你手动改成最终想要的，比如：

```c
int speed = 200;
```

然后删除 `<<<<<<<`、`=======`、`>>>>>>>` 这些标记。

### 第三步：标记冲突已解决

```bash
git add main.c
```

### 第四步：继续 rebase

```bash
git rebase --continue
```

如果还有冲突，继续重复：

```bash
改文件
git add 文件
git rebase --continue
```

---

## 5. 想放弃 rebase 怎么办？

如果 rebase 到一半你懵了，不想继续了：

```bash
git rebase --abort
```

它会回到 rebase 之前的状态。

这个命令很重要，记住：

```bash
git rebase --abort
```

---

## 6. interactive rebase：整理 commit 用的

这个也很常用，尤其是 Gerrit / Code Review 前。

比如你最近提交了 3 个 commit：

```bash
git log --oneline
```

看到：

```text
a3c1111 fix typo
b2d2222 add uart driver
c1e3333 debug print
```

你想整理最近 3 个提交：

```bash
git rebase -i HEAD~3
```

会打开一个编辑界面：

```text
pick c1e3333 debug print
pick b2d2222 add uart driver
pick a3c1111 fix typo
```

常用操作：

```text
pick    保留这个 commit
reword  修改 commit message
squash  合并到上一个 commit，并合并 message
fixup   合并到上一个 commit，但丢掉当前 message
drop    删除这个 commit
```

比如你想把 `fix typo` 合并到 `add uart driver`：

```text
pick c1e3333 debug print
pick b2d2222 add uart driver
fixup a3c1111 fix typo
```

保存退出后，Git 会帮你整理提交。

---

## 7. 嵌入式项目里最常用的 rebase 流程

假设你在公司开发 `cat_litter_motor_ctrl` 分支，主分支是 `dev`：

```bash
# 1. 切到自己的开发分支
git checkout cat_litter_motor_ctrl

# 2. 拉取远程最新代码
git fetch origin

# 3. 把自己的提交搬到最新 dev 后面
git rebase origin/dev

# 4. 如果有冲突，解决冲突
git status
# 手动改冲突文件
git add xxx.c
git rebase --continue

# 5. rebase 完后看提交历史
git log --oneline --graph --decorate --all
```

如果你之前已经推送过这个分支，rebase 后 commit id 变了，普通 push 会失败，需要：

```bash
git push --force-with-lease
```

不要随便用：

```bash
git push -f
```

更推荐：

```bash
git push --force-with-lease
```

它比 `-f` 安全一些，防止你把别人新推的代码覆盖掉。

---

## 8. 最重要的禁忌

**不要随便 rebase 公共分支。**

比如这些分支不要乱 rebase：

```bash
master
main
dev
release
```

尤其是多人都在用的远程分支。

适合 rebase 的是你自己的开发分支：

```bash
feature_xxx
bugfix_xxx
my_uart_debug
```

一句话：

```text
自己的分支可以 rebase；
大家共用的分支不要乱 rebase。
```

---

## 9. 你实际最常用记这几个就够了

```bash
# 更新远程信息
git fetch origin

# 把当前分支变基到最新 dev
git rebase origin/dev

# 解决冲突后继续
git add .
git rebase --continue

# 放弃 rebase
git rebase --abort

# 拉代码时使用 rebase
git pull --rebase

# 整理最近 3 个提交
git rebase -i HEAD~3
```

---

## 10. 跟 Gerrit 的关系

你之前用过 Gerrit / repo，这里 rebase 很常见。

比如你提交 review 前，远程 `tws-dev` 更新了，你本地 patch 落后了，通常流程是：

```bash
git fetch origin
git rebase origin/tws-dev
git push origin HEAD:refs/for/tws-dev
```

如果 Gerrit 提示冲突，基本就是让你先在本地 rebase 到最新目标分支，再重新推 review。

---

可以把 `rebase` 理解成一句话：

```text
我写的代码不变，但我想假装自己是在最新代码基础上写的。
```

所以它会把你的 commit 一个一个摘下来，然后重新放到最新分支后面。
