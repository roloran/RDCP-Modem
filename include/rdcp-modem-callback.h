/* rdcp-modem-callback.h */

#ifndef _RDCP_MODEM_CALLBACK
#define _RDCP_MODEM_CALLBACK

#include <Arduino.h> 
#include "rdcp-modem-constants.h"

/**
 * Callback when a LoRa packet has been transmitted
 * @param radio Radio that has finished transmitting
 * @param channel Channel on which the LoRa packet was transmitted
 * @param cb_type Callback type, CALLBACK_LOCAL or CALLBACK_AIR
 */
void callback_tx_finished(uint8_t radio, uint8_t channel, int cb_type=CALLBACK_LOCAL);

/**
 * Callback when a LoRa packet is about to be transmitted
 * @param radio Radio which is going to transmit
 * @param channel Channel on which the LoRa packet is sent just now
 * @param cb_type Callback type, CALLBACK_LOCAL or CALLBACK_AIR
 */
void callback_tx_started(uint8_t radio, uint8_t channel, int cb_type=CALLBACK_LOCAL);

/**
 * Callback when CAD (channel activity detection) results are available for a channel
 * @param radio Radio which has performed CAD
 * @param channel Channel on which CAD was performed
 * @param cad_result Flag whether channel is busy of free according to CAD
 * @param cb_type Callback type, CALLBACK_LOCAL or CALLBACK_AIR
 */
void callback_cad_result(uint8_t radio, uint8_t channel, bool cad_result, int cb_type=CALLBACK_LOCAL);

#endif

/* EOF rdcp-modem-callback.h */