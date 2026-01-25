/* rdcp-modem-serial.h */

#ifndef _RDCP_MODEM_SERIAL
#define _RDCP_MODEM_SERIAL

#include <Arduino.h> 

/**
 * Initialize Serial / UART communication. Should be called once after device power-on.
 */
void serial_setup(void);

/**
 * Print a String on the Serial port.
 * @param s char* to print on Serial/UART connection
 * @param use_prefix true if the default device prefix is to be used, false for no prefix 
 * @return true if the string was printed on Serial, false if it was suppressed
 */
void serial_write(const char *s, bool use_prefix=true);

/**
 * Print a String on the Serial port and add a newline symbol.
 * Same as serial_write(), but with a newline at the end and no return value.
 * @param s char* to print on Serial/UART 
 * @param use_prefix true if the default device prefix is to be used, false for no prefix
 */
void serial_writeln(const char *s, bool use_prefix=true);

/**
 * Convert binary data to Base64 and print it on the Serial port.
 * @param data Binary data to print 
 * @param len Length of binary data in number of bytes 
 * @param add_newline true if a trailing newline should be printed 
 */
void serial_write_base64(const char *data, uint8_t len, bool add_newline=false);

/**
 * Read a string from Serial port.
 * Input is expected to consist of one line of text (with a newline symbol at the end).
 * @param dest char* buffer to hold the input that has been read; set to empty string if no input available.
 * @param maxlen buffer size / maximum length of stored string
 */
void serial_readln(char *dest, int maxlen=256);

/**
 * Process a command received via Serial / UART.
 * @param s char* with command to process 
 * @param processing_mode char* with processing mode, such as "ECHO: " or "REPLAY: "
 */
void serial_process_command(const char *s, const char *processing_mode="ECHO: ");

/**
 * Print a text banner with some device settings on Serial
 */
void serial_banner(void);

/**
 * Enable Serial BT access as equivalent to UART
 */
void serial_bluetooth_enable(void);

/**
 * Disable Serial BT access
 */
void serial_bluetooth_disable(void);

#endif 
/* EOF rdcp-modem-serial.h */