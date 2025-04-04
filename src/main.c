#include <stdio.h>
#include <stdint.h>
#include "flex.h"
#include <time.h>
#include "modbussensor.h"

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

typedef struct
{
  uint8_t sequence_number;
  uint32_t time;
  // int32_t latitude;
  // int32_t longitude;
  int16_t temperature;
  int16_t pressure;
  int16_t pressure_ain;
  int16_t pulse_rate;
} __attribute__((packed)) Message;

_Static_assert(sizeof(Message) <= FLEX_MAX_MESSAGE_SIZE, "can't exceed the max message size");

// Arrays to store sensor readings
static float temperature_readings[SENSOR_READINGS_COUNT];
static float pressure_readings[SENSOR_READINGS_COUNT];
// static uint32_t flow_meter_pulse_count = 0;

// Simulated functions for temperature, pressure, and flow meter
static float ReadTemperatureSensor(void)
{
  // Replace with actual temperature sensor reading logic
  float temperature = UINT32_MAX / 10.0; // Simulated temperature reading
  int result = Modbus_Request_Receive_Temperature(&temperature);
  if (result)
  {
    printf("Failed to Read Temperature from Modbus sensor.\r\n");
  }
  return temperature;
}

static float ReadPressureSensor(void)
{
  uint32_t SensorReading = UINT32_MAX;

  if (FLEX_AnalogInputReadVoltage(&SensorReading) != 0)
  {
    printf("Failed to Read Voltage.\r\n");
  }
  return SensorReading / 1000.0; // Convert from mV to V
}

static uint32_t pulse_count_start_tick;
static uint32_t pulse_count_end_tick;

static void StartFlowMeterTimer(void)
{
  // Start counting pulses from the flow meter
  printf("Starting flow meter pulse counting...\r\n");
  pulse_count_start_tick = FLEX_TickGet(); // Use FLEX SDK function to get start tick
}

typedef struct
{
  uint32_t pulse_count;
  uint32_t elapsed_time_ms;
} FlowMeterData;

static FlowMeterData StopFlowMeterPulseCounting(void)
{
  uint32_t flow_meter_pulse_count = (uint32_t)FLEX_PulseCounterGet();

  // Replace with actual logic to stop counting and return pulse count and elapsed time
  pulse_count_end_tick = FLEX_TickGet(); // Use FLEX SDK function to get end tick
  uint32_t elapsed_time_ms = pulse_count_end_tick - pulse_count_start_tick;
  printf("Elapsed Time for Pulse Counting: %u milliseconds\r\n", (uint16_t)elapsed_time_ms);

  FlowMeterData data = {flow_meter_pulse_count, elapsed_time_ms};
  return data;
}

static void CollectSensorData(void)
{

  uint32_t prev_pulse_count = (uint32_t)FLEX_PulseCounterGet();
  // Collect temperature and pressure readings
  for (int i = 0; i < SENSOR_READINGS_COUNT; i++)
  {
    uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
    temperature_readings[i] = ReadTemperatureSensor();
    pressure_readings[i] = ReadPressureSensor();

    printf("Temperature: %.2f °C, Pressure: %.3f V, pulses: %u\r\n", temperature_readings[i], pressure_readings[i], (uint16_t)(pulse_count - prev_pulse_count));
    prev_pulse_count = pulse_count;
    FLEX_DelayMs(DATA_COLLECTION_INTERVAL_MS);
  }

  // Calculate averages
  float temperature_sum = 0.0, pressure_sum = 0.0;
  for (int i = 0; i < SENSOR_READINGS_COUNT; i++)
  {
    temperature_sum += temperature_readings[i];
    pressure_sum += pressure_readings[i];
  }
  float avg_temperature = temperature_sum / SENSOR_READINGS_COUNT;
  float avg_pressure_ain = pressure_sum / SENSOR_READINGS_COUNT;

  // Map avg_pressure_ain from 0.5-4.5 volts to 0-5 bar
  float avg_pressure;
  if (avg_pressure_ain < 0.5 || avg_pressure_ain > 4.5)
  {
    printf("Error: Pressure reading out of range (%.2fV)\r\n", avg_pressure_ain);
    avg_pressure = -1; // Indicate an error
  }
  else
  {
    float calib_ain = 0.05;                                                    // Calibration value
    avg_pressure = (avg_pressure_ain - calib_ain - 0.5) * (5.0 / (4.5 - 0.5)); // Linear mapping
  }

  // Stop flow meter pulse counting
  FlowMeterData flow_data = StopFlowMeterPulseCounting();

  // Calculate flow rate (pulses per second)
  uint32_t total_pulses = flow_data.pulse_count;
  uint32_t elapsed_time_ms = flow_data.elapsed_time_ms;
  float flow_rate = (float)total_pulses / (elapsed_time_ms / 1000.0); // Calculate flow rate in pulses/sec

  // Print results
  printf("Average Temperature: %.2f °C\r\n", avg_temperature);
  printf("Average Pressure AIN: %.3f V\r\n", avg_pressure_ain);
  printf("Average Pressure: %.3f bar\r\n", avg_pressure);
  printf("Flow Rate: %.2f pulses/sec\r\n", flow_rate);
}

static int InitDevice(void)
{
  if (Modbus_Init() != 0)
  {
    printf("Failed to Init Modbus.\r\n");
    return -1;
  }
  return 0;
}

static int InitSensors(void)
{
  printf("Initialising sensors...\r\n");
  // Enable power supply to sensors
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("Failed to enable sensor power supply.\r\n");
    return -1;
  }
  else
  {
    printf("Sensor power supply enabled.\r\n");
  }
  // Initialise the analog input

  if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
  {
    printf("Failed to Init Analog Input.\r\n");
    return -1;
  }
  else
  {
    printf("Analog Input initialised.\r\n");
  }
  // if (Modbus_Init() != 0)
  // {
  //   printf("Failed to Init Modbus.\r\n");
  //   // return -1;
  // }

  // Wait to begin counting pulses from the flow meter
  FLEX_DelayMs(SENSOR_FLOW_METER_STABILISE_DELAY_MS);

  // Initialise to generate event every N pulses
  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS))
  {
    printf("Failed to initialise pulse counter\n");
    return -1;
  }
  else
  {
    printf("Pulse counter initialised.\r\n");
  }
  StartFlowMeterTimer();

  // Wait for sensors to stabilize
  FLEX_DelayMs(SENSOR_STABILISE_DELAY_MS);
  return 0;
}

static void DeinitSensors(void)
{
  // Deinit sensors
  FLEX_AnalogInputDeinit();
  FLEX_PowerOutDeinit();
  FLEX_PulseCounterDeinit();
  // Modbus_Deinit();
}

// Blinky Interval
#define BLINKY_INTERVAL_MIN 1

// Number of LED Flashes
#define NO_OF_FLASHES 10

// Green LED ON time
#define LED_ON_TIME_SEC 1

// Green LED OFF time
#define LED_OFF_TIME_SEC 1

static time_t ScheduleNextRun(void)
{
  // Schedule next run in 1 hour
  time_t next_run_time = FLEX_TimeGet() + 30;

  FLEX_LEDGreenStateSet(FLEX_LED_ON);
  printf("Green LED On\n");

  // Init sensors
  if (InitSensors())
  {
    printf("Aborting Init Sensors, retry in 10 seconds\r\n");
    next_run_time = FLEX_TimeGet() + 10; // Retry in 1 minute
  }
  else
  {
    CollectSensorData();
  }
  DeinitSensors();

  FLEX_LEDGreenStateSet(FLEX_LED_OFF);
  return next_run_time; // Schedule next run in 1 hour
}

// static time_t send_message(void) {
//   static uint8_t sequence_number = 0;

//   Message message = {0};
//   message.sequence_number = sequence_number++;
//   message.time = FLEX_TimeGet();

//   int32_t latitude = 0;
//   int32_t longitude = 0;
//   FLEX_LastLocationAndLastFixTime(&latitude, &longitude, NULL);
//   message.latitude = latitude;
//   message.longitude = longitude;

//   int16_t temperature = 0;
//   int16_t humidity = 0;
//   read_temperature_and_humidity(&temperature, &humidity);
//   message.temperature = temperature;
//   message.humidity = humidity;

//   // Schedule messages for satellite transmission
//   FLEX_MessageSchedule((const uint8_t *const)&message, sizeof(message));
//   printf("Scheduled message: \n");
//   printf("  sequence_number: %u\n", message.sequence_number);
//   printf("  time: %lu\n", message.time);
//   printf("  latitude: %ld\n", message.latitude);
//   printf("  longitude: %ld\n", message.longitude);
//   printf("  temperature: %d\n", message.temperature);
//   printf("  humidity: %d\n", message.humidity);

//   return (FLEX_TimeGet() + 24 * 3600 / MESSAGES_PER_DAY);
// }

void FLEX_AppInit()
{
  printf("%s\r\n", APPLICATION_NAME);
  InitDevice();
  FLEX_JobSchedule(ScheduleNextRun, FLEX_ASAP());
}