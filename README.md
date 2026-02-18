# ROLORAN RDCP-Modem Implementation with AIR Support

Generic LoRa firmware based on RadioLib for rapid prototyping with RDCP and Array of Independent Radios (AIR) support.

Key characteristics:

- LoRa and RDCP (ROLORAN Disaster Communication Protocol) support (see [RDCP Specs](https://github.com/roloran/RDCP-Specs))
- Generic and configurable. Most settings can be adjusted both in the source code and using run-time configuration via Serial/UART commands, including the pinout of connected LoRa radios. Run-time configuration commands can be saved to so-called initscripts for persistence across device power cycles.
- This firmware introduces AIR support. AIR is an acronym for _Array of Independent Radios_. Several devices, such as commercial-off-the-shelf LoRa development boards, can be linked via Serial/Serial1/Serial2 connections and make remote use of each other's physical LoRa radios. Typically, one device (referred to as AIR controller) will use the AIR radios provided by connected other devices to increase the number of LoRa radios available to the LoRa/RDCP application running on a single primary device. However, having peer implementations operate an AIR array jointly can also be used to improve resilience against hardware faults.
- Source code extensibility. As is, the firmware in this project can be used to send and receive LoRa packets with multiple LoRa radios on multiple LoRa channels and provides some functionality typical for RDCP devices. However, its main purpose is to be extended with new LoRa/RDCP functionality by plugging into loops provided, adding own RDCP messages or LoRa packets to the scheduling system, reacting to incoming packets or Serial commands etc.

As of now, support for SX1262, SX1268, and AIR radios has been implemented; basically any LoRa radio supported by RadioLib can be used by extending a few parts of this project's source code.

## Building, flashing, and configuring

!!! NEVER power on a device without antennas connected to its physical LoRa radios !!!

This repository uses PlatformIO (PIO), and thus all dependency management and workflows should be handled automatically or at least similar to other PIO projects.

While using PIO through IDEs like Visual Studio Code works if you prefer GUIs, the recommended workflow is CLI-based along with [ROLORAN-Terminal](https://github.com/roloran/ROLORAN-Terminal) as RDCP-enhanced serial monitor solution, for example:

```bash
export LORADEV=/dev/cu.usbserial-0001
platformio run --target upload --upload-port $LORADEV
roloran-terminal.py
```

The firmware can be used as-is and be configured at run-time, including the specification of LoRa radio pinout settings. However, the following settings should be adjusted before building and flashing the firmware:

- Bluetooth (BT) vs. Bluetooth Low Energy (BLE) support. Most hardware devices support only one of those two variants. Set `DEVICE_HAS_BLUETOOTH` or `DEVICE_HAS_BLUETOOTH_LE` in `rdcp-modem-hardware-settings.h` accordingly.

You may also hard-code any configuration settings so the device can be used without provisioning via Serial/UART commands. Project-specific source adjustments can be made in:

- `include/rdcp-modem-hardware-settings.h`: Radio hardware settings, Serial/UART settings
- `include/rdcp-modem-lora-settings.h`: Default device configuration
- `src/rdcp-modem-hardware-settings.cpp`: Pinout and radio module injection for pre-configured LoRa radios
- `src/rdcp-modem-lora-settings.cpp`: Default channel configuration 

Source code-based and run-time configurations can be mixed as run-time settings can overwrite most of the defaults stored in the firmware.

## Commands for use over Serial/UART 

The commands to control the firmware via Serial/UART are intended to be used by automated provisioning processes and controlling software, not manually, except for manual tests and experimentation as a part of custom hardware and application prototyping. Note that wrong configuration settings can fry your hardware. Some of the commands are easier to understand once a couple of devices has been configured by modifying the settings in the firmware's source code. Note that the syntax of the commands must be adhered to strictly; there is no user-friendly error handling yet. The provided commands are intended for developers, not end-users.

!!! If in doubt, do not use these commands !!!

- Serial commands can be prefixed with `+` to store them in the currently used initscript, with `!` to suppress their echo, and with `+!` as a combination.

- `DELAY %d` delays further operation by the given number of milliseconds. Can be useful as part of an initscript to wait while AIR-controlled devices have finished their initialization when powered on at the same time.

- `RADIOINIT` initializes the configured radios on their configured default channels. Typically used towards the end of initscripts.

- `SET RADIO NUM %d` sets the total number of radios used by the device, i.e., the sum of physical and AIR radios.

- `SET RADIO AIR a b c d e f` configures an AIR radio:

    - `a` is the local radio id, starting at `0`. 
    - `b` the Serial port to use (`0` for Serial, `1` for Serial1, `2` for Serial2).
    - `c` is the Serial port-specific local radio id, starting at `0`.
    - `d` is the number of hops to the AIR destination, `0` for the direct neighbor. 
    - `e` is the remote radio id, i.e., the physical radio number on the AIR device.
    - `f` is the default channel to use for this AIR radio.

- `SET RADIO SX1262 a b c d e f g h i j k l m n o` configures an SX1262 radio:

    - `a` is the local radio id.
    - `b` is the local radio id in the group of SX1262 radios, starting at `0`.
    - `c` is the SPI interface to use, usually either `1` or `2`.
    - `d` is the MISO pin number.
    - `e` is the MOSI pin number. 
    - `f` is the CLOCK pin number. 
    - `g` is the CS/NSS pin number. 
    - `h` is the DIO1 pin number. 
    - `i` is the BUSY pin number. 
    - `j` is the RESET pin number. 
    - `k` is the TXenable pin number. 
    - `l` is the RXenable pin number. 
    - `m` is the default channel for this radio.
    - `a`-`c` are single digits. `d`-`m` are two-digit numbers. Pins can be set to `-1` if not used. Note that pin numbers typically correspond to GPIO pins. When setting up a new type of LoRa development board, check the vendor-provided pinout diagrams, factory firmware, or other LoRa software implementations for the correct pinout.

- `SET RADIO SX1268 a b c d e f g h i j k l m n o` configures an SX1268 radio. This command uses the same syntax as `SET RADIO SX1262`; note that `b` applies to the group of SX1268 radios, respectively.

- `SET RADIO INTERFACE a b c d e f g h i` sets hardware parameters for SX1262/SX1268/AIR radios: 

    - `a` sets whether the first SPI interface should be reset on first use. Must be `0` (do not reset) or `1` (do reset).
    - `b` is the same as `a`, but for the second SPI interface. 
    - `c` sets whether the Serial interface should be prepared to provide (not use) AIR radios. Must be `0` (disable) or `1` (enable). Typically disabled.
    - `d` is the same as `c`, but for Serial1. Typically enabled if at least one AIR radio is provided or proxied to a controller on Serial1.
    - `e` is the same as `c`, but for Serial2. Typically enabled if the device is wired to two other AIR devices.
    - `f` specifies the RX pin for Serial1 (two-digit number or `-1` if not used).
    - `g` specifies the TX pin for Serial1. 
    - `h` specifies the RX pin for Serial2. 
    - `i` specifies the TX pin for Serial2.

- `SET CHANNEL NUM %d` sets the total number of channels used by the device.

- `SET CHANNEL LORA a b c d e f g h i j` configures a LoRa channel:

    - `a` is the channel number, starting at `0`. (must be 2 digits)
    - `b` is the channel frequency in MHz, e.g., `868.200`. (must be `%7.3f`)
    - `c` is the channel bandwidth in kHz, e.g., `125`. (must be 3 digits)
    - `d` is the spreading factor to use, e.g., `07`. (must be 2 digits)
    - `e` is the coding rate to use, e.g., `5`. (value range as in RadioLib)
    - `f` is the syncword to use, e.g., `12`. (must be 2 hex digits)
    - `g` is the TX power in dBm, e.g., `00`. (must be 2 digits)
    - `h` is the preamble length in symbols, e.g., `15`. (must be 2 digits)
    - `i` indicates whether sending on the channel is enabled (`1`) or disabled (`0`).
    - `j` sets the CFEst mode for the channel. `0` = Generic LoRa/LoRaWAN, `1` = RDCP v0.4 433 MHz, `2` = RDCP v0.4 868 MHz.

- `DUMP OVERVIEW` prints a compact overview of the current device configuration. 

- `DUMP INITSCRIPT` prints the currently used initscript.

- `SET CONFIG BLUETOOTH a b` sets the device Bluetooth (BT/BLE) configuration:

    - `a` enables or disables Bluetooth (BT/BLE) access (`1` or `0`). Note that the Bluetooth type (BT or BLE) is chosen hard-coded in the firmware. 
    - `b` is the Bluetooth device name used when Bluetooth access is enabled.

- `SET CONFIG PRINT a b c d e f` sets the Serial output configuration:

    - `a` enables or disables the printing of RX/TX lines (`1` or `0`).
    - `b` enables or disables the printing of RXMETA/TXMETA lines (`1` or `0`).
    - `c` enables or disables the printing of RDCPCSV lines on RX (`1` or `0`).
    - `d` enables or disables the printing of AIR information lines (`1` or `0`).
    - `e` enables or disables the printing of the prefix on Serial AIR messages (`1` or `0`).
    - `f` is the Serial prefix to use, e.g., `RDCP-Modem:`. A trailing space is appended automatically.

- `SET RDCP LEGACY a b c d` sets configuration options for use with legacy RDCP devices:

    - `a` is the radio id of the default radio to use
    - `b` is the default channel to use (must be 2 digits)
    - `c` sets Serial0 legacy mode (`1` = enable, `0` = disable). Should be enabled for MERLIN HQ use only.
    - `d` sets HQ mode(`1` = enable, `0` = disable). Should be enabled for MERLIN HQ use.

- `SET RDCP NUMRELAYS n` sets the number of RDCP Relays in the active RDCP scenario to `n`.

- `SET RDCP ADDR n` sets the device's RDCP address to `n` (must be up to 4 hex digits).

- `SERIAL text` sends `text` to the Serial interface without the Serial prefix. Can be useful if a device, such as an RDCP v0.4 DA, is connected to Serial.

- `SERIALP text` is the same as `SERIAL`, but includes the Serial prefix.

- `SERIAL1 text` is the same as `SERIAL`, but for Serial1.

- `SERIAL2 text` is the same as `SERIAL`, but for Serial2.

- `INITSCRIPT DELETE` removes the currently used initscript.

- `INITSCRIPT NAME fn` sets `fn`as the new initscript filename.

- `AIR message` sends an AIR message to the device on Serial. See `include/rdcp-modem-airmodem.h` for message format details.

- `SIMRX cc base64lorapacketpayload` simulates receiving a new LoRa packet on the given channel `cc` (must be 2 digits) with the payload given as Base64-encoded string.

- `RESTART` or `REBOOT` restart the device.

- `TX base64lorapacketpayload` is a legacy command for MERLIN HQ backwards compatibility. Do not use otherwise.

- `TXSCHED n base64lorapacketpayload` is a legacy command for MERLIN HQ backwards compatibility. Do not use otherwise.

- `TXCF a b base64lorapacketpayload` schedules a message for sending once the channel has become free:
    - `a` is the channel number (must be 2 digits)
    - `b` is the payload type (`0` - generic LoRa/LoRaWAN, `1` - RDCP v0.4)
    - The LoRa packet payload to send must be given as Base64-encoded string and conform to the given payload type.

- `TXFT a b c base64content` schedules a message for sending at a fixed time:
    - `a` is the channel number (must be 2 digits)
    - `b` is the payload type (as with `TXCF`)
    - `c` specifies the fixed time for sending the message (positive number: delay in milliseconds, `0` send at current CFEst, negative number: append to current CFEst)
    - Use `TXCF` whenever possible. Fixed-time sending is usually the wrong choice unless protocol specs mandate it.

- `CSVLOG command` controls the logging of RDCPCSV lines on the device. `command` is either of
    - `ENABLE` to start logging RDCPCSV lines
    - `DISABLE` to stop logging
    - `DUMP` to print the logged RDCPCVS lines
    - `DELETE` to delete the logfile

- `PIN OH n` sets GPIO pin `n` to output mode and high.

- `PIN OL n` sets GPIO pin `n` to output mode and low.

- `PIN II n` sets GPIO pin `n` to pure input mode.

- `PIN IU n` sets GPIO pin `n` to input mode with pull-up.

- `PIN ID n` sets GPIO pin `n` to input mode with pull-down.

- `SPI END` performs an end() operation on the default SPI interface.

- `SPI BEGIN aa bb cc` performs a begin() operation on the default SPI interface with the given clock, MISO, and MOSI GPIO pins.

- `SPI JUSTBEGIN` is the same as `SPI BEGIN`, but uses the MISO, MOSI, and CLK pins pre-configured in the source code without creating a new SPIClass.

- `SWITCH a b` forces radio `a` to switch to channel `b`.

- `SLEEP LIGHT` will put an ESP32 device into light sleep to be woken up by LoRa packet reception on a physical LoRa radio or after a timer fires. Note that AIR radios cannot be used as wake-up triggers.

- `SLEEP DEEP` is similar to `SLEEP LIGHT`, but uses deep sleep on ESP32 devices. Note that ESP32 devices restart on waking from deep sleep, so any run-time states (such as messages queued for TX but not sent yet) are lost. To avoid longer wake-up times, it is recommended to hard-code any radio and channel settings in the firmware instead of using initscripts.

- `SLEEP TIMER a` sets the wake-up timer to `a` milliseconds on ESP32 devices for both light and deep sleep (set `a` to `0` to disable timer-based wake-up). (ESP32 only)

- `SLEEP WAKEUPPIN a` sets an additional GPIO pin `a` that can be used to wake up from (light or deep) sleep (`-1` to disable). Physical radio DIO1 pins are always used. Note that this additional GPIO pin usually has to be prepared by own code added to the firmware; otherwise it might be in a (default) state that always wakes the device immediately. (ESP32 only)

- Sending an empty line or one containing only a single space will print some status information.

## Hardware wiring and configuration for AIR radios

Using or providing AIR radios requires an additional Serial connection between the hardware devices.

### Example with two devices 

Assume that we have two hardware devices, A and B, with one physical LoRa radio each. B shall be set up to provide its physical LoRa radio as an AIR radio to A.

For the hardware wiring, perform the following steps:

- Choose two free GPIO pins for the Serial1 connection on each device. We name then A.RX and A.TX for A, and B.RX and B.TX for B.
- Set up three wires to connect A and B (using jumper cables when prototyping, or on your PCB):
  - Establish common ground: Connect any GND pin on A to any GND pin on B
  - Connect A.TX to B.RX
  - Connect A.RX to B.TX
  - Note the cross-over between RX and TX on both devices.

For the firmware configuration on both devices, make sure that Serial1 is initialized for AIR use properly (see `SET RADIO INTERFACE`, parameters `d`, `f`, and `g`).

Device B must only configure its physical LoRa radio. Since we assume that it only has one physical LoRa radio, typically radio id `0` will be used on B.

Device A should configure two radios: Radio id `0` will typically be used for its own physical LoRa radio. Then, `SET RADIO AIR 1 1 0 0 0 1` can be used to set up the AIR radio. The first parameter indicates that the local radio id `1` is used (since `0` is already A's own physical LoRa radio). The second parameter indicates the use of Serial1. The third parameter must be `0` as this is the first AIR radio on Serial1. The fourth parameter indicates zero hops, as device B is connected directly to device A. The fifth parameter is B's physical radio id. The final parameter sets the default channel for the AIR radio; a LoRa channel with this id has to be set up on both devices before their (physical and AIR) radios are initialized with `RADIOINIT`.

A configuration for device B could look like this (pinout for Heltec LoRa32 v3.2/v4 with GPIO pins 15 and 16 for Serial1):
```
+SET RADIO NUM 1
+SET CHANNEL NUM 2
+SET CHANNEL LORA 00 869.525 125 07 5 12 00 15 1 2
+SET CHANNEL LORA 01 868.200 125 07 5 12 00 15 1 2
+SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 1
+SET RADIO INTERFACE 0 0 0 1 0 15 16 -1 -1
+RADIOINIT
```

A configuration for device A could then look like this:
```
+DELAY 100
+SET RADIO NUM 2
+SET CHANNEL NUM 2
+SET CHANNEL LORA 00 869.525 125 07 5 12 00 15 1 2
+SET CHANNEL LORA 01 868.200 125 07 5 12 00 15 1 2
+SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 0
+SET RADIO AIR 1 1 0 0 0 1
+SET RADIO INTERFACE 0 0 0 0 0 15 16 -1 -1
+RADIOINIT
```

### Example with three devices

Assume that we have three hardware devices, A, B, and C, with one physical LoRa radio each. A will use B and C as AIR radios. One way to do the hardware wiring is to connect A to B and additionally B to C:

- Choose two free GPIO pins for the Serial1 connection on each device, named A.RX1, A.TX1, B.RX1, B.TX1, C.RX1, and C.TX1.
- Choose two additional free GPIO pins for the Serial2 connection on B, named B.RX2 and B.TX2.
- Set up six wires to connect A, B, and C:
  - Establish common ground: Connect any GND pin on A to any GND pin on B, and any (other) GND pin on B to any GND pin on C
  - Connect A.TX1 to B.RX1
  - Connect A.RX1 to B.TX1
  - Connect B.RX2 to C.TX1
  - Connect B.TX2 to C.RX1

For the firmware configuration, devices A and C must initialize their Serial1 for AIR use, and device B must initialize its Serial1 and Serial2 ports.

Devices B and C must only configure their physical LoRa radios, typically with radio id `0` each.

Device A should configure its physical LoRa radio and both AIR radios. For the AIR radio provided by B, `SET RADIO AIR 1 1 0 0 0 1` can be used like in the example with two devices above. The AIR radio provided by C can be configured with `SET RADIO AIR 2 1 1 1 0 2`.

### AIR limitations

From a conceptual perspective, AIR radios act as proxies for selected RadioLib functions to remote devices connected via multi-hop Serial connections. While this enables setting up multi-channel LoRa devices made with COTS LoRa development boards, it has some limitations, most notably: 

- From a cost and energy efficiency perspective, it might be better to use multi-channel LoRa radios such as those used in LoRaWAN gateways. However, this depends on COTS hardware availability, more complex custom hardware and software design, and whether sending on a single channel while not being able to listen on the other channels is sufficient. AIR radios can be operated in parallel with dedicated antennas for their frequency bands.

- Serial communication and device-internal processing times for AIR messages inherently induce a bit of latency. While this is irrelevant for most of the functionality, two currently resulting pain points are:

    - CAD results (channel activity detection) may be out of time. A channel reported as busy may meanwhile be free and vice versa. This can be worked around by providing a "start sending with CAD" functionality via AIR messages.
    - Fetching a random number generated by a physical LoRa radio is slow via AIR. If a higher-throughput hardware randomness source is required, buffering and batch processing functionality should be implemented on the AIR provider side.

On ESP32 devices, all three of Serial, Serial1, and Serial2 can be used to connect AIR radios. Serial is usually also accessible via USB, and the pins for Serial1/Serial2 can be chosen via configuration. For nRF52 devices, the implementation currently only uses the hardware UARTs: Serial is available via USB, and Serial1 uses the TX/RX pins defined by the board chosen in `platformio.ini`. More flexibility could be achieved by extending the implementation to use the `SoftwareSerial` library in order to also provide a third serial interface, or to make the pinout of Serial1 more flexible.

## Using initscripts

Serial configuration commands should be tested before persisting them. If the device freezes during initscript processing, there is no comfortable way of removing the offending commands.

Initscripts should perform the following actions:

- Set basic device configuration, including the number of radios used, the number of channels used, and the appropriate `RADIO INTERFACE` settings.
- Configure all required LoRa channels (for use by physical as well as AIR radios)
- Configure all (physical and AIR) radios
- If required, configure other device and RDCP settings, such as enabling Bluetooth access
- Initialize the radios

## Extending the implementation

For contributing to the source code, an obvious recommendation is to study the project structure and the functions already provided based on the C header files in the `include/` directory and their documentation. Naming and code style conventions should be also used for any extensions.

To add new functionality, `src/rdcp-modem-plugin.cpp` is a good point to start.

## Hardware examples 

### Heltec LoRa32 V3.2 and V4

The 868 MHz version of Heltec LoRa32 devices (both V3 and V4) has one SX1262 radio, which can be configured with

```
SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 0
```

The recommended Serial1 and Serial2 pins on V4 are 15/16 and 17/18 (the GPIO pin pairs left and right to the antenna connector). The AIR setup examples above include complete initscripts for V4 devices.

### LILYGO T-Deck and T-Deck Plus

Some devices come with multiple components on their primary SPI interface and need some care for their initialization. For LILYGO devices, their open source factory / unit test firmware gives some clues on how to proceed. To set up an 868 MHz T-Deck or T-Deck Plus, the following initscript can be used:

```
SET RADIO NUM 1
SET CHANNEL NUM 1
SET CHANNEL LORA 00 868.200 125 07 5 12 00 15 1 2
SET RADIO SX1262 0 0 1 38 41 40 09 45 13 17 -1 -1 0
SET RADIO INTERFACE 0 0 0 0 0 -1 -1 -1 -1
PIN OH 39
PIN OH 12
PIN OH 09
PIN IU 38
SPI BEGIN 40 38 41
RADIOINIT
```

### Heltec Mesh Node T114 v2.0

Heltec's Mesh Node T114 is nRF52840-based; it is recommended to configure its LoRa SPI pins (MISO 23, MOSI 22, CLK 19) in the firmware. In its factory default, the firmware can be flashed by uploading a `.uf2` file to the device after pressing the reset button twice.

```
python3 tools/uf2conv.py .pio/build/heltec_mesh_node_t114_V20/firmware.bin --family 0xADA52840 --base 0x26000 --convert --output firmware.uf2
```

For use as a stand-alone device, the following initscript can be used:

```
SET CHANNEL NUM 1
SET CHANNEL LORA 00 868.200 125 07 5 12 00 15 1 2
SET RADIO NUM 1
SPI JUSTBEGIN
SET RADIO SX1262 0 0 1 23 22 19 24 20 17 25 -1 -1 0
SET RADIO INTERFACE 0 0 0 0 0 -1 -1 -1 -1
RADIOINIT
```

### ROLORAN Dual Channel Board (v2025)

The dual-channel board developed in the ROLORAN project for the RDCP two-channel relay is ESP32-based and uses Ebyte LoRa two radios in the 433 MHz and 868 MHz domain connected to two different SPI interfaces. A proper PlatformIO environment is provided in `platformio.ini`. For stand-alone use, the following initscript can be applied:

```
SET RADIO NUM 2
SET CHANNEL NUM 2
SET CHANNEL LORA 00 869.525 125 07 5 12 00 15 1 2
SET CHANNEL LORA 01 433.175 125 07 5 12 00 15 1 1
SET RADIO SX1262 0 0 1 36 23 18 05 27 34 21 16 19 0
SET RADIO SX1268 1 0 2 39 13 14 17 33 35 22 26 25 1
SET RADIO INTERFACE 1 1 0 0 0 -1 -1 -1 -1
RADIOINIT
```

<!-- EOF README.md -->