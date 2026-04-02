#ifndef RTOS_SERIAL_H
#define RTOS_SERIAL_H

#include <Arduino.h>
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
 *   Serial.begin(115200);
 *   rtosSerial.println(3.14);              // thread-safe, any task
 *   String cmd = rtosSerial.readLine();    // broadcast, non-blocking
 *   int b = rtosSerial.read();             // broadcast, byte-level
 */

class RTOSSerial : public Stream {
public:
  void begin(size_t bufSize = 512);

  // Stream interface (broadcast reads, mutex-protected writes)
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;
  int    available() override;
  int    read() override;
  int    peek() override;
  void   flush() override;

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

extern RTOSSerial rtosSerial;

#endif