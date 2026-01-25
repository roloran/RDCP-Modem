/* rdcp-modem-hal.cpp */

#include <Arduino.h>
#include "rdcp-modem-constants.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-serial.h"

extern device_config cfg;

#if defined(ESP32)
int32_t  old_free_heap     = ZERO_RAM;
int32_t  old_min_free_heap = ZERO_RAM;
int32_t  free_heap         = ZERO_RAM;
int32_t  min_free_heap     = ZERO_RAM;
char     hal_info[INFOLEN];

int64_t my_millis(void)
{
  return (int64_t) esp_timer_get_time() / MILLISECONDS_TO_MICROSECONDS;
}

void cpu_fast(void)
{
  setCpuFrequencyMhz(240);
  return;
}

void cpu_slow(void)
{
  if (!cfg.bt_enabled) setCpuFrequencyMhz(80);
  return;
}

void hal_device_restart(void)
{
  serial_writeln("INFO: Restarting device");
  ESP.restart();
  return;
}

void hal_memory_check(void)
{
  free_heap     = ESP.getFreeHeap();
  min_free_heap = ESP.getMinFreeHeap();

  if (old_free_heap == ZERO_RAM) old_free_heap = free_heap;
  if (old_min_free_heap == ZERO_RAM) old_min_free_heap = min_free_heap;

  if ((free_heap < old_free_heap) || (min_free_heap < old_min_free_heap))
  {
    snprintf(hal_info, INFOLEN, "WARNING: Free heap dropped from %" PRId32 "/%" PRId32 " to %" PRId32 "/%" PRId32, 
      old_min_free_heap, old_free_heap, min_free_heap, free_heap);
    serial_writeln(hal_info);

    old_min_free_heap = min_free_heap;
    old_free_heap     = free_heap;

    if (free_heap < MINIMUM_FREE_RAM)
    {
      serial_writeln("ERROR: OUT OF MEMORY - restarting as countermeasure");
      delay(1 * SECONDS_TO_MILLISECONDS);
      hal_device_restart();
    }
  }
  return;
}
#elif defined(USE_NRF52)
int64_t my_millis(void)
{
  return millis(); // recommended for nRF52 when using the Arduino framework
}

void cpu_fast(void)
{
  return; // CPU frequency cannot be changed on nRF52
}

void cpu_slow(void)
{
  return; // CPU frequency cannot be changed on nRF52 (sleeping recommended to reduce energy consumption)
}

void hal_device_restart(void)
{
  NVIC_SystemReset();
  return;
}

extern "C" {
  extern char __HeapLimit;
  extern char __StackTop;
}

#include <unistd.h>
uint32_t nrf52_getFreeHeap(void)
{
  char stack_dummy;
  char *stack_ptr = &stack_dummy;

  char *heap_end = (char*) sbrk(COUNT_ZERO);

  if (stack_ptr > heap_end) return (uint32_t)(stack_ptr - heap_end);
  return 0;
}

uint32_t old_free_heap     = ZERO_RAM;
uint32_t old_min_free_heap = ZERO_RAM;
uint32_t free_heap         = ZERO_RAM;
uint32_t min_free_heap     = ZERO_RAM;
char     hal_info[INFOLEN];

void hal_memory_check(void)
{
  free_heap = nrf52_getFreeHeap();
  if (old_free_heap == ZERO_RAM) old_free_heap = free_heap;

  if (free_heap < old_free_heap) 
  {
    snprintf(hal_info, INFOLEN, "WARNING: Free heap dropped from %" PRIu32 " to %" PRIu32, 
             old_free_heap, free_heap);
    serial_writeln(hal_info);

    if (free_heap < MINIMUM_FREE_RAM)
    {
      serial_writeln("ERROR: OUT OF MEMORY - restarting as countermeasure");
      delay(1 * SECONDS_TO_MILLISECONDS);
      hal_device_restart();
    }
  }
  return;
}
#else
/* Default implementation if no HAL-specific implementation available */
int64_t my_millis(void)
{
  return millis();
}

void cpu_fast(void)
{
  return;
}

void cpu_slow(void)
{
  return;
}

void hal_device_restart(void)
{
  serial_writeln("ERROR: Cannot restart this device (partial implementation)");
  return;
}

void hal_memory_check(void)
{
  return;
}
#endif

int64_t my_random_in_range(uint32_t r_min, uint32_t r_max)
{
  static bool srand_called = false;

  if (!srand_called)
  {
      srand_called = true;
      uint32_t my_seed = radio_random_byte() + 
                         (radio_random_byte() << 8) +   
                         (radio_random_byte() << 16) +   
                         (radio_random_byte() << 24);
      srand(my_seed);
  }

  int64_t result = rand() % (r_max - r_min + 1) + r_min;
  return result;
}

/* EOF rdcp-modem-hal.cpp */