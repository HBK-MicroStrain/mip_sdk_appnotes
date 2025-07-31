
#include <microstrain/logging.hpp>
#include <microstrain/connections/serial/serial_connection.hpp>

#include <mip/mip_interface.hpp>
#include <mip/definitions/commands_base.hpp>
#include <mip/definitions/commands_3dm.hpp>
#include <mip/definitions/data_sensor.hpp>

#include <cstdio>
#include <csignal>
#include <thread>
#include <chrono>

#define _USE_MATH_DEFINES
#include <cmath>

////////////////////////////////////////////////////////////////////////////////
//
// Configuration
//
////////////////////////////////////////////////////////////////////////////////

// This is the port used to communicate with the device.
constexpr const char* const SERIAL_PORT = "/dev/ttyACM0";

// This should match the device's baud rate.
constexpr uint32_t SERIAL_BAUD = 115200;

//
// Trigger Behavior
//

// Trigger and action instance numbers.
constexpr uint8_t TRIGGER_ID = 1;

// Trigger data quantity - Euler angles (could also use filter euler attitude)
constexpr uint8_t TRIGGER_DESC_SET   = mip::data_sensor::DESCRIPTOR_SET;
constexpr uint8_t TRIGGER_FIELD_DESC = mip::data_sensor::DATA_COMP_EULER_ANGLES;
constexpr uint8_t TRIGGER_PARAM      = 1; // 1=roll, 2=pitch, 3=yaw for compensated euler angles.

// Threshold angles in radians.
// If high and low are swapped, the signal will be inverted.
constexpr float THRESHOLD_LOW  = -M_PI / 4;
constexpr float THRESHOLD_HIGH = +M_PI / 4;

//
// Action configuration
//

constexpr uint8_t ACTION_ID  = 1;

// Logical GPIO pin number. All pins 1-4 are supported.
constexpr uint8_t GPIO_PIN = 3;

// Behavior of the robot's E-Stop system:
// Is the e-stop input a dedicated digital input from the CV7, or is it shared?
//
// Some systems use open-drain control lines which may be shared with multiple devices.
// By default, the line is pulled high through a resistor, and then any single device can pull it low to signal a fault.
// Do not use push-pull mode with such systems, as it may interfere with other devices attempting to signal a fault.

// Set this to true if the e-stop signal is open-drain.
// For dedicated signalling (CV7 is the only thing driving the signal), use push-pull mode by
// setting this to false.
constexpr bool ESTOP_OPEN_DRAIN = false;

// This should be the polarity of the e-stop signal in the STOP state.
constexpr bool ESTOP_POLARITY = true;

// If you want to try open-drain mode, but don't have a resistor handy, you may enable
// the built-in pull-up resistor. This is only valid for open-drain mode.
constexpr bool ESTOP_OD_PULLUP  = false;

// If you want the e-stop signal to stay asserted even if the trigger condition is cleared,
// set this to true. Otherwise, the CV7 will de-assert the e-stop signal when the robot
// is turned back upright.
constexpr bool ESTOP_ONE_SHOT = false;


////////////////////////////////////////////////////////////////////////////////

//
// Basic sanity checks on parameters.
//
static_assert(GPIO_PIN >= 1 && GPIO_PIN <= 4, "GPIO pin must be in the range [1-4].");

static_assert(!ESTOP_OPEN_DRAIN || ESTOP_POLARITY==false, "Polarity must be active low when using open drain mode.");
static_assert(!ESTOP_OD_PULLUP || ESTOP_OPEN_DRAIN, "Pull-up resistor can only be used in open-drain mode.");

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
// BEGIN COMMON SETUP
////////////////////////////////////////////////////////////////////////////////

int demo(mip::Interface& device);

void log_callback(void*, const microstrain_log_level level, const char* fmt, va_list args)
{
    // Print log messages to stdout.
    std::printf("[%s]: ", microstrain_logging_level_name(level));
    std::vprintf(fmt, args);
}

int main()
{
    //
    // DEVICE SETUP
    //

    // Initialize logging so messages can be printed.
    // Change the log level if you want to see more- or less-detailed messages.
    MICROSTRAIN_LOG_INIT(log_callback, MICROSTRAIN_LOG_LEVEL_INFO, nullptr);

    // Create the connection. This does not open the port yet.
    microstrain::connections::SerialConnection connection(SERIAL_PORT, SERIAL_BAUD);

    // Open the port now.
    if(!connection.connect())
    {
        MICROSTRAIN_LOG_FATAL("Could not open serial port '%s'\n", connection.interfaceName());
        return 1;
    }


    const uint32_t PARSE_TIMEOUT = 100;  // Time for one mip packet to be parsed before timing out.
    const uint32_t REPLY_TIMEOUT = 1000; // Max turnaround time between sending a command and expecting the reply.

    // Create the device interface using the specified connection.
    mip::Interface device(&connection, PARSE_TIMEOUT, REPLY_TIMEOUT);

    // Set the device to idle and test communications.
    MICROSTRAIN_LOG_DEBUG("Testing communication with the device\n");
    mip::CmdResult result = mip::commands_base::setIdle(device);
    if(result == mip::CmdResult::STATUS_TIMEDOUT)
    {
        // If the device is streaming at a high rate,
        // the reply might get lost. Try again.
        result = mip::commands_base::setIdle(device);
    }
    // Check if result was NACK or failed the second time also.
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to set the device to idle: %d %s\n", result.value, result.name());
        return 1;
    }

    //
    // Clear existing configuration to avoid conflicts or confusing behavior.
    //

    // This is technically optional, but ensures the example runs as expected by starting
    // with a clean slate.


    // Reset to default settings.
    // This will lose communications if the baud rate is not the default 115200 value.
    // Instead, reset just the relevant configurations below.

    //result = mip::commands_3dm::defaultDeviceSettings(device);
    //if(!result)
    //{
    //    MICROSTRAIN_LOG_FATAL("Failed to reset to default settings: %d %s\n", result.value, result.name());
    //    return 1;
    //}

    return demo(device);
}

////////////////////////////////////////////////////////////////////////////////
// END COMMON SETUP
////////////////////////////////////////////////////////////////////////////////




int demo(mip::Interface& device)
{
    mip::CmdResult result;

    //
    // Clear existing configuration to avoid conflicts or confusing behavior.
    //

    // Clear all event triggers.
    result = mip::commands_3dm::defaultEventTrigger(device, 0);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to reset event triggers: %d %s\n", result.value, result.name());
        return 1;
    }

    // Clear all event actions.
    result = mip::commands_3dm::defaultEventAction(device, 0);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to reset event actions: %d %s\n", result.value, result.name());
        return 1;
    }

    //
    // Gpio Setup
    //

    // Configure the pin to the General Purpose Output feature.
    // This is effective immediately.

    mip::commands_3dm::GpioConfig gpio;
    gpio.function = mip::FunctionSelector::WRITE;
    gpio.pin = GPIO_PIN;
    gpio.feature = mip::commands_3dm::GpioConfig::Feature::GPIO;

    // Initialize the pin state to the "OK" state (i.e. not signaling a fault).
    if(ESTOP_POLARITY == true)
        gpio.behavior = mip::commands_3dm::GpioConfig::Behavior::GPIO_OUTPUT_LOW;
    else
        gpio.behavior = mip::commands_3dm::GpioConfig::Behavior::GPIO_OUTPUT_HIGH;

    // Set open-drain mode if desired.
    gpio.pin_mode.openDrain(ESTOP_OPEN_DRAIN);
    // Set the pull-up resistor if desired (this will cause a NACK if true and open drain is false).
    gpio.pin_mode.pullup(ESTOP_OD_PULLUP);

    result = device.runCommand(gpio);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure GPIO pin: %d %s\n", result.value, result.name());
        MICROSTRAIN_LOG_FATAL("The attempted configuration was:\n");
        MICROSTRAIN_LOG_FATAL("  Function: %u\n", gpio.function);
        MICROSTRAIN_LOG_FATAL("  Pin:      %u\n", gpio.pin);
        MICROSTRAIN_LOG_FATAL("  Feature:  %u\n", gpio.feature);
        MICROSTRAIN_LOG_FATAL("  Behavior: %u\n", gpio.behavior);
        MICROSTRAIN_LOG_FATAL("  Mode:     0x%02X\n", gpio.pin_mode.value);
        return 1;
    }

    //
    // Action setup
    //

    mip::commands_3dm::EventAction action;
    action.function = mip::FunctionSelector::WRITE;
    action.instance = ACTION_ID;
    action.trigger  = TRIGGER_ID;  // this must match the trigger configured below.
    action.type     = mip::commands_3dm::EventAction::Type::GPIO;
    action.parameters.gpio.pin = GPIO_PIN;  // This must match the pin configured above.

    // Set the output control so that an active trigger condition
    // correctly activates the e-stop system.
    if(ESTOP_POLARITY == true)  // E-Stop is triggered by a HIGH state
    {
        if(ESTOP_ONE_SHOT)  // Don't auto-clear the e-stop signal.
            action.parameters.gpio.mode = mip::commands_3dm::EventAction::GpioParams::Mode::ONESHOT_HIGH;
        else
            action.parameters.gpio.mode = mip::commands_3dm::EventAction::GpioParams::Mode::ACTIVE_HIGH;
    }
    else  // E-Stop is triggered by a LOW state (always the case for open-drain mode)
    {
        if(ESTOP_ONE_SHOT)  // Don't auto-clear the e-stop signal.
            action.parameters.gpio.mode = mip::commands_3dm::EventAction::GpioParams::Mode::ONESHOT_LOW;
        else
            action.parameters.gpio.mode = mip::commands_3dm::EventAction::GpioParams::Mode::ACTIVE_LOW;
    }

    result = device.runCommand(action);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure action: %d %s\n", result.value, result.name());
        MICROSTRAIN_LOG_FATAL("The attempted configuration was:\n");
        MICROSTRAIN_LOG_FATAL("  Function: %u\n", action.function);
        MICROSTRAIN_LOG_FATAL("  Instance: %u\n", action.instance);
        MICROSTRAIN_LOG_FATAL("  Trigger:  %u\n", action.trigger);
        MICROSTRAIN_LOG_FATAL("  Type:     %u\n", action.type);
        MICROSTRAIN_LOG_FATAL("  Pin:      %u\n", action.parameters.gpio.pin);
        MICROSTRAIN_LOG_FATAL("  Mode:     %u\n", action.parameters.gpio.mode);
        return 1;
    }

    //
    // Trigger setup
    //

    mip::commands_3dm::EventTrigger trigger;
    trigger.function = mip::FunctionSelector::WRITE;
    trigger.instance = TRIGGER_ID;
    trigger.type     = mip::commands_3dm::EventTrigger::Type::THRESHOLD;
    trigger.parameters.threshold.desc_set   = TRIGGER_DESC_SET;
    trigger.parameters.threshold.field_desc = TRIGGER_FIELD_DESC;
    trigger.parameters.threshold.param_id   = TRIGGER_PARAM;
    trigger.parameters.threshold.type       = mip::commands_3dm::EventTrigger::ThresholdParams::Type::WINDOW;

    // Normally, window triggers are active when the value is between the low and high thresholds.
    // This is the opposite of what's needed for an e-stop system where the robot should stop if the
    // value goes out of bounds.
    // If the low and high thresholds are swapped, the condition is flipped and the trigger will
    // activate when the value lies outside the specified range.
    // The same effect could be achieved by switching the GPIO action polarity instead. However,
    // it's more intuitive to have the trigger state represent the e-stop condition directly.
    // This way the e-stop is signaled when the trigger is active, and the GPIO action polarity
    // matches the hardware behavior.
    trigger.parameters.threshold.low_thres  = THRESHOLD_HIGH;
    trigger.parameters.threshold.high_thres = THRESHOLD_LOW;

    result = device.runCommand(trigger);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure trigger: %d %s\n", result.value, result.name());
        MICROSTRAIN_LOG_FATAL("The attempted configuration was:\n");
        MICROSTRAIN_LOG_FATAL("  Function:   %u\n", trigger.function);
        MICROSTRAIN_LOG_FATAL("  Instance:   %u\n", trigger.instance);
        MICROSTRAIN_LOG_FATAL("  Type:       %u\n", trigger.type);
        MICROSTRAIN_LOG_FATAL("  Desc set:   0x%02X\n", trigger.parameters.threshold.desc_set);
        MICROSTRAIN_LOG_FATAL("  Field desc: 0x%02X\n", trigger.parameters.threshold.field_desc);
        MICROSTRAIN_LOG_FATAL("  Parameter:  %u\n", trigger.parameters.threshold.param_id);
        MICROSTRAIN_LOG_FATAL("  Comp. type: %u\n", trigger.parameters.threshold.type);
        MICROSTRAIN_LOG_FATAL("  Low thres:  %f\n", trigger.parameters.threshold.low_thres);
        MICROSTRAIN_LOG_FATAL("  High thres: %f\n", trigger.parameters.threshold.high_thres);
        return 1;
    }

    //
    // Enable trigger
    //

    result = mip::commands_3dm::writeEventControl(device, TRIGGER_ID, mip::commands_3dm::EventControl::Mode::ENABLED);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to enable trigger: %d %s\n", result.value, result.name());
        return 1;
    }

    MICROSTRAIN_LOG_INFO("Configuration complete!\n");

    //
    // Main loop
    //

    if(ESTOP_ONE_SHOT)
    {
        while(true)
        {
            // When the user presses enter, restore the GPIO pin state.
            // It would also be possible to use another trigger to do this, say when the
            // robot is restored to within 5 degrees of vertical.
            // In that case, use another trigger+action pair configured to reset the e-stop
            // instead of asserting it. Note that more than one GPIO action can be set to the
            // same pin if they are both one-shot types (two non-oneshot actions on the same pin
            // will fight each other, resulting in glitches on the pin).

            std::printf("Press [Enter] to reset the e-stop signal: ");
            std::getc(stdin);

            // Reset the GPIO pin to the inactive state.
            result = mip::commands_3dm::writeGpioState(device, GPIO_PIN, !ESTOP_POLARITY);
            if(!result)
            {
                MICROSTRAIN_LOG_ERROR("Failed to set gpio state: %d %s\n", result.value, result.name());
            }
        }
    }

    return 0;
}
