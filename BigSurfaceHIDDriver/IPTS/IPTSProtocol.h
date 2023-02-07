//
//  IPTSProtocol.h
//  SurfaceTouchScreen
//
//  Created by Xavier on 2022/6/7.
//  Copyright © 2022 Xia Shangning. All rights reserved.
//

#ifndef IPTSProtocol_h
#define IPTSProtocol_h

#include "IPTSKenerlUserShared.h"

/*
 * Queries the device for vendor specific information.
 *
 * The command must not contain any payload.
 * The response will contain IPTSGetDeviceInfoResponse as payload.
 */
#define IPTS_CMD_GET_DEVICE_INFO 0x00000001
#define IPTS_RSP_GET_DEVICE_INFO 0x80000001

/*
 * Sets the mode that IPTS will operate in.
 *
 * The command must contain IPTSSetModeCommand as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_SET_MODE 0x00000002
#define IPTS_RSP_SET_MODE 0x80000002

/*
 * Configures the memory buffers that the ME will use
 * for passing data to the host.
 *
 * The command must contain IPTSSetMemoryWindowCommand as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_SET_MEM_WINDOW 0x00000003
#define IPTS_RSP_SET_MEM_WINDOW 0x80000003

/*
 * Stops the data flow from the device to the driver.
 *
 * The command must contain IPTSQuiesceIOCommand as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_QUIESCE_IO 0x00000004
#define IPTS_RSP_QUIESCE_IO 0x80000004

/*
 * Signals that the host is ready to receive data from the ME.
 *
 * The command must not contain any payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_READY_FOR_DATA 0x00000005
#define IPTS_RSP_READY_FOR_DATA 0x80000005

/*
 * Signals that a buffer can be refilled from the ME.
 *
 * The command must contain IPTSFeedbackCommand as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_FEEDBACK 0x00000006
#define IPTS_RSP_FEEDBACK 0x80000006

/*
 * Resets the data flow from the ME to the hosts and
 * clears the buffers that were set with SET_MEM_WINDOW.
 *
 * The command must not contain any payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_CLEAR_MEM_WINDOW 0x00000007
#define IPTS_RSP_CLEAR_MEM_WINDOW 0x80000007

/*
 * Instructs the ME to reset the touch sensor.
 *
 * The command must contain struct IPTSResetSensorCommand as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_RESET_SENSOR 0x0000000B
#define IPTS_RSP_RESET_SENSOR 0x8000000B

/*
 * Get HID report descriptor.
 *
 * The command must contain struct IPTSGetReportDescriptor.
 * The response will contain 8 bytes zeros padding following HID report descriptor.
 */
#define IPTS_CMD_GET_REPORT_DESC 0x0000000F
#define IPTS_RSP_GET_REPORT_DESC 0x8000000F

/**
 * enum IPTSStatus - Possible status codes returned by IPTS commands.
 * @IPTSCommandSuccess:             Operation completed successfully.
 * @IPTSCommandInvalidParams:       Command contained a payload with invalid parameters.
 * @IPTSCommandAccessDenied:        ME could not validate buffer addresses supplied by host.
 * @IPTSCommandPayloadSizeError:    Command contains an invalid payload.
 * @IPTSCommandNotReady:            Buffer addresses have not been set.
 * @IPTSCommandRequestOutstanding:  There is an outstanding command of the same type.
 *                                       The host must wait for a response before sending another
 *                                       command of the same type.
 * @IPTSCommandNoSensorFound:       No sensor could be found. Either no sensor is connected, it
 *                                       has not been initialized yet, or the system is improperly
 *                                       configured.
 * @IPTSCommandOutOfMemory:         Not enough free memory for requested operation.
 * @IPTSCommandUnexpectedError:     An unexpected error occurred.
 * @IPTSCommandSensorDisabled:      The sensor has been disabled and must be reinitialized.
 * @IPTSCommandCompatCheckFail:     Compatibility revision check between sensor and ME failed.
 *                                       The host can ignore this error and attempt to continue.
 * @IPTSCommandExpectedReset:       The sensor went through a reset initiated by ME or host.
 * @IPTSCommandUnexpectedReset:     The sensor went through an unexpected reset.
 * @IPTSCommandResetFailed:         Requested sensor reset failed to complete.
 * @IPTSCommandTimeout:             The operation timed out.
 * @IPTSCommandTestModeFail:        Test mode pattern did not match expected values.
 * @IPTSCommandSensorFatalFail:     The sensor reported a fatal error during reset sequence.
 *                                       Further progress is not possible.
 * @IPTSCommandSensorFail:          The sensor reported a fatal error during reset sequence.
 *                                       The host can attempt to continue.
 * @IPTSCommandInvalidDeviceCapability: The device reported invalid capabilities.
 * @IPTSCommandQuiesceIOInProgress:  Command cannot be completed until Quiesce IO is done.
 */
enum IPTSCommandStatus {
    IPTSCommandSuccess              = 0x00,
    IPTSCommandInvalidParams        = 0x01,
    IPTSCommandAccessDenied         = 0x02,
    IPTSCommandPayloadSizeError     = 0x03,
    IPTSCommandNotReady             = 0x04,
    IPTSCommandRequestOutstanding   = 0x05,
    IPTSCommandNoSensorFound        = 0x06,
    IPTSCommandOutOfMemory          = 0x07,
    IPTSCommandUnexpectedError      = 0x08,
    IPTSCommandSensorDisabled       = 0x09,
    IPTSCommandCompatCheckFail      = 0x0A,
    IPTSCommandExpectedReset        = 0x0B,
    IPTSCommandUnexpectedReset      = 0x0C,
    IPTSCommandResetFailed          = 0x0D,
    IPTSCommandTimeout              = 0x0E,
    IPTSCommandTestModeFail         = 0x0F,
    IPTSCommandSensorFatalFail      = 0x10,
    IPTSCommandSensorFail           = 0x11,
    IPTSCommandInvalidDeviceCapability = 0x12,
    IPTSCommandQuiesceIOInProgress  = 0x13,
};

/*
 * The special buffer ID that is used for direct host2me feedback.
 */
#define IPTS_TX_BUFFER IPTS_BUFFER_NUM

struct PACKED IPTSCommand {
    UInt32 code;
    UInt8  payload[320];
};

/**
 * enum IPTSTouchMode - Configures what data the device produces and how its sent
 * @IPTSModeEvent:      The device will send an event once a buffer was filled. DO NOT USE, buggy
 *                        Older device will return singletouch data in this mode.
 * @IPTSModeDoorbell:   The device will notify the driver by incrementing the doorbell value.
 *                        Older devices will return multitouch data in this mode.
 */
enum IPTSTouchMode {
    IPTSModeEvent    = 0x00,
    IPTSModeDoorbell = 0x01,
};

/**
 * struct IPTSSetModeCommand - Payload for the SET_MODE command.
 * @mode: The mode that IPTS should operate in.
 */
struct PACKED IPTSSetModeCommand {
    IPTSTouchMode mode;
    UInt8 reserved[12];
};

#define IPTS_WORKQUEUE_SIZE     8192
#define IPTS_WORKQUEUE_ITEM_SIZE 16

/**
 * struct IPTSSetMemoryWindowCommand - Payload for the SET_MEM_WINDOW command.
 * @data_buffer_addr_lower:     Lower 32 bits of the data buffer addresses.
 * @data_buffer_addr_upper:     Upper 32 bits of the data buffer addresses.
 * @workqueue_addr_lower:       Lower 32 bits of the workqueue buffer address.
 * @workqueue_addr_upper:       Upper 32 bits of the workqueue buffer address.
 * @doorbell_addr_lower:        Lower 32 bits of the doorbell buffer address.
 * @doorbell_addr_upper:        Upper 32 bits of the doorbell buffer address.
 * @feedback_buffer_addr_lower: Lower 32 bits of the feedback buffer addresses.
 * @feedback_buffer_addr_upper: Upper 32 bits of the feedback buffer addresses.
 * @host2me_addr_lower:         Lower 32 bits of the host2me buffer address.
 * @host2me_addr_upper:         Upper 32 bits of the host2me buffer address.
 * @workqueue_item_size:        Magic value. (IPTS_WORKQUEUE_ITEM_SIZE)
 * @workqueue_size:             Magic value. (IPTS_WORKQUEUE_SIZE)
 *
 * The data buffers are buffers that get filled with touch data by the ME.
 * The doorbell buffer is a UInt32 that gets incremented by the ME once a data
 * buffer has been filled with new data.
 *
 * The other buffers are required for using GuC submission with binary
 * firmware. Since support for GuC submission has been dropped from i915,
 * they are not used anymore, but they need to be allocated and passed,
 * otherwise the hardware will refuse to start.
 */
struct PACKED IPTSSetMemoryWindowCommand {
    UInt32 data_buffer_addr_lower[IPTS_BUFFER_NUM];
    UInt32 data_buffer_addr_upper[IPTS_BUFFER_NUM];
    UInt32 workqueue_addr_lower;
    UInt32 workqueue_addr_upper;
    UInt32 doorbell_addr_lower;
    UInt32 doorbell_addr_upper;
    UInt32 feedback_buffer_addr_lower[IPTS_BUFFER_NUM];
    UInt32 feedback_buffer_addr_upper[IPTS_BUFFER_NUM];
    UInt32 host2me_addr_lower;
    UInt32 host2me_addr_upper;
    UInt32 host2me_size;
    UInt8  reserved1;
    UInt8  workqueue_item_size;
    UInt16 workqueue_size;
    UInt8  reserved[32];
};

/**
 * struct IPTSQuiesceIOCommand - Payload for the QUIESCE_IO command.
 */
struct PACKED IPTSQuiesceIOCommand {
    UInt8 reserved[12];
};

/**
 * struct IPTSFeedbackCommand - Payload for the FEEDBACK command.
 * @buffer: The buffer that the ME should refill.
 */
struct PACKED IPTSFeedbackCommand {
    UInt32 buffer;
    UInt8  reserved[12];
};

/**
 * enum IPTSFeedbackCommandType - Commands that can be executed on the sensor through feedback.
 */
enum IPTSFeedbackCommandType {
    IPTSFeedbackCommandTypeNone         = 0,
    IPTSFeedbackCommandTypeSoftReset    = 1,
    IPTSFeedbackCommandTypeGotoArmed    = 2,
    IPTSFeedbackCommandTypeGotoSensing  = 3,
    IPTSFeedbackCommandTypeGotoSleep    = 4,
    IPTSFeedbackCommandTypeGotoDoze     = 5,
    IPTSFeedbackCommandTypeHardReset    = 6,
};

/**
 * enum IPTSFeedbackDataType - Describes the data that a feedback buffer contains.
 * @IPTSFeedbackDataTypeVendor:        The buffer contains vendor specific feedback.
 * @IPTSFeedbackDataTypeSetFeatures:  The buffer contains a HID set features command.
 * @IPTSFeedbackDataTypeGetFeatures:  The buffer contains a HID get features command.
 * @IPTSFeedbackDataTypeOutputReport: The buffer contains a HID output report.
 * @IPTSFeedbackDataTypeStoreData:    The buffer contains calibration data for the sensor.
 */
enum IPTSFeedbackDataType {
    IPTSFeedbackDataTypeVendor = 0,
    IPTSFeedbackDataTypeSetFeatures = 1,
    IPTSFeedbackDataTypeGetFeatures = 2,
    IPTSFeedbackDataTypeOutputReport = 3,
    IPTSFeedbackDataTypeStoreData = 4,
};

/**
 * struct IPTSFeedbackHeader - The contents of an IPTS feedback buffer.
 * @cmd_type: A command that should be executed on the sensor.
 * @size: The size of the payload to be written.
 * @buffer: The ID of the buffer that contains this feedback data.
 * @protocol: The protocol version of the EDS.
 * @data_type: The type of payload that the buffer contains.
 * @spi_offset: The offset at which to write the payload data.
 * @payload: Payload for the feedback command, or 0 if no payload is sent.
 */
struct PACKED IPTSFeedbackHeader {
    IPTSFeedbackCommandType cmd_type;
    UInt32 size;
    UInt32 buffer;
    UInt32 protocol;
    IPTSFeedbackDataType data_type;
    UInt32 spi_offset;
    UInt8 reserved[40];
    UInt8 payload[];
};

/**
 * enum IPTSResetType - Possible ways of resetting the touch sensor
 * @IPTSResetTypeHard: Perform hardware reset using GPIO pin.
 * @IPTSResetTypeSoft: Perform software reset using SPI interface.
 */
enum IPTSResetType {
    IPTSResetTypeHard = 0,
    IPTSResetTypeSoft = 1,
};

/**
 * struct IPTSResetSensorCommand - Payload for the RESET_SENSOR command.
 * @type: What type of reset should be performed.
 */
struct PACKED IPTSResetSensorCommand {
    IPTSResetType type;
    UInt8 reserved[4];
};

#define IPTS_REPORT_DESC_PADDING    8

/**
 * struct IPTSGetReportDescriptorCommand - Payload for the GET_DESCRIPTOR command.
 * @addr_lower:  The lower 32 bits of the descriptor buffer address.
 * @addr_upper:  The upper 32 bits of the descriptor buffer address.
 * @padding_len: A magic value. Must be 8.
 */
struct PACKED IPTSGetReportDescriptorCommand {
    UInt32 addr_lower;
    UInt32 addr_upper;
    UInt32 padding_len; // MUST be IPTS_REPORT_DESC_PADDING
    UInt8 reserved[12];
};

/**
 * struct IPTSResponse - Data returned from the device in response to a command.
 * @code:    The command that this response answers.
 * @status:  The return code of the command.
 * @payload: The data that was produced by the command.
 */
struct PACKED IPTSResponse {
    UInt32 code;
    IPTSCommandStatus status;
    UInt8 payload[80];
};

/**
 * struct IPTSGetDeviceInfoResponse - Vendor information of the IPTS device.
 * @vendor_id:     Vendor ID of this device.
 * @device_id:     Product ID of this device.
 * @hw_rev:        Hardware revision of this device.
 * @fw_rev:        Firmware revision of this device.
 * @data_size:     Requested size for a data buffer.
 * @feedback_size: Requested size for a feedback buffer.
 * @mode:          Mode that the device currently operates in.
 * @max_contacts:  Maximum amount of concurrent touches the sensor can process.
 */
struct PACKED IPTSGetDeviceInfoResponse {
    UInt16 vendor_id;
    UInt16 device_id;
    UInt32 hw_rev;
    UInt32 fw_rev;
    UInt32 data_size;
    UInt32 feedback_size;
    IPTSTouchMode mode;
    UInt8 max_contacts;
    UInt8 reserved1[3];
    UInt8 sensor_min_eds;
    UInt8 sensor_maj_eds;
    UInt8 me_min_eds;
    UInt8 me_maj_eds;
    UInt8 intf_eds;
    UInt8 reserved2[11];
};

/**
 * struct IPTSFeedbackResponse - Payload for the FEEDBACK response.
 * @buffer: The buffer that has received feedback.
 */
struct PACKED IPTSFeedbackResponse {
    UInt32 buffer;
};

/**
 * enum IPTSDataType - Defines what type of data a buffer contains.
 * @IPTSDataTypeFrame:        Raw data frame.
 * @IPTSDataTypeError:        Error data.
 * @IPTSDataTypeVendor:       Vendor specific data.
 * @IPTSDataTypeHID:          A HID report.
 * @IPTSDataTypeGetFeatures:  The response to a GET_FEATURES command.
 */
enum IPTSDataType {
    IPTSDataTypeFrame   = 0x00,
    IPTSDataTypeError   = 0x01,
    IPTSDataTypeVendor  = 0x02,
    IPTSDataTypeHID     = 0x03,
    IPTSDataTypeGetFeatures = 0x04,
    IPTSDataTypeDescriptor  = 0x05,
};

/**
 * struct IPTSDataHeader - Header that is prefixed to the data in a data buffer.
 * @type: What data the buffer contains.
 * @size: How much data the buffer contains.
 * @buffer: Which buffer the data is in.
 */
struct PACKED IPTSDataHeader {
    IPTSDataType type;
    UInt32 size;
    UInt32 buffer;
    UInt8  reserved[52];
    UInt8  data[];
};

#endif /* IPTSProtocol_h */
