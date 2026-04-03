#include "rtosSerial.h"

RTOSSerial rtosSerial;

#ifdef ESP32

// ── Mutex ────────────────────────────────────────────────────

void RTOSSerial::_ensureMtx() {
  if (_wMtx) return;  // fast path: already initialized
  static portMUX_TYPE initLock = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&initLock);
  if (!_wMtx) _wMtx = xSemaphoreCreateMutex();
  if (!_rMtx) _rMtx = xSemaphoreCreateMutex();
  taskEXIT_CRITICAL(&initLock);
}

void RTOSSerial::begin(unsigned long baud, size_t bufSize) {
  if (baud) Serial.begin(baud);
  _ensureMtx();
  if (!_buf) {
    _bufSize = bufSize;
    _buf = (uint8_t*)malloc(bufSize);
    if (!_buf) _bufSize = 0;
  }
}

void RTOSSerial::end() {
  if (_rxUp) { Serial.onReceive(nullptr); _rxUp = false; }
  if (_buf) { free(_buf); _buf = nullptr; _bufSize = 0; _head = 0; }
  if (_wMtx) { vSemaphoreDelete(_wMtx); _wMtx = nullptr; }
  if (_rMtx) { vSemaphoreDelete(_rMtx); _rMtx = nullptr; }
  for (int i = 0; i < _subCnt; i++) _subs[i].task = nullptr;
  _subCnt = 0;
}

// ── RX callback (event-driven, no task needed) ───────────────

void _rtosOnRx() {
  if (!rtosSerial._buf) return;
  xSemaphoreTake(rtosSerial._rMtx, portMAX_DELAY);
  while (Serial.available()) {
    uint8_t c = (uint8_t)Serial.read();
    rtosSerial._buf[rtosSerial._head % rtosSerial._bufSize] = c;
    rtosSerial._head++;
  }
  xSemaphoreGive(rtosSerial._rMtx);
}

void RTOSSerial::_startRx() {
  if (_rxUp) return;
  if (!_buf) begin();
  Serial.onReceive(_rtosOnRx);
  _rxUp = true;
}

// ── Subscriber management (must hold _rMtx) ─────────────────

int RTOSSerial::_sub() {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  for (int i = 0; i < _subCnt; i++)
    if (_subs[i].task == me) return i;
  if (_subCnt < MAX_SUB) {
    int i = _subCnt++;
    _subs[i] = { me, _head, _head };
    return i;
  }
  // MARK: reclaim slot from deleted/invalid task
  for (int i = 0; i < _subCnt; i++) {
    if (!_subs[i].task || eTaskGetState(_subs[i].task) == eDeleted) {
      _subs[i] = { me, _head, _head };
      return i;
    }
  }
  return -1;
}

// ── Write (mutex-protected) ──────────────────────────────────

size_t RTOSSerial::write(uint8_t c) {
  _ensureMtx();
  xSemaphoreTake(_wMtx, portMAX_DELAY);
  size_t n = Serial.write(c);
  xSemaphoreGive(_wMtx);
  return n;
}

size_t RTOSSerial::write(const uint8_t* buf, size_t size) {
  _ensureMtx();
  xSemaphoreTake(_wMtx, portMAX_DELAY);
  size_t n = Serial.write(buf, size);
  xSemaphoreGive(_wMtx);
  return n;
}

// ── Byte-level broadcast read (Stream interface) ─────────────

int RTOSSerial::available() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0) { xSemaphoreGive(_rMtx); return 0; }
  uint32_t behind = _head - _subs[i].byteCur;
  if (behind > _bufSize) { _subs[i].byteCur = _head - (uint32_t)_bufSize; behind = _bufSize; }
  xSemaphoreGive(_rMtx);
  return (int)behind;
}

int RTOSSerial::read() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0 || _subs[i].byteCur == _head) {
    xSemaphoreGive(_rMtx);
    return -1;
  }
  if (_head - _subs[i].byteCur > _bufSize) _subs[i].byteCur = _head - (uint32_t)_bufSize;
  uint8_t b = _buf[_subs[i].byteCur % _bufSize];
  _subs[i].byteCur++;
  xSemaphoreGive(_rMtx);
  return b;
}

int RTOSSerial::peek() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0 || _subs[i].byteCur == _head) {
    xSemaphoreGive(_rMtx);
    return -1;
  }
  if (_head - _subs[i].byteCur > _bufSize) _subs[i].byteCur = _head - (uint32_t)_bufSize;
  uint8_t b = _buf[_subs[i].byteCur % _bufSize];
  xSemaphoreGive(_rMtx);
  return b;
}

void RTOSSerial::flush() {
  _ensureMtx();
  xSemaphoreTake(_wMtx, portMAX_DELAY);
  Serial.flush();
  xSemaphoreGive(_wMtx);
}

// ── Bulk read (single mutex hold) ────────────────────────────

size_t RTOSSerial::readBytes(char* buffer, size_t length) {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0) { xSemaphoreGive(_rMtx); return 0; }
  uint32_t behind = _head - _subs[i].byteCur;
  if (behind > _bufSize) { _subs[i].byteCur = _head - (uint32_t)_bufSize; behind = _bufSize; }
  size_t n = (length < behind) ? length : behind;
  for (size_t j = 0; j < n; j++)
    buffer[j] = (char)_buf[(_subs[i].byteCur + j) % _bufSize];
  _subs[i].byteCur += n;
  xSemaphoreGive(_rMtx);
  return n;
}

// ── Broadcast line read (scans byte buffer for \n) ───────────
// MARK: readLine — pre-scan under lock, build String outside

String RTOSSerial::readLine() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0) { xSemaphoreGive(_rMtx); return ""; }

  if (_head - _subs[i].lineCur > _bufSize) _subs[i].lineCur = _head - (uint32_t)_bufSize;

  // skip leading CR/LF
  uint32_t pos = _subs[i].lineCur;
  while (pos != _head && (_buf[pos % _bufSize] == '\n' || _buf[pos % _bufSize] == '\r'))
    pos++;
  _subs[i].lineCur = pos;

  // scan for line content ending at next CR/LF
  uint32_t lineStart = pos;
  while (pos != _head && _buf[pos % _bufSize] != '\n' && _buf[pos % _bufSize] != '\r')
    pos++;

  uint32_t len = pos - lineStart;
  if (!len || pos == _head) { xSemaphoreGive(_rMtx); return ""; }

  // copy bytes to local buffer under mutex, then release
  uint8_t tmp[257];
  uint8_t* heap = nullptr;
  uint8_t* dst;
  if (len < sizeof(tmp)) {
    dst = tmp;
  } else {
    heap = (uint8_t*)malloc(len + 1);
    if (!heap) { xSemaphoreGive(_rMtx); return ""; }
    dst = heap;
  }
  for (uint32_t j = 0; j < len; j++) dst[j] = _buf[(lineStart + j) % _bufSize];
  dst[len] = '\0';

  _subs[i].lineCur = pos;
  xSemaphoreGive(_rMtx);

  // build String outside mutex — single allocation
  String line((const char*)dst);
  if (heap) free(heap);
  return line;
}

#endif // ESP32
