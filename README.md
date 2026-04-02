# Esp32-RTOS-Serial

Thread-safe Serial for ESP32 FreeRTOS. Mutex writes, ring buffer reads.

## Quick Start

```cpp
#include <rtosSerial.h>

void setup() {
  Serial.begin(115200);
  rtosSerial.println("Ready");
}

void loop() {
  String cmd = rtosSerial.read();
  if (cmd.length())
    rtosSerial.printf("Got: %s\n", cmd.c_str());
  vTaskDelay(pdMS_TO_TICKS(10));
}
```

No `begin()` needed — auto-initializes on first use.

## API

| Method | Description |
|--------|-------------|
| `rtosSerial.begin(ringSize)` | Optional init (default 256B ring buffer) |
| `rtosSerial.print(s)` | Thread-safe print |
| `rtosSerial.println(s)` | Thread-safe print + newline |
| `rtosSerial.printf(fmt, ...)` | Thread-safe printf |
| `rtosSerial.read()` | Non-blocking line read (returns `""` if empty) |

## How It Works

- **Write**: All `print`/`println`/`printf` share one mutex — output never interleaves
- **Read**: A background reader task starts on first `read()` call, reads lines from Serial into a ring buffer. `read()` returns the next buffered line or `""`.

## Multi-Task Example

```cpp
#include <rtosSerial.h>

void sensorTask(void*) {
  for (;;) {
    rtosSerial.printf("[sensor] heap=%lu\n", (unsigned long)ESP.getFreeHeap());
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void cmdTask(void*) {
  for (;;) {
    String cmd = rtosSerial.read();
    if (cmd == "ping") rtosSerial.println("pong!");
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(cmdTask,    "cmd",    2048, NULL, 1, NULL, 1);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
```

## License

MIT