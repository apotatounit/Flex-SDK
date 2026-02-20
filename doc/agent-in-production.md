Sensor Node Firmware – System Requirements Specification (Production)

1. System Overview

The system is a low-power sensor node that periodically:
	1.	Wakes up
	2.	Powers sensors
	3.	Acquires measurements
	4.	Enqueues a transmission message
	5.	Returns to sleep

The nominal measurement cycle period is 3600 seconds.

⸻

2. Operating Cycle

2.1 Measurement Cycle
	•	The system shall wake up every 3600 seconds.
	•	The system shall:
	1.	Initialize sensor power.
	2.	Wait for a configurable wake-up delay.
	3.	Acquire sensor measurements.
	4.	Encode measurement values.
	5.	Enqueue a transmission message.
	6.	Suspend sensor power.
	7.	Enter sleep mode.
	•	The system shall repeat this cycle every 3600 seconds.

⸻

3. Sensor Interfaces

3.1 Pressure Sensor (Analog Input)
	•	The system shall read analog voltage from a pressure sensor.
	•	The system shall sample the analog input one or multiple times during the measurement window.
	•	The system shall compute the median of the collected samples.
	•	The system shall encode the result as millivolts (mV) in range 0–5000 mV with 1 mV resolution (i.e. 0.000–5.000 V with 0.001 V precision).

Encoding requirement:
	•	Voltage shall be encoded as an integer count of millivolts in range 0–5000:

encoded_voltage_mV = floor(voltage_in_volts * 1000)



⸻

3.2 Flow Rate Sensor (Pulse Input)
	•	The system shall count pulses from the flow rate sensor during the measurement window.
	•	The system shall derive pulse-per-minute (ppm) as:

ppm = floor(pulses_counted * (60 / window_seconds))

	•	The ppm value shall be an unsigned integer.
	•	The ppm value shall support range 0–15000.
	•	If ppm exceeds 15000, it shall be clamped to 15000.

Encoding requirement:
	•	ppm shall be stored as unsigned integer supporting range 0–15000.

⸻

3.3 Temperature Sensor (Modbus over RS485)
	•	The system shall communicate with the temperature sensor using Modbus over RS485.
	•	The system shall initialize the Modbus interface at each wake cycle.
	•	The system shall read temperature one or multiple times during the measurement window.
	•	The system shall compute the median temperature value.
	•	The system shall tolerate Modbus initialization and read errors.
	•	The system shall suspend/deinitialize Modbus before entering sleep mode.

⸻

4. Measurement Window
	•	The system shall perform measurements during a configurable time window between 1 and 60 seconds.
	•	The measurement window shall be defined as a code constant.
	•	Sampling frequency is implementation-defined but must allow median computation.

⸻

5. Fault Handling

The system shall determine faults of each external interface and internal component and store them in a bitwise error code.

5.1 Required Error Flags

The error code shall contain at least the following flags:
	•	Modbus initialization error
	•	Temperature reading error
	•	Analog reading error
	•	Interrupt (pulse) reading error
	•	Power initialization error
	•	Deinitialization error

5.2 Fault Behavior
	•	The system shall continue operating even if any sensor fails or is disconnected.
	•	The system shall enqueue a message even if sensor errors occur.
	•	The error code shall reflect all detected faults during the cycle.
	•	Sensor failure shall not reset the rolling counter.

⸻

6. Message Payload

The system shall enqueue one message per measurement cycle containing:
	•	Current timestamp
	•	Rolling counter
	•	Pulse-per-minute (ppm)
	•	Encoded analog voltage
	•	Temperature value
	•	Error code bitmask

6.1 Rolling Counter
	•	The rolling counter shall increment once per successful measurement cycle.
	•	The rolling counter shall not reset due to sensor errors.
	•	The rolling counter shall reset only on power cycle or firmware restart.

⸻

7. Power Control
	•	The system shall initialize power supply to sensors at the beginning of the wake cycle.
	•	The system shall suspend power supply to sensors before entering sleep mode.
	•	The system shall detect and report power initialization and deinitialization errors.

⸻

8. Wake-Up Delay
	•	The system shall support a configurable wake-up delay before measurement begins.
	•	The wake-up delay shall be defined as a code constant.

⸻

9. Debug Mode

Debug mode shall be implemented as a separate firmware build.

9.1 Activation
	•	Debug mode shall activate when powered via USB.
	•	Debug firmware shall reuse the main firmware logic but modify behavior as defined below.

9.2 Debug Behavior
	•	The system shall not enter sleep mode.
	•	The system shall not queue transmission messages.
	•	The system shall continuously read sensor outputs.
	•	The system shall print sensor readings to the debug console at a configurable interval.

⸻

10. Configuration Parameters

All configuration parameters shall be implemented as hardcoded compile-time constants, including:
	•	Measurement window duration
	•	Wake-up delay
	•	Debug print interval
	•	Modbus parameters
	•	Sensor scaling constants

⸻

11. Known Previous Issue (For Implementation Review)

In a previous implementation:
	•	If the Modbus sensor was disconnected or powered off:
	•	A message was still queued and sent later.
	•	The Modbus Deinit probably fails
	•	Measurement intervals became inconsistent.
	•	The error code indicated a temperature sensor fault.
	•	The rolling counter was always reset to 0.

The current implementation shall prevent:
	•	Rolling counter reset due to sensor fault.
	•	Measurement interval distortion due to Modbus failure.

⸻

12. Instructions for Cursor companion

When comparing this specification to the existing implementation:
	1.	Verify ppm scaling and rounding logic.
	2.	Verify voltage encoding (0–5V with 0.001V precision).
	3.	Verify error bitmask completeness and consistency.
	4.	Verify rolling counter persistence behavior.
	5.	Verify Modbus failure handling does not:
	•	Block cycle completion
	•	Distort 3600-second interval
	•	Reset rolling counter
	6.	Verify debug firmware separation and behavior differences.
	7.	Identify mismatches between required clamping behavior and implementation.
	8.	Identify missing fault flags based on used interfaces (FlexSense API, ADC, GPIO interrupt, RS485).
