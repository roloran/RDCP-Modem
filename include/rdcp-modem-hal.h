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

#endif

/* EOF rdcp-modem-hal.h */