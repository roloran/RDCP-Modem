/* rdcp-modem-constants.h */

#ifndef _RDCP_MODEM_CONSTANTS 
#define _RDCP_MODEM_CONSTANTS

// At least on ESP32, increase the stack size from the 8 kb default
#define DEVICE_STACK_SIZE 16*1024

//< Number of LoRa radios supported by the firmware
#define MAX_NUMBER_OF_RADIOS 8

//< Maximum number of LoRa channels used by the firmware
#define MAX_NUMBER_OF_CHANNELS 16

// Settings for the delay following power-on and Serial0 initialization
#if defined(ESP32)
#define PRE_START_DELAY_NUM  5
#elif defined(USE_NRF52)
#define PRE_START_DELAY_NUM 10
#define USE_LFXO
#endif
#define PRE_START_DELAY_TIME 200

// Hardware-specific internal processing time before slotted RDCP retransmissions
#define RETRANSMISSION_PROCESSING_TIME 200

// Number of entries in the global scheduler TX Queue
#define MAX_TXQUEUE_ENTRIES 32

// Maximum number of Serial connections for AIR radios besides Serial0
#define AIR_NUM_SERIAL 2

// Serial port settings
#define SERIAL_BAUDRATE 115200
#define SERIAL_TIMEOUT  10
#define SERIAL_AIR_BAUDRATE 115200
#define SERIAL_AIR_TIMEOUT  10

// Various buffer sizes
#define INFOLEN        256
#define LONGINFOLEN    512
#define AIRMSGLEN      512
#define FILENAMELEN     32
#define SERIALINPUTLEN 512
#define DATALEN        256
#define LEN32           32
#define KEYLENTEXT     256
#define KEYLENAES       32
#define SHABUFSIZE      32
#define SIGBUFSIZE     128 
#define NONCENAMESIZE   64

// Maximum payload size in bytes of a LoRa packet
#define MAX_LORA_PAYLOAD_SIZE 250

// Maximum number of entries in the RDCP duplicate table
#define NUM_DUPETABLE_ENTRIES 256

// Some factors for time conversions
#define SECONDS_TO_MILLISECONDS 1000
#define MINUTES_TO_MILLISECONDS 60000 
#define HOURS_TO_MILLISECONDS   3600000
#define MILLISECONDS_TO_MICROSECONDS 1000
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24
#define SECONDS_PER_HOUR 3600

// Constants used for various data structs
#define ZERO_TIMESTAMP 0
#define ZERO_LENGTH    0

// Constants used for LoRa radio types and interfaces
#define RADIO_TYPE_SX1262 0
#define RADIO_TYPE_SX1268 1
#define RADIO_TYPE_AIR    2
#define SPI_NONE          0
#define SPI_ONE           1
#define SPI_TWO           2
#define SPI_AIR_NONE      64
#define SPI_AIR_ONE       65
#define SPI_AIR_TWO       66

// Constants used for LoRa channels
#define CHANNEL0 0
#define CHANNEL1 1
#define CHANNEL2 2
#define CHANNEL3 3
#define CHANNEL4 4
#define CHANNEL5 5
#define CHANNEL6 6
#define CHANNEL7 7
#define CHANNEL8 8
#define CHANNEL9 9
#define CHANNEL10 10 
#define CHANNEL11 11 
#define CHANNEL12 12 
#define CHANNEL13 13 
#define CHANNEL14 14 
#define CHANNEL15 15 

// Constants used for radios
#define RADIO0 0
#define RADIO1 1
#define RADIO2 2
#define RADIO3 3
#define RADIO4 4
#define RADIO5 5
#define RADIO6 6
#define RADIO7 7

// Constants used for AIR radios
#define AIRRADIO0 0
#define AIRRADIO1 1
#define AIRRADIO2 2
#define AIRRADIO3 3
#define AIRRADIO4 4
#define AIRRADIO5 5
#define AIRRADIO6 6
#define AIRRADIO7 7

#define AIRMODEM_SERIAL_NONE 99
#define AIRMODEM_SERIAL0 0
#define AIRMODEM_SERIAL1 1
#define AIRMODEM_SERIAL2 2

#define AIRHOP_DIRECT_NEIGHBOR 0

// Constants with magic values
#define NO_REAL_RADIO       99
#define NO_REMOTE_RADIO     99
#define NO_AIR_RADIO        99
#define NO_CHANNEL          99
#define CURRENT_CHANNEL    100
#define RADIO_SIM          100
#define NO_RADIO_AVAILABLE 127
#define BASE10              10
#define BASE16              16

// Array indices and special bytes in strings
#define FIRST_IN_GROUP  0
#define SECOND_IN_GROUP 1
#define THIRD_IN_GROUP  2
#define FOURTH_IN_GROUP 3
#define FIRST_BYTE_IN_ARRAY 0
#define ZEROBYTE 0

// Flags for various function calls
#define FORCED_CHANNEL_SWITCH true
#define NO_FORCED_CHANNEL_SWITCH false

#define CAD_CHANNEL_BUSY true 
#define CAD_CHANNEL_FREE false
#define CAD_STATE_FREE 0
#define CAD_STATE_BUSY 1
#define NO_CAD_RESULT_AVAILABLE false
#define CAD_RESULT_AVAILABLE true

#define NO_MESSAGE_AVAILABLE false 
#define MESSAGE_AVAILABLE    true

#define CALLBACK_LOCAL 0 
#define CALLBACK_AIR   1

#define SEND_ENABLED true 
#define SEND_DISABLED false

// Constants used for LoRa channel specifications
#define NO_BW 0.0
#define BW125 125.0
#define BW250 250.0 

#define SF7 7
#define SF8 8
#define SF9 9
#define SF10 10
#define SF11 11
#define SF12 12

#define CR5 5
#define CR6 6
#define CR7 7
#define CR8 8

#define PL08 8
#define PL15 15
#define NO_FREQ 0.0

#define NO_RSSI 0.0
#define NO_SNR  0.0

#define TXPOWER_LOW    0
#define TXPOWER_MEDIUM 11
#define TXPOWER_HIGH   22

#define SYNCWORD_PRIVATE 0x12
#define SYNCWORD_PUBLIC  0x34

// Constants used for callback functions
#define CALLBACK_TYPE_UNKNOWN 0
#define CALLBACK_TYPE_TXSTART 1
#define CALLBACK_TYPE_TXFIN   2 
#define CALLBACK_TYPE_RX      3 
#define CALLBACK_TYPE_CADRES  4
#define TX_CALLBACK_NONE      0

// Constants used as filename prefixes
#define FILENAME_AUTOSTART "/autostart.lnk"
#define FILENAME_INITSCRIPT_DEFAULT "/config.1"
#define FILENAME_PREFIX_SEQNR "/rdcp_seqnr_"
#define FILENAME_PREFIX_NONCE "/rdcp_nonce_"

// Constants used for Serial prefix handling
#define DEFAULT_SERIAL_PREFIX "ECHO: "
#define USE_PREFIX true
#define DONT_USE_PREFIX false
#define HAS_SERIAL0_INPUT true

// Constants used in pinouts
#define PIN_NOT_USED -1

// Channel Free Estimator modes
#define CFEST_MODE_UNKNOWN      0
#define CFEST_MODE_RDCP_V04_433 1
#define CFEST_MODE_RDCP_V04_868 2

// LoRa payload types 
#define PAYLOAD_TYPE_GENERIC_LORA 0
#define PAYLOAD_TYPE_RDCP_V04     1

// Constants used for delays
#define MINIMUM_DELAY 1

// Constants used for RAM thresholds
#define ZERO_RAM 0
#define MINIMUM_FREE_RAM 8192

//< Generic, RDCP- and scheduling-related constants
#define CFEST_ZERO 0

#define RDCP_ADDRESS_ZERO 0x0000
#define AIRTIME_ZERO   0
#define TIMESTAMP_ZERO 0
#define DURATION_ZERO  0
#define TIMESLOTS_ZERO 0
#define COUNT_ZERO     0 

#define OPTION_ENABLED true 
#define OPTION_DISABLED false

#define IN_PROCESS true 
#define NOT_IN_PROCESS false
#define IS_WAITING true 
#define IS_NOT_WAITING false
#define IN_CAD_MODE true 
#define NOT_IN_CAD_MODE false

#define RDCP_INDEX_NONE -1
#define THIS_ONE         1
#define ZERO_TIMESLOTS   0
#define ZERO_DELAY       0

#define SCHEDULING_MODE_CHANNEL_FREE 0
#define SCHEDULING_MODE_FIXED_TIME   1

#define TX_TIMEOUT 60000
#define TX_LATENCY_CAP 250

#define UPDATE_CFEST_MODE_RX 0
#define UPDATE_CFEST_MODE_TX 1

// RDCPCSV-related constants
#define RDCPv04_CSVLOGFILE_MAX_ENTRIES 5000
#define FILENAME_RDCPv04CSV_LOGFILE "/rdcp_v04_csv.log"

#define DEFAULT_NRF52_MISO  23
#define DEFAULT_NRF52_MOSI  22
#define DEFAULT_NRF52_CLOCK 19

#endif

/* EOF rdcp-modem-constants.h */