/* rdcp-modem-plugin.cpp */

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "rdcp-modem-plugin.h"
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
#define MAX_TUNNEL_KEYS 256
static uint32_t tunnel_keys[MAX_TUNNEL_KEYS];
static size_t tunnel_keys_len = 0;

static char line_uppercase[SERIALINPUTLEN];
static char tunnel_cmd[SERIALINPUTLEN];

extern nsa_global nsa;

bool tunnel_key_contains(uint32_t key){
  size_t low = 0, high = tunnel_keys_len;
  while (low < high){
    size_t mid = low + (high - low) / 2;
    uint32_t m = tunnel_keys[mid];
    if (m == key) return true;
    if (m < key) low = mid + 1;
    else high = mid;
  }
  return false;
}

bool tunnel_key_insert(uint32_t key){
  // if maximum number of keys already reached then do not insert
  // even if the value is already inserted this will fail
  if (tunnel_keys_len >= MAX_TUNNEL_KEYS) {
    return false;
  }

  // find insertion point
  size_t low = 0, high = tunnel_keys_len;
  while (low < high){
    size_t mid = low + (high - low) / 2;
    uint32_t m = tunnel_keys[mid];
    if (m == key) return true;
    if (m < key) low = mid + 1;
    else high = mid;
  }

  // low is now at insertion point
  // need to make room
  for (size_t i = tunnel_keys_len; i > low; i--){
    tunnel_keys[i] = tunnel_keys[i - 1];
  }

  tunnel_keys[low] = key;
  tunnel_keys_len++;
  return true;
}

bool tunnel_key_remove(uint32_t key){
  if (tunnel_keys_len == 0){
    return false;
  }

  bool found = false;

  // first identify position of key
  size_t low = 0, high = tunnel_keys_len;
  size_t mid = 0;
  while (low < high){
    mid = low + (high - low) / 2;
    uint32_t m = tunnel_keys[mid];
    if (m == key) found = true;
    if (m < key) low = mid + 1;
    else high = mid;
  }

  if (!found) return false;

  // mid is the position of the element, need to move all higher values down
  // we actually skip the last element and leave it there, but its outside the tunnel_keys_len
  for (size_t i = mid; i > tunnel_keys_len - 1; i++){
    tunnel_keys[i] = tunnel_keys[i + 1];
  }

  tunnel_keys_len--;
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
        bool res = tunnel_key_insert(dev_addr);
        res? serial_writeln("Success!"): serial_writeln("Failure!");
        return true;
      }
      else if (nsa_startsWith(tunnel_cmd, "DEL "))
      {
        bool res = tunnel_key_remove(dev_addr);

        res? serial_writeln("DevAddr removed"): serial_writeln("DevAddr not in Tunnel configuration");
        return true;
      }
    }
  }
  return false;
}

extern lora_message    current_lora_message;
extern rdcpv04_message current_rdcpv04_message;
extern bool            current_rdcpv04_message_is_duplicate;

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
