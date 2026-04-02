#include "rtosSerial.h"
#include <freertos/ringbuf.h>

RTOSSerial rtosSerial;

static RingbufHandle_t _ring = nullptr;

// ── Reader task (created on first read()) ────────────────────

void _rtosReaderTask(void*) {
  for (;;) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.length()) {
        const char* s = line.c_str();
        size_t len = line.length() + 1;
        if (xRingbufferSend(_ring, s, len, 0) != pdTRUE) {
          size_t dl;
          void* old = xRingbufferReceive(_ring, &dl, 0);
          if (old) vRingbufferReturnItem(_ring, old);
          xRingbufferSend(_ring, s, len, pdMS_TO_TICKS(5));
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ── Init ─────────────────────────────────────────────────────

void RTOSSerial::begin(size_t ringSize) {
  _ringSize = (ringSize < 64) ? RTOS_RING_SIZE : ringSize;
  if (!_mtx) _mtx = xSemaphoreCreateMutex();
}

void RTOSSerial::_lock() {
  if (!_mtx) begin();
  xSemaphoreTake(_mtx, portMAX_DELAY);
}

void RTOSSerial::_unlock() {
  xSemaphoreGive(_mtx);
}

void RTOSSerial::_startReader() {
  if (_readerUp) return;
  if (!_mtx) begin();
  _ring = xRingbufferCreate(_ringSize, RINGBUF_TYPE_NOSPLIT);
  xTaskCreatePinnedToCore(_rtosReaderTask, "rtos_serial", 3072, nullptr, 2, nullptr, 1);
  _readerUp = true;
}

// ── Write ────────────────────────────────────────────────────

void RTOSSerial::print(const char* s) {
  _lock(); Serial.print(s); _unlock();
}

void RTOSSerial::print(const String& s) {
  _lock(); Serial.print(s); _unlock();
}

void RTOSSerial::println(const char* s) {
  _lock(); Serial.println(s); _unlock();
}

void RTOSSerial::println(const String& s) {
  _lock(); Serial.println(s); _unlock();
}

void RTOSSerial::printf(const char* fmt, ...) {
  _lock();
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
  _unlock();
}

// ── Read ─────────────────────────────────────────────────────

String RTOSSerial::read() {
  _startReader();
  size_t len;
  char* item = (char*)xRingbufferReceive(_ring, &len, 0);
  if (!item) return "";
  String s(item);
  vRingbufferReturnItem(_ring, item);
  return s;
}