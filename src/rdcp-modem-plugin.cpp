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

extern device_config   cfg; // Device configuration
extern lora_message    current_lora_message; // Most recent incoming/processed LoRa message
extern rdcpv04_message current_rdcpv04_message; // Most recent incoming/processed RDCP v0.4 message
extern bool            current_rdcpv04_message_is_duplicate; // Duplicate status of current RDCP v0.4 message
extern uint8_t         channel_used_by_radio[MAX_NUMBER_OF_RADIOS]; // Mapping of radios to their currently used channels
extern txqueue         txq; // Global TX Queue

/**
 * Example plugin function: 
 * Automatically switch a single radio to the channel where we have a transmission queued.
 * Called periodically from plugin loop.
 */
void autoswitch_loop(void)
{
  /* Remember for the future from which channel we have auto-switched. */
  static uint8_t previous_original_channel = NO_CHANNEL;

  /* Buffer for serial information output about auto-switching */
  char autoswitch_info[INFOLEN];

  /* 
    Only automatically switch if the current device has a single radio. On devices with several LoRa
    radios, a more sophisticated approach should be used, especially if they are for different frequency ranges.
  */
  if (cfg.number_of_radios != 1) return;

  /* We may only need to switch if the device is configured for more than one channel. */
  if (cfg.number_of_channels <= 1) return;

  /* Which channel does our single radio use currently? */
  uint8_t currently_used_channel = channel_used_by_radio[RADIO0];

  /* Abort if the radio is not set up properly yet */
  if (currently_used_channel == NO_CHANNEL) return;
  if (!radio_get_has_radio(RADIO0)) return;

  /* Do not switch channel if we have transmissions upcoming on the currently used channel. */
  for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
  {
    if ((txq.entries[i].waiting == IS_WAITING) && (txq.entries[i].channel_to_send_on == currently_used_channel))
    {
      return; // we have a message to send on the current channel, do not switch
    }
  }

  /* Find the channel of the expected earliest next message to send by going through the TX queue. */
  int64_t earliest_timestamp = TIMESTAMP_ZERO;
  uint8_t channel_of_earliest = NO_CHANNEL;
  for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
  {
    if (txq.entries[i].waiting == IS_WAITING)
    {
      if ((earliest_timestamp == TIMESTAMP_ZERO) || (txq.entries[i].time_setting < earliest_timestamp))
      {
        earliest_timestamp = txq.entries[i].time_setting;
        channel_of_earliest = txq.entries[i].channel_to_send_on;
      }
    }
  }

  /* If we have found a message waiting on another channel, switch to it. */
  if (channel_of_earliest != NO_CHANNEL)
  {
    snprintf(autoswitch_info, INFOLEN, "INFO: Automatically switching radio %" PRIu8 " to channel %" PRIu8 " due to upcoming transmission",
      RADIO0, channel_of_earliest);
    serial_writeln(autoswitch_info);

    radio_switch_to_channel(RADIO0, channel_of_earliest, FORCED_CHANNEL_SWITCH);
    previous_original_channel = currently_used_channel;
  }
  else
  {
    /* No messages waiting in the queue. Switch back to the original channel if we had switched before. */
    if (previous_original_channel != NO_CHANNEL)
    {
      snprintf(autoswitch_info, INFOLEN, "INFO: Automatically switching radio %" PRIu8 " back to channel %" PRIu8,
        RADIO0, previous_original_channel);
      serial_writeln(autoswitch_info);

      radio_switch_to_channel(RADIO0, previous_original_channel, FORCED_CHANNEL_SWITCH);
      previous_original_channel = NO_CHANNEL;
    }
  }

  return;
}

bool plugin_serial(const char* line)
{
  /* Handle any additional Serial commands here. Return true if handled, false if not handled. */

  return false;
}

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
  /* Example: Automatically switch to channels where we have something to send. */
  autoswitch_loop();

  /* Handle any periodic tasks here. Called in each main loop. */

  return;
}

/* EOF rdcp-modem-plugin.cpp */