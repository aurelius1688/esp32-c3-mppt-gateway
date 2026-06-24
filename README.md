# ESP32-C3 MPPT Gateway

通过 ESP32-C3 作为 UDP 网关，将 MPPT 太阳能控制器的实时数据接入 Home Assistant (ESPHome)。

## 项目背景

MPPT 太阳能控制器（基于 ESP32）通过 I2C 总线读取 INA226 电流/电压传感器，获取太阳能板输入和电池输出的电压、电流、功率数据。控制器自身通过 WiFi UDP 广播这些数据（端口 2333）。

本项目用 **ESP32-C3** 作为 UDP 客户端接收这些数据，通过 **ESPHome** 转发到 **Home Assistant**，实现太阳能系统的远程监控。

## 工作原理

```
┌─────────────────────────────────────────────────────┐
│                  MPPT 控制器 (ESP32)                   │
│  INA226 #1 ← 太阳能输入 (VI/CI/PI)                   │
│  INA226 #2 ← 电池输出 (VO/CO/PO)                    │
│  UDP 端口 2333 → 广播逗号分隔数据                     │
└──────────────────────┬──────────────────────────────┘
                       │ WiFi UDP
                       ▼
┌─────────────────────────────────────────────────────┐
│              ESP32-C3 网关 (本方案)                    │
│  WiFiUDP.begin(2333) → 接收数据                       │
│  ESPHome API → 加密转发到 HA                          │
└──────────────────────┬──────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────┐
│              Home Assistant                           │
│  Solar Voltage / Current / Power                     │
│  Battery Voltage / Current / Power                   │
│  PWM / PPWM / PWMC / Loop Time / Error Code          │
└─────────────────────────────────────────────────────┘
```

## UDP 数据格式

MPPT 控制器每隔约 250ms 发送一次逗号分隔的数据：

```
rnfa0.00,rnfb0.00,P1985.00,PP1985.00,PC4.00,PI0.00,VI0.04,CI-0.07,PO0.78,VO13.14,CO-0.06,D1.00,LT8.05,
```

每个字段由 **名称(字母)** + **数值(数字)** 直接拼接。

### 字段映射表

| 字段 | 含义 | 单位 | ESPHome 传感器 |
|------|------|------|----------------|
| `VI` | 太阳能输入电压 | V | Solar Voltage |
| `CI` | 太阳能输入电流 | A | Solar Current |
| `PI` | 太阳能输入功率 | W | Solar Power |
| `VO` | 电池输出电压 | V | Battery Voltage |
| `CO` | 电池输出电流 | A | Battery Current |
| `PO` | 电池输出功率 | W | Battery Power |
| `P` | PWM 占空比值 | - | PWM Value |
| `PP` | 预测 PWM 值 | - | PPWM Value |
| `PC` | PWM 计数 | - | PWMC Value |
| `D` | 状态标志 | - | (内部) |
| `LT` | 循环耗时 | ms | Loop Time |
| `rnfa` | 参考电压 A | V | (内部) |
| `rnfb` | 参考电压 B | V | (内部) |

## 硬件需求

| 组件 | 说明 |
|------|------|
| ESP32-C3 开发板 | 如 ESP32-C3-DevKitM-1 或兼容板 |
| MPPT 控制器 | 基于 ESP32 并支持 UDP 广播的 MPPT 太阳能控制器 |
| USB 数据线 | 用于首次刷机 |
| 5V 电源 | 为 ESP32-C3 供电 |

## 软件环境

- [ESPHome](https://esphome.io/) ≥ 2025.x
- [Home Assistant](https://www.home-assistant.io/) ≥ 2024.x
- Python ≥ 3.10

## 快速开始

### 1. 克隆/下载本项目

```bash
git clone [https://github.com/your-username/esp32-mppt-c3-gateway.git](https://github.com/aurelius1688/esp32-c3-mppt-gateway.git)
cd esp32-mppt-c3-gateway
```

### 2. 配置 secrets.yaml

```yaml
wifi_ssid: "你的WiFi名称"
wifi_password: "你的WiFi密码"
api_key: "你的API密钥"        # 运行 python -c "import base64, os; print(base64.b64encode(os.urandom(32)).decode())" 生成
ota_password: "你的OTA密码"
```

### 3. 修改 MPPT 控制器 IP

编辑 `mppt_gateway.yaml`，找到以下行并修改为目标 MPPT 控制器的 IP 地址：

```yaml
mppt_udp.begin("192.168.1.160", 2333);
```

### 4. 编译

```bash
esphome compile mppt_gateway.yaml
```

### 5. 刷机

**USB 首次刷机：**
```bash
esphome upload mppt_gateway.yaml --device COMx
```
或在浏览器打开 [ESP Web Flasher](https://web.esphome.io/)，连接设备后选择编译生成的 `mppt-gateway.factory.bin`。

**OTA 无线更新：**
```bash
esphome upload mppt_gateway.yaml
```

### 6. 接入 Home Assistant

1. Home Assistant → **设置** → **设备与服务** → **添加集成**
2. 搜索并选择 **ESPHome**
3. 输入 ESP32-C3 的 IP 地址（可在路由器管理页面查看）
4. 输入 `secrets.yaml` 中配置的 API 密钥

## 传感器说明

设备接入 HA 后，会创建以下实体：

### 传感器 (Sensor)

| 名称 | 单位 | 说明 |
|------|------|------|
| Solar Voltage | V | 太阳能板输入电压 |
| Solar Current | A | 太阳能板输入电流 |
| Solar Power | W | 太阳能板输入功率 |
| Battery Voltage | V | 电池端电压 |
| Battery Current | A | 电池充放电电流 |
| Battery Power | W | 电池充放电功率 |
| Battery Efficiency | % | 转换效率 = (输出功率/输入功率) × 100 |
| Loop Time | ms | 控制器主循环耗时 |
| PWM Value | - | PWM 占空比值 |
| PPWM Value | - | 预测 PWM 值 |
| PWMC Value | - | PWM 计数值 |
| Error Code | - | 错误码（0=正常） |

### 二进制传感器 (Binary Sensor)

| 名称 | 说明 |
|------|------|
| Charging Status | 充电中/空闲 |
| Fan Status | 风扇运转中/停止 |
| MPPT Connected | UDP 通信连接正常/断开 |

### 文本传感器 (Text Sensor)

| 名称 | 说明 |
|------|------|
| MPPT Status | Charging / Idle / Error / Disconnected |
| Gateway IP Address | ESP32-C3 本机 IP |

### 控制 (Controls)

| 类型 | 名称 | 说明 |
|------|------|------|
| Switch | MPPT Start | 开始/停止接收数据 |
| Button | Reconnect MPPT | 重新发送 START1 命令 |
| Text | MPPT IP Address | 动态修改目标 MPPT 控制器 IP |
| Number | Max Battery Voltage | 设置最大电池电压 |
| Number | Charging Current | 设置充电电流 |

## 开发说明

### 项目结构

```
esp32-mppt-c3-gateway/
├── mppt_gateway.yaml           # ESPHome 主配置
├── secrets.yaml                # 密钥配置 (不入库)
├── .gitignore
├── README.md
├── components/
│   └── mppt_udp_client.h       # UDP 客户端 C++ 实现
└── ...
```

### UDP 客户端核心类 (`components/mppt_udp_client.h`)

`MPPTUDPClient` 类提供以下能力：

- **UDP 绑定** — `udp_.begin(2333)` 监听本地 2333 端口
- **协议握手** — 每 3 秒发送 `START1` 维持连接
- **数据解析** — `strtok` 按逗号分割，自动提取名称+数值
- **动态字段发现** — 首次遇到未知字段名自动注册并输出到日志
- **超时检测** — 10 秒无数据自动标记断开

关键方法：

```cpp
void begin(const char* host, uint16_t port);  // 设置目标地址
void loop();                                   // 周期性调用，处理收发
bool is_connected();                           // 是否已收到数据
float get_value(const char* name);             // 按字段名取值
float get_voltage_in();                        // 常用传感器取值方法
void send_cmd(const char* cmd);                // 发送命令
```

### 调试日志

日志级别设为 `DEBUG`，在 ESPHome Web 日志 (`http://设备IP/log`) 可见：

```
[D][mppt:034]: NEW FIELD[0]: rnfa
[D][mppt:034]: NEW FIELD[1]: rnfb
...
[D][mppt:161]: rnfa=0.00, rnfb=0.00, P=1985.00, ...
```

## 注意事项

1. **WiFi 状态检查** — 不要在 `WiFi.status()` 上做条件判断，ESPHome 环境下此 API 不可靠
2. **日志输出** — 使用 `ESP_LOGD` 而非 `Serial.printf`，后者在 ESPHome Web 日志中不可见
3. **UDP 端口一致性** — 收发都用端口 2333，MPPT 固定回复到源端口
4. **`udp_.write()` 类型转换** — 参数需强制转换为 `const uint8_t*`
5. **初始化时机** — ESPHome 的 `interval` 在 setup 阶段就开始运行，注意时序

## 常见问题

**Q: 数据一直是 0，MPPT Connected 显示 OFF？**  
A: 检查 MPPT 控制器 IP 地址是否正确，确认 MPPT 控制器和 ESP32-C3 在同一个 WiFi 网络。

**Q: PWM 值显示 1985，PPWM 也是 1985？**  
A: 这是 PWM 的原始计数值（11 位分辨率，最大值 2047），属于正常工作状态。

**Q: 如何查看原始 UDP 数据？**  
A: 在 Web 日志 (`http://设备IP/log`) 中搜索 `[D][mppt:161]:` 即可看到每帧原始数据。

**Q: 可以修改数据更新频率吗？**  
A: 可以，在 `mppt_gateway.yaml` 中修改 `interval: 2s` 的数值即可。

## 许可证

MIT
