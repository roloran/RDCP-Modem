/* rdcp-modem-serial.cpp */

#include <Arduino.h>
#include "rdcp-modem-serial.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-persistence.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-plugin.h"
#include "rdcp-modem-airmodem.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-scheduler.h"
#include "Base64ren.h"
#include "nsa.h"

#ifdef DEVICE_HAS_BLUETOOTH
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
#endif

#ifdef DEVICE_HAS_BLUETOOTH_LE
#include <BleSerial.h>
BleSerial SerialBLE;
#endif

extern device_config cfg;
extern lora_channel_config lora_channel[];
extern radio_pinout pinout[];

extern controlled_air_radio air_radios_air_none[MAX_NUMBER_OF_RADIOS];
extern controlled_air_radio air_radios_air_one [MAX_NUMBER_OF_RADIOS];
extern controlled_air_radio air_radios_air_two [MAX_NUMBER_OF_RADIOS];
extern provided_air_radio   air_radios_provided[MAX_NUMBER_OF_RADIOS];
extern SX1262* sx1262_radios[MAX_NUMBER_OF_RADIOS];
extern SX1268* sx1268_radios[MAX_NUMBER_OF_RADIOS];
extern lora_message lora_queue_rx[MAX_NUMBER_OF_CHANNELS];
extern lora_message lora_queue_tx[MAX_NUMBER_OF_CHANNELS];

String serial_input, bt_input, line_from_file;
char line[SERIALINPUTLEN], line_uppercase[SERIALINPUTLEN], p1[SERIALINPUTLEN], p1u[SERIALINPUTLEN];
char serial_info[LONGINFOLEN];
char serial_p[SERIALINPUTLEN];

extern bool air_serial_initialized[AIR_NUM_SERIAL+1];

extern nsa_global nsa;

#if defined(USE_NRF52)
extern SPIClass lora_spi;
#endif

void serial_allocate_string_memory()
{
  serial_input.reserve(SERIALINPUTLEN);
  bt_input.reserve(SERIALINPUTLEN);
  line_from_file.reserve(SERIALINPUTLEN);
  return;
}

void serial_setup(void)
{
  Serial.begin(SERIAL_BAUDRATE);
  Serial.setTimeout(SERIAL_TIMEOUT);
  serial_allocate_string_memory();
  return;
}

void serial_bluetooth_enable(void)
{
#if defined(DEVICE_HAS_BLUETOOTH)
  SerialBT.begin(cfg.bt_device_name);
  SerialBT.setTimeout(10);
  cfg.bt_enabled = true;
  serial_writeln("INFO: Enabling BT access");
#elif defined(DEVICE_HAS_BLUETOOTH_LE)  
  char btdevicename[LEN32];
  snprintf(btdevicename, LEN32, "%s", cfg.bt_device_name);
  SerialBLE.begin(btdevicename);
  SerialBLE.setTimeout(10);
#else 
  serial_writeln("WARNING: Cannot enable BT access due to hardware restrictions");
#endif
  return;
}

void serial_bluetooth_disable(void)
{
  serial_writeln("INFO: Disabling BT access");
#if defined(DEVICE_HAS_BLUETOOTH)
  SerialBT.end();
  cfg.bt_enabled = false;
#elif defined(DEVICE_HAS_BLUETOOTH_LE)
  SerialBLE.end();
  cfg.bt_enabled = false;
#endif
  return;
}

void serial_write(const char *s, bool use_prefix)
{
  if (use_prefix == true)
  {
    Serial.print(cfg.serial_prefix);
    Serial.print(s);
#if defined(DEVICE_HAS_BLUETOOTH)
    if (cfg.bt_enabled)
    {
        SerialBT.print(cfg.serial_prefix);
        SerialBT.print(s);
    }
#elif defined(DEVICE_HAS_BLUETOOTH_LE)
    if (cfg.bt_enabled)
    {
        SerialBLE.print(cfg.serial_prefix);
        SerialBLE.print(s);
    }
#endif
  }
  else
  {
    Serial.print(s);
#if defined(DEVICE_HAS_BLUETOOTH)
    if (cfg.bt_enabled) SerialBT.print(s);
#elif defined(DEVICE_HAS_BLUETOOTH_LE)
    if (cfg.bt_enabled) SerialBLE.print(s);
#endif
  }
  Serial.flush();
#ifdef DEVICE_HAS_BLUETOOTH
  if (cfg.bt_enabled) SerialBT.flush();
#endif
  return;
}

void serial_writeln(const char *s, bool use_prefix)
{
  serial_write(s, use_prefix);
  serial_write("\n", false); // do not use prefix for newline symbol
  return;
}

void serial_write_base64(const char *data, uint8_t len, bool add_newline)
{
  int encodedLength = Base64ren.encodedLength(len);
  char encodedString[encodedLength + 1];
  Base64ren.encode(encodedString, (char *) data, len);
  if (add_newline == true)
  {
    serial_writeln(encodedString, false);
  }
  else
  {
    serial_write(encodedString, false);
  }
  return;
}

void serial_readln(char *dest, int maxlen)
{
  dest[FIRST_BYTE_IN_ARRAY] = ZEROBYTE;

#if defined(DEVICE_HAS_BLUETOOTH)
  if (cfg.bt_enabled)
  {
    if (SerialBT.available()) 
    {
      bt_input = SerialBT.readStringUntil('\n');
      bt_input.toCharArray(dest, maxlen);
      return;
    }
  }
#elif defined(DEVICE_HAS_BLUETOOTH_LE)
  if (cfg.bt_enabled)
  {
    if (SerialBLE.available()) 
    {
      bt_input = SerialBLE.readStringUntil('\n');
      bt_input.toCharArray(dest, maxlen);
      return;
    }
  }
#endif

  if (!Serial.available())
  {
    return;
  }

  if (cfg.serial0_legacy == false)
  {
    serial_input = Serial.readStringUntil('\n');
  }
  else 
  { // allows to distinguish between no input and empty input, but may read multiple lines at once
    serial_input = Serial.readString();
  }
  serial_input.toCharArray(dest, maxlen);

  return;
}

void serial_banner(void)
{
  char buf[INFOLEN];

  snprintf(buf, INFOLEN, "INFO: RDCP-Modem Firmware, built %s %s", __DATE__, __TIME__);
  serial_writeln(buf);

  snprintf(buf, INFOLEN, "INFO: Hardware device: %s, number of radios: %d", USED_HARDWARE_DEVICE, (int) cfg.number_of_radios);
  serial_writeln(buf);

  for (int i=0; i<cfg.number_of_radios; i++)
  {
    snprintf(buf, INFOLEN, "INFO: Pinout for radio id %d: id in type group %d is %d, SPI %d, MISO %d, MOSI %d, CLK %d, CS/NSS %d, DIO1 %d, BUSY %d, RESET %d, TXEN %d, RXEN %d, def ch %d",
        (int) i, (int) pinout[i].radio_type, (int) pinout[i].radio_id_in_group, (int) pinout[i].num_spi,
        (int) pinout[i].spi_miso, (int) pinout[i].spi_mosi, (int) pinout[i].spi_clock, (int) pinout[i].spi_cs, 
        (int) pinout[i].lora_dio1, (int) pinout[i].lora_busy, (int) pinout[i].lora_reset, (int) pinout[i].lora_txen, 
        (int) pinout[i].lora_rxen, (int) pinout[i].default_channel);
    serial_writeln(buf);
  }

  for (int i=0; i<cfg.number_of_channels; i++)
  {
    snprintf(buf, INFOLEN, "INFO: Settings for LoRa channel %d: FREQ %.3f MHz, BW %3.0f kHz, SF %d, CR %d, SW %02X, PW %d, PL %d, CFEst %d %s",
        (int) i, lora_channel[i].frequency_in_mhz, lora_channel[i].bandwidth_in_khz, (int) lora_channel[i].spreading_factor, 
        (int) lora_channel[i].coding_rate, (int) lora_channel[i].syncword, (int) lora_channel[i].tx_power, (int) lora_channel[i].preamble_length, 
        (int) lora_channel[i].cfest_mode, lora_channel[i].send_enabled ? "(send enabled)" : "(send disabled)");
    serial_writeln(buf);
  }

  snprintf(buf, INFOLEN, "INFO: Relays in scenario = %d, Bluetooth %s (%s)",
    (int) cfg.scenario_num_relays, cfg.bt_enabled ? "enabled" : "disabled", cfg.bt_device_name);
  serial_writeln(buf);

  snprintf(buf, INFOLEN, "INFO: num ch = %d, init on start = %s, initialized = %s, rxl %s, rxmetal %s, rdcpcsvl %s",
    (int) cfg.number_of_channels, cfg.init_radios_on_start ? "yes" : "no", cfg.radios_initialized ? "yes" : "no", 
    cfg.print_rx_lines ? "+" : "-", cfg.print_rxmeta_lines ? "+" : "-", cfg.print_rdcpcsv_lines ? "+" : "-");
  serial_writeln(buf);

  return;
}

void serial_show_status_hqmode(void)
{ // adjusted from the ROLODECK implementation, rough approximation for MERLIN HQ devices
  char status[INFOLEN];
  int64_t now = my_millis();
  int64_t cfdelta = rdcpv04_get_channel_free_estimation(cfg.default_channel) - now;
  int txqe = scheduler_get_num_txq_entries();
  int txaqe = COUNT_ZERO;
  uint16_t roam = RDCP_ADDRESS_ZERO;
  char timestamp[LEN32];
  snprintf(timestamp, LEN32, "n/a");
  int cirestate = COUNT_ZERO;
  int reported_cpu_freq = 240;

  scheduler_dump_txqueue();

  snprintf(status, INFOLEN, "STATUS: T %" PRId64 " ms, rCF %" PRId64 " ms, Q %d/%d, R %04X, CPU %d MHz, clk %s, CIREstate %d", 
    now, cfdelta, txqe, txaqe, (int) roam, reported_cpu_freq, timestamp, cirestate);
  serial_writeln(status);

  return;
}

void serial_show_status(void)
{
  if (cfg.hq_mode == OPTION_ENABLED)
  {
    serial_writeln("INFO: Limited emulation of HQ mode output");
    serial_show_status_hqmode();
    return;
  }

  char status[INFOLEN];
  int64_t now = my_millis();
#if defined(ESP32)
  int32_t free_heap = ESP.getFreeHeap();
  int32_t min_free_heap = ESP.getMinFreeHeap();
#else 
#endif

  scheduler_dump_txqueue();

  int32_t uptime_sec = (int32_t) (now/SECONDS_TO_MILLISECONDS);
  int days = uptime_sec / (HOURS_PER_DAY * SECONDS_PER_HOUR);
  int seconds_within_today = (uptime_sec - days * HOURS_PER_DAY * SECONDS_PER_HOUR);
  int hours = seconds_within_today / SECONDS_PER_HOUR; 
  int seconds_within_this_hour = (seconds_within_today - hours * SECONDS_PER_HOUR);
  int minutes = seconds_within_this_hour / MINUTES_PER_HOUR;
  int seconds = seconds_within_this_hour % MINUTES_PER_HOUR;

#if defined(ESP32)
  snprintf(status, INFOLEN, "STATUS: Uptime %" PRId64 " ms (%d days %02d hours %02d minutes %02d seconds), Heap %d/%d", 
    now, days, hours, minutes, seconds, min_free_heap, free_heap);
#elif defined(USE_NRF52)
  snprintf(status, INFOLEN, "STATUS: Uptime %d ms (%d days %02d hours %02d minutes %02d seconds), Heap %u", 
    (int) now, days, hours, minutes, seconds, nrf52_getFreeHeap());
#endif
  serial_writeln(status);
  serial_writeln("READY");

  return;
}

void serial_process_command(const char* s, const char* processing_mode)
{
  bool suppress_echo = false;
  char basic_serial_command[SERIALINPUTLEN];

  strncpy(line, s, SERIALINPUTLEN);

  if (nsa_startsWith(line, "+"))
  {
    nsa_substring(line, 1);
    strncpy(line, nsa.result, SERIALINPUTLEN);
    nsa_strtrim(line);
    strncpy(line, nsa.result, SERIALINPUTLEN);
    persistence_add_line_to_initscript(line);
  }
  if (nsa_startsWith(line, "!"))
  {
    suppress_echo = true;
    nsa_substring(line, 1);
    strncpy(line, nsa.result, SERIALINPUTLEN);
  }
  strncpy(line_uppercase, line, SERIALINPUTLEN);
  for (int x=COUNT_ZERO; line_uppercase[x] != '\0'; x++) line_uppercase[x] = toUpperCase(line_uppercase[x]);
  strncpy(basic_serial_command, line, SERIALINPUTLEN);

  if (nsa_strlen(line) <= 2) // "empty" input requests status only
  {
    serial_show_status();
    return;
  }

  if (!suppress_echo)
  {
    snprintf(serial_info, LONGINFOLEN, "%s%s", processing_mode, s);
    serial_writeln(serial_info);
  }

  if (nsa_startsWith(line_uppercase, "DELAY "))
  {
    // DELAY 12345
    // 01234567890
    nsa_substring(line, 6);
    int delay_time = strtol(nsa.result, NULL, BASE10);
    snprintf(serial_info, INFOLEN, "INFO: Delaying for %d ms", delay_time);
    serial_writeln(serial_info);
    delay(delay_time);
  }
  else if (nsa_startsWith(line_uppercase, "RADIOINIT"))
  {
    serial_writeln("INFO: Setting up radios");
    lora_hardware_setup();
    radio_setup();
    cfg.radios_initialized = true;
  }
  else if (nsa_startsWith(line_uppercase, "SET RADIO "))
  {
    // SET RADIO x
    // 01234567890
    nsa_substring(line_uppercase, 10);
    strncpy(p1, nsa.result, SERIALINPUTLEN);

    if (nsa_startsWith(p1, "NUM "))
    {
      // SET RADIO NUM %d
      //           01234
      nsa_substring(p1, 4);
      cfg.number_of_radios = strtol(nsa.result, NULL, BASE10);
      snprintf(serial_info, INFOLEN, "INFO: Number of radios has been set to %d", cfg.number_of_radios);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(p1, "AIR "))
    {
      // SET RADIO AIR 1 1 0 0 0 1     
      //           012345678901234
      // (Local radio id, Serial port, id on this serial port, number of hops, remote radio id, default channel)
      nsa_strsplice(p1);
      uint8_t local_radio_id    = strtol(nsa.part[1], NULL, BASE10);
      uint8_t serial_port       = strtol(nsa.part[2], NULL, BASE10);
      uint8_t radio_id_in_group = strtol(nsa.part[3], NULL, BASE10);
      uint8_t number_of_hops    = strtol(nsa.part[4], NULL, BASE10);
      uint8_t remote_radio_id   = strtol(nsa.part[5], NULL, BASE10);
      uint8_t default_channel   = strtol(nsa.part[6], NULL, BASE10);

      snprintf(serial_info, INFOLEN, "INFO: Setting up new AIR radio as radio %d, on Serial%d, id in group is %d, %d hops, remote radio id is %d, default channel %d",
        (int) local_radio_id, serial_port, (int) radio_id_in_group, (int) number_of_hops, (int) remote_radio_id, (int) default_channel);
      serial_writeln(serial_info);

      pinout[local_radio_id].radio_type        = RADIO_TYPE_AIR;
      pinout[local_radio_id].radio_id_in_group = radio_id_in_group;
      pinout[local_radio_id].num_spi           = SPI_AIR_NONE + serial_port;
      pinout[local_radio_id].default_channel   = default_channel;
      pinout[local_radio_id].spi_miso          = PIN_NOT_USED;
      pinout[local_radio_id].spi_mosi          = PIN_NOT_USED;
      pinout[local_radio_id].spi_clock         = PIN_NOT_USED;
      pinout[local_radio_id].spi_cs            = PIN_NOT_USED;
      pinout[local_radio_id].lora_busy         = PIN_NOT_USED;
      pinout[local_radio_id].lora_dio1         = PIN_NOT_USED;
      pinout[local_radio_id].lora_reset        = PIN_NOT_USED;
      pinout[local_radio_id].lora_rxen         = PIN_NOT_USED;
      pinout[local_radio_id].lora_txen         = PIN_NOT_USED;

      if (serial_port == AIRMODEM_SERIAL0)
      {
        air_radios_air_none[radio_id_in_group].remote_radio         = remote_radio_id;
        air_radios_air_none[radio_id_in_group].airhops              = number_of_hops;
        air_radios_air_none[radio_id_in_group].rx_buf.available     = NO_MESSAGE_AVAILABLE;
        air_radios_air_none[radio_id_in_group].cad_result_available = NO_CAD_RESULT_AVAILABLE;
      }
      else if (serial_port == AIRMODEM_SERIAL1)
      {
        air_radios_air_one[radio_id_in_group].remote_radio         = remote_radio_id;
        air_radios_air_one[radio_id_in_group].airhops              = number_of_hops;
        air_radios_air_one[radio_id_in_group].rx_buf.available     = NO_MESSAGE_AVAILABLE;
        air_radios_air_one[radio_id_in_group].cad_result_available = NO_CAD_RESULT_AVAILABLE;
      }
      else if (serial_port == AIRMODEM_SERIAL2)
      {
        air_radios_air_two[radio_id_in_group].remote_radio         = remote_radio_id;
        air_radios_air_two[radio_id_in_group].airhops              = number_of_hops;
        air_radios_air_two[radio_id_in_group].rx_buf.available     = NO_MESSAGE_AVAILABLE;
        air_radios_air_two[radio_id_in_group].cad_result_available = NO_CAD_RESULT_AVAILABLE;
      }
    }
    else if (nsa_startsWith(p1, "SX1262 "))
    {
      // SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 00     
      //           012345678901234567890123456789012345678901
      //           0         1         2         3         4
      // (Local radio id, id in SX1262 group, SPI, MISO, MOSI, CLK, CS, DIO1, BUSY, RESET, TXEN, RXEN, default channel)
      nsa_strsplice(p1);
      uint8_t local_radio_id    = strtol(nsa.part[1], NULL, BASE10); 
      uint8_t radio_id_in_group = strtol(nsa.part[2], NULL, BASE10); 
      int spi_interface         = strtol(nsa.part[3], NULL, BASE10); 
      int pin_miso              = strtol(nsa.part[4], NULL, BASE10); 
      int pin_mosi              = strtol(nsa.part[5], NULL, BASE10); 
      int pin_clk               = strtol(nsa.part[6], NULL, BASE10); 
      int pin_cs                = strtol(nsa.part[7], NULL, BASE10); 
      int pin_dio1              = strtol(nsa.part[8], NULL, BASE10); 
      int pin_busy              = strtol(nsa.part[9], NULL, BASE10); 
      int pin_reset             = strtol(nsa.part[10], NULL, BASE10); 
      int pin_txen              = strtol(nsa.part[11], NULL, BASE10); 
      int pin_rxen              = strtol(nsa.part[12], NULL, BASE10); 
      uint8_t default_channel   = strtol(nsa.part[13], NULL, BASE10); 

      snprintf(serial_info, INFOLEN, "INFO: Setting up new SX1262 radio as radio %d, on SPI%d, id in group is %d, MISO %d, MOSI %d, CLK %d, CS %d, DIO1 %d, BUSY %d, RESET %d, TXEN %d, RXEN %d, default channel %d",
        (int) local_radio_id, (int) spi_interface, (int) radio_id_in_group, pin_miso, pin_mosi, pin_clk, pin_cs, 
        pin_dio1, pin_busy, pin_reset, pin_txen, pin_rxen, (int) default_channel);
      serial_writeln(serial_info);

      pinout[local_radio_id].radio_type        = RADIO_TYPE_SX1262;
      pinout[local_radio_id].radio_id_in_group = radio_id_in_group;
      pinout[local_radio_id].num_spi           = spi_interface;
      pinout[local_radio_id].spi_miso          = pin_miso;
      pinout[local_radio_id].spi_mosi          = pin_mosi;
      pinout[local_radio_id].spi_clock         = pin_clk;
      pinout[local_radio_id].spi_cs            = pin_cs;
      pinout[local_radio_id].lora_busy         = pin_busy;
      pinout[local_radio_id].lora_dio1         = pin_dio1;
      pinout[local_radio_id].lora_reset        = pin_reset;
      pinout[local_radio_id].lora_rxen         = pin_rxen;
      pinout[local_radio_id].lora_txen         = pin_txen;
      pinout[local_radio_id].default_channel   = default_channel;

      sx1262_radios[radio_id_in_group] = new SX1262(
                                         new Module(pinout[local_radio_id].spi_cs, 
                                                    pinout[local_radio_id].lora_dio1, 
                                                    pinout[local_radio_id].lora_reset, 
                                                    pinout[local_radio_id].lora_busy
#if defined (USE_NRF52)                                                    
                                                   , lora_spi
#endif
                                                  ));
    }
    else if (nsa_startsWith(p1, "SX1268 "))
    {
      // SET RADIO SX1268 0 0 1 11 10 09 08 14 13 12 -1 -1 00     
      //           012345678901234567890123456789012345678901
      //           0         1         2         3         4
      // (Local radio id, id in SX1262 group, SPI, MISO, MOSI, CLK, CS, DIO1, BUSY, RESET, TXEN, RXEN, default channel)
      nsa_strsplice(p1);
      uint8_t local_radio_id    = strtol(nsa.part[1], NULL, BASE10); 
      uint8_t radio_id_in_group = strtol(nsa.part[2], NULL, BASE10); 
      int spi_interface         = strtol(nsa.part[3], NULL, BASE10); 
      int pin_miso              = strtol(nsa.part[4], NULL, BASE10); 
      int pin_mosi              = strtol(nsa.part[5], NULL, BASE10); 
      int pin_clk               = strtol(nsa.part[6], NULL, BASE10); 
      int pin_cs                = strtol(nsa.part[7], NULL, BASE10); 
      int pin_dio1              = strtol(nsa.part[8], NULL, BASE10); 
      int pin_busy              = strtol(nsa.part[9], NULL, BASE10); 
      int pin_reset             = strtol(nsa.part[10], NULL, BASE10); 
      int pin_txen              = strtol(nsa.part[11], NULL, BASE10); 
      int pin_rxen              = strtol(nsa.part[12], NULL, BASE10); 
      uint8_t default_channel   = strtol(nsa.part[13], NULL, BASE10); 

      snprintf(serial_info, INFOLEN, "INFO: Setting up new SX1268 radio as radio %d, on SPI%d, id in group is %d, MISO %d, MOSI %d, CLK %d, CS %d, DIO1 %d, BUSY %d, RESET %d, TXEN %d, RXEN %d, default channel %d",
        (int) local_radio_id, (int) spi_interface, (int) radio_id_in_group, pin_miso, pin_mosi, pin_clk, pin_cs, 
        pin_dio1, pin_busy, pin_reset, pin_txen, pin_rxen, (int) default_channel);
      serial_writeln(serial_info);

      pinout[local_radio_id].radio_type        = RADIO_TYPE_SX1268;
      pinout[local_radio_id].radio_id_in_group = radio_id_in_group;
      pinout[local_radio_id].num_spi           = spi_interface;
      pinout[local_radio_id].spi_miso          = pin_miso;
      pinout[local_radio_id].spi_mosi          = pin_mosi;
      pinout[local_radio_id].spi_clock         = pin_clk;
      pinout[local_radio_id].spi_cs            = pin_cs;
      pinout[local_radio_id].lora_busy         = pin_busy;
      pinout[local_radio_id].lora_dio1         = pin_dio1;
      pinout[local_radio_id].lora_reset        = pin_reset;
      pinout[local_radio_id].lora_rxen         = pin_rxen;
      pinout[local_radio_id].lora_txen         = pin_txen;
      pinout[local_radio_id].default_channel   = default_channel;

      sx1268_radios[radio_id_in_group] = new SX1268(
                                         new Module(pinout[local_radio_id].spi_cs, 
                                                    pinout[local_radio_id].lora_dio1, 
                                                    pinout[local_radio_id].lora_reset, 
                                                    pinout[local_radio_id].lora_busy
#if defined (USE_NRF52)                                                    
                                                   , lora_spi
#endif
                                                  ));
    }
    else if (nsa_startsWith(p1, "INTERFACE "))
    {
      // SET RADIO INTERFACE 0 0 0 1 0 15 16 -1 -1
      //           012345678901234567890123456789012345678901
      //           0         1         2         3         4
      // (Reset SPI1, Reset SPI2, Prepare for AIR 0, Prepare for AIR 1, Prepare for AIR 2,
      //  Serial1 RX, Serial1 TX, Serial2 RX, Serial 2 TX)
      nsa_strsplice(p1);
      int i_rst_spi1  = strtol(nsa.part[1], NULL, BASE10);
      int i_rst_spi2  = strtol(nsa.part[2], NULL, BASE10);
      int i_prep_air0 = strtol(nsa.part[3], NULL, BASE10);
      int i_prep_air1 = strtol(nsa.part[4], NULL, BASE10);
      int i_prep_air2 = strtol(nsa.part[5], NULL, BASE10);
      int i_s1_rx     = strtol(nsa.part[6], NULL, BASE10);
      int i_s1_tx     = strtol(nsa.part[7], NULL, BASE10);
      int i_s2_rx     = strtol(nsa.part[8], NULL, BASE10);
      int i_s2_tx     = strtol(nsa.part[9], NULL, BASE10);

      cfg.reset_spi_one = i_rst_spi1 ? true : false;
      cfg.reset_spi_two = i_rst_spi2 ? true : false;
      cfg.prepare_for_air[AIRMODEM_SERIAL0] = i_prep_air0 ? true : false;
      cfg.prepare_for_air[AIRMODEM_SERIAL1] = i_prep_air1 ? true : false;
      cfg.prepare_for_air[AIRMODEM_SERIAL2] = i_prep_air2 ? true : false;
      cfg.serial1_rx = i_s1_rx;
      cfg.serial1_tx = i_s1_tx;
      cfg.serial2_rx = i_s2_rx;
      cfg.serial2_tx = i_s2_tx;

      snprintf(serial_info, INFOLEN, "INFO: Radio hardware interface settings updated: Reset SPI: %c%c, prepare for AIR: %c%c%c, serial1/2 ports %d %d %d %d",
        cfg.reset_spi_one ? '+' : '-', 
        cfg.reset_spi_two ? '+' : '-', 
        cfg.prepare_for_air[AIRMODEM_SERIAL0] ? '+' : '-', 
        cfg.prepare_for_air[AIRMODEM_SERIAL1] ? '+' : '-', 
        cfg.prepare_for_air[AIRMODEM_SERIAL2] ? '+' : '-', 
        cfg.serial1_rx, cfg.serial1_tx, cfg.serial2_rx, cfg.serial2_tx);
      serial_writeln(serial_info);
    }
  }
  else if (nsa_startsWith(line_uppercase, "SET CHANNEL "))
  {
    // SET CHANNEL x
    // 01234567890123
    nsa_substring(line_uppercase, 12);
    strncpy(p1, nsa.result, SERIALINPUTLEN);

    if (nsa_startsWith(p1, "NUM "))
    {
      // SET CHANNEL NUM %d
      //             01234
      nsa_substring(p1, 4);
      cfg.number_of_channels = strtol(nsa.result, NULL, BASE10);
      snprintf(serial_info, INFOLEN, "INFO: Number of channels has been set to %d", (int) cfg.number_of_channels);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(p1, "LORA "))
    {
      // SET RADIO LORA 00 868.200 125 07 5 12 00 15 1 2
      //           01234567890123456789012345678901234567890
      //           0         1         2         3         4
      // (Channel id, freq, bw, sf, cr, sw, txpwr, pl, send enabled, cfest mode)
      nsa_strsplice(p1);
      int channel_id = strtol(nsa.part[1], NULL, BASE10);

      lora_channel[channel_id].frequency_in_mhz = strtod(nsa.part[2], NULL);
      lora_channel[channel_id].bandwidth_in_khz = strtod(nsa.part[3], NULL);
      lora_channel[channel_id].spreading_factor = strtol(nsa.part[4], NULL, BASE10);
      lora_channel[channel_id].coding_rate      = strtol(nsa.part[5], NULL, BASE10);
      lora_channel[channel_id].syncword         = strtol(nsa.part[6], NULL, BASE16);
      lora_channel[channel_id].tx_power         = strtol(nsa.part[7], NULL, BASE10);
      lora_channel[channel_id].preamble_length  = strtol(nsa.part[8], NULL, BASE10);
      lora_channel[channel_id].send_enabled     = strtol(nsa.part[9], NULL, BASE10) ? true : false;
      lora_channel[channel_id].cfest_mode       = strtol(nsa.part[10], NULL, BASE10);

      snprintf(serial_info, INFOLEN, "INFO: Channel %d configuration: Frequency %.3f MHz, bandwidth %.3f kHz, SF %d, CR %d, SW %02X, TX Power %d dBm, PL %d, send %s, CFEst mode %d", 
        (int) channel_id, lora_channel[channel_id].frequency_in_mhz, lora_channel[channel_id].bandwidth_in_khz, 
        (int) lora_channel[channel_id].spreading_factor, (int) lora_channel[channel_id].coding_rate, 
        (int) lora_channel[channel_id].syncword, (int) lora_channel[channel_id].tx_power, 
        (int) lora_channel[channel_id].preamble_length, lora_channel[channel_id].send_enabled ? "enabled" : "disabled", 
        (int) lora_channel[channel_id].cfest_mode);
      serial_writeln(serial_info);
    }
  }
  else if (nsa_startsWith(line_uppercase, "PIN "))
  {
    // PIN cc pp
    // 01234567
    if (nsa_startsWith(line_uppercase, "PIN OH "))
    {
      nsa_substring(line_uppercase, 7);
      int pin = strtol(nsa.result, NULL, BASE10);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      snprintf(serial_info, INFOLEN, "INFO: Setting GPIO pin %d to output HIGH", pin);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(line_uppercase, "PIN OL "))
    {
      nsa_substring(line_uppercase, 7);
      int pin = strtol(nsa.result, NULL, BASE10);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
      snprintf(serial_info, INFOLEN, "INFO: Setting GPIO pin %d to output LOW", pin);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(line_uppercase, "PIN IU "))
    {
      nsa_substring(line_uppercase, 7);
      int pin = strtol(nsa.result, NULL, BASE10);
      pinMode(pin, INPUT_PULLUP);
      snprintf(serial_info, INFOLEN, "INFO: Setting GPIO pin %d to INPUT PULLUP", pin);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(line_uppercase, "PIN ID "))
    {
      nsa_substring(line_uppercase, 7);
      int pin = strtol(nsa.result, NULL, BASE10);
      pinMode(pin, INPUT_PULLDOWN);
      snprintf(serial_info, INFOLEN, "INFO: Setting GPIO pin %d to INPUT PULLDOWN", pin);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(line_uppercase, "PIN II "))
    {
      nsa_substring(line_uppercase, 7);
      int pin = strtol(nsa.result, NULL, BASE10);
      pinMode(pin, INPUT);
      snprintf(serial_info, INFOLEN, "INFO: Setting GPIO pin %d to pure INPUT", pin);
      serial_writeln(serial_info);
    }
  }
  else if (nsa_startsWith(line_uppercase, "SPI "))
  {
    if (nsa_startsWith(line_uppercase, "SPI END"))
    {
      serial_writeln("INFO: Executing SPI end");
#if defined(ESP32)
      SPI.end();
#elif defined(USE_NRF52)
      lora_spi.end();
#endif
    }
    else if (nsa_startsWith(line_uppercase, "SPI BEGIN "))
    {
      // SPI BEGIN 11 22 33
      // 012345678901234567
      nsa_strsplice(line_uppercase);
      int clock = strtol(nsa.part[2], NULL, BASE10);
      int miso = strtol(nsa.part[3], NULL, BASE10);
      int mosi = strtol(nsa.part[4], NULL, BASE10);
      snprintf(serial_info, INFOLEN, "INFO: Executing SPI.begin(%d, %d, %d)", clock, miso, mosi);
      serial_writeln(serial_info);
#if defined(ESP32)
      SPI.begin(clock, miso, mosi);
#elif defined(USE_NRF52)
      lora_spi = SPIClass(NRF_SPIM0, miso, clock, mosi);
      lora_spi.begin();
#endif
    }
    else if (nsa_startsWith(line_uppercase, "SPI JUSTBEGIN"))
    {
#if defined(ESP32)
      SPI.begin();
#elif defined(USE_NRF52)
      serial_writeln("INFO: nRF52 LORA SPI begin()");
      lora_spi.begin();
#endif
    }
  }
  else if (nsa_startsWith(line_uppercase, "DUMP "))
  {
    // DUMP p1
    // 012345
    nsa_substring(line_uppercase, 5);
    strncpy(p1, nsa.result, SERIALINPUTLEN);
    if (nsa_startsWith(p1, "OVERVIEW"))
    {
      serial_banner();
    }
    else if (nsa_startsWith(p1, "INITSCRIPT"))
    {
      persistence_dump_initscript();
    }
  }
  else if (nsa_startsWith(line_uppercase, "SET CONFIG "))
  {
    // SET CONFIG p1
    // 01234567890123
    nsa_substring(line, 11);
    strncpy(p1, nsa.result, SERIALINPUTLEN);
    strncpy(p1u, p1, SERIALINPUTLEN);
    for (int x=COUNT_ZERO; line_uppercase[x] != '\0'; x++) p1u[x] = toUpperCase(p1u[x]);

    if (nsa_startsWith(p1u, "BLUETOOTH "))
    { // BLUETOOTH 1 RDCP-Modem-0001
      // 0123456789012
      nsa_substring(p1, 10, 11);
      int do_bt_enable = strtol(nsa.result, NULL, BASE10);
      nsa_substring(p1, 12);

      if (cfg.bt_enabled) serial_bluetooth_disable();
      cfg.bt_enabled = do_bt_enable ? true : false;
      snprintf(cfg.bt_device_name, LEN32, "%s", nsa.result);
      if (cfg.bt_enabled) serial_bluetooth_enable();

      snprintf(serial_info, INFOLEN, "INFO: Bluetooth %s, device name: %s", 
        cfg.bt_enabled ? "enabled" : "disabled", cfg.bt_device_name);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(p1u, "PRINT "))
    {
      // PRINT 1 1 1 1 0 RDCP-Modem:
      // 0123456789012345678
      // (RX lines, RXMETA lines, RDCPCSV lines, AIRINFO lines, Prefix on AIR Serial0, Prefix String)
      nsa_strsplice(p1);
      cfg.print_rx_lines       = strtol(nsa.part[1], NULL, BASE10) ? true : false;
      cfg.print_rxmeta_lines   = strtol(nsa.part[2], NULL, BASE10) ? true : false;
      cfg.print_rdcpcsv_lines  = strtol(nsa.part[3], NULL, BASE10) ? true : false;
      cfg.print_airinfo_lines  = strtol(nsa.part[4], NULL, BASE10) ? true : false;
      cfg.prefix_on_air_serial = strtol(nsa.part[5], NULL, BASE10) ? true : false;
      snprintf(cfg.serial_prefix, LEN32, "%s ", nsa.part[6]);

      snprintf(serial_info, INFOLEN, "INFO: Serial print options set to RXL %c, RXMETAL %x, RDCPCSVL %c, AIRINFO %c, AIRPREFIX %c, prefix '%s'",
        cfg.print_rx_lines       ? '+' : '-',
        cfg.print_rxmeta_lines   ? '+' : '-',
        cfg.print_rdcpcsv_lines  ? '+' : '-',
        cfg.print_airinfo_lines  ? '+' : '-',
        cfg.prefix_on_air_serial ? '+' : '-',
        cfg.serial_prefix
      );
      serial_writeln(serial_info);
    }
  }
  else if (nsa_startsWith(line_uppercase, "SET RDCP "))
  {
    // SET RDCP p1
    // 01234567890
    nsa_substring(line_uppercase, 9);
    strncpy(p1, nsa.result, SERIALINPUTLEN);

    if (nsa_startsWith(p1, "LEGACY "))
    {
      // LEGACY 0 00 1 1 
      // 012345678901234567890
      // (default radio, default channel, Serial0 legacy mode, HQ mode)
      nsa_strsplice(p1);
      cfg.default_radio   = strtol(nsa.part[1], NULL, BASE10);
      cfg.default_channel = strtol(nsa.part[2], NULL, BASE10);
      cfg.serial0_legacy  = strtol(nsa.part[3], NULL, BASE10) ? true : false; 
      cfg.hq_mode         = strtol(nsa.part[4], NULL, BASE10) ? true : false;

      snprintf(serial_info, INFOLEN, "INFO: RDCP legacy configuration set to default radio %d, default channel %d, Serial0 legacy %c, HQ mode %c",
        (int) cfg.default_radio, (int) cfg.default_channel, cfg.serial0_legacy ? '+' : '-', cfg.hq_mode ? '+' : '-');
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(p1, "NUMRELAYS "))
    {
      // NUMRELAYS 10
      // 01234567890 
      nsa_substring(p1, 10);
      cfg.scenario_num_relays = strtol(nsa.result, NULL, BASE10);
      snprintf(serial_info, INFOLEN, "INFO: Number of relays in RDCP scenario set to %d", (int) cfg.scenario_num_relays);
      serial_writeln(serial_info);
    }
    else if (nsa_startsWith(p1, "ADDR "))
    {
      nsa_substring(p1, 5);
      uint16_t new_rdcp_address = strtol(nsa.result, NULL, BASE16);
      cfg.rdcp_address = new_rdcp_address;
      snprintf(serial_info, INFOLEN, "INFO: Changed this device's RDCP address to %04X", (unsigned int) cfg.rdcp_address);
      serial_writeln(serial_info);
    }
  }
  else if (nsa_startsWith(line_uppercase, "SERIAL "))
  {
    // SERIAL p1
    // 01234567
    nsa_substring(line, 7);
    serial_writeln(nsa.result, DONT_USE_PREFIX);
  }
  else if (nsa_startsWith(line_uppercase, "SERIALP "))
  {
    // SERIALP p1
    // 012345678
    nsa_substring(line, 8);
    serial_writeln(nsa.result, USE_PREFIX);
  }
  else if (nsa_startsWith(line_uppercase, "SERIAL1 "))
  {
    // SERIAL1 p1
    // 012345678
    nsa_substring(line, 8);

    if (air_serial_initialized[AIRMODEM_SERIAL1]) 
    {
      char s_airmsg[AIRMSGLEN];
      snprintf(s_airmsg, AIRMSGLEN, "%s\n", nsa.result);
#if defined(ESP32) || defined(USE_NRF52)
      Serial1.write(s_airmsg);
#endif
      serial_writeln("INFO: Message forwarded via Serial1");
    }
    else 
    {
      serial_writeln("WARNING: Serial1 not initialized for use");
    }
  }
  else if (nsa_startsWith(line_uppercase, "SERIAL2 "))
  {
    // SERIAL2 p1
    // 012345678
    nsa_substring(line, 8);

    if (air_serial_initialized[AIRMODEM_SERIAL2]) 
    {
      char s_airmsg[AIRMSGLEN];
      snprintf(s_airmsg, AIRMSGLEN, "%s\n", nsa.result);
#if defined(ESP32)
      Serial2.write(s_airmsg);
#endif
      serial_writeln("INFO: Message forwarded via Serial2");
    }
    else 
    {
      serial_writeln("WARNING: Serial2 not initialized for use");
    }
  }
  else if (nsa_startsWith(line_uppercase, "INITSCRIPT "))
  {
    // INITSCRIPT p1
    // 01234567890123
    nsa_substring(line_uppercase, 11);
    strncpy(p1, nsa.result, SERIALINPUTLEN);

    if (nsa_startsWith(p1, "DELETE"))
    {
      snprintf(serial_info, INFOLEN, "INFO: Deleting initscript %s", persistence_get_filename_initscript());
      serial_writeln(serial_info);
      persistence_remove_initscript();
    }
    else if (nsa_startsWith(p1, "NAME "))
    { // NAME name
      // 012345
      nsa_substring(p1, 5);
      char new_filename[FILENAMELEN];
      snprintf(new_filename, FILENAMELEN, "/config.%s", nsa.result);
      snprintf(serial_info, INFOLEN, "INFO: Setting initscript filename to %s", new_filename);
      serial_writeln(serial_info);
      persistence_set_filename_initscript(new_filename);
    }
  }
  else if (nsa_startsWith(line_uppercase, "AIR "))
  {
    air_loop(HAS_SERIAL0_INPUT, line);
  }
  else if (nsa_startsWith(line_uppercase, "SWITCH "))
  {
    // SWITCH 0 04
    // 0123456789
    nsa_strsplice(line);
    uint8_t radio_id   = strtol(nsa.part[1], NULL, BASE10);
    uint8_t channel_id = strtol(nsa.part[2], NULL, BASE10);
    radio_switch_to_channel(radio_id, channel_id, FORCED_CHANNEL_SWITCH);
  }
  else if (nsa_startsWith(line_uppercase, "SIMRX "))
  {
    // SIMRX 04 base64content
    // 01234567890
    nsa_substring(line, 6, 8);
    uint8_t channel_id = strtol(nsa.result, NULL, BASE10);

    if (channel_id >= cfg.number_of_channels)
    {
      serial_writeln("ERROR: Invalid SIMRX channel number");
      return;
    }

    nsa_substring(line, 9);
    strncpy(serial_p, nsa.result, SERIALINPUTLEN);
    int b64msg_len = strlen(serial_p);
    int decoded_length = Base64ren.decodedLength(serial_p, b64msg_len);
    char decoded_string[decoded_length + 1];
    Base64ren.decode(decoded_string, serial_p, b64msg_len);

    if (decoded_length > MAX_LORA_PAYLOAD_SIZE) decoded_length = MAX_LORA_PAYLOAD_SIZE;

    if (decoded_length > 0)
    {
      lora_queue_rx[channel_id].available      = MESSAGE_AVAILABLE;
      lora_queue_rx[channel_id].payload_length = decoded_length;
      lora_queue_rx[channel_id].rssi           = NO_RSSI;
      lora_queue_rx[channel_id].snr            = NO_SNR;
      lora_queue_rx[channel_id].radio          = RADIO_SIM;
      lora_queue_rx[channel_id].channel        = channel_id;
      lora_queue_rx[channel_id].timestamp      = my_millis();;

      for (int i=0; i != decoded_length; i++) lora_queue_rx[channel_id].payload[i] = decoded_string[i];

      snprintf(serial_info, INFOLEN, "INFO: LoRa Radio SIM received packet on channel %d, length %d bytes", 
        (int) channel_id, decoded_length);
      serial_writeln(serial_info);

      if (cfg.print_rxmeta_lines)
      {
        snprintf(serial_info, INFOLEN, "RXMETA %d %.2f %.2f %.3f", 
          (int) lora_queue_rx[channel_id].payload_length, lora_queue_rx[channel_id].rssi, 
          lora_queue_rx[channel_id].snr, lora_channel[channel_id].frequency_in_mhz);
        serial_writeln(serial_info);
      }
      if (cfg.print_rx_lines) serial_write_incoming_message(channel_id);
    }
    else
    {
      serial_writeln("WARNING: SIMRX empty packet - ignored.");
    }
  }
  else if (nsa_startsWith(line_uppercase, "TX"))
  {
    uint8_t tx_channel           = cfg.default_channel;
    uint8_t tx_payload_type      = PAYLOAD_TYPE_RDCP_V04;
    uint8_t tx_scheduling_mode   = SCHEDULING_MODE_CHANNEL_FREE;
    uint8_t tx_callback_selector = TX_CALLBACK_NONE;
    int64_t tx_forced_time       = ZERO_TIMESTAMP;

    if (nsa_startsWith(line_uppercase, "TX "))
    {
      // TX base64content
      // 0123
      nsa_substring(line, 3);
      strncpy(serial_p, nsa.result, SERIALINPUTLEN);
      nsa_strtrim(serial_p);
      strncpy(serial_p, nsa.result, SERIALINPUTLEN);
    }
    else if (nsa_startsWith(line_uppercase, "TXSCHED "))
    {
      // TXSCHED 12345 base64content
      // 012345678
      nsa_strsplice(line);
      tx_forced_time = strtol(nsa.part[1], NULL, BASE10);
      nsa_strtrim(nsa.part[2]);
      strncpy(serial_p, nsa.result, SERIALINPUTLEN);
      tx_scheduling_mode = SCHEDULING_MODE_FIXED_TIME;
    }
    else if (nsa_startsWith(line_uppercase, "TXCF "))
    {
      // TXCF 04 1 base64content
      // 01234567890
      nsa_strsplice(line);
      tx_channel = strtol(nsa.part[1], NULL, BASE10);
      tx_payload_type = strtol(nsa.part[2], NULL, BASE10);
      nsa_strtrim(nsa.part[3]);
      strncpy(serial_p, nsa.result, SERIALINPUTLEN);
    }
    else if (nsa_startsWith(line_uppercase, "TXFT "))
    {
      // TXFT 04 1 12345 base64content
      // 01234567890
      nsa_strsplice(line);
      tx_channel      = strtol(nsa.part[1], NULL, BASE10);
      tx_payload_type = strtol(nsa.part[2], NULL, BASE10);
      tx_forced_time  = strtol(nsa.part[3], NULL, BASE10);
      nsa_strtrim(nsa.part[4]);
      strncpy(serial_p, nsa.result, SERIALINPUTLEN);
      tx_scheduling_mode = SCHEDULING_MODE_FIXED_TIME;
    }

    int b64msg_len = strlen(serial_p);
    int decoded_length = Base64ren.decodedLength(serial_p, b64msg_len);
    char decoded_string[decoded_length + 1];
    Base64ren.decode(decoded_string, serial_p, b64msg_len);
    if (decoded_length > MAX_LORA_PAYLOAD_SIZE) decoded_length = MAX_LORA_PAYLOAD_SIZE;

    if (decoded_length == COUNT_ZERO)
    {
      serial_writeln("ERROR: Illegal instruction");
    }
    else if ((tx_payload_type == PAYLOAD_TYPE_RDCP_V04) && (decoded_length < 16))
    {
      serial_writeln("ERROR: The greatest teacher, failure is -- Yoda");
    }
    else 
    {
#if defined(ESP32)
      snprintf(serial_info, INFOLEN, "INFO: Adding to TXQ: ch%02d pt%d l%d, mode %s @ %" PRId64 ", base64: |%s|", 
        (int) tx_channel, 
        (int) tx_payload_type, 
        (int) decoded_length, 
        tx_scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE ? "cf" : "hard", 
        tx_forced_time,
        serial_p);
#elif defined(USE_NRF52)
      snprintf(serial_info, INFOLEN, "INFO: Adding to TXQ: ch%02d pt%d l%d, mode %s @ %d, base64: |%s|", 
        (int) tx_channel, 
        (int) tx_payload_type, 
        (int) decoded_length, 
        tx_scheduling_mode == SCHEDULING_MODE_CHANNEL_FREE ? "cf" : "hard", 
        (int) tx_forced_time,
        serial_p);
#endif
      serial_writeln(serial_info);

      scheduler_enqueue(tx_channel, tx_payload_type, (uint8_t*) decoded_string, decoded_length, tx_scheduling_mode, tx_callback_selector, tx_forced_time);
    }
  }
  else if (nsa_startsWith(line_uppercase, "CSVLOG "))
  {
    // CSVLOG cmd
    // 01234567
    nsa_substring(line_uppercase, 7);
    strncpy(p1, nsa.result, SERIALINPUTLEN);
    if (nsa_startsWith(p1, "ENABLE"))
    {
      rdcpv04_csvlogfile_set_status(OPTION_ENABLED);
    }
    else if (nsa_startsWith(p1, "DISABLE"))
    {
      rdcpv04_csvlogfile_set_status(OPTION_DISABLED);
    }
    else if (nsa_startsWith(p1, "DUMP"))
    {
      rdcpv04_csvlogfile_dump();
    }
    else if (nsa_startsWith(p1, "DELETE"))
    {
      rdcpv04_csvlogfile_delete();
    }
  }
  else if (nsa_startsWith(line_uppercase, "RESTART") || nsa_startsWith(line_uppercase, "REBOOT"))
  {
    hal_device_restart();
  }
  else 
  {
    bool handled = plugin_serial(basic_serial_command);
    if (!handled)
    {
      serial_writeln("WARNING: Unknown command");
    }
  }
  return;
}

/* EOF rdcp-modem-serial.cpp */