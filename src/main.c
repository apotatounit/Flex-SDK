#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"
#include "modbussensor.h"

#define APPLICATION_NAME "Sensor Timing Calibration"

// Test enable/disable configuration
#define ENABLE_PULSE_TEST 1
#define ENABLE_ANALOG_TEST 1
#define ENABLE_MODBUS_TEST 1

// Test range configuration
#define PULSE_POWERUP_DELAY_MIN_MS 0
#define PULSE_POWERUP_DELAY_MAX_MS 5000
#define PULSE_POWERUP_DELAY_STEP_MS 100

#define PULSE_READ_DELAY_MIN_MS 10
#define PULSE_READ_DELAY_MAX_MS 1000
#define PULSE_READ_DELAY_STEP_MS 50

#define ANALOG_POWERUP_DELAY_MIN_MS 0
#define ANALOG_POWERUP_DELAY_MAX_MS 5000
#define ANALOG_POWERUP_DELAY_STEP_MS 100

#define ANALOG_READ_DELAY_MIN_MS 1
#define ANALOG_READ_DELAY_MAX_MS 500
#define ANALOG_READ_DELAY_STEP_MS 10

#define MODBUS_POWERUP_DELAY_MIN_MS 0
#define MODBUS_POWERUP_DELAY_MAX_MS 10000
#define MODBUS_POWERUP_DELAY_STEP_MS 200

#define MODBUS_READ_DELAY_MIN_MS 10
#define MODBUS_READ_DELAY_MAX_MS 2000
#define MODBUS_READ_DELAY_STEP_MS 50

// Test parameters
#define PULSE_COUNT_WINDOW_MS 1000
#define PULSE_READ_WINDOW_MS 100
#define PULSE_READ_ITERATIONS 10
#define ANALOG_READ_COUNT_POWERUP 10
#define ANALOG_READ_COUNT_FREQ 100
#define MODBUS_READ_RETRIES 3
#define MODBUS_READ_ITERATIONS 20

// Validation thresholds
#define ANALOG_STD_DEV_THRESHOLD_MV 10
#define ANALOG_ALIVE_VOLTAGE_V 1.0
#define ANALOG_ALIVE_TOLERANCE_V 0.2
#define MODBUS_SUCCESS_RATE_THRESHOLD 95
#define INTER_CYCLE_DELAY_MS 5000

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE FLEX_ANALOG_IN_VOLTAGE
#define PULSE_WAKEUP_COUNT 0

typedef struct
{
  uint32_t timestamp_ms;
  uint32_t powerup_delay_ms;
  uint32_t read_delay_ms;
  bool sensor_alive;
  bool success;
  float value;
  uint32_t duration_ms;
} MeasurementResult;

typedef struct
{
  float mean;
  float median;
  float std_dev;
  float min;
  float max;
  uint32_t count;
} Statistics;

static uint32_t GetTickMs(void)
{
  return FLEX_TickGet();
}

static void PowerOn(void)
{
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("ERROR: Failed to enable sensor power supply.\r\n");
  }
}

static void PowerOff(void)
{
  FLEX_PowerOutDeinit();
}

// Pulse Interrupt Sensor Tests
#if ENABLE_PULSE_TEST

static bool IsPulseSensorAlive(uint32_t pulse_count, float *rates, uint32_t rate_count)
{
  if (pulse_count <= 1)
    return false;

  if (rate_count < 2)
    return true; // Need at least 2 readings to check stability

  // Check if rates are relatively stable (variation < 20%)
  float avg_rate = 0;
  for (uint32_t i = 0; i < rate_count; i++)
  {
    avg_rate += rates[i];
  }
  avg_rate /= rate_count;

  float max_deviation = 0;
  for (uint32_t i = 0; i < rate_count; i++)
  {
    float deviation = fabsf(rates[i] - avg_rate) / avg_rate;
    if (deviation > max_deviation)
      max_deviation = deviation;
  }

  return max_deviation < 0.2; // 20% tolerance
}

static uint32_t TestPulsePowerUpDelay(void)
{
  printf("\r\n=== Pulse Sensor: Power-Up Delay Test ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n", 
         PULSE_POWERUP_DELAY_MIN_MS, PULSE_POWERUP_DELAY_MAX_MS, PULSE_POWERUP_DELAY_STEP_MS);

  uint32_t min_reliable_delay = UINT32_MAX;

  for (uint32_t delay = PULSE_POWERUP_DELAY_MIN_MS; delay <= PULSE_POWERUP_DELAY_MAX_MS; delay += PULSE_POWERUP_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(delay);

    uint32_t start_tick = GetTickMs();
    if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
    {
      printf("ERROR: Failed to init pulse counter\r\n");
      PowerOff();
      continue;
    }

    FLEX_DelayMs(PULSE_COUNT_WINDOW_MS);
    uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
    uint32_t end_tick = GetTickMs();
    uint32_t duration_ms = end_tick - start_tick;

    FLEX_PulseCounterDeinit();
    PowerOff();

    float rate = (pulse_count > 0) ? (pulse_count * 60000.0f / duration_ms) : 0;
    bool alive = IsPulseSensorAlive(pulse_count, &rate, 1);

    printf("PULSE_POWERUP: ts=%lu, delay=%lu, count=%lu, duration=%lu, rate=%.1f, alive=%s\r\n",
           GetTickMs(), delay, pulse_count, duration_ms, rate, alive ? "yes" : "no");

    if (alive && delay < min_reliable_delay)
    {
      min_reliable_delay = delay;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  if (min_reliable_delay != UINT32_MAX)
  {
    printf("RESULT: Minimum reliable power-up delay = %lu ms\r\n", min_reliable_delay);
  }
  else
  {
    printf("RESULT: No reliable delay found in range\r\n");
  }
}

static void TestPulseReadingFrequency(uint32_t min_powerup_delay)
{
  printf("\r\n=== Pulse Sensor: Reading Frequency Test ===\r\n");
  printf("Using power-up delay: %lu ms\r\n", min_powerup_delay);
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         PULSE_READ_DELAY_MIN_MS, PULSE_READ_DELAY_MAX_MS, PULSE_READ_DELAY_STEP_MS);

  uint32_t min_reliable_delay = UINT32_MAX;

  for (uint32_t delay = PULSE_READ_DELAY_MIN_MS; delay <= PULSE_READ_DELAY_MAX_MS; delay += PULSE_READ_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(min_powerup_delay);

    float rates[PULSE_READ_ITERATIONS];
    uint32_t counts[PULSE_READ_ITERATIONS];
    bool all_success = true;

    for (int i = 0; i < PULSE_READ_ITERATIONS; i++)
    {
      if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
      {
        all_success = false;
        break;
      }

      uint32_t start_tick = GetTickMs();
      FLEX_DelayMs(PULSE_READ_WINDOW_MS);
      counts[i] = (uint32_t)FLEX_PulseCounterGet();
      uint32_t end_tick = GetTickMs();
      uint32_t duration_ms = end_tick - start_tick;

      rates[i] = (counts[i] > 0) ? (counts[i] * 60000.0f / duration_ms) : 0;
      FLEX_PulseCounterDeinit();

      if (delay > 0)
        FLEX_DelayMs(delay);
    }

    PowerOff();

    bool alive = all_success && IsPulseSensorAlive(0, rates, PULSE_READ_ITERATIONS);

    float avg_count = 0;
    for (int i = 0; i < PULSE_READ_ITERATIONS; i++)
    {
      avg_count += counts[i];
    }
    avg_count /= PULSE_READ_ITERATIONS;

    printf("PULSE_FREQ: ts=%lu, delay=%lu, avg_count=%.1f, alive=%s\r\n",
           GetTickMs(), delay, avg_count, alive ? "yes" : "no");

    if (alive && delay < min_reliable_delay)
    {
      min_reliable_delay = delay;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  if (min_reliable_delay != UINT32_MAX)
  {
    printf("RESULT: Minimum reliable reading delay = %lu ms\r\n", min_reliable_delay);
  }
  else
  {
    printf("RESULT: No reliable delay found in range\r\n");
  }
}

#endif // ENABLE_PULSE_TEST

// Analog Input Sensor Tests
#if ENABLE_ANALOG_TEST

static void CalculateStatistics(float *values, uint32_t count, Statistics *stats)
{
  if (count == 0)
  {
    stats->mean = 0;
    stats->median = 0;
    stats->std_dev = 0;
    stats->min = 0;
    stats->max = 0;
    stats->count = 0;
    return;
  }

  // Calculate mean and min/max
  float sum = 0;
  stats->min = values[0];
  stats->max = values[0];
  for (uint32_t i = 0; i < count; i++)
  {
    sum += values[i];
    if (values[i] < stats->min)
      stats->min = values[i];
    if (values[i] > stats->max)
      stats->max = values[i];
  }
  stats->mean = sum / count;
  stats->count = count;

  // Calculate standard deviation
  float variance = 0;
  for (uint32_t i = 0; i < count; i++)
  {
    float diff = values[i] - stats->mean;
    variance += diff * diff;
  }
  stats->std_dev = sqrtf(variance / count);

  // Calculate median (simple sort for small arrays)
  float sorted[ANALOG_READ_COUNT_FREQ];
  for (uint32_t i = 0; i < count; i++)
    sorted[i] = values[i];
  // Simple bubble sort
  for (uint32_t i = 0; i < count - 1; i++)
  {
    for (uint32_t j = 0; j < count - i - 1; j++)
    {
      if (sorted[j] > sorted[j + 1])
      {
        float temp = sorted[j];
        sorted[j] = sorted[j + 1];
        sorted[j + 1] = temp;
      }
    }
  }
  stats->median = (count % 2 == 0) ? (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0f : sorted[count / 2];
}

static bool IsAnalogStable(Statistics *stats)
{
  return (stats->std_dev * 1000.0f < ANALOG_STD_DEV_THRESHOLD_MV) &&
         (stats->min >= 0.0f && stats->max <= 5.0f);
}

static bool IsAnalogAlive(Statistics *stats)
{
  float target = ANALOG_ALIVE_VOLTAGE_V;
  float tolerance = ANALOG_ALIVE_TOLERANCE_V;
  return (stats->mean >= target - tolerance) && (stats->mean <= target + tolerance);
}

static uint32_t TestAnalogPowerUpDelay(void)
{
  printf("\r\n=== Analog Input Sensor: Power-Up Delay Test ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         ANALOG_POWERUP_DELAY_MIN_MS, ANALOG_POWERUP_DELAY_MAX_MS, ANALOG_POWERUP_DELAY_STEP_MS);

  uint32_t min_reliable_delay = UINT32_MAX;

  for (uint32_t delay = ANALOG_POWERUP_DELAY_MIN_MS; delay <= ANALOG_POWERUP_DELAY_MAX_MS; delay += ANALOG_POWERUP_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(delay);

    if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
    {
      printf("ERROR: Failed to init analog input\r\n");
      PowerOff();
      continue;
    }

    float readings[ANALOG_READ_COUNT_POWERUP];
    for (int i = 0; i < ANALOG_READ_COUNT_POWERUP; i++)
    {
      uint32_t raw_mv = UINT32_MAX;
      if (FLEX_AnalogInputReadVoltage(&raw_mv) == 0)
      {
        readings[i] = raw_mv / 1000.0f; // Convert to volts
      }
      else
      {
        readings[i] = 0;
      }
      FLEX_DelayMs(1); // Minimal delay between readings
    }

    Statistics stats;
    CalculateStatistics(readings, ANALOG_READ_COUNT_POWERUP, &stats);
    bool stable = IsAnalogStable(&stats);
    bool alive = IsAnalogAlive(&stats);

    FLEX_AnalogInputDeinit();
    PowerOff();

    printf("ANALOG_POWERUP: ts=%lu, delay=%lu, mean=%.3fV, std_dev=%.3fV, min=%.3fV, max=%.3fV, stable=%s, alive=%s\r\n",
           GetTickMs(), delay, stats.mean, stats.std_dev, stats.min, stats.max,
           stable ? "yes" : "no", alive ? "yes" : "no");

    if (stable && delay < min_reliable_delay)
    {
      min_reliable_delay = delay;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  if (min_reliable_delay != UINT32_MAX)
  {
    printf("RESULT: Minimum reliable power-up delay = %lu ms\r\n", min_reliable_delay);
  }
  else
  {
    printf("RESULT: No reliable delay found in range\r\n");
    min_reliable_delay = 1000; // Use default if not found
  }
  return min_reliable_delay;
}

static void TestAnalogReadingFrequency(uint32_t min_powerup_delay)
{
  printf("\r\n=== Analog Input Sensor: Reading Frequency Test ===\r\n");
  printf("Using power-up delay: %lu ms\r\n", min_powerup_delay);
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         ANALOG_READ_DELAY_MIN_MS, ANALOG_READ_DELAY_MAX_MS, ANALOG_READ_DELAY_STEP_MS);

  uint32_t min_reliable_delay = UINT32_MAX;

  for (uint32_t delay = ANALOG_READ_DELAY_MIN_MS; delay <= ANALOG_READ_DELAY_MAX_MS; delay += ANALOG_READ_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(min_powerup_delay);

    if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
    {
      printf("ERROR: Failed to init analog input\r\n");
      PowerOff();
      continue;
    }

    float readings[ANALOG_READ_COUNT_FREQ];
    for (int i = 0; i < ANALOG_READ_COUNT_FREQ; i++)
    {
      uint32_t raw_mv = UINT32_MAX;
      if (FLEX_AnalogInputReadVoltage(&raw_mv) == 0)
      {
        readings[i] = raw_mv / 1000.0f;
      }
      else
      {
        readings[i] = 0;
      }
      if (delay > 0)
        FLEX_DelayMs(delay);
    }

    Statistics stats;
    CalculateStatistics(readings, ANALOG_READ_COUNT_FREQ, &stats);
    bool stable = IsAnalogStable(&stats);
    bool alive = IsAnalogAlive(&stats);

    FLEX_AnalogInputDeinit();
    PowerOff();

    printf("ANALOG_FREQ: ts=%lu, delay=%lu, mean=%.3fV, std_dev=%.3fV, alive=%s\r\n",
           GetTickMs(), delay, stats.mean, stats.std_dev, alive ? "yes" : "no");

    if (stable && delay < min_reliable_delay)
    {
      min_reliable_delay = delay;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  if (min_reliable_delay != UINT32_MAX)
  {
    printf("RESULT: Minimum reliable reading delay = %lu ms\r\n", min_reliable_delay);
  }
  else
  {
    printf("RESULT: No reliable delay found in range\r\n");
  }
}

#endif // ENABLE_ANALOG_TEST

// Modbus Temperature Sensor Tests
#if ENABLE_MODBUS_TEST

static uint32_t TestModbusPowerUpDelay(void)
{
  printf("\r\n=== Modbus Temperature Sensor: Power-Up Delay Test ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         MODBUS_POWERUP_DELAY_MIN_MS, MODBUS_POWERUP_DELAY_MAX_MS, MODBUS_POWERUP_DELAY_STEP_MS);

  uint32_t min_reliable_delay = UINT32_MAX;
  uint32_t success_count = 0;
  uint32_t total_tests = 0;

  for (uint32_t delay = MODBUS_POWERUP_DELAY_MIN_MS; delay <= MODBUS_POWERUP_DELAY_MAX_MS; delay += MODBUS_POWERUP_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(delay);

    uint32_t init_start = GetTickMs();
    int init_result = Modbus_Init();
    uint32_t init_duration = GetTickMs() - init_start;

    bool read_success = false;
    float temperature = 0;
    uint32_t read_duration = 0;

    if (init_result == 0)
    {
      uint32_t read_start = GetTickMs();
      for (int retry = 0; retry < MODBUS_READ_RETRIES; retry++)
      {
        if (Modbus_Request_Receive_Temperature(&temperature) == 0)
        {
          read_success = true;
          break;
        }
      }
      read_duration = GetTickMs() - read_start;

      uint32_t deinit_start = GetTickMs();
      int deinit_result = Modbus_Deinit();
      uint32_t deinit_duration = GetTickMs() - deinit_start;
      (void)deinit_result; // Track but don't fail on deinit
    }

    PowerOff();

    bool alive = (init_result == 0) && read_success;
    uint32_t total_duration = init_duration + read_duration;

    printf("MODBUS_POWERUP: ts=%lu, delay=%lu, init_ok=%d, read_ok=%d, temp=%.1f°C, duration=%lu, alive=%s\r\n",
           GetTickMs(), delay, (init_result == 0), read_success, temperature, total_duration, alive ? "yes" : "no");

    total_tests++;
    if (alive)
    {
      success_count++;
      if (delay < min_reliable_delay)
      {
        min_reliable_delay = delay;
      }
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  float success_rate = (total_tests > 0) ? (success_count * 100.0f / total_tests) : 0;
  if (min_reliable_delay == UINT32_MAX)
  {
    min_reliable_delay = 2000; // Use default if not found
  }
  printf("RESULT: Success rate = %.1f%%, Minimum reliable delay = %lu ms\r\n",
         success_rate, min_reliable_delay);
  return min_reliable_delay;
}

static void TestModbusReadingFrequency(uint32_t min_powerup_delay)
{
  printf("\r\n=== Modbus Temperature Sensor: Reading Frequency Test ===\r\n");
  printf("Using power-up delay: %lu ms\r\n", min_powerup_delay);
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         MODBUS_READ_DELAY_MIN_MS, MODBUS_READ_DELAY_MAX_MS, MODBUS_READ_DELAY_STEP_MS);

  uint32_t min_reliable_delay = UINT32_MAX;

  for (uint32_t delay = MODBUS_READ_DELAY_MIN_MS; delay <= MODBUS_READ_DELAY_MAX_MS; delay += MODBUS_READ_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(min_powerup_delay);

    if (Modbus_Init() != 0)
    {
      printf("ERROR: Failed to init Modbus\r\n");
      PowerOff();
      continue;
    }

    uint32_t success_count = 0;
    float temperatures[MODBUS_READ_ITERATIONS];
    uint32_t durations[MODBUS_READ_ITERATIONS];

    for (int i = 0; i < MODBUS_READ_ITERATIONS; i++)
    {
      uint32_t read_start = GetTickMs();
      float temp = 0;
      bool success = (Modbus_Request_Receive_Temperature(&temp) == 0);
      uint32_t read_duration = GetTickMs() - read_start;

      if (success)
      {
        temperatures[success_count] = temp;
        durations[success_count] = read_duration;
        success_count++;
      }

      if (delay > 0)
        FLEX_DelayMs(delay);
    }

    int deinit_result = Modbus_Deinit();
    PowerOff();

    bool alive = (deinit_result == 0) && (success_count > 0);
    float success_rate = (MODBUS_READ_ITERATIONS > 0) ? (success_count * 100.0f / MODBUS_READ_ITERATIONS) : 0;

    float avg_temp = 0;
    uint32_t avg_duration = 0;
    if (success_count > 0)
    {
      for (uint32_t i = 0; i < success_count; i++)
      {
        avg_temp += temperatures[i];
        avg_duration += durations[i];
      }
      avg_temp /= success_count;
      avg_duration /= success_count;
    }

    printf("MODBUS_FREQ: ts=%lu, delay=%lu, success=%u/%d, rate=%.1f%%, avg_temp=%.1f°C, avg_duration=%lu, alive=%s\r\n",
           GetTickMs(), delay, success_count, MODBUS_READ_ITERATIONS, success_rate, avg_temp, avg_duration, alive ? "yes" : "no");

    if (alive && (success_rate >= MODBUS_SUCCESS_RATE_THRESHOLD) && delay < min_reliable_delay)
    {
      min_reliable_delay = delay;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  if (min_reliable_delay != UINT32_MAX)
  {
    printf("RESULT: Minimum reliable reading delay = %lu ms\r\n", min_reliable_delay);
  }
  else
  {
    printf("RESULT: No reliable delay found in range\r\n");
  }
}

#endif // ENABLE_MODBUS_TEST

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);
  printf("Test configuration: PULSE=%d, ANALOG=%d, MODBUS=%d\r\n",
         ENABLE_PULSE_TEST, ENABLE_ANALOG_TEST, ENABLE_MODBUS_TEST);

  // Run tests sequentially
#if ENABLE_PULSE_TEST
  uint32_t pulse_powerup_delay = TestPulsePowerUpDelay();
  TestPulseReadingFrequency(pulse_powerup_delay);
#endif

#if ENABLE_ANALOG_TEST
  uint32_t analog_powerup_delay = TestAnalogPowerUpDelay();
  TestAnalogReadingFrequency(analog_powerup_delay);
#endif

#if ENABLE_MODBUS_TEST
  uint32_t modbus_powerup_delay = TestModbusPowerUpDelay();
  TestModbusReadingFrequency(modbus_powerup_delay);
#endif

  printf("\r\n=== Calibration Complete ===\r\n");
}
