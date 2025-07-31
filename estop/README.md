
Threshold Trigger Demo
======================

This is the companion project for the application note
[Stopping a Robot if it Tips Over](https://s3.amazonaws.com/files.microstrain.com/CV7+Online/user_manual_content/app_notes/Example%20Emergency%20Stop.htm)

This project will configure the device to assert a GPIO pin when a threshold on a given data quantity is reached.

Operation
---------

### Initial Setup
* Create & open the connection
* Create the device interface
* Ensure the device is idle and communicates
* Clear any existing events and GPIO configurations


### Application Configuration
These steps mirror those found in the application note.

* Configure a GPIO for output mode
  * The selected pin is configured for push-pull output or open-drain output.
* Assign a GPIO action to control the state
  * The GPIO action will control the previously-configured pin based on the state
    of a trigger.
  * The GPIO action will use either continuous or one-shot mode.
* Configure a threshold trigger in window mode
  * The trigger will continuously check if a value is within set limits.
* Enable the trigger


### Application Run Loop

If the configuration is set to one-shot mode, the demo will allow the
user to reset the e-stop signal by pressing the Enter key.


Configuration Options
---------------------

These values are defined at the top of the main file for clarity and to permit
easy modification.


| Option             | Default                       | Notes                                                                                                                                                             |
|--------------------|-------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SERIAL_PORT        | /dev/ttyACM0                  | Set to match the port where your device is connected.                                                                                                             |
| SERIAL_BAUD        | 115200                        | Set to match your device's baudrate. Ignored for USB.                                                                                                             |
| TRIGGER_ID         | 1                             | Selects which event trigger slot to use.                                                                                                                          |
| ACTION_ID          | 1                             | Selects which event action slot to use.                                                                                                                           |
| GPIO_PIN           | 1                             |                                                                                                                                                                   |
| TRIGGER_DESC_SET   | 0x80 Sensor Data              | Controls which descriptor set contains the monitored data quantity. Can be 0x80 Sensor Data, 0x82 Filter Data, 0xA0 System Data. GNSS Data sets are not suported. |
| TRIGGER_FIELD_DESC | 0x0C Compensated Euler Angles | Controls which field descriptor (within the above descriptor set) contains the monitored data quantity.                                                           |
| TRIGGER_PARAM      | 1    Roll                     | Controls which parameter within the MIP field contains the monitored data quantity. For Comp. Euler Angles, 1=Roll, 2=Pitch, 3=Yaw.                               |
| THRESHOLD_LOW      | -Pi/4 (-45 degrees)           | The estop will activate when the value is below the low threshold.                                                                                                |
| THRESHOLD_HIGH     | +Pi/4 (+45 degrees)           | The estop will activate when the value is above the high threshold.                                                                                               |
| ESTOP_OPEN_DRAIN   | false                         | If true, the e-stop signal will not be pulled high.                                                                                                               |
| ESTOP_POLARITY     | true (signal high to stop)    | Determines whether an active e-stop signal is indicated by the high state (true) or low state (false).                                                            |
| ESTOP_OD_PULLUP    | false                         | When using open-drain mode, an internal pullup resistor can be enabled.                                                                                           |
| ESTOP_ONE_SHOT     | false                         | When true, the e-stop signal is not automatically cleared when the value goes back into the valid range.                                                          |


Building and Running
--------------------

Follow the usual CMake steps described in the top-level readme.
The executable will be located at `build/estop/EStopDemo`.
