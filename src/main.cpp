/* main.cpp */

/*
  ROLORAN RDCP-Modem Implementation
*/

#include <Arduino.h>
#include "rdcp-modem-serial.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-airmodem.h"
#include "rdcp-modem-persistence.h"
#include "rdcp-modem-incoming.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-scheduler.h"
#include "rdcp-modem-plugin.h"

extern device_config cfg;

#if defined(ESP32)
SET_LOOP_TASK_STACK_SIZE(DEVICE_STACK_SIZE);
#endif

char main_info[INFOLEN];
char serial_input_line[SERIALINPUTLEN];

void setup() 
{
  serial_setup();                   // Set up the primary Serial/UART connection
  for (int i=COUNT_ZERO; i <= PRE_START_DELAY_NUM; i++)
  {
    snprintf(main_info, INFOLEN, "INIT: Pre-start delay %d/%d", i, PRE_START_DELAY_NUM);
    serial_writeln(main_info);
    delay(PRE_START_DELAY_TIME);
  }

  persistence_setup();              // Set up persistence

  if (cfg.init_radios_on_start)
  {
    serial_writeln("INIT: Setting up radios");
    lora_hardware_setup();          // Set up the used radio hardware
    radio_setup();                  // Initialize RadioLib and channel settings
    cfg.radios_initialized = true;
  }
  else if (!cfg.radios_initialized)
  {
    serial_writeln("INIT: Manual initialization of radios required!");
  }

  serial_banner();                  // Show current device configuration over Serial
  serial_writeln("READY");          // Signal LoRa modem readiness

  return;
}

void loop() 
{
  delay(MINIMUM_DELAY);             // For background tasks such as watchdogs

  if (cfg.radios_initialized)
  {
    radio_loop();                   // Periodically let the LoRa radios do their work
    air_loop();                     // Periodically let the AIR radios do their work
    incoming_loop();                // Periodically process received LoRa packets
    scheduler_loop();               // Periodically let the TX scheduler do its work
  }

  /* Check for and process any Serial0 input */
  serial_readln(serial_input_line, SERIALINPUTLEN);
  if (serial_input_line[FIRST_BYTE_IN_ARRAY] != ZEROBYTE)
  {
    serial_process_command(serial_input_line, DEFAULT_SERIAL_PREFIX);
  }

  plugin_loop();                   // Peridically let any plugins do their work

  hal_memory_check();              // Restart device if running out of memory

  return;
}

/* EOF main.cpp */