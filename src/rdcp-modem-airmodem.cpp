/* rdcp-modem-airmodem.cpp */

#include <Arduino.h>
#include "rdcp-modem-airmodem.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-callback.h"
#include "rdcp-modem-hal.h"
#include <RadioLib.h> 
#include <Base64ren.h>
#include "nsa.h"

char air_info[INFOLEN];       // Info message for Serial output
char air_msg[AIRMSGLEN];      // AIR message for exchange with a direct neighbor
char air_command[AIRMSGLEN];  // AIR command as part of an AIR message

bool air_serial_initialized[AIR_NUM_SERIAL+1] = { false, false, false };

extern device_config cfg;
extern radio_pinout pinout[];
extern lora_channel_config lora_channel[];
extern controlled_air_radio air_radios_air_none[MAX_NUMBER_OF_RADIOS];
extern controlled_air_radio air_radios_air_one [MAX_NUMBER_OF_RADIOS];
extern controlled_air_radio air_radios_air_two [MAX_NUMBER_OF_RADIOS];
extern provided_air_radio   air_radios_provided[MAX_NUMBER_OF_RADIOS];
extern SX1262* sx1262_radios[MAX_NUMBER_OF_RADIOS];
extern SX1268* sx1268_radios[MAX_NUMBER_OF_RADIOS];

extern String serial_input;
extern nsa_global nsa;

/* Variables for processing AIR message inputs */
char   air_in[AIRMSGLEN];
int    ali_a, ali_b, ali_c, ali_d;
char   ali_text[AIRMSGLEN], ali_text_sub1[AIRMSGLEN], ali_text_sub2[AIRMSGLEN], ali_text_sub3[AIRMSGLEN], ali_text_sub4[AIRMSGLEN];
int    ali_serial = AIRMODEM_SERIAL_NONE;

void air_setup(int air_spi_interface)
{
  int serial_num = air_spi_interface - SPI_AIR_NONE; // [64..66] -> [0..2]
  if ((serial_num > AIR_NUM_SERIAL) || (serial_num < 0))
  { 
    serial_writeln("ERROR: Invalid AIR interface specified");
    return;
  }
  air_serial_initialized[serial_num] = true;

  if (air_spi_interface == SPI_AIR_NONE)
  {
    serial_writeln("INFO: Primary Serial/UART interface has been set up for AIR modem use");
  }
  else if (air_spi_interface == SPI_AIR_ONE)
  {
#if defined(ESP32)
    snprintf(air_info, INFOLEN, "INFO: Setting up Serial1 on RX(in) %d, TX(out) %d for AIR support (%d, timeout %d)", 
        cfg.serial1_rx, cfg.serial1_tx, SERIAL_AIR_BAUDRATE, SERIAL_AIR_TIMEOUT);
    serial_writeln(air_info);
    Serial1.begin(SERIAL_AIR_BAUDRATE, SERIAL_8N1, cfg.serial1_rx, cfg.serial1_tx);
#elif defined(USE_NRF52)
    serial_writeln("INFO: Setting up Serial1 for AIR radio communication on default pins (nRF52 implementation)");
    Serial1.begin(SERIAL_AIR_BAUDRATE, SERIAL_8N1);
#endif
#if defined(ESP32) || defined(USE_NRF52)
    Serial1.setTimeout(SERIAL_AIR_TIMEOUT);
#endif
  }
  else if (air_spi_interface == SPI_AIR_TWO)
  {
    snprintf(air_info, INFOLEN, "INFO: Setting up Serial2 on RX(in) %d, TX(out) %d for AIR support (%d, timeout %d)", 
        cfg.serial2_rx, cfg.serial2_tx, SERIAL_AIR_BAUDRATE, SERIAL_AIR_TIMEOUT);
    serial_writeln(air_info);
#if defined(ESP32)
    Serial2.begin(SERIAL_AIR_BAUDRATE, SERIAL_8N1, cfg.serial2_rx, cfg.serial2_tx);
    Serial2.setTimeout(SERIAL_AIR_TIMEOUT);
#endif
  }
  else 
  {
    serial_writeln("WARNING: AIR SPI interface has not been set up (partial implementation)");
  }

  return;
}

void air_fan_out(uint8_t radio, uint8_t channel, char *content)
{
  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  uint8_t remote_radio_id;
  uint8_t hops_to_target;

  if (air_spi_num == SPI_AIR_NONE)
  {
    remote_radio_id = air_radios_air_none[air_radio_id_in_group].remote_radio;
    hops_to_target = air_radios_air_none[air_radio_id_in_group].airhops;
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    remote_radio_id = air_radios_air_one[air_radio_id_in_group].remote_radio;
    hops_to_target = air_radios_air_one[air_radio_id_in_group].airhops;
    if (!air_serial_initialized[SPI_AIR_ONE-SPI_AIR_NONE])
    {
      serial_writeln("ERROR: AIR fan out inhibited by uninitialized Serial1");
      return;
    }
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    remote_radio_id = air_radios_air_two[air_radio_id_in_group].remote_radio;
    hops_to_target = air_radios_air_two[air_radio_id_in_group].airhops;
    if (!air_serial_initialized[SPI_AIR_TWO-SPI_AIR_NONE])
    {
      serial_writeln("ERROR: AIR fan out inhibited by uninitialized Serial2");
      return;
    }
  }
  else 
  {
    snprintf(air_info, INFOLEN, "ERROR: Cannot AIR fan out message for radio %d on channel %d (partial implementation)",
      (int) radio, (int) channel);
    serial_writeln(air_info);
    return;
  }

  snprintf(air_msg, AIRMSGLEN, "AIR %d %d %d %d %s\n",
    (int) hops_to_target, (int) AIRHOP_DIRECT_NEIGHBOR, (int) remote_radio_id, (int) radio, content);

  if (air_spi_num == SPI_AIR_NONE)
  {
    serial_write(air_msg, cfg.prefix_on_air_serial);
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
#if defined(ESP32) || defined(USE_NRF52)
    Serial1.print(air_msg);
#endif
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
#if defined(ESP32)
    Serial2.print(air_msg);
#endif
  }

  if (cfg.print_airinfo_lines)
  {
    snprintf(air_info, INFOLEN, "INFO: AIR OUT on Serial%d: %s",
      (int) (air_spi_num - SPI_AIR_NONE), air_msg);
    serial_writeln(air_info);
  }

  return;
}

void air_fan_back(uint8_t provided_radio, char* content)
{
  // If the Serial connection has not been set up yet, we cannot AIR BACK.
  // This is important to suppress callback messages for radios not controlled remotely
  if (air_radios_provided[provided_radio].serial_to_controller == AIRMODEM_SERIAL_NONE) return;

  int hops_to_target  = air_radios_provided[provided_radio].airhops;
  int remote_radio_id = air_radios_provided[provided_radio].remote_radio;
  int local_radio_id  = provided_radio; // 1:1 mapping of provided to physical radios
  int serial_to_use   = air_radios_provided[provided_radio].serial_to_controller;

  snprintf(air_msg, AIRMSGLEN, "AIR %d %d %d %d %s\n",
    (int) hops_to_target, (int) AIRHOP_DIRECT_NEIGHBOR, (int) local_radio_id, (int) remote_radio_id, content); // NB: c = my radio_id, d = controller's radio_id

  if (serial_to_use == AIRMODEM_SERIAL0)
  {
    serial_write(air_msg, cfg.prefix_on_air_serial);
  }
  else if (serial_to_use == AIRMODEM_SERIAL1)
  {
#if defined(ESP32) || defined(USE_NRF52)
    Serial1.print(air_msg);
#endif
  }
  else if (serial_to_use == AIRMODEM_SERIAL2)
  {
#if defined(ESP32)
    Serial2.print(air_msg);
#endif
  }
  else 
  {
    serial_writeln("WARNING: No Serial interface for AIR BACK (partial implementation)");
  }

  if (cfg.print_airinfo_lines)
  {
    snprintf(air_info, INFOLEN, "INFO: AIR BACK on Serial%d: %s",
      serial_to_use, air_msg);
    serial_writeln(air_info);
  }

  return;
}

void air_fan_callback_txstart(uint8_t real_radio, uint8_t channel)
{
  snprintf(air_command, AIRMSGLEN, "cb_TXSTART %02d", (int) channel);
  air_fan_back(real_radio, air_command);
  return;
}

void air_fan_callback_txfin(uint8_t real_radio, uint8_t channel)
{
  snprintf(air_command, AIRMSGLEN, "cb_TXFIN %02d", (int) channel);
  air_fan_back(real_radio, air_command);
  return;
}

void air_fan_callback_cadresult(uint8_t real_radio, uint8_t channel, int cad_state)
{
  snprintf(air_command, AIRMSGLEN, "cb_CAD %02d %d", (int) channel, cad_state == RADIOLIB_CHANNEL_FREE ? 0 : 1);
  air_fan_back(real_radio, air_command);
  return;
}

void air_fan_callback_rx(uint8_t real_radio, double rssi, double snr, uint8_t length, uint8_t* data)
{
  int encodedLength = Base64ren.encodedLength(length);
  char base64string[encodedLength + 1];
  Base64ren.encode(base64string, (char *) data, length);

  snprintf(air_command, AIRMSGLEN, "cb_RX %8.3f %8.3f %03d %s", rssi, snr, (int) length, base64string);
  air_fan_back(real_radio, air_command);
  return;
}

void air_loop_process_message()
{ 
  if (nsa_startsWith(ali_text, "do_BEGIN"))
  { // do_BEGIN
    // 01234567

    // initialize the local radio if it is not initialized yet
    // store relationship between controller's radio `d` and my local radio `c` as well as number of hops `b`
    // trigger the creation of a first random number
    if (!radio_get_has_radio(ali_c))
    {
      lora_hardware_setup(); // set up all local radios, only once (subject to later change)
    }

    air_radios_provided[ali_c].airhops = ali_b;
    air_radios_provided[ali_c].serial_to_controller = ali_serial;
    air_radios_provided[ali_c].remote_radio = ali_d;

    uint8_t random_number = radio_random_byte(ali_c);
    snprintf(air_command, AIRMSGLEN, "cb_RNG %03d", (int) random_number);
    air_fan_back(ali_c, air_command);
  }
  else if (nsa_startsWith(ali_text, "do_RX_ON_CHANNEL "))
  {
    // do_RX_ON_CHANNEL 04
    // 0123456789012345678
    nsa_substring(ali_text, 17);
    int channel = strtol(nsa.result, NULL, BASE10);
    snprintf(air_info, INFOLEN, "INFO: AIR duty: Starting receiving on channel %d with radio %d", channel, ali_c);
    serial_writeln(air_info);
    radio_start_receive(ali_c, channel, FORCED_CHANNEL_SWITCH);
  }
  else if (nsa_startsWith(ali_text, "do_TX_ON_CHANNEL "))
  {
    // do_TX_ON_CHANNEL 04 plaintext
    // 0123456789012345678901234
    nsa_substring(ali_text, 17, 19);
    int channel = strtol(nsa.result, NULL, BASE10);

    nsa_substring(ali_text, 20);
    radio_send_message_binary(ali_c, channel, (uint8_t*) nsa.result, strlen(nsa.result), FORCED_CHANNEL_SWITCH);
  }
  else if (nsa_startsWith(ali_text, "do_TXSTART_ON_CHANNEL "))
  {
    // do_TXSTART_ON_CHANNEL 04 123 base64content
    // 012345678901234567890123456789
    nsa_substring(ali_text, 22, 24);
    int channel = strtol(nsa.result, NULL, BASE10);

    nsa_substring(ali_text, 25, 28);
    int length = strtol(nsa.result, NULL, BASE10);

    nsa_substring(ali_text, 29);
    strncpy(ali_text_sub3, nsa.result, AIRMSGLEN);
    int b64content_len = strlen(ali_text_sub3);
    int decoded_length = Base64ren.decodedLength(ali_text_sub3, b64content_len);
    char decoded_string[decoded_length + 1];
    Base64ren.decode(decoded_string, ali_text_sub3, b64content_len);
    uint8_t data[MAX_LORA_PAYLOAD_SIZE];
    for (int i=0; i<decoded_length; i++)
    {
      if (i < MAX_LORA_PAYLOAD_SIZE) data[i] = decoded_string[i];
    }
    radio_send_message_binary(ali_c, channel, data, length, FORCED_CHANNEL_SWITCH);
  }
  else if (nsa_startsWith(ali_text, "do_CAD"))
  { // do_CAD
    // 012345
    radio_start_cad(ali_c, CURRENT_CHANNEL, FORCED_CHANNEL_SWITCH);
  }
  else if (nsa_startsWith(ali_text, "do_RNG"))
  { // do_RNG
    // 012345
    uint8_t random_number = radio_random_byte(ali_c);
    snprintf(air_command, AIRMSGLEN, "cb_RNG %03d", (int) random_number);
    air_fan_back(ali_c, air_command);
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_FREQUENCY "))
  {
    // set_CHANNEL_FREQUENCY 04 123.456
    // 012345678901234567890123456789
    nsa_substring(ali_text, 22, 24);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 25);
    double freq = strtod(nsa.result, NULL);
    lora_channel[channel].frequency_in_mhz = freq;
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_BANDWIDTH "))
  {
    // set_CHANNEL_BANDWIDTH 04 123.456
    // 012345678901234567890123456789
    nsa_substring(ali_text, 22, 24);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 25);
    double bw = strtod(nsa.result, NULL);
    lora_channel[channel].bandwidth_in_khz = bw;
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_SPREADING_FACTOR "))
  {
    // set_CHANNEL_SPREADING_FACTOR 04 07
    // 0123456789012345678901234567890123
    nsa_substring(ali_text, 29, 31);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 32);
    int sf = strtol(nsa.result, NULL, BASE10);
    lora_channel[channel].spreading_factor = sf;
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_CODING_RATE "))
  {
    // set_CHANNEL_CODING_RATE 04 07
    // 0123456789012345678901234567890123
    nsa_substring(ali_text, 24, 26);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 27);
    int cr = strtol(nsa.result, NULL, BASE10);
    lora_channel[channel].coding_rate = cr;
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_SYNCWORD "))
  {
    // set_CHANNEL_SYNCWORD 04 12
    // 0123456789012345678901234567890123
    nsa_substring(ali_text, 21, 23);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 24);
    uint8_t sw = strtol(nsa.result, NULL, BASE16) & 0xFF;
    lora_channel[channel].syncword = sw;
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_TXPOWER "))
  {
    // set_CHANNEL_TXPOWER 04 12
    // 0123456789012345678901234567890123
    nsa_substring(ali_text, 20, 22);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 23);
    int txpwr = strtol(nsa.result, NULL, BASE10);
    lora_channel[channel].tx_power = txpwr;
  }
  else if (nsa_startsWith(ali_text, "set_CHANNEL_PREAMBLE_LENGTH "))
  {
    // set_CHANNEL_PREAMBLE_LENGTH 04 15
    // 0123456789012345678901234567890123
    nsa_substring(ali_text, 28, 30);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 31);
    uint16_t pl = strtol(nsa.result, NULL, BASE10);
    lora_channel[channel].preamble_length = pl;
  }
  else if (nsa_startsWith(ali_text, "set_RADIO_CURRENT_LIMIT "))
  {
    // set_RADIO_CURRENT_LIMIT 123
    // 012345678901234567890123456
    nsa_substring(ali_text, 24);
    int climit = strtol(nsa.result, NULL, BASE10);

    int radio_id_in_group = pinout[ali_c].radio_id_in_group;
    if (pinout[ali_c].radio_type == RADIO_TYPE_SX1262)
    {
      sx1262_radios[radio_id_in_group]->setCurrentLimit(climit);
    }
    else if (pinout[ali_c].radio_type == RADIO_TYPE_SX1268)
    {
      sx1268_radios[radio_id_in_group]->setCurrentLimit(climit);
    }
    else 
    {
      serial_writeln("WARNING: Cannot AIR set current limit for radio (partial implementation)");
    }
  }
  else if (nsa_startsWith(ali_text, "set_RADIO_CRC "))
  {
    // set_RADIO_CRC 1
    // 01234567890123456
    nsa_substring(ali_text, 14);
    bool crc_setting = strtol(nsa.result, NULL, BASE10) > 0 ? true : false;

    int radio_id_in_group = pinout[ali_c].radio_id_in_group;
    if (pinout[ali_c].radio_type == RADIO_TYPE_SX1262)
    {
      sx1262_radios[radio_id_in_group]->setCRC(crc_setting);
    }
    else if (pinout[ali_c].radio_type == RADIO_TYPE_SX1268)
    {
      sx1268_radios[radio_id_in_group]->setCRC(crc_setting);
    }
    else 
    {
      serial_writeln("WARNING: Cannot AIR set CRC handling for radio (partial implementation)");
    }
  }
  else if (nsa_startsWith(ali_text, "cb_TXSTART "))
  {
    // cb_TXSTART 04
    // 0123456789012
    nsa_substring(ali_text, 11);
    int channel = strtol(nsa.result, NULL, BASE10);
    callback_tx_started(ali_d, channel, CALLBACK_AIR); // for callbacks, use radio number on controlling device
  }
  else if (nsa_startsWith(ali_text, "cb_TXFIN "))
  {
    // cb_TXFIN 04
    // 0123456789012
    nsa_substring(ali_text, 9);
    int channel = strtol(nsa.result, NULL, BASE10);
    callback_tx_finished(ali_d, channel, CALLBACK_AIR);
    radio_set_transmission_flag(ali_d);
  }
  else if (nsa_startsWith(ali_text, "cb_CAD "))
  {
    // cb_CAD 04 1
    // 0123456789012
    nsa_substring(ali_text, 7, 9);
    int channel = strtol(nsa.result, NULL, BASE10);
    nsa_substring(ali_text, 10);
    int cad_state = strtol(nsa.result, NULL, BASE10);

    bool cad_result = CAD_CHANNEL_FREE;
    if (cad_state > 0) cad_result = CAD_CHANNEL_BUSY;

    uint8_t air_radio_id_in_group = pinout[ali_d].radio_id_in_group;

    if (ali_serial == AIRMODEM_SERIAL0)
    {
      air_radios_air_none[air_radio_id_in_group].cad_result_available = true;
      air_radios_air_none[air_radio_id_in_group].cad_result = cad_result == CAD_CHANNEL_FREE ? RADIOLIB_CHANNEL_FREE : RADIOLIB_LORA_DETECTED;
    }
    else if (ali_serial == AIRMODEM_SERIAL1)
    {
      air_radios_air_one[air_radio_id_in_group].cad_result_available = true;
      air_radios_air_one[air_radio_id_in_group].cad_result = cad_result == CAD_CHANNEL_FREE ? RADIOLIB_CHANNEL_FREE : RADIOLIB_LORA_DETECTED;
    }
    else if (ali_serial == AIRMODEM_SERIAL2)
    {
      air_radios_air_two[air_radio_id_in_group].cad_result_available = true;
      air_radios_air_two[air_radio_id_in_group].cad_result = cad_result == CAD_CHANNEL_FREE ? RADIOLIB_CHANNEL_FREE : RADIOLIB_LORA_DETECTED;
    }
    callback_cad_result(ali_d, channel, cad_result, CALLBACK_AIR);
    radio_set_transmission_flag(ali_d);
  }
  else if (nsa_startsWith(ali_text, "cb_RNG "))
  {
    // cb_RNG 123
    // 0123456789012
    nsa_substring(ali_text, 7);
    int random_number = strtol(nsa.result, NULL, BASE10);

    uint8_t air_radio_id_in_group = pinout[ali_d].radio_id_in_group;

    if (ali_serial == AIRMODEM_SERIAL0)
    {
      air_radios_air_none[air_radio_id_in_group].random_number = random_number;
    }
    else if (ali_serial == AIRMODEM_SERIAL1)
    {
      air_radios_air_one[air_radio_id_in_group].random_number = random_number;
    }
    else if (ali_serial == AIRMODEM_SERIAL2)
    {
      air_radios_air_two[air_radio_id_in_group].random_number = random_number;
    }
  }
  else if (nsa_startsWith(ali_text, "cb_RX "))
  {
    // cb_RX -123.456 -123.456 123 base64content
    // 01234567890123456789012345678
    nsa_substring(ali_text, 6, 14);
    double rssi = strtod(nsa.result, NULL);

    nsa_substring(ali_text, 15, 23);
    double snr = strtod(nsa.result, NULL);

    nsa_substring(ali_text, 24, 27);
    int length_in_bytes = strtol(nsa.result, NULL, BASE10);

    nsa_substring(ali_text, 28);
    strncpy(ali_text_sub4, nsa.result, AIRMSGLEN);

    int b64content_len = strlen(ali_text_sub4);
    int decoded_length = Base64ren.decodedLength(ali_text_sub4, b64content_len);
    char decoded_string[decoded_length + 1];
    Base64ren.decode(decoded_string, ali_text_sub4, b64content_len);
    uint8_t data[MAX_LORA_PAYLOAD_SIZE];
    for (int i=0; i<decoded_length; i++)
    {
      if (i < MAX_LORA_PAYLOAD_SIZE) data[i] = decoded_string[i];
    }

    snprintf(air_info, INFOLEN, "INFO: AIR IN received LoRa packet, length %d, for radio %d, RSSI %.1f", 
      length_in_bytes, ali_d, rssi);
    serial_writeln(air_info);

    uint8_t air_radio_id_in_group = pinout[ali_d].radio_id_in_group;

    if (ali_serial == AIRMODEM_SERIAL0)
    {
      air_radios_air_none[air_radio_id_in_group].rx_buf.available = true;
      air_radios_air_none[air_radio_id_in_group].rx_buf.rssi = rssi;
      air_radios_air_none[air_radio_id_in_group].rx_buf.snr = snr;
      air_radios_air_none[air_radio_id_in_group].rx_buf.timestamp = my_millis();
      air_radios_air_none[air_radio_id_in_group].rx_buf.radio = ali_d;
      air_radios_air_none[air_radio_id_in_group].rx_buf.payload_length = length_in_bytes;
      air_radios_air_none[air_radio_id_in_group].rx_buf.channel = CURRENT_CHANNEL;
      for (int i=0; i<length_in_bytes; i++)
      {
        if (i < MAX_LORA_PAYLOAD_SIZE) air_radios_air_none[air_radio_id_in_group].rx_buf.payload[i] = data[i];
      }
    }
    else if (ali_serial == AIRMODEM_SERIAL1)
    {
      air_radios_air_one[air_radio_id_in_group].rx_buf.available = true;
      air_radios_air_one[air_radio_id_in_group].rx_buf.rssi = rssi;
      air_radios_air_one[air_radio_id_in_group].rx_buf.snr = snr;
      air_radios_air_one[air_radio_id_in_group].rx_buf.timestamp = my_millis();
      air_radios_air_one[air_radio_id_in_group].rx_buf.radio = ali_d;
      air_radios_air_one[air_radio_id_in_group].rx_buf.payload_length = length_in_bytes;
      air_radios_air_one[air_radio_id_in_group].rx_buf.channel = CURRENT_CHANNEL;
      for (int i=0; i<length_in_bytes; i++)
      {
        if (i < MAX_LORA_PAYLOAD_SIZE) air_radios_air_one[air_radio_id_in_group].rx_buf.payload[i] = data[i];
      }
    }
    else if (ali_serial == AIRMODEM_SERIAL2)
    {
      air_radios_air_two[air_radio_id_in_group].rx_buf.available = true;
      air_radios_air_two[air_radio_id_in_group].rx_buf.rssi = rssi;
      air_radios_air_two[air_radio_id_in_group].rx_buf.snr = snr;
      air_radios_air_two[air_radio_id_in_group].rx_buf.timestamp = my_millis();
      air_radios_air_two[air_radio_id_in_group].rx_buf.radio = ali_d;
      air_radios_air_two[air_radio_id_in_group].rx_buf.payload_length = length_in_bytes;
      air_radios_air_two[air_radio_id_in_group].rx_buf.channel = CURRENT_CHANNEL;
      for (int i=0; i<length_in_bytes; i++)
      {
        if (i < MAX_LORA_PAYLOAD_SIZE) air_radios_air_two[air_radio_id_in_group].rx_buf.payload[i] = data[i];
      }
    }
    radio_set_transmission_flag(ali_d);
  }
  else if (nsa_startsWith(ali_text, "SERIAL "))
  {
    // SERIAL text
    // 0123456789012
    nsa_substring(ali_text, 7);
    strncpy(ali_text_sub1, nsa.result, AIRMSGLEN);
    serial_process_command(ali_text_sub1, "AIRCMD: ");
  }
  else 
  {
    serial_writeln("WARNING: Unknown AIR message type received, cannot handle. (partial implementation)");
  }

  return;
}

void air_loop(bool has_serial0_input, const char* serial0_input)
{
  for (int i=AIRMODEM_SERIAL0; i<=AIR_NUM_SERIAL; i++)
  {
    if (!air_serial_initialized[i]) continue;

    if (i == AIRMODEM_SERIAL0)
    {
      if (!has_serial0_input) continue;
      strncpy(air_in, serial0_input, AIRMSGLEN);
    }
    else if (i == AIRMODEM_SERIAL1)
    {
#if defined(ESP32) || defined(USE_NRF52)
      serial_input = Serial1.readStringUntil('\n');
#else 
      serial_input = "";
#endif
      serial_input.toCharArray(air_in, SERIALINPUTLEN);
    }
    else if (i == AIRMODEM_SERIAL2)
    {
#if defined(ESP32)
      serial_input = Serial2.readStringUntil('\n');
#else 
      serial_input = "";
#endif
      serial_input.toCharArray(air_in, SERIALINPUTLEN);
    }
    if(!nsa_startsWith(air_in, "AIR ")) continue; // no new input received

    ali_serial = i;

    snprintf(air_info, INFOLEN, "INFO: AIR IN on Serial%d: %s", ali_serial, air_in);
    if (cfg.print_airinfo_lines) serial_writeln(air_info);

    // AIR Message format:
    // AIR a b c d text
    // 0123456789012
    nsa_strsplit(air_in, " ", 1);
    ali_a = strtol(nsa.result, NULL, BASE10);
    nsa_strsplit(air_in, " ", 2);
    ali_b = strtol(nsa.result, NULL, BASE10);
    nsa_strsplit(air_in, " ", 3);
    ali_c = strtol(nsa.result, NULL, BASE10);
    nsa_strsplit(air_in, " ", 4);
    ali_d = strtol(nsa.result, NULL, BASE10);
    nsa_substring(air_in, 12);
    strncpy(ali_text, nsa.result, AIRMSGLEN);
    
    /* 
      If we are the destination of the AIR message, we have to process it.
      Otherwise, we have to pass it on in the direction of the destination.
    */
    if (ali_a == AIRHOP_DIRECT_NEIGHBOR)
    {
      air_loop_process_message();
    }
    else 
    {
      ali_a = ali_a - 1; // Decrement "number of airhops to destination"
      ali_b = ali_b + 1; // Increment "number of airhops from source"

      if (ali_a < AIRHOP_DIRECT_NEIGHBOR) continue; // not needed in theory :)

      snprintf(air_msg, AIRMSGLEN, "AIR %d %d %d %d %s\n",
        ali_a, ali_b, ali_c, ali_d, ali_text);
      if (i == AIRMODEM_SERIAL1)
      {
        if (air_serial_initialized[SPI_AIR_NONE-SPI_AIR_NONE]) serial_write(air_msg, cfg.prefix_on_air_serial);
#if defined(ESP32)
        if (air_serial_initialized[SPI_AIR_TWO-SPI_AIR_NONE])  Serial2.print(air_msg);
#endif
      }
      else if (i == AIRMODEM_SERIAL2)
      {
        if (air_serial_initialized[SPI_AIR_NONE-SPI_AIR_NONE]) serial_write(air_msg, cfg.prefix_on_air_serial);
#if defined(ESP32) || defined(USE_NRF52)
        if (air_serial_initialized[SPI_AIR_ONE-SPI_AIR_NONE])  Serial1.print(air_msg);
#endif
      }
    } // pass the message on towards its destination
  } // iterate over Serial1/2 to check for new AIR messages
  return;
}

void air_send_message_binary(uint8_t radio, uint8_t channel, uint8_t *payload, uint8_t length)
{
  int encodedLength = Base64ren.encodedLength(length);
  char encodedString[encodedLength + 1];
  Base64ren.encode(encodedString, (char *) payload, length);

  snprintf(air_command, AIRMSGLEN, "do_TX_ON_CHANNEL %02d %03d %s", (int) channel, (int) length, encodedString);
  air_fan_out(radio, channel, air_command);
  return;
}

int16_t air_start_receive(uint8_t radio, uint8_t channel)
{
  snprintf(air_command, AIRMSGLEN, "do_RX_ON_CHANNEL %02d", (int) channel);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_frequency(uint8_t radio, uint8_t channel, double freq)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_FREQUENCY %02d %3.3f", (int) channel, freq);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_bandwidth(uint8_t radio, uint8_t channel, double bw)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_BANDWIDTH %02d %3.3f", (int) channel, bw);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_spreading_factor(uint8_t radio, uint8_t channel, int sf)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_SPREADING_FACTOR %02d %02d", (int) channel, sf);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_coding_rate(uint8_t radio, uint8_t channel, int cr)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_CODING_RATE %02d %d", (int) channel, cr);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_syncword(uint8_t radio, uint8_t channel, uint8_t syncword)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_SYNCWORD %02d %02X", (int) channel, (int) syncword);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_txpower(uint8_t radio, uint8_t channel, int tx_power)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_TXPOWER %02d %d", (int) channel, tx_power);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_preamble_length(uint8_t radio, uint8_t channel, uint16_t pl)
{
  snprintf(air_command, AIRMSGLEN, "set_CHANNEL_PREAMBLE_LENGTH %02d %d", (int) channel, (int) pl);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_send_now(uint8_t radio, uint8_t channel, char *s)
{
  snprintf(air_command, AIRMSGLEN, "do_TX_ON_CHANNEL %02d %s", (int) channel, s);
  air_fan_out(radio, channel, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_start_send(uint8_t radio, uint8_t channel, const uint8_t* data, size_t len)
{
  int encodedLength = Base64ren.encodedLength(len);
  char encodedString[encodedLength + 1];
  Base64ren.encode(encodedString, (char *) data, len);

  snprintf(air_command, AIRMSGLEN, "do_TXSTART_ON_CHANNEL %02d %03d %s", (int) channel, (int) len, encodedString);
  air_fan_out(radio, channel, air_command);

  return RADIOLIB_ERR_NONE;
}

int16_t air_begin(uint8_t radio)
{
  snprintf(air_command, AIRMSGLEN, "do_BEGIN");
  air_fan_out(radio, NO_CHANNEL, air_command); // do not send do_BEGIN if we are being remotely controlled
  return RADIOLIB_ERR_NONE;
}

int16_t air_current_limit(uint8_t radio, int climit)
{
  snprintf(air_command, AIRMSGLEN, "set_RADIO_CURRENT_LIMIT %03d", climit);
  air_fan_out(radio, NO_CHANNEL, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_set_crc(uint8_t radio, bool crcsetting)
{
  snprintf(air_command, AIRMSGLEN, "set_RADIO_CRC %d", crcsetting ? 1 : 0);
  air_fan_out(radio, NO_CHANNEL, air_command);
  return RADIOLIB_ERR_NONE;
}

int16_t air_get_channel_scan_result(uint8_t radio)
{
  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    if (air_radios_air_none[air_radio_id_in_group].cad_result_available)
    {
      air_radios_air_none[air_radio_id_in_group].cad_result_available = false;
      return air_radios_air_none[air_radio_id_in_group].cad_result; 
    }
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    if (air_radios_air_one[air_radio_id_in_group].cad_result_available)
    {
      air_radios_air_one[air_radio_id_in_group].cad_result_available = false;
      return air_radios_air_one[air_radio_id_in_group].cad_result; 
    }
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    if (air_radios_air_two[air_radio_id_in_group].cad_result_available)
    {
      air_radios_air_two[air_radio_id_in_group].cad_result_available = false;
      return air_radios_air_two[air_radio_id_in_group].cad_result; 
    }
  }

  snprintf(air_info, INFOLEN, "WARNING: AIR CAD results requested but not available for radio %d",
    (int) radio);
  serial_writeln(air_info);

  return RADIOLIB_CHANNEL_FREE;
}

int16_t air_start_channel_scan(uint8_t radio)
{
  snprintf(air_command, AIRMSGLEN, "do_CAD");
  air_fan_out(radio, NO_CHANNEL, air_command);

  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    air_radios_air_none[air_radio_id_in_group].cad_result_available = false;
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    air_radios_air_one[air_radio_id_in_group].cad_result_available = false;
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    air_radios_air_two[air_radio_id_in_group].cad_result_available = false;
  }

  return RADIOLIB_ERR_NONE;
}

int air_get_packet_length(uint8_t radio)
{
  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    return air_radios_air_none[air_radio_id_in_group].rx_buf.payload_length;
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    return air_radios_air_one[air_radio_id_in_group].rx_buf.payload_length;
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    return air_radios_air_two[air_radio_id_in_group].rx_buf.payload_length;
  }

  return 0;
}

int16_t air_read_data(uint8_t radio, uint8_t* rx_buffer, int num_bytes)
{
  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    for (int i=0; i<num_bytes; i++) rx_buffer[i] = air_radios_air_none[air_radio_id_in_group].rx_buf.payload[i];
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    for (int i=0; i<num_bytes; i++) rx_buffer[i] = air_radios_air_one[air_radio_id_in_group].rx_buf.payload[i];
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    for (int i=0; i<num_bytes; i++) rx_buffer[i] = air_radios_air_two[air_radio_id_in_group].rx_buf.payload[i];
  }
  return RADIOLIB_ERR_NONE;
}

double air_get_rssi(uint8_t radio)
{
  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    return air_radios_air_none[air_radio_id_in_group].rx_buf.rssi;
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    return air_radios_air_one[air_radio_id_in_group].rx_buf.rssi;
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    return air_radios_air_two[air_radio_id_in_group].rx_buf.rssi;
  }
  return 0.0;
}

double air_get_snr(uint8_t radio)
{
  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    return air_radios_air_none[air_radio_id_in_group].rx_buf.snr;
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    return air_radios_air_one[air_radio_id_in_group].rx_buf.snr;
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    return air_radios_air_two[air_radio_id_in_group].rx_buf.snr;
  }
  return 0.0;
}

uint8_t air_random_byte(uint8_t radio)
{
  // request next random number and deliver the most recently received one
  snprintf(air_command, AIRMSGLEN, "do_RNG");
  air_fan_out(radio, NO_CHANNEL, air_command);

  uint8_t air_radio_id_in_group = pinout[radio].radio_id_in_group;
  uint8_t air_spi_num = pinout[radio].num_spi;

  if (air_spi_num == SPI_AIR_NONE)
  {
    return air_radios_air_none[air_radio_id_in_group].random_number;
  }
  else if (air_spi_num == SPI_AIR_ONE)
  {
    return air_radios_air_one[air_radio_id_in_group].random_number;
  }
  else if (air_spi_num == SPI_AIR_TWO)
  {
    return air_radios_air_two[air_radio_id_in_group].random_number;
  }

  serial_writeln("WARNING: air_random_byte() has no randomness source available");
  return 0;
}

/* EOF rdcp-modem-airmodem.cpp */