"""Parse JSON telemetry from ESP32 over USB serial."""

from __future__ import annotations

import json
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Deque, Optional

import serial
import serial.tools.list_ports


@dataclass
class SensorReading:
    timestamp: float
    temp_c: Optional[float]
    temp_v: float
    temp_st: str
    turb_raw: int
    turb_v: float
    turb_st: str
    level_pct: int
    level_raw: int
    level_v: float
    level_label: str
    level_st: str
    connected: int
    active: int


@dataclass
class SerialBridge:
    port: Optional[str] = None
    baud: int = 115200
    history: Deque[SensorReading] = field(default_factory=lambda: deque(maxlen=100))
    _ser: Optional[serial.Serial] = field(default=None, repr=False)
    _thread: Optional[threading.Thread] = field(default=None, repr=False)
    _stop: threading.Event = field(default_factory=threading.Event, repr=False)
    last_error: str = ""
    lines_read: int = 0

    @property
    def is_connected(self) -> bool:
        return self._ser is not None and self._ser.is_open

    @property
    def latest(self) -> Optional[SensorReading]:
        return self.history[-1] if self.history else None

    def list_ports(self) -> list[str]:
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port: str) -> None:
        self.disconnect()
        self.port = port
        self._stop.clear()
        self._ser = serial.Serial(port, self.baud, timeout=0.5)
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def disconnect(self) -> None:
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1.0)
        self._thread = None
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    def clear_history(self) -> None:
        self.history.clear()
        self.lines_read = 0

    def _read_loop(self) -> None:
        assert self._ser is not None
        buffer = ""
        while not self._stop.is_set():
            try:
                chunk = self._ser.read(512).decode("utf-8", errors="ignore")
                if not chunk:
                    continue
                buffer += chunk
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    reading = parse_line(line.strip())
                    if reading is not None:
                        self.history.append(reading)
                        self.lines_read += 1
                        self.last_error = ""
            except Exception as exc:  # noqa: BLE001
                self.last_error = str(exc)
                time.sleep(0.5)


def parse_line(line: str) -> Optional[SensorReading]:
    if not line.startswith("{") or '"t":1' not in line:
        return None

    try:
        payload = json.loads(line)
    except json.JSONDecodeError:
        return None

    if payload.get("t") != 1:
        return None

    temp_c = payload.get("temp_c")
    if temp_c is not None:
        temp_c = float(temp_c)

    return SensorReading(
        timestamp=time.time(),
        temp_c=temp_c,
        temp_v=float(payload.get("temp_v", 0.0)),
        temp_st=str(payload.get("temp_st", "OFF")),
        turb_raw=int(payload.get("turb_raw", 0)),
        turb_v=float(payload.get("turb_v", 0.0)),
        turb_st=str(payload.get("turb_st", "OFF")),
        level_pct=int(payload.get("level_pct", 0)),
        level_raw=int(payload.get("level_raw", 0)),
        level_v=float(payload.get("level_v", 0.0)),
        level_label=str(payload.get("level_label", "DRY")),
        level_st=str(payload.get("level_st", "OFF")),
        connected=int(payload.get("connected", 0)),
        active=int(payload.get("active", 0)),
    )
