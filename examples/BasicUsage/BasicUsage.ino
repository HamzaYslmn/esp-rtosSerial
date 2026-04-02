/*
 * BasicUsage.ino — Esp32-RTOS-Serial
 *
 * Thread-safe Serial from any task.
 * Type something and press Enter to echo back.
 */

#include <rtosSerial.h>

void setup() {
  Serial.begin(115200);
  rtosSerialInit();
  rtosPrintln("Type something:");
}

void loop() {
  String cmd = rtosRead();
  if (cmd.length()) {
    rtosPrintf("Echo: %s\n", cmd.c_str());
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}
