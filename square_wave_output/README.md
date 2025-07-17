
Synchronized Square Wave Output
===============================

This is the companion project for the application note
[Syncrhonized Square Wave Output](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/user_manual_content/app_notes/Example%20Square%20Wave%20Output.htm)


This project will configure the device to output a square wave on one of the GPIO pins using the event system.
The square wave will be synchronized with either reference or external time.


The GPIO action will control the state of the pin based on the trigger status.
The threshold trigger will monitor one of the available timestamp data quantities and continuously
compare it against a recurring interval. When the time is between the start of the interval and the threshold value,
the trigger will be activated, and when the time is between the threshold and the end of the interval, the trigger will
be deactivated. This sequence then repeats for the next interval.

For example, if the interval is set to 10 seconds, and the threshold to 4 seconds, the trigger will
be active when the time is between 0-4, 10-14, 20-24, etc. seconds, and inactive between 4-9, 14-19, 24-29, etc. seconds.
The pin will be high during the active times and low otherwise, thus producing a square wave with 40% duty cycle.


Operation
---------

### Initial Setup
* Create & open the connection
* Create the device interface
* Ensure the device is idle and communicates
* Clear any existing events and GPIO configurations.

### Application Configuration
These steps mirror those found in the application note.

* Configure the GPIO pin for GPIO_OUTPUT mode
* Assign a GPIO action to control the pin
* Assign a threshold trigger in interval mode
* Enable the trigger

### Application Run Loop
For demonstration & debug purposes, this project will continuously monitor the trigger status.
This gives a visual representation of the behavior directly in the terminal, without any additional
hardware besides the 3DM device itself.

If the configured frequency is less than 5 Hz, a verbose output mode is used. It prints
lines like this every 100 ms (10 Hz):

`[INFO ]: Trigger 1 status:  [ ] Active  [X] Enabled  [ ] Testing`

Each `[ ]` will have an `X` if the corresponding flag is set.

If the configured frequency is more than 5 Hz, a high-speed output is used. It prints
one `+` or `-` every millisecond, depending on the trigger's active state. Output will
look similar to this:

```
--------++++++++---------++++++++--------+++++++++--------++++++++---------++++++++--------+++++++++
--------++++++++---------++++++++--------+++++++++--------++++++++---------++++++++--------+++++++++
--------++++++++---------++++++++--------+++++++++--------++++++++---------++++++++--------+++++++++
```

This graphs the trigger's state over time, giving a rough estimate of the period and duty cycle.
Each character roughly represents a sample roughly every millisecond. The example above is for a
60 Hz square wave, so there are groups of 8 `+` and 8 `-`. Every third group has an extra one
since 60 Hz is a period of 16.667 ms, which isn't an integral number of ms.

(!) CAUTION: This is only intended for demo and debugging purposes. You should not rely on it nor assume
the timing is accurate from this display alone. In particular, it is sensitive to your PC's performance.
Trigger status cannot be streamed, only polled, so it is subject to significant jitter.
Always check the hardware with a suitable instrument like an oscilloscope or frequency counter.


Configuration Options
---------------------

These values are defined at the top of the main file for clarity and to permit easy modification.

| Option                     | Default                           | Notes                                                                                                                                                                            |
|----------------------------|-----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SERIAL_PORT                | /dev/ttyACM0                      | Set to match the port where your device is connected.                                                                                                                            |
| SERIAL_BAUD                | 115200                            | Set to match your device's baudrate. Ignored for USB.                                                                                                                            |
| LOGICAL_GPIO_PIN           | 1                                 | Set to the logical (as opposed to physical connector pin) GPIO pin number (e.g. 1-4).                                                                                            |
| TRIGGER_ID                 | 1                                 | Selects which event trigger slot to use.                                                                                                                                         |
| ACTION_ID                  | 1                                 | Selects which event action slot to use.                                                                                                                                          |
| FREQUENCY                  | 60                                | Sets the square wave frequency in Hz.                                                                                                                                            |
| DUTY_CYCLE                 | 0.5                               | Sets the square wave duty cycle. 50% --> 0.5, 10% --> 0.1, etc. The minimum period is 1ms (shorter periods will only work intermittently).                                       |
| TIMESTAMP_DESCRIPTOR_SET   | 0x80 Sensor Data                  | Controls which descriptor set the time is sourced from. Can be 0x80 Sensor Data, 0x82 Filter Data, 0xA0 System Data. GNSS Data sets are not supported (use system time instead). |
| TIMESTAMP_FIELD_DESCRIPTOR | 0xD5 Internal Reference           | Controls the time reference frame is used for the square wave. Can be 0xD5 Shared Reference Time, 0xD7 Shared External Time, or 0xD3 Shared GPS Time (same as External Time).    |
| TIMESTAMP_PARAMETER        | 1 (Nanoseconds since startup)     | Which field from the timestamp contains the time. For all 3 fields mentioned above, parameter 1 holds the actual time quantity.                                                  |
| TIMESTAMP_UNITS            | 1.0e-9 (Nanoseconds)              | Used in computing the threshold and interval from the frequency and duty cycle. Expressed as a ratio of UNIT to seconds.                                                         |
| TIMESTAMP_INTERVAL         | 1.0 / FREQUENCY / TIMESTAMP_UNITS | [ADVANCED] Period of the square wave in units of the selected time value. For Reference and External time, this is nanoseconds. For GPS TOW, this is seconds.                    |
| TIMESTAMP_THRESHOLD        | DUTY_CYCLE * TIMESTAMP_INTERVAL   | [ADVANCED] Threshold of the square wave in the same units as the interval. The pin will be HIGH when time is between the start of the interval and this (relative) threshold.    |


Valid Time Sources
------------------

### Descriptor Sets

* 0x80 Sensor Data - `mip::data_sensor::DESCRIPTOR_SET`
* 0x82 Filter Data - `mip::data_filter::DESCRIPTOR_SET`
* 0xA0 System Data - `mip::data_system::DESCRIPTOR_SET`

Note: Threshold triggers do not support GNSS data quantities on CV7.

### Shared Timestamp Quantities

See [Synchronization Overview](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/user_manual_content/app_notes/Synchronization%20Overview.htm)
for a description of the differences between these quantities.

* [0xD5 Reference Timestamp](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/external_content/dcp/Data/0xff/data/0xd5.htm)
* [0xD7 External Timestamp](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/external_content/dcp/Data/0xff/data/0xd7.htm)
* [0xD3 GPS Timestamp](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/external_content/dcp/Data/0xff/data/0xd3.htm)
  Note: For CV7, this is the same as the External Timestamp, just formatted as GPS Time.
  If you select this option, the units must be changed from nanoseconds to seconds.

