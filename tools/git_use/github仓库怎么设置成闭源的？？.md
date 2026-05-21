可以，GitHub 仓库可以从 **Public（开源/公开）** 改成 **Private（闭源/私有）**。操作不会影响你本地的一堆提交，只是改 GitHub 上这个仓库的可见性。

## 网页端操作

进你的仓库，比如：

`Vik-W001/em_doc`

然后按这个路径：

**仓库页面 → Settings → General → 拉到最下面 Danger Zone → Change repository visibility → Change to private**

然后 GitHub 会让你确认仓库名，确认后就变成私有仓库了。GitHub 官方文档里也是通过仓库设置里的 visibility 来改公开/私有状态。([GitHub Docs][1])

## 命令行方式

如果你装了 GitHub CLI，也可以这样：

```bash
gh repo edit Vik-W001/em_doc --visibility private
```

有时需要加确认参数：

```bash
gh repo edit Vik-W001/em_doc --visibility private --accept-visibility-change-consequences
```

## 需要注意的坑

改成私有后，别人默认就看不到你的代码了；但如果以前已经是公开仓库，别人之前 clone 过、fork 过、下载过，那 GitHub 没法帮你“收回”。另外，GitHub 官方说明：公开仓库改私有时，原来的 public forks 不会自动变私有，而是会被分离出去。([GitHub Docs][1])

如果你开过 GitHub Pages，仓库改私有后，公开的 Pages 站点可能会被取消发布。([GitHub Docs][1])

GitHub Free 个人账号也支持私有仓库，不过私有仓库某些高级功能会受套餐限制。([GitHub Docs][2])

## 你现在这种情况

你前面这个仓库是：

```bash
https://github.com/Vik-W001/em_doc.git
```

所以你直接去：

**GitHub → Vik-W001/em_doc → Settings → Danger Zone → Change visibility → Private**

改完后，你本地仓库不用重新拉，继续正常：

```bash
git push
```

只是你 HTTPS 推送还需要解决 token 认证问题；仓库私有之后，更需要用 **Personal Access Token** 或者改用 **SSH** 推送。

[1]: https://docs.github.com/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility?utm_source=chatgpt.com "Setting repository visibility"
[2]: https://docs.github.com/en/repositories/creating-and-managing-repositories/about-repositories?utm_source=chatgpt.com "About repositories"
