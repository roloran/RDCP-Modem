/* rdcp-modem-lora-radio.h */

#ifndef _RDCP_MODEM_LORA_RADIO
#define _RDCP_MODEM_LORA_RADIO 

#include <Arduino.h> 
#include "rdcp-modem-constants.h"

/**
 * Data structure for storing LoRa packets. 
 * Independent of channel. May be simulated or real. 
 */
struct lora_message {
    bool    available      = NO_MESSAGE_AVAILABLE;
    uint8_t payload[MAX_LORA_PAYLOAD_SIZE];
    uint8_t payload_length = ZERO_LENGTH;
    double  rssi           = NO_RSSI;
    double  snr            = NO_SNR;
    uint8_t radio          = RADIO0;
    uint8_t channel        = CHANNEL0;
    int64_t timestamp      = ZERO_TIMESTAMP;
};

/**
 * Initialize the EBYTE LoRa radios
 */
void lora_hardware_setup(void);

/**
 * Configure the EBYTE LoRa radios with channel-specific settings
 */
bool radio_setup(void);

/**
 * Periodically handle reception, transmission, CAD, timeouts etc. 
 */
void radio_loop(void);

/**
 * Send a LoRa packet. 
 * @param radio Which radio to use for TX
 * @param channel Which channel to use for TX (may be CURRENT_CHANNEL)
 * @param payload LoRa packet content to send 
 * @param length Length of payload in bytes 
 * @param forced_switch true if channel must be re-actived before action
 */
void radio_send_message_binary(uint8_t radio=RADIO0, uint8_t channel=CURRENT_CHANNEL, uint8_t *payload=NULL, uint8_t length=0, bool forced_switch=false);

/**
 * Start channel activity detection on the given channel. 
 * Results are processed via callbacks. 
 * @param radio Radio to use for CAD
 * @param channel Channel to use for CAD (may be CURRENT_CHANNEL)
 * @param forced_switch true if channel must be re-actived before action
 */
void radio_start_cad(uint8_t radio=RADIO0, uint8_t channel=CURRENT_CHANNEL, bool forced_switch=false);

/**
 * Start receiving on the given channel.
 * @param radio Radio to use for RX
 * @param channel Channel to use for RX (may be CURRENT_CHANNEL)
 * @param forced_switch true if channel must be re-actived before action
 */
int radio_start_receive(uint8_t radio=RADIO0, uint8_t channel=CURRENT_CHANNEL, bool forced_switch=false);

/**
 * Get a random byte from the default LoRa radio via RadioLib.
 * @param radio Which LoRa radio to use as source for random number seed
 * @return Random byte value (0..255)
 */
uint8_t radio_random_byte(uint8_t radio=RADIO0);

/**
 * Check whether a radio is available
 * @param radio Radio to check has_radio status for
 */
bool radio_get_has_radio(uint8_t radio=RADIO0);

/**
 * Set the transmission flag for a radio. Used for AIR radios only.
 * @param radio Radio to set the transmission flag for 
 * @param flag true (default) or false
 */
void radio_set_transmission_flag(uint8_t radio=RADIO0, bool flag=true);

/**
 * Send a string via a radio on a given channel now.
 * @param radio Radio id of radio to use 
 * @param channel Which channel to use (may be CURRENT_CHANNEL)
 * @param s C string to transmit 
 * @param forced_switch Force switch to channel even if already used
 */
int radio_send_now(uint8_t radio, uint8_t channel=CURRENT_CHANNEL, char *s=NULL, bool forced_switch=false);

/**
 * Send binary data via a radio on a given channel now. 
 * @param radio Radio id of radio to use 
 * @param channel Which channel to send on (may be CURRENT_CHANNEL)
 * @param data Binary data to send 
 * @param len Length of data to send (number of bytes)
 * @param forced_switch Force switch to channel even if it is currently used
 */
int radio_start_send(uint8_t radio, uint8_t channel=CURRENT_CHANNEL, const uint8_t* data=NULL, size_t len=0, bool forced_switch=false);

/**
 * Switch a radio to a given channel.
 * @param radio Radio id of radio to use 
 * @param channel_to_switch_to Channel id of channel to use 
 * @param force_switch True if switching is forced even if target channel is already used
 */
bool radio_switch_to_channel(uint8_t radio, uint8_t channel_to_switch_to, bool force_switch=false);

/**
 * Dump a received LoRa packet on Serial0.
 * @param channel Channel id of channel on which the packet to print was received
 */
void serial_write_incoming_message(uint8_t channel);

#endif 

/* EOF rdcp-modem-lora-radio.h */