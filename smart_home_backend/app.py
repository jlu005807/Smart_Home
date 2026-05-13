from collections import deque
from copy import deepcopy
from datetime import datetime
from threading import Lock, Thread
import json
import os
import socket
import time

from flask import Flask, jsonify, render_template_string, request

app = Flask(__name__)

DEFAULT_DEVICE_ID = "smart-home-001"
ALLOWED_COMMANDS = {
    "LED_ON",
    "LED_OFF",
    "FAN_ON",
    "FAN_OFF",
    "AUTO_MODE",
    "MANUAL_MODE",
    "READ_STATUS",
}

DEVICE_TCP_HOST = os.getenv("C3_DEVICE_HOST", "192.168.103.45")
DEVICE_TCP_PORT = int(os.getenv("C3_DEVICE_PORT", "5001"))
DEVICE_POLL_INTERVAL = float(os.getenv("C3_POLL_INTERVAL", "1.0"))
DEVICE_CONNECT_TIMEOUT = float(os.getenv("C3_CONNECT_TIMEOUT", "1.0"))
DEVICE_SOCKET_TIMEOUT = float(os.getenv("C3_SOCKET_TIMEOUT", "1.0"))
DEVICE_QUERY_ENABLED = os.getenv("C3_QUERY_ENABLED", "0") != "0"

TCP_LISTEN_HOST = os.getenv("C3_TCP_LISTEN_HOST", "0.0.0.0")
TCP_LISTEN_PORT = int(os.getenv("C3_TCP_LISTEN_PORT", "5001"))
TCP_LISTEN_ENABLED = os.getenv("C3_TCP_LISTEN_ENABLED", "0") != "0"
TCP_SOCKET_TIMEOUT = float(os.getenv("C3_TCP_SOCKET_TIMEOUT", "2.0"))
TCP_MAX_PAYLOAD = int(os.getenv("C3_TCP_MAX_PAYLOAD", "1024"))

_data_lock = Lock()
_latest_status = {}
_command_queue = deque()


def _read_socket_text(sock, max_bytes=2048):
    chunks = []
    total = 0
    while total < max_bytes:
        try:
            data = sock.recv(256)
        except socket.timeout:
            break
        if not data:
            break
        chunks.append(data)
        total += len(data)
        if b"\n" in data:
            break
    return b"".join(chunks).decode("utf-8", errors="ignore")


def _read_tcp_message(sock):
    chunks = []
    total = 0
    while total < TCP_MAX_PAYLOAD:
        try:
            data = sock.recv(256)
        except socket.timeout:
            break
        if not data:
            break
        chunks.append(data)
        total += len(data)
        if b"\n" in data:
            break
    return b"".join(chunks).decode("utf-8", errors="ignore")


def _queue_command(cmd, device_id=None):
    item = {
        "cmd": cmd,
        "device_id": None if device_id in (None, "") else _normalize_device_id(device_id),
        "created_at": _now_text(),
    }
    with _data_lock:
        _command_queue.append(item)
        return len(_command_queue)


def _handle_tcp_message(text, addr):
    line = text.strip()
    if not line:
        return ""

    if line.startswith("REPORT ") or line.startswith("REPORT:"):
        payload = line.split(" ", 1)[1] if " " in line else line.split(":", 1)[1]
        status = _update_status_from_device(payload)
        return "OK\n" if status else "ERR\n"

    if line.startswith("GET_CMD") or line.startswith("NEXT_CMD"):
        parts = line.split()
        device_id = parts[1] if len(parts) > 1 else None
        cmd_item = _take_next_command(_normalize_device_id(device_id))
        return f"{cmd_item['cmd'] if cmd_item else 'NONE'}\n"

    if line.startswith("STATUS"):
        parts = line.split()
        device_id = parts[1] if len(parts) > 1 else None
        with _data_lock:
            status = deepcopy(_latest_status.get(_normalize_device_id(device_id), _default_status(_normalize_device_id(device_id))))
        return f"{json.dumps(status, ensure_ascii=False)}\n"

    if line.startswith("CMD "):
        cmd = line[4:].strip().upper()
        if cmd not in ALLOWED_COMMANDS:
            return "ERR\n"
        _queue_command(cmd)
        return "QUEUED\n"

    if line.startswith("{"):
        status = _update_status_from_device(line)
        return "OK\n" if status else "ERR\n"

    if line.upper() in ALLOWED_COMMANDS:
        _queue_command(line.upper())
        return "QUEUED\n"

    return "OK\n"


def _tcp_client_worker(conn, addr):
    with conn:
        conn.settimeout(TCP_SOCKET_TIMEOUT)
        payload = _read_tcp_message(conn)
        print(f"[TCP_RX] from={addr[0]}:{addr[1]} payload={payload!r}")
        response = _handle_tcp_message(payload, addr)
        if response:
            print(f"[TCP_TX] to={addr[0]}:{addr[1]} resp={response!r}")
            conn.sendall(response.encode("utf-8"))


def _tcp_server_loop():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((TCP_LISTEN_HOST, TCP_LISTEN_PORT))
        server.listen(5)
        print(f"[TCP_SERVER] listen={TCP_LISTEN_HOST}:{TCP_LISTEN_PORT}")
        while True:
            conn, addr = server.accept()
            Thread(target=_tcp_client_worker, args=(conn, addr), daemon=True).start()


def _send_device_message(message):
    if not DEVICE_QUERY_ENABLED:
        return ""
    try:
        with socket.create_connection((DEVICE_TCP_HOST, DEVICE_TCP_PORT), timeout=DEVICE_CONNECT_TIMEOUT) as sock:
            sock.settimeout(DEVICE_SOCKET_TIMEOUT)
            sock.sendall(message.encode("utf-8"))
            return _read_socket_text(sock)
    except Exception as exc:
        print(f"[DEVICE_QUERY] error={exc}")
        return ""


def _take_next_command(device_id):
    with _data_lock:
        for index, item in enumerate(_command_queue):
            target = item.get("device_id")
            if target is None or target == device_id:
                del _command_queue[index]
                return item
    return None


def _update_status_from_device(payload):
    try:
        data = json.loads(payload.strip())
    except Exception:
        print(f"[DEVICE_QUERY] bad payload: {payload!r}")
        return None

    device_id = _normalize_device_id(data.get("device_id"))
    status = _default_status(device_id)
    for key in ("temperature", "humidity", "light", "fan", "led", "mode"):
        if key in data:
            status[key] = data[key]
    status["updated_at"] = _now_text()
    return status


def _device_poll_loop():
    while True:
        status_text = _send_device_message("STATUS\n")
        if status_text:
            status = _update_status_from_device(status_text)
            if status:
                with _data_lock:
                    _latest_status[status["device_id"]] = status

        cmd_item = _take_next_command(DEFAULT_DEVICE_ID)
        if cmd_item is not None:
            response = _send_device_message(f"{cmd_item['cmd']}\n")
            if not response:
                with _data_lock:
                    _command_queue.appendleft(cmd_item)

        time.sleep(DEVICE_POLL_INTERVAL)


@app.before_request
def log_all_requests():
    print(f"[HTTP] from={request.remote_addr} {request.method} {request.path}")


def _normalize_device_id(value):
    if value is None:
        return DEFAULT_DEVICE_ID
    value = str(value).strip()
    return value if value else DEFAULT_DEVICE_ID


def _default_status(device_id):
    return {
        "device_id": device_id,
        "temperature": None,
        "humidity": None,
        "light": None,
        "fan": "unknown",
        "led": "unknown",
        "mode": "unknown",
        "updated_at": None,
    }


def _now_text():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


@app.route("/api/device/report", methods=["POST"])
def device_report():
    payload = request.get_json(silent=True) or {}
    device_id = _normalize_device_id(payload.get("device_id"))

    status = _default_status(device_id)
    for key in ("temperature", "humidity", "light", "fan", "led", "mode"):
        if key in payload:
            status[key] = payload[key]
    status["updated_at"] = _now_text()

    with _data_lock:
        _latest_status[device_id] = status
    print(f"[DEVICE_REPORT] from={request.remote_addr} id={device_id} temp={status.get('temperature')} hum={status.get('humidity')} light={status.get('light')}")

    return jsonify({"ok": True, "device_id": device_id, "saved_at": status["updated_at"]})


@app.route("/api/device/next_cmd", methods=["GET"])
def device_next_cmd():
    device_id = _normalize_device_id(request.args.get("device_id"))
    next_command = "NONE"

    with _data_lock:
        for index, item in enumerate(_command_queue):
            target = item.get("device_id")
            if target is None or target == device_id:
                next_command = item["cmd"]
                del _command_queue[index]
                break
    print(f"[DEVICE_NEXT_CMD] from={request.remote_addr} id={device_id} cmd={next_command}")

    return jsonify({"cmd": next_command})


@app.route("/api/web/command", methods=["POST"])
def web_command():
    payload = request.get_json(silent=True) or {}
    cmd = str(payload.get("cmd", "")).strip().upper()
    if cmd not in ALLOWED_COMMANDS:
        return jsonify({"ok": False, "error": "unsupported cmd", "allowed": sorted(ALLOWED_COMMANDS)}), 400

    target_id = payload.get("device_id")
    target_id = None if target_id in (None, "") else _normalize_device_id(target_id)

    item = {"cmd": cmd, "device_id": target_id, "created_at": _now_text()}

    with _data_lock:
        _command_queue.append(item)
        queue_size = len(_command_queue)

    return jsonify({"ok": True, "queued": item, "pending_count": queue_size})


@app.route("/api/web/status", methods=["GET"])
def web_status():
    device_id = _normalize_device_id(request.args.get("device_id"))

    with _data_lock:
        current = deepcopy(_latest_status.get(device_id, _default_status(device_id)))
        all_status = deepcopy(_latest_status)
        pending = list(deepcopy(_command_queue))

    return jsonify(
        {
            "device_id": device_id,
            "current_status": current,
            "all_devices": all_status,
            "pending_commands": pending,
        }
    )


@app.route("/", methods=["GET"])
def index():
    page = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Smart Home Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 24px; max-width: 900px; }
    h1 { margin-bottom: 8px; }
    .grid { display: grid; grid-template-columns: repeat(2, minmax(220px, 1fr)); gap: 12px; margin-bottom: 20px; }
    .card { border: 1px solid #ddd; border-radius: 6px; padding: 12px; }
    .label { color: #666; font-size: 13px; margin-bottom: 6px; }
    .value { font-size: 20px; font-weight: bold; }
    .buttons { display: grid; grid-template-columns: repeat(3, minmax(120px, 1fr)); gap: 10px; margin: 16px 0; }
    button { padding: 10px 12px; border: 1px solid #888; background: #fff; border-radius: 6px; cursor: pointer; }
    button:hover { background: #f5f5f5; }
    pre { background: #f6f8fa; border: 1px solid #ddd; border-radius: 6px; padding: 12px; overflow: auto; }
    .hint { color: #666; font-size: 13px; }
  </style>
</head>
<body>
  <h1>小米 AIoT 智能家居</h1>
  <div class="hint">设备ID：<span id="device-id"></span></div>

  <div class="grid">
    <div class="card"><div class="label">温度</div><div class="value" id="temperature">--</div></div>
    <div class="card"><div class="label">湿度</div><div class="value" id="humidity">--</div></div>
    <div class="card"><div class="label">光照</div><div class="value" id="light">--</div></div>
    <div class="card"><div class="label">模式</div><div class="value" id="mode">--</div></div>
    <div class="card"><div class="label">风扇</div><div class="value" id="fan">--</div></div>
    <div class="card"><div class="label">LED</div><div class="value" id="led">--</div></div>
  </div>

  <div class="buttons">
    <button onclick="sendCmd('LED_ON')">打开 LED</button>
    <button onclick="sendCmd('LED_OFF')">关闭 LED</button>
    <button onclick="sendCmd('FAN_ON')">打开风扇</button>
    <button onclick="sendCmd('FAN_OFF')">关闭风扇</button>
    <button onclick="sendCmd('AUTO_MODE')">自动模式</button>
    <button onclick="sendCmd('MANUAL_MODE')">手动模式</button>
  </div>

  <h3>待执行命令队列</h3>
  <pre id="queue">[]</pre>

  <script>
    const deviceId = new URLSearchParams(window.location.search).get("device_id") || "smart-home-001";
    document.getElementById("device-id").textContent = deviceId;

    function textOrDash(value, suffix = "") {
      return value === null || value === undefined ? "--" : `${value}${suffix}`;
    }

    async function sendCmd(cmd) {
      const resp = await fetch("/api/web/command", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({ cmd, device_id: deviceId })
      });
      if (!resp.ok) {
        const err = await resp.text();
        alert("命令下发失败: " + err);
        return;
      }
      await refreshStatus();
    }

    async function refreshStatus() {
      const resp = await fetch(`/api/web/status?device_id=${encodeURIComponent(deviceId)}`);
      const data = await resp.json();
      const s = data.current_status || {};

      document.getElementById("temperature").textContent = textOrDash(s.temperature, " °C");
      document.getElementById("humidity").textContent = textOrDash(s.humidity, " %");
      document.getElementById("light").textContent = textOrDash(s.light, " lx");
      document.getElementById("mode").textContent = textOrDash(s.mode);
      document.getElementById("fan").textContent = textOrDash(s.fan);
      document.getElementById("led").textContent = textOrDash(s.led);
      document.getElementById("queue").textContent = JSON.stringify(data.pending_commands || [], null, 2);
    }

    refreshStatus();
    setInterval(refreshStatus, 1500);
  </script>
</body>
</html>
"""
    return render_template_string(page)


if __name__ == "__main__":
    try:
        addrs = {info[4][0] for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)}
    except Exception:
        addrs = set()

    try:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.connect(("8.8.8.8", 80))
        addrs.add(probe.getsockname()[0])
        probe.close()
    except Exception:
        pass

    print("[LOCAL_IPV4]", ", ".join(sorted(addrs)) if addrs else "N/A")
    if DEVICE_QUERY_ENABLED:
        print(f"[DEVICE_QUERY] target={DEVICE_TCP_HOST}:{DEVICE_TCP_PORT} interval={DEVICE_POLL_INTERVAL}s")
        Thread(target=_device_poll_loop, daemon=True).start()
    if TCP_LISTEN_ENABLED:
        Thread(target=_tcp_server_loop, daemon=True).start()
    app.run(host="0.0.0.0", port=5000, debug=False)
