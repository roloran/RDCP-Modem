/* rdcp-modem-rdcp-v04.h */

#ifndef _RDCP_MODEM_RDCP_V04
#define _RDCP_MODEM_RDCP_V04

#include <Arduino.h> 
#include "rdcp-modem-constants.h"

#define RDCPv04_HEADER_SIZE      16
#define RDCPv04_MAX_LORA_PAYLOAD_SIZE 200
#define RDCPv04_MAX_INNER_PAYLOAD_SIZE (RDCPv04_MAX_LORA_PAYLOAD_SIZE - RDCPv04_HEADER_SIZE)
#define RDCPv04_CRC_SIZE         2

#define RDCPv04_TIMESTAMP_ZERO          0
#define RDCPv04_OA_REFNR_SPECIAL_ZERO   0x0000
#define RDCPv04_SEQUENCENR_SPECIAL_ZERO 0x0000

#define RDCPv04_MSGTYPE_TEST                    0x00
#define RDCPv04_MSGTYPE_ECHO_REQUEST            0x01
#define RDCPv04_MSGTYPE_ECHO_RESPONSE           0x02
#define RDCPv04_MSGTYPE_BBK_STATUS_REQUEST      0x03
#define RDCPv04_MSGTYPE_BBK_STATUS_RESPONSE     0x04
#define RDCPv04_MSGTYPE_DA_STATUS_REQUEST       0x05
#define RDCPv04_MSGTYPE_DA_STATUS_RESPONSE      0x06
#define RDCPv04_MSGTYPE_TRACEROUTE_REQUEST      0x07
#define RDCPv04_MSGTYPE_TRACEROUTE_RESPONSE     0x08
#define RDCPv04_MSGTYPE_DEVICE_BLOCK_ALERT      0x09
#define RDCPv04_MSGTYPE_TIMESTAMP               0x0a
#define RDCPv04_MSGTYPE_DEVICE_RESET            0x0b
#define RDCPv04_MSGTYPE_DEVICE_REBOOT           0x0c
#define RDCPv04_MSGTYPE_MAINTENANCE             0x0d
#define RDCPv04_MSGTYPE_INFRASTRUCTURE_RESET    0x0e
#define RDCPv04_MSGTYPE_ACK                     0x0f
  
#define RDCPv04_MSGTYPE_OFFICIAL_ANNOUNCEMENT   0x10
#define RDCPv04_MSGTYPE_RESET_ALL_ANNOUNCEMENTS 0x11
  
#define RDCPv04_MSGTYPE_CITIZEN_REPORT          0x1a
#define RDCPv04_MSGTYPE_PRIVILEGED_REPORT       0x1c
  
#define RDCPv04_MSGTYPE_FETCH_ALL_NEW_MESSAGES  0x20
#define RDCPv04_MSGTYPE_FETCH_MESSAGE           0x21
#define RDCPv04_MSGTYPE_DELIVERY_RECEIPT        0x2a
#define RDCPv04_MSGTYPE_SCHEDULE_RCPT           0x2b
  
#define RDCPv04_MSGTYPE_SIGNATURE               0x30
#define RDCPv04_MSGTYPE_HEARTBEAT               0x31
#define RDCPv04_MSGTYPE_RTC                     0x32

#define RDCPv04_MSGTYPE_TUNNEL                  0x40
#define RDCPv04_MSGTYPE_ROAMING_BEACON          0x41

#define RDCPv04_TIMESLOT_BUFFERTIME 1000

#define RDCPv04_NRT_LEVEL_LOW    0
#define RDCPv04_NRT_LEVEL_MIDDLE 2
#define RDCPv04_NRT_LEVEL_HIGH   4

#define RDCPv04_ADDRESS_MG_LOWERBOUND 0x0300 
#define RDCPv04_ADDRESS_MG_UPPERBOUND 0xFEFF
#define RDCPv04_ADDRESS_BBKDA_LOWERBOUND 0x0100
#define RDCPv04_ADDRESS_DA_LOWERBOUND    0x0200

#define RDCPv04_EP_HEADSTART_DELAY    7500
#define RDCPv04_RELAY1_NO_EP 0xE0

#define RDCPv04_HEADER_RELAY_MAGIC_EP   0xEE
#define RDCPv04_HEADER_RELAY_MAGIC_NONE 0xEE
#define RDCPv04_HEADER_RELAY_MAGIC_EP_ECHO 0xDE
#define RDCPv04_HEADER_RELAY_MAGIC_PERIODICS 0xCE

#define RDCPv04_SIGNATURE_LENGTH 65
#define RDCPv04_PAYLOAD_SIZE_INLINE_TIMESTAMP 6
#define RDCPv04_PAYLOAD_SIZE_ECHO_RESPONSE 0
#define RDCPv04_PAYLOAD_SIZE_ACK_UNSIGNED        3
#define RDCPv04_PAYLOAD_SIZE_ECHO_RESPONSE       0
#define RDCPv04_PAYLOAD_SIZE_INLINE_BLOCKDEVICE  4
#define RDCPv04_PAYLOAD_SIZE_INLINE_TIMESTAMP    6
#define RDCPv04_PAYLOAD_SIZE_INLINE_DEVICERESET  2
#define RDCPv04_PAYLOAD_SIZE_INLINE_DEVICEREBOOT 2
#define RDCPv04_PAYLOAD_SIZE_INLINE_MAINTENANCE  2
#define RDCPv04_PAYLOAD_SIZE_INLINE_INFRARESET   2
#define RDCPv04_PAYLOAD_SIZE_INLINE_RTC          3
#define RDCPv04_PAYLOAD_SIZE_SUBHEADER_CIRE      3
#define RDCPv04_PAYLOAD_SIZE_FANM                2
#define RDCPv04_PAYLOAD_SIZE_FETCHONE            2
#define RDCPv04_PAYLOAD_SIZE_MG_HEARTBEAT        4

#define RDCPv04_BROADCAST_ADDRESS    0xFFFF
#define RDCPv04_HQ_MULTICAST_ADDRESS 0x00FF

#define RDCPv04_ADDRESS_MULTICAST_LOWERBOUND 0xB000
#define RDCPv04_ADDRESS_MULTICAST_UPPERBOUND 0xBFFF

#define RDCPv04_ADDRESS_HQ_LOWERBOUND    0x0001 
#define RDCPv04_ADDRESS_HQ_UPPERBOUND    0x00FF
#define RDCPv04_ADDRESS_SPECIAL_ZERO     0x0000
#define RDCPv04_OA_REFNR_SPECIAL_ZERO    0x0000
#define RDCPv04_SEQUENCENR_SPECIAL_ZERO  0x0000
#define RDCPv04_ADDRESS_SPECIAL_MAX      0xFFFF

#define RDCPv04_CRC_SIZE 2
#define RDCPv04_AESTAG_SIZE 16

/*
 * Subtypes for OFFICIAL ANNOUNCEMENTs
 */
#define RDCPv04_MSGTYPE_OA_SUBTYPE_RESERVED     0x00
#define RDCPv04_MSGTYPE_OA_SUBTYPE_NONCRISIS    0x10
#define RDCPv04_MSGTYPE_OA_SUBTYPE_CRISIS_TXT   0x20
#define RDCPv04_MSGTYPE_OA_SUBTYPE_CRISIS_GFX   0x21
#define RDCPv04_MSGTYPE_OA_SUBTYPE_UPDATE       0x22
#define RDCPv04_MSGTYPE_OA_SUBTYPE_FEEDBACK     0x30
#define RDCPv04_MSGTYPE_OA_SUBTYPE_INQUIRY      0x31
  
/*
 * Subtypes for CITIZEN REPORTs
 */
#define RDCPv04_MSGTYPE_CIRE_SUBTYPE_EMERGENCY  0x00
#define RDCPv04_MSGTYPE_CIRE_SUBTYPE_REQUEST    0x01
#define RDCPv04_MSGTYPE_CIRE_SUBTYPE_RESPONSE   0x02
  
/*
 * Subtypes for ACKNOWLEDGMENTs
 */
#define RDCPv04_ACKNOWLEDGMENT_POSITIVE         0x00
#define RDCPv04_ACKNOWLEDGMENT_NEGATIVE         0x01
#define RDCPv04_ACKNOWLEDGMENT_POSNEG           0x02

/*
 * RDCP Infrastructure modes 
 */
#define RDCPv04_INFRASTRUCTURE_MODE_NONCRISIS      0x00
#define RDCPv04_INFRASTRUCTURE_MODE_CRISIS         0x01
#define RDCPv04_INFRASTRUCTURE_MODE_CRISIS_NOSTAFF 0x02

/**
  * Data structure for an RDCP v0.4 Header
  */
struct rdcpv04_header {
    uint16_t sender;              //< current sender of the message (may be a relay)
    uint16_t origin;              //< original sender of the message (who created it)
    uint16_t sequence_number;     //< for detecting duplicates (specific for "origin")
    uint16_t destination;         //< intended destination (may be the broadcast address)
    uint8_t  message_type;        //< type of RDCP message according to specification
    uint8_t  rdcp_payload_length; //< the length of the inner RDCP payload
    uint8_t  counter;             //< retransmission counter
    uint8_t  relay1;              //< relay 1 designation, delay 1 assignment
    uint8_t  relay2;              //< relay 2 designation, delay 2 assignment
    uint8_t  relay3;              //< relay 3 designation, delay 3 assignment
    uint16_t checksum;            //< CRC-16 (CCITT) checksum
};
  
/**
  * Data structure for storing the RDCP v0.4 Payload of an RDCP Message
  */
struct rdcpv04_payload {
  uint8_t data[RDCPv04_MAX_INNER_PAYLOAD_SIZE]; // RDCP payload must not exceed 184 bytes
};
  
/**
  * Data structure for an RDCP v0.4 Message, consisting of RDCP Header and RDCP Payload
  */
struct rdcpv04_message {
  struct rdcpv04_header  header;
  struct rdcpv04_payload payload;
};

/**
  * Data structure for the overall Duplicate Table
  */
struct rdcpv04_dtable {
  int      num_entries = RDCP_INDEX_NONE;    //< Number of currently stored entries
  uint16_t origin[NUM_DUPETABLE_ENTRIES];    // RDCP Origin address
  uint16_t seqnr[NUM_DUPETABLE_ENTRIES];     //< Sequence Number
  int64_t  last_seen[NUM_DUPETABLE_ENTRIES]; //< Timestamp when last updated
};

/**
 * Calculate the RDCP Message checksum for given data and length.
 * @param data Binary data to calculate the checksum for 
 * @param len Length of the data (number of bytes)
 * @return RDCPv04 16-bit Checksum
 */
uint16_t crc16_rdcpv04(uint8_t *data, uint16_t len);

/**
 * Calculate airtime in milliseconds
 * @param channel Channel id of channel with appropriate LoRa settings 
 * @param payload_size LoRa packet payload size in number of bytes 
 * @return Airtime in milliseconds
 */
uint16_t airtime_in_ms(uint8_t channel, uint8_t payload_size);

/**
 * Verify that a received RDCP v0.4 Message has a correct checksum.
 * @param real_packet_length LoRa packet payload of what was received
 * @return true if checksum is valid, false if packet is broken
 */
bool rdcpv04_check_crc_in(uint8_t real_packet_length);

/**
 * Print an RDCPCSV line for the current RDCP Message.
 */
void rdcpv04_print_csv(void);

/**
 * Enable or disable the RDCPv04 CSV Logfile
 * @param enabled true to enable, false to disable
 */
void rdcpv04_csvlogfile_set_status(bool enabled);

/**
 * Delete the current RDCPv04 CSV Logfile
 */
void rdcpv04_csvlogfile_delete(void);

/**
 * Print the content of the current RDCPv04 CSV Logfile
 */
void rdcpv04_csvlogfile_dump(void);

/**
 * Append a line of text to the current RDCPv04 CSV Logfile
 * @param line String to append to the logfile
 */
void rdcpv04_csvlogfile_append(char* line);

/**
 * Determine whether an RDCP v0.4 Message is a duplicate based on Origin/SeqNr pairs.
 * @param origin RDCP Origin 
 * @param sequence_number RDCP Sequence Number
 * @return true if duplicate, false if fresh
 */
bool rdcpv04_check_duplicate_message(uint16_t origin, uint16_t sequence_number);

/**
 * Update CFEst after RDCP v0.4 Message reception.
 */
void rdcpv04_update_cfest_rx(uint8_t mode=UPDATE_CFEST_MODE_RX);

/**
 * Update CFEst when transmitting an RDCP v0.4 message
 * @param channel Channel id of channel on which the relevant message is about to be sent
 */
void rdcpv04_update_cfest_tx(uint8_t channel);

/**
 * Get channel free estimation.
 * @param channel Channel id
 * @return Timestamp of when channel is expected to be free
 */
int64_t rdcpv04_get_channel_free_estimation(uint8_t channel);

/**
 * Set the channel free estimation for a channel.
 * @param channel Channel id
 * @param new_value Timestamp of when channel will be free
 */
void rdcpv04_set_channel_free_estimation(uint8_t channel, int64_t new_value);

/**
 * Set the channel free estimation for a channel, but only actually update it 
 * if channel will be busier for a longer time than assumed so far.
 * @param channel Channel id 
 * @param new_value Timestamp of when channel will be free
 * @return true if CFEst was adjusted, false if `new_value` is less than known CFEst
 */
bool rdcpv04_prolong_channel_free_estimation(uint8_t channel, int64_t new_value);

/**
 * Get the SF multiplier for a channel (used for waiting/scheduling times).
 * @param channel Channel for which the SF multiplier is requested
 */
int rdcpv04_get_sf_multiplier(uint8_t channel);

/**
 * Get the duration of a timeslot on a specific channel for a given message.
 * @param channel Channel id of used channel
 * @param data RDCP Message to analyze
 */
int64_t rdcpv04_get_timeslot_duration(uint8_t channel, uint8_t *data);

/**
 * Basic handling of incoming RDCP v0.4 Messages.
 */
void rdcpv04_process_incoming_message(void);

/* RTC Handling */
struct rtc_entry {
    bool active     = OPTION_DISABLED;
    int64_t alarm   = ZERO_TIMESTAMP;
    uint8_t restart = COUNT_ZERO;
    uint8_t persist = COUNT_ZERO;
    char rtc[INFOLEN];
};
#define MAX_RTC 16

/**
 * Periodically send RDCP v0.4 heartbeats.
 */
void rdcpv04_check_heartbeat(void);

/**
 * Periodically check for active RTCs.
 */
void rdcpv04_cmd_check_rtc(void);

#define TUNNEL_TYPE_LORAWAN     0
#define TUNNEL_TYPE_IPv4        1
#define TUNNEL_TYPE_IPv6        2
#define TUNNEL_TYPE_LOCALSENSOR 3

/**
 * Send an RDCP v0.4 TUNNEL message (backported from RDCP v0.5) to the HQ multicast address.
 * @param channel Channel to schedule the tunneled message for; note that a radio must be switched to this channel before of after using this function to have it sent.
 * @param data Binary data to be tunneled, max. 182 bytes for RDCP v0.4
 * @param len Length of `data`, i.e., number of bytes of the tunneled payload
 * @param tunneltype Tunneled payload type, e.g., TUNNEL_TYPE_LORAWAN or TUNNEL_TYPE_LOCALSENSOR
 */
void rdcpv04_tunnel(uint8_t channel, uint8_t* data, uint8_t len, uint8_t tunneltype);

#endif 

/* EOF rdcp-modem-rdcp-v04.h */