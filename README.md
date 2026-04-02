# Esp32-RTOS-Serial

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue?logo=arduino)](https://github.com/HamzaYslmn/Esp32-RTOS-Serial)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Thread-safe Serial wrapper for ESP32 FreeRTOS. Inherits from `Stream` — full native Arduino API with mutex-protected I/O. **Broadcast reads** — every task sees every byte and every line.

## Quick Start

```cpp
#include <rtosSerial.h>

void setup() {
  Serial.begin(115200);
  rtosSerial.println("Ready");
}

void loop() {
  String cmd = rtosSerial.readLine();
  if (cmd.length())
    rtosSerial.printf("Got: %s\n", cmd.c_str());
  vTaskDelay(pdMS_TO_TICKS(10));
}
```

No `begin()` needed — auto-initializes on first use.

## API

Inherits from Arduino's `Print` class — all `print`/`println` overloads work automatically.

| Method | Description |
|--------|-------------|
| `rtosSerial.begin()` | Optional init (auto-inits on first use) |
| `rtosSerial.print(x)` | All `Print` overloads — int, float, String, char, etc. |
| `rtosSerial.println(x)` | All `Print` overloads + newline |
| `rtosSerial.printf(fmt, ...)` | Formatted output |
| `rtosSerial.write(buf, len)` | Raw byte output |
| `rtosSerial.readLine()` | Broadcast line read (returns `""` if no line, non-blocking) |

## How It Works

- **Write**: All output methods share one mutex — output from different tasks never interleaves
- **Read (broadcast)**: A background reader task (started on first read) reads Serial into two broadcast buffers: a **byte ring** (512B, for `read()`/`readBytes()`) and a **line ring** (8 slots, for `readLine()`). Each task gets its own cursor — **every task sees everything**. Up to 4 subscribers.
- **Lazy init**: Mutexes created on first use. Reader task created on first `read()` call.

## Multi-Task Broadcast Example

```cpp
#include <rtosSerial.h>

void sensorTask(void*) {
  for (;;) {
    // This task also sees every command typed
    String cmd = rtosSerial.readLine();
    if (cmd == "/start") rtosSerial.println("[sensor] Starting!");
    rtosSerial.printf("[sensor] heap=%lu\n", (unsigned long)ESP.getFreeHeap());
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 2048, NULL, 1, NULL, 0);
  rtosSerial.println("Type /start — both tasks will see it.");
}

void loop() {
  // Main loop also sees the same command
  String cmd = rtosSerial.readLine();
  if (cmd == "/start") rtosSerial.println("[main] Starting!");
  vTaskDelay(pdMS_TO_TICKS(10));
}
```

## Used By

- [esp32-tunnel](https://github.com/HamzaYslmn/esp32-tunnel) — thread-safe serial for tunnel CLI
- [espfetch](https://github.com/HamzaYslmn/espfetch) — thread-safe system info output

## License

MIT

## Author

**Hamza Yesilmen** — [@HamzaYslmn](https://github.com/HamzaYslmn)
