/* rdcp-modem-incoming.h */

/**
 * Check for freshly received LoRa packets and handle one of them.
 * @return true if a message was handled, false if no new message was available
 */
bool incoming_loop(void);

/* EOF rdcp-modem-incoming.h */