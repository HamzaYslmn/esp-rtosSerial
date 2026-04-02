#ifndef RTOS_SERIAL_H
#define RTOS_SERIAL_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * RTOSSerial — Thread-safe Serial for ESP32 FreeRTOS
 *
 *   rtosSerial.begin();                // optional (auto-inits)
 *   rtosSerial.printf("x=%d\n", x);   // safe from any task
 *   String cmd = rtosSerial.read();    // non-blocking line read
 *
 * Write: mutex-protected, safe from any core/task
 * Read:  single ring buffer, reader task starts on first read()
 */

#ifndef RTOS_RING_SIZE
#define RTOS_RING_SIZE 256
#endif

class RTOSSerial {
public:
  void begin(size_t ringSize = RTOS_RING_SIZE);

  // Write (thread-safe)
  void print(const char* s);
  void print(const String& s);
  void println(const char* s = "");
  void println(const String& s);
  void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

  // Read (non-blocking, line-based)
  String read();

private:
  SemaphoreHandle_t _mtx = nullptr;
  size_t _ringSize = RTOS_RING_SIZE;
  bool _readerUp = false;
  void _lock();
  void _unlock();
  void _startReader();
  friend void _rtosReaderTask(void*);
};

extern RTOSSerial rtosSerial;

#endif