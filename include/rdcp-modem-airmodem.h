/* rdcp-modem-airmodem.h */

#ifndef _RDCP_MODEM_AIRMODEM 
#define _RDCP_MODEM_AIRMODEM 

#include <Arduino.h>
#include "rdcp-modem-constants.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-lora-settings.h"

/*
 * AIR message format:
 *   AIR a b c d text
 *       a = %1d number of airhops to destination
 *       b = %1d number of airhops from source
 *       c = %1d radio number on remotely controlled device
 *       d = %1d radio number on controlling device
 *       text = %s AIR modem command
 * 
 * AIR commands:
 *   do_BEGIN                                 do initialize radio `c`, store relationship to controller's radio `d` as well as number of hops required `b`
 *   do_RX_ON_CHANNEL %02d                    do start receiving on the given channel with radio `c`
 *   do_TX_ON_CHANNEL %02d %s                 do a blocking transmission now on the given channel with radio `c`, given string content
 *   do_TXSTART_ON_CHANNEL %02d %03d %s       do start transmitting (asynchronously) on the given channel with radio `c`, given length in bytes and given Base64-encoded content
 *   do_CAD                                   do start channel activity detection on the currently used channel
 *   do_RNG                                   do create a new random number
 *   set_CHANNEL_FREQUENCY %02d %3.3f         set given channel to given frequency in MHz
 *   set_CHANNEL_BANDWIDTH %02d %3.3f         set given channel to given bandwidth in kHz
 *   set_CHANNEL_SPREADING_FACTOR %02d %02d   set given channel to given spreading factor
 *   set_CHANNEL_CODING_RATE %02d %d          set given channel to given (RadioLib) coding rate
 *   set_CHANNEL_SYNCWORD %02d %02X           set given channel to given sync word
 *   set_CHANNEL_TXPOWER %02d %d              set given channel to given TX power in dBm
 *   set_CHANNEL_PREAMBLE_LENGTH %02d %d      set given channel to given preamble length in number of symbols
 *   set_RADIO_CURRENT_LIMIT %03d             set radio `c` to given RadioLib current limit
 *   set_RADIO_CRC %d                         set radio `c` to CRC-handling by RadioLib/hardware (0 = disable, 1 = enable)
 * 
 * AIR callbacks:
 *   cb_TXSTART %02d                          note that asynchronous transmission on the given channel has started
 *   cb_TXFIN %02d                            note that asynchronous transmission on the given channel has finished
 *   cb_CAD %02d %d                           note that on the given channel, the given CAD result is available (0 = free, 1 = busy)
 *   cb_RNG %03d                              note that the given new random number is available
 *   cb_RX %8.3f %8.3f %03d %s                note that a LoRa packet was received with given RSSI and given SNR values, given length in number of bytes and given Base64-encoded payload
 */

/**
 * Initialize AIR modem interface 
 * @param air_spi_interface SPI_AIR_ONE or SPI_AIR_TWO, additional serial connection to use
 */
void air_setup(int air_spi_interface=SPI_AIR_NONE);

/**
 * Periodically handle incoming AIR modem events
 * @param has_serial0_input true if loop should handle input from Serial0 (false by default)
 * @param serial0_input Input to handle if `has_serial0_input` is true
 */
void air_loop(bool has_serial0_input=false, const char* serial0_input=NULL);

/**
 * Send a LoRa packet via AIR radio. 
 * @param radio Which AIR radio to use for TX
 * @param channel Which channel to use for TX
 * @param payload LoRa packet content to send 
 * @param length Length of payload in bytes 
 */
void air_send_message_binary(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, uint8_t *payload=NULL, uint8_t length=0);

/**
 * Start receiving on the given channel via AIR modem.
 * @param radio AIR radio to use for RX
 * @param channel Channel to use for RX
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_start_receive(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0);

/**
 * Set the frequency for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by frequency change
 * @param freq New frequency in MHz
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_frequency(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, double freq=868.2);

/**
 * Set the bandwidth for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by bandwidth change
 * @param freq New bandwidth in kHz
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_bandwidth(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, double bw=125.0);

/**
 * Set the spreading factor for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by spreading factor change
 * @param freq New spreading factor (e.g., 7..12)
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_spreading_factor(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, int sf=7);


/**
 * Set the coding rate for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by coding rate change
 * @param freq New coding rate (RadioLib-style, e.g., 5..8)
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_coding_rate(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, int cr=5);

/**
 * Set the sync word for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by sync word change
 * @param freq New sync word (e.g., 0x12 or 0x34)
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_syncword(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, uint8_t syncword=0x12);

/**
 * Set the TX power for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by TX power change
 * @param freq New TX power in dBm
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_txpower(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, int tx_power=0);

/**
 * Set the preamble length for a specific channel or an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel affected by preamble length change
 * @param freq New preamble length in number of symbols
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_preamble_length(uint8_t radio=RADIO0, uint8_t channel=CHANNEL0, uint16_t pl=15);

/**
 * Send a text string via AIR radio right now
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel to use
 * @param s String to transmit
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_send_now(uint8_t radio, uint8_t channel, char *s);

/**
 * Send binary via AIR radio right now
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param channel Id of channel to use
 * @param data Byte array of binary data to send
 * @param len Number of bytes to send (length of binary data to send)
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_start_send(uint8_t radio, uint8_t channel, const uint8_t* data, size_t len);

/**
 * Initialize an AIR radio connection
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_begin(uint8_t radio);

/**
 * Set the current limit for an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param climit New current limit (used by RadioLib, e.g., 140)
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_current_limit(uint8_t radio, int climit=RADIOLIB_CURRENT_LIMIT);

/**
 * Set CRC handling for an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param crcsetting true if RadioLib/hardware should handle LoRa CRC, false to perform manual checks
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_set_crc(uint8_t radio, bool crcsetting=RADIOLIB_CHECK_CRC);

/**
 * Return channel activity detection results for an AIR radio on which a channel scan was started earlier.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return RadioLib status code, usually RADIOLIB_CHANNEL_FREE or RADIOLIB_LORA_DETECTED
 */
int16_t air_get_channel_scan_result(uint8_t radio);

/**
 * Start channel activity detection with an AIR radio on its current channel.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return RadioLib status code, usually RADIOLIB_ERR_NONE
 */
int16_t air_start_channel_scan(uint8_t radio);

/**
 * Get the number of bytes received in the most recent LoRa packet by an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return Number of bytes received (0 for empty packet)
 */
int air_get_packet_length(uint8_t radio);

/**
 * Copy the LoRa payload of the most recently received LoRa packet via an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @param rx_buffer Byte array to copy the LoRa payload content to
 * @param num_bytes Maximum number of bytes to copy, usually given by air_get_packet_length()
 * @return Number of bytes received (0 for empty packet)
 */
int16_t air_read_data(uint8_t radio, uint8_t* rx_buffer, int num_bytes);

/**
 * Get the RSSI value of the most recent LoRa packet received by an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return RSSI value
 */
double air_get_rssi(uint8_t radio);

/**
 * Get the SNR value of the most recent LoRa packet received by an AIR radio.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return SNR value
 */
double air_get_snr(uint8_t radio);

/**
 * Get an eventually random number from an AIR radio.
 * Locally available TRNGs shall be preferred. Must not be used for cryptographic purposes.
 * May deliver the same number over and over again until a fresh one becomes available.
 * @param radio Local radio id; radio must be of type RADIO_TYPE_AIR
 * @return Eventually random byte value (0..255)
 */
uint8_t air_random_byte(uint8_t radio);

/**
 * Send a callback for TXSTART to the controller
 * @param real_radio Radio id on which the TXSTART event happened
 * @param channel Channel id on which the TXSTART event happened
 */
void air_fan_callback_txstart(uint8_t real_radio, uint8_t channel);

/**
 * Send a callback for TXFIN to the controller
 * @param real_radio Radio id on which the TXFIN event happened
 * @param channel Channel id on which the TXFIN event happened
 */
void air_fan_callback_txfin(uint8_t real_radio, uint8_t channel);

/**
 * Send a callback for CAD results to the controller
 * @param real_radio Radio id on which the CAD results are available
 * @param channel Channel id on which the CAD results are available 
 * @param cad_state RADIOLIB_CHANNEL_FREE (free) or RADIOLIB_LORA_DETECTED (busy)
 */
void air_fan_callback_cadresult(uint8_t real_radio, uint8_t channel, int cad_state);

/**
 * Send a callback for RX (LoRa packet received) to the controller
 * @param real_radio Radio id on which the LoRa packet was received
 * @param rssi RSSI value for the LoRa packet reception
 * @param snr SNR value for the LoRa packet reception
 * @param length Length of the received LoRa packet (number of bytes)
 * @param data Received LoRa packet payload (bytes, number/size given by `length`)
 */
void air_fan_callback_rx(uint8_t real_radio, double rssi, double snr, uint8_t length, uint8_t* data);

#endif

/* EOF rdcp-modem-airmodem.h */