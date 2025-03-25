#include <stdio.h>
#include <stdint.h>
#include "flex.h"
#include <time.h>

#define APPLICATION_NAME "Sensor Data Logger"

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE FLEX_ANALOG_IN_VOLTAGE
#define PULSE_WAKEUP_COUNT 0

#define SENSOR_FLOW_METER_STABILISE_DELAY_MS 1000
#define SENSOR_STABILISE_DELAY_MS 2000
#define DATA_COLLECTION_DURATION_SEC 20
#define DATA_COLLECTION_INTERVAL_MS 1000
#define SENSOR_READINGS_COUNT (DATA_COLLECTION_DURATION_SEC * 1000 / DATA_COLLECTION_INTERVAL_MS)

// Arrays to store sensor readings
static float temperature_readings[SENSOR_READINGS_COUNT];
static float pressure_readings[SENSOR_READINGS_COUNT];
// static uint32_t flow_meter_pulse_count = 0;

// Simulated functions for temperature, pressure, and flow meter
static float ReadTemperatureSensor(void) {
  // Replace with actual temperature sensor reading logic
  return 25.0 + (rand() % 100) / 100.0; // Example: 25.0°C ± random noise
}

static float ReadPressureSensor(void) {
  uint32_t SensorReading = UINT32_MAX;
  
  if (FLEX_AnalogInputReadVoltage(&SensorReading) != 0) {
    printf("Failed to Read Voltage.\r\n");
  }
  return SensorReading / 1000.0; // Convert from mV to V
}

static uint32_t pulse_count_start_tick;
static uint32_t pulse_count_end_tick;

static void StartFlowMeterTimer(void) {
  pulse_count_start_tick = FLEX_TickGet(); // Use FLEX SDK function to get start tick
}

typedef struct {
  uint32_t pulse_count;
  uint32_t elapsed_time_ms;
} FlowMeterData;

static FlowMeterData StopFlowMeterPulseCounting(void) {
  uint32_t flow_meter_pulse_count = (uint32_t)FLEX_PulseCounterGet();

  // Replace with actual logic to stop counting and return pulse count and elapsed time
  pulse_count_end_tick = FLEX_TickGet(); // Use FLEX SDK function to get end tick
  uint32_t elapsed_time_ms = pulse_count_end_tick - pulse_count_start_tick;
  printf("Elapsed Time for Pulse Counting: %u milliseconds\r\n", elapsed_time_ms);

  FlowMeterData data = {flow_meter_pulse_count, elapsed_time_ms};
  return data;
}

static void CollectSensorData(void) {

  // Collect temperature and pressure readings
  for (int i = 0; i < SENSOR_READINGS_COUNT; i++) {
    temperature_readings[i] = ReadTemperatureSensor();
    pressure_readings[i] = ReadPressureSensor();
    printf("Temperature: %.2f°C, Pressure: %.2fV\r\n", temperature_readings[i], pressure_readings[i]);
    FLEX_DelayMs(DATA_COLLECTION_INTERVAL_MS);
  }

  // Calculate averages
  float temperature_sum = 0.0, pressure_sum = 0.0;
  for (int i = 0; i < SENSOR_READINGS_COUNT; i++) {
    temperature_sum += temperature_readings[i];
    pressure_sum += pressure_readings[i];
  }
  float avg_temperature = temperature_sum / SENSOR_READINGS_COUNT;
  float avg_pressure_ain = pressure_sum / SENSOR_READINGS_COUNT;

  // Map avg_pressure_ain from 0.5-4.5 volts to 0-5 bar
  float avg_pressure;
  if (avg_pressure_ain < 0.5 || avg_pressure_ain > 4.5) {
    printf("Error: Pressure reading out of range (%.2fV)\r\n", avg_pressure_ain);
    avg_pressure = -1; // Indicate an error
  } else {
    avg_pressure = (avg_pressure_ain - 0.5) * (5.0 / (4.5 - 0.5)); // Linear mapping
  }

  // Stop flow meter pulse counting
  FlowMeterData flow_data = StopFlowMeterPulseCounting();

  // Calculate flow rate (pulses per second)
  uint32_t total_pulses = flow_data.pulse_count;
  uint32_t elapsed_time_ms = flow_data.elapsed_time_ms;
  float flow_rate = (float)total_pulses / (elapsed_time_ms / 1000.0); // Calculate flow rate in pulses/sec

  // Print results
  printf("Average Temperature: %.2f°C\r\n", avg_temperature);
  printf("Average Pressure: %.2fV\r\n", avg_pressure);
  printf("Flow Rate: %.2f pulses/sec\r\n", flow_rate);

  // Disable power supply to sensors
  FLEX_PowerOutDeinit();
}

static int InitSensors(void) {
  // Enable power supply to sensors
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0) {
    printf("Failed to enable sensor power supply.\r\n");
    return -1;
  }
  if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0) {
    printf("Failed to Init Analog Input.\r\n");
    return -1;
  }

  // Wait to begin counting pulses from the flow meter
  FLEX_DelayMs(SENSOR_FLOW_METER_STABILISE_DELAY_MS);

  // Initialise to generate event every N pulses
  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS)) {
    printf("Failed to initialise pulse counter\n");
    return -1;
  }
  StartFlowMeterTimer();

  // Wait for sensors to stabilize
  FLEX_DelayMs(SENSOR_STABILISE_DELAY_MS);
  return 0;
}

static void DeinitSensors(void) {
  // Deinit sensors
  FLEX_AnalogInputDeinit();
  FLEX_PowerOutDeinit();
}

static time_t ScheduleNextRun(void) {
  // Init sensors
  if (InitSensors()) {
    printf("Aborting Init Sensors, retry in 1 minute");
    return FLEX_TimeGet() + 60; // Retry in 1 minute
  }
  else {
    CollectSensorData();
  }
  DeinitSensors();

  // Schedule next run in 1 hour
  return FLEX_TimeGet() + 3600; // Schedule next run in 1 hour
}

void FLEX_AppInit() {
  printf("%s\r\n", APPLICATION_NAME);
  FLEX_JobSchedule(ScheduleNextRun, FLEX_ASAP());
}