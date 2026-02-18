/* rdcp-modem-hardware-settings.h */

#ifndef _RDCP_MODEM_HARDWARE_SETTINGS
#define _RDCP_MODEM_HARDWARE_SETTINGS

#include <Arduino.h> 
#include <SPI.h>
#include <RadioLib.h>
#include "rdcp-modem-constants.h"
#include "rdcp-modem-lora-radio.h"

/*
    Settings to be changed on a per-device or per-scenario basis.
*/

// Hardware shortcuts... pick 0..1
// #define USE_HELTEC_V3
//#define USE_ROLORAN_BOARD_V2025

// Configuration shortcuts... pick any
// AIR controllers do not provide AIR radios to others, but eventually make use of AIR radios provided by other devices
#define AIR_CONTROLLER

//< Human-readable hardware/board identifier
#if defined (USE_HELTEC_V3)
#define USED_HARDWARE_DEVICE "Heltec LoRa 32 (v3.2/v4)"
#elif defined (USE_ROLORAN_BOARD_V2025)
#define USED_HARDWARE_DEVICE "ROLORAN Dual-Channel Relay"
#else 
#if defined(ESP32)
#define USED_HARDWARE_DEVICE "Generic ESP32 LoRa DevBoard"
#elif defined(USE_NRF52)
#define USED_HARDWARE_DEVICE "Generic nRF52 LoRa DevBoard"
#endif
#endif

//< Automatically initialize radios on power-on?
#if defined (USE_HELTEC_V3)
#define INIT_RADIOS_ON_START true
#else
#define INIT_RADIOS_ON_START false
#endif

//< Static prefix for each output line via Serial/UART
#define SERIAL_PREFIX "RDCP-Modem: "

//< Defined if the used hardware supports Bluetooth (BT) (libraries must be available for platform)
#ifdef ESP32
//#define DEVICE_HAS_BLUETOOTH
#endif

//< Defined if the used hardware supports Bluetooth Low Energy (BLE) (libraries must be available for platform)
#ifdef ESP32
#define DEVICE_HAS_BLUETOOTH_LE
#endif

#if defined(USE_ROLORAN_BOARD_V2025)
#undef DEVICE_HAS_BLUETOOTH_LE
#undef DEVICE_HAS_BLUETOOTH 
#define DEVICE_HAS_BLUETOOTH
#endif

//< Data structure for defining a radio pinout
struct radio_pinout
{
    int  radio_type;         //< LoRa chip type: RADIO_TYPE_SX1262, RADIO_TYPE_SX1268
    int  radio_id_in_group;  //< Id of this radio chip within its group of chip types, e.g. first SX1262 has Id 0, second SX1262 has Id 1
    int  num_spi;            //< Which SPI to use, SPI_ONE or SPI_TWO
    int  spi_miso;           //< MISO pin to use
    int  spi_mosi;           //< MOSI pin to use
    int  spi_clock;          //< SPI Clock pin to use 
    int  spi_cs;             //< SPI Client Select pin to use (a.k.a. CS, NSS, SS; dedicated one per SPI device)
    int  lora_dio1;          //< Pin connected to LoRa radio DIO1
    int  lora_busy;          //< Pin connected to LoRa radio BUSY
    int  lora_reset;         //< Pin connected to LoRa radio RESET
    int  lora_txen;          //< Pin for LoRa radio TX_ENABLE or -1 if not used
    int  lora_rxen;          //< Pin for LoRa radio RX_ENABLE or -1 if not used
    uint8_t default_channel; //< Default channel to use with this radio
};

//< Data structures for AIR radios
struct controlled_air_radio 
{
    int airhops;               //< How many hops is the destination radio away?  
    uint8_t remote_radio;      //< Remote physical radio tied to this AIR radio
    lora_message rx_buf;       //< Most recently received LoRa packet
    bool cad_result_available; //< Has received a CAD result
    int cad_result;            //< Most recently received CAD result
    uint8_t random_number;     //< Most recently received random number
};

struct provided_air_radio 
{
    int serial_to_controller = AIRMODEM_SERIAL_NONE;   //< Serial interface to use when being remotely controlled
    int airhops              = AIRHOP_DIRECT_NEIGHBOR; //< How many hops is the destination radio away?  
    uint8_t remote_radio     = NO_REAL_RADIO;          //< Remote controlled radio tied to this AIR radio
};

// ===========================================================================

/*
  Function prototypes
*/

/**
 * Inject the radio modules for an SPI
 * @param spi_num SPI interface to use
 */
void radio_inject_modules(int spi_num);

/**
 * Hardware-specific basic radio settings
 */
void radio_basic_setup(void);

#endif 

/* EOF rdcp-modem-hardware-settings.h */