/* rdcp-modem-hardware-settings.cpp */

#include <Arduino.h> 
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-lora-settings.h"
#include <RadioLib.h>

extern device_config cfg;

//< The radio pinout for the used hardware
// Different groups are:
//   - SX1262 radios
//   - SX1268 radios
//   - AIR radios on SPI_AIR_NONE
//   - AIR radios on SPI_AIR_ONE
//   - AIR radios on SPI_AIR_TWO
radio_pinout pinout[] =
{ 
#if defined(ESP32)
  {RADIO_TYPE_SX1262, FIRST_IN_GROUP,  SPI_ONE,     GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9, GPIO_NUM_8, GPIO_NUM_14, GPIO_NUM_13, GPIO_NUM_12, PIN_NOT_USED, PIN_NOT_USED, CHANNEL1 },
#else
  {RADIO_TYPE_SX1262, FIRST_IN_GROUP,  SPI_ONE,     PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL0 },
#endif
  {RADIO_TYPE_AIR,    FIRST_IN_GROUP,  SPI_AIR_ONE, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL1 },
  {RADIO_TYPE_SX1262, SECOND_IN_GROUP, SPI_ONE,     PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL2 },
  {RADIO_TYPE_SX1262, THIRD_IN_GROUP,  SPI_TWO,     PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL3 },
  {RADIO_TYPE_SX1262, FOURTH_IN_GROUP, SPI_TWO,     PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL4 },
  {RADIO_TYPE_AIR,    SECOND_IN_GROUP, SPI_AIR_TWO, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL5 },
  {RADIO_TYPE_AIR,    THIRD_IN_GROUP,  SPI_AIR_TWO, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL6 },
  {RADIO_TYPE_AIR,    FOURTH_IN_GROUP, SPI_AIR_TWO, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, PIN_NOT_USED, CHANNEL7 },
};

SX1262* sx1262_radios[MAX_NUMBER_OF_RADIOS] = { NULL };
SX1268* sx1268_radios[MAX_NUMBER_OF_RADIOS] = { NULL };
controlled_air_radio air_radios_air_none[MAX_NUMBER_OF_RADIOS];
controlled_air_radio air_radios_air_one [MAX_NUMBER_OF_RADIOS];
controlled_air_radio air_radios_air_two [MAX_NUMBER_OF_RADIOS];
provided_air_radio   air_radios_provided[MAX_NUMBER_OF_RADIOS];

void radio_basic_setup(void)
{
#ifdef USE_HELTEC_V3
  cfg.number_of_radios = 1;
#ifdef AIR_CONTROLLER
  cfg.number_of_radios = 2;
#endif 
#endif
#ifdef USE_ROLORAN_BOARD_V2025
  cfg.number_of_radios = 2;
#endif
  return;
}

void radio_inject_modules(int spi_num)
{
#ifdef USE_ROLORAN_BOARD_V2025
  extern SPIClass spi1, spi2;
  // Must set pinout adequately above!! The one provided by default is a for a single-SX1262 Heltec LoRa32 device!
  if (spi_num == SPI_ONE)
  {
    sx1262_radios[0] = new SX1262(new Module(pinout[0].spi_cs, pinout[0].lora_dio1, pinout[0].lora_reset, pinout[0].lora_busy, spi1, SPISettings(1000000, MSBFIRST, SPI_MODE0)));
  }
  else if (spi_num == SPI_TWO)
  {
    sx1268_radios[0] = new SX1268(new Module(pinout[1].spi_cs, pinout[1].lora_dio1, pinout[1].lora_reset, pinout[1].lora_busy, spi2, SPISettings(1000000, MSBFIRST, SPI_MODE0)));
  }
#endif

#ifdef USE_HELTEC_V3
#ifdef AIR_CONTROLLER
  if (spi_num == SPI_AIR_ONE)
  {
    air_radios_air_one[0].remote_radio         = AIRRADIO0;
    air_radios_air_one[0].airhops              = AIRHOP_DIRECT_NEIGHBOR;
    air_radios_air_one[0].rx_buf.available     = NO_MESSAGE_AVAILABLE;
    air_radios_air_one[0].cad_result_available = NO_CAD_RESULT_AVAILABLE;
  }
#endif
  if (spi_num == SPI_ONE)
  {
    // For deep-sleep devices, make sure that RadioLib does not touch the RESET pin on its begin()
    if (cfg.woken_from_deep_sleep) pinout[0].lora_reset = RADIOLIB_NC; // avoid resetting radio on wake-up  
    sx1262_radios[0] = new SX1262(new Module(pinout[0].spi_cs, pinout[0].lora_dio1, pinout[0].lora_reset, pinout[0].lora_busy));
  }
#endif

  return;
}

/* EOF rdcp-modem-hardware-settings.cpp */