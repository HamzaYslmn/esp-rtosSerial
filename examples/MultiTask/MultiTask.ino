/*
 * MultiTask.ino — Esp32-RTOS-Serial broadcast demo
 *
 * Three tasks all calling read() — each sees every command.
 * Type /ping, /status, or /help to see broadcast in action.
 *
 * Also demonstrates: printf, println(int), println(float), write().
 */

#include <rtosSerial.h>

// ── Task 1: responds to /ping ────────────────────────────────

void pingTask(void*) {
  rtosSerial.println("[ping] Task ready on core 0");
  for (;;) {
    String cmd = rtosSerial.readLine();
    if (cmd == "/ping") {
      rtosSerial.printf("[ping] pong! (heap=%lu)\n", (unsigned long)ESP.getFreeHeap());
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ── Task 2: responds to /status, periodic heartbeat ──────────

void statusTask(void*) {
  rtosSerial.println("[status] Task ready on core 1");
  uint32_t beat = 0;
  for (;;) {
    String cmd = rtosSerial.readLine();
    if (cmd == "/status") {
      rtosSerial.print("[status] uptime=");
      rtosSerial.print(millis() / 1000);
      rtosSerial.print("s heap=");
      rtosSerial.print((int)ESP.getFreeHeap());
      rtosSerial.print(" beats=");
      rtosSerial.println(beat);
    }
    if (++beat % 600 == 0) {
      rtosSerial.println("[status] heartbeat");
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ── Setup ────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(pingTask,   "ping",   2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(statusTask, "status", 2048, NULL, 1, NULL, 1);

  rtosSerial.println("=== Broadcast Demo ===");
  rtosSerial.println("Commands: /ping /status /help");
}

// ── Main loop: also a subscriber ─────────────────────────────

void loop() {
  String cmd = rtosSerial.readLine();
  if (cmd.length()) {
    if (cmd == "/help") {
      rtosSerial.println("[main] /ping   — pong from ping task");
      rtosSerial.println("[main] /status — stats from status task");
      rtosSerial.println("[main] All tasks see every command (broadcast)");
    } else if (cmd == "/ping" || cmd == "/status") {
      rtosSerial.printf("[main] saw '%s' too\n", cmd.c_str());
    } else {
      rtosSerial.printf("[main] unknown: %s\n", cmd.c_str());
    }
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}
