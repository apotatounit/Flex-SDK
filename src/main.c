/*
 * Init order for reliable Modbus reads (minimal time):
 *   Power on -> POWER_SETTLE_MS -> Modbus_Init -> SERIAL_SETTLE_MS -> read(s).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "flex.h"
#include <time.h>
#include "modbussensor.h"

#define APPLICATION_NAME "Sensor Pipeline Logger"

// Sensor configuration
#define SENSOR_POWER_SUPPLY       FLEX_POWER_OUT_5V
#define POWER_SETTLE_MS           800u   /* ms after power-on before Modbus_Init (sensor stabilise) */
#define SERIAL_SETTLE_MS          150u   /* ms after Modbus_Init before first read */
#define ANALOG_IN_MODE            FLEX_ANALOG_IN_VOLTAGE
#define PULSE_WAKEUP_COUNT        0

#define SENSOR_FLOW_METER_STABILISE_DELAY_MS 100
#define SENSOR_STABILISE_DELAY_MS 0   /* rely on POWER_SETTLE_MS + SERIAL_SETTLE_MS for Modbus */
#define TARGET_COLLECTION_DURATION_MS 20000u  /* entire read cycle must fit in 20 s */
#define SENSOR_READINGS_COUNT 7u
#define DATA_COLLECTION_INTERVAL_MS (TARGET_COLLECTION_DURATION_MS / SENSOR_READINGS_COUNT)  /* ~2857 ms between start of each sample */

#define INTERVAL_WAKEUP_TRANSMIT (60 * 60) /* 1 hour: wake every hour, read 20 s, schedule message, sleep */

#define ENABLE_TRANSMIT 1
#define ENABLE_MODBUS 1
bool bInitModbusRequired = true; // only required on first init after power supply init
#define LED_BLINK_RAPID_MS 50    /* ms on/off for rapid startup blink */
/* Solid green = sampling OK; brief fast OFF pulses = sensor fault (1 temp, 2 analog, 3 both). */
#define LED_GLITCH_OFF_MS 45u
#define LED_GLITCH_ON_MS 40u
#define LED_RUN_OK_HOLD_MS 400u
#define LED_RUN_FAIL_GLITCHES 4u

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
static void BlinkLedRapid(int count);
static void LedGlitchOffPulses(unsigned count);
static void IndicatePerSampleReads(bool temp_ok, bool ain_ok);
static uint16_t GetPulseRate(void);

/* Returns temperature and, when elapsed_ms != NULL, writes read duration in ms. */
static ReadResult ReadTemperatureSensor(uint32_t *elapsed_ms)
{
  float temperature = MODBUS_TEMPERATURE_INVALID;
  int result = 0;
  uint32_t t0 = FLEX_TickGet();
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
  if (elapsed_ms != NULL)
    *elapsed_ms = FLEX_TickGet() - t0;  /* ticks; 1000 ticks = 1 s on this platform */
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
  uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
  uint32_t elapsed_ticks = FLEX_TickGet() - pulse_count_start_tick;
  if (elapsed_ticks == 0u)
    return 0u;
  return (uint16_t)((1000.0 * (double)pulse_count) / (double)elapsed_ticks);  /* pulses per second; 1000 ticks = 1 s */
}

static SensorMeasurements CollectSensorData(void)
{
  FLEX_LEDGreenStateSet(FLEX_LED_ON); /* baseline: on while collecting */

  // Calculate averages
  float temperature_sum = 0.0, pressure_sum = 0.0;
  unsigned int sum_counter_pres = 0;
  unsigned int sum_counter_temp = 0;
  int16_t err_temp = 0;
  int16_t err_ain = 0;
  for (unsigned int i = 0u; i < SENSOR_READINGS_COUNT; i++)
  {
    uint32_t sample_start = FLEX_TickGet();
    printf("Collecting sensor data...\r\n");
    uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
    uint16_t pulse_rate = GetPulseRate();
    ReadResult temperature_result = ReadTemperatureSensor(NULL);
    ReadResult pressure_result = ReadPressureSensor();

    if (i == 0u)
    {
      if (temperature_result.return_code)
        printf("[debug] First Modbus read: FAIL (result=%d)\r\n", temperature_result.return_code);
      else
        printf("[debug] First Modbus read: OK %.1f °C (settle timeouts sufficient)\r\n", (double)temperature_result.value);
    }
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

    IndicatePerSampleReads(temperature_result.return_code == 0, pressure_result.return_code == 0);

    float temperature = temperature_result.value;
    float pressure = pressure_result.value;

    if (isnan(temperature))
      printf(">temperature: N/A °C, >analog_in: %.3f V, >pulses: %lu, >pulse_rate: %u\r\n", (double)pressure, (unsigned long)pulse_count, pulse_rate);
    else
      printf(">temperature: %.1f °C, >analog_in: %.3f V, >pulses: %lu, >pulse_rate: %u\r\n", (double)temperature, (double)pressure, (unsigned long)pulse_count, pulse_rate);

    /* Elapsed includes read + blink + printf; delay so full cycle fits in DATA_COLLECTION_INTERVAL_MS */
    uint32_t sample_elapsed_ticks = FLEX_TickGet() - sample_start;
    uint32_t sample_elapsed_ms = sample_elapsed_ticks;  /* 1000 ticks = 1 s */
    int delay_ms = (int)DATA_COLLECTION_INTERVAL_MS - (int)sample_elapsed_ms;
    if (delay_ms > 0)
      FLEX_DelayMs((uint32_t)delay_ms);
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
  return measurements;
}

static int InitDevice(void)
{
  return 0;
}

static int InitSensors(void)
{
  printf("Initialising sensors (power->%u ms->Modbus_Init->%u ms->read)...\r\n",
         (unsigned)POWER_SETTLE_MS, (unsigned)SERIAL_SETTLE_MS);
  /* 1. Power on */
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("Failed to enable sensor power supply.\r\n");
    return -1;
  }
  printf("[init] Power on OK, waiting POWER_SETTLE_MS=%u ms...\r\n", (unsigned)POWER_SETTLE_MS);
  FLEX_DelayMs(POWER_SETTLE_MS);
  printf("[init] Power settle done.\r\n");

  if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
  {
    printf("Failed to Init Analog Input.\r\n");
    return -1;
  }
  printf("Analog Input initialised.\r\n");

  /* 2. Modbus init after power settle; then serial settle before first read */
  if (ENABLE_MODBUS && bInitModbusRequired)
  {
    if (Modbus_Init() != 0)
    {
      printf("Failed to Init Modbus.\r\n");
      return -1;
    }
    bInitModbusRequired = false;
    printf("[init] Modbus_Init OK, waiting SERIAL_SETTLE_MS=%u ms...\r\n", (unsigned)SERIAL_SETTLE_MS);
    FLEX_DelayMs(SERIAL_SETTLE_MS);
    printf("[init] Serial settle done. Ready for first read.\r\n");
  }

  FLEX_DelayMs(SENSOR_FLOW_METER_STABILISE_DELAY_MS);
  StartFlowMeterTimer();
  if (SENSOR_STABILISE_DELAY_MS > 0u)
    FLEX_DelayMs(SENSOR_STABILISE_DELAY_MS);
  return 0;
}

static void DeinitSensors(void)
{
  if (ENABLE_MODBUS)
  {
    Modbus_Deinit();
    bInitModbusRequired = true;
  }
  FLEX_AnalogInputDeinit();
  FLEX_PowerOutDeinit();
  FLEX_PulseCounterDeinit();
}

static void LedGlitchOffPulses(unsigned count)
{
  for (unsigned i = 0u; i < count; i++)
  {
    FLEX_LEDGreenStateSet(FLEX_LED_OFF);
    FLEX_DelayMs(LED_GLITCH_OFF_MS);
    FLEX_LEDGreenStateSet(FLEX_LED_ON);
    if (i + 1u < count)
      FLEX_DelayMs(LED_GLITCH_ON_MS);
  }
}

/* Solid ON; only dips briefly on failure — 1 glitch = Modbus/temp, 2 = analog, 3 = both. */
static void IndicatePerSampleReads(bool temp_ok, bool ain_ok)
{
  FLEX_LEDGreenStateSet(FLEX_LED_ON);
  unsigned pulses = 0u;
  if (!temp_ok && !ain_ok)
    pulses = 3u;
  else if (!temp_ok)
    pulses = 1u;
  else if (!ain_ok)
    pulses = 2u;
  LedGlitchOffPulses(pulses);
}

static void BlinkLedRapid(int count)
{
  for (int i = 0; i < count; i++)
  {
    FLEX_LEDGreenStateSet(FLEX_LED_ON);
    FLEX_DelayMs(LED_BLINK_RAPID_MS);
    FLEX_LEDGreenStateSet(FLEX_LED_OFF);
    FLEX_DelayMs(LED_BLINK_RAPID_MS);
  }
}

static time_t ScheduleNextRun(void)
{
  time_t wakeup_time = FLEX_TimeGet();
  time_t next_run_time = wakeup_time + INTERVAL_WAKEUP_TRANSMIT;

  BlinkLedRapid(2);   /* 2 rapid blinks on startup */
  FLEX_LEDGreenStateSet(FLEX_LED_ON); /* solid baseline before init / collect */

  if (InitSensors() != 0)
  {
    printf("Failed Init Sensors\n");
    LedGlitchOffPulses(5u); /* init failure: five fast dips */
  }
  else
  {
    printf("Sensors initialised, collecting %u samples...\r\n", (unsigned)SENSOR_READINGS_COUNT);
    SensorMeasurements measurements = CollectSensorData();
    printf("Sensor data collected\r\n");
    if (ENABLE_TRANSMIT)
    {
      printf("Making message...\r\n");
      Message message = MakeMessage(measurements);
      int ret = send_message(message);
      printf("Message sent with result: %d\r\n", ret);
      FLEX_LEDGreenStateSet(FLEX_LED_ON);
      if (message.error_code != 0)
        LedGlitchOffPulses(LED_RUN_FAIL_GLITCHES); /* fast dips = run had sensor faults */
      else
        FLEX_DelayMs(LED_RUN_OK_HOLD_MS); /* stay solid = all channels reported OK */
    }
  }
  printf("Deinitialising sensors...\r\n");
  DeinitSensors();

  printf("Next run in %ld seconds (1 hour)\r\n", (long)(next_run_time - FLEX_TimeGet()));
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
  return message;
}

static int send_message(Message message)
{
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
  printf("Nilus App release_v04\r\n");
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);
  InitDevice();
  FLEX_JobSchedule(ScheduleNextRun, FLEX_ASAP());
}