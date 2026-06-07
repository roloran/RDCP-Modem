/* rdcp-modem-scheduler.cpp */

#include <Arduino.h> 
#include "rdcp-modem-scheduler.h"
#include "rdcp-modem-rdcp-v04.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-rdcp-v04.h"
#include "Base64ren.h"

txqueue txq; //< Global TX Queue

extern lora_message current_lora_message;
extern device_config cfg;
extern int64_t CFEst[MAX_NUMBER_OF_CHANNELS]; 
extern uint16_t rdcpv04_most_recent_airtime;
extern uint8_t  rdcpv04_most_recent_future_timeslots;
extern lora_channel_config lora_channel[];
extern lora_message lora_queue_tx[MAX_NUMBER_OF_CHANNELS];
extern uint8_t channel_used_by_radio[MAX_NUMBER_OF_RADIOS];

int tx_ongoing[MAX_NUMBER_OF_CHANNELS] = { RDCP_INDEX_NONE };          //< Index of TXQ entry currently up for transmission
int64_t tx_process_start[MAX_NUMBER_OF_CHANNELS] = { ZERO_TIMESTAMP }; //< Timestamp of when TX process started
int64_t tx_start[MAX_NUMBER_OF_CHANNELS] = { ZERO_TIMESTAMP };         //< Timestamp of when TX started
int64_t tx_latency[MAX_NUMBER_OF_CHANNELS] = { ZERO_TIMESTAMP };       //< Message sending latency
int retransmission_count[MAX_NUMBER_OF_CHANNELS] = { COUNT_ZERO };     //< Retransmission counter for current message
int64_t last_tx_activity[MAX_NUMBER_OF_CHANNELS] = { ZERO_TIMESTAMP }; //< Timestamp of last TX scheduler activity

char sched_info[INFOLEN];
char long_sched_info[LONGINFOLEN];

bool scheduler_initialized = OPTION_DISABLED;

/*
  The scheduler loops over all radios and the channels they are currently on.
  It starts the transmission of entries that should meanwhile be sent.
  Note that outgoing messages on channels with no radio switched to are not touched;
  outgoing messages queued on such channels may fill up the queue.
*/
bool scheduler_loop(void)
{
  int64_t now = my_millis();

  if (!scheduler_initialized)
  {
    scheduler_initialized = OPTION_ENABLED;
    for (int i=COUNT_ZERO; i<MAX_NUMBER_OF_CHANNELS; i++)
    {
      tx_ongoing[i]           = RDCP_INDEX_NONE;
      tx_process_start[i]     = ZERO_TIMESTAMP;
      tx_start[i]             = ZERO_TIMESTAMP;
      tx_latency[i]           = ZERO_TIMESTAMP;
      retransmission_count[i] = COUNT_ZERO;
      last_tx_activity[i]     = ZERO_TIMESTAMP;
    }
  }

  for (int radio_id=COUNT_ZERO; radio_id < cfg.number_of_radios; radio_id++)
  {
    uint8_t channel_id = channel_used_by_radio[radio_id];
    if (channel_id == NO_CHANNEL) continue;

    /* Skip if any transmission is already ongoing */
    if (tx_ongoing[channel_id] != RDCP_INDEX_NONE)
    {
      if (now - last_tx_activity[channel_id] > TX_TIMEOUT)
      {
        snprintf(sched_info, INFOLEN, "WARNING: TX Activity Timeout on radio %d, channel %d, restarting TXQ processing",
          (int) radio_id, (int) channel_id);
        serial_writeln(sched_info);
        txq.entries[tx_ongoing[channel_id]].in_process = NOT_IN_PROCESS;
        txq.entries[tx_ongoing[channel_id]].cad_retry = COUNT_ZERO;
        tx_ongoing[channel_id] = RDCP_INDEX_NONE;
        radio_switch_to_channel(radio_id, channel_id, FORCED_CHANNEL_SWITCH);
      }
      continue;
    }
  
    last_tx_activity[channel_id] = now;
    int found_entry = RDCP_INDEX_NONE;
  
    /* Look for hard-scheduled entries first */
    for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
    {
      /* Look for active entries only */
      if (txq.entries[i].waiting == IS_NOT_WAITING) continue;

      /* Look for entries for current channel only */
      if (txq.entries[i].channel_to_send_on != channel_id) continue;

      if (txq.entries[i].scheduling_mode == SCHEDULING_MODE_FIXED_TIME)
      {
        if (txq.entries[i].time_setting <= now)
        { // found a hard-scheduled entry that should be sent ASAP
          if (found_entry == RDCP_INDEX_NONE)
          { // First found
            found_entry = i;
          }
          else 
          { // Not first found, use this one if scheduled for earlier transmission
            if (txq.entries[i].time_setting < txq.entries[found_entry].time_setting)
            {
              found_entry = i;
            }
          }
        }
      }
    }

    /* If we did not find a hard-scheduled entry, look for other entries if channel is free */
    if ((found_entry == RDCP_INDEX_NONE) && (now >= CFEst[channel_id]))
    {
      for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
      {
        /* Look for active entries only */
        if (txq.entries[i].waiting == IS_NOT_WAITING) continue;
        /* Look for entries for current channel only */
        if (txq.entries[i].channel_to_send_on != channel_id) continue;

        if (txq.entries[i].scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE)
        {
          if (found_entry == RDCP_INDEX_NONE)
          { // First found
            found_entry = i;
          }
          else 
          { // Not first found, use this one if it was scheduled earlier
            if (txq.entries[i].time_setting < txq.entries[found_entry].time_setting)
            {
              found_entry = i;
            }
          }
        }
      }
    }

    /* Nothing to do on this channel if we have not found an entry to send */
    if (found_entry == RDCP_INDEX_NONE) continue;

    /* Prepare for processing the found entry */
    tx_ongoing[channel_id] = found_entry;

    txq.entries[found_entry].in_process = true;
    tx_process_start[channel_id] = now;
  
    snprintf(sched_info, INFOLEN, "INFO: Outgoing message up for send-processing -> TXQ%02d i%d %s, len %d, TSd %d, @%d, =%d",
         (int) channel_id, 
         (int) found_entry,
         txq.entries[found_entry].scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE ? "cf" : "hard",
         (int) txq.entries[found_entry].payload_length, 
         (int) txq.entries[found_entry].timeslot_duration, 
         (int) txq.entries[found_entry].time_setting, 
         (int) now);
    serial_writeln(sched_info);

    if (txq.entries[found_entry].scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE)
    {
      txq.entries[found_entry].in_cad_mode = IN_CAD_MODE;
      scheduler_send_message_cad(radio_id, channel_id);
    }
    else
    {
      txq.entries[found_entry].in_cad_mode = NOT_IN_CAD_MODE;
      scheduler_send_message_force(radio_id, channel_id);
    }

    return true; // only start transmitting one message per scheduler loop
  } // loop over all radios

  return false;
}

/* 
  We stop transmissions after RDCP Message reception only if the
  the outgoing message on the affected channel is currently in CAD mode.
  If it is not in CAD mode, we have to send it even if the channel 
  may be busy due to another RDCP Message to not lose synchronization.
  This is also relevant if we have multiple radios on the same channel 
  and one of our radio receives a message sent by one of our other radios.  
*/
void scheduler_stop_and_reschedule_on_busy_channel(uint8_t channel_id)
{
  if (channel_id >= cfg.number_of_channels)
  {
    serial_writeln("WARNING: Invalid channel for scheduler stop on busy channel");
    return;
  } 

  if (tx_ongoing[channel_id] != RDCP_INDEX_NONE)
  {
    if (txq.entries[tx_ongoing[channel_id]].in_cad_mode == IN_CAD_MODE)
    {
      txq.entries[tx_ongoing[channel_id]].in_cad_mode = NOT_IN_CAD_MODE;
      txq.entries[tx_ongoing[channel_id]].cad_retry = COUNT_ZERO;
      txq.entries[tx_ongoing[channel_id]].in_process = NOT_IN_PROCESS;
      snprintf(sched_info, INFOLEN, "INFO: Postponing transmission of TXQ%02d i%d due to busy channel", 
        (int) channel_id, (int) tx_ongoing[channel_id]);
      serial_writeln(sched_info);
      tx_ongoing[channel_id] = RDCP_INDEX_NONE;
    }
  }

  return;
}

void scheduler_send_message_cad(uint8_t radio_id, uint8_t channel_id)
{
  snprintf(sched_info, INFOLEN, "INFO: Starting to send with CAD first, radio %d, channel %d", 
    (int) radio_id, (int) channel_id);
  serial_writeln(sched_info);
  radio_start_cad(radio_id, channel_id);
  // sending process continues when scheduler receives CAD callback
  return;
}

void scheduler_send_message_force(uint8_t radio_id, uint8_t channel)
{
  int64_t now = my_millis();
  int64_t timediff = now - 
                     txq.entries[tx_ongoing[channel]].time_setting - 
                     retransmission_count[channel] * 
                          (airtime_in_ms(channel, txq.entries[tx_ongoing[channel]].payload_length) + 
                          RDCPv04_TIMESLOT_BUFFERTIME);

  snprintf(sched_info, INFOLEN, "INFO: TXStart for TXQ%d i%d, len %d, TSd %d ms, latency %d ms", 
      (int) channel, 
      (int) tx_ongoing[channel], 
      (int) txq.entries[tx_ongoing[channel]].payload_length, 
      (int) txq.entries[tx_ongoing[channel]].timeslot_duration, 
      (int) timediff);
  serial_writeln(sched_info);
  
  if (cfg.print_rxmeta_lines)
  {
    snprintf(sched_info, INFOLEN, "TXMETA %d %d %3.3f", 
             (int) txq.entries[tx_ongoing[channel]].payload_length, 
             (int) now, 
             lora_channel[channel].frequency_in_mhz);
    serial_writeln(sched_info);
  }
  
  if (cfg.print_rx_lines)
  {
    int encodedLength = Base64ren.encodedLength(txq.entries[tx_ongoing[channel]].payload_length);
    char b64msg[encodedLength + 1];
    Base64ren.encode(b64msg, (char *) txq.entries[tx_ongoing[channel]].payload, 
                     txq.entries[tx_ongoing[channel]].payload_length);
  
    snprintf(long_sched_info, LONGINFOLEN, "TX %s", b64msg);
    serial_writeln(long_sched_info);
  }

  tx_start[channel] = my_millis();
  tx_latency[channel] = timediff > TIMESTAMP_ZERO ? timediff : TIMESTAMP_ZERO;

  radio_send_message_binary(radio_id, channel, txq.entries[tx_ongoing[channel]].payload, 
                            txq.entries[tx_ongoing[channel]].payload_length);
  
  if (txq.entries[tx_ongoing[channel]].payload_type == PAYLOAD_TYPE_RDCP_V04)
  {
    rdcpv04_update_cfest_tx(channel);
  }

  txq.entries[tx_ongoing[channel]].in_cad_mode = NOT_IN_CAD_MODE;

  return; 
}

void scheduler_callback_txfin(uint8_t radio_to_use, uint8_t channel)
{
  last_tx_activity[channel] = my_millis();

  int num_waiting = RDCP_INDEX_NONE;
  int num_waiting_on_same_channel = RDCP_INDEX_NONE;
  for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
  { 
    if (txq.entries[i].waiting) 
    { 
      num_waiting++;
      if (txq.entries[i].channel_to_send_on == channel) num_waiting_on_same_channel++;
    }
  }
  
  int num_retransmissions = COUNT_ZERO;
  rdcpv04_message rm;

  if (txq.entries[tx_ongoing[channel]].payload_type == PAYLOAD_TYPE_RDCP_V04)
  {
    memcpy(&rm.header, txq.entries[tx_ongoing[channel]].payload, RDCPv04_HEADER_SIZE);
    for (int i=COUNT_ZERO; i < rm.header.rdcp_payload_length; i++) 
        rm.payload.data[i] = txq.entries[tx_ongoing[channel]].payload[RDCPv04_HEADER_SIZE + i];
    num_retransmissions = rm.header.counter;
  }
  
  snprintf(sched_info, INFOLEN, "INFO: TXFIN 4 TXQ%d i%d, %d retransmissions ahead, %d/%d more messages waiting", 
        (int) channel, 
        (int) tx_ongoing[channel], 
        (int) num_retransmissions, 
        (int) num_waiting_on_same_channel, 
        (int) num_waiting);
  serial_writeln(sched_info);
  
  txq.entries[tx_ongoing[channel]].cad_retry = COUNT_ZERO;
  
  /* Handle RDCP v0.4 retransmissions */
  if ((txq.entries[tx_ongoing[channel]].payload_type == PAYLOAD_TYPE_RDCP_V04) && (num_retransmissions > COUNT_ZERO))
  { 
    rm.header.counter -= 1;
  
    uint8_t data_for_crc[INFOLEN];
    memcpy(&data_for_crc, &rm.header, RDCPv04_HEADER_SIZE - 2);
    for (int i=COUNT_ZERO; i < rm.header.rdcp_payload_length; i++) 
      data_for_crc[i + RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE] = rm.payload.data[i];
    uint16_t actual_crc = crc16_rdcpv04(data_for_crc, RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE + rm.header.rdcp_payload_length);
    rm.header.checksum = actual_crc;
    memcpy(txq.entries[tx_ongoing[channel]].payload, &rm.header, RDCPv04_HEADER_SIZE); // no need to overwrite RDCP payload
  
    retransmission_count[channel]++;

    /* 
      The timestamp for sending the next retransmission from plain RDCP specs is the
      timestamp of the previous transmission + its airtime + the 1000 ms buffer time. 
      However, we schedule the retransmission a bit earlier due to processing time, 
      approximated by a constant value for now. Additionally, we consider the latency
      accumulated before the previous transmission with an upper bound of 250 ms for now. 
    */
    int64_t next_timestamp = tx_start[channel] + 
                             airtime_in_ms(channel, txq.entries[tx_ongoing[channel]].payload_length) + 
                             RDCPv04_TIMESLOT_BUFFERTIME;
    next_timestamp -= RETRANSMISSION_PROCESSING_TIME;
    next_timestamp -= tx_latency[channel] < TX_LATENCY_CAP ? tx_latency[channel] : TX_LATENCY_CAP;

    txq.entries[tx_ongoing[channel]].scheduling_mode = SCHEDULING_MODE_FIXED_TIME;
    txq.entries[tx_ongoing[channel]].time_setting = next_timestamp;
    txq.entries[tx_ongoing[channel]].number_of_postponements = COUNT_ZERO;
    txq.entries[tx_ongoing[channel]].waiting = IS_WAITING;
    txq.entries[tx_ongoing[channel]].in_process = IN_PROCESS;
    txq.entries[tx_ongoing[channel]].in_cad_mode = NOT_IN_CAD_MODE;
    tx_ongoing[channel] = RDCP_INDEX_NONE; 
  }
  else
  { // no further retransmissions for any sent message
    if (txq.entries[tx_ongoing[channel]].callback_selector == TX_CALLBACK_NONE)
    {
      // Nothing to do; no callback necessary.
    }
    else /* Add more callback options here later */
    {
      // do nothing so far
    }
    /* Clear the TXQ entry */
    txq.entries[tx_ongoing[channel]].waiting = IS_NOT_WAITING;
    txq.entries[tx_ongoing[channel]].payload_length = ZERO_LENGTH;
    txq.entries[tx_ongoing[channel]].in_process = NOT_IN_PROCESS;
    txq.entries[tx_ongoing[channel]].number_of_postponements = COUNT_ZERO;
    txq.entries[tx_ongoing[channel]].in_cad_mode = NOT_IN_CAD_MODE;
    txq.num_entries--;
    retransmission_count[channel] = COUNT_ZERO;
    tx_ongoing[channel] = RDCP_INDEX_NONE;
  }
  
  return;
}

bool scheduler_callback_cad(uint8_t radio_id, uint8_t channel_id, bool cad_busy)
{
  if (tx_ongoing[channel_id] == RDCP_INDEX_NONE) return false; // CAD operation without TX ongoing

  last_tx_activity[channel_id] = my_millis();
  
  txq.entries[tx_ongoing[channel_id]].cad_retry += 1;
  uint8_t retry = txq.entries[tx_ongoing[channel_id]].cad_retry;
  
  snprintf(sched_info, INFOLEN, "INFO: Send-processing: CAD reports channel %d %s (try %d)", 
    (int) channel_id, 
    cad_busy == CAD_CHANNEL_FREE ? "free" : "busy", 
    (int) retry);
  serial_writeln(sched_info);
  
  if (cad_busy == CAD_CHANNEL_FREE)
  {
    scheduler_send_message_force(radio_id, channel_id);
    return true;
  }
  
  /* Channel is currently busy */

  /* Handle generic LoRa packets by sending them as soon as the channel becomes free */
  if (txq.entries[tx_ongoing[channel_id]].payload_type == PAYLOAD_TYPE_GENERIC_LORA)
  {
    radio_start_cad(radio_id, channel_id);
    return false;
  }

  /* For RDCP v0.4 Messages, apply the back-off procedure */
  if (txq.entries[tx_ongoing[channel_id]].payload_type == PAYLOAD_TYPE_RDCP_V04)
  {
    int sf_multiplier = rdcpv04_get_sf_multiplier(channel_id);
    if (retry < 5)
    {
      radio_start_cad(radio_id, channel_id);
    }
    else if (retry == 5)
    {
      radio_start_receive(radio_id, channel_id);
      txq.entries[tx_ongoing[channel_id]].in_process = false;
      tx_ongoing[channel_id] = -1;
      rdcpv04_set_channel_free_estimation(channel_id, rdcpv04_get_channel_free_estimation(channel_id) + 
        sf_multiplier * my_random_in_range(2100, 2500));
    }
    else if ((retry >= 6) && (retry <= 9))
    {
      radio_start_cad(radio_id, channel_id);
    }
    else if ((retry >= 10) && (retry <= 14))
    {
      radio_start_receive(radio_id, channel_id);
      txq.entries[tx_ongoing[channel_id]].in_process = false;
      tx_ongoing[channel_id] = -1;
      rdcpv04_set_channel_free_estimation(channel_id, rdcpv04_get_channel_free_estimation(channel_id) + 
        sf_multiplier * my_random_in_range(3100, 3500));
    }
    else if (retry >= 15)
    {
      snprintf(sched_info, INFOLEN, "WARNING: CAD retry timeout for TXQ%02d i%d, force-sending now", 
        (int) channel_id, (int) tx_ongoing[channel_id]);
      serial_writeln(sched_info);
      scheduler_send_message_force(radio_id, channel_id);
      return true;
    }
  } // handle RDCP v0.4 back-off

  return false;
}

void scheduler_dump_txqueue(void)
{
  int64_t now = my_millis();
#if defined(ESP32)
  snprintf(sched_info, INFOLEN, "INFO: Listing TXQ at %lld ms, %hhd entries", now, txq.num_entries);
#elif defined(USE_NRF52)
  snprintf(sched_info, INFOLEN, "INFO: Listing TXQ, %d entries", (int) txq.num_entries);
#endif
  serial_writeln(sched_info);

  for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
  {
    if (txq.entries[i].waiting)
    {
      int64_t timediff = txq.entries[i].time_setting - now;
      int32_t td = (int32_t) timediff;

      snprintf(sched_info, INFOLEN, "INFO: TXQ i%02d ch%02d rt%.3fms len%03d sm%d t%dms",
        (int) i, 
        (int) txq.entries[i].channel_to_send_on,
        td / 1000.0, 
        (int) txq.entries[i].payload_length, 
        (int) txq.entries[i].scheduling_mode,
        (int) txq.entries[i].time_setting);
      serial_writeln(sched_info);
    }
  }

  serial_writeln("INFO: Listing TXQ finished");
  return;
}

bool scheduler_enqueue(uint8_t channel, uint8_t payload_type, uint8_t *data, uint8_t len, uint8_t scheduling_mode, uint8_t callback_selector, int64_t forced_time)
{
  if (txq.num_entries == MAX_TXQUEUE_ENTRIES)
  {
    serial_writeln("WARNING: TX Queue is full, cannot add this new entry");
    scheduler_dump_txqueue();
    return false;
  }

  if (len > RDCPv04_MAX_LORA_PAYLOAD_SIZE)
  {
    serial_writeln("WARNING: Scheduled message would exceed maximum allowed LoRa packet payload size, refusing");
    return false;
  }
  
  int64_t now = my_millis();

  for (int i=COUNT_ZERO; i < MAX_TXQUEUE_ENTRIES; i++)
  {
    if (txq.entries[i].waiting == IS_NOT_WAITING)
    { // free slot found
      txq.num_entries += 1;
      txq.entries[i].channel_to_send_on = channel;
      for (int j=COUNT_ZERO; j < len; j++) txq.entries[i].payload[j] = data[j];
      txq.entries[i].payload_length = len;
      txq.entries[i].scheduling_mode = scheduling_mode;
      txq.entries[i].number_of_postponements = COUNT_ZERO;
      txq.entries[i].callback_selector = callback_selector;
      txq.entries[i].cad_retry = COUNT_ZERO;
      txq.entries[i].waiting = IS_WAITING;
      txq.entries[i].in_process = NOT_IN_PROCESS;
      txq.entries[i].in_cad_mode = NOT_IN_CAD_MODE;
      txq.entries[i].payload_type = payload_type;

      if (payload_type == PAYLOAD_TYPE_RDCP_V04)
      {
        txq.entries[i].timeslot_duration = rdcpv04_get_timeslot_duration(channel, data);
      }
      else 
      { 
        txq.entries[i].timeslot_duration = airtime_in_ms(channel, len);
      }

      if (scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE)
      {
        txq.entries[i].time_setting = now; // time when added is used as sorting criterion
      }
      else
      { // Absolute time scheduling 
        int64_t txq_cfest = rdcpv04_get_channel_free_estimation(channel);
        if (forced_time == ZERO_TIMESTAMP)
        { // 0 -> force-send at current CFEst
          if (txq_cfest < now) txq.entries[i].time_setting = now;
          else txq.entries[i].time_setting = txq_cfest;
        }
        else if (forced_time < ZERO_TIMESTAMP)
        { // negative -> force-send but append to CFEst
          txq_cfest -= forced_time;
          if (txq_cfest < now) txq.entries[i].time_setting = now;
          else txq.entries[i].time_setting = txq_cfest;
        }
        else 
        { // positive -> treat as relative to now
          txq.entries[i].time_setting = now + forced_time;
        }
      }
  
      snprintf(sched_info, INFOLEN, "INFO: Outgoing message scheduled -> TXQ%02d i %d, len %d, TSd%dms, @%dms %s", 
          (int) channel, (int) i, (int) len, 
          (int) txq.entries[i].timeslot_duration, 
          (int) txq.entries[i].time_setting, 
          txq.entries[i].scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE ? "cf" : "hard");
      serial_writeln(sched_info);

      scheduler_dump_txqueue();

      return true; // found free spot and added entry, exit loop here.
    } // free txq slot found
  } // loop over all txq slots
  
  serial_writeln("ERROR: All TXQ slots marked as occupied");
  return false;
}

int scheduler_get_num_txq_entries(void)
{
  return txq.num_entries;
}

/* EOF rdcp-modem-scheduler.cpp */