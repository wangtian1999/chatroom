# 聊天室项目 (Chat Room Project)

一个基于 C 语言和 Windows Socket 开发的客户端-服务器聊天程序，支持多用户实时聊天、私聊、聊天记录管理等功能。

## 项目特性

### 🌟 核心功能
- **多用户支持**：最多支持 10 个用户同时在线
- **实时聊天**：支持公聊和私聊消息
- **用户管理**：用户注册、昵称管理、在线状态显示
- **聊天记录**：本地聊天记录存储、查看和导出
- **消息类型**：区分公聊、私聊和系统消息

### 📋 已实现功能

#### 服务器端 (Server)
- ✅ 网络连接管理（绑定IP和端口）
- ✅ 多客户端连接处理
- ✅ 用户注册和昵称管理
- ✅ 在线用户列表维护
- ✅ 消息解析和转发
- ✅ 公聊消息群发
- ✅ 私聊消息一对一转发
- ✅ 用户加入/退出通知

#### 客户端 (Client)
- ✅ 服务器连接功能
- ✅ 用户昵称注册
- ✅ 实时消息收发
- ✅ 公聊和私聊支持
- ✅ 聊天记录本地存储
- ✅ 历史记录分页查看
- ✅ 聊天记录导出功能
- ✅ 多线程消息接收

## 系统要求

- **操作系统**：Windows 7/8/10/11 (支持 Winsock2)
- **开发环境**：Visual Studio 2015 (推荐) 或 Visual Studio 2017/2019/2022
- **Windows SDK**：Windows 10 SDK 或更高版本
- **运行库**：Visual C++ 2015 Redistributable
- **网络**：TCP/IP 支持
- **内存**：最小 8MB 可用内存

## 安装和编译

### 1. 克隆项目
```bash
git clone <repository-url>
cd chat-room-project
```

### 2. 编译项目

#### 使用 Visual Studio 2015 IDE (推荐)
1. 启动 Visual Studio 2015
2. 打开 `Server/Server.sln` 解决方案文件
3. 确认目标平台为 x86 或 x64
4. 选择 Debug 或 Release 配置
5. 右键解决方案 → 生成解决方案 (或按 Ctrl+Shift+B)
6. 检查输出窗口确认编译成功

#### 使用 Visual Studio 2015 命令行
```cmd
# 打开 VS2015 开发者命令提示符
# 开始菜单 → Visual Studio 2015 → VS2015 开发者命令提示符

# 或者手动设置环境
"C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"

# 编译项目
cd /d "项目路径"
msbuild Server\Server.sln /p:Configuration=Debug /p:Platform=x86
```

#### 编译注意事项
- 确保已安装 Windows SDK
- 如遇到 "无法找到 Windows.h" 错误，请安装/修复 Windows SDK
- 编译前确保关闭杀毒软件实时保护（可能误报）

### 3. 运行程序

#### 文件位置
编译成功后，可执行文件位于：
- 服务器：`Server/Debug/Server.exe`
- 客户端：`Server/Debug/Client.exe`

#### 运行步骤
1. **首先启动服务器**
   - 双击运行 `Server.exe` 或在命令行中执行
   - 确认服务器显示 "Server started on 127.0.0.1:8888"
   
2. **然后启动客户端**
   - 可同时运行多个 `Client.exe` 实例测试多用户功能
   - 每个客户端需要输入不同的昵称

#### 防火墙设置
- Windows 可能弹出防火墙提示，请选择 "允许访问"
- 如无法连接，请检查 Windows 防火墙设置

## 使用说明

### 启动服务器
1. 运行 `Server.exe`
2. 服务器将在 `127.0.0.1:8888` 上监听连接
3. 显示服务器状态和连接信息

### 启动客户端
1. 运行 `Client.exe`
2. 输入用户昵称进行注册
3. 连接成功后即可开始聊天

### 聊天命令

#### 基本聊天
- 直接输入消息发送公聊
- 使用 `@用户名 消息内容` 发送私聊

#### 聊天记录管理
- `/history` - 查看第一页聊天记录
- `/history <页码>` - 查看指定页的聊天记录
- `/next` - 下一页
- `/prev` - 上一页
- `/export` - 导出聊天记录到文件

#### 其他命令
- `/help` - 显示帮助信息
- `/quit` - 退出程序

## 项目结构

```
project/
├── Client/                 # 客户端项目
│   ├── client.c           # 客户端源代码
│   ├── Client.vcxproj     # 项目文件
│   └── Debug/             # 编译输出目录
├── Server/                 # 服务器项目
│   ├── server.c           # 服务器源代码
│   ├── Server.vcxproj     # 项目文件
│   ├── Server.sln         # 解决方案文件
│   └── Debug/             # 编译输出目录
├── develop.md             # 开发文档
└── README.md              # 项目说明文档
```

## 技术实现

### 网络通信
- 使用 Windows Socket API (Winsock2)
- TCP 协议保证消息可靠传输
- 多线程处理并发连接

### 消息协议
```c
// 消息类型
#define MSG_REGISTER 1    // 用户注册
#define MSG_CHAT 2        // 公聊消息
#define MSG_PRIVATE 3     // 私聊消息
#define MSG_SYSTEM 4      // 系统消息
#define MSG_USER_LIST 5   // 用户列表
```

### 数据结构
```c
// 用户信息
typedef struct {
    SOCKET socket;
    char nickname[NICKNAME_SIZE];
    char ip_address[INET_ADDRSTRLEN];
    int port;
    time_t join_time;
    int is_active;
} UserInfo;

// 聊天记录
typedef struct {
    char timestamp[32];
    char message_type[16];
    char sender[NICKNAME_SIZE];
    char receiver[NICKNAME_SIZE];
    char content[BUFFER_SIZE];
} ChatRecord;
```

## 功能特色

### 聊天记录管理
- **自动保存**：所有聊天消息自动保存到本地
- **分页显示**：每页显示 20 条记录，支持翻页
- **时间戳**：每条消息包含详细的时间信息
- **消息分类**：区分公聊、私聊和系统消息
- **导出功能**：支持将聊天记录导出为文本文件

### 用户体验
- **实时通信**：消息即时收发，无明显延迟
- **状态提示**：清晰的连接状态和操作反馈
- **错误处理**：完善的错误提示和异常处理
- **命令帮助**：内置帮助系统，方便用户使用

## 开发进度

- [x] 第一阶段：基础网络连接
- [x] 第二阶段：用户管理
- [x] 第三阶段：消息处理
- [x] 第四阶段：聊天记录管理
- [ ] 第五阶段：界面优化

## 注意事项

1. **防火墙设置**：确保防火墙允许程序网络访问
2. **端口占用**：默认使用 8888 端口，确保端口未被占用
3. **编码问题**：建议使用英文昵称避免编码问题
4. **内存管理**：聊天记录最多保存 1000 条，超出后循环覆盖

## 故障排除

### Visual Studio 2015 相关问题

**Q: 编译时提示 "无法找到 Windows.h"**
```
A: 安装/修复 Windows SDK
1. 打开 Visual Studio 2015 安装程序
2. 选择 "修改" → "Windows 和 Web 开发" → "Windows 10 SDK"
3. 或下载独立的 Windows 10 SDK 安装
```

**Q: 编译时提示 "error MSB8020: 无法找到 v140 的生成工具"**
```
A: 安装 Visual C++ 2015 生成工具
1. 下载 "Microsoft Visual C++ Build Tools 2015"
2. 或安装完整的 Visual Studio 2015
```

**Q: 运行时提示 "缺少 MSVCR140.dll"**
```
A: 安装 Visual C++ 2015 Redistributable
1. 下载 "Microsoft Visual C++ 2015 Redistributable (x86/x64)"
2. 安装对应版本的运行库
```

### 网络连接问题

**Q: 连接服务器失败**
```
A: 按以下步骤检查：
1. 确认服务器已启动并显示监听状态
2. 检查 Windows 防火墙设置
3. 确认端口 8888 未被其他程序占用
4. 尝试关闭杀毒软件的网络保护
```

**Q: 消息发送失败**
```
A: 检查项目：
1. 网络连接是否正常
2. 服务器是否仍在运行
3. 客户端是否正确连接
4. 重启客户端重新连接
```

### 功能使用问题

**Q: 聊天记录导出失败**
```
A: 检查以下项目：
1. 程序是否有文件写入权限
2. 磁盘空间是否充足
3. 目标目录是否存在
4. 以管理员身份运行程序
```

**Q: 中文显示乱码**
```
A: 编码问题解决：
1. 建议使用英文昵称和消息
2. 确保控制台编码设置正确
3. 在 VS2015 中设置项目字符集为 Unicode
```

## 贡献指南

欢迎提交 Issue 和 Pull Request 来改进项目：

1. Fork 项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议，请通过以下方式联系：
- 提交 GitHub Issue
- 发送邮件至项目维护者

---

**项目状态**: 开发完成 ✅  
**最后更新**: 2025年6月  
**版本**: v1.0.0