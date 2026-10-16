# 编译说明

## 项目结构要求

确保 `SConscript` 文件包含所有新的源文件。

### 更新 applications/SConscript

如果项目中有 `applications/SConscript` 文件，请添加以下内容：

```python
Import('RTT_ROOT')
Import('rtconfig')
from building import *

cwd = GetCurrentDir()

# 获取所有源文件
src = Split('''
    main.c
    can_receiver.c
    mqtt_client.c
    sd_storage.c
''')

# 获取头文件路径
CPPPATH = [cwd]

# 创建目标对象
group = DefineGroup('Applications', src, depend = [''], CPPPATH = CPPPATH)

Return('group')
```

### 更新顶层 SConscript

在项目根目录的 `SConscript` 中，确保包含了 `applications` 目录：

```python
SConscript('applications/SConscript')
```

## 编译步骤

### 方法1：使用RT-Thread Studio IDE

1. 右键点击项目 → **Build Project**
2. 等待编译完成
3. 查看 Build Console 中的编译结果

### 方法2：使用命令行

```bash
cd d:\RT-ThreadStudio\workspace\STM32F407ZGT6_Finalversion

# 完整编译
scons

# 并行编译（加速）
scons -j4

# 清理构建输出
scons --clean

# 重新编译
scons -f
```

## IntelliSense 配置（解决IDE提示错误）

### 在 VS Code 中：

1. 打开 `.vscode/c_cpp_properties.json`
2. 添加 include 路径：

```json
{
    "configurations": [
        {
            "name": "STM32",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/rt-thread/include",
                "${workspaceFolder}/drivers/include",
                "${workspaceFolder}/applications"
            ],
            "defines": [
                "RT_USING_ARM",
                "STM32F407xx"
            ]
        }
    ]
}
```

### 在 RT-Thread Studio 中：

1. **Project** → **Properties**
2. **C/C++** → **Paths and Symbols**
3. **Includes** 选项卡中添加路径：
   - `${workspace_loc:/STM32F407ZGT6_Finalversion/rt-thread/include}`
   - `${workspace_loc:/STM32F407ZGT6_Finalversion/drivers/include}`
   - `${workspace_loc:/STM32F407ZGT6_Finalversion/applications}`

## 常见编译错误及解决方案

### 错误1：找不到 rtthread.h

**原因：** rt-thread 路径配置错误

**解决方案：**
1. 检查 rt-thread 文件是否在项目中
2. 更新 C++ 路径配置
3. 重新打开项目

### 错误2：undefined reference to `can_receiver_init`

**原因：** can_receiver.c 未被编译

**解决方案：**
1. 检查 SConscript 中是否包含了 `can_receiver.c`
2. 执行 `scons --clean` 后重新编译
3. 检查文件是否存在

### 错误3：多个定义的 main 函数

**原因：** 可能存在其他 main.c 文件

**解决方案：**
1. 搜索项目中所有 main.c 文件
2. 删除或重命名额外的 main 函数
3. 确保只有 applications/main.c 中定义 main()

## 烧写程序

编译成功后会生成 `.elf` 和 `.bin` 文件。

### 使用 JLink 烧写：

```bash
# 通过 JLink Commander
JLinkExe -device STM32F407ZG -if SWD -speed 4000 -CommandFile script.jlink

# script.jlink 内容：
# r
# h
# loadbin build\Debug\STM32F407ZGT6_Finalversion.bin 0x8000000
# r
# g
# exit
```

### 使用 STLink 烧写：

```bash
# 使用 ST-Link Utility 或 STM32CubeProgrammer
# 或命令行工具 stm32flash 等
```

### 使用 RT-Thread Studio 内置工具：

1. 连接开发板
2. **Run** → **Debug As** → **GDB Hardware Debugging**
3. 或点击工具栏的 **Debug** 按钮

## 调试

### 打开调试输出

确保在 rtconfig.h 中启用调试：

```c
#define RT_USING_DEBUG
#define DBG_LOG
```

### 连接串口查看日志

使用串口工具（如 PuTTY、Serial Assistant）：
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验位：无

## 文件清单

成功编译后，应包含以下源文件：

```
✓ applications/main.c          - 主程序
✓ applications/can_receiver.c  - CAN接收模块
✓ applications/can_receiver.h  - CAN接收头文件
✓ applications/mqtt_client.c   - MQTT客户端模块
✓ applications/mqtt_client.h   - MQTT客户端头文件
✓ applications/sd_storage.c    - SD存储模块
✓ applications/sd_storage.h    - SD存储头文件
```

## 检查编译输出

编译完成后检查以下文件：

```
build/
├── Debug/                      # 调试版本
│   ├── STM32F407ZGT6_Finalversion.elf
│   ├── STM32F407ZGT6_Finalversion.bin
│   ├── STM32F407ZGT6_Finalversion.map
│   └── ...
```

## 性能优化编译

如需编译优化版本：

```bash
# Release 编译（更小、更快）
scons --release

# 带优化标志
CFLAGS=-O2 scons
```

## 故障排除

### 编译非常慢

- 使用并行编译：`scons -j4`
- 检查磁盘空间
- 关闭不必要的 IDE 后台进程

### 编译输出太长

- 查看完整日志：`scons 2>&1 | tee build.log`
- 只看错误：`scons 2>&1 | grep -i error`

### 链接失败

- 清理旧文件：`scons --clean`
- 删除 build 目录重新编译
- 检查库文件是否完整

## 验证编译成功

编译成功的标志：

1. Console 最后显示：`Building finished with exit code 0`
2. build 目录中生成了 `.elf` 和 `.bin` 文件
3. 没有 error 级别的编译错误（warning 可忽略）

## 下一步

编译成功后，参考 `QUICKSTART.md` 和 `README.md` 进行配置和测试。
