/* rdcp-modem-callback.cpp */

#include <Arduino.h>
#include "rdcp-modem-callback.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-plugin.h"

void callback_tx_finished(uint8_t radio, uint8_t channel, int cb_type)
{
  char info_msg[INFOLEN];

  snprintf(info_msg, INFOLEN, "INFO: Callback tx_finished type %d fired for radio %d, channel %d", 
    cb_type, (int) radio, (int) channel);
  serial_writeln(info_msg);

  /* Usually, we want to avoid processing AIR radio callbacks twice, so we handle only LOCAL callbacks */
  if (cb_type == CALLBACK_LOCAL)
  {
    plugin_callback(radio, channel, CALLBACK_TYPE_TXFIN, OPTION_DISABLED);
  }

  return;
}

void callback_tx_started(uint8_t radio, uint8_t channel, int cb_type)
{
  char info_msg[INFOLEN];

  snprintf(info_msg, INFOLEN, "INFO: Callback tx_started type %d fired for radio %d, channel %d", 
    cb_type, (int) radio, (int) channel);
  serial_writeln(info_msg);

  if (cb_type == CALLBACK_LOCAL)
  {
    plugin_callback(radio, channel, CALLBACK_TYPE_TXSTART, OPTION_DISABLED);
  }

  return;
}

void callback_cad_result(uint8_t radio, uint8_t channel, bool cad_result, int cb_type)
{
  char info_msg[INFOLEN];

  snprintf(info_msg, INFOLEN, "INFO: Callback cad_result type %d fired for radio %d, channel %d, state is %s", 
    cb_type, (int) radio, (int) channel, cad_result == CAD_CHANNEL_BUSY ? "busy" : "free");
  serial_writeln(info_msg);

  if (cb_type == CALLBACK_LOCAL)
  {
    plugin_callback(radio, channel, CALLBACK_TYPE_CADRES, cad_result);
  }

  return;
}

/* EOF rdcp-modem-callback.cpp */