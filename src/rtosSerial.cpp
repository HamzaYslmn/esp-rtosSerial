#include "rtosSerial.h"

RTOSSerial rtosSerial;

// ── Mutex ────────────────────────────────────────────────────

void RTOSSerial::_ensureMtx() {
  if (!_wMtx) _wMtx = xSemaphoreCreateMutex();
  if (!_rMtx) _rMtx = xSemaphoreCreateMutex();
}

void RTOSSerial::begin(size_t bufSize) {
  _ensureMtx();
  if (!_buf) {
    _bufSize = bufSize;
    _buf = (uint8_t*)malloc(bufSize);
  }
}

// ── RX callback (event-driven, no task needed) ───────────────

void _rtosOnRx() {
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
  uint32_t oldest = (_head > _bufSize) ? _head - _bufSize : 0;
  if (_subs[i].byteCur < oldest) _subs[i].byteCur = oldest;
  int n = (int)(_head - _subs[i].byteCur);
  xSemaphoreGive(_rMtx);
  return n;
}

int RTOSSerial::read() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0 || _subs[i].byteCur >= _head) {
    xSemaphoreGive(_rMtx);
    return -1;
  }
  uint32_t oldest = (_head > _bufSize) ? _head - _bufSize : 0;
  if (_subs[i].byteCur < oldest) _subs[i].byteCur = oldest;
  uint8_t b = _buf[_subs[i].byteCur % _bufSize];
  _subs[i].byteCur++;
  xSemaphoreGive(_rMtx);
  return b;
}

int RTOSSerial::peek() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0 || _subs[i].byteCur >= _head) {
    xSemaphoreGive(_rMtx);
    return -1;
  }
  uint32_t oldest = (_head > _bufSize) ? _head - _bufSize : 0;
  if (_subs[i].byteCur < oldest) _subs[i].byteCur = oldest;
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

// ── Broadcast line read (scans byte buffer for \n) ───────────

String RTOSSerial::readLine() {
  _startRx();
  xSemaphoreTake(_rMtx, portMAX_DELAY);
  int i = _sub();
  if (i < 0) { xSemaphoreGive(_rMtx); return ""; }

  uint32_t oldest = (_head > _bufSize) ? _head - _bufSize : 0;
  if (_subs[i].lineCur < oldest) _subs[i].lineCur = oldest;

  uint32_t pos = _subs[i].lineCur;
  String line;
  while (pos < _head) {
    uint8_t c = _buf[pos % _bufSize];
    pos++;
    if (c == '\n' || c == '\r') {
      if (line.length()) {
        _subs[i].lineCur = pos;
        xSemaphoreGive(_rMtx);
        return line;
      }
      _subs[i].lineCur = pos;
      continue;
    }
    line += (char)c;
  }

  xSemaphoreGive(_rMtx);
  return "";
}
