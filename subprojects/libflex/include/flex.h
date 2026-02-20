/// \file flex.h FlexSense Board specific definition
// Copyright (c) 2024, Myriota Pty Ltd, All Rights Reserved
// SPDX-License-Identifier: BSD-3-Clause-Attribution
//
// This file is licensed under the BSD with attribution  (the "License"); you
// may not use these files except in compliance with the License.
//
// You may obtain a copy of the License here:
// LICENSE-BSD-3-Clause-Attribution.txt and at
// https://spdx.org/licenses/BSD-3-Clause-Attribution.html
//
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FLEX_H
#define FLEX_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "flex_diag_conf.h"
#include "flex_errors.h"

/// @defgroup Version_Info Version Information
/// @brief Get the Flex Library version and set your own application version
///
/// Retrieve the Flex Library Version information and implement \p FLEX_AppVersionString to view
/// your app version over Bluetooth using the FlexAssist mobile application
/// ([Android](https://play.google.com/store/apps/details?id=com.myriota.binzel&hl=en) /
/// [iOS](https://apps.apple.com/us/app/flexassist/id6474694371?uo=2)).
///@{

/// Get the Flex Library Version in "Major.Minor.Patch" format.
/// \return the Flex Library Version.
const char *FLEX_VersionString(void);
/// Get the Flex Library Major Version information.
/// \return the Flex Library Version Major value.
uint16_t FLEX_VersionMajor(void);
/// Get the Flex Library Minor Version information.
/// \return the Flex Library Version Minor value.
uint16_t FLEX_VersionMinor(void);
/// Get the Flex Library Patch Version information.
/// \return the Flex Library Version Patch value.
uint16_t FLEX_VersionPatch(void);

/// Implement this function to provide a version for your application
/// which will then be visible over BLE
/// \return the version formatted as a string
const char *FLEX_AppVersionString(void);

///@}

/// @defgroup Analog_Input Analog Input
/// @brief Configure and control the FlexSense Analog Input Interface
///
/// The FlexSense Analog input can be configured to read Current in micro-amps or Voltage in
/// milli-volts.
///
/// \warning To maximise the battery life of your FlexSense, we recommend initialising and
/// de-initialising the Analog Input interface each time the job using it is called. This will
/// reduce the idle power usage of your FlexSense.
///@{

/// Analog Input Modes.
typedef enum {
  FLEX_ANALOG_IN_VOLTAGE,  ///< use when measuring Voltage at the Analog Input
  FLEX_ANALOG_IN_CURRENT   ///< use when measuring Current at the Analog Input
} FLEX_AnalogInputMode;

/// Initialise the Analog Input in Voltage or Current mode.
///
/// \note De-initialise the Analog Input before a job is completed to preserve device power.
///
/// \param[in] InputMode the operation mode selected from \p FLEX_AnalogInputMode.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise i/o expander device.
/// \retval -FLEX_ERROR_POWER_OUT: failed to enable power to the analog input interface.
int FLEX_AnalogInputInit(const FLEX_AnalogInputMode InputMode);
/// De-initialise the Analog Input interface.
///
/// \note De-initialise the Analog Input before a job is completed to preserve device power.
///
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to de-initialise i/o expander device.
/// \retval -FLEX_ERROR_POWER_OUT: failed to disable power to the analog input interface.
int FLEX_AnalogInputDeinit(void);
/// Get the Current reading in micro-amps from the Analog Input interface.
///
/// \note The Analog Input interface needs to be initialised in Current mode before performing a
/// Current reading.
///
/// \param[out] pMicroAmps the Current reading in uA.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EINVAL: pMicroAmps is a NULL parameter
/// \retval -FLEX_ERROR_EOPNOTSUPP: Analog input set to incorrect state (i.e. trying
///         to read current when init to voltage mode)
/// \retval -FLEX_ERROR_READ_FAIL: Error reading from ADC device
int FLEX_AnalogInputReadCurrent(uint32_t *const pMicroAmps);
/// Get the Voltage reading in milli-volts from the Analog Input interface.
///
/// \note The Analog Input interface needs to be initialised in Voltage mode before performing a
/// Voltage reading.
///
/// \param[out] pMilliVolts the Voltage reading in mV.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EINVAL: pMilliVolts is a NULL parameter
/// \retval -FLEX_ERROR_EOPNOTSUPP: Analog input set to incorrect state (i.e. trying
///         to read Voltage when init to current mode)
/// \retval -FLEX_ERROR_READ_FAIL: Error reading from ADC device
int FLEX_AnalogInputReadVoltage(uint32_t *const pMilliVolts);

///@}

/// @defgroup Power_Out Power Out Control
/// @brief Configure and control the FlexSense Power Out interface
///
/// The FlexSense Power Out can supply DC power at 5V, 12V or 24V. The maximum Current supported on
/// this interface is 25mA
///
/// \warning To maximise the battery life of your FlexSense, we recommend initialising and
/// de-initialising the Power Out interface each time the job using it is called. This will
/// reduce the idle power usage of your FlexSense.
///@{

/// Power Output Voltage Options.
typedef enum {
  FLEX_POWER_OUT_24V,  ///< set output voltage to 24V
  FLEX_POWER_OUT_12V,  ///< set output voltage to 12V
  FLEX_POWER_OUT_5V    ///< set output voltage to 5V
} FLEX_PowerOut;

/// Enable and sets the Output Voltage.
///
/// \note De-initialise the Power Output before a job is completed to preserve device power.
///
/// \param[in] Voltage the required output voltage selected from \p FLEX_PowerOut.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EALREADY: already initialised
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_EOPNOTSUPP: tried to set invalid power out value
/// \retval -FLEX_ERROR_PWM: failed to setup or configure pwm device
int FLEX_PowerOutInit(const FLEX_PowerOut Voltage);
/// Disables the Output Voltage
///
/// \note De-initialise the Power Output before a job is completed to preserve device power.
///
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to configure expander device
int FLEX_PowerOutDeinit(void);

///@}

/// @defgroup LED_Control LED Control
/// @brief Control the LED on the FlexSense board.
///
/// The FlexSense offers a Blue and Green LED that can be individually controlled.
///
/// \warning To maximise the battery life of your FlexSense, we recommend limiting how often and how
/// long the LED is on.
///@{

/// LED States.
typedef enum {
  FLEX_LED_ON,   ///< turn LED On
  FLEX_LED_OFF,  ///< turn LED Off
} FLEX_LEDState;

/// Change the state of the Green LED.
///
/// \note To maximise the battery life of your FlexSense, we recommend limiting how often and how
/// long the LED is on.
///
/// \param[in] LEDState the required LED state selected from \p FLEX_LEDState.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
int FLEX_LEDGreenStateSet(const FLEX_LEDState LEDState);

/// Change the state of the Blue LED.
///
/// \note To maximise the battery life of your FlexSense, we recommend limiting how often and how
/// long the LED is on.
///
/// \param[in] LEDState the required LED state selected from \p FLEX_LEDState.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
int FLEX_LEDBlueStateSet(const FLEX_LEDState LEDState);

///@}

/// @defgroup Handler Modify Actions
/// @brief These actions are inputs to FlexSense APIs where handlers are modified
///@{

/// Handler Modify Actions
typedef enum {
  FLEX_HANDLER_MODIFY_ADD,     ///< action to add a handler
  FLEX_HANDLER_MODIFY_REMOVE,  ///< action to remove a handler
} FLEX_HandlerModifyAction;

///@}

/// @defgroup Ext_Digital_IO External Digital I/O
/// @brief Configure and control the FlexSense External Digital I/O interface
///@{

/// Available Digital I/O Pins.
typedef enum {
  FLEX_EXT_DIGITAL_IO_1,  ///< Digital I/O Pin 1
  FLEX_EXT_DIGITAL_IO_2   ///< Digital I/O Pin 2
} FLEX_DigitalIOPin;

/// Digital I/O level Options.
typedef enum {
  FLEX_EXT_DIGITAL_IO_LOW = 0,  ///< Digital I/O level Low
  FLEX_EXT_DIGITAL_IO_HIGH      ///< Digital I/O level High
} FLEX_DigitalIOLevel;

/// Set the level of an external Digital I/O pin to High or Low.
/// \param[in] PinNum the external Digital I/O pin number.
/// \param[in] Level the required Digital I/O level for the selected pin.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_EINVAL: Invalid level parameter
/// \retval -FLEX_ERROR_I2C: Error with i2c device when setting IO
int FLEX_ExtDigitalIOSet(const FLEX_DigitalIOPin PinNum, const FLEX_DigitalIOLevel Level);
/// Get the level of an External Digital I/O pin.
/// \param[in] PinNum the external Digital I/O pin number.
/// \return FLEX_DigitalIOLevel if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_GPIO: Error with GPIO device when setting IO
int FLEX_ExtDigitalIOGet(const FLEX_DigitalIOPin PinNum);

/// External Digital I/O Wakeup Modify Actions
typedef enum {
  FLEX_EXT_DIGITAL_IO_WAKEUP_ENABLE,   ///< Action to enable wakeup for an External Digital I/O pin
  FLEX_EXT_DIGITAL_IO_WAKEUP_DISABLE,  ///< Action to disable wakeup for an External Digital I/O pin
} FLEX_ExtDigitalIOWakeupModifyAction;

/// Enable/Disable an External Digital IO with wakeup capability for the next wakeup.
/// This wakes up on a falling edge of the External Digital IO.
/// \param[in] PinNum the external Digital I/O pin number.
/// \param[in] Action Enable/Disable wakeup for the selected Digital I/O pin.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_GPIO: Error with GPIO device when setting IO
int FLEX_ExtDigitalIOWakeupModify(const FLEX_DigitalIOPin PinNum,
  const FLEX_ExtDigitalIOWakeupModifyAction Action);

/// Wakeup Handler Function Pointer Declaration.
typedef void (*FLEX_IOWakeupHandler)(void);

/// Add or remove an external Digital I/O wakeup handler that will be called on a falling edge on
/// the external Digital I/O pin.
/// \param[in] Handler function pointer of the external Digital I/O wakeup handler.
/// \param[in] Action Add/Remove the input IO wakeup handler.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EALREADY: handler already exists, remove first
/// \retval -FLEX_ERROR_EINVAL: attempt to remove non-existent handler
int FLEX_ExtDigitalIOWakeupHandlerModify(const FLEX_IOWakeupHandler Handler,
  const FLEX_HandlerModifyAction Action);

///@}

/// @defgroup Ext_I2C External I2C
/// @brief Interact with FlexSense external i2c peripheral
///
/// The FlexSense device supports i2c communication with external sensors through the cable
/// interface. \warning The i2c communication should only be implemented with a cable length of 1m
/// or less between the FlexSense and a sensor.
///@{

/// Write to an i2c device at a given address.
///
/// \note i2c Addresses 0x20 and 0x42 are reserved, and a sensor with this address should not be
/// interfaced with FlexSense.
///
/// \param[in] Address the peripheral device address.
/// \param[in] TxData pointer to the TX buffer containing registers address and command.
/// \param[in] TxLength length of data to be sent.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EINVAL: invalid i2c address (address in use by internal device)
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_POWER_OUT: failed to power on external i2c bus
/// \retval -FLEX_ERROR_I2C: error initialising i2c bus
int FLEX_ExtI2CWrite(int Address, const uint8_t *const TxData, uint16_t TxLength);

/// Write to an i2c device at a given address and then read the response.
///
/// \note i2c Addresses 0x20 and 0x42 are reserved, and a sensor with this address should not be
/// interfaced with FlexSense.
///
/// \param[in] Address the peripheral device address.
/// \param[in] TxData pointer to TX buffer containing registers address and command.
/// \param[in] TxLength length of data to be sent.
/// \param[out] RxData pointer to the RX buffer.
/// \param[in] RxLength length of data to be received.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EINVAL: invalid i2c address (address in use by internal device)
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_POWER_OUT: failed to power on external i2c bus
/// \retval -FLEX_ERROR_I2C: error initialising i2c bus
int FLEX_ExtI2CRead(int Address, const uint8_t *const TxData, uint16_t TxLength,
  uint8_t *const RxData, uint16_t RxLength);

///@}

/// @defgroup Serial Serial (RS-485/RS-232)
/// @brief Configure and control the FlexSense serial interface
///
/// The FlexSense Serial interface supports RS-485 and RS-232. The RS-232 is configurable to support
/// a wide range of interface requirements.
///
/// \warning To maximise the battery life of your FlexSense, we recommend initialising and
/// de-initialising the Serial interface each time the job using it is called. This will
/// reduce the idle power usage of your FlexSense.
///@{

/// Serial Interface - Protocol Options
typedef enum {
  FLEX_SERIAL_PROTOCOL_RS485,  ///< use RS-485 as the Serial Protocol
  FLEX_SERIAL_PROTOCOL_RS232,  ///< use RS-232 as the Serial Protocol
} FLEX_SerialProtocol;

/// Serial Interface - Parity Options
typedef enum {
  FLEX_SERIAL_PARITY_NONE,  ///< No parity for serial device
  FLEX_SERIAL_PARITY_EVEN,  ///< Even parity for serial device
  FLEX_SERIAL_PARITY_ODD,   ///< Odd parity for serial device
} FLEX_SerialParity;

/// Serial Interface - Databits Options
typedef enum {
  FLEX_SERIAL_DATABITS_EIGHT,  ///< Eight databits for serial device
  FLEX_SERIAL_DATABITS_NINE,   ///< Nine databits for serial device
} FLEX_SerialDatabits;

/// Serial Interface - Stopbits Options
typedef enum {
  FLEX_SERIAL_STOPBITS_ONE,         ///< 1 stopbits for serial device
  FLEX_SERIAL_STOPBITS_HALF,        ///< 0.5 stopbits for serial device
  FLEX_SERIAL_STOPBITS_ONEANDHALF,  ///< 1.5 stopbits for serial device
  FLEX_SERIAL_STOPBITS_TWO,         ///< 2 stopbits for serial device
} FLEX_SerialStopbits;

/// Serial Interface - Serial Extended Configuration Options
typedef struct {
  FLEX_SerialProtocol protocol;  ///< Protocol for serial communication
  uint32_t baud_rate;            ///< Baud rate for serial communication
  FLEX_SerialParity parity;      ///< Parity for serial communication
  FLEX_SerialDatabits databits;  ///< Databits for serial communication
  FLEX_SerialStopbits stopbits;  ///< Stopbits for serial communication
} FLEX_SerialExOptions;

/// Initialise the selected serial interface (RS-485 or RS-232).
///
/// \note RS-485 and RS-232 cannot be initialised at the same time
///
/// \note De-initialise the Serial Interface before a job is completed to preserve device power.
///
/// \param[in] Protocol required protocol for serial communication.
/// \param[in] BaudRate required baudrate.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EALREADY: device already initialised
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_SERIAL: failed to initialise serial device
/// \retval -FLEX_ERROR_POWER_OUT: failed to enable power to serial interface
int FLEX_SerialInit(FLEX_SerialProtocol Protocol, uint32_t BaudRate);
/// Initialise the selected serial interface (RS-485 or RS-232).
///
/// \note RS-485 and RS-232 cannot be initialised at the same time
///
/// \note De-initialise the Serial Interface before a job is completed to preserve device power.
///
/// \param[in] Options extended configuration options for serial protocol.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EALREADY: device already initialised
/// \retval -FLEX_ERROR_IO_EXPANDER: failed to initialise or configure expander device
/// \retval -FLEX_ERROR_SERIAL: failed to initialise serial device
/// \retval -FLEX_ERROR_POWER_OUT: failed to enable power to serial interface
int FLEX_SerialInitEx(const FLEX_SerialExOptions Options);
/// Write to the serial interface synchronously.
/// \param[in] Tx pointer to the transmit buffer to be sent.
/// \param[in] Length length of data to be sent.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_NOT_INIT: device not initialised
int FLEX_SerialWrite(const uint8_t *Tx, size_t Length);
/// Read from the input buffer of the serial interface. The buffer size is 50 bytes.
/// \param[out] Rx pointer to the receive buffer.
/// \param[in] Length length of the receive buffer.
/// \return number of bytes read back or < 0 if read failed.
/// \retval -FLEX_ERROR_NOT_INIT: device not initialised
int FLEX_SerialRead(uint8_t *Rx, size_t Length);
/// De-initialise the serial interface.
///
/// \note De-initialise the Serial Interface before a job is completed to preserve device power.
///
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_POWER_OUT: failed to disable power to serial interface
int FLEX_SerialDeinit(void);

///@}

/// @defgroup Pulse_Counter Pulse Counter
/// @brief Configure and control the FlexSense Pulse Counter
///@{

/// Pulse Counter Options, bit-wise, can be ORed
typedef enum {
  FLEX_PCNT_DEFAULT_OPTIONS = 0,    ///< default option, counts on rising edge
  FLEX_PCNT_EDGE_FALLING = 1 << 0,  ///< count on falling edge, default rising edge
  FLEX_PCNT_DEBOUNCE_DISABLE =
    1 << 1,  ///< disable hardware debouncing, default enabled for about 160us
  FLEX_PCNT_PULL_UP
    __attribute__((deprecated)),  ///< Flag has been deprecated the FlexSenses pulse counter
                                  ///< internal pull-up/down state is handled internally.
} FLEX_PulseCounterOption;

/// Initialise the pulse counter and configure the event generation logic.
/// An event is generated when pulse count hits a multiple of \p Limit. Limit can be set
/// to a value from 0 to 256, or a multiple of 256.
///
/// \note Set \p Limit to 0 to disable event generation.
///
/// \param[in] Limit maximum value to count to before overflow occurs and reset counter to 0.
/// \param[in] Options configuration options selected from \p FLEX_PulseCounterOption.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
int FLEX_PulseCounterInit(const uint32_t Limit, const uint32_t Options);
/// Get the total count of the pulse counter.
/// \return the total count.
uint64_t FLEX_PulseCounterGet(void);
/// De-initialise the pulse counter.
void FLEX_PulseCounterDeinit(void);

/// Wakeup Handler Function Pointer Declaration.
typedef void (*FLEX_PCNTWakeupHandler)(void);

/// Add or remove a wakeup handler that will be called when the pulse counter is
/// triggered.
/// \param[in] Handler function pointer to the Pulse Count wakeup handler.
/// \param[in] Action Add/Remove the input Pulse Count wakeup handler.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EALREADY: handler already exists, remove first
/// \retval -FLEX_ERROR_EINVAL: attempt to remove non-existent handler
int FLEX_PulseCounterHandlerModify(const FLEX_PCNTWakeupHandler Handler,
  const FLEX_HandlerModifyAction Action);

///@}

/// @defgroup Delays Delays
/// @brief Application delays
///@{

/// Delay for a number of milliseconds.
/// \param[in] mSec delay time in milliseconds.
void FLEX_DelayMs(const uint32_t mSec);
/// Delay for a number of microseconds.
/// \param[in] uSec delay time in microseconds.
void FLEX_DelayUs(const uint32_t uSec);
/// Put the system in lower power mode for \p Sec seconds.
/// The calling job won't be interrupted by other jobs except for events while
/// sleeping.
/// \param[in] Sec sleep time in seconds.
void FLEX_Sleep(const uint32_t Sec);

///@}

/// @defgroup Time_Location Time and Location
/// @brief GNSS interface for time and location
///
/// \section sync_job_note GNSS Location and Time Synchronisation Job
/// The system in the background schedules a periodic task to synchronise
/// the system location and time, with the GNSS location and time by calling
/// the FLEX_GNSSFix() function.
/// This job runs periodically with increasing intervals until a GNSS fix is
/// obtained once every week. This helps to compensate for system clock drift,
/// but also ensures that there is an accurate location fix.
/// Please also note that when building user applications, enabling the
/// \p skip_gnss option in the build will disable this GNSS synchronisation
/// feature and warning messages will be printed on the debug console.
/// During production builds, please validate that the \p skip_gnss option is
/// disabled to ensure the correct operation of the FlexSense device.
/// View the section "Building the User Application" for more information regarding
/// the \p skip_gnss option.
///@{

/// Performs a GNSS FIX. On success, it outputs the fixed GNSS
/// latitude, longitude, and time values. It will also update the
/// systems real-time clock and latitude and longitude values.
/// \param[out] Lat the recorded latitude.
/// \param[out] Lon the recorded longitude.
/// \param[out] Time the recorded fix time.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_GNSS: error on GNSS device, failed to read
/// \retval -FLEX_ERROR_POWER_OUT: failed to enable power to module
int FLEX_GNSSFix(int32_t *const Lat, int32_t *const Lon, time_t *const Time);

/// Returns true if the system has obtained a valid time and location fix.
bool FLEX_GNSSHasValidFix(void);

/// Gets the latitude, longitude, and time of the last GNSS fix.
/// \param[out] LastLatitude the last GNSS fixes latitude in degrees multiplied by 1e7.
/// \param[out] LastLongitude the last GNSS fixes longitude in degrees multiplied by 1e7.
/// \param[out] LastFixTime the time of the last GNSS fix as an epoch time.
void FLEX_LastLocationAndLastFixTime(int32_t *const LastLatitude, int32_t *const LastLongitude,
  time_t *const LastFixTime);

/// Get the current epoch time
time_t FLEX_TimeGet(void);

///@}

/// @defgroup User_Msg User Messages
/// @brief Build and schedule messages for satellite transmission
///@{

/// Calling ScheduleMessage when the number of slots returned by
/// \p MessageSlotsFree is 0 and replaces an existing message in the queue. This may
/// result in dropped messages.
/// \param[in] Message pointer to the message to be scheduled.
/// \param[in] MessageSize length of the message.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
int FLEX_MessageSchedule(const uint8_t *const Message, const size_t MessageSize);
/// Returns the number of available slots in the internal message queue,
/// that is, the number of messages that can be scheduled with \p FLEX_MessageSchedule
int FLEX_MessageSlotsFree(void);
/// Returns the number of bytes remaining in the internal queue, that is,
/// the number of bytes that can be scheduled with \p FLEX_MessageSchedule
/// \return number of bytes remaining in the internal queue.
size_t FLEX_MessageBytesFree(void);
/// Save all messages in the message queue to the module's persistent storage.
/// Saved messages will be transmitted after reset.
void FLEX_MessageSave(void);
/// Clear all messages in the message queue.
void FLEX_MessageQueueClear(void);

///@}

/// @defgroup Device Control
/// @brief Handle control messages received by FlexSense
/// @{

/// Message Receive Handler Function Pointer Declaration.
/// \param message A pointer to the message if size is greater than 0 else NULL.
/// \param size The length of the received message.
typedef void (*FLEX_MessageReceiveHandler)(uint8_t *const message, const int size);

/// Add or remove a handler that will be called when a message is received.
/// \param[in] Handler function pointer to the Message Receive handler.
/// \param[in] Action Add/Remove the input Message Receive handler.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EALREADY: handler already exists, remove first
/// \retval -FLEX_ERROR_EINVAL: attempt to remove non-existent handler
int FLEX_MessageReceiveHandlerModify(const FLEX_MessageReceiveHandler Handler,
  const FLEX_HandlerModifyAction Action);

/// @}

/// @defgroup Time_Job_Scheduling Time and Job Scheduling
/// @brief Create and schedule jobs for the application
///@{

/// Scheduled Job Function Pointer Declaration.
/// The return value is the time at which the job should next run.
typedef time_t (*FLEX_ScheduledJob)(void);
/// Reset the schedule for an existing job or add a new job.
/// \param[in] Job function pointer to the job to be scheduled.
/// \param[in] Time time to run the job.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_ENOMEM:  if the maximum number of jobs reached.
int FLEX_JobSchedule(const FLEX_ScheduledJob Job, const time_t Time);

/// Use this function to schedule a job to run immediately,
/// by assigning the return value of this function to the time value
/// at which the job should next run.
/// \return time at which the job will next run.
time_t FLEX_ASAP(void);
/// Use this function to stop a job from being scheduled again,
/// by assigning the return value of this function to the time value
/// at which the job should next run.
/// \return time at which the job will next run.
time_t FLEX_Never(void);
/// Use this function to set the time value at which a job will run next.
/// The job will run \p Secs after the current run.
/// \param[in] Secs seconds after to run the job next.
/// \return time at which the job will next run.
time_t FLEX_SecondsFromNow(const unsigned Secs);
/// Use this function to set the time value at which a job will run next.
/// The job will run \p Mins after the current run.
/// \param[in] Mins minutes after to run the job next.
/// \return time at which the job will next run.
time_t FLEX_MinutesFromNow(const unsigned Mins);
/// Use this function to set the time value at which a job will run next.
/// The job will run \p Hours after the current run.
/// \param[in] Hours hours after to run the job next.
/// \return time at which the job will next run.
time_t FLEX_HoursFromNow(const unsigned Hours);
/// Use this function to set the time value at which a job will run next.
/// The job will run \p Days after the current run.
/// \param[in] Days days after to run the job next.
/// \return time at which the job will next run.
time_t FLEX_DaysFromNow(const unsigned Days);

///@}

/// @defgroup Mod_ID_and_Reg_ID Module ID and Part Number
/// @brief Module and Registration ID of the FlexSense device
///@{

/// Returns the string of module ID in the format of "00xxxxxxxx Mx-2x" where
/// 00xxxxxxxx is the module ID in hexadecimal string and Mx-2x is the module's
/// part number.
/// \return null if module ID is not available.
const char *FLEX_ModuleIDGet(void);

/// Returns the string of the registration code.
/// \return null if registration code is not available.
const char *FLEX_RegistrationCodeGet(void);

///@}

/// @defgroup Temp_Sensor Temperature Sensor
/// @brief Onboard temperature sensor interface
///@{

/// Get the temperature inside the module in degrees Celsius.
/// \param[out] Temperature recorded temperature value in degrees Celsius.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
int FLEX_TemperatureGet(float *const Temperature);

///@}

/// @defgroup Tests Self Tests
/// @brief Run a self-test on the FlexSense device
///@{

/// Performs a Hardware test for the FlexSense. The test enables the 5V power
/// on the external connector, and blinks the boards led from green to blue.
/// The test blocks all other board functionality until the push button is pressed.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_POWER_OUT: unable to initialise power out or LED
/// \retval -FLEX_ERROR_BUTTON: error enabling or disabling push button
/// \retval -FLEX_ERROR_LED: error setting LED state
int FLEX_HWTest(void);

///@}

/// @defgroup System_Tick System Tick
/// @brief Get the system tick
///@{

/// Get the current system tick (1000 ticks per second).
/// \return the current system tick
uint32_t FLEX_TickGet(void);

///@}

/// @defgroup Power_Diag Power Diagnostics
/// @brief Diagnostics for the power subsystem
///@{

/// Reads the FlexSense's battery voltage in mV.
/// \note When the FlexSense is operating on external power, the battery
/// Voltage reports zero.
/// \param[out] VoltageMilliVolts the current battery voltage in mV.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_ENOTRECOVERABLE: BLE module comms in an unrecoverable state.
/// \retval -FLEX_ERROR_EPROTO: BLE module comms protocol error most likely a version miss match.
/// \retval -FLEX_ERROR_ECOMM: Failed to communicate with the BLE module.
int FLEX_GetBatteryVoltage(int32_t *const VoltageMilliVolts);

// Reads if FlexSense is running on external power or battery power.
/// \param[out] IsOnExternalPower current state of the external power.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_ENOTRECOVERABLE: BLE module comms in an unrecoverable state.
/// \retval -FLEX_ERROR_EPROTO: BLE module comms protocol error most likely a version miss match.
/// \retval -FLEX_ERROR_ECOMM: Failed to communicate with the BLE module.
int FLEX_IsOnExternalPower(bool *const IsOnExternalPower);

/// On External Power Handler Function Pointer Declaration.
/// \param[in] IsOnExternalPower if the FlexSense is using external power or not.
typedef void (*FLEX_OnExternalPowerHandler)(const bool *const IsOnExternalPower);

/// Adds an event handler for when the external power state changes.
/// \note There can only be one handler subscribed to the event at a time,
/// so passing in a new handler will remove the current one.
/// \param[in] Handler the event handler to be called on external power state change.
void FLEX_OnExternalPowerHandlerSet(const FLEX_OnExternalPowerHandler Handler);

///@}

#endif /* FLEX_H */
