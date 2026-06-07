/* rdcp-modem-scheduler.h */

#ifndef _RDCP_MODEM_SCHEDULER 
#define _RDCP_MODEM_SCHEDULER 

#include <Arduino.h> 
#include "rdcp-modem-constants.h"
#include "rdcp-modem-rdcp-v04.h"

/**
  * Data structure for a TX Queue entry.
  */
struct txqueue_entry {
  uint8_t channel_to_send_on = NO_CHANNEL;                         //< Channel on which the message is to be sent
  uint8_t payload[MAX_LORA_PAYLOAD_SIZE];                          //< data of the outgoing message
  uint8_t payload_length = ZERO_LENGTH;                            //< length of the outgoing message
  uint8_t scheduling_mode = SCHEDULING_MODE_CHANNEL_FREE;          //< fixed-time or channel-free scheduling
  int64_t time_setting = TIMESTAMP_ZERO;                           //< timestamp when to transmit (hard-scheduled) or timestamp when added (channel free)
  uint8_t number_of_postponements = COUNT_ZERO;                    //< how often the entry has already been rescheduled
  uint8_t callback_selector = TX_CALLBACK_NONE;                    //< which callback function to use when TX is finished
  int64_t timeslot_duration = DURATION_ZERO;                       //< timeslot duration in milliseconds including retransmissions
  uint8_t cad_retry = COUNT_ZERO;                                  //< CAD retry attempt number
  bool waiting = IS_NOT_WAITING;                                   //< message is still waiting to be sent
  bool in_process = NOT_IN_PROCESS;                                //< this message is currently being processed
  bool in_cad_mode = NOT_IN_CAD_MODE;                              //< indicate whether we are currently checking for channel really being free
  uint8_t payload_type = PAYLOAD_TYPE_GENERIC_LORA;                //< Payload type for message to send
};
  
/**
  * Data structure for the overall TX Queue.
  */
struct txqueue {
  uint8_t num_entries = COUNT_ZERO;                    //< Number of queue entries currently in use
  struct txqueue_entry entries[MAX_TXQUEUE_ENTRIES];   //< The queue entries
};

/**
 * Scheduler loop, to be called periodically.
 * @return true if a queue entry was brought on its way to transmission, false otherwise
 */
bool scheduler_loop(void);

/**
 * Notify scheduler that outgoing processing on a given channel needs to be postponed because 
 * the channel has become busy.
 * @param channel_id Channel id of channel that has become busy
 */
void scheduler_stop_and_reschedule_on_busy_channel(uint8_t channel_id);
        
/**
 * Send the tx_ongoing-flagged message with channel activity detection. Scheduler-internal use.
 * @param radio_id Radio id of radio to use
 * @param channel_id Channel id of relevant channel
 */
void scheduler_send_message_cad(uint8_t radio_id, uint8_t channel_id);

/**
 * Send the tx_ongoing-flagged message now. Scheduler-internal use.
 * @param radio_id Radio id of radio to use
 * @param channel_id Channel id of relevant channel
 */
void scheduler_send_message_force(uint8_t radio_id, uint8_t channel_id);

/**
 * Let the scheduler know about a new CAD result so it can start sending or re-schedule.
 * @param radio_id Radio id
 * @param channel_id Channel id
 * @param cad_busy CAD_CHANNEL_FREE or CAD_CHANNEL_BUSY
 */
bool scheduler_callback_cad(uint8_t radio_id, uint8_t channel_id, bool cad_busy);

/**
 * Let the scheduler know that a transmission has finished, so it can schedule retransmissions.
 * @param radio_id Radio id
 * @param channel_id Channel id
 */
void scheduler_callback_txfin(uint8_t radio_to_use, uint8_t channel);

/**
 * Add a message to send to the TX Queue scheduler.
 * @param channel Channel id of channel to use for sending
 * @param payload_type Payload type, e.g., PAYLOAD_TYPE_RDCP_V04
 * @param data Binary data to send (array of bytes)
 * @param len Length of data to send (number of bytes)
 * @param scheduling_mode SCHEDULING_MODE_CHANNEL_FREE or SCHEDULING_MODE_FIXED_TIME
 * @param callback_selector Callback to use, e.g., TX_CALLBACK_NONE
 * @param forced_time Timestamp for SCHEDULING_MODE_FIXED_TIME, usually positive in ms, 0=current CFEst, negative=append to current CFEst
 */
bool scheduler_enqueue(uint8_t channel=CURRENT_CHANNEL, uint8_t payload_type=PAYLOAD_TYPE_RDCP_V04, uint8_t* data=NULL, uint8_t len=0, uint8_t scheduling_mode=SCHEDULING_MODE_CHANNEL_FREE, uint8_t callback_selector=TX_CALLBACK_NONE, int64_t forced_time=0);

/**
 * Dump the TX Queue on Serial.
 */
void scheduler_dump_txqueue(void);

/**
 * Get the number of entries currently queued in TXQ.
 * @return Number of outgoing LoRa packets queued
 */
int scheduler_get_num_txq_entries(void);

#endif

/* EOF rdcp-modem-scheduler.h */