/* rdcp-modem-plugin.cpp */

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "rdcp-modem-plugin.h"
#include "class/msc/msc.h"
#include "nsa.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-rdcp-v04.h"
#include "rdcp-modem-scheduler.h"

// could potentially be setable in platformio.ini
#define MAX_TUNNEL_DEV_ADDRS 256

/*
 * Represents a device whose data shall be tunneled. The device is identified by its
 * DevAddr. Additionally it stores the timestamp of the last LoRaWAN message received
 * from this device.
 */
struct tunnel_dev {
  uint32_t dev_addr;     // DevAddr of the LoRaWAN device
  uint64_t ts_last_rx;   // timestamp in milli seconds when last message from this device was received
};

/*
 * An optional `tunnel_dev`. If `valid` is not true then `tunnel_dev` must not be read
 */
struct maybe_tunnel_dev {
  tunnel_dev dev; // only access this if valid is true. No guaranteed content otherwise
  bool valid;            // indicates if `tunnel_dev` may be used
};

// tunnel_keys maps DevAddr to the last timestamp they a message from this address was received
static tunnel_dev tunnel_list[MAX_TUNNEL_DEV_ADDRS];
static size_t tunnel_list_len = 0;

// the radio on which to listen for packets
static size_t listen_radio = 0;
// the channel on which to listen for packets
static size_t listen_channel = 0;
// the radio over which to tunnel packets
static size_t tunnel_radio = 0;
// the channel over which to tunnel packets
static size_t tunnel_channel = 0;
// minimum time between two packets to be considered for tunneling. Default 10 min
static uint64_t min_tunnel_period = 10 * 60 * 1000;

static char line_uppercase[SERIALINPUTLEN];
static char tunnel_cmd[SERIALINPUTLEN];

extern nsa_global nsa;
extern lora_channel_config lora_channel[];

typedef enum {
  SF_7 = 0,
  SF_8,
  SF_9,
  SF_10,
  SF_11,
  SF_12,
  SF_COUNT,
} spreading_factor_t;

typedef enum {
  BW_125 = 0,
  BW_250,
  BW_COUNT,
} bandwidth_t;

// We ignore DR_7 because it uses FSK instead of LoRa
typedef enum {
  DR_0 = 0,
  DR_1,
  DR_2,
  DR_3,
  DR_4,
  DR_5,
  DR_6,
  DR_COUNT,
} data_rate_t;

// DR_COUNT -> means unsupported configuration
static const data_rate_t DATA_RATE_TABLE[SF_COUNT][BW_COUNT] = {
  /* BW 125, BW 250 */ 
  { DR_5,     DR_6 }, /* SF7 */
  { DR_4, DR_COUNT }, /* SF8 */
  { DR_3, DR_COUNT }, /* SF9 */
  { DR_2, DR_COUNT }, /* SF10 */
  { DR_1, DR_COUNT }, /* SF11 */
  { DR_0, DR_COUNT }  /* SF12 */
};

static const uint8_t FRM_PAYLOAD_TABLE[DR_COUNT] = {
  51,   /* DR_0 */
  51,   /* DR_1 */
  51,   /* DR_2 */
  115,  /* DR_3 */
  222,  /* DR_4 */
  222,  /* DR_5 */
  222,  /* DR_6 */
};

extern lora_message    current_lora_message;
extern rdcpv04_message current_rdcpv04_message;
extern bool            current_rdcpv04_message_is_duplicate;

/**
 * Return the FRM payload size given a known spreading factor and bandwidth.
 * 0 is returned in case there is not defined payload size for the argument combination.
 * The values returned are based on the LoRaWAN 1.0.3 Region specification
 */
uint8_t frm_payload_for_config(int sf, int bw){
  if (sf < 7 || sf > 12) return 0;
  int bandwith;
  if (bw == 125) bandwith = BW_125;
  else if (bw == 250) bandwith = BW_250;
  else return 0; 

  data_rate_t dr = DATA_RATE_TABLE[sf - 7][bandwith];

  if (dr == DR_COUNT) return 0;
  return FRM_PAYLOAD_TABLE[dr];
}

/*
 * Check if the given DevAddr is in the list of tunneled DevAddrs and if so retrieve
 * the corresponding meta information.
 * Internally uses binary search, so should be reasonably fast.
 */
maybe_tunnel_dev tunnel_dev_addr_get(uint32_t key){
  size_t low = 0, high = tunnel_list_len;
  while (low < high){
    size_t mid = low + (high - low) / 2;
    uint32_t m = tunnel_list[mid].dev_addr;
    if (m == key) return maybe_tunnel_dev{.dev = tunnel_list[mid], .valid=true};
    if (m < key) low = mid + 1;
    else high = mid;
  }
  return maybe_tunnel_dev{.dev=tunnel_dev{}, .valid=false};
}

/*
 * Check if the given DevAddr is in the list of tunneled DevAddrs
 */ 
bool tunnel_dev_addr_contains(uint32_t dev_addr){
  return tunnel_dev_addr_get(dev_addr).valid;
}


/*
 * Add a new DevAddr to the list of tunneled DevAddrs.
 *
 * The list is sorted, so addition may shift previos values. Can fail if already
 * `MAX_TUNNEL_DEV_ADDRS` DevAddrs are stored in the list. Failures is indicated
 * by returning `false` 
 */
bool tunnel_dev_addr_insert(uint32_t dev_addr){
  // if maximum number of keys already reached then do not insert
  // even if the value is already inserted this will fail
  if (tunnel_list_len >= MAX_TUNNEL_DEV_ADDRS) {
    return false;
  }

  // find insertion point
  size_t low = 0, high = tunnel_list_len;
  while (low < high){
    size_t mid = low + (high - low) / 2;
    uint32_t m = tunnel_list[mid].dev_addr;
    if (m == dev_addr) return true;
    if (m < dev_addr) low = mid + 1;
    else high = mid;
  }

  // low is now at insertion point
  // need to make room
  for (size_t i = tunnel_list_len; i > low; i--){
    tunnel_list[i] = tunnel_list[i - 1];
  }

  tunnel_dev entry = {.dev_addr = dev_addr, .ts_last_rx = 0};
  tunnel_list[low] = entry;
  tunnel_list_len++;
  return true;
}

/**
 * Remove a DevAddr from the list of tunneled DevAddr
 *
 * Internally operates on a sorted list, may shift values to ensure a contigous search
 * space. A return value of `true` indicates successful removal. `false` means that
 * `dev_addr` was not part of the list.
 */
bool tunnel_dev_addr_remove(uint32_t dev_addr){
  if (tunnel_list_len == 0){
    return false;
  }

  bool found = false;

  // first identify position of key
  size_t low = 0, high = tunnel_list_len;
  size_t mid = 0;
  while (low < high){
    mid = low + (high - low) / 2;
    uint32_t m = tunnel_list[mid].dev_addr;
    if (m == dev_addr) found = true;
    if (m < dev_addr) low = mid + 1;
    else high = mid;
  }

  if (!found) return false;

  // mid is the position of the element, need to move all higher values down
  // we actually skip the last element and leave it there, but its outside the tunnel_keys_len
  for (size_t i = mid; i > tunnel_list_len - 1; i++){
    tunnel_list[i] = tunnel_list[i + 1];
  }

  tunnel_list_len--;
  return true;
}

bool plugin_serial(const char* line)
{
  /* Handle any additional Serial commands here. Return true if handled, false if not handled. */

  // For continuity reasons most commands should operate on the unified upper case input
  strncpy(line_uppercase, line, SERIALINPUTLEN);
  for (int x=COUNT_ZERO; line_uppercase[x] != '\0'; x++) line_uppercase[x] = toUpperCase(line_uppercase[x]);

  if (nsa_startsWith(line_uppercase, "TUNNEL ")) {
    nsa_substring(line_uppercase, 7);
    strncpy(tunnel_cmd, nsa.result, SERIALINPUTLEN);

    serial_writeln(tunnel_cmd);

    if (nsa_startsWith(tunnel_cmd, "ADD ") || nsa_startsWith(tunnel_cmd, "DEL ")){
      // Add a number to our list of supported numbers
      nsa_substring(tunnel_cmd, 4);

      // Ensure address is of correct length, 9th character is prob newline
      if (strlen(nsa.result) != 9){
        serial_writeln("Invalid DevAddr, must be exactly 8 hex digits");
        return true;
      }

      // The conversion never fails, will at worst output 0 or MAX, e.g., if invalid characters are detected
      // On 32bit platforms assignment is no prob, on 64 bit platforms, addresses to large are truncated
      uint32_t dev_addr = strtol(nsa.result, NULL, BASE16);

      if (nsa_startsWith(tunnel_cmd, "ADD "))
      {
        bool res = tunnel_dev_addr_insert(dev_addr);
        res? serial_writeln("Success!"): serial_writeln("Failure!");
        return true;
      }
      else if (nsa_startsWith(tunnel_cmd, "DEL "))
      {
        bool res = tunnel_dev_addr_remove(dev_addr);

        res? serial_writeln("DevAddr removed"): serial_writeln("DevAddr not in Tunnel configuration");
        return true;
      }
    } else if (nsa_startsWith(tunnel_cmd, "CHANNEL ")){
      // TUNNEL CHANNEL 0 01 1 02
      nsa_strsplice(tunnel_cmd);
      listen_radio   = strtol(nsa.part[1], NULL, BASE10);
      listen_channel = strtol(nsa.part[2], NULL, BASE10);
      tunnel_radio   = strtol(nsa.part[3], NULL, BASE10);
      tunnel_channel = strtol(nsa.part[4], NULL, BASE10);
      return true;
    }
  }
  return false;
}

/*
 * Read a `uint32` in little endian from the first four bytes of `buf` regardless of target architecture
 */
uint32_t read_uint32_le(const uint8_t *buf){
  return ((uint32_t)buf[0])       |
         ((uint32_t)buf[1] << 8)  |
         ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

void plugin_incoming(uint8_t lora_payload_type)
{
  /* Handle incoming LoRa packets based on their payload type here. */
  /* 
    The received LoRa packet is available as `current_lora_message`.
    If it is an RDCP v0.4 Message with a valid CRC, it is also available as `current_rdcpv04_message`.
    Amend further external variables if required.
  */

  serial_writeln("-- LoRaWAN Plugin Processing --");

  uint8_t len = current_lora_message.payload_length;
  char printbuf[250 * 2 +1];

  static const char hex[] = "0123456789ABCDEF";

  for (size_t i = 0; i < current_lora_message.payload_length; i++){
    printbuf[i*2] = hex[current_lora_message.payload[i] >> 4];
    printbuf[i*2 + 1] = hex[current_lora_message.payload[i] & 0x0F];
  }

  printbuf[current_lora_message.payload_length * 2] = '\0';

  serial_writeln(printbuf);

  // Print only the address, its inverted
  uint32_t dev_addr = read_uint32_le(&current_lora_message.payload[1]);

  snprintf(printbuf, sizeof(printbuf), "DevAddr: %02x", dev_addr);
  serial_writeln(printbuf);

  // Check if address is in our tunnel config
  maybe_tunnel_dev maybe_dev = tunnel_dev_addr_get(dev_addr);

  if (!maybe_dev.valid){ return; }

  // dev is valid
  tunnel_dev td = maybe_dev.dev;

  // compare timestamps
  if (my_millis() - td.ts_last_rx < min_tunnel_period){ return; }

  // We need to tunnel the address
  td.ts_last_rx = my_millis();

  // Make sure that payload does not exceed tunnel length
  lora_channel_config chnl = lora_channel[tunnel_channel];
  uint8_t frm_size = frm_payload_for_config(chnl.spreading_factor, (int) chnl.bandwidth_in_khz);
  snprintf(printbuf, sizeof(printbuf), "Allowable FRM Payload size: %d bytes", frm_size);
  serial_writeln(printbuf);

  // Prepare transmission
  // Currently this always initiates a channel switch even if two different radios are used
  // The listen radio might not yet be on the correct channel in the first place
  radio_switch_to_channel(tunnel_radio, tunnel_channel, FORCED_CHANNEL_SWITCH);

  // Here the buffer to be sent should be build
  // scheduler_enqueue(tunnel_channel, tx_payload_type, (uint8_t*) decoded_string, decoded_length, tx_scheduling_mode, tx_callback_selector, tx_forced_time);
  
  radio_switch_to_channel(listen_radio, listen_channel, FORCED_CHANNEL_SWITCH);
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
