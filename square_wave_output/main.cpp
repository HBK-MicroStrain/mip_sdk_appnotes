
#include <microstrain/logging.hpp>
#include <microstrain/connections/serial/serial_connection.hpp>

#include <mip/mip_interface.hpp>
#include <mip/definitions/commands_base.hpp>
#include <mip/definitions/commands_3dm.hpp>
#include <mip/definitions/data_sensor.hpp>
#include <mip/definitions/data_filter.hpp>
#include <mip/definitions/data_system.hpp>
#include <mip/definitions/data_gnss.hpp>
#include <mip/definitions/data_shared.hpp>

#include <cstdio>
#include <csignal>
#include <thread>
#include <chrono>


////////////////////////////////////////////////////////////////////////////////
//
// Configuration
//
////////////////////////////////////////////////////////////////////////////////

// This is the port used to communicate with the device.
static constexpr const char* const SERIAL_PORT = "/dev/ttyACM0";

// This should match the device's baud rate.
static constexpr uint32_t    SERIAL_BAUD = 115200;

// The square wave will be output on this logical GPIO pin number.
static constexpr uint8_t LOGICAL_GPIO_PIN = 1;

// Trigger instance number.
static constexpr uint8_t TRIGGER_ID = 1;

// Action instance number.
static constexpr uint8_t ACTION_ID  = 1;

// Square wave frequency in Hz.
static constexpr double FREQUENCY = 60.0;

// Square wave duty cycle in the range 0 to 1.
// E.g. 50% --> 0.5 or 10% --> 0.1
static constexpr double DUTY_CYCLE = 0.5;


// The square wave will be synchronized with the timestamp from this descriptor set.
//static const uint8_t TIMESTAMP_DESCRIPTOR_SET = mip::data_sensor::DESCRIPTOR_SET;
//static const uint8_t TIMESTAMP_DESCRIPTOR_SET = mip::data_filter::DESCRIPTOR_SET;
static constexpr uint8_t TIMESTAMP_DESCRIPTOR_SET = mip::data_system::DESCRIPTOR_SET;

// Select one of these options:
// - Reference Time [default]: Internal reference time in nanoseconds.
// - External Time:            External synchronized time (e.g. GNSS) in nanoseconds.
// - GPS Time:                 Same as external time, but with time of week in seconds. Requires changing threshold (see below).
static constexpr uint8_t TIMESTAMP_FIELD_DESCRIPTOR = mip::data_shared::DATA_REFERENCE_TIME;
//static const uint8_t TIMESTAMP_FIELD_DESCRIPTOR = mip::data_shared::DATA_EXTERNAL_TIME;
//static const uint8_t TIMESTAMP_FIELD_DESCRIPTOR = mip::data_shared::DATA_GPS_TIME;

// Selects which parameter in the corresponding timestamp field is used for comparison.
// For reference and external timestamps, the first parameter is nanoseconds.
// For GPS time, Time of Week (TOW) is the first parameter.
static constexpr uint8_t TIMESTAMP_PARAMETER = 1;

// Units for the timestamp field. This is only used for the math in this section.
// You may ignore this value if setting TIMESTAMP_INTERVAL yourself (in which case,
// you may also need to disable some of the validation checks if they complain).
static constexpr double TIMESTAMP_UNITS = 1.0e-9;  // Nanoseconds, for Internal or External Timestamps.
//static constexpr double TIMESTAMP_UNITS = 1.0;     // Seconds, for GPS Time.

// This value is what actually controls the period of the square wave.
// The units are determined by the specific timestamp field chosen.
// By default, and for explanatory purposes, it is computed from the FREQUENCY and
// TIMESTAMP_UNITS values, but you may override that logic if you wish to set it directly.
// If you do set this parameter manually, FREQUENCY will no longer be used.
// You may still need to set TIMESTAMP_UNITS so that the validation assertions don't complain.
static constexpr double TIMESTAMP_INTERVAL  = 1.0 / (TIMESTAMP_UNITS * FREQUENCY);

// This value is what actually controls the duty cycle. The pin will be HIGH
// when the time is between the start of the interval and this value (relative to the start of the interval).
// Duty cycle = threshold / interval.
// If you set this parameter manually, DUTY_CYCLE will no longer be used.
static constexpr double TIMESTAMP_THRESHOLD = DUTY_CYCLE * TIMESTAMP_INTERVAL;

////////////////////////////////////////////////////////////////////////////////

//
// Basic sanity checks on parameters.
//

static_assert(LOGICAL_GPIO_PIN >= 1 && LOGICAL_GPIO_PIN <= 4, "LOGICAL_GPIO_PIN must be within [1,4] for CV7.");
static_assert(TRIGGER_ID >= 1 && TRIGGER_ID <= 12, "TRIGGER_ID must be within [1,12] for CV7.");
static_assert(ACTION_ID >= 1 && ACTION_ID <= 12, "ACTION_ID must be within [1,12] for CV7.");
static_assert(FREQUENCY <= 1000.0, "Maximum frequency is 1000 Hz for CV7.");
static_assert(DUTY_CYCLE >= 0 && DUTY_CYCLE <= 1.0, "DUTY_CYCLE must be between 0 and 1.");
static_assert(TIMESTAMP_INTERVAL*TIMESTAMP_UNITS > 0.001, "TIMESTAMP_INTERVAL must be at least 1 ms.");
static_assert(TIMESTAMP_INTERVAL*TIMESTAMP_UNITS < 1000, "TIMESTAMP_INTERVAL is > 1000 s, this is probably not what you wanted. Check the units.");
static_assert(TIMESTAMP_THRESHOLD*TIMESTAMP_UNITS > 0.001, "TIMESTAMP_THRESHOLD must be at least 1 ms.");
static_assert(TIMESTAMP_THRESHOLD < TIMESTAMP_INTERVAL, "TIMESTAMP_THRESHOLD must be less than TIMESTAMP_INTERVAL.");


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


bool showTriggerStatus(mip::Interface& device, bool fullDisplay=true);

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

    // Reset all GPIO pins to default UNUSED feature.
    result = mip::commands_3dm::defaultGpioConfig(device, 0);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to reset all GPIO pins: %d %s\n", result.value, result.name());
        return 1;
    }

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

    // Show that the trigger is not enabled nor active yet.
    // This is just for demonstration purposes and is completely optional.
    MICROSTRAIN_LOG_INFO("Trigger status after setting factory defaults:\n");
    showTriggerStatus(device);

    //
    // Gpio Setup
    //

    // Configure the pin to the General Purpose Output feature.
    // This will enable the pin's output driver, taking it out of the high-Z state.
    // Effective immediately.

    // The pin is configured as OUTPUT_LOW by default. This makes the pin start in the LOW state,
    // which may help avoid potential false triggers during setup in downstream logic.
    // It may also be configured to OUTPUT_HIGH instead, if that is desired (for example, if you
    // need inverted logic).

    // The pin mode is NONE by default. Open-drain mode is supported, and you may also enable
    // the built-in pullup resistor in that case.

    result = mip::commands_3dm::writeGpioConfig(
        /* device   */ device,
        /* pin      */ LOGICAL_GPIO_PIN,
        /* Feature  */ mip::commands_3dm::GpioConfig::Feature::GPIO,
        /* Behavior */ mip::commands_3dm::GpioConfig::Behavior::GPIO_OUTPUT_LOW, // Could also use GPIO_OUTPUT_HIGH.
        /* Mode     */ mip::commands_3dm::GpioConfig::PinMode::NONE  // OPEN_DRAIN or (OPEN_DRAIN | PULLUP) are also valid.
    );

    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure gpio pin %u: %d %s\n", LOGICAL_GPIO_PIN, result.value, result.name());
        return 1;
    }

    //
    // Event Action Setup
    //

    // Configures an event action to set the pin HIGH while the trigger is activated, and LOW otherwise.

    // We use the command struct instead of the write function for clarity.
    // mip::commands_3dm::writeEventAction, etc. would work fine too.

    // The gpio mode parameter may be ACTIVE_HIGH or ACTIVE_LOW for this example.
    // ACTIVE_LOW will invert the square wave.
    // The TOGGLE and ONESHOT modes will not produce a square wave.

    mip::commands_3dm::EventAction action;
    action.function = mip::FunctionSelector::WRITE;  // Apply new configuration.
    action.instance = ACTION_ID;   // Can be any supported ID.
    action.trigger  = TRIGGER_ID;  // This must match the trigger setup below.
    action.type     = mip::commands_3dm::EventAction::Type::GPIO;
    action.parameters.gpio.pin  = LOGICAL_GPIO_PIN;  // Must match the pin configured above.
    action.parameters.gpio.mode = mip::commands_3dm::EventAction::GpioParams::Mode::ACTIVE_HIGH;

    result = device.runCommand(action);

    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure gpio action in slot %u: %d %s\n", ACTION_ID, result.value, result.name());
        return 1;
    }

    //
    // Event Trigger Setup
    //

    // Configure a threshold trigger in INTERVAL mode to monitor the timestamp.

    mip::commands_3dm::EventTrigger trigger;
    trigger.function = mip::FunctionSelector::WRITE;
    trigger.instance = TRIGGER_ID;  // This must match the action's trigger ID.
    trigger.type     = mip::commands_3dm::EventTrigger::Type::THRESHOLD;
    trigger.parameters.threshold.desc_set   = TIMESTAMP_DESCRIPTOR_SET;
    trigger.parameters.threshold.field_desc = TIMESTAMP_FIELD_DESCRIPTOR;
    trigger.parameters.threshold.param_id   = TIMESTAMP_PARAMETER;
    trigger.parameters.threshold.type       = mip::commands_3dm::EventTrigger::ThresholdParams::Type::INTERVAL;
    trigger.parameters.threshold.int_thres  = TIMESTAMP_THRESHOLD;
    trigger.parameters.threshold.interval   = TIMESTAMP_INTERVAL;

    result = device.runCommand(trigger);

    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure threshold trigger in slot %u: %d %s\n", TRIGGER_ID, result.value, result.name());
        return 1;
    }

    // Show that the trigger is not enabled nor active yet.
    // This is just for demonstration purposes and is completely optional.
    MICROSTRAIN_LOG_INFO("Trigger status before trigger is enabled:\n");
    showTriggerStatus(device);

    //
    // Enable The Trigger
    //

    // By default, triggers are not enabled after configuration.
    // This command will enable the trigger and start the square wave output.
    // For debugging, you could also put it into TEST mode, which would
    // force it to the active state. This is useful to help determine if the
    // corresponding action is working properly.

    result = mip::commands_3dm::writeEventControl(
        /* device   */ device,
        /* instance */ TRIGGER_ID,
        /* mode     */ mip::commands_3dm::EventControl::Mode::ENABLED
    );

    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to enable the trigger in slot %u: %d %s\n", TRIGGER_ID, result.value, result.name());
        return 1;
    }

    //
    // DONE
    //

    // Show that the trigger is not enabled nor active yet.
    // This is just for demonstration purposes and is completely optional.
    MICROSTRAIN_LOG_INFO("Trigger status after trigger is enabled:\n");
    showTriggerStatus(device);

    MICROSTRAIN_LOG_INFO("Configuration complete!\n");

    //
    // Extra: Monitor the state of the trigger for demonstration/debugging purposes.
    //

    // If log printing is disabled, don't bother running the display loop.
    if(microstrain_logging_level() < MICROSTRAIN_LOG_LEVEL_INFO)
        return 0;

    // Setup a signal handler to catch ctrl+C.
    static volatile std::sig_atomic_t stop = false;
    std::signal(SIGTERM, [](int){ stop=true; });

    if(FREQUENCY <= 5.0)
    {
        while(!stop)
        {
            bool ok = showTriggerStatus(device);
            if(!ok)
                return 1;

            // 10 Hz updates
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    else // High frequency
    {
        MICROSTRAIN_LOG_INFO("Trigger status stream (+ = active, - = inactive):\n");

        auto sampleTime = std::chrono::system_clock::now();

        // Run for 10 seconds, printing a + or - every 10 milliseconds depending on the trigger state.
        // Each line has 100 samples.
        for(unsigned int i=0; i<10'000; i++)
        {
            // Sleep until the next scheduled sample time.
            // Using sleep_until, rather than sleep_for, allows for more accurate scheduling
            // because it accounts for delays elsewhere in the loop, such as waiting for the
            // trigger status command response.
            std::this_thread::sleep_until(sampleTime);
            sampleTime += std::chrono::milliseconds(1);

            showTriggerStatus(device, false);

            // Newline every 100 samples.
            if(i % 100 == 99)
                putchar('\n');
        }
    }

    return 0;
}

bool showTriggerStatus(mip::Interface& device, bool fullDisplay)
{
    // Request trigger status for a list of triggers.
    // In this example, we have only one trigger.
    mip::commands_3dm::GetEventTriggerStatus statusCmd;
    statusCmd.requested_instances[0] = TRIGGER_ID;
    statusCmd.requested_count = 1;
    mip::commands_3dm::GetEventTriggerStatus::Response statusRsp;

    // Run the command and get the response data.
    mip::CmdResult result = device.runCommand(statusCmd, statusRsp);

    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to get trigger status: %d %s\n", TRIGGER_ID, result.value, result.name());
        return false;
    }

    // Basic sanity check that it returned the expected number of trigger statuses.
    if(statusRsp.count != 1)
    {
        MICROSTRAIN_LOG_FATAL("Unexpected count %u for trigger status response\n", statusRsp.count);
        return false;
    }

    if(fullDisplay)
    {
        // The trigger status is a bitfield of 3 flags.
        // Assign either an 'X' or blank, depending on the flags, for printing.
        const char active  = statusRsp.triggers[0].status.active()  ? 'X' : ' ';
        const char enabled = statusRsp.triggers[0].status.enabled() ? 'X' : ' ';
        const char testing = statusRsp.triggers[0].status.test()    ? 'X' : ' ';

        // Display all 3 status parameters
        MICROSTRAIN_LOG_INFO("Trigger %u status:  [%c] Active  [%c] Enabled  [%c] Testing\n", statusCmd.requested_instances[0], active, enabled, testing);
    }
    else
    {
        const char active  = statusRsp.triggers[0].status.active()  ? '+' : '-';

        // Just print an X or space with no newline, for high update speed.
        putchar(active);
    }

    return true;
}