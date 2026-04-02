#include "rtosSerial.h"

// ── State ────────────────────────────────────────────────────

struct _RtosBuf {
  TaskHandle_t   task;
  RingbufHandle_t ring;
};

static SemaphoreHandle_t _mtx = nullptr;
static _RtosBuf _bufs[RTOS_MAX_TASKS];
static volatile size_t _nBufs = 0;
static size_t _ringSize = RTOS_RING_SIZE;

// ── Reader task ──────────────────────────────────────────────

static void _readerTask(void*) {
  for (;;) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) { vTaskDelay(1); continue; }

      const char* raw = line.c_str();
      size_t len = line.length() + 1;   // include null terminator

      // Broadcast to all registered ring buffers
      xSemaphoreTake(_mtx, portMAX_DELAY);
      size_t n = _nBufs;
      xSemaphoreGive(_mtx);

      for (size_t i = 0; i < n; i++) {
        RingbufHandle_t rb = _bufs[i].ring;
        if (!rb) continue;
        if (xRingbufferSend(rb, raw, len, 0) != pdTRUE) {
          // Full — discard oldest, retry
          size_t discard_len;
          void* old = xRingbufferReceive(rb, &discard_len, 0);
          if (old) vRingbufferReturnItem(rb, old);
          xRingbufferSend(rb, raw, len, pdMS_TO_TICKS(5));
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ── Init ─────────────────────────────────────────────────────

void rtosSerialInit(size_t ringSize) {
  if (_mtx) return;                     // already initialized
  _ringSize = (ringSize < 64) ? RTOS_RING_SIZE : ringSize;
  _mtx = xSemaphoreCreateMutex();
  memset(_bufs, 0, sizeof(_bufs));
  xTaskCreatePinnedToCore(_readerTask, "rtos_sr", 3072, nullptr, 2, nullptr, 1);
}

// ── Per-task buffer registration ─────────────────────────────

static _RtosBuf* _getBuf() {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();

  xSemaphoreTake(_mtx, portMAX_DELAY);

  // Find existing
  for (size_t i = 0; i < _nBufs; i++) {
    if (_bufs[i].task == me) {
      xSemaphoreGive(_mtx);
      return &_bufs[i];
    }
  }

  // Register new
  if (_nBufs >= RTOS_MAX_TASKS) {
    xSemaphoreGive(_mtx);
    return nullptr;
  }
  _RtosBuf& b = _bufs[_nBufs];
  b.task = me;
  b.ring = xRingbufferCreate(_ringSize, RINGBUF_TYPE_NOSPLIT);
  if (!b.ring) { xSemaphoreGive(_mtx); return nullptr; }
  _nBufs++;
  xSemaphoreGive(_mtx);
  return &b;
}

// ── Read ─────────────────────────────────────────────────────

String rtosRead() {
  _RtosBuf* b = _getBuf();
  if (!b || !b->ring) return "";
  size_t len;
  char* item = (char*)xRingbufferReceive(b->ring, &len, 0);
  if (!item) return "";
  String s(item);
  vRingbufferReturnItem(b->ring, item);
  return s;
}

size_t rtosReadBytes(uint8_t* buf, size_t maxlen) {
  _RtosBuf* b = _getBuf();
  if (!b || !b->ring || !buf || !maxlen) return 0;
  size_t len;
  char* item = (char*)xRingbufferReceive(b->ring, &len, 0);
  if (!item) return 0;
  size_t n = (len < maxlen) ? len : maxlen;
  memcpy(buf, item, n);
  vRingbufferReturnItem(b->ring, item);
  return n;
}

// ── Write ────────────────────────────────────────────────────

void rtosPrint(const char* s) {
  if (!_mtx) return;
  xSemaphoreTake(_mtx, portMAX_DELAY);
  Serial.print(s);
  xSemaphoreGive(_mtx);
}

void rtosPrint(const String& s) {
  if (!_mtx) return;
  xSemaphoreTake(_mtx, portMAX_DELAY);
  Serial.print(s);
  xSemaphoreGive(_mtx);
}

void rtosPrintln(const char* s) {
  if (!_mtx) return;
  xSemaphoreTake(_mtx, portMAX_DELAY);
  Serial.println(s);
  xSemaphoreGive(_mtx);
}

void rtosPrintln(const String& s) {
  if (!_mtx) return;
  xSemaphoreTake(_mtx, portMAX_DELAY);
  Serial.println(s);
  xSemaphoreGive(_mtx);
}

void rtosPrintf(const char* fmt, ...) {
  if (!_mtx) return;
  xSemaphoreTake(_mtx, portMAX_DELAY);
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
  xSemaphoreGive(_mtx);
}