/* rdcp-modem-persistence.cpp */

#include <Arduino.h> 
#include "rdcp-modem-persistence.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-constants.h"

#if defined(ESP32)
#include <FS.h> 
#include <LittleFS.h>
#elif defined(USE_NRF52)
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#endif

bool has_persistence = false;
char filename_initscript[FILENAMELEN];
extern String line_from_file;
char initscript_line[SERIALINPUTLEN];

#if defined(USE_NRF52)
using namespace Adafruit_LittleFS_Namespace;
File f(InternalFS);
#endif

bool persistence_has_persistence(void)
{
  return has_persistence;
}

bool persistence_init_fs(void)
{
#if defined(ESP32)
  if (LittleFS.begin(true)) return true;
#elif defined(USE_NRF52)
  return InternalFS.begin();
#endif
  return false;
}

void persistence_set_filename_initscript(const char *fn)
{
  snprintf(filename_initscript, FILENAMELEN, "%s", fn);
#if defined(ESP32)
  File f = LittleFS.open(FILENAME_AUTOSTART, FILE_WRITE);
#elif defined(USE_NRF52)
  InternalFS.remove(FILENAME_AUTOSTART);
  f.open(FILENAME_AUTOSTART, FILE_O_WRITE);
#endif
  if (!f) 
  {
    serial_writeln("ERROR: Cannot FS-write autostart file");
    return;
  }
  f.printf("%s\n", fn);
  f.close();
  return;
}

char* persistence_get_filename_initscript(void)
{
#if defined(ESP32)
  File f = LittleFS.open(FILENAME_AUTOSTART, FILE_READ);
#elif defined(USE_NRF52)
  f.open(FILENAME_AUTOSTART, FILE_O_READ);
#endif
  if (!f) 
  {
    persistence_set_filename_initscript(FILENAME_INITSCRIPT_DEFAULT);
    return filename_initscript;
  }

  while (f.available())
  {
    line_from_file = f.readStringUntil('\n');
    line_from_file.toCharArray(filename_initscript, FILENAMELEN);
  }
  f.close();

  return filename_initscript;
}

void persistence_setup(void)
{
  has_persistence = persistence_init_fs();
  if (!has_persistence)
  {
    serial_writeln("ERROR: No persistance layer available (init FS failed)");
    return;
  }
#if defined(ESP32)
  File f = LittleFS.open(persistence_get_filename_initscript(), FILE_READ);
#elif defined(USE_NRF52)
  f.open(persistence_get_filename_initscript(), FILE_O_READ);
#endif
  if (!f)
  {
    serial_writeln("WARNING: No persisted configuration found");
    return;
  }

  while (f.available())
  {
    line_from_file = f.readStringUntil('\n');
    line_from_file.toCharArray(initscript_line, SERIALINPUTLEN);
    serial_process_command(initscript_line, "INITSCRIPT: ");
    delay(MINIMUM_DELAY);
  }

  f.close();

  return;
}

void persistence_dump_initscript(void)
{
  char dump_info[INFOLEN];

  if (!has_persistence)
  {
    serial_writeln("ERROR: No persistance layer available, cannot dump initscript");
    return;
  }

  snprintf(dump_info, INFOLEN, "DUMP: Configuration filename is %s", persistence_get_filename_initscript());
  serial_writeln(dump_info);

#if defined(ESP32)
  File f = LittleFS.open(persistence_get_filename_initscript(), FILE_READ);
#elif defined(USE_NRF52)
  f.open(persistence_get_filename_initscript(), FILE_O_READ);
#endif
  if (!f)
  {
    serial_writeln("WARNING: No persisted configuration found");
    return;
  }

  while (f.available())
  {
    line_from_file = f.readStringUntil('\n');
    line_from_file.toCharArray(initscript_line, SERIALINPUTLEN);
    snprintf(dump_info, INFOLEN, "DUMP: %s", initscript_line);
    serial_writeln(dump_info);
  }

  f.close();

  return;
}

void persistence_add_line_to_initscript(const char *s)
{
  if (!has_persistence)
  {
    serial_writeln("ERROR: No persistence layer, cannot add line to initscript");
    return;
  } 
#if defined(ESP32)
  File f = LittleFS.open(persistence_get_filename_initscript(), FILE_APPEND);
#elif defined(USE_NRF52)
  f.open(persistence_get_filename_initscript(), FILE_O_WRITE);
#endif
  if (!f) 
  {
    serial_writeln("ERROR: Cannot FS-append to initscript");
    return;
  }
  f.printf("%s\n", s);
  f.close();
  return;
}

void persistence_remove_initscript(void)
{
#if defined(ESP32)
  if (has_persistence) LittleFS.remove(persistence_get_filename_initscript());
#elif defined(USE_NRF52)
  if (has_persistence) InternalFS.remove(persistence_get_filename_initscript());
#endif
  return;
}

uint16_t persistence_get_next_rdcp_sequence_number(uint16_t origin)
{
  uint16_t seq = 1;
  if (!has_persistence)
  { 
    serial_writeln("WARNING: Missing persistence layer, using default sequence number of 1");
    return seq;
  }

  char fn[FILENAMELEN];
  snprintf(fn, FILENAMELEN, "%s%04X", FILENAME_PREFIX_SEQNR, (int) origin);
#if defined(ESP32)
  File f = LittleFS.open(fn, FILE_READ);
#elif defined(USE_NRF52)
  f.open(fn, FILE_O_READ);
#endif
  if (!f)
  {
    serial_writeln("WARNING: Missing sequence number file, starting with defaults");
    persistence_set_next_rdcp_sequence_number(origin, 2);
    return seq;
  }
  line_from_file = "0";
  if (f.available())
  {
    line_from_file = f.readString();
  }
  seq = line_from_file.toInt();
  f.close();

  if (seq == 0) serial_writeln("WARNING: Existing sequence number file yielded 0");
  persistence_set_next_rdcp_sequence_number(origin, seq+1);

  return seq;
}

uint16_t persistence_set_next_rdcp_sequence_number(uint16_t origin, uint16_t seq)
{
  if (!has_persistence)
  { 
    serial_writeln("WARNING: Missing persistence layer, cannot save next RDCP sequence number");
    return seq;
  }

  char p_info[INFOLEN];
  snprintf(p_info, INFOLEN, "INFO: Persisting next-up seqnr %u for %04X", seq, origin);
  serial_writeln(p_info);

  char fn[FILENAMELEN];
  snprintf(fn, FILENAMELEN, "%s%04X", FILENAME_PREFIX_SEQNR, origin);
#if defined(ESP32)
  LittleFS.remove(fn);
  File f = LittleFS.open(fn, FILE_WRITE);
#elif defined(USE_NRF52)
  InternalFS.remove(fn);
  f.open(fn, FILE_O_WRITE);
#endif
  if (!f)
  { 
    serial_writeln("WARNING: Saving sequence number file failed");
    return seq;
  }

  char content[INFOLEN];
  snprintf(content, INFOLEN, "%" PRIu16 "\n", seq);
  f.print(content);
  f.close();
  delay(1);

  return seq;
}

bool persistence_checkset_nonce(char *name, uint16_t nonce)
{
  if (!has_persistence) return false;

  bool is_valid = false;

  char filename[INFOLEN];
  snprintf(filename, INFOLEN, "%s%s", FILENAME_PREFIX_NONCE, name);
#if defined(ESP32)
  File f = LittleFS.open(filename, FILE_READ);
#elif defined(USE_NRF52)
  f.open(filename, FILE_O_READ);
#endif
  if (!f)
  {
    is_valid = true; // never seen a nonce for this type before
  }
  else
  {
    line_from_file = "0";
    if (f.available())
    {
      line_from_file = f.readString();
    }
    uint16_t old_nonce = line_from_file.toInt();
    f.close();

    if (old_nonce < nonce)
    {
      is_valid = true;
    }
    else 
    {
      char p_info[INFOLEN];
      snprintf(p_info, INFOLEN, "WARNING: Old nonce == %" PRIu16 ", new nonce == %" PRIu16, old_nonce, nonce);
      serial_writeln(p_info);
    }
  }

  if (is_valid)
  {
#if defined(ESP32)
    f = LittleFS.open(filename, FILE_WRITE);
#elif defined(USE_NRF52)
    InternalFS.remove(filename);
    f.open(filename, FILE_O_WRITE);
#endif
    if (!f) 
    { 
      is_valid = false; // cannot persist nonce, don't trust it
      serial_writeln("ERROR: Cannot persist nonce");
    }
    char content[INFOLEN];
    snprintf(content, INFOLEN, "%" PRIu16 "\n", nonce);
    f.print(content);
    f.close();
  }

  return is_valid;
}

/* EOF rdcp-modem-persistence.cpp */