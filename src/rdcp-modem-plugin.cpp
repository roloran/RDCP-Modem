/* rdcp-modem-plugin.cpp */

#include <Arduino.h>
#include "rdcp-modem-plugin.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-rdcp-v04.h"
#include "rdcp-modem-scheduler.h"

bool plugin_serial(const char* line)
{
  /* Handle any additional Serial commands here. Return true if handled, false if not handled. */

  return false;
}

extern lora_message    current_lora_message;
extern rdcpv04_message current_rdcpv04_message;
extern bool            current_rdcpv04_message_is_duplicate;

void plugin_incoming(uint8_t lora_payload_type)
{
  /* Handle incoming LoRa packets based on their payload type here. */
  /* 
    The received LoRa packet is available as `current_lora_message`.
    If it is an RDCP v0.4 Message with a valid CRC, it is also available as `current_rdcpv04_message`.
    Amend further external variables if required.
  */

  return;
}

void plugin_callback(uint8_t radio, uint8_t channel, int cb_type, bool cad_result)
{
  /* Handle callbacks here. */

  if (cb_type == CALLBACK_TYPE_TXFIN)
  {

  }
  else if (cb_type == CALLBACK_TYPE_TXSTART)
  {

  }
  else if (cb_type == CALLBACK_TYPE_CADRES)
  {
    /* cad_result will be CAD_CHANNEL_FREE or CAD_CHANNEL_BUSY */
  }

  /* Note that no RX callback is provided for plugins currently. plugin_incoming() should be used. */

  return;
}

void plugin_loop(void)
{
  /* Handle any periodic tasks here. Called in each main loop. */

  return;
}

/* EOF rdcp-modem-plugin.cpp */