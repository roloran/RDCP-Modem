/* rdcp-modem-lora-settings.h */

#ifndef _RDCP_MODEM_LORA_SETTINGS
#define _RDCP_MODEM_LORA_SETTINGS

#include <Arduino.h> 
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-constants.h"

//< Configuration settings for one LoRa channel
struct lora_channel_config 
{
    float    frequency_in_mhz;
    float    bandwidth_in_khz;
    int      spreading_factor;
    int      coding_rate;
    uint8_t  syncword;
    int      tx_power;
    uint16_t preamble_length;
    bool     send_enabled;
    uint8_t  cfest_mode;
};

//< Maximum current limit for RadioLib (0 .. 140)
#define RADIOLIB_CURRENT_LIMIT 140

//< Do not let RadioLib check the CRC of LoRa packets 
#define RADIOLIB_CHECK_CRC false

//< Configuration settings for an RDCP device
struct device_config 
{
    uint8_t  scenario_num_relays   = 10;                      /// Number of relays in the current RDCP scenario
    bool     bt_enabled            = false;                   /// Bluetooth enabled
    char     bt_device_name[LEN32] = "RDCP-Modem (unnamed)";  /// Device name for Bluetooth connections
    char     serial_prefix[LEN32]  = SERIAL_PREFIX;           /// Serial prefix to use on UART output
    int      number_of_radios      = 0;                       /// Number of radios in use
#ifdef USE_HELTEC_V3
#ifdef AIR_CONTROLLER
    uint8_t  number_of_channels    = 2;                       /// Number of channels available
#else
    uint8_t  number_of_channels    = 1;                       /// Number of channels available
#endif
#else 
    uint8_t  number_of_channels    = 0;                        /// Number of channels available
#endif
    bool     init_radios_on_start  = INIT_RADIOS_ON_START;     /// Automatically initialize radios on power-on
    bool     radios_initialized    = false;                    /// Have the radios been initialized? Set at run-time.
    bool     print_rx_lines        = true;                     /// Print RX lines on LoRa packet reception
    bool     print_rxmeta_lines    = true;                     /// Print RXMETA lines on LoRa packet reception
    bool     print_rdcpcsv_lines   = true;                     /// Print RDCPCSV lines on RDCP message reception
    bool     print_airinfo_lines   = true;                     /// Print AIR communication information lines
#ifdef AIR_CONTROLLER
    bool     prepare_for_air[3]    = { false, false, false } ; /// Prepare Serial ports as controller
#else
    bool     prepare_for_air[3]    = { false, true, true } ;   /// Prepare Serial ports as non-controller
#endif
#ifdef USE_HELTEC_V3
    int      serial1_rx            = GPIO_NUM_15;              /// GPIO pin for Serial1 RX
    int      serial1_tx            = GPIO_NUM_16;              /// GPIO pin for Serial1 TX
    int      serial2_rx            = PIN_NOT_USED;             /// GPIO pin for Serial2 RX
    int      serial2_tx            = PIN_NOT_USED;             /// GPIO pin for Serial2 TX
#else 
    int      serial1_rx            = PIN_NOT_USED;             /// GPIO pin for Serial1 RX
    int      serial1_tx            = PIN_NOT_USED;             /// GPIO pin for Serial1 TX
    int      serial2_rx            = PIN_NOT_USED;             /// GPIO pin for Serial2 RX
    int      serial2_tx            = PIN_NOT_USED;             /// GPIO pin for Serial2 TX
#endif
    bool     prefix_on_air_serial  = OPTION_DISABLED;          /// Use "prefix" output for AIR messages on Serial0?
    bool     reset_spi_one         = OPTION_DISABLED;          /// Reset first SPI on first use?
    bool     reset_spi_two         = OPTION_DISABLED;          /// Reset second SPI on first use?
    uint8_t  default_radio         = RADIO0;                   /// Default radio to use
    uint8_t  default_channel       = CHANNEL0;                 /// Default channel to use
    bool     serial0_legacy        = OPTION_ENABLED;           /// Use ROLODECK-like Serial0 input handling
    bool     hq_mode               = OPTION_DISABLED;          /// Use ROLODECK-like HQ mode
    uint16_t rdcp_address          = RDCP_ADDRESS_ZERO;        /// -> This device's RDCP unicast address
};

#endif 

/* EOF rdcp-modem-lora-settings.h */