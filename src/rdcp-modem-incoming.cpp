/* rdcp-modem-incoming.cpp */

#include <Arduino.h>
#include "rdcp-modem-serial.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-airmodem.h"
#include "rdcp-modem-persistence.h"
#include "rdcp-modem-incoming.h"
#include "rdcp-modem-rdcp-v04.h"
#include "rdcp-modem-plugin.h"
#include "rdcp-modem-scheduler.h"
#include "rdcp-modem-hal.h"

extern device_config cfg;
extern lora_channel_config lora_channel[];
extern radio_pinout pinout[];

extern controlled_air_radio air_radios_air_none[MAX_NUMBER_OF_RADIOS];
extern controlled_air_radio air_radios_air_one [MAX_NUMBER_OF_RADIOS];
extern controlled_air_radio air_radios_air_two [MAX_NUMBER_OF_RADIOS];
extern provided_air_radio   air_radios_provided[MAX_NUMBER_OF_RADIOS];
extern SX1262* sx1262_radios[MAX_NUMBER_OF_RADIOS];
extern SX1268* sx1268_radios[MAX_NUMBER_OF_RADIOS];
extern lora_message lora_queue_rx[MAX_NUMBER_OF_CHANNELS];
extern lora_message lora_queue_tx[MAX_NUMBER_OF_CHANNELS];

char incoming_info[INFOLEN];
lora_message current_lora_message;
rdcpv04_message current_rdcpv04_message;
bool current_rdcpv04_message_is_duplicate;

int64_t CFEst[MAX_NUMBER_OF_CHANNELS] = { CFEST_ZERO };
uint16_t rdcpv04_most_recent_airtime = AIRTIME_ZERO;
uint8_t  rdcpv04_most_recent_future_timeslots = TIMESLOTS_ZERO;

bool incoming_initialized = OPTION_DISABLED;

uint8_t incoming_determine_payload_type(void)
{
  if (current_lora_message.payload_length >= RDCPv04_HEADER_SIZE)
  {
    /* Copy the message to process into the target data structure */
    memcpy(&current_rdcpv04_message.header, &current_lora_message.payload, RDCPv04_HEADER_SIZE);
    for (int i=RDCPv04_HEADER_SIZE; i<current_lora_message.payload_length; i++) 
      current_rdcpv04_message.payload.data[i-RDCPv04_HEADER_SIZE] = current_lora_message.payload[i];
    
    /* Verify the CRC-16 checksum */
    if (rdcpv04_check_crc_in(current_lora_message.payload_length))
      return PAYLOAD_TYPE_RDCP_V04;
  }

  /* Handle further payload types here. */

  /* Default if we cannot determine a more specific payload type */
  return PAYLOAD_TYPE_GENERIC_LORA;
}

void incoming_handle_current_lora_message(void)
{
  if (!incoming_initialized)
  {
    incoming_initialized = OPTION_ENABLED;
    for (int i=COUNT_ZERO; i<MAX_NUMBER_OF_CHANNELS; i++) CFEst[i] = TIMESTAMP_ZERO;
  }

  snprintf(incoming_info, INFOLEN, "INFO: Processing LoRa packet received by radio %d on channel %d, RSSI %.2f, SNR %.1f, length %d bytes, timestamp %" PRId64, 
    (int) current_lora_message.radio, (int) current_lora_message.channel, 
    current_lora_message.rssi, current_lora_message.snr, 
    (int) current_lora_message.payload_length, current_lora_message.timestamp);
  serial_writeln(incoming_info);

  /* Determine LoRa payload type */
  uint8_t lora_payload_type = incoming_determine_payload_type();

  /* Basic treatment of RDCP v0.4 Messages */
  if (lora_payload_type == PAYLOAD_TYPE_RDCP_V04)
  {
    if (cfg.print_rdcpcsv_lines) rdcpv04_print_csv();
    
    /* Check the RDCP Message duplicate status */
    current_rdcpv04_message_is_duplicate = rdcpv04_check_duplicate_message(current_rdcpv04_message.header.origin, current_rdcpv04_message.header.sequence_number);

    /* Update Channel Free Estimator based on RDCP Message properties */
    rdcpv04_update_cfest_rx();

    /* Stop any TX events on the current channel as long as it is busy */
    scheduler_stop_and_reschedule_on_busy_channel(current_lora_message.channel);

    /* Process selected incoming RDCP v0.4 Messages */
    rdcpv04_process_incoming_message();
  }
  else 
  {
    CFEst[current_lora_message.channel] = my_millis();
  }

  // Also let plugins handle the message
  plugin_incoming(lora_payload_type);

  return;
}

bool incoming_loop(void)
{
  for (uint8_t channel_id = CHANNEL0; channel_id < cfg.number_of_channels; channel_id++)
  {
    if (lora_queue_rx[channel_id].available)
    {
      memcpy(&current_lora_message, &lora_queue_rx[channel_id], sizeof(lora_message));
      lora_queue_rx[channel_id].available = false;
      incoming_handle_current_lora_message();
      return true; // Only handle one received LoRa packet at a time
    }
  }
  return false;
}

/* EOF rdcp-modem-incoming.cpp */