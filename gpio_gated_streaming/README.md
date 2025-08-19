
GPIO-Gated Streaming
====================

This is the companion project for the application note
[GPIO-Gated Streaming](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/user_manual_content/app_notes/Example%20Gated%20Streaming.htm)
Note that the default parameters may not match the app note exactly.

This project will configure the device to stream data while a GPIO pin is held
in a particular state. By default, the following descriptors are streamed at 10 Hz:
* 0x80 Sensor Dataset
  * 0xD0 Event Source (this will match the trigger ID)
  * 0xD5 Reference Time (nanoseconds in the reference frame corresponding to the data)
  * 0xD6 Ref Time Delta (time between messages of this type, for this event)
  * 0xD3 GPS Time (if time synchronization is enabled, this is the external time frame, otherwise its the same as reference time but in gps format).
  * 0xD4 Delta GPS Time (time between messages of this type, for this event)
  * 0x07 Delta Theta (integrated angular rate over the delta time period)
  * 0x08 Delta Velocity (integrated acceleration over the delta time period)
* 0x82 Filter Dataset
  * 0xD0 Event Source (this will match the trigger ID)
  * 0xD5 Reference Time (this should match the sensor data ref time)
  * 0xD6 Ref Time Delta (also the same as the sensor data delta)
  * 0x05 Attitude Euler Angles (filtered attitude expressed as roll, pitch, yaw)


Operation
---------

### Initial Setup
* Create & open the connection
* Create the device interface
* Ensure the device is idle and communicates
* Clear any existing events and GPIO configurations.

### Application Configuration
These steps mirror those found in the application note.

* Configure the GPIO pin as GPIO INPUT
* Assign a GPIO trigger in edge mode to the pin
* Assign message actions with the descriptors to be transmitted
* Enable the trigger

### Application Run Loop
Once the configuration is complete, the demo will continuously display received
packets until ctrl+C is pressed.

Configuration Options
---------------------

These values are defined at the top of the main file for clarity and to permit easy modification.

| Option                   | Default        | Notes                                                                                                                                                                                           |
|--------------------------|----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SERIAL_PORT              | /dev/ttyACM0   | Set to match the port where your device is connected.                                                                                                                                           |
| SERIAL_BAUD              | 115200         | Set to match your device's baudrate. Ignored for USB.                                                                                                                                           |
| APPLY_DEFAULT_SETTINGS   | false          | If true, resets the device to factory default settings. This is useful to ensure consistent behavior in the demo, but may not be desired. Note that this resets the serial baud rate to 115200. |
| TRIGGER_ID               | 1              | Selects which event trigger slot to use.                                                                                                                                                        |
| GPIO_PIN                 | 1              | Set to the logical (as opposed to physical connector pin) GPIO pin number (e.g. 1-4).                                                                                                           |
| TRIGGER_ID               | 1              | Selects which event trigger slot to use.                                                                                                                                                        |
| PIN_EDGE                 | Rising         | Selects which direction will trigger data transmission. Can be The rising edge, falling edge, or both.                                                                                          |
| PIN_MODE                 | None           | Enables the built-in pullup or pulldown resistor. Use this if the input may be left floating in some cases.                                                                                     |
| ACTION_ID_SENSOR         | 1              | Event action ID for transmission of Sensor Data (0x80).                                                                                                                                         |
| ACTION_ID_FILTER         | 2              | Event action ID for transmission of Filter Data (0x82).                                                                                                                                         |
| FIELD_DESCRIPTORS_SENSOR |                | Lists the descriptors to be transmitted from the Sensor Data set.                                                                                                                               |
| FIELD_DESCRIPTORS_FILTER |                | Lists the descriptors to be transmitted from the Filter Data set.                                                                                                                               |
| DATA_RATE_SENSOR         | 10 Hz          | Controls the data rate for Sensor Data in Hz.                                                                                                                                                   |
| DATA_RATE_FILTER         | 10 Hz          | Controls the data rate for Filter Data in Hz.                                                                                                                                                   |
| CLEAR_REGULAR_STREAMING  | true           | If true, regular streaming will be disabled.                                                                                                                                                    |


Building and Running
--------------------

Follow the usual CMake steps described in the top-level readme.
The executable will be located at `build/gpio_gated_streaming/GpioGatedStreaming`.
