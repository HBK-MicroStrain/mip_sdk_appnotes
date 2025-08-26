
#include <microstrain/logging.hpp>
#include <microstrain/connections/serial/serial_connection.hpp>
#include <microstrain/span.hpp>

#include <mip/mip_interface.hpp>
#include <mip/definitions/commands_base.hpp>
#include <mip/definitions/commands_3dm.hpp>
#include <mip/definitions/data_sensor.hpp>
#include <mip/definitions/data_filter.hpp>
#include <mip/definitions/data_shared.hpp>

#include <csignal>


////////////////////////////////////////////////////////////////////////////////
//
// Configuration
//
////////////////////////////////////////////////////////////////////////////////

//
// Device Connection
//

// This is the port used to communicate with the device.
constexpr const char* const SERIAL_PORT = "/dev/ttyACM0";

// This should match the device's UART baud rate, if using a serial connection.
// You do not need to change this for USB connections.
constexpr uint32_t SERIAL_BAUD = 115200;

// Optionally clear the device configuration before starting.
// This could be useful if there are conflicting settings or unexpected
// behavior. The settings related to this demo are always cleared regardless.
// Note that this will reset the UART baudrate(s) to 115200.
constexpr bool APPLY_DEFAULT_SETTINGS = false;

//
// Event Trigger
//

// Trigger instance number.
constexpr uint8_t TRIGGER_ID = 1;

// Logical GPIO pin number. All pins 1-4 are supported.
constexpr uint8_t GPIO_PIN = 1;

// Stream when the pin is high if true, or low if false.
constexpr bool ACTIVE_HIGH = true;

// Pin mode - can be NONE, PULLUP, or PULLDOWN.
constexpr uint8_t PIN_MODE = mip::commands_3dm::GpioConfig::PinMode::NONE;

//
// Event Actions
//

// Action IDs (one per streamed descriptor set)
constexpr uint8_t ACTION_ID_SENSOR = 1;
constexpr uint8_t ACTION_ID_FILTER = 2;

// Output data rates for each descriptor set
constexpr uint16_t DATA_RATE_SENSOR = 10;
constexpr uint16_t DATA_RATE_FILTER = 10;

// Sensor data descriptors
constexpr uint8_t FIELD_DESCRIPTORS_SENSOR[] = {
    mip::data_shared::DATA_EVENT_SOURCE,
    mip::data_shared::DATA_REFERENCE_TIME,
    mip::data_shared::DATA_REF_TIME_DELTA,
    mip::data_shared::DATA_GPS_TIME,
    mip::data_shared::DATA_DELTA_TIME,
    mip::data_sensor::DATA_DELTA_THETA,
    mip::data_sensor::DATA_DELTA_VELOCITY,
};

// Filter data descriptors
constexpr uint8_t FIELD_DESCRIPTORS_FILTER[] = {
    mip::data_shared::DATA_EVENT_SOURCE,
    mip::data_shared::DATA_REFERENCE_TIME,
    mip::data_filter::DATA_ATT_EULER_ANGLES,
};

// Convert to spans for easy reference below.
constexpr microstrain::Span<const uint8_t> FIELD_DESCRIPTORS_SPAN_SENSOR( FIELD_DESCRIPTORS_SENSOR );
constexpr microstrain::Span<const uint8_t> FIELD_DESCRIPTORS_SPAN_FILTER( FIELD_DESCRIPTORS_FILTER );

//
// Misc
//

// If this is true, regular streaming will be disabled so that only the
// configured event data will be transmitted.
// If this is set to false, you may set the message format manually before
// running this demo. Both regular streaming data and event data will be
// transmitted in this case.
constexpr bool CLEAR_REGULAR_STREAMING = true;

////////////////////////////////////////////////////////////////////////////////

//
// Basic sanity checks on parameters.
//

static_assert(SERIAL_BAUD == 115200 || !APPLY_DEFAULT_SETTINGS, "APPLY_DEFAULT_SETTINGS will reset the baudrate to 115200 and lose the connection.");

constexpr size_t MAX_MESSAGE_DESCRIPTORS = sizeof(mip::commands_3dm::EventAction::MessageParams::descriptors)/sizeof(uint8_t);

static_assert(GPIO_PIN >= 1 && GPIO_PIN <= 4, "Pin must be in the range [1-4].");

static_assert(FIELD_DESCRIPTORS_SPAN_SENSOR.size() <= MAX_MESSAGE_DESCRIPTORS);
static_assert(FIELD_DESCRIPTORS_SPAN_FILTER.size() <= MAX_MESSAGE_DESCRIPTORS);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
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

    if(APPLY_DEFAULT_SETTINGS)
    {
        // Reset to default settings.
        // This will lose communications if the baud rate is not the default 115200 value.
        // Instead, reset just the relevant configurations below.
        // This ensures the example runs as expected by starting with a clean slate.

        result = mip::commands_3dm::defaultDeviceSettings(device);
        if(!result)
        {
            MICROSTRAIN_LOG_FATAL("Failed to reset to default settings: %d %s\n", result.value, result.name());
            return 1;
        }
    }

    return demo(device);
}

////////////////////////////////////////////////////////////////////////////////
// END COMMON SETUP
////////////////////////////////////////////////////////////////////////////////

void print_packet(void*, const mip::PacketView& packet, mip::Timestamp);

mip::CmdResult configure_message_action(
    mip::Interface& device,
    uint8_t action_id,
    uint8_t desc_set,
    microstrain::Span<const uint8_t> field_descriptors,
    uint16_t output_rate
);


int demo(mip::Interface& device)
{
    mip::CmdResult result;

    //
    // Clear existing configuration to avoid conflicts or confusing behavior.
    //

    // This is only needed if the global configuration has not been cleared already.
    if(!APPLY_DEFAULT_SETTINGS)
    {
        if(CLEAR_REGULAR_STREAMING)
        {
            // Reset all message formats to the default empty state.
            result = mip::commands_3dm::defaultMessageFormat(device, 0x00);
            if(!result)
            {
                MICROSTRAIN_LOG_FATAL("Failed to reset message format: %d %s\n", result.value, result.name());
                return 1;
            }
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
    }

    //
    // Gpio Setup
    //

    // Configure the pin to the General Purpose Output feature.
    // This is effective immediately.

    mip::commands_3dm::GpioConfig gpio;
    gpio.function = mip::FunctionSelector::WRITE;
    gpio.pin      = GPIO_PIN;
    gpio.feature  = mip::commands_3dm::GpioConfig::Feature::GPIO;
    gpio.behavior = mip::commands_3dm::GpioConfig::Behavior::GPIO_INPUT;
    gpio.pin_mode = PIN_MODE;

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
    // Trigger setup
    //

    mip::commands_3dm::EventTrigger trigger;
    trigger.function = mip::FunctionSelector::WRITE;
    trigger.instance = TRIGGER_ID;
    trigger.type     = mip::commands_3dm::EventTrigger::Type::GPIO;

    trigger.parameters.gpio.pin = GPIO_PIN;  // This must match the pin configured above.

    // Use one of the "WHILE_" modes, so that the trigger state continuously reflects the pin state.
    // Note: EDGE mode would require the pin to be configured for the timestamp feature.
    if(ACTIVE_HIGH)
        trigger.parameters.gpio.mode = mip::commands_3dm::EventTrigger::GpioParams::Mode::WHILE_HIGH;
    else
        trigger.parameters.gpio.mode = mip::commands_3dm::EventTrigger::GpioParams::Mode::WHILE_LOW;

    result = device.runCommand(trigger);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure trigger: %d %s\n", result.value, result.name());
        MICROSTRAIN_LOG_FATAL("The attempted configuration was:\n");
        MICROSTRAIN_LOG_FATAL("  Function:   %u\n", trigger.function);
        MICROSTRAIN_LOG_FATAL("  Instance:   %u\n", trigger.instance);
        MICROSTRAIN_LOG_FATAL("  Type:       %u\n", trigger.type);
        MICROSTRAIN_LOG_FATAL("  Pin:        %u\n", trigger.parameters.gpio.pin);
        MICROSTRAIN_LOG_FATAL("  Mode:       %u\n", trigger.parameters.gpio.mode);
        return 1;
    }

    //
    // Action setup
    //

    // Configure one action per descriptor set.
    bool ok = true;
    ok = ok && configure_message_action(device, ACTION_ID_SENSOR, mip::data_sensor::DESCRIPTOR_SET, FIELD_DESCRIPTORS_SPAN_SENSOR, DATA_RATE_SENSOR);
    ok = ok && configure_message_action(device, ACTION_ID_FILTER, mip::data_filter::DESCRIPTOR_SET, FIELD_DESCRIPTORS_SPAN_FILTER, DATA_RATE_FILTER);
    if(!ok)
        return 1;

    //
    // Enable trigger
    //

    result = mip::commands_3dm::writeEventControl(device, TRIGGER_ID, mip::commands_3dm::EventControl::Mode::ENABLED);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to enable trigger: %d %s\n", result.value, result.name());
        return 1;
    }

    //
    // Resume streaming so event data can be transmitted.
    //

    // Event data obeys the global streaming/idle flag.
    result = mip::commands_base::resume(device);
    if(!result)
    {
        MICROSTRAIN_LOG_ERROR("Failed to resume data streaming: %d %s\n", result.value, result.name());
    }

    //
    // Main loop
    //

    MICROSTRAIN_LOG_INFO("Configuration complete!\n");

    // Setup a signal handler to catch ctrl+C.
    static volatile std::sig_atomic_t stop = false;
    std::signal(SIGTERM, [](int){ stop=true; });

    if(microstrain_logging_level() >= MICROSTRAIN_LOG_LEVEL_INFO)
    {
        mip::DispatchHandler handler;
        device.registerPacketCallback<&print_packet>(handler, mip::Dispatcher::ANY_DATA_SET, false, nullptr);

        while(!stop)
        {
            device.update(100);
        }
    }

    return 0;
}


void print_packet(void*, const mip::PacketView& packet, mip::Timestamp)
{
    MICROSTRAIN_LOG_INFO("Packet: DS=0x%02X\n", packet.descriptorSet());

    // Track the event id.
    // Initialize to 0xFF, which is not a valid trigger ID.
    // 0 is reserved for non-event data.
    uint8_t event_source = 0xFF;

    for(mip::FieldView field : packet)
    {
        switch(field.fieldDescriptor())
        {
        case mip::data_shared::EventSource::FIELD_DESCRIPTOR:
            if( mip::data_shared::EventSource value; field.extract(value) )
            {
                MICROSTRAIN_LOG_INFO("  Event Trigger ID: %d\n", value.trigger_id);
                event_source = value.trigger_id;
            }
            break;

        case mip::data_shared::ReferenceTimestamp::FIELD_DESCRIPTOR:
            if( mip::data_shared::ReferenceTimestamp value; field.extract(value) )
            {
                MICROSTRAIN_LOG_INFO("  Event Ref Time:   %zu ns\n", value.nanoseconds);
            }
            break;

        case mip::data_shared::GpsTimestamp::FIELD_DESCRIPTOR:
            if( mip::data_shared::GpsTimestamp value; field.extract(value) )
            {
                MICROSTRAIN_LOG_INFO("  Event GPS Time:   Week %u, TOW %f s\n", value.week_number, value.tow);
            }
            break;
        }
    }

    // If the packet has an EventSource field with trigger ID 0, then it must be
    // from regular/non-event streaming.
    if(event_source == 0)
    {
        MICROSTRAIN_LOG_INFO("  Non-event packet (event source 0)\n");
    }

    // The packet might not have an event source field in two situations:
    // 1. EventSource has not been added in the message action descriptors, or
    // 2. The packet is from regular/non-event streaming and EventSource is not
    //    listed in the message format corresponding to the packet's descriptor
    //    set.
    else if(event_source == 0xFF)
    {
        MICROSTRAIN_LOG_INFO("  No event source field\n");
    }
}


mip::CmdResult configure_message_action(
    mip::Interface& device,
    uint8_t action_id,
    uint8_t desc_set,
    microstrain::Span<const uint8_t> field_descriptors,
    uint16_t output_rate
)
{
    mip::CmdResult result;

    // First, get the base rate of the descriptor set so the decimation can be computed.
    uint16_t base_rate = 0;
    result = mip::commands_3dm::getBaseRate(device, desc_set, &base_rate);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to get the base rate for descriptor set 0x%02X: %d %s\n", desc_set, result.value, result.name());
        return result;
    }
    if(base_rate == 0)
    {
        MICROSTRAIN_LOG_FATAL("Base rate of 0 is unexpected for descriptor set 0x%02X.\n", desc_set);
        return mip::CmdResult::STATUS_ERROR;
    }

    // Solve for decimation: output_rate = base_rate / decimation
    // ==> decimation = base_rate / output_rate
    uint16_t decimation = base_rate / output_rate;

    mip::commands_3dm::EventAction action;

    action.function = mip::FunctionSelector::WRITE;
    action.instance = action_id;  // Must be unique for each message action.
    action.trigger  = TRIGGER_ID;  // This must match the trigger configured above.
    action.type     = mip::commands_3dm::EventAction::Type::MESSAGE;

    action.parameters.message.desc_set   = desc_set;
    action.parameters.message.decimation = decimation;
    action.parameters.message.num_fields = field_descriptors.size();

    // Copy descriptors to the command structure.
    std::copy(field_descriptors.begin(), field_descriptors.end(), &action.parameters.message.descriptors[0]);

    result = device.runCommand(action);
    if(!result)
    {
        MICROSTRAIN_LOG_FATAL("Failed to configure action: %d %s\n", result.value, result.name());
        MICROSTRAIN_LOG_FATAL("The attempted configuration was:\n");
        MICROSTRAIN_LOG_FATAL("  Function:   %u\n", action.function);
        MICROSTRAIN_LOG_FATAL("  Instance:   %u\n", action.instance);
        MICROSTRAIN_LOG_FATAL("  Trigger:    %u\n", action.trigger);
        MICROSTRAIN_LOG_FATAL("  Type:       %u\n", action.type);
        MICROSTRAIN_LOG_FATAL("  Desc Set:   0x%02X\n", action.parameters.message.desc_set);
        MICROSTRAIN_LOG_FATAL("  Num Desc:   %u\n", action.parameters.message.num_fields);
        MICROSTRAIN_LOG_FATAL("  Decimation: %u\n", action.parameters.message.decimation);
        return result;
    }

    return result;
}
