# Embedded MIDI Keyboard

A PCB embedded at the bottom of a custom guitar, with capacitive touch electrodes that function like keyboard keys. Touching keys sends MIDI messages over Bluetooth.

![embedded keyboard](./visualization.png)

Each of the 13 keys is further divided into electrodes, allowing the user to slide their finger up and down on each key to control MIDI "brightness" parameter (`CC74`), which would typically be linked to opening/closing a filter on the synthesizer being controlled by this MIDI keyboard.

![mapping](./mapping.png)

## Overview

The software will adhere to MIDI specification:

* All MIDI messages are sent on channel `1`
* Touching any electrode of a key will send a MIDI `Note On` message with maximum velocity and the note value set according to the key index
* When touch is no longer registered on a key, the controller will send a MIDI `Note Off` message with minimum velocity
* Only one note and one electrode can be active at a time (*monophonic* behavior). If several are registering touch, the software will try to find the geometric center of all the electrodes being touched and consider the electrode that intersects the center as the active one.
* Depending on which key electrode was touched (`0` through `12`), the controller will send a `Control Change` message with controller `74` (or `4AH`, mapped to *brightness* in MIDI specification) and value equal to the index of the key segment mapped onto `0` through `127` range.

## Requirements

+ Given my finger is over `C1` key
+ When I touch any of the electrodes within this key
  - Then I get **MIDI Control Change** message on channel `1`
    * And Controller is `4AH` (`74` decimal) labeled "Brightness" in MIDI manual
    * And Control Value is between `0` and `127` depending the closest electrode to my fingerHp when the key is touched (see Mapping below)
  - Then I get **MIDI Note On** message on channel `1`
    * And Pitch is `C1`
    * And Velocity is always `127` (maximum velocity)
+ When I touch any of the electrodes within `C2` key while still holding down `C1` key
  - Then I get **MIDI Control Change** message on channel `1`
    * And Controller is `4AH` (`74` decimal) labeled "Brightness" in MIDI manual
    * And Control Value is between `0` and `127` depending the closest electrode
to my fingerHp when the key is touched (see Mapping below) o ThenIgetMIDINoteOffmessageonChannel1
    * And Pitch is `C1`
    * And Velocity is always `0` (minimum velocity)
  - Then I get **MIDI Note On** message on channel `1`
    * And Pitch is `C2`
    * And Velocity is always `127` (maximum velocity)
+ When I release the key
  - Then I get MIDI Control Change message on channel `1`
    * And Controller is `4AH` (`74` decimal) labeled "Brightness" in MIDI manual
    * And Control Value is between `0` and `127` depending the closest electrode
to my fingerHp when the key is touched (see Mapping below)
  - Then I get **MIDI Note Off** message on channel `1`
    * And Pitch is `C2`
    * And Velocity is always `0` (minimum velocity)

The Key electrodes map onto MIDI brightness value as follows:

|Electrode|CC Value|
|-|-|
|`0`|`0`|
|`1`|`11`|
|`2`|`21`|
|`3`|`32`|
|`4`|`42`|
|`5`|`53`|
|`6`|`64`|
|`7`|`74`|
|`8`|`85`|
|`9`|`95`|
|`10`|`106`|
|`11`|`116`|
|`12`|`127`|

## Testing

[MIDI Monitor](hWps://www.snoize.com/MIDIMonitor/) can be used for testing this device once assembled to ensure it adheres to above requirements.

## Architecture

### Controller

The [ESP32-S3-DEVKITC-1-N8](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-DEVKITC-1-N8/15199021) was chosen as the controller because it can function like a Bluetooth MIDI device. A computer running sequencer software like Ableton Live can connect to this embedded MIDI controller, and pressing keys will then play notes in the program.

### Capacitive Touch

The [MPR121WR2](https://www.digikey.com/en/products/detail/nxp-usa-inc/mpr121qr2/2186527) was chosen as a capacitive touch sensor because it's simple to integrate onto the I2C bus.

The [TCA9548APWR](https://www.digikey.com/en/products/detail/texas-instruments/tca9548apwr/3615458) was chosen as the I2C bus multiplexer, because of the sheer number of capacitive touch electrodes in this project (`13` keys x `13` steps per key = `169` total electrodes). Two of these units can handle up to `180` electrodes in total.

### Power

A USB-C receptacle is used to program the board, with [AMS1117-3.3](https://www.digikey.com/en/products/detail/umw/ams1117-3-3/17635254) buck converter initially chosen to convert from USB `5V` to `3.3V` needed to power ESP32.

The circuitry from [Adafruit LiPo backpack](https://www.adafruit.com/product/2124) should be integrated directly onto the board to provide power from a LiPo battery after the device is programmed. This includes a **battery connector** and a **sliding power switch**.

The power switch will be **external** for this project - but a `JST-XH` 2-pin locking connector will be needed on the board to connect this external switch in a modular fashion.

## Schematic

A diagram with the initial design has been prepared: [schematic](./Schematic.pdf)
