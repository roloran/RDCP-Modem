/* rdcp-modem-rdcp-v04.cpp */

#include <Arduino.h>
#include "rdcp-modem-rdcp-v04.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-scheduler.h"
#if defined(ESP32)
#include <LittleFS.h>
#elif defined(USE_NRF52)
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
extern File f;
#endif
#include "rdcp-modem-persistence.h"
#include "rdcp-modem-crypto.h"

extern rdcpv04_message current_rdcpv04_message;
rdcpv04_message prepared_rdcpv04_message;
extern lora_channel_config lora_channel[];

int64_t last_csv_timestamp[MAX_NUMBER_OF_CHANNELS] = { RDCPv04_TIMESTAMP_ZERO };

extern lora_message current_lora_message;
extern device_config cfg;
extern int64_t CFEst[MAX_NUMBER_OF_CHANNELS]; 
extern uint16_t rdcpv04_most_recent_airtime;
extern uint8_t  rdcpv04_most_recent_future_timeslots;
extern lora_channel_config lora_channel[];
extern txqueue txq;
extern int tx_ongoing[MAX_NUMBER_OF_CHANNELS];
extern String line_from_file;
extern int64_t reboot_requested;
extern bool seqnr_reset_requested;
extern bool current_rdcpv04_message_is_duplicate;

bool     rdcpv04_csvlogfile_enabled = OPTION_DISABLED;
uint16_t rdcpv04_csvlogfile_count   = COUNT_ZERO;

rdcpv04_dtable rdcpv04_dupe_table;

int64_t last_heartbeat_sent = ZERO_TIMESTAMP;
bool rtc_active = OPTION_DISABLED;
rtc_entry RTC[MAX_RTC];

bool rdcpv04_check_duplicate_message(uint16_t origin, uint16_t sequence_number)
{
  if (rdcpv04_dupe_table.num_entries == RDCP_INDEX_NONE)
  {
    /* Initialize dupe table on first use */
    for (int i=0; i<NUM_DUPETABLE_ENTRIES; i++)
    {
      rdcpv04_dupe_table.origin[i]    = RDCP_ADDRESS_ZERO;
      rdcpv04_dupe_table.seqnr[i]     = RDCPv04_SEQUENCENR_SPECIAL_ZERO;
      rdcpv04_dupe_table.last_seen[i] = TIMESTAMP_ZERO;
    }
    rdcpv04_dupe_table.num_entries = 0;
  }

  int pos = RDCP_INDEX_NONE;
  for (int i=0; i != rdcpv04_dupe_table.num_entries; i++)
  {
    if (rdcpv04_dupe_table.origin[i] == origin) pos = i;
  }

  if (pos == RDCP_INDEX_NONE) // new entry
  {
    if (rdcpv04_dupe_table.num_entries > NUM_DUPETABLE_ENTRIES-1)
    {
      Serial.println("WARNING: RDCP v0.4 duplicate table overflow - increase size!");
      return false;
    }
    rdcpv04_dupe_table.origin[rdcpv04_dupe_table.num_entries]    = origin;
    rdcpv04_dupe_table.seqnr[rdcpv04_dupe_table.num_entries]     = sequence_number;
    rdcpv04_dupe_table.last_seen[rdcpv04_dupe_table.num_entries] = my_millis();
    rdcpv04_dupe_table.num_entries++;
    return false;
  }
  else
  {
    rdcpv04_dupe_table.last_seen[pos] = my_millis();
    if (rdcpv04_dupe_table.seqnr[pos] < sequence_number)
    { // update highest sequence number
      rdcpv04_dupe_table.seqnr[pos] = sequence_number;
      return false;
    }
    else
    { // duplicate found
      return true;
    }
  }
  return false;
}

uint16_t airtime_in_ms(uint8_t channel, uint8_t payload_size)
{
  uint16_t time_for_packet = 0;

  uint32_t bandwidth_in_hz = (uint32_t) lora_channel[channel].bandwidth_in_khz * 1000;
  uint8_t  low_data_rate_optimization = 1;
  uint8_t  implicit_header_mode = 0;
  uint8_t  coding_rate = lora_channel[channel].coding_rate - 4;
  uint8_t  SF = lora_channel[channel].spreading_factor;

  double time_per_symbol = pow(2, SF) / bandwidth_in_hz;

  /* Calculate the airtime for the preamble */
  uint8_t number_of_preamble_symbols = lora_channel[channel].preamble_length;
  double time_for_preamble = (number_of_preamble_symbols + 4.25) * time_per_symbol;

  /* Calculate the airtime for the payload */
  double payload_symbol_arg =
         (8.0 * payload_size - 4.0 * SF + 28.0 + 16.0 - 20.0 * implicit_header_mode) /
         (4.0 * (SF - 2.0 * low_data_rate_optimization));
  double number_of_payload_symbols =
         8.0 + max((coding_rate + 4.0) * ceil(payload_symbol_arg), 0.0);
  double time_for_payload = number_of_payload_symbols * time_per_symbol;

  /* Sum it up, converting from seconds to milliseconds and from Double to Int */
  time_for_packet = (uint16_t) ceil(1000.0 * (time_for_preamble + time_for_payload));

  /* Set global variable for legacy RDCP v0.4 handling */
  rdcpv04_most_recent_airtime = time_for_packet;

  return time_for_packet;
}

int64_t rdcpv04_get_channel_free_estimation(uint8_t channel)
{
  return CFEst[channel];
}

void rdcpv04_set_channel_free_estimation(uint8_t channel, int64_t new_value)
{
  CFEst[channel] = new_value;
  return;
}

bool rdcpv04_prolong_channel_free_estimation(uint8_t channel, int64_t new_value)
{
  if (rdcpv04_get_channel_free_estimation(channel) < new_value)
  {
    rdcpv04_set_channel_free_estimation(channel, new_value);
    return true;
  }
  return false;
}

int rdcpv04_get_sf_multiplier(uint8_t channel)
{ // map 1..32 to 1..10
  if (lora_channel[channel].spreading_factor == SF7) return 1;
  if (lora_channel[channel].spreading_factor == SF8) return 2;
  if (lora_channel[channel].spreading_factor == SF9) return 3;
  if (lora_channel[channel].spreading_factor == SF10) return 4;
  if (lora_channel[channel].spreading_factor == SF11) return 7;
  if (lora_channel[channel].spreading_factor == SF12) return 10;
  return 1;
}

void rdcpv04_update_cfest_rx(uint8_t mode)
{
  uint16_t airtime = airtime_in_ms(current_lora_message.channel, RDCPv04_HEADER_SIZE + current_rdcpv04_message.header.rdcp_payload_length);
  uint16_t airtime_with_buffer = airtime + RDCPv04_TIMESLOT_BUFFERTIME;

  uint32_t remaining_current_sender_time = airtime_with_buffer * current_rdcpv04_message.header.counter;

  uint8_t nrt = RDCPv04_NRT_LEVEL_LOW;
  uint8_t mt = current_rdcpv04_message.header.message_type;
  if ( (mt == RDCPv04_MSGTYPE_INFRASTRUCTURE_RESET) || (mt == RDCPv04_MSGTYPE_ACK) ||
       (mt == RDCPv04_MSGTYPE_RESET_ALL_ANNOUNCEMENTS) ) nrt = RDCPv04_NRT_LEVEL_MIDDLE;
  if ( (mt == RDCPv04_MSGTYPE_OFFICIAL_ANNOUNCEMENT) || (mt == RDCPv04_MSGTYPE_CITIZEN_REPORT) ||
       (mt == RDCPv04_MSGTYPE_SIGNATURE) ) nrt = RDCPv04_NRT_LEVEL_HIGH;

  uint32_t timeslot_duration = (nrt+1) * airtime_with_buffer;

  uint8_t future_timeslots = ZERO_TIMESLOTS;
  uint32_t magic_delay = ZERO_DELAY;

  if (lora_channel[current_lora_message.channel].cfest_mode == CFEST_MODE_RDCP_V04_433)
  { 
    if ((current_rdcpv04_message.header.sender < RDCPv04_ADDRESS_MG_LOWERBOUND) && 
        (current_rdcpv04_message.header.sender >= RDCPv04_ADDRESS_BBKDA_LOWERBOUND))
    { // DA or BBK sending
      if ((current_rdcpv04_message.header.relay1 & 0x0F) == 0x00) future_timeslots = 8;
      if ((current_rdcpv04_message.header.relay1 & 0x0F) == 0x02) future_timeslots = 7;
      if ((current_rdcpv04_message.header.relay1 & 0x0F) == 0x03) future_timeslots = 4; // Third Hop 1 assigned with Delay 3
      if ((current_rdcpv04_message.header.relay2 & 0x0F) == 0x04) future_timeslots = 6; // Second Hop 4 assigned with Delay 4
      if (current_rdcpv04_message.header.relay1 == 0xE4) future_timeslots = 5;
      if (current_rdcpv04_message.header.relay1 == 0xE2) future_timeslots = 3;
      if (current_rdcpv04_message.header.relay1 == 0xE1) future_timeslots = 2;
      if (current_rdcpv04_message.header.relay1 == 0xE0) future_timeslots = 1;
      if (current_rdcpv04_message.header.relay1 == 0xEE) future_timeslots = 0;
    }
    else
    { // other device sending, not leading to relay on same channel
      future_timeslots = ZERO_TIMESLOTS;
    }
  }

  if (lora_channel[current_lora_message.channel].cfest_mode == CFEST_MODE_RDCP_V04_868)
  {
    /* 
      The basic assumption for 868 MHz channels with RDCP v0.4 is that the channel will 
      be free after the current timeslot (zero future timeslots).
      However, implementation changes to RDCP Relays in DAs attempt to keep the 868 MHz
      channel free until the shadow propagation cycle has finished.
      We attempt to derive the CFEst value depending on whether a DA is currently sending,
      or whether it is a MG sending anything but a heartbeat.
    */
    if ((current_rdcpv04_message.header.sender < RDCPv04_ADDRESS_MG_LOWERBOUND) && 
        (current_rdcpv04_message.header.sender >= RDCPv04_ADDRESS_BBKDA_LOWERBOUND))
    { // DA or BBK sending
      uint8_t relay_currently_sending = current_rdcpv04_message.header.sender & 0x0F;
      int future_relays = cfg.scenario_num_relays - relay_currently_sending - THIS_ONE;
      future_timeslots = future_relays > ZERO_TIMESLOTS ? future_relays : ZERO_TIMESLOTS;
      
      /* 
        A magic value in the relay3 header field indicates that a message's Entry Point
        echoes back a new message in its local area. In this case, a full 868.forward
        cycle will still follow afterwards.
      */    
      if ((current_rdcpv04_message.header.relay1 == RDCPv04_HEADER_RELAY_MAGIC_NONE) &&
          (current_rdcpv04_message.header.relay3 == RDCPv04_HEADER_RELAY_MAGIC_EP_ECHO))
      {
        future_timeslots = cfg.scenario_num_relays;
        magic_delay = RDCPv04_EP_HEADSTART_DELAY;
      }

      /* Selected message types stay local to DAs and must be ignored when sent by a DA origin */
      if ((current_rdcpv04_message.header.message_type == RDCPv04_MSGTYPE_ACK) || 
          (current_rdcpv04_message.header.message_type == RDCPv04_MSGTYPE_ROAMING_BEACON))
      {
        if ((current_rdcpv04_message.header.origin < RDCPv04_ADDRESS_MG_LOWERBOUND) &&
            (current_rdcpv04_message.header.origin >= RDCPv04_ADDRESS_BBKDA_LOWERBOUND))
        {
          future_timeslots = 0;
        }
      }
    }
    else 
    { // HQ or MG device sending
      if (current_rdcpv04_message.header.sequence_number > RDCPv04_SEQUENCENR_SPECIAL_ZERO)
      { // not an MG heartbeat
        if (current_rdcpv04_message.header.relay1 != RDCPv04_RELAY1_NO_EP)
        {
          // only apply if EP is set
          future_timeslots = cfg.scenario_num_relays;
          magic_delay = RDCPv04_EP_HEADSTART_DELAY;
        }
      }
    }
  }
  
  if (lora_channel[current_lora_message.channel].cfest_mode == CFEST_MODE_UNKNOWN)
  {
    /* If CFEst mode is "unknown", we assume that the current sender will use its indicated
       retransmissions, but we have no information about future timeslots. */
    future_timeslots = ZERO_TIMESLOTS;
  }

  uint32_t channel_free_after = remaining_current_sender_time + future_timeslots * timeslot_duration + magic_delay;
  int64_t channel_free_at = my_millis() + channel_free_after;
  rdcpv04_most_recent_future_timeslots = future_timeslots;

  rdcpv04_prolong_channel_free_estimation(current_lora_message.channel, channel_free_at);

  char buf[INFOLEN];
#if defined(ESP32)
  snprintf(buf, INFOLEN, "INFO: Channel %d CFEst4current (%s, CFEst mode %d): +%d ms, @%" PRId64 " ms (airtime %" PRIu16 " ms, retrans %" PRIu32 " ms, timeslot %" PRIu32 " ms, %" PRIu8 " fut ts, magic %" PRIu32 " ms)", 
    (int) current_lora_message.channel,
    mode == UPDATE_CFEST_MODE_RX ? "in" : "out",
    (int) lora_channel[current_lora_message.channel].cfest_mode,
    channel_free_after, 
    channel_free_at, 
    airtime, 
    remaining_current_sender_time, 
    timeslot_duration, 
    future_timeslots,
    magic_delay);
#elif defined(USE_NRF52)
  snprintf(buf, INFOLEN, "INFO: Channel %d CFEst4current (%s, CFEst mode %d): +%d ms, @%d ms (airtime %d ms, retrans %d ms, timeslot %d ms, %d fut ts, magic %d ms)", 
    (int) current_lora_message.channel,
    mode == UPDATE_CFEST_MODE_RX ? "in" : "out",
    (int) lora_channel[current_lora_message.channel].cfest_mode,
    (int) channel_free_after, 
    (int) channel_free_at, 
    (int) airtime, 
    (int) remaining_current_sender_time, 
    (int) timeslot_duration, 
    (int) future_timeslots,
    (int) magic_delay);
#endif
  serial_writeln(buf);

  return;
}

void rdcpv04_update_cfest_tx(uint8_t channel)
{ 
  /* 
    Map the relevant properties of the outgoing message to an incoming one and 
    use the ~_rx function to handle CFEst 
  */

  current_lora_message.channel = channel;
  memcpy(&current_rdcpv04_message.header, &txq.entries[tx_ongoing[channel]].payload, RDCPv04_HEADER_SIZE);
  rdcpv04_update_cfest_rx(UPDATE_CFEST_MODE_TX);
  
  return;
}

int64_t rdcpv04_get_timeslot_duration(uint8_t channel, uint8_t *data)
{
  int64_t duration = DURATION_ZERO;

  rdcpv04_header h;
  memcpy(&h, data, RDCPv04_HEADER_SIZE);

  uint16_t airtime = airtime_in_ms(channel, RDCPv04_HEADER_SIZE + h.rdcp_payload_length);
  uint16_t airtime_with_buffer = airtime + RDCPv04_TIMESLOT_BUFFERTIME;

  uint8_t nrt = RDCPv04_NRT_LEVEL_LOW;
  uint8_t mt = h.message_type;
  if ( (mt == RDCPv04_MSGTYPE_INFRASTRUCTURE_RESET) || (mt == RDCPv04_MSGTYPE_ACK) ||
       (mt == RDCPv04_MSGTYPE_RESET_ALL_ANNOUNCEMENTS) ) nrt = RDCPv04_NRT_LEVEL_MIDDLE;
  if ( (mt == RDCPv04_MSGTYPE_OFFICIAL_ANNOUNCEMENT) || (mt == RDCPv04_MSGTYPE_CITIZEN_REPORT) ||
       (mt == RDCPv04_MSGTYPE_SIGNATURE) ) nrt = RDCPv04_NRT_LEVEL_HIGH;

  duration = (nrt+1) * airtime_with_buffer;

  return duration;
}

void rdcpv04_print_csv(void)
{
  int64_t now = my_millis();
  char info[LONGINFOLEN];

  uint16_t refnr = RDCPv04_OA_REFNR_SPECIAL_ZERO;
  if (current_rdcpv04_message.header.message_type == RDCPv04_MSGTYPE_OFFICIAL_ANNOUNCEMENT)
  {
    refnr = current_rdcpv04_message.payload.data[1] + 256 * current_rdcpv04_message.payload.data[2];
  }
  else if (current_rdcpv04_message.header.message_type == RDCPv04_MSGTYPE_SIGNATURE)
  {
    refnr = current_rdcpv04_message.payload.data[0] + 256 * current_rdcpv04_message.payload.data[1];
  }
#if defined (ESP32)
  snprintf(info, LONGINFOLEN, "RDCPCSV: %04X-%02d,%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%d,%04X,%d,%04X,%04X,%04X,%04X,%02X,%d,%02X,%02X,%02X,%04X,%d,%3.3f,%.2f,%.2f", 
    cfg.rdcp_address, 
    current_lora_message.channel,
    now - last_csv_timestamp[current_lora_message.channel],
    now, 
    CFEst[current_lora_message.channel],
    CFEst[current_lora_message.channel] - now,
    current_rdcpv04_message.header.rdcp_payload_length + RDCPv04_HEADER_SIZE,
    refnr,
    rdcpv04_most_recent_future_timeslots,
    current_rdcpv04_message.header.sender,
    current_rdcpv04_message.header.origin,
    current_rdcpv04_message.header.sequence_number,
    current_rdcpv04_message.header.destination,
    current_rdcpv04_message.header.message_type,
    current_rdcpv04_message.header.counter,
    current_rdcpv04_message.header.relay1,
    current_rdcpv04_message.header.relay2,
    current_rdcpv04_message.header.relay3,
    current_rdcpv04_message.header.checksum,
    rdcpv04_most_recent_airtime,
    lora_channel[current_lora_message.channel].frequency_in_mhz,
    current_lora_message.rssi,
    current_lora_message.snr
  );
#elif defined(USE_NRF52)
  snprintf(info, LONGINFOLEN, "RDCPCSV: %04X-%02d,%d,%d,%d,%d,%d,%04X,%d,%04X,%04X,%04X,%04X,%02X,%d,%02X,%02X,%02X,%04X,%d,%3.3f,%.2f,%.2f", 
    (unsigned int) cfg.rdcp_address, 
    (int) current_lora_message.channel,
    (int) (now - last_csv_timestamp[current_lora_message.channel]),
    (int) now, 
    (int) CFEst[current_lora_message.channel],
    (int) (CFEst[current_lora_message.channel] - now),
    (int) (current_rdcpv04_message.header.rdcp_payload_length + RDCPv04_HEADER_SIZE),
    (int) refnr,
    (int) rdcpv04_most_recent_future_timeslots,
    (unsigned int) current_rdcpv04_message.header.sender,
    (unsigned int) current_rdcpv04_message.header.origin,
    (unsigned int) current_rdcpv04_message.header.sequence_number,
    (unsigned int) current_rdcpv04_message.header.destination,
    (unsigned int) current_rdcpv04_message.header.message_type,
    (unsigned int) current_rdcpv04_message.header.counter,
    (unsigned int) current_rdcpv04_message.header.relay1,
    (unsigned int) current_rdcpv04_message.header.relay2,
    (unsigned int) current_rdcpv04_message.header.relay3,
    (unsigned int) current_rdcpv04_message.header.checksum,
    (unsigned int) rdcpv04_most_recent_airtime,
    lora_channel[current_lora_message.channel].frequency_in_mhz,
    current_lora_message.rssi,
    current_lora_message.snr
  );
#endif

  serial_writeln(info);
  rdcpv04_csvlogfile_append(info);

  last_csv_timestamp[current_lora_message.channel] = now;
  return;
}

void rdcpv04_csvlogfile_set_status(bool enabled)
{
  rdcpv04_csvlogfile_enabled = enabled;
  if (enabled) 
    serial_writeln("INFO: RDCPv04 CSV Logfile enabled");
  else 
    serial_writeln("INFO: RDCPv04 CSV Logfile disabled");
  return;
}

void rdcpv04_csvlogfile_append(char* line)
{
  if (!rdcpv04_csvlogfile_enabled) return;
  if (rdcpv04_csvlogfile_count > RDCPv04_CSVLOGFILE_MAX_ENTRIES)
  {
    serial_writeln("ERROR: RDCPv04 CSV Logfile maximum size exceeded");
    return;
  }
  rdcpv04_csvlogfile_count++;

#if defined(ESP32)
  File f = LittleFS.open(FILENAME_RDCPv04CSV_LOGFILE, FILE_APPEND);
#elif defined (USE_NRF52)
  f.open(FILENAME_RDCPv04CSV_LOGFILE, FILE_O_WRITE);
#endif
  if (!f) return;

  f.printf("%s\n", line);
  f.close();
  return;
}

void rdcpv04_csvlogfile_delete(void)
{
#if defined(ESP32)
  LittleFS.remove(FILENAME_RDCPv04CSV_LOGFILE);
#elif defined(USE_NRF52)
  InternalFS.remove(FILENAME_RDCPv04CSV_LOGFILE);
#endif
  rdcpv04_csvlogfile_count = 0;
  serial_writeln("INFO: RDCPv04 CSV Logfile deleted");
  return;
}

void rdcpv04_csvlogfile_dump(void)
{
#if defined(ESP32)
  File f = LittleFS.open(FILENAME_RDCPv04CSV_LOGFILE, FILE_READ);
#elif defined(USE_NRF52)
  f.open(FILENAME_RDCPv04CSV_LOGFILE, FILE_O_READ);
#endif
  if (!f)
  {
    serial_writeln("INFO: No RDCPv04 CSV Logfile available");
    return;
  }
  serial_writeln("INFO: BEGIN OF RDCPv04 CSV LOGFILE");

  char line_c[SERIALINPUTLEN];

  while (f.available())
  {
    line_from_file = f.readStringUntil('\n');
    line_from_file.toCharArray(line_c, SERIALINPUTLEN);
    serial_writeln(line_c);
  }

  f.close();
  serial_writeln("INFO: END OF RDCPv04 CSV LOGFILE");
  return;
}

bool rdcpv04_check_crc_in(uint8_t real_packet_length)
{
  uint8_t data_for_crc[DATALEN];

  /* Copy RDCP header and payload into data structure for CRC calculation */
  memcpy(&data_for_crc, &current_rdcpv04_message.header, RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE);
  for (int i=0; i < real_packet_length - RDCPv04_HEADER_SIZE; i++)
  {
    data_for_crc[i + RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE] = current_rdcpv04_message.payload.data[i];
  }

  /* Calculate and check CRC */
  uint16_t actual_crc = crc16_rdcpv04(data_for_crc, real_packet_length - RDCPv04_CRC_SIZE);

  if (actual_crc == current_rdcpv04_message.header.checksum)
  {
    return true;
  }

  return false;
}

uint16_t crc16_rdcpv04(uint8_t *data, uint16_t len)
{
    uint16_t lookup[] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108,
        0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
        0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B,
        0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
        0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE,
        0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
        0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D,
        0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5,
        0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
        0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
        0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
        0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13,
        0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
        0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E,
        0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1,
        0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
        0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0,
        0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
        0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657,
        0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
        0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882,
        0x28A3, 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
        0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E,
        0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
        0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D,
        0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
        0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

    uint16_t crc = 0xFFFF;
    for (int i=0; i < len; i++)
    {
        uint8_t b = data[i];
        crc = (crc << 8) ^ lookup[(crc >> 8) ^ b];
        crc &= 0xFFFF;
    }
    return crc;
}

/**
 * Return the default number of retransmissions for a given message type. 
 * @param mt RDCP v0.4 Message Type 
 * @return Default initial number of retransmission for the queried message type
 */
uint8_t rdcpv04_get_default_retransmission_counter_for_messagetype(uint8_t mt)
{
  uint8_t nrt = RDCPv04_NRT_LEVEL_LOW;

  if ( (mt == RDCPv04_MSGTYPE_INFRASTRUCTURE_RESET) || (mt == RDCPv04_MSGTYPE_ACK) ||
       (mt == RDCPv04_MSGTYPE_RESET_ALL_ANNOUNCEMENTS) ) nrt = RDCPv04_NRT_LEVEL_MIDDLE;

  if ( (mt == RDCPv04_MSGTYPE_OFFICIAL_ANNOUNCEMENT) || (mt == RDCPv04_MSGTYPE_CITIZEN_REPORT) ||
       (mt == RDCPv04_MSGTYPE_SIGNATURE) ) nrt = RDCPv04_NRT_LEVEL_HIGH;

  return nrt;
}

/**
 * Fill the RDCP Header fields for outgoing responses as they are
 * the same for any outgoing response.
 * 
 * Important:
 * Destination, Message Type, Payload Length, Relay123 and the whole Payload
 * must be set before calling this function.
 * 
 * @param reuse_seqnr true if existing SeqNr should be kept, false to set a new one
 */
void rdcpv04_prepare_response_header(bool reuse_seqnr)
{
    prepared_rdcpv04_message.header.sender = cfg.rdcp_address;
    prepared_rdcpv04_message.header.origin = cfg.rdcp_address;

    if (!reuse_seqnr) prepared_rdcpv04_message.header.sequence_number = persistence_get_next_rdcp_sequence_number(cfg.rdcp_address);
    prepared_rdcpv04_message.header.counter = rdcpv04_get_default_retransmission_counter_for_messagetype(prepared_rdcpv04_message.header.message_type);

    /* Update CRC header field */
    uint8_t data_for_crc[INFOLEN];
    memcpy(&data_for_crc, &prepared_rdcpv04_message.header, RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE);
    for (int i=COUNT_ZERO; i < prepared_rdcpv04_message.header.rdcp_payload_length; i++)
        data_for_crc[i + RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE] = prepared_rdcpv04_message.payload.data[i];
    uint16_t actual_crc = crc16_rdcpv04(data_for_crc, RDCPv04_HEADER_SIZE - RDCPv04_CRC_SIZE + prepared_rdcpv04_message.header.rdcp_payload_length);
    prepared_rdcpv04_message.header.checksum = actual_crc;

    return;
}

/**
 * Schedule the prepared response for transmission on the given channel.
 * @param channel Channel to use
 * @param no_delay Send ASAP if true, add a delay if false
 */
void rdcpv04_pass_response_to_scheduler(uint8_t channel, bool no_delay=false)
{
    uint8_t data_for_scheduler[INFOLEN];
    memcpy(&data_for_scheduler, &prepared_rdcpv04_message.header, RDCPv04_HEADER_SIZE);
    for (int i=COUNT_ZERO; i < prepared_rdcpv04_message.header.rdcp_payload_length; i++)
        data_for_scheduler[i + RDCPv04_HEADER_SIZE] = prepared_rdcpv04_message.payload.data[i];

    int64_t forced_time = TIMESTAMP_ZERO; 

    if (!no_delay)
    {
      forced_time -= cfg.default_response_delay * rdcpv04_get_sf_multiplier(channel);
    }

    /* Relay-specific handling (4 second 433 MHz headstart handling, see RDCP-Relay) */
    if (cfg.device_is_relay)
    {
      int factor = no_delay ? 0 : 20;
      forced_time = -1 * factor * SECONDS_TO_MILLISECONDS * rdcpv04_get_sf_multiplier(channel);
      forced_time = -100 * (1 + cfg.relay_identifier);
      if (lora_channel[channel].cfest_mode == CFEST_MODE_RDCP_V04_868) forced_time -= 4 * SECONDS_TO_MILLISECONDS;
    }

    scheduler_enqueue(channel, PAYLOAD_TYPE_RDCP_V04, data_for_scheduler, 
                      RDCPv04_HEADER_SIZE + prepared_rdcpv04_message.header.rdcp_payload_length,
                      forced_time == TIMESTAMP_ZERO ? SCHEDULING_MODE_CHANNEL_FREE : SCHEDULING_MODE_FIXED_TIME,
                      TX_CALLBACK_NONE, forced_time);

    return;
}

/**
 * In order to verify the Schnorr signature of an incoming RDCP Message, calculate the
 * hash value for the relevant RDCP Header and RDCP Payload elements.
 * @param m Pointer to an rdcp_message data structure
 * @param payloadprefixlength Number of bytes at the beginning of the RDCP Payload to consider
 * @param hashtarget Pointer where to store the 32-byte hash result
 */
void get_inline_hash(struct rdcpv04_message *m, uint8_t payloadprefixlength, uint8_t *hashtarget)
{
  /* Prepare the signed data */
  uint8_t data_to_sign[INFOLEN];
  data_to_sign[0] = m->header.origin % 256;
  data_to_sign[1] = m->header.origin / 256;
  data_to_sign[2] = m->header.sequence_number % 256;
  data_to_sign[3] = m->header.sequence_number / 256;
  data_to_sign[4] = m->header.destination % 256;
  data_to_sign[5] = m->header.destination / 256;
  data_to_sign[6] = m->header.message_type;
  data_to_sign[7] = m->header.rdcp_payload_length;
  for (int i=COUNT_ZERO; i<payloadprefixlength; i++) data_to_sign[8+i] = m->payload.data[i];
  uint8_t data_to_sign_length = 8 + payloadprefixlength;

  /* Get the SHA-256 hash for the data */
  SHA256 h = SHA256();
  h.reset();
  h.update(data_to_sign, data_to_sign_length);
  uint8_t sha[SHABUFSIZE];
  h.finalize(sha, SHABUFSIZE);

  /* Copy result to target buffer */
  for (int i=COUNT_ZERO; i<SHABUFSIZE; i++) hashtarget[i] = sha[i];

  return;
}

uint8_t rdcpv04_get_channel_for_mode(uint8_t mode)
{
  for (uint8_t c=COUNT_ZERO; c < cfg.number_of_channels; c++)
  {
    if (lora_channel[c].cfest_mode == mode) return c;
  }
  serial_writeln("WARNING: No suitable channel for response identified");
  return 0;
}

uint16_t rdcpv04_getSuggestedRelay(int num_try)
{
  /* Roaming not implemented yet */
  if (num_try == 0) return cfg.default_entry_point;
  return cfg.default_entry_point;
}

void rdcpv04_cmd_send_echo_response(void)
{
  if (current_rdcpv04_message.header.destination != cfg.rdcp_address) return; // respond to personal pings only
  prepared_rdcpv04_message.header.destination = current_rdcpv04_message.header.origin;
  prepared_rdcpv04_message.header.message_type = RDCPv04_MSGTYPE_ECHO_RESPONSE;
  prepared_rdcpv04_message.header.rdcp_payload_length = RDCPv04_PAYLOAD_SIZE_ECHO_RESPONSE;

  uint8_t channel = current_lora_message.channel;

  if (cfg.device_is_relay)
  {
    /* Respond on the same channel we got the request from unless it was forwarded, set Relays accordingly. */
    if (lora_channel[current_lora_message.channel].cfest_mode == CFEST_MODE_RDCP_V04_433 ||
        (current_rdcpv04_message.header.origin != current_rdcpv04_message.header.sender))
    {
      prepared_rdcpv04_message.header.relay1 = (cfg.cirerelays[0] << 4) + 0;
      prepared_rdcpv04_message.header.relay2 = (cfg.cirerelays[1] << 4) + 1;
      prepared_rdcpv04_message.header.relay3 = (cfg.cirerelays[2] << 4) + 2;
      channel = rdcpv04_get_channel_for_mode(CFEST_MODE_RDCP_V04_433);
    }
    else
    {
      prepared_rdcpv04_message.header.relay1 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
      prepared_rdcpv04_message.header.relay2 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
      prepared_rdcpv04_message.header.relay3 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
      channel = rdcpv04_get_channel_for_mode(CFEST_MODE_RDCP_V04_868);
    }
  }
  else 
  { // Not a relay, send via Entry Point on same channel as received
    prepared_rdcpv04_message.header.relay1 = (uint8_t) ((rdcpv04_getSuggestedRelay(0) & 0x000F) * 16) + (uint8_t) 0x0;
    prepared_rdcpv04_message.header.relay2 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
    prepared_rdcpv04_message.header.relay3 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
  }
  rdcpv04_prepare_response_header(OPTION_DISABLED);
  rdcpv04_pass_response_to_scheduler(channel);

  return;
}

void rdcpv04_cmd_timestamp(void)
{
  if (current_rdcpv04_message.header.rdcp_payload_length != RDCPv04_SIGNATURE_LENGTH + RDCPv04_PAYLOAD_SIZE_INLINE_TIMESTAMP)
  {
    serial_writeln("WARNING: Payload size of received RDCP Timestamp is invalid - ignoring");
    return;
  }

  uint8_t sha[SHABUFSIZE];
  get_inline_hash(&current_rdcpv04_message, RDCPv04_PAYLOAD_SIZE_INLINE_TIMESTAMP, sha);
  uint8_t sig[SIGBUFSIZE];
  for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) sig[i] = current_rdcpv04_message.payload.data[RDCPv04_PAYLOAD_SIZE_INLINE_TIMESTAMP+i];
  bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
  if (!valid_signature)
  {
    serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP Timestamp - ignoring");
    return;
  }

  uint8_t year   = current_rdcpv04_message.payload.data[0];
  uint8_t month  = current_rdcpv04_message.payload.data[1];
  uint8_t day    = current_rdcpv04_message.payload.data[2];
  uint8_t hour   = current_rdcpv04_message.payload.data[3];
  uint8_t minute = current_rdcpv04_message.payload.data[4];
  uint8_t status = current_rdcpv04_message.payload.data[5];

  char msg[INFOLEN];
  snprintf(msg, INFOLEN, "INFO: Received valid RDCP Timestamp: %02d.%02d.%04d %02d:%02d (Status %d)", 
           day, month, 2025 + year, hour, minute, status);
  serial_writeln(msg);

  /* With attached DA only 
  snprintf(msg, INFOLEN, "DA_TIME %02d.%02d.%04d %02d:%02d (%d)", day, month, 2025 + year, hour, minute, status);
  serial_writeln(msg);
  */

  /*
      We don't need the timestamp ourselves, only the overall RDCP Infrastructure status for now.
  */
  cfg.infrastructure_status = status;

  return;
}

void rdcpv04_cmd_device_reset(void)
{
  if (current_rdcpv04_message.header.destination != cfg.rdcp_address) return; // respond to personal device resets only

  uint8_t cmd_payload_len = RDCPv04_PAYLOAD_SIZE_INLINE_DEVICERESET;
  if (current_rdcpv04_message.header.rdcp_payload_length != RDCPv04_SIGNATURE_LENGTH + cmd_payload_len)
  {
    serial_writeln("WARNING: Payload size of received RDCP Device Reset is invalid - ignoring");
    return;
  }

  uint8_t sha[SHABUFSIZE];
  get_inline_hash(&current_rdcpv04_message, cmd_payload_len, sha);
  uint8_t sig[SIGBUFSIZE];
  for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) sig[i] = current_rdcpv04_message.payload.data[cmd_payload_len+i];
  bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
  if (!valid_signature)
  {
    serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP Device Reset - ignoring");
    return;
  }

  uint16_t nonce = current_rdcpv04_message.payload.data[0] + 256 * current_rdcpv04_message.payload.data[1];

  char noncename[NONCENAMESIZE]; snprintf(noncename, NONCENAMESIZE, "rstdev");
  if (!persistence_checkset_nonce(noncename, nonce))
  {
    serial_writeln("WARNING: Invalid nonce received for signed RDCP RESET of DEVICE");
    return;
  }
  else
  {
    serial_writeln("INFO: Performing RESET OF DEVICE");
    /* serial_writeln("DA_RESETDEVICE"); */ // Relay with attached DA only
  }

  /* We can reset all volatile data by simply restarting. Needs to be extended if more data is persisted. */
  hal_device_restart();

  return;
}

void rdcpv04_cmd_device_reboot(void)
{
  if (current_rdcpv04_message.header.destination != cfg.rdcp_address) return; // respond to personal device reboots only

  uint8_t cmd_payload_len = RDCPv04_PAYLOAD_SIZE_INLINE_DEVICEREBOOT;
  if (current_rdcpv04_message.header.rdcp_payload_length != RDCPv04_SIGNATURE_LENGTH + cmd_payload_len)
  {
    serial_writeln("WARNING: Payload size of received RDCP Device Reboot is invalid - ignoring");
    return;
  }

  uint8_t sha[SHABUFSIZE];
  get_inline_hash(&current_rdcpv04_message, cmd_payload_len, sha);
  uint8_t sig[SIGBUFSIZE];
  for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) sig[i] = current_rdcpv04_message.payload.data[cmd_payload_len+i];
  bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
  if (!valid_signature)
  {
    serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP Device Reboot - ignoring");
    return;
  }

  uint16_t nonce = current_rdcpv04_message.payload.data[0] + 256 * current_rdcpv04_message.payload.data[1];

  char noncename[NONCENAMESIZE]; snprintf(noncename, NONCENAMESIZE, "rstdev");
  if (!persistence_checkset_nonce(noncename, nonce))
  {
    serial_writeln("WARNING: Invalid nonce received for signed RDCP Reboot");
    return;
  }
  else
  {
    serial_writeln("INFO: Performing reboot");
    /* serial_writeln("DA_REBOOT"); */
  }
  hal_device_restart();
  return;
}

void rdcpv04_cmd_maintenance(void)
{
  if (current_rdcpv04_message.header.destination != cfg.rdcp_address) return; // respond to personal device maintenance only

  uint8_t cmd_payload_len = RDCPv04_PAYLOAD_SIZE_INLINE_MAINTENANCE;
  if (current_rdcpv04_message.header.rdcp_payload_length != RDCPv04_SIGNATURE_LENGTH + cmd_payload_len)
  {
    serial_writeln("WARNING: Payload size of received RDCP Device Maintenance is invalid - ignoring");
    return;
  }

  uint8_t sha[SHABUFSIZE];
  get_inline_hash(&current_rdcpv04_message, cmd_payload_len, sha);
  uint8_t sig[SIGBUFSIZE];
  for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) sig[i] = current_rdcpv04_message.payload.data[cmd_payload_len+i];
  bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
  if (!valid_signature)
  {
    serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP Maintenance - ignoring");
    return;
  }

  uint16_t nonce = current_rdcpv04_message.payload.data[0] + 256 * current_rdcpv04_message.payload.data[1];

  char noncename[NONCENAMESIZE]; snprintf(noncename, NONCENAMESIZE, "rstdev");
  if (!persistence_checkset_nonce(noncename, nonce))
  {
    serial_writeln("WARNING: Invalid nonce received for signed RDCP Maintenance");
    return;
  }
  else
  {
    serial_writeln("INFO: Starting DA Maintenance mode");
    serial_writeln("DA_MAINTENANCE");
    serial_bluetooth_enable();
  }

  return;
}

void rdcpv04_cmd_infrastructure_reset(void)
{
  uint8_t cmd_payload_len = RDCPv04_PAYLOAD_SIZE_INLINE_INFRARESET;
  if (current_rdcpv04_message.header.rdcp_payload_length != RDCPv04_SIGNATURE_LENGTH + cmd_payload_len)
  {
    serial_writeln("WARNING: Payload size of received RDCP Infrastructure Reset is invalid - ignoring");
    return;
  }

  uint8_t sha[SHABUFSIZE];
  get_inline_hash(&current_rdcpv04_message, cmd_payload_len, sha);
  uint8_t sig[SIGBUFSIZE];
  for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) sig[i] = current_rdcpv04_message.payload.data[cmd_payload_len+i];
  bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
  if (!valid_signature)
  {
    serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP Infrastructure Reset - ignoring");
    return;
  }

  uint16_t nonce = current_rdcpv04_message.payload.data[0] + 256 * current_rdcpv04_message.payload.data[1];

  char noncename[NONCENAMESIZE]; snprintf(noncename, NONCENAMESIZE, "rstinfra");
  if (!persistence_checkset_nonce(noncename, nonce))
  {
    serial_writeln("WARNING: Invalid nonce received for signed RDCP Infrastructure Reset");
    return;
  }
  else
  {
    serial_writeln("INFO: Performing Infrastructure Reset, rebooting in 3 minutes");
    /* serial_writeln("DA_INFRASTRUCTURE_RESET"); */
  }

  reboot_requested = my_millis() + 3 * MINUTES_TO_MILLISECONDS; // Reboot after 3 minutes
  seqnr_reset_requested = true; // Reset sequence numbers on infrastructure reset

  return;
}

void rdcpv04_derive_infrastructure_status_from_oa(void)
{
  /* Ignore private OAs as they are encrypted and we cannot extract the subheader */
  if ((current_rdcpv04_message.header.destination == RDCPv04_BROADCAST_ADDRESS) ||
     ((current_rdcpv04_message.header.destination >= RDCPv04_ADDRESS_MULTICAST_LOWERBOUND) && 
      (current_rdcpv04_message.header.destination <= RDCPv04_ADDRESS_MULTICAST_UPPERBOUND)))
  {
    /* RDCP v0.4 OAs have a subheader, and thus the OA subtype is the first byte of the RDCP Payload */
    uint8_t oatype = current_rdcpv04_message.payload.data[0];
    if (oatype == RDCPv04_MSGTYPE_OA_SUBTYPE_NONCRISIS)  cfg.infrastructure_status = RDCPv04_INFRASTRUCTURE_MODE_NONCRISIS;
    if (oatype == RDCPv04_MSGTYPE_OA_SUBTYPE_CRISIS_TXT) cfg.infrastructure_status = RDCPv04_INFRASTRUCTURE_MODE_CRISIS;
  }
  return;
}

void rdcpv04_cmd_oa_reset(void)
{
  if (current_rdcpv04_message.header.rdcp_payload_length != RDCPv04_SIGNATURE_LENGTH)
  {
    serial_writeln("WARNING: Payload size of received RDCP OA Reset is invalid - ignoring");
    return;
  }

  uint8_t sha[SHABUFSIZE];
  get_inline_hash(&current_rdcpv04_message, 0, sha);
  uint8_t sig[SIGBUFSIZE];
  for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) sig[i] = current_rdcpv04_message.payload.data[0+i];
  bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
  if (!valid_signature)
  {
    serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP OA Reset - ignoring");
    return;
  }

  /* serial_writeln("DA_OA_RESET"); */
  /* We do not handle OAs in this implementation yet, so there is nothing to do for an OA RESET yet either. */
  return;
}

void rdcpv04_check_heartbeat(void)
{
    if (cfg.heartbeat_interval == 0) return; // Heartbeat-sending disabled

    if (last_heartbeat_sent == TIMESTAMP_ZERO)
    { // Set initial delay for heartbeat messages
      // (intended to evenly spread heartbeat messages after scenario-wide device resets)
      last_heartbeat_sent = 4 * (cfg.rdcp_address & 0x0FFF) * 161 + ((cfg.rdcp_address >> 24) & 0x0F) * 161;
    }

    int64_t now = my_millis();
    if (last_heartbeat_sent + cfg.heartbeat_interval < now)
    {
        /* Don't even schedule a heartbeat when channel is currently busy. */
        if (scheduler_get_num_txq_entries() > 1)
        {
            serial_writeln("INFO: Postponing heartbeat by 5 minutes due to busy channel");
            last_heartbeat_sent += 5 * MINUTES_TO_MILLISECONDS;
            return;
        }

        last_heartbeat_sent = now;
        if (cfg.heartbeat_channel == NO_CHANNEL) return;
        serial_writeln("INFO: Preparing to send Heartbeat");

        prepared_rdcpv04_message.header.destination = RDCPv04_HQ_MULTICAST_ADDRESS;
        prepared_rdcpv04_message.header.message_type = RDCPv04_MSGTYPE_HEARTBEAT;
        prepared_rdcpv04_message.header.sequence_number = RDCPv04_SEQUENCENR_SPECIAL_ZERO; // for MG heartbeats

        /* With no OA handling and roaming implemented yet, fill with plausible defaults to avoid periodics */
        prepared_rdcpv04_message.payload.data[0] = 0xFFFF % 256;
        prepared_rdcpv04_message.payload.data[1] = 0xFFFF / 256;
        prepared_rdcpv04_message.payload.data[2] = cfg.default_entry_point % 256;
        prepared_rdcpv04_message.payload.data[3] = cfg.default_entry_point / 256;
        prepared_rdcpv04_message.header.rdcp_payload_length = 4;

        prepared_rdcpv04_message.header.relay1 = RDCPv04_HEADER_RELAY_MAGIC_NONE; // for MG heartbeats
        prepared_rdcpv04_message.header.relay2 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
        prepared_rdcpv04_message.header.relay3 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
        rdcpv04_prepare_response_header(OPTION_ENABLED); // needs pre-configured special sequence number
        rdcpv04_pass_response_to_scheduler(cfg.heartbeat_channel, OPTION_ENABLED);
    }

    return;
}

void rdcpv04_cmd_check_rtc(void)
{
  if (!rtc_active) return;
  bool one_active = false;
  for (int i=COUNT_ZERO; i<MAX_RTC; i++)
      if (RTC[i].active) one_active = true;
  if (!one_active) rtc_active = false;
  else
  {
    for (int i=COUNT_ZERO; i<MAX_RTC; i++)
    {
      if ((RTC[i].active) && (my_millis() > RTC[i].alarm))
      {
        RTC[i].active = false;
        if (RTC[i].restart) reboot_requested = my_millis() + 5 * RTC[i].restart * MINUTES_TO_MILLISECONDS;
        char s[SERIALINPUTLEN];
        snprintf(s, SERIALINPUTLEN, "%s%s", 
                 RTC[i].persist != 0 ? "+" : "",
                 RTC[i].rtc);
        serial_process_command(s, "RTC: ");
      }
    }
  }
  return;
}

void rdcpv04_cmd_rtc(void)
{
    uint8_t sha[SHABUFSIZE];
    if (current_rdcpv04_message.header.rdcp_payload_length < 3 + RDCPv04_SIGNATURE_LENGTH) return;
    get_inline_hash(&current_rdcpv04_message, current_rdcpv04_message.header.rdcp_payload_length - RDCPv04_SIGNATURE_LENGTH, sha);
    uint8_t sig[SIGBUFSIZE];
    for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++) 
      sig[i] = current_rdcpv04_message.payload.data[current_rdcpv04_message.header.rdcp_payload_length - RDCPv04_SIGNATURE_LENGTH + i];
    bool valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);
    if (!valid_signature)
    {
      serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP RTC - ignoring");
      return;
    }

    for (int i=COUNT_ZERO; i<MAX_RTC; i++)
    {
        if (RTC[i].active == false)
        {
            RTC[i].active  = true;
            RTC[i].alarm   = my_millis() + current_rdcpv04_message.payload.data[0] * MINUTES_TO_MILLISECONDS;
            RTC[i].restart = current_rdcpv04_message.payload.data[1];
            RTC[i].persist = current_rdcpv04_message.payload.data[2];
            for (int j=COUNT_ZERO; j<current_rdcpv04_message.header.rdcp_payload_length - RDCPv04_SIGNATURE_LENGTH - RDCPv04_PAYLOAD_SIZE_INLINE_RTC; j++)
            {
                RTC[i].rtc[j+0] = current_rdcpv04_message.payload.data[j+RDCPv04_PAYLOAD_SIZE_INLINE_RTC];
                RTC[i].rtc[j+1] = ZEROBYTE;
            }
            break;
        }
    }
    rtc_active = true;
    return;
}

void rdcpv04_process_incoming_message(void)
{
  uint8_t mt = current_rdcpv04_message.header.message_type;

  if (current_rdcpv04_message_is_duplicate) return;

  if (mt == RDCPv04_MSGTYPE_ECHO_REQUEST)                 rdcpv04_cmd_send_echo_response();
  else if (mt == RDCPv04_MSGTYPE_TIMESTAMP)               rdcpv04_cmd_timestamp();
  else if (mt == RDCPv04_MSGTYPE_DEVICE_RESET)            rdcpv04_cmd_device_reset();
  else if (mt == RDCPv04_MSGTYPE_DEVICE_REBOOT)           rdcpv04_cmd_device_reboot();
  else if (mt == RDCPv04_MSGTYPE_MAINTENANCE)             rdcpv04_cmd_maintenance();
  else if (mt == RDCPv04_MSGTYPE_INFRASTRUCTURE_RESET)    rdcpv04_cmd_infrastructure_reset();
  else if (mt == RDCPv04_MSGTYPE_OFFICIAL_ANNOUNCEMENT)   rdcpv04_derive_infrastructure_status_from_oa();
  else if (mt == RDCPv04_MSGTYPE_RTC)                     rdcpv04_cmd_rtc();
  else if (mt == RDCPv04_MSGTYPE_RESET_ALL_ANNOUNCEMENTS) rdcpv04_cmd_oa_reset();
  return;
}

void rdcpv04_tunnel(uint8_t channel, uint8_t* data, uint8_t len, uint8_t tunneltype)
{
  if (len == 0)
  {
    serial_writeln("WARNING: Refusing to tunnel empty payload");
    return;
  }
  if ((channel == NO_CHANNEL) || (channel >= cfg.number_of_channels))
  {
    serial_writeln("WARNING: Invalid channel for tunneled message");
    return;
  }

  if (len > 182)
  {
    serial_writeln("WARNING: Tunneled data too large, ignoring");
    return;
  } 

  prepared_rdcpv04_message.header.destination = RDCPv04_HQ_MULTICAST_ADDRESS;
  prepared_rdcpv04_message.header.message_type = RDCPv04_MSGTYPE_TUNNEL;
  prepared_rdcpv04_message.header.rdcp_payload_length = 2 + len; 

  prepared_rdcpv04_message.payload.data[0] = 0; // Backport to RDCP v0.5 forces a single transmission of TUNNEL messages
  prepared_rdcpv04_message.payload.data[1] = tunneltype;
  for (int i=COUNT_ZERO; i<len; i++) prepared_rdcpv04_message.payload.data[i+2] = data[i];

  prepared_rdcpv04_message.header.relay1 = (uint8_t) ((rdcpv04_getSuggestedRelay(0) & 0x000F) * 16) + (uint8_t) 0x0;
  prepared_rdcpv04_message.header.relay2 = RDCPv04_HEADER_RELAY_MAGIC_NONE;
  prepared_rdcpv04_message.header.relay3 = RDCPv04_HEADER_RELAY_MAGIC_NONE;

  rdcpv04_prepare_response_header(OPTION_DISABLED);
  rdcpv04_pass_response_to_scheduler(channel, OPTION_ENABLED);

  return;
}
/* EOF rdcp-modem-rdcp-v04.cpp */