#include <Arduino.h> 
#include "rdcp-modem-constants.h"
#include "rdcp-modem-timer.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-persistence.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-rdcp-v04.h"

int64_t reboot_requested = TIMESTAMP_ZERO;
bool    seqnr_reset_requested = false;
extern device_config cfg;

void timer_check(void)
{
  /* Delayed restart */
  if ((reboot_requested > 0) && (my_millis() > reboot_requested))
  {
    if (seqnr_reset_requested) persistence_set_next_rdcp_sequence_number(cfg.rdcp_address, 1); // Reset own sequence numbers
    delay(1 * SECONDS_TO_MILLISECONDS);
    hal_device_restart();
  }

  rdcpv04_check_heartbeat();
  rdcpv04_cmd_check_rtc();

  return;
}

/* EOF */