# esp-rtosSerial

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue?logo=arduino)](https://github.com/HamzaYslmn/Esp32-RTOS-Serial)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Thread-safe Serial wrapper for ESP32 and ESP8266. Inherits from `Stream` — full native Arduino API. **ESP32**: mutex-protected I/O, broadcast reads — every task sees every byte. **ESP8266**: thin passthrough wrapper for cross-platform compatibility.

## Quick Start

```cpp
#include <rtosSerial.h>

void setup() {
  rtosSerial.begin(115200);
  rtosSerial.println("Ready");
}

void loop() {
  String cmd = rtosSerial.readLine();
  if (cmd.length())
    rtosSerial.printf("Got: %s\n", cmd.c_str());
  vTaskDelay(pdMS_TO_TICKS(10));
}
```

`rtosSerial.begin(baud)` calls `Serial.begin()` internally. Omit baud to auto-initialize on first use (requires `Serial.begin()` separately).

## API

Inherits from Arduino's `Print` class — all `print`/`println` overloads work automatically.

| Method | Description |
|--------|-------------|
| `rtosSerial.begin(baud)` | Init Serial + ring buffer (or omit baud for lazy init) |
| `rtosSerial.end()` | Cleanup — frees buffer, detaches ISR, deletes mutexes |
| `rtosSerial.print(x)` | All `Print` overloads — int, float, String, char, etc. |
| `rtosSerial.println(x)` | All `Print` overloads + newline |
| `rtosSerial.printf(fmt, ...)` | Formatted output |
| `rtosSerial.write(buf, len)` | Raw byte output |
| `rtosSerial.readLine()` | Broadcast line read (returns `""` if no line, non-blocking) |

## How It Works

- **Write**: All output methods share one mutex — output from different tasks never interleaves
- **Read (broadcast)**: `Serial.onReceive()` callback reads incoming bytes into a single ring buffer (512B default). Each task gets its own read cursor and line cursor — **every task sees everything**. Up to 4 subscribers (dead-task slots are reclaimed automatically).
- **Lazy init**: Mutexes created on first use (thread-safe via spinlock). Ring buffer allocated on first `read()` call.
- **Note**: Don't mix `read()` and `readLine()` in the same task — they use independent cursors.

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
  rtosSerial.begin(115200);
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
