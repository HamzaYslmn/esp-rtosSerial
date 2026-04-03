#ifndef RTOS_SERIAL_H
#define RTOS_SERIAL_H

#include <Arduino.h>

#ifdef ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * RTOSSerial — Thread-safe Serial for ESP32 with broadcast reads
 *
 * Inherits from Stream → native Arduino API:
 *   read(), available(), peek(), readBytes(), readStringUntil(), etc.
 *
 * All I/O is mutex-protected. Reads are broadcast — every task sees
 * every byte via its own cursor into a shared ring buffer.
 *
 * No polling, no background task. Uses Serial.onReceive() for
 * zero-latency, event-driven byte capture.
 *
 *   rtosSerial.begin(115200);
 *   rtosSerial.println(3.14);              // thread-safe, any task
 *   String cmd = rtosSerial.readLine();    // broadcast, non-blocking
 *   int b = rtosSerial.read();             // broadcast, byte-level
 */

class RTOSSerial : public Stream {
public:
  void begin(unsigned long baud = 0, size_t bufSize = 512);
  void end();

  // Stream interface (broadcast reads, mutex-protected writes)
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;
  int    available() override;
  int    read() override;
  int    peek() override;
  void   flush() override;

  // Bulk read — single mutex hold (overrides Stream default)
  size_t readBytes(char* buffer, size_t length) override;

  // Non-blocking broadcast line read (returns "" if no complete line)
  String readLine();

  using Print::write;

private:
  SemaphoreHandle_t _wMtx = nullptr;
  SemaphoreHandle_t _rMtx = nullptr;
  bool _rxUp = false;

  // Single byte ring buffer
  uint8_t* _buf = nullptr;
  size_t   _bufSize = 512;
  volatile uint32_t _head = 0;

  // Per-task subscribers with independent cursors
  static const int MAX_SUB = 4;
  struct Sub {
    TaskHandle_t task;
    uint32_t byteCur;
    uint32_t lineCur;
  };
  Sub _subs[MAX_SUB];
  int _subCnt = 0;

  int  _sub();
  void _ensureMtx();
  void _startRx();
  friend void _rtosOnRx();
};

#else // ESP8266 — thin passthrough (single-threaded, no RTOS needed)

/*
 * RTOSSerial — ESP8266 passthrough wrapper
 *
 * Same API as the ESP32 version so user code compiles on both.
 * No mutexes (ESP8266 is single-threaded). readLine() is non-blocking.
 */

class RTOSSerial : public Stream {
public:
  void begin(unsigned long baud = 0, size_t = 512) { if (baud) Serial.begin(baud); }
  void end() { _linePos = 0; }

  size_t write(uint8_t c) override           { return Serial.write(c); }
  size_t write(const uint8_t* b, size_t s) override { return Serial.write(b, s); }
  int    available() override                { return Serial.available(); }
  int    read() override                     { return Serial.read(); }
  int    peek() override                     { return Serial.peek(); }
  void   flush() override                    { Serial.flush(); }

  String readLine() {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (_linePos) {
          String l(_lineBuf, _linePos);
          _linePos = 0;
          return l;
        }
        continue;
      }
      if (_linePos < sizeof(_lineBuf) - 1) _lineBuf[_linePos++] = c;
    }
    return "";
  }

  using Print::write;

private:
  char   _lineBuf[256];
  size_t _linePos = 0;
};

#endif

extern RTOSSerial rtosSerial;

#endif