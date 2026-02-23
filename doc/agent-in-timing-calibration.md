Sensor Timing Calibration – Requirements Specification

1. Overview

Calibrate minimum power-up delays and maximum reading frequencies for:
	•	Pulse interrupt sensor
	•	Analog input sensor
	•	Modbus temperature sensor

Separate firmware build from production.

⸻

2. Operating Behavior

	•	No sleep mode
	•	No message transmission
	•	Continuous power cycling: power on → delay → read → power off → wait 5s → repeat
	•	Print all measurements and timing to console (milliseconds)

⸻

3. Pulse Interrupt Sensor

3.1 Power-Up Delay Test
	•	Sweep delay: 0–5000 ms (configurable)
	•	For each delay:
		1. Power on
		2. Wait delay
		3. Init pulse counter
		4. Count for 1 second
		5. Read count
		6. Deinit counter
		7. Power off
	•	Find minimum delay where counting is reliable

3.2 Reading Frequency Test
	•	Sweep delay: 10–1000 ms (configurable)
	•	For each delay:
		1. Power on
		2. Wait (minimum from 3.1)
		3. Repeat 10×: init counter → count 100ms → read → deinit → wait delay
		4. Power off
	•	Find minimum delay between readings

3.3 Validation
	•	Sensor alive if: pulse count > 1 AND rate is stable
	•	Report: delay, count, duration, alive status, statistics

⸻

4. Analog Input Sensor

4.1 Power-Up Delay Test
	•	Sweep delay: 0–5000 ms (configurable)
	•	For each delay:
		1. Power on
		2. Wait delay
		3. Init analog input
		4. Take 10 readings (minimal delay between)
		5. Calculate: mean, median, std dev, min, max
		6. Deinit analog input
		7. Power off
	•	Find minimum delay where readings stabilize

4.2 Reading Frequency Test
	•	Sweep delay: 1–500 ms (configurable)
	•	For each delay:
		1. Power on
		2. Wait (minimum from 4.1)
		3. Init analog input
		4. Take 100 readings with delay between each
		5. Calculate statistics
		6. Deinit analog input
		7. Power off
	•	Find minimum delay between readings

4.3 Validation
	•	Stable if: std dev < threshold (e.g., 10 mV) AND readings in 0–5V range
	•	Sensor alive if: reading ≈ 1.0V ± tolerance (e.g., ±0.2V)
	•	Report: delay, voltage, alive status, statistics

⸻

5. Modbus Temperature Sensor

5.1 Power-Up Delay Test
	•	Sweep delay: 0–10000 ms (configurable)
	•	For each delay:
		1. Power on
		2. Wait delay
		3. Init Modbus
		4. Read temperature (retry up to 3×)
		5. Record success/failure, response time
		6. Deinit Modbus
		7. Power off
	•	Find minimum delay where communication is reliable (>95% success)

5.2 Reading Frequency Test
	•	Sweep delay: 10–2000 ms (configurable)
	•	For each delay:
		1. Power on
		2. Wait (minimum from 5.1)
		3. Init Modbus
		4. Read temperature 20× with delay between each
		5. Record success/failure, response time for each
		6. Deinit Modbus
		7. Power off
	•	Find minimum delay between reads

5.3 Validation
	•	Sensor alive if: Modbus init succeeds AND deinit succeeds AND at least one temperature read succeeds
	•	Report: delay, temperature, transaction duration, alive status, success rate

⸻

6. Configuration

Compile-time defines:
	•	ENABLE_PULSE_TEST (0/1)
	•	ENABLE_ANALOG_TEST (0/1)
	•	ENABLE_MODBUS_TEST (0/1)
	•	Test ranges (min, max, step) for each sensor
	•	Stability thresholds
	•	Success rate threshold (default: 95%)

⸻

7. Output Format

For each measurement:
	•	Timestamp (ms)
	•	Test name
	•	Power-up delay (ms)
	•	Reading delay (ms)
	•	Sensor reading (count/voltage/temperature)
	•	Sensor alive (yes/no)
	•	Success/failure
	•	Timing statistics

Summary report:
	•	Recommended minimum power-up delay per sensor
	•	Recommended minimum reading delay per sensor
	•	Maximum reliable reading frequency per sensor
