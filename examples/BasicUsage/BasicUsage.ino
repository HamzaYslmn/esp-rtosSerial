/*
 * BasicUsage.ino — esp-rtosSerial
 *
 * Thread-safe Serial from any task.
 * Type something and press Enter to echo back.
 */

#include <rtosSerial.h>

void setup() {
  rtosSerial.begin(115200);
  rtosSerial.println("Type something:");
}

void loop() {
  String cmd = rtosSerial.readLine();
  if (cmd.length()) {
    rtosSerial.printf("Echo: %s\n", cmd.c_str());
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}
