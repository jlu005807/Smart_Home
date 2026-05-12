# Smart Home Backend

用于当前智能家居固件的 Flask 后端。

这个 README 只描述仓库里已经实现的后端接口，并同步当前 C3 固件默认配置。当前后端代码已完成，C3 实机联网仍在联调中。

## 1. 目录结构

```text
smart_home_backend/
├── app.py
├── requirements.txt
└── README.md
```

## 2. 安装与启动

建议使用 Python 3.9+：

```bash
cd smart_home_backend
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python app.py
```

启动后：

- 服务监听 `0.0.0.0:5000`
- 控制台会打印本机可用 IPv4，例如 `[LOCAL_IPV4] 192.168.137.1`

浏览器可直接打开：

```text
http://127.0.0.1:5000/
```

## 3. 当前固件默认对接参数

当前固件默认值来自 [c3_wifi.c](/abs/path/d:/xiaomi/Smart_Home/demo/Project/GD32F450Z_BSP/src/c3_wifi.c)：

```c
#define C3_WIFI_SSID         "ozy"
#define C3_WIFI_PASSWORD     "12345678"
#define C3_SERVER_HOST       "192.168.137.1"
#define C3_SERVER_PORT       5000U
#define C3_DEVICE_ID         "smart-home-001"
```

这意味着：

- 如果你按当前固件直接烧录，C3 会尝试连接热点 `ozy`
- 它会把 `192.168.137.1:5000` 当作后端地址
- 如果你的电脑热点 IP 不是 `192.168.137.1`，需要同步修改固件中的 `C3_SERVER_HOST`

## 4. 后端接口

### 4.1 设备状态上报

- 方法：`POST`
- 路径：`/api/device/report`

示例请求：

```json
{
  "device_id": "smart-home-001",
  "temperature": 28.5,
  "humidity": 52.3,
  "light": 360,
  "fan": "off",
  "led": "on",
  "mode": "auto"
}
```

示例响应：

```json
{
  "ok": true,
  "device_id": "smart-home-001",
  "saved_at": "2026-05-12 10:30:00"
}
```

### 4.2 设备轮询命令

- 方法：`GET`
- 路径：`/api/device/next_cmd?device_id=smart-home-001`

有命令时返回：

```json
{"cmd":"FAN_ON"}
```

无命令时返回：

```json
{"cmd":"NONE"}
```

### 4.3 网页或调试工具下发命令

- 方法：`POST`
- 路径：`/api/web/command`

示例请求：

```json
{"cmd":"FAN_ON","device_id":"smart-home-001"}
```

当前支持的命令：

- `LED_ON`
- `LED_OFF`
- `FAN_ON`
- `FAN_OFF`
- `AUTO_MODE`
- `MANUAL_MODE`
- `READ_STATUS`

### 4.4 查看当前状态

- 方法：`GET`
- 路径：`/api/web/status`

推荐直接带设备号：

```text
/api/web/status?device_id=smart-home-001
```

返回内容包含：

- `current_status`
- `all_devices`
- `pending_commands`

## 5. Web 控制页

访问：

```text
http://127.0.0.1:5000/?device_id=smart-home-001
```

页面当前能力：

- 展示温度、湿度、光照、模式、风扇、LED
- 下发 `LED_ON / LED_OFF`
- 下发 `FAN_ON / FAN_OFF`
- 下发 `AUTO_MODE / MANUAL_MODE`
- 每 1.5 秒刷新一次状态

## 6. 当前固件与后端交互方式

当前固件不是长连接模式，而是“每次请求单独建连、发送、关闭”：

1. `Wifi_Init()` 阶段先发一次 `GET /api/device/next_cmd` 探测后端。
2. 运行中每约 1 秒发一次 `POST /api/device/report`。
3. 运行中每约 1 秒发一次 `GET /api/device/next_cmd`。
4. 若连续多次 TCP 建连失败，固件会把网络状态置为未就绪并进入重连流程。

## 7. 联调建议

### 7.1 电脑热点方案

如果沿用当前固件默认值，建议：

- 电脑开启热点，SSID 设为 `ozy`
- 密码设为 `12345678`
- 确认热点网卡 IPv4 是 `192.168.137.1`
- 在电脑上启动 Flask 后端
- 放行 Windows 防火墙的 `5000` 端口

### 7.2 后端连通性自测

先在电脑本机浏览器访问：

```text
http://127.0.0.1:5000/api/web/status?device_id=smart-home-001
```

再在同一局域网的手机或另一台电脑访问：

```text
http://<电脑IP>:5000/api/web/status?device_id=smart-home-001
```

如果这一步不通，问题不在 C3 固件，而在网络、热点或防火墙。

## 8. 常见问题

### 8.1 浏览器能打开，C3 还是不通

优先检查：

- C3 实际连上的是不是目标热点
- `C3_SERVER_HOST` 是否与电脑热点网关一致
- Windows 防火墙是否放行 5000 端口
- 电脑是否同时连着手机热点，导致你误判了目标网络

### 8.2 C3 总是回连旧手机热点

当前固件已经加入以下清理动作：

- `AT+RESTORE`
- `AT+CWAUTOCONN=0`
- `AT+CWRECONNCFG=0,0`
- `AT+CWQAP`
- `AT+SYSSTORE=0`

如果仍然回连旧热点，至少要同时满足：

- 旧手机热点先关闭
- 上电后查看串口日志里的 `C3 AP INFO:`
- 确认 `AT+CWJAP?` 返回的 SSID 真的是 `ozy`

### 8.3 `CIPSEND` 手工调试失败

手工 AT 调试时，`AT+CIPSEND=<len>` 的长度必须与后续 HTTP 报文的实际字节数完全一致。

固件内部会自动计算长度；只有你手工在串口工具里发 AT 指令时，才需要自己算。

### 8.4 后端状态一直是空

说明后端没有收到 `POST /api/device/report`。优先看 Flask 控制台是否出现：

- `[HTTP]`
- `[DEVICE_REPORT]`
- `[DEVICE_NEXT_CMD]`

如果这三类日志都没有，问题还在 C3 到后端之间。

## 9. 当前结论

- 后端接口已完整可用。
- 固件默认按 `ozy / 12345678 / 192.168.137.1:5000` 联调。
- 文档现在与当前代码实现一致。
- C3 实机联网问题仍待继续排查，不应把当前状态写成“已联通”。
