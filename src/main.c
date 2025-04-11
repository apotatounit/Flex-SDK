#include <stdio.h>
#include <stdint.h>
#include "flex.h"
#include <time.h>
#include "modbussensor.h"

#define APPLICATION_NAME "Sensor Pipeline Logger"

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE FLEX_ANALOG_IN_VOLTAGE
#define PULSE_WAKEUP_COUNT 0

#define SENSOR_FLOW_METER_STABILISE_DELAY_MS 100
#define SENSOR_STABILISE_DELAY_MS 100
// #define DATA_COLLECTION_DURATION_SEC 10
#define DATA_COLLECTION_INTERVAL_MS 1000
#define SENSOR_READINGS_COUNT 5

#define ENABLE_TRANSMIT 0
#define ENABLE_MODBUS 1
bool bInitModbusRequired = true; // only required on first init after power supply init
#define LED_BLINK_DELAY 200 // ms

typedef struct
{
  int16_t temperature;
  uint16_t analog_in;
  uint16_t pulse_per_minute;
  uint8_t ret_temp;
  uint8_t ret_ain;
  uint8_t ret_ppm;
  uint8_t ret_flexsense;
} SensorMeasurements;

#define NEXT_RUN_INTERVAL_SEC 60 * 60
typedef struct
{
  uint8_t sequence_number;
  uint32_t time;
  // int32_t latitude;
  // int32_t longitude;
  int16_t temperature;
  uint16_t analog_in;
  uint16_t pulse_per_minute;
} __attribute__((packed)) Message;

typedef struct
{
  int16_t return_code;
  float value;
} ReadResult;

_Static_assert(sizeof(Message) <= FLEX_MAX_MESSAGE_SIZE, "can't exceed the max message size");

static Message MakeMessage(SensorMeasurements measurements);
static int send_message(Message message);
static void BlinkLed(int count, bool inverse);

// Arrays to store sensor readings
// static float temperature_readings[SENSOR_READINGS_COUNT];
// static float pressure_readings[SENSOR_READINGS_COUNT];
// static uint32_t flow_meter_pulse_count = 0;

// Simulated functions for temperature, pressure, and flow meter
static ReadResult ReadTemperatureSensor(void)
{
  // Replace with actual temperature sensor reading logic
  float temperature = UINT32_MAX / 10.0; // Simulated temperature reading
  int result = 0;
  if (ENABLE_MODBUS)
  {
    result = Modbus_Request_Receive_Temperature(&temperature);
  }
  else
  {
    // Simulate a successful read
    temperature = 25.0; // Simulated temperature reading
    result = 0;
  }
  // Modbus_Request_Receive_Temperature(&temperature);
  if (result)
  {
    printf("Failed to Read Temperature from Modbus sensor.\r\n");
  }
  ReadResult read_result = {result, temperature};
  return read_result;
}

static ReadResult ReadPressureSensor(void)
{
  uint32_t SensorReading = UINT32_MAX;
  ReadResult read_result = {0, 0};

  int ret = FLEX_AnalogInputReadVoltage(&SensorReading);
  if (ret != 0)
  {
    printf("Failed to Read Voltage.\r\n");
    read_result.return_code = ret;
  }
  else
  {
    read_result.value = SensorReading / 1000.0; // Convert from mV to V
    read_result.return_code = 0;
    // printf("Analog Input Voltage: %.3f V\r\n", read_result.value);
  }

  return read_result;
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

static SensorMeasurements CollectSensorData(void)
{
  uint32_t prev_pulse_count = (uint32_t)FLEX_PulseCounterGet();
  // Calculate averages
  float temperature_sum = 0.0, pressure_sum = 0.0;
  int sum_counter = 0;
  // Collect temperature and pressure readings
  for (int i = 0; i < SENSOR_READINGS_COUNT; i++)
  {
    printf("Collecting sensor data...\r\n");
    uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
    ReadResult temperature_result = ReadTemperatureSensor();
    ReadResult pressure_result = ReadPressureSensor();

    if (temperature_result.return_code != 0 || pressure_result.return_code != 0)
    {
      printf("Error reading sensors. Skipping this iteration.\r\n");
      continue; // Skip this iteration if there's an error
      BlinkLed(2, true);
    }

    float temperature = temperature_result.value;
    float pressure = pressure_result.value;

    printf(">temperature: %.1f °C, >analog_in: %.3f V, >pulses: %u\r\n", temperature, pressure, (uint16_t)(pulse_count - prev_pulse_count));


    temperature_sum += temperature;
    pressure_sum += pressure;
    sum_counter++; // doesn't increment in case of read error

    prev_pulse_count = pulse_count;

    BlinkLed(1, true);
    if (DATA_COLLECTION_INTERVAL_MS > 2 * LED_BLINK_DELAY)
    {
      FLEX_DelayMs(DATA_COLLECTION_INTERVAL_MS - 2*LED_BLINK_DELAY);
    }
  }

  float avg_temperature = temperature_sum / sum_counter;
  float avg_pressure_ain = pressure_sum / sum_counter;

  // Map avg_pressure_ain from 0.5-4.5 volts to 0-5 bar
  float avg_pressure;
  if (avg_pressure_ain < 0.3 || avg_pressure_ain > 4.5)
  {
    printf("reading out of range (%.2fV)\r\n", avg_pressure_ain);
    avg_pressure = -1; // Indicate an error
  }
  else
  {
    float calib_ain = 0.05;                                                    // Calibration value
    avg_pressure = (avg_pressure_ain - calib_ain - 0.5) * (5.0 / (4.5 - 0.5)); // Linear mapping
  }

  // Stop flow meter pulse counting
  FlowMeterData flow_data = StopFlowMeterPulseCounting();
  flow_data.pulse_count -= 1; // Adjust for the initial pulse

  // Calculate flow rate (pulses per minute)
  uint32_t pulses_per_minute = (flow_data.pulse_count * 60000) / flow_data.elapsed_time_ms;

  // Print results
  printf("Average Temperature: %.1f °C\r\n", avg_temperature);
  printf("Average AIN: %.3f V\r\n", avg_pressure_ain);
  printf("Average Pressure: %.3f bar\r\n", avg_pressure);
  printf("Pulse Rate: %.2ld pulses/min\r\n", pulses_per_minute);

  SensorMeasurements measurements = {0};
  measurements.temperature = (int16_t)(avg_temperature * 10 + 0.5); // Round to nearest 0.1 and convert to tenths of degrees
  measurements.analog_in = (uint16_t)(avg_pressure_ain * 1000);     // Convert to millivolts
  measurements.pulse_per_minute = (uint16_t)pulses_per_minute;
  // TODO assign ret_temp - error code for sensor interfacing
  return measurements;
}

static int InitDevice(void)
{
  // if (ENABLE_MODBUS && Modbus_Init() != 0)
  // {
  //   printf("Failed to Init Modbus.\r\n");
  //   return -1;
  // }
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

  if (bInitModbusRequired && Modbus_Init() != 0)
  {
    printf("Failed to Init Modbus.\r\n");
    // return -1;
  }
  else {
    bInitModbusRequired = false;
    printf("Modbus initialised.\r\n");
  }

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

// function to blink LED n times, defined by argument
static void BlinkLed(int count, bool inverse)
{ 
  for (int i = 0; i < count; i++)
  {
    if (inverse)
    {
      FLEX_LEDGreenStateSet(FLEX_LED_OFF);
      // printf("Green LED Off\n");
    }
    else
    {
      FLEX_LEDGreenStateSet(FLEX_LED_ON);
      // printf("Green LED On\n");
    }
    FLEX_DelayMs(LED_BLINK_DELAY);
    if (inverse)
    {
      FLEX_LEDGreenStateSet(FLEX_LED_ON);
      // printf("Green LED On\n");
    }
    else
    {
      FLEX_LEDGreenStateSet(FLEX_LED_OFF);
      // printf("Green LED Off\n");
    }
    FLEX_DelayMs(LED_BLINK_DELAY);
  }
}

static time_t ScheduleNextRun(void)
{
  // Schedule next run in 1 hour since wakeup time
  time_t wakeup_time = FLEX_TimeGet();
  time_t next_run_time = wakeup_time + 30;

  FLEX_LEDGreenStateSet(FLEX_LED_ON);
  printf("Green LED On\n");

  // Init sensors
  if (InitSensors())
  {
    printf("Aborting Init Sensors, retry in 10 seconds\r\n");
    next_run_time = wakeup_time + 10; // Retry in 10 seconds
  }
  else
  {
    printf("Sensors initialised\r\n");
    BlinkLed(1, true);
    SensorMeasurements measurements = CollectSensorData();
    printf("Sensor data collected\r\n");
    if (ENABLE_TRANSMIT)
    {
      printf("Making message...\r\n");
      Message message = MakeMessage(measurements);
      int ret = send_message(message);
      printf("send_message returned: %d\n", ret);
      printf("Message sent\r\n");
      next_run_time = wakeup_time + NEXT_RUN_INTERVAL_SEC;
      printf("Next run time: %lu\n", (uint32_t)next_run_time);
    }
  }
  DeinitSensors();

  FLEX_LEDGreenStateSet(FLEX_LED_OFF);
  return next_run_time;
}
static Message MakeMessage(SensorMeasurements measurements)
{
  static uint8_t sequence_number = 0;

  Message message = {0};
  message.sequence_number = sequence_number++;
  message.time = FLEX_TimeGet();
  message.temperature = (int16_t)measurements.temperature;
  message.analog_in = (uint16_t)measurements.analog_in;
  message.pulse_per_minute = (uint16_t)measurements.pulse_per_minute;

  //   int32_t latitude = 0;
  //   int32_t longitude = 0;
  //   FLEX_LastLocationAndLastFixTime(&latitude, &longitude, NULL);
  //   message.latitude = latitude;
  //   message.longitude = longitude;

  return message;
}

static int send_message(Message message)
{
  //   FLEX_LastLocationAndLastFixTime(&latitude, &longitude, NULL);
  //   message.latitude = latitude;
  //   message.longitude = longitude;

  //   int16_t temperature = 0;
  //   int16_t humidity = 0;
  //   read_temperature_and_humidity(&temperature, &humidity);
  //   message.temperature = temperature;
  //   message.humidity = humidity;

  // Schedule messages for satellite transmission
  int ret = FLEX_MessageSchedule((const uint8_t *const)&message, sizeof(message));
  printf("Message scheduling returned: %d\n", ret);
  printf("Scheduled message: \n");

  printf("  Sequence Number: %u\n", message.sequence_number);
  printf("  Timestamp: %lu\n", message.time);
  printf("  Temperature: %d /10 °C\n", message.temperature);
  printf("  Analog Input (Pressure): %u mV\n", message.analog_in);
  printf("  Flow Rate (Pulses/Minute): %u\n", message.pulse_per_minute);

  return ret;
}

void FLEX_AppInit()
{
  printf("%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\n", __DATE__, __TIME__);
  InitDevice();
  FLEX_JobSchedule(ScheduleNextRun, FLEX_ASAP());
}