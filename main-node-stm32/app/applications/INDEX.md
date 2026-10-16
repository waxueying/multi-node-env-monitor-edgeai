# 📚 项目文档导航

欢迎使用STM32F407ZGT6 IoT应用框架！这个项目包含了完整的CAN接收、WiFi/MQTT上传和SD卡备份功能。

## 🎯 快速导航

### 📖 新用户入门

1. **首先阅读** → [`QUICKSTART.md`](QUICKSTART.md) ⭐ 
   - 5分钟了解项目结构
   - 快速配置步骤
   - 常见问题解答

2. **然后编译** → [`COMPILE.md`](COMPILE.md)
   - 详细的编译步骤
   - IDE配置方法
   - 故障排除

3. **最后部署** → [`README.md`](README.md)
   - 完整的功能说明
   - 硬件连接指南
   - 监控和测试

### 👨‍💻 开发者文档

- **API参考** → [`API_REFERENCE.md`](API_REFERENCE.md)
  - 所有函数的详细说明
  - 使用示例代码
  - 参数说明

- **项目总结** → [`SUMMARY.md`](SUMMARY.md)
  - 项目完成情况总结
  - 系统架构说明
  - 性能指标

### 📁 源代码文件

#### 模块1：CAN接收
- [`can_receiver.h`](can_receiver.h) - 头文件（定义和接口）
- [`can_receiver.c`](can_receiver.c) - 实现文件（80+行）

#### 模块2：WiFi/MQTT
- [`mqtt_client.h`](mqtt_client.h) - 头文件（结构体和接口）
- [`mqtt_client.c`](mqtt_client.c) - 实现文件（450+行）

#### 模块3：SD卡存储
- [`sd_storage.h`](sd_storage.h) - 头文件（数据结构）
- [`sd_storage.c`](sd_storage.c) - 实现文件（380+行）

#### 主程序
- [`main.c`](main.c) - 应用入口（180+行）

---

## 📊 文档一览

| 文档 | 行数 | 主要内容 | 适合人群 |
|------|------|--------|---------|
| **QUICKSTART.md** | 200+ | 快速开始、配置 | 新手 |
| **README.md** | 300+ | 完整功能说明 | 所有人 |
| **API_REFERENCE.md** | 600+ | API文档、示例 | 开发者 |
| **COMPILE.md** | 200+ | 编译部署指南 | 开发者 |
| **SUMMARY.md** | 250+ | 项目总结、架构 | 项目管理 |
| **INDEX.md** | 本文件 | 导航和索引 | 所有人 |

---

## 🏗️ 项目结构

```
applications/
├── 📄 文档文件
│   ├── QUICKSTART.md          ⭐ 新手必读
│   ├── README.md              完整文档
│   ├── API_REFERENCE.md       开发参考
│   ├── COMPILE.md             编译指南
│   ├── SUMMARY.md             项目总结
│   └── INDEX.md               本文件
│
├── 📂 模块1：CAN接收
│   ├── can_receiver.h         (接口定义)
│   └── can_receiver.c         (实现代码)
│
├── 📂 模块2：WiFi/MQTT
│   ├── mqtt_client.h          (接口定义)
│   └── mqtt_client.c          (实现代码)
│
├── 📂 模块3：SD存储
│   ├── sd_storage.h           (接口定义)
│   └── sd_storage.c           (实现代码)
│
└── 📂 主程序
    └── main.c                 (集成各模块)
```

---

## 🚀 使用流程

### 第一次使用

```
1. 阅读 QUICKSTART.md
    ↓
2. 修改配置 (WiFi, MQTT, CAN ID)
    ↓
3. 查看 COMPILE.md 编译
    ↓
4. 烧写到开发板
    ↓
5. 用串口查看输出
    ↓
6. 测试各功能
```

### 开发和扩展

```
1. 查看 API_REFERENCE.md
    ↓
2. 查看相应模块的头文件
    ↓
3. 编写你的代码
    ↓
4. 调试和测试
```

---

## 🔑 关键配置项

### WiFi/MQTT配置 (main.c)
```c
mqtt_config.wifi_ssid = "YOUR_SSID";
mqtt_config.wifi_password = "YOUR_PASSWORD";
mqtt_config.mqtt_broker = "your.broker.address";
mqtt_config.mqtt_port = 1883;
mqtt_config.mqtt_publish_topic = "your/topic";
```

### CAN ID配置 (can_receiver.h)
```c
#define CAN_ID_1 0x001
#define CAN_ID_2 0x002
```

### UART配置 (mqtt_client.c)
```c
mqtt_client.uart_dev = rt_device_find("uart2");
```

---

## ✨ 功能对比表

| 功能 | CAN接收 | WiFi/MQTT | SD存储 |
|------|---------|----------|--------|
| 自动初始化 | ✓ | ✓ | ✓ |
| 消息队列 | ✓ | ✗ | ✓ (缓冲) |
| 自动重连 | ✗ | ✓ | 重试挂载 |
| 线程数 | 1 | 2 | 1 |
| 线程优先级 | 15 | 12-13 | 14 |

---

## 📞 问题排查

### 编译问题
→ 查看 [`COMPILE.md`](COMPILE.md) 的"常见编译错误"章节

### 配置问题
→ 查看 [`QUICKSTART.md`](QUICKSTART.md) 的"快速配置步骤"章节

### 运行问题
→ 查看 [`README.md`](README.md) 的"常见问题"章节

### 使用API
→ 查看 [`API_REFERENCE.md`](API_REFERENCE.md) 对应函数

---

## 🎓 学习路径

### 初级（理解项目）
1. 阅读 QUICKSTART.md
2. 查看 main.c 中的初始化代码
3. 理解三个模块的作用

### 中级（使用项目）
1. 修改配置参数
2. 编译和烧写
3. 用串口监控运行
4. 验证各模块功能

### 高级（扩展项目）
1. 学习 API_REFERENCE.md
2. 阅读各模块源代码
3. 添加新功能或修改现有功能
4. 优化性能

---

## 📋 代码统计

```
总代码行数：  > 1500 行
源文件数：    7 个
文档文件数：  6 个
线程数：      5 个
模块数：      3 个
优先级范围：  11-15
```

---

## ✅ 检查清单

### 首次配置
- [ ] 修改 WiFi SSID 和密码
- [ ] 修改 MQTT broker 地址
- [ ] 确认 CAN ID 正确
- [ ] 确认 UART 设备名称

### 编译前
- [ ] 确认所有文件存在
- [ ] 检查 rt-thread 库配置
- [ ] 确保编译工具链可用

### 烧写前
- [ ] 确保编译成功
- [ ] 备份原有代码
- [ ] 测试烧写工具

### 运行后
- [ ] 检查串口输出
- [ ] 验证 CAN 接收
- [ ] 验证 WiFi 连接
- [ ] 验证 MQTT 发送
- [ ] 检查 SD 卡文件

---

## 🔗 相关资源

### RT-Thread 官方
- [RT-Thread 官网](https://www.rt-thread.io/)
- [RT-Thread 文档](https://docs.rt-thread.io/)
- [RT-Thread GitHub](https://github.com/RT-Thread/rt-thread)

### ESP8266
- [ESP8266 技术参考](https://www.espressif.com/)
- [AT 指令集](https://www.espressif.com/)

### MQTT
- [MQTT 协议](https://mqtt.org/)
- [MQTTX 客户端](https://mqttx.app/)

### CAN 总线
- [CAN 总线介绍](https://en.wikipedia.org/wiki/CAN_bus)

---

## 📝 文档更新日志

| 日期 | 内容 |
|------|------|
| 2025-12-02 | 初始版本发布 |
| 2025-12-02 | 添加所有文档文件 |

---

## 📞 技术支持

### 常见问题已在文档中解答
- ❓ 如何修改 CAN ID？→ QUICKSTART.md
- ❓ 如何配置 WiFi？→ QUICKSTART.md  
- ❓ 编译失败怎么办？→ COMPILE.md
- ❓ API 如何使用？→ API_REFERENCE.md

### 调试技巧
- 启用详细日志：修改 DBG_LVL 为 DBG_LOG
- 查看缓冲状态：调用相应的 get_count() 函数
- 监控连接状态：查看各模块的 get_status() 函数

---

## 🎯 后续优化方向

1. **数据压缩** - 减少 MQTT 流量
2. **本地数据库** - 支持离线查询
3. **远程配置** - 通过 MQTT 更新参数
4. **低功耗模式** - 扩展续航时间
5. **Web 界面** - 配置和监测工具

---

## 📄 许可证

Apache License 2.0

---

## 🙏 致谢

感谢使用本项目框架！希望它能帮助你快速构建 IoT 应用。

**项目完成日期：** 2025-12-02

---

## 快速链接

| 链接 | 说明 |
|------|------|
| [QUICKSTART.md](QUICKSTART.md) | ⭐ 新手必读 |
| [API_REFERENCE.md](API_REFERENCE.md) | 开发者参考 |
| [README.md](README.md) | 完整文档 |
| [COMPILE.md](COMPILE.md) | 编译指南 |
| [SUMMARY.md](SUMMARY.md) | 项目总结 |

---

**💡 提示：** 如果这是你第一次使用此项目，请从 QUICKSTART.md 开始！
