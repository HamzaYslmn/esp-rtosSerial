/*
 * MultiTask.ino — Esp32-RTOS-Serial
 *
 * Two tasks + loop all printing safely.
 * Type "ping" for a pong from Task 2.
 */

#include <rtosSerial.h>

void sensorTask(void*) {
  for (;;) {
    rtosSerial.printf("[sensor] heap=%lu uptime=%lus\n",
      (unsigned long)ESP.getFreeHeap(), millis() / 1000);
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void cmdTask(void*) {
  for (;;) {
    String cmd = rtosSerial.read();
    if (cmd == "ping") rtosSerial.println("[cmd] pong!");
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(sensorTask, "sensor", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(cmdTask,    "cmd",    2048, NULL, 1, NULL, 1);

  rtosSerial.println("Ready. Type 'ping' for pong.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
