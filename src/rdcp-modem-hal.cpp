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

#if defined(ESP32)
#include <rom/rtc.h>
#include <driver/rtc_io.h>
extern radio_pinout pinout[];

void sleep_prepare(void)
{
  uint64_t wakeup_bitmask = 0;

  for (int i=0; i<cfg.number_of_radios; i++)
  {
    if (pinout[i].radio_type == RADIO_TYPE_SX1262 || pinout[i].radio_type == RADIO_TYPE_SX1268)
    {
      wakeup_bitmask += pow(2, pinout[i].lora_dio1);
    	rtc_gpio_pulldown_en((gpio_num_t) pinout[i].lora_dio1);
	    rtc_gpio_pullup_en((gpio_num_t)   pinout[i].lora_reset);
	    rtc_gpio_pullup_en((gpio_num_t)   pinout[i].spi_cs);
    }
  }

  if (cfg.auto_wake > 0)
  {
    esp_sleep_enable_timer_wakeup(cfg.auto_wake * (uint64_t)1000); // needs microseconds
  }

  if (cfg.additional_wakeup_pin != PIN_NOT_USED)
  {
    wakeup_bitmask += pow(2, cfg.additional_wakeup_pin);
  }

	esp_sleep_enable_ext1_wakeup(wakeup_bitmask, ESP_EXT1_WAKEUP_ANY_HIGH);

  return;
}

int sleep_restore_once_only = COUNT_ZERO;

void sleep_restore_after_wakeup(bool woken_from_lightsleep)
{ 
  if (sleep_restore_once_only > 0) return; // only run this function once after wake-up, even if called multiple times
  sleep_restore_once_only++;

  uint64_t bitmask = esp_sleep_get_ext1_wakeup_status();

  snprintf(hal_info, INFOLEN, "INFO: Wake-up procedure, number of radios: %d, wake-up bitmask %" PRIu64, cfg.number_of_radios, bitmask);
  serial_writeln(hal_info);

  for (int i=0; i<cfg.number_of_radios; i++)
  {
    if (pinout[i].radio_type == RADIO_TYPE_SX1262 || pinout[i].radio_type == RADIO_TYPE_SX1268)
    {
      snprintf(hal_info, INFOLEN, "INFO: Performing wake-up pin configuration for radio %d", i);
      serial_writeln(hal_info);
    	rtc_gpio_pulldown_dis((gpio_num_t) pinout[i].lora_dio1);
	    rtc_gpio_pullup_dis((gpio_num_t) pinout[i].spi_cs);
      if (cfg.disengage_reset_pin) rtc_gpio_pullup_dis((gpio_num_t) pinout[i].lora_reset);
      if (bitmask & (1ULL << pinout[i].lora_dio1)) 
      { 
        cfg.waking_radio = i;
        pinout[i].lora_reset = RADIOLIB_NC; // apparently too late, better in radio module injection
      }
    }
  }

  if (woken_from_lightsleep)
  {
    serial_writeln("INFO: Woke up from light sleep");
  }
  else
  {
    serial_writeln("INFO: Wake-up from deep sleep complete");
  }
 
  return;
}

bool check_wakeup_reason(void)
{ 
	RESET_REASON cpu0WakeupReason = rtc_get_reset_reason(0);
	RESET_REASON cpu1WakeupReason = rtc_get_reset_reason(1);

  if ((cpu0WakeupReason == DEEPSLEEP_RESET) || (cpu1WakeupReason == DEEPSLEEP_RESET))
	{ /* We woke from deep sleep. Don't fully initialize. */
    cfg.woken_from_deep_sleep = OPTION_ENABLED;
    if (cfg.toggle_spi_on_sleep) SPI.begin();
    return true;
  }

  return false;
}

void sleep_light(void)
{
  serial_writeln("INFO: Entering ESP32 light sleep mode");
  sleep_restore_once_only = COUNT_ZERO;
  sleep_prepare();
  esp_light_sleep_start(); // returns on wake-up
  sleep_restore_after_wakeup(OPTION_ENABLED);
  return;
}

/**
 * Enter deep sleep mode.
 */
void sleep_deep(void)
{
  serial_writeln("INFO: Entering ESP32 deep sleep mode");
  sleep_restore_once_only = COUNT_ZERO;
  sleep_prepare();
  if (cfg.toggle_spi_on_sleep) SPI.end();
	esp_deep_sleep_start(); // never returns; system restarts on wake-up
  return;
}
#endif

/* EOF rdcp-modem-hal.cpp */