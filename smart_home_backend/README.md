# Smart Home Backend (Flask)

用于 C3 Wi-Fi 子板（ESP32 AT 指令）接入的课程设计 Demo 后端。

## 1. 项目结构

```text
smart_home_backend/
├── app.py
├── requirements.txt
└── README.md
```

## 2. 安装依赖

建议使用 Python 3.9+。

```bash
cd smart_home_backend
python -m venv .venv
.venv\\Scripts\\activate
pip install -r requirements.txt
```

## 3. 启动后端

```bash
python app.py
```

服务默认监听：

- `0.0.0.0:5000`
- 局域网内 C3 可通过 `http://<电脑IP>:5000` 访问

## 4. 接口说明

### 4.1 设备上报

- `POST /api/device/report`
- 示例 JSON：

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

### 4.2 设备轮询命令

- `GET /api/device/next_cmd?device_id=smart-home-001`
- 有命令返回：

```json
{"cmd":"FAN_ON"}
```

- 无命令返回：

```json
{"cmd":"NONE"}
```

### 4.3 网页/调试工具下发命令

- `POST /api/web/command`
- 示例 JSON：

```json
{"cmd":"FAN_ON"}
```

支持命令：

- `LED_ON`
- `LED_OFF`
- `FAN_ON`
- `FAN_OFF`
- `AUTO_MODE`
- `MANUAL_MODE`
- `READ_STATUS`

### 4.4 查看状态

- `GET /api/web/status`
- 返回当前设备最新状态 + 待执行命令队列。

## 5. Web 页面

浏览器打开：

```text
http://127.0.0.1:5000/
```

页面提供：

- 温度、湿度、光照、风扇、LED、模式显示
- LED/风扇开关按钮
- 自动/手动模式按钮

## 6. 查看电脑局域网 IP

Windows（推荐）：

```powershell
ipconfig
```

找到当前网卡的 `IPv4 Address`，例如 `192.168.1.100`。

## 7. C3 通过 AT 指令连接 Flask 测试

下面示例基于 ESP32 AT 固件，串口波特率按你板子设置（常见 115200）。

1) 连 Wi-Fi：

```text
AT
AT+CWMODE=1
AT+CWJAP="你的WiFi名","你的WiFi密码"
```

2) 连 Flask 服务器（电脑 IP 假设 `192.168.1.100`）：

```text
AT+CIPMUX=1
AT+CIPSTART=0,"TCP","192.168.1.100",5000
```

3) 发送轮询命令 HTTP 请求（GET）：

```text
AT+CIPSEND=0,89
GET /api/device/next_cmd?device_id=smart-home-001 HTTP/1.1
Host: 192.168.1.100:5000

```

说明：`CIPSEND` 的长度需要与你实际发送字节数一致，不一致会失败。

4) 发送状态上报 HTTP 请求（POST）：

请求格式示意（长度请按实际内容计算）：

```text
POST /api/device/report HTTP/1.1
Host: 192.168.1.100:5000
Content-Type: application/json
Content-Length: <len>

{"device_id":"smart-home-001","temperature":28.5,"humidity":52.3,"light":360,"fan":"off","led":"on","mode":"auto"}
```

## 8. 常见问题排查

1. C3 连不上服务器  
   - 确认电脑和 C3 在同一局域网  
   - 确认 Flask 已启动并监听 `0.0.0.0:5000`

2. 浏览器能开，C3 不通  
   - 检查 Windows 防火墙是否放行 5000 端口  
   - 用手机或同网段电脑访问 `http://<电脑IP>:5000/api/web/status` 验证

3. `CIPSEND` 后无响应或报错  
   - 检查发送长度是否正确  
   - 检查 HTTP 报文是否包含空行分隔 Header 和 Body

4. 设备一直收到 `NONE`  
   - 先调用 `POST /api/web/command` 下发命令  
   - 再让设备轮询 `GET /api/device/next_cmd`

5. 状态不更新  
   - 检查 `POST /api/device/report` JSON 是否合法  
   - 确认 `Content-Type: application/json`
