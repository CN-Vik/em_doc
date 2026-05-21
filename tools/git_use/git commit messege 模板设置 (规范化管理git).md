# 配置方法
> 1. git config --global core.editor vim  （设置 Git 的默认编辑器为 Vim）
> 2. 在用户根目录下（~），创建一个.git_commit_msg文件，然后把下面的内容拷贝到文件中并保存。
> [version][模块][类型]{解决xxx问题}
[Bug ID]：{123345}
[风险描述]：{}
[测试建议]：{}
[提交依赖]：{}
**version 建议每次提交都加个版本号, 例如V1.2.3**
>3. git config --global commit.template   ~/.git_commit_msg (将 .git_commit_msg 文件设置为 Git 的提交模板)
>配置完成后可以查看gitconfig文件：
>4. 实际案例如下
>![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/a60add20f1ff4f25a3b8e8729f0b26e3.gif)
