/* rdcp-modem-hal.h */

#ifndef _RDCP_MODEM_HAL
#define _RDCP_MODEM_HAL

#include <Arduino.h>

/**
 * @return Number of milliseconds since device start as int64_t
 */
int64_t my_millis(void);

/**
 * Set maximum CPU frequency. 
 */
void cpu_fast(void);

/**
 * Set power-conserving CPU frequency.
 */
void cpu_slow(void);

/**
 * Generate a random number between r_min and r_max.
 */
int64_t my_random_in_range(uint32_t r_min, uint32_t r_max);

/**
 * Restart the device
 */
void hal_device_restart(void);

/**
 * Check free RAM and restart device if necessary
 */
void hal_memory_check(void);

#if defined(USE_NRF52)
/**
 * Get free heap on nRF52 MCUs
 */
uint32_t nrf52_getFreeHeap(void);
#endif

#if defined(ESP32)
/**
 * Enter light sleep mode.
 */
void sleep_light(void);

/**
 * Enter deep sleep mode.
 */
void sleep_deep(void);

/**
 * Check the device's wake-up reason; perform required actions after wake-up from deep sleep.
 * @return true if woken from deep sleep, false otherwise.
 */
bool check_wakeup_reason(void);

/**
 * Restore pin configuration after wake-up.
 * @param woken_from_lightsleep true if we were in light sleep, false if we were in deep sleep
 */
void sleep_restore_after_wakeup(bool woken_from_lightsleep=OPTION_DISABLED);

#endif

#endif

/* EOF rdcp-modem-hal.h */