#ifndef RTOS_SERIAL_H
#define RTOS_SERIAL_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

/*
 * Thread-Safe Serial for ESP32 FreeRTOS (v1.0.0)
 * -----------------------------------------------
 * Write: mutex-protected — safe from any task/core
 * Read:  per-task ring buffer — each task gets its own copy
 *        background reader broadcasts Serial input to all
 *
 * Usage:
 *   rtosSerialInit();           // call once in setup()
 *   rtosPrintf("val=%d\n", x);  // write from any task
 *   String cmd = rtosRead();    // non-blocking read
 */

#ifndef RTOS_MAX_TASKS
#define RTOS_MAX_TASKS    8       // max concurrent reading tasks
#endif
#ifndef RTOS_RING_SIZE
#define RTOS_RING_SIZE    256     // default ring buffer per task
#endif

// Init — call once in setup() after Serial.begin()
void rtosSerialInit(size_t ringSize = RTOS_RING_SIZE);

// Write (thread-safe, mutex-protected)
void rtosPrint(const char* s);
void rtosPrint(const String& s);
void rtosPrintln(const char* s);
void rtosPrintln(const String& s);
void rtosPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Read (non-blocking, per-task ring buffer)
String rtosRead();
size_t rtosReadBytes(uint8_t* buf, size_t maxlen);

#endif