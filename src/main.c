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
#define SENSOR_STABILISE_DELAY_MS 5000
// #define DATA_COLLECTION_DURATION_SEC 10
#define DATA_COLLECTION_INTERVAL_MS 1000
#define SENSOR_READINGS_COUNT 5

#define INTERVAL_WAKEUP_DEFAULT 30       // 30 seconds
#define INTERVAL_WAKEUP_TRANSMIT 60 * 60 // 1 hour
// #define INTERVAL_WAKEUP_TEST 10 // 10 seconds

#define ENABLE_TRANSMIT 0
#define ENABLE_MODBUS 1
/** Set to 1 to scan Modbus slaves 0x01..0x0F after init. Tune MODBUS_SCAN_SETTLE_MS to find minimal delay. */
#define MODBUS_SCAN_AFTER_INIT 1
#define MODBUS_SCAN_SETTLE_MS  50  /* minimal delay after Modbus_Init before scan (tune down to 0 to find minimum) */
/** Set to 1 to run all Modbus diagnostic tests sequentially at startup (power + Modbus only). */
#define MODBUS_DIAGNOSTIC_TEST 1
bool bInitModbusRequired = true;
#define LED_BLINK_DELAY 200      // ms

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

typedef struct
{
  uint8_t sequence_number;
  uint32_t time;
  // int32_t latitude;
  // int32_t longitude;
  int16_t temperature;
  uint16_t analog_in;
  uint16_t pulse_per_minute;
  uint8_t error_code;
} __attribute__((packed)) Message;

typedef enum
{
  SENSOR_ERROR_NONE = 0x00, // No error
  SENSOR_ERROR_TEMP = 0x01, // Temperature sensor error
  SENSOR_ERROR_AIN = 0x02   // Analog input sensor error
} SensorError;

typedef struct
{
  int16_t return_code;
  float value;
} ReadResult;

static Message MakeMessage(SensorMeasurements measurements);
static int send_message(Message message);
static void BlinkLed(int count);
static uint16_t GetPulseRate(void);
static time_t ScheduleNextRun(void);

// Arrays to store sensor readings
// static float temperature_readings[SENSOR_READINGS_COUNT];
// static float pressure_readings[SENSOR_READINGS_COUNT];
// static uint32_t flow_meter_pulse_count = 0;

// Simulated functions for temperature, pressure, and flow meter
static ReadResult ReadTemperatureSensor(void)
{
  float temperature = MODBUS_TEMPERATURE_INVALID;
  int result = 0;
  if (ENABLE_MODBUS)
  {
    result = Modbus_Request_Receive_Temperature(&temperature);
    if (result)
      printf("Failed to Read Temperature from Modbus sensor (result=%d).\r\n", result);
  }
  else
  {
    temperature = 25.0f;
    result = 0;
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

uint32_t pulse_count_start_tick;
uint32_t pulse_count_end_tick;

static void StartFlowMeterTimer(void)
{
  // Initialise to generate event every N pulses
  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS))
  {
    printf("Failed to initialise pulse counter\n");
  }
  else
  {
    printf("Pulse counter initialised.\r\n");
  }
  // Start counting pulses from the flow meter
  pulse_count_start_tick = FLEX_TickGet(); // Use FLEX SDK function to get start tick
  printf("Pulse counting started at tick: %ld\r\n", pulse_count_start_tick);
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

  FLEX_PulseCounterDeinit(); // Deinitialise the pulse counter
  printf("Pulse counter deinitialised.\r\n");

  FlowMeterData data = {flow_meter_pulse_count, elapsed_time_ms};
  return data;
}

static uint16_t GetPulseRate(void)
{
  // Replace with actual logic to get current pulse rate
  uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
  uint16_t pulse_rate = (uint16_t)(1000.0 / (FLEX_TickGet() - pulse_count_start_tick) * pulse_count); // pulses per second
  return pulse_rate;
}

static SensorMeasurements CollectSensorData(void)
{
  // Calculate averages
  float temperature_sum = 0.0, pressure_sum = 0.0;
  unsigned int sum_counter_pres = 0;
  unsigned int sum_counter_temp = 0;
  int16_t err_temp = 0;
  int16_t err_ain = 0;
  // int16_t err_pulse = 0;
  // Collect temperature and pressure readings
  for (int i = 0; i < SENSOR_READINGS_COUNT; i++)
  {
    printf("Collecting sensor data...\r\n");
    uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
    uint16_t pulse_rate = GetPulseRate();
    ReadResult temperature_result = ReadTemperatureSensor();
    ReadResult pressure_result = ReadPressureSensor();

    if (temperature_result.return_code)
    {
      printf("Error reading temperature sensor\r\n");
      err_temp = temperature_result.return_code;
    }
    else
    {
      sum_counter_temp++;
      temperature_sum += temperature_result.value;
      err_temp = 0;
    }

    if (pressure_result.return_code)
    {
      printf("Error reading pressure sensor\r\n");
      err_ain = pressure_result.return_code;
    }
    else
    {
      sum_counter_pres++;
      pressure_sum += pressure_result.value;
      err_ain = 0;
    }

    if (pressure_result.return_code || temperature_result.return_code)
    {
      BlinkLed(3);
    }
    else
    {
      BlinkLed(1);
    }

    float temperature = temperature_result.value;
    float pressure = pressure_result.value;

    if (isnan(temperature))
      printf(">temperature: N/A °C, >analog_in: %.3f V, >pulses: %ld, >pulse_rate: %u\r\n", pressure, pulse_count, pulse_rate);
    else
      printf(">temperature: %.1f °C, >analog_in: %.3f V, >pulses: %ld, >pulse_rate: %u\r\n", temperature, pressure, pulse_count, pulse_rate);

    if (DATA_COLLECTION_INTERVAL_MS > 2 * LED_BLINK_DELAY)
    {
      FLEX_DelayMs(DATA_COLLECTION_INTERVAL_MS - 2 * LED_BLINK_DELAY);
    }
  }

  float avg_temperature = sum_counter_temp ? temperature_sum / sum_counter_temp : 0;
  float avg_pressure_ain = sum_counter_pres ? pressure_sum / sum_counter_pres : 0;

  // Map avg_pressure_ain from 0.5-4.5 volts to 0-5 bar
  float avg_pressure;
  if (avg_pressure_ain < 0.3 || avg_pressure_ain > 5)
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
  if (flow_data.pulse_count)
  {
    flow_data.pulse_count -= 1; // Adjust for the initial pulse
  }

  // Calculate flow rate (pulses per minute)
  uint32_t pulses_per_minute = (uint32_t)(flow_data.pulse_count * (60000.0 / flow_data.elapsed_time_ms));

  // Print results
  if (sum_counter_temp)
    printf("Average Temperature: %.1f °C\r\n", avg_temperature);
  else
    printf("Average Temperature: N/A (no valid Modbus reads)\r\n");
  printf("Average AIN: %.3f V\r\n", avg_pressure_ain);
  printf("Average Pressure: %.3f bar\r\n", avg_pressure);
  printf("Pulse Rate: %.2ld pulses/min\r\n", pulses_per_minute);

  SensorMeasurements measurements = {0};
  measurements.temperature = (int16_t)(avg_temperature * 10 + 0.5); // Round to nearest 0.1 and convert to tenths of degrees
  measurements.analog_in = (uint16_t)(avg_pressure_ain * 1000);     // Convert to millivolts
  measurements.pulse_per_minute = (uint16_t)pulses_per_minute;
  measurements.ret_temp = (uint8_t)err_temp;
  measurements.ret_ain = (uint8_t)err_ain;

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

  if (ENABLE_MODBUS && bInitModbusRequired)
  {
    if (Modbus_Init() != 0)
      printf("Failed to Init Modbus.\r\n");
    else
    {
      bInitModbusRequired = false;
      printf("Modbus initialised.\r\n");
#if MODBUS_SCAN_AFTER_INIT
      if (MODBUS_SCAN_SETTLE_MS > 0)
        FLEX_DelayMs(MODBUS_SCAN_SETTLE_MS);
      {
        uint8_t slave = 0;
        float scan_temp = MODBUS_TEMPERATURE_INVALID;
        Modbus_ScanForTemperatureSensor(&slave, &scan_temp);
      }
#endif
    }
  }

  FLEX_DelayMs(SENSOR_FLOW_METER_STABILISE_DELAY_MS);

  StartFlowMeterTimer();

  // Wait for sensors to stabilize
  FLEX_DelayMs(SENSOR_STABILISE_DELAY_MS);
  return 0;
}

static void DeinitSensors(void)
{
  if (ENABLE_MODBUS)
  {
    printf("Modbus_Deinit: disabling...\r\n");
    int r = Modbus_Deinit();
    printf("Modbus_Deinit: %s\r\n", r ? "done (with error)" : "done");
    bInitModbusRequired = true;
  }
  FLEX_AnalogInputDeinit();
  FLEX_PowerOutDeinit();
  FLEX_PulseCounterDeinit();
}

// function to blink LED n times, defined by argument
static void BlinkLed(int count)
{
  bool inverse = false;
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

#if MODBUS_DIAGNOSTIC_TEST
static int TryOneModbusRead(float *out_temperature)
{
  float t = MODBUS_TEMPERATURE_INVALID;
  int r = Modbus_Request_Receive_Temperature(&t);
  if (out_temperature)
    *out_temperature = t;
  return r;
}

static time_t RunModbusDiagnostic(void)
{
  time_t next_run_time = FLEX_TimeGet() + INTERVAL_WAKEUP_DEFAULT;
  float t;
  uint32_t t0, t1;

  printf("\r\n");
  printf("========================================\r\n");
  printf("MODBUS DIAGNOSTIC – follow CONNECT/DISCONNECT prompts\r\n");
  printf("========================================\r\n");

  /* Test 1: Power cycle, Modbus stays init. Tests: reconnect before power-on. */
  printf("\r\n=== Test 1: Power cycle sensor, Modbus stays init ===\r\n");
  printf(">>> CONNECT sensor (cable plugged, will be powered by board)\r\n");
  FLEX_DelayMs(3000);
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
    printf("PowerOutInit failed\r\n");
  else if (Modbus_Init() != 0)
    printf("Modbus_Init failed\r\n");
  else
  {
    bInitModbusRequired = false;
    FLEX_DelayMs(200);
    if (TryOneModbusRead(&t))
      printf("Initial read failed\r\n");
    else
      printf("Initial read OK: %.1f °C\r\n", (double)t);
    for (int cycle = 0; cycle < 3; cycle++)
    {
      printf(">>> DISCONNECT sensor NOW (unplug cable). Power off in 5s...\r\n");
      FLEX_DelayMs(5000);
      FLEX_PowerOutDeinit();
      FLEX_DelayMs(2000);
      printf(">>> RECONNECT sensor NOW (plug cable). Power on in 3s...\r\n");
      FLEX_DelayMs(3000);
      FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
      FLEX_DelayMs(100);
      if (TryOneModbusRead(&t))
        printf("  Cycle %d: read FAIL after 100ms settle\r\n", cycle + 1);
      else
        printf("  Cycle %d: read OK %.1f °C\r\n", cycle + 1, (double)t);
    }
    Modbus_Deinit();
    bInitModbusRequired = true;
    FLEX_PowerOutDeinit();
  }

  /* Test 2: Init Modbus with sensor unpowered. Correct order for cold start. */
  printf("\r\n=== Test 2: Init Modbus WITHOUT sensor powered (then power + read) ===\r\n");
  printf(">>> DISCONNECT sensor OR leave unpowered. Modbus init in 3s...\r\n");
  FLEX_DelayMs(3000);
  if (Modbus_Init() != 0)
    printf("Modbus_Init (sensor off) failed\r\n");
  else
  {
    bInitModbusRequired = false;
    printf(">>> RECONNECT sensor NOW. Power on in 3s...\r\n");
    FLEX_DelayMs(3000);
    FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
    FLEX_DelayMs(200);
    t0 = FLEX_TickGet();
    int r = TryOneModbusRead(&t);
    t1 = FLEX_TickGet();
    printf("Read: %s, %.1f °C, duration %lu ticks\r\n", r ? "FAIL" : "OK", (double)t, (unsigned long)(t1 - t0));
    Modbus_Deinit();
    bInitModbusRequired = true;
    FLEX_PowerOutDeinit();
  }

  /* Test 3: Min settle time – sensor power up, Modbus already init. */
  printf("\r\n=== Test 3: Min settle calibration (Modbus already init) ===\r\n");
  printf(">>> CONNECT sensor. We power on, then power-cycle and try 0,25,50...ms settle.\r\n");
  FLEX_DelayMs(3000);
  FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
  if (Modbus_Init() != 0)
  {
    printf("Modbus_Init failed\r\n");
    FLEX_PowerOutDeinit();
  }
  else
  {
    bInitModbusRequired = false;
    FLEX_DelayMs(500);
    TryOneModbusRead(&t);
    static const unsigned int settle_list[] = {0, 25, 50, 75, 100, 150, 200, 300, 500};
    for (size_t i = 0; i < sizeof(settle_list) / sizeof(settle_list[0]); i++)
    {
      unsigned int settle_ms = settle_list[i];
      printf("  Power off 500ms, power on, wait %u ms...\r\n", settle_ms);
      FLEX_PowerOutDeinit();
      FLEX_DelayMs(500);
      FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
      FLEX_DelayMs(settle_ms);
      if (!TryOneModbusRead(&t))
      {
        printf("Min settle: %u ms -> read OK %.1f °C\r\n", settle_ms, (double)t);
        break;
      }
      printf("  settle %u ms: fail\r\n", settle_ms);
    }
    Modbus_Deinit();
    bInitModbusRequired = true;
    FLEX_PowerOutDeinit();
  }

  /* Test 4: Min settle with Modbus reinit each power cycle. */
  printf("\r\n=== Test 4: Min settle with Modbus REINIT each cycle ===\r\n");
  printf(">>> CONNECT sensor. Each trial: power off, Modbus deinit, power on, Modbus init, settle, read.\r\n");
  FLEX_DelayMs(3000);
  {
    static const unsigned int settle_list[] = {0, 25, 50, 75, 100, 150, 200, 300, 500};
    for (size_t i = 0; i < sizeof(settle_list) / sizeof(settle_list[0]); i++)
    {
      unsigned int settle_ms = settle_list[i];
      FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
      FLEX_DelayMs(50);
      Modbus_Deinit();
      bInitModbusRequired = true;
      FLEX_PowerOutDeinit();
      FLEX_DelayMs(500);
      FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
      if (Modbus_Init() != 0)
      {
        printf("  settle %u ms: Modbus_Init failed\r\n", settle_ms);
        FLEX_PowerOutDeinit();
        continue;
      }
      bInitModbusRequired = false;
      FLEX_DelayMs(settle_ms);
      if (!TryOneModbusRead(&t))
      {
        printf("Min settle (reinit): %u ms -> read OK %.1f °C\r\n", settle_ms, (double)t);
        Modbus_Deinit();
        bInitModbusRequired = true;
        FLEX_PowerOutDeinit();
        break;
      }
      printf("  settle %u ms: fail\r\n", settle_ms);
      Modbus_Deinit();
      bInitModbusRequired = true;
      FLEX_PowerOutDeinit();
    }
  }

  /* Test 5: Max sampling rate */
  printf("\r\n=== Test 5: Max sampling rate ===\r\n");
  printf(">>> CONNECT sensor (if not already).\r\n");
  FLEX_DelayMs(2000);
  FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
  if (Modbus_Init() != 0)
  {
    printf("Modbus_Init failed\r\n");
    FLEX_PowerOutDeinit();
  }
  else
  {
    bInitModbusRequired = false;
    FLEX_DelayMs(300);
    t0 = FLEX_TickGet();
    int r = TryOneModbusRead(&t);
    t1 = FLEX_TickGet();
    printf("Single read: %s, %.1f °C, duration %lu ticks\r\n", r ? "FAIL" : "OK", (double)t, (unsigned long)(t1 - t0));
    static const unsigned int intervals[] = {0, 50, 100, 200, 500};
    for (size_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
    {
      unsigned int interval_ms = intervals[i];
      unsigned int ok = 0, fail = 0;
      uint32_t sum_ticks = 0;
      for (int k = 0; k < 5; k++)
      {
        if (interval_ms && k > 0)
          FLEX_DelayMs(interval_ms);
        t0 = FLEX_TickGet();
        int res = TryOneModbusRead(&t);
        t1 = FLEX_TickGet();
        sum_ticks += (t1 - t0);
        if (res) fail++; else ok++;
      }
      printf("  interval %u ms: %u OK, %u fail, avg duration %lu ticks\r\n",
          interval_ms, ok, fail, (unsigned long)(sum_ticks / 5));
    }
    Modbus_Deinit();
    bInitModbusRequired = true;
    FLEX_PowerOutDeinit();
  }

  /* Test 6: Sleep/wake with sensor DISCONNECTED (reproduces FlexSense sleep/wake issue) */
  printf("\r\n=== Test 6: Sleep/wake with sensor DISCONNECTED ===\r\n");
  printf("Simulates: measure -> deinit (sleep) with sensor unplugged -> next wake init -> read.\r\n");
  printf(">>> CONNECT sensor. We do one read, then you DISCONNECT before deinit.\r\n");
  FLEX_DelayMs(3000);
  FLEX_PowerOutInit(SENSOR_POWER_SUPPLY);
  if (Modbus_Init() != 0)
    printf("Modbus_Init failed\r\n");
  else
  {
    bInitModbusRequired = false;
    FLEX_DelayMs(200);
    if (TryOneModbusRead(&t))
      printf("Read before deinit: FAIL\r\n");
    else
      printf("Read before deinit: OK %.1f °C\r\n", (double)t);
    printf(">>> DISCONNECT sensor NOW. Deinit in 5s (simulating sleep)...\r\n");
    FLEX_DelayMs(5000);
    printf("Modbus_Deinit: disabling...\r\n");
    Modbus_Deinit();
    bInitModbusRequired = true;
    printf("Modbus_Deinit: done. Power off.\r\n");
    FLEX_PowerOutDeinit();
    printf(">>> RECONNECT sensor NOW. Next run will Init and read (in ~30s or when job runs).\r\n");
  }

  printf("\r\n=== Modbus diagnostics done. Next: ScheduleNextRun (full init + collect). ===\r\n");
  FLEX_JobSchedule(ScheduleNextRun, FLEX_ASAP());
  return next_run_time;
}
#endif

static time_t ScheduleNextRun(void)
{
  // Schedule next run in 1 hour since wakeup time
  time_t wakeup_time = FLEX_TimeGet();
  time_t next_run_time = wakeup_time + INTERVAL_WAKEUP_DEFAULT;

  // FLEX_LEDGreenStateSet(FLEX_LED_ON);
  // printf("Green LED On\n");
  BlinkLed(5);

  // Init sensors
  if (InitSensors())
  {
    printf("Failed Init Sensors\n");
  }
  else
  {
    printf("Sensors initialised\r\n");
    SensorMeasurements measurements = CollectSensorData();
    printf("Sensor data collected\r\n");
    if (ENABLE_TRANSMIT)
    {
      printf("Making message...\r\n");
      Message message = MakeMessage(measurements);
      int ret = send_message(message);
      printf("Message sent with result: %d\r\n", ret);
      next_run_time = wakeup_time + INTERVAL_WAKEUP_TRANSMIT;
      BlinkLed(5);
    }
  }
  printf("Deinitialising sensors...\r\n");
  DeinitSensors();

  // FLEX_LEDGreenStateSet(FLEX_LED_OFF);
  printf("Next run in %ld seconds\r\n", (int32_t)(next_run_time - FLEX_TimeGet()));
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

  if (measurements.ret_temp)
  {
    message.error_code |= SENSOR_ERROR_TEMP;
  }
  if (measurements.ret_ain)
  {
    message.error_code |= SENSOR_ERROR_AIN;
  }

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
  printf("Nilus App dev_v04\r\n");
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);
  InitDevice();
#if MODBUS_DIAGNOSTIC_TEST
  FLEX_JobSchedule(RunModbusDiagnostic, FLEX_ASAP());
#else
  FLEX_JobSchedule(ScheduleNextRun, FLEX_ASAP());
#endif
}