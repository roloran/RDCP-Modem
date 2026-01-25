/* rdcp-modem-plugin.h */

#ifndef _RDCP_MODEM_PLUGIN 
#define _RDCP_MODEM_PLUGIN

#include <Arduino.h> 
#include "rdcp-modem-constants.h"

/**
 * Plugin function to handle otherwise unknown Serial commands.
 * @param s Serial command to handle
 * @return true if command was handled, false if it was unknown
 */
bool plugin_serial(const char *s);

/**
 * Plugin function to handle incoming LoRa packets.
 * Will be called after basic packet processing has finished.
 * @param lora_payload_type Type of the LoRa packet's payload, e.g. PAYLOAD_TYPE_RDCP_V04
 */
void plugin_incoming(uint8_t lora_payload_type=PAYLOAD_TYPE_GENERIC_LORA);

/**
 * Plugin function to handle callbacks.
 * @param radio Radio id of radio which has triggered the callback
 * @param channel Channel id of channel relevant to callback
 * @param cb_type Callback type
 * @param cad_result CAD result on CAD callbacks
 */
void plugin_callback(uint8_t radio, uint8_t channel, int cb_type, bool cad_result);

/**
 * Plugin function periodically called from the main loop.
 */
void plugin_loop(void);

#endif

/* EOF rdcp-modem-plugin.h */