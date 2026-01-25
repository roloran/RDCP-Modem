/* rdcp-modem-persistence.h */

#ifndef _RDCP_MODEM_PERSISTENCE
#define _RDCP_MODEM_PERSISTENCE

#include <Arduino.h>
#include "rdcp-modem-constants.h"

/**
 * Initialize persistent storage and replay configuration from initscript.
 * To be called once after power-on.
 */
void persistence_setup(void);

/**
 * Set the filename of the initscript
 * @param fn Filename to use
 */
void persistence_set_filename_initscript(const char *fn=FILENAME_INITSCRIPT_DEFAULT);

/**
 * Get the filename of the currently used initscript
 * @return Filename of the initscript currently used
 */
char* persistence_get_filename_initscript(void);

/**
 * Store a Serial command to be automatically executed again on next device power-on when using the current initscript.
 * @param s String with a Serial command
 */
void persistence_add_line_to_initscript(const char *s);

/**
 * Reset/delete the current initscript
 */
void persistence_remove_initscript(void);

/**
 * Get the next RDCP Sequence Number to use for a specific RDCP address as Origin. 
 * @param origin RDCP Address of the device that needs the SequenceNumber as Origin 
 * @return uint16_t SequenceNumber as used in RDCP Header
 */
uint16_t persistence_get_next_rdcp_sequence_number(uint16_t origin);

/**
 * Set the next RDCP Sequence Number to use for a specific RDCP address. Used internally 
 * as well as for testing purposes. 
 * @param origin RDCP Address of Origin 
 * @param seq Next SequenceNumber to use for this Origin 
 * @return SequenceNumber that has been set
 */
uint16_t persistence_set_next_rdcp_sequence_number(uint16_t origin, uint16_t seq);

/**
 * Check whether the device has a persistence layer available and initialized
 * @return true if the device has persistence available and initialized, false otherwise
 */
bool persistence_has_persistence(void);

/**
 * Check for nonce validity on management RDCP messages. 
 * @param name Name of the nonce type 
 * @param nonce Received nonce 
 * @return true if nonce is valid, false otherwise
 */
bool persistence_checkset_nonce(char *name, uint16_t nonce);

/**
 * Print the currently used initscript on Serial0.
 */
void persistence_dump_initscript(void);

#endif

/* EOF rdcp-modem-persistence.h */