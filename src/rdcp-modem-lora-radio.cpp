/* rdcp-modem-lora-radio.cpp */

#include <Arduino.h>
#include "rdcp-modem-constants.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-callback.h"
#include "rdcp-modem-airmodem.h"
#include "rdcp-modem-scheduler.h"

#include <RadioLib.h>
#include <SPI.h>
#include "Base64ren.h"

extern device_config cfg;
extern lora_channel_config lora_channel[];
extern radio_pinout pinout[];
extern SX1262* sx1262_radios[MAX_NUMBER_OF_RADIOS];
extern SX1268* sx1268_radios[MAX_NUMBER_OF_RADIOS];

lora_message lora_queue_rx[MAX_NUMBER_OF_CHANNELS];
lora_message lora_queue_tx[MAX_NUMBER_OF_CHANNELS];

bool has_radio[MAX_NUMBER_OF_RADIOS];
bool enable_interrupt[MAX_NUMBER_OF_RADIOS];
bool has_msg_to_send[MAX_NUMBER_OF_CHANNELS];
bool msg_on_the_way[MAX_NUMBER_OF_CHANNELS];
bool transmission_flag[MAX_NUMBER_OF_RADIOS];
int transmission_state[MAX_NUMBER_OF_CHANNELS];
int64_t start_of_transmission[MAX_NUMBER_OF_CHANNELS];
bool cad_mode[MAX_NUMBER_OF_RADIOS];
uint8_t channel_used_by_radio[MAX_NUMBER_OF_RADIOS];

#if defined(ESP32)
SemaphoreHandle_t highlander = NULL;
#elif defined(USE_NRF52)
SPIClass lora_spi = SPIClass(NRF_SPIM0, DEFAULT_NRF52_MISO, DEFAULT_NRF52_CLOCK, DEFAULT_NRF52_MOSI);
#endif

bool get_has_radio(uint8_t radio)
{
  return has_radio[radio];
}

uint8_t radio_random_byte(uint8_t radio)
{
  int radio_id_in_group = pinout[radio].radio_id_in_group;

  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    return sx1262_radios[radio_id_in_group]->randomByte();
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    return sx1268_radios[radio_id_in_group]->randomByte();
  } 
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    return air_random_byte(radio);
  } 
  else 
  {
    serial_writeln("ERROR: LoRa Radio for randomness unavailable");
  }
  return 0;
}

void initialize_channel(uint8_t channel)
{
  has_msg_to_send[channel]       = false;
  msg_on_the_way[channel]        = false;
  transmission_state[channel]    = RADIOLIB_ERR_NONE;
  start_of_transmission[channel] = ZERO_TIMESTAMP;
  return;
}

bool radio_switch_to_channel(uint8_t radio, uint8_t channel_to_switch_to, bool force_switch)
{
  char info_msg[INFOLEN];
  int radio_id_in_group = pinout[radio].radio_id_in_group;
  int state = RADIOLIB_ERR_NONE;

  uint8_t channel = channel_to_switch_to;
  if (channel == CURRENT_CHANNEL) channel = channel_used_by_radio[radio];

  /* Don't switch to the channel the radio is already on unless requested */
  if (channel_used_by_radio[radio] == channel)
  {
    if (!force_switch) return true;
  }

  snprintf(info_msg, INFOLEN, "INFO: Switching radio %d to channel %d (%.3f MHz)", 
    (int) radio, (int) channel, lora_channel[channel].frequency_in_mhz);
  serial_writeln(info_msg);

  // set frequency
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setFrequency(lora_channel[channel].frequency_in_mhz);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setFrequency(lora_channel[channel].frequency_in_mhz);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_frequency(radio, channel, lora_channel[channel].frequency_in_mhz);
  }
  if (state == RADIOLIB_ERR_INVALID_FREQUENCY)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to frequency %.3f MHz on radio %d", 
      (int) channel, lora_channel[channel].frequency_in_mhz, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  // set bandwidth
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setBandwidth(lora_channel[channel].bandwidth_in_khz);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setBandwidth(lora_channel[channel].bandwidth_in_khz);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_bandwidth(radio, channel, lora_channel[channel].bandwidth_in_khz);
  }
  if (state == RADIOLIB_ERR_INVALID_BANDWIDTH)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to bandwidth %3.0f kHz on radio %d", 
      (int) channel, lora_channel[channel].bandwidth_in_khz, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  // set spreading factor
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setSpreadingFactor(lora_channel[channel].spreading_factor);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setSpreadingFactor(lora_channel[channel].spreading_factor);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_spreading_factor(radio, channel, lora_channel[channel].spreading_factor);
  }
  if (state == RADIOLIB_ERR_INVALID_SPREADING_FACTOR)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to spreading factor %d on radio %d", 
      (int) channel, lora_channel[channel].spreading_factor, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  // set coding rate
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setCodingRate(lora_channel[channel].coding_rate);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setCodingRate(lora_channel[channel].coding_rate);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_coding_rate(radio, channel, lora_channel[channel].coding_rate);
  }
  if (state == RADIOLIB_ERR_INVALID_CODING_RATE)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to coding rate %d on radio %d", 
      (int) channel, lora_channel[channel].coding_rate, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  // set sync word
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setSyncWord(lora_channel[channel].syncword);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setSyncWord(lora_channel[channel].syncword);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_syncword(radio, channel, lora_channel[channel].syncword);
  }
  if (state == RADIOLIB_ERR_INVALID_SYNC_WORD)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to sync word %02X on radio %d", 
      (int) channel, lora_channel[channel].syncword, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  // set output power
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setOutputPower(lora_channel[channel].tx_power);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setOutputPower(lora_channel[channel].tx_power);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_txpower(radio, channel, lora_channel[channel].tx_power);
  }
  if (state == RADIOLIB_ERR_INVALID_OUTPUT_POWER)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to output power %d dBm on radio %d", 
      (int) channel, lora_channel[channel].tx_power, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  // set preamble length
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    state = sx1262_radios[radio_id_in_group]->setPreambleLength(lora_channel[channel].preamble_length);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    state = sx1268_radios[radio_id_in_group]->setPreambleLength(lora_channel[channel].preamble_length);
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    state = air_set_preamble_length(radio, channel, lora_channel[channel].preamble_length);
  }
  if (state == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH)
  {
    snprintf(info_msg, INFOLEN, "ERROR: Channel %d cannot be set to preamble length %d symbols on radio %d", 
      (int) channel, (int) lora_channel[channel].preamble_length, (int) radio);
    serial_writeln(info_msg);
    return false;
  }

  channel_used_by_radio[radio] = channel;
  return true;
}

int radio_start_receive(uint8_t radio, uint8_t channel_to_use, bool forced_switch)
{
  uint8_t channel = channel_to_use;
  if (channel == CURRENT_CHANNEL) channel = channel_used_by_radio[radio];

  if ((channel_used_by_radio[radio] != channel) || forced_switch)
  {
    radio_switch_to_channel(radio, channel, forced_switch);
  }

  if (pinout[radio].lora_rxen != -1)
  {
    digitalWrite(pinout[radio].lora_txen, LOW);
    delay(1);
    digitalWrite(pinout[radio].lora_rxen, HIGH);
  }

  int radio_id_in_group = pinout[radio].radio_id_in_group;
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    return sx1262_radios[radio_id_in_group]->startReceive();
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    return sx1268_radios[radio_id_in_group]->startReceive();
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    return air_start_receive(radio, channel);
  }

  return -1;
}

int radio_send_now(uint8_t radio, uint8_t channel_to_use, char *s, bool forced_switch)
{
  uint8_t channel = channel_to_use;
  if (channel == CURRENT_CHANNEL) channel = channel_used_by_radio[radio];

  if ((channel_used_by_radio[radio] != channel) || forced_switch)
  {
    radio_switch_to_channel(radio, channel);
  }

  if (pinout[radio].lora_txen != -1)
  {
    digitalWrite(pinout[radio].lora_rxen, LOW);
    delay(1);
    digitalWrite(pinout[radio].lora_txen, HIGH);
    delay(1);
  }
  if (lora_channel[channel].send_enabled)
  {
    int radio_id_in_group = pinout[radio].radio_id_in_group;
    if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
    {
      sx1262_radios[radio_id_in_group]->transmit(s);
    }
    else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
    {
      sx1268_radios[radio_id_in_group]->transmit(s);
    }
    else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
    {
      air_send_now(radio, channel, s);
    }
  }
  else 
  {
    char info_msg[INFOLEN];
    snprintf(info_msg, INFOLEN, "INFO: Sending on channel %d (%.3f MHz) disabled", 
      (int) channel, lora_channel[channel].frequency_in_mhz);
    serial_writeln(info_msg);
  }
  transmission_flag[channel] = false;
  return radio_start_receive(radio, channel);
}

int radio_start_send(uint8_t radio, uint8_t channel_to_use, const uint8_t* data, size_t len, bool forced_switch)
{
  uint8_t channel = channel_to_use;
  if (channel == CURRENT_CHANNEL) channel = channel_used_by_radio[radio];

  if ((channel_used_by_radio[radio] != channel) || forced_switch)
  {
    radio_switch_to_channel(radio, channel);
  }

  if (pinout[radio].lora_txen != -1)
  {
    digitalWrite(pinout[radio].lora_rxen, LOW);
    delay(1);
    digitalWrite(pinout[radio].lora_txen, HIGH);
    delay(1);
  }
  if (lora_channel[channel].send_enabled)
  {
    int radio_id_in_group = pinout[radio].radio_id_in_group;
    if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
    {
      sx1262_radios[radio_id_in_group]->startTransmit(data, len);
    }
    else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
    {
      sx1268_radios[radio_id_in_group]->startTransmit(data, len);
    }
    else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
    {
      air_start_send(radio, channel, data, len);
    }
  }
  else 
  {
    char info_msg[INFOLEN];
    snprintf(info_msg, INFOLEN, "INFO: Sending on channel %d (%.3f MHz) disabled", 
      (int) channel, lora_channel[channel].frequency_in_mhz);
    serial_writeln(info_msg);
  }
  return RADIOLIB_ERR_NONE;
}

#if defined(ESP32)
#ifdef VSPI
  SPIClass spi1 = SPIClass(VSPI);
  SPIClass spi2 = SPIClass(HSPI);
#else
  SPIClass spi1 = SPIClass(FSPI);
  SPIClass spi2 = SPIClass(HSPI);
#endif
#elif defined(USE_NRF52)
#endif

void lora_hardware_setup(void)
{
  char info_msg[INFOLEN];

  radio_basic_setup(); // sets the number of radios to initialize

  // Initialize the Serial ports for being AIR controlled
  for (int i=AIRMODEM_SERIAL0; i<AIRMODEM_SERIAL2; i++)
  {
    if (cfg.prepare_for_air[i]) air_setup(SPI_AIR_NONE+i);
  }

  /* Initialize channel states on radio initialization */
  for (uint8_t i=0; i<cfg.number_of_channels; i++) initialize_channel(i);

  for (uint8_t i=0; i<cfg.number_of_radios; i++)
  {
    snprintf(info_msg, INFOLEN, "INFO: Setting up radio %d", (int) i);
    serial_writeln(info_msg);

    if (pinout[i].radio_id_in_group == FIRST_IN_GROUP)
    {
      if (pinout[i].num_spi == SPI_AIR_NONE)
      {
        air_setup(SPI_AIR_NONE);
        radio_inject_modules(SPI_AIR_NONE);
      }
      else if (pinout[i].num_spi == SPI_AIR_ONE)
      {
        air_setup(SPI_AIR_ONE);
        radio_inject_modules(SPI_AIR_ONE);
      }
      else if (pinout[i].num_spi == SPI_AIR_TWO)
      {
        air_setup(SPI_AIR_TWO);
        radio_inject_modules(SPI_AIR_TWO);
      }
      else if (pinout[i].num_spi == SPI_ONE)
      {
        if (cfg.reset_spi_one)
        { 
#if defined(ESP32)
          spi1.begin(pinout[i].spi_clock, pinout[i].spi_miso, pinout[i].spi_mosi, pinout[i].spi_cs);
#elif defined(USE_NRF52)
          serial_writeln("INFO: Setting up LORA_SPI (nRF52)");
          lora_spi = SPIClass(NRF_SPIM0, pinout[i].spi_miso, pinout[i].spi_clock, pinout[i].spi_mosi);
          lora_spi.begin();
#endif
        }
        radio_inject_modules(SPI_ONE);
      }
      else if (pinout[i].num_spi == SPI_TWO)
      {
#if defined(ESP32)
        if (cfg.reset_spi_two) spi2.begin(pinout[i].spi_clock, pinout[i].spi_miso, pinout[i].spi_mosi, pinout[i].spi_cs);
#endif
        radio_inject_modules(SPI_TWO);
      }
    }

    if (pinout[i].lora_txen != -1)
    {
      pinMode(pinout[i].lora_rxen, OUTPUT);
      pinMode(pinout[i].lora_txen, OUTPUT);
      digitalWrite(pinout[i].lora_txen, LOW);
      digitalWrite(pinout[i].lora_rxen, HIGH);
    }

    int state;
    int radio_id_in_group = pinout[i].radio_id_in_group;

#if defined(ESP32)
    // With radios injected, perform wake-up pin configuration if necessary
    if (cfg.woken_from_deep_sleep) sleep_restore_after_wakeup(OPTION_DISABLED);
#endif

    if (pinout[i].radio_type == RADIO_TYPE_SX1262)
    {
#if defined(ESP32)
      state = sx1262_radios[radio_id_in_group]->begin();
#elif defined(USE_NRF52)
      uint8_t c = pinout[i].default_channel;
      state = sx1262_radios[radio_id_in_group]->begin(lora_channel[c].frequency_in_mhz, 
                                                      lora_channel[c].bandwidth_in_khz,
                                                      lora_channel[c].spreading_factor, 
                                                      lora_channel[c].coding_rate,
                                                      lora_channel[c].syncword, 
                                                      lora_channel[c].tx_power,
                                                      lora_channel[c].preamble_length,
                                                      1.8, false);
#endif
    }
    else if (pinout[i].radio_type == RADIO_TYPE_SX1268)
    {
      state = sx1268_radios[radio_id_in_group]->begin();
    }
    else if (pinout[i].radio_type == RADIO_TYPE_AIR)
    {
      state = air_begin(i);
    }
    else 
    {
      serial_writeln("ERROR: Unknown LoRa radio type, cannot initialize (partial implementation)");
      state = -1;
    }
    
    char info_msg[INFOLEN];
    if (state == RADIOLIB_ERR_NONE)
    {
      snprintf(info_msg, INFOLEN, "INIT: Radio %d hardware initialized successfully.", (int) i);
      has_radio[i] = true;
    } 
    else 
    {
      snprintf(info_msg, INFOLEN, "INIT: ERROR: Radio %d failed to initialize. Error code: %d", (int) i, (int) state);
      has_radio[i] = false;
    }
    serial_writeln(info_msg);

    channel_used_by_radio[i] = pinout[i].default_channel;
    enable_interrupt[i]      = true;
    transmission_flag[i]     = false;
    // if waking from deep sleep, the waking radio's DIO1 ISR will not have been triggered, 
    // so we set the flag manually to allow it to proceed with reception handling
    if (i == cfg.waking_radio) 
    { 
      transmission_flag[i] = true; 
      snprintf(info_msg, INFOLEN, "INFO: Flagging radio %d for LoRa reception handling during deep sleep", i);
      serial_writeln(info_msg);
    }
    cad_mode[i]              = false;
  }

  serial_writeln("INFO: Finished hardware setup");
  
  return;
}

void serial_write_incoming_message(uint8_t channel)
{
  char info[LONGINFOLEN];
  int encodedLength;

  encodedLength = Base64ren.encodedLength(lora_queue_rx[channel].payload_length);
  char encodedString[encodedLength + 1];
  Base64ren.encode(encodedString, (char *) lora_queue_rx[channel].payload, lora_queue_rx[channel].payload_length);

  snprintf(info, LONGINFOLEN, "RX %s", encodedString);
  serial_writeln(info);
  return;
}

#if defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlagRadio0(void)
{
  if (!enable_interrupt[RADIO0]) return;
  transmission_flag[RADIO0] = true;
  return;
}

#if defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlagRadio1(void)
{
  if (!enable_interrupt[RADIO1]) return;
  transmission_flag[RADIO1] = true;
  return;
}

#if defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlagRadio2(void)
{
  if (!enable_interrupt[RADIO2]) return;
  transmission_flag[RADIO2] = true;
  return;
}

#if defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlagRadio3(void)
{
  if (!enable_interrupt[RADIO3]) return;
  transmission_flag[RADIO3] = true;
  return;
} /* max of 4 radios per device assumed; amend further ISRs if necessary (partial implementation) */

void radio_set_transmission_flag(uint8_t radio, bool flag)
{
  transmission_flag[radio] = flag;
  return;
}

bool radio_setup(void)
{
  char info_msg[INFOLEN];

#if defined(ESP32)
  highlander = xSemaphoreCreateBinary();
  assert(highlander);
  xSemaphoreGive(highlander);
#endif

  for (uint8_t i=0; i<cfg.number_of_radios; i++)
  {
    if (!has_radio[i]) continue;
    int radio_id_in_group = pinout[i].radio_id_in_group;
    int state = RADIOLIB_ERR_NONE;

    radio_switch_to_channel(i, channel_used_by_radio[i], FORCED_CHANNEL_SWITCH);

    // set current limit 
    if (pinout[i].radio_type == RADIO_TYPE_SX1262)
    {
      state = sx1262_radios[radio_id_in_group]->setCurrentLimit(RADIOLIB_CURRENT_LIMIT);
    }
    else if (pinout[i].radio_type == RADIO_TYPE_SX1268)
    {
      state = sx1268_radios[radio_id_in_group]->setCurrentLimit(RADIOLIB_CURRENT_LIMIT);
    }
    else if (pinout[i].radio_type == RADIO_TYPE_AIR)
    {
      state = air_current_limit(i, RADIOLIB_CURRENT_LIMIT);
    }
    if (state == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
      snprintf(info_msg, INFOLEN, "ERROR: Radio %d cannot be set to RadioLib current limit %d", (int) i, RADIOLIB_CURRENT_LIMIT);
      serial_writeln(info_msg);
      return false;
    }

    // set RadioLib / LoRa chip CRC handlung
    if (pinout[i].radio_type == RADIO_TYPE_SX1262)
    {
      state = sx1262_radios[radio_id_in_group]->setCRC(RADIOLIB_CHECK_CRC);
    }
    else if (pinout[i].radio_type == RADIO_TYPE_SX1268)
    {
      state = sx1268_radios[radio_id_in_group]->setCRC(RADIOLIB_CHECK_CRC);
    }
    else if (pinout[i].radio_type == RADIO_TYPE_AIR)
    {
      state = air_set_crc(i, RADIOLIB_CHECK_CRC);
    }
    if (state == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION)
    {
      snprintf(info_msg, INFOLEN, "ERROR: Radio %d cannot be set to CRC handling option", (int) i);
      serial_writeln(info_msg);
      return false;
    }

    // Set DIO1 ISR
    if (pinout[i].radio_type == RADIO_TYPE_SX1262)
    {
      if (i == 0) { sx1262_radios[radio_id_in_group]->setDio1Action(setFlagRadio0); }
      else if (i == 1) { sx1262_radios[radio_id_in_group]->setDio1Action(setFlagRadio1); }
      else if (i == 2) { sx1262_radios[radio_id_in_group]->setDio1Action(setFlagRadio2); }
      else if (i == 3) { sx1262_radios[radio_id_in_group]->setDio1Action(setFlagRadio3); }
      else { serial_writeln("ERROR: Out of available ISRs (partial implementation)"); }
    }
    else if (pinout[i].radio_type == RADIO_TYPE_SX1268)
    {
      if (i == 0) { sx1268_radios[radio_id_in_group]->setDio1Action(setFlagRadio0); }
      else if (i == 1) { sx1268_radios[radio_id_in_group]->setDio1Action(setFlagRadio1); }
      else if (i == 2) { sx1268_radios[radio_id_in_group]->setDio1Action(setFlagRadio2); }
      else if (i == 3) { sx1268_radios[radio_id_in_group]->setDio1Action(setFlagRadio3); }
      else { serial_writeln("ERROR: Out of available ISRs (partial implementation)"); }
    }

#if defined(ESP32)
    if (xSemaphoreTake(highlander, portMAX_DELAY) == pdTRUE)
#endif
    {
      if (has_radio[i])
      {
        state = radio_start_receive(i, channel_used_by_radio[i]);
        if (state == RADIOLIB_ERR_NONE)
        {
          snprintf(info_msg, INFOLEN, "INIT: Parameters for channel %d set successfully and receiving with radio %d", 
            (int) channel_used_by_radio[i], (int) i);
        }
        else
        {
          snprintf(info_msg, INFOLEN, "ERROR: Parameters for channel %d set, but receiving state %d on radio %d", 
            (int) channel_used_by_radio[i], state, (int) i);
        }
        serial_writeln(info_msg);
      }
#if defined(ESP32)
      xSemaphoreGive(highlander);
#endif
    }
  }

  return true;
}

void radio_loop(void)
{
  char info_msg[INFOLEN];

  for (uint8_t i=0; i<cfg.number_of_radios; i++)
  {
    if (!has_radio[i]) continue;
    uint8_t current_channel = channel_used_by_radio[i];

#if defined(ESP32)
    if (xSemaphoreTake(highlander, portMAX_DELAY) == pdTRUE)
#endif
    {
      if (has_msg_to_send[current_channel])
      {
        if (msg_on_the_way[current_channel])
        {
          if (transmission_flag[i])
          {
            enable_interrupt[i] = false;
            transmission_flag[i] = false;
            if (transmission_state[current_channel] == RADIOLIB_ERR_NONE)
            {
              snprintf(info_msg, INFOLEN, "INFO: LoRa transmission on channel %d (%.3f MHz) successfully finished by radio %d!", 
                (int) current_channel, lora_channel[current_channel].frequency_in_mhz, (int) i);
              serial_writeln(info_msg);
              snprintf(info_msg, INFOLEN, "INFO: Transmission wallclock time was %d ms", 
                (int) (my_millis() - start_of_transmission[current_channel]));
              serial_writeln(info_msg);
            }
            else 
            {
              snprintf(info_msg, INFOLEN, "ERROR: LoRa transmission on channel %d failed, code %d, by radio %d", 
                (int) current_channel, (int) transmission_state[current_channel], (int) i);
              serial_writeln(info_msg);
            }
            has_msg_to_send[current_channel] = false;
            msg_on_the_way[current_channel] = false;
            radio_start_receive(i, current_channel);
            enable_interrupt[i] = true;
            scheduler_callback_txfin(i, current_channel);
            callback_tx_finished(i, current_channel);
            if (pinout[i].radio_type != RADIO_TYPE_AIR) air_fan_callback_txfin(i, current_channel);
          } // DIO1 transmission flag was set
        } // message was already on the way
        else 
        {
          snprintf(info_msg, INFOLEN, "INFO: Transmitting LoRa message now, length %d bytes, on %.3f MHz (radio %d, channel %d)", 
            (int) lora_queue_tx[current_channel].payload_length, 
            lora_channel[current_channel].frequency_in_mhz, 
            (int) i, (int) current_channel);
          serial_writeln(info_msg);
          start_of_transmission[current_channel] = my_millis();
          transmission_state[current_channel] = radio_start_send(i, current_channel, lora_queue_tx[current_channel].payload, lora_queue_tx[current_channel].payload_length);
          msg_on_the_way[current_channel] = true;
          enable_interrupt[i] = true;
          callback_tx_started(i, current_channel);
          if (pinout[i].radio_type != RADIO_TYPE_AIR) air_fan_callback_txstart(i, current_channel);
        } // message still needs to be sent
      } // has message to send
      else 
      { // nothing to send right now, check whether we have received a LoRa packet or have CAD results
        if (transmission_flag[i])
        { // LoRa chip has signalled some action
          enable_interrupt[i] = false;
          transmission_flag[i] = false;
          if (cad_mode[i])
          { // we were in CAD mode, not actually receiving a LoRa packet
            cad_mode[i] = false;
            int cad_state = CAD_STATE_FREE;
            int radio_id = pinout[i].radio_id_in_group;
            if (pinout[i].radio_type == RADIO_TYPE_SX1262)
            {
              cad_state = sx1262_radios[radio_id]->getChannelScanResult();
            }
            else if (pinout[i].radio_type == RADIO_TYPE_SX1268)
            {
              cad_state = sx1268_radios[radio_id]->getChannelScanResult();
            }
            else if (pinout[i].radio_type == RADIO_TYPE_AIR)
            {
              cad_state = air_get_channel_scan_result(i);
            }
            radio_start_receive(i, current_channel);
            enable_interrupt[i] = true;
            if (cad_state == RADIOLIB_LORA_DETECTED)
            {
              scheduler_callback_cad(i, current_channel, CAD_CHANNEL_BUSY);
              callback_cad_result(i, current_channel, CAD_CHANNEL_BUSY);
            }
            else 
            {
              scheduler_callback_cad(i, current_channel, CAD_CHANNEL_FREE);
              callback_cad_result(i, current_channel, CAD_CHANNEL_FREE);
            }
            if (pinout[i].radio_type != RADIO_TYPE_AIR) air_fan_callback_cadresult(i, current_channel, cad_state);
          } // CAD results were available 
          else 
          { // we have received a LoRa packet
            byte rx_buffer[MAX_LORA_PAYLOAD_SIZE];
            int rx_num_bytes = COUNT_ZERO;
            int rx_state = COUNT_ZERO;
            double rx_rssi = NO_RSSI;
            double rx_snr = NO_SNR;

            int radio_id_in_group = pinout[i].radio_id_in_group;
            if (pinout[i].radio_type == RADIO_TYPE_SX1262)
            {
              rx_num_bytes = sx1262_radios[radio_id_in_group]->getPacketLength();
              if (rx_num_bytes > MAX_LORA_PAYLOAD_SIZE) rx_num_bytes = MAX_LORA_PAYLOAD_SIZE;
              rx_state = sx1262_radios[radio_id_in_group]->readData(rx_buffer, rx_num_bytes);
              rx_rssi = sx1262_radios[radio_id_in_group]->getRSSI();
              rx_snr = sx1262_radios[radio_id_in_group]->getSNR();
            }
            else if (pinout[i].radio_type == RADIO_TYPE_SX1268)
            {
              rx_num_bytes = sx1268_radios[radio_id_in_group]->getPacketLength();
              if (rx_num_bytes > MAX_LORA_PAYLOAD_SIZE) rx_num_bytes = MAX_LORA_PAYLOAD_SIZE;
              rx_state = sx1268_radios[radio_id_in_group]->readData(rx_buffer, rx_num_bytes);
              rx_rssi = sx1268_radios[radio_id_in_group]->getRSSI();
              rx_snr = sx1268_radios[radio_id_in_group]->getSNR();
            }
            else if (pinout[i].radio_type == RADIO_TYPE_AIR)
            {
              rx_num_bytes = air_get_packet_length(i);
              if (rx_num_bytes > MAX_LORA_PAYLOAD_SIZE) rx_num_bytes = MAX_LORA_PAYLOAD_SIZE;
              rx_state = air_read_data(i, rx_buffer, rx_num_bytes);
              rx_rssi = air_get_rssi(i);
              rx_snr = air_get_snr(i);
            }

            if ((rx_state == RADIOLIB_ERR_NONE) || (rx_state == RADIOLIB_ERR_CRC_MISMATCH))
            {
              if ((rx_state == RADIOLIB_ERR_NONE) || (!RADIOLIB_CHECK_CRC))
              {
                if (pinout[i].radio_type != RADIO_TYPE_AIR) air_fan_callback_rx(i, rx_rssi, rx_snr, rx_num_bytes, rx_buffer);
                if (rx_num_bytes > 0)
                {
                  snprintf(info_msg, INFOLEN, "INFO: LoRa radio %d received packet on channel %d", 
                    (int) i, (int) current_channel);
                  serial_writeln(info_msg);

                  lora_queue_rx[current_channel].available = true;
                  lora_queue_rx[current_channel].channel = current_channel;
                  lora_queue_rx[current_channel].radio = i;
                  lora_queue_rx[current_channel].rssi = rx_rssi; 
                  lora_queue_rx[current_channel].snr = rx_snr; 
                  lora_queue_rx[current_channel].timestamp = my_millis();
                  lora_queue_rx[current_channel].payload_length = rx_num_bytes;
                  for (int j=0; j != rx_num_bytes; j++) lora_queue_rx[current_channel].payload[j] = rx_buffer[j];

                  if (cfg.print_rxmeta_lines)
                  {
                    snprintf(info_msg, INFOLEN, "RXMETA %d %.2f %.2f %.3f", 
                      (int) lora_queue_rx[current_channel].payload_length, lora_queue_rx[current_channel].rssi, 
                      lora_queue_rx[current_channel].snr, lora_channel[current_channel].frequency_in_mhz);
                    serial_writeln(info_msg);
                  }
                  if (cfg.print_rx_lines) serial_write_incoming_message(current_channel);
                }
                else 
                {
                  snprintf(info_msg, INFOLEN, "INFO: LoRa radio %d received empty packet on channel %d", 
                    (int) i, (int) current_channel);
                  serial_writeln(info_msg);
                }
              }
              else 
              {
                snprintf(info_msg, INFOLEN, "INFO: LoRa packet received by radio %d had bad LoRa CRC on channel %d", 
                  (int) i, (int) current_channel);
                serial_writeln(info_msg);
              }
            }
            else 
            {
              snprintf(info_msg, INFOLEN, "ERROR: LoRa packet receiving failed on channel %d, code %d, radio %d",
                (int) current_channel, rx_state, (int) i);
                serial_writeln(info_msg);
            }

            radio_start_receive(i, current_channel);
            enable_interrupt[i] = true;
          } // LoRa packet was received
        } // transmission flag was set
      } // checked whether CAD results or new received LoRa packet were available
    } // semaphore taken
#if defined(ESP32)
    xSemaphoreGive(highlander);
#endif
  } // for each channel
  return;
}

void radio_send_message_binary(uint8_t radio, uint8_t channel_to_use, uint8_t *payload, uint8_t length, bool forced_switch)
{
  if (length == 0) return; // do not send empty LoRa packets
  if (length > MAX_LORA_PAYLOAD_SIZE) return; // do not send too long LoRa packets

  uint8_t channel = channel_to_use;
  if (channel == CURRENT_CHANNEL) channel = channel_used_by_radio[radio];

  if ((channel_used_by_radio[radio] != channel) || forced_switch)
  {
    radio_switch_to_channel(radio, channel);
  }

  for (int i=0; i != length; i++) { lora_queue_tx[channel].payload[i] = payload[i]; }
  lora_queue_tx[channel].payload_length = length;
  has_msg_to_send[channel] = true;
  return;
}

void radio_start_cad(uint8_t radio, uint8_t channel, bool forced_switch)
{
  int cad_channel = channel;
  if (cad_channel == CURRENT_CHANNEL) cad_channel = channel_used_by_radio[radio];

  if ((channel_used_by_radio[radio] != cad_channel) || forced_switch)
  {
    radio_switch_to_channel(radio, cad_channel, forced_switch);
  }

  cad_mode[radio] = true;

  if (pinout[radio].lora_rxen != -1)
  {
    digitalWrite(pinout[radio].lora_txen, LOW);
    delay(1);
    digitalWrite(pinout[radio].lora_rxen, HIGH);
  }

  int radio_id_in_group = pinout[radio].radio_id_in_group;
  if (pinout[radio].radio_type == RADIO_TYPE_SX1262)
  {
    sx1262_radios[radio_id_in_group]->startChannelScan();
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_SX1268)
  {
    sx1268_radios[radio_id_in_group]->startChannelScan();
  }
  else if (pinout[radio].radio_type == RADIO_TYPE_AIR)
  {
    air_start_channel_scan(radio);
  }

  return;
}

bool radio_get_has_radio(uint8_t radio)
{
  return has_radio[radio];
}

/* EOF rdcp-modem-lora-radio.cpp */