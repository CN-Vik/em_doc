是的，从你提供的记录来看，每次通过 SSH 连接这台服务器时都需要输入密码。这是 SSH 默认的身份验证方式。

不过，你可以通过设置 **SSH 公钥认证** 来免去每次输入密码的麻烦。以下是设置步骤：

### 设置 SSH 公钥认证（免密码登录）

1. **在本地 Windows 电脑生成 SSH 密钥对**（如果还没有）：
   ```bash
   # 在 CMD 或 PowerShell 中运行
   ssh-keygen -t ed25519
   ```
   这会在 `C:\Users\lingshuo.wang\.ssh\` 目录下生成两个文件：
   - `id_ed25519`（私钥，必须保密）
   - `id_ed25519.pub`（公钥）

2. **将公钥上传到远程服务器**：
   ```bash
   # 将本地公钥复制到服务器
   scp C:\Users\lingshuo.wang\.ssh\id_ed25519.pub lingshuo-wang@10.10.34.75:~/
   ```

3. **在远程服务器上配置公钥**：
   ```bash
   # 登录服务器
   ssh lingshuo-wang@10.10.34.75
   
   # 创建 .ssh 目录（如果不存在）
   mkdir -p ~/.ssh
   chmod 700 ~/.ssh
   
   # 将公钥添加到授权密钥文件
   cat ~/id_ed25519.pub >> ~/.ssh/authorized_keys
   
   # 设置正确的权限
   chmod 600 ~/.ssh/authorized_keys
   
   # 删除临时文件
   rm ~/id_ed25519.pub
   ```

4. **测试免密码登录**：
   ```bash
   # 在本地重新连接
   ssh lingshuo-wang@10.10.34.75
   ```
   现在应该不需要输入密码就能直接登录了。

### 其他注意事项

1. **服务器端可能需要调整 SSH 配置**（如果需要的话，请联系管理员）：
   ```bash
   # 检查服务器 SSH 配置
   sudo grep -E "PubkeyAuthentication|PasswordAuthentication" /etc/ssh/sshd_config
   ```
   确保包含：
   ```
   PubkeyAuthentication yes
   ```

2. **如果私钥有密码**：
   - 你仍然需要在每次会话开始时输入一次私钥密码
   - 可以使用 `ssh-agent` 来管理私钥密码，实现一次解锁多次使用

3. **安全提示**：
   - 私钥文件（`id_ed25519`）相当于你的"数字身份证"，务必妥善保管
   - 不要在公用或不安全的电脑上设置免密码登录
   - 定期更换密钥对是个好习惯

设置完成后，你以后连接 `10.10.34.75` 就不再需要每次都输入密码了。