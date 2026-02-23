#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"

// Sensor selection - enable/disable tests at compile time
#ifndef ENABLE_PULSE_TEST
#define ENABLE_PULSE_TEST 1
#endif

#ifndef ENABLE_ANALOG_TEST
#define ENABLE_ANALOG_TEST 1
#endif

#define APPLICATION_NAME "Sensor Timing Calibration"

// Test ranges for pulse sensor
#define PULSE_POWERUP_DELAY_MIN_MS 0
#define PULSE_POWERUP_DELAY_MAX_MS 200
#define PULSE_POWERUP_DELAY_STEP_MS 10

#define PULSE_READ_DELAY_MIN_MS 0
#define PULSE_READ_DELAY_MAX_MS 50
#define PULSE_READ_DELAY_STEP_MS 5

// Test ranges for analog sensor
#define ANALOG_POWERUP_DELAY_MIN_MS 0
#define ANALOG_POWERUP_DELAY_MAX_MS 200
#define ANALOG_POWERUP_DELAY_STEP_MS 10

#define ANALOG_READ_DELAY_MIN_MS 0
#define ANALOG_READ_DELAY_MAX_MS 20
#define ANALOG_READ_DELAY_STEP_MS 1

// Test parameters
#define PULSE_COUNT_WINDOW_MS 1000  // Count pulses for 1 second
#define PULSE_READ_ITERATIONS 10    // Number of readings per test
#define ANALOG_READ_ITERATIONS 10   // Number of readings for power-up test
#define ANALOG_FREQ_READ_ITERATIONS 100  // Number of readings for frequency test
#define INTER_CYCLE_DELAY_MS 1000

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define PULSE_WAKEUP_COUNT 0
#define ANALOG_EXPECTED_VOLTAGE_V 1.0f
#define ANALOG_TOLERANCE_V 0.2f
#define ANALOG_STD_DEV_THRESHOLD_MV 10.0f

// ============================================================================
// Pulse Sensor Types and Functions
// ============================================================================

#if ENABLE_PULSE_TEST

typedef struct
{
  uint32_t pulse_count;
  float rate_ppm;  // pulses per minute
  bool sensor_alive;
} PulseReading;

typedef struct
{
  float mean_rate;
  float std_dev_rate;
  uint32_t min_count;
  uint32_t max_count;
  uint32_t valid_readings;
} PulseStats;

static bool IsPulseSensorAlive(uint32_t pulse_count, float *rates, uint32_t rate_count)
{
  // Sensor alive if: pulse count > 1 AND rate is relatively stable
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

  return max_deviation < 0.2f; // 20% tolerance
}

static void CalculatePulseStats(PulseReading *readings, uint32_t count, PulseStats *stats)
{
  if (count == 0)
  {
    stats->mean_rate = 0;
    stats->std_dev_rate = 0;
    stats->min_count = 0;
    stats->max_count = 0;
    stats->valid_readings = 0;
    return;
  }

  float sum_rate = 0;
  stats->min_count = readings[0].pulse_count;
  stats->max_count = readings[0].pulse_count;
  uint32_t valid = 0;

  for (uint32_t i = 0; i < count; i++)
  {
    if (readings[i].sensor_alive)
    {
      sum_rate += readings[i].rate_ppm;
      if (readings[i].pulse_count < stats->min_count)
        stats->min_count = readings[i].pulse_count;
      if (readings[i].pulse_count > stats->max_count)
        stats->max_count = readings[i].pulse_count;
      valid++;
    }
  }

  if (valid == 0)
  {
    stats->mean_rate = 0;
    stats->std_dev_rate = 0;
    stats->valid_readings = 0;
    return;
  }

  stats->mean_rate = sum_rate / valid;
  stats->valid_readings = valid;

  // Calculate standard deviation
  float variance = 0;
  for (uint32_t i = 0; i < count; i++)
  {
    if (readings[i].sensor_alive)
    {
      float diff = readings[i].rate_ppm - stats->mean_rate;
      variance += diff * diff;
    }
  }
  stats->std_dev_rate = sqrtf(variance / valid);
}

static bool TestPulsePowerUpDelaySingle(uint32_t delay_ms, PulseReading *reading_out)
{
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    return false;
  }
  FLEX_DelayMs(delay_ms);

  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
  {
    FLEX_PowerOutDeinit();
    return false;
  }

  uint32_t start_tick = FLEX_TickGet();
  FLEX_DelayMs(PULSE_COUNT_WINDOW_MS);
  uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
  uint32_t end_tick = FLEX_TickGet();
  uint32_t duration_ms = end_tick - start_tick;

  FLEX_PulseCounterDeinit();
  FLEX_PowerOutDeinit();

  float rate_ppm = (duration_ms > 0) ? (pulse_count * 60000.0f / duration_ms) : 0;
  float rates[1] = {rate_ppm};
  bool alive = IsPulseSensorAlive(pulse_count, rates, 1);

  reading_out->pulse_count = pulse_count;
  reading_out->rate_ppm = rate_ppm;
  reading_out->sensor_alive = alive;

  return true;
}

static void TestPulsePowerUpDelay(void)
{
  printf("\r\n=== Test 1: Pulse Sensor Power-Up Delay ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         PULSE_POWERUP_DELAY_MIN_MS, PULSE_POWERUP_DELAY_MAX_MS, PULSE_POWERUP_DELAY_STEP_MS);
  printf("Count window: %ums\r\n", PULSE_COUNT_WINDOW_MS);
  printf("Format: delay(ms) | count | rate(ppm) | alive\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_delay = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t delay = PULSE_POWERUP_DELAY_MIN_MS; delay <= PULSE_POWERUP_DELAY_MAX_MS; delay += PULSE_POWERUP_DELAY_STEP_MS)
  {
    PulseReading reading;
    if (!TestPulsePowerUpDelaySingle(delay, &reading))
    {
      printf("delay=%lu | ERROR: Test failed\r\n", (unsigned long)delay);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    printf("delay=%lu | count=%lu | rate=%.1fppm | alive=%s\r\n",
           (unsigned long)delay, (unsigned long)reading.pulse_count, reading.rate_ppm,
           reading.sensor_alive ? "YES" : "no");

    if (reading.sensor_alive && delay < min_stable_delay)
    {
      min_stable_delay = delay;
      found_stable = true;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stable power-up delay: %lu ms\r\n", (unsigned long)min_stable_delay);
  }
  else
  {
    printf("No stable delay found in range\r\n");
  }
}

static bool TestPulseReadingFrequencySingle(uint32_t powerup_delay_ms, uint32_t read_delay_ms, PulseStats *stats_out)
{
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    return false;
  }
  FLEX_DelayMs(powerup_delay_ms);

  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
  {
    FLEX_PowerOutDeinit();
    return false;
  }

  PulseReading readings[PULSE_READ_ITERATIONS];
  float rates[PULSE_READ_ITERATIONS];
  uint32_t prev_count = 0;
  uint32_t prev_tick = FLEX_TickGet();

  // First reading after init
  FLEX_DelayMs(PULSE_COUNT_WINDOW_MS);
  uint32_t first_count = (uint32_t)FLEX_PulseCounterGet();
  uint32_t first_tick = FLEX_TickGet();
  uint32_t first_duration_ms = first_tick - prev_tick;

  float first_rate_ppm = (first_duration_ms > 0) ? (first_count * 60000.0f / first_duration_ms) : 0;
  rates[0] = first_rate_ppm;
  readings[0].pulse_count = first_count;
  readings[0].rate_ppm = first_rate_ppm;
  prev_count = first_count;
  prev_tick = first_tick;

  // Subsequent readings without restarting counter
  for (int i = 1; i < PULSE_READ_ITERATIONS; i++)
  {
    if (read_delay_ms > 0)
    {
      FLEX_DelayMs(read_delay_ms);
    }

    uint32_t current_tick = FLEX_TickGet();
    uint32_t current_count = (uint32_t)FLEX_PulseCounterGet();
    uint32_t duration_ms = current_tick - prev_tick;
    uint32_t count_diff = current_count - prev_count;

    float rate_ppm = (duration_ms > 0) ? (count_diff * 60000.0f / duration_ms) : 0;
    rates[i] = rate_ppm;
    readings[i].pulse_count = count_diff;
    readings[i].rate_ppm = rate_ppm;

    prev_count = current_count;
    prev_tick = current_tick;
  }

  FLEX_PulseCounterDeinit();
  FLEX_PowerOutDeinit();

  // Check if sensor is alive for each reading
  for (int i = 0; i < PULSE_READ_ITERATIONS; i++)
  {
    readings[i].sensor_alive = IsPulseSensorAlive(readings[i].pulse_count, rates, PULSE_READ_ITERATIONS);
  }

  CalculatePulseStats(readings, PULSE_READ_ITERATIONS, stats_out);
  return true;
}

static void TestPulseReadingFrequency(void)
{
  const uint32_t POWERUP_DELAY_MS = 100;  // Use minimum from power-up test
  
  printf("\r\n=== Test 2: Pulse Sensor Reading Frequency ===\r\n");
  printf("Power-up delay: %lums (fixed)\r\n", (unsigned long)POWERUP_DELAY_MS);
  printf("Counter initialized once, then read %d times with delay between reads\r\n", PULSE_READ_ITERATIONS);
  printf("Sweep read delay: %u-%u ms, step: %u ms\r\n",
         PULSE_READ_DELAY_MIN_MS, PULSE_READ_DELAY_MAX_MS, PULSE_READ_DELAY_STEP_MS);
  printf("First reading after %ums, subsequent readings with specified delay\r\n", PULSE_COUNT_WINDOW_MS);
  printf("Format: read_delay(ms) | avg_rate(ppm) | std_dev(ppm) | count_range | stable\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_delay = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t delay = PULSE_READ_DELAY_MIN_MS; delay <= PULSE_READ_DELAY_MAX_MS; delay += PULSE_READ_DELAY_STEP_MS)
  {
    PulseStats stats;
    if (!TestPulseReadingFrequencySingle(POWERUP_DELAY_MS, delay, &stats))
    {
      printf("delay=%lu | ERROR: Test failed\r\n", (unsigned long)delay);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    // Consider stable if std dev is reasonable and we have valid readings
    bool stable = (stats.valid_readings > 0) && (stats.std_dev_rate < 100.0f); // 100 ppm tolerance

    printf("delay=%lu | avg_rate=%.1fppm | std_dev=%.1fppm | count=%lu-%lu | stable=%s\r\n",
           (unsigned long)delay, stats.mean_rate, stats.std_dev_rate,
           (unsigned long)stats.min_count, (unsigned long)stats.max_count,
           stable ? "YES" : "no");

    if (stable && delay < min_stable_delay)
    {
      min_stable_delay = delay;
      found_stable = true;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stable read delay: %lu ms\r\n", (unsigned long)min_stable_delay);
  }
  else
  {
    printf("No stable read delay found in range\r\n");
  }
}

#endif // ENABLE_PULSE_TEST

// ============================================================================
// Analog Sensor Types and Functions
// ============================================================================

#if ENABLE_ANALOG_TEST

typedef struct
{
  float voltage_v;
  bool sensor_alive;
} AnalogReading;

typedef struct
{
  float mean_v;
  float std_dev_mv;
  float min_v;
  float max_v;
  uint32_t valid_readings;
} AnalogStats;

static bool IsAnalogSensorAlive(float voltage_v)
{
  // Sensor alive if: reading ≈ 1.0V ± tolerance
  float diff = fabsf(voltage_v - ANALOG_EXPECTED_VOLTAGE_V);
  return diff <= ANALOG_TOLERANCE_V;
}

static bool IsStable(float std_dev_mv, float min_v, float max_v)
{
  // Stable if: std dev < threshold AND readings in reasonable range
  return (std_dev_mv < ANALOG_STD_DEV_THRESHOLD_MV) && (min_v >= 0.0f) && (max_v <= 5.0f);
}

static void CalculateAnalogStats(float *voltages, uint32_t count, AnalogStats *stats)
{
  if (count == 0)
  {
    stats->mean_v = 0;
    stats->std_dev_mv = 0;
    stats->min_v = 0;
    stats->max_v = 0;
    stats->valid_readings = 0;
    return;
  }

  float sum = 0;
  stats->min_v = voltages[0];
  stats->max_v = voltages[0];
  uint32_t valid = 0;

  for (uint32_t i = 0; i < count; i++)
  {
    if (voltages[i] >= 0.0f && voltages[i] <= 5.0f)
    {
      sum += voltages[i];
      if (voltages[i] < stats->min_v)
        stats->min_v = voltages[i];
      if (voltages[i] > stats->max_v)
        stats->max_v = voltages[i];
      valid++;
    }
  }

  if (valid == 0)
  {
    stats->mean_v = 0;
    stats->std_dev_mv = 0;
    stats->valid_readings = 0;
    return;
  }

  stats->mean_v = sum / valid;
  stats->valid_readings = valid;

  // Calculate standard deviation
  float variance = 0;
  for (uint32_t i = 0; i < count; i++)
  {
    if (voltages[i] >= 0.0f && voltages[i] <= 5.0f)
    {
      float diff = voltages[i] - stats->mean_v;
      variance += diff * diff;
    }
  }
  stats->std_dev_mv = sqrtf(variance / valid) * 1000.0f; // Convert to mV
}

static bool TestAnalogPowerUpDelaySingle(uint32_t delay_ms, AnalogStats *stats_out)
{
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    return false;
  }
  FLEX_DelayMs(delay_ms);

  if (FLEX_AnalogInputInit(FLEX_ANALOG_INPUT_DEFAULT) != 0)
  {
    FLEX_PowerOutDeinit();
    return false;
  }

  float voltages[ANALOG_READ_ITERATIONS];
  for (uint32_t i = 0; i < ANALOG_READ_ITERATIONS; i++)
  {
    uint32_t voltage_mv = 0;
    if (FLEX_AnalogInputReadVoltage(&voltage_mv) == 0)
    {
      voltages[i] = voltage_mv / 1000.0f; // Convert mV to V
    }
    else
    {
      voltages[i] = -1.0f; // Invalid reading
    }
    // Minimal delay between readings
    if (i < ANALOG_READ_ITERATIONS - 1)
    {
      FLEX_DelayMs(1);
    }
  }

  FLEX_AnalogInputDeinit();
  FLEX_PowerOutDeinit();

  CalculateAnalogStats(voltages, ANALOG_READ_ITERATIONS, stats_out);
  return true;
}

static void TestAnalogPowerUpDelay(void)
{
  printf("\r\n=== Test 1: Analog Sensor Power-Up Delay ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         ANALOG_POWERUP_DELAY_MIN_MS, ANALOG_POWERUP_DELAY_MAX_MS, ANALOG_POWERUP_DELAY_STEP_MS);
  printf("Taking %d readings per test (1ms between readings)\r\n", ANALOG_READ_ITERATIONS);
  printf("Format: delay(ms) | mean(V) | std_dev(mV) | range(V) | stable | alive\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_delay = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t delay = ANALOG_POWERUP_DELAY_MIN_MS; delay <= ANALOG_POWERUP_DELAY_MAX_MS; delay += ANALOG_POWERUP_DELAY_STEP_MS)
  {
    AnalogStats stats;
    if (!TestAnalogPowerUpDelaySingle(delay, &stats))
    {
      printf("delay=%lu | ERROR: Test failed\r\n", (unsigned long)delay);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    bool stable = IsStable(stats.std_dev_mv, stats.min_v, stats.max_v);
    bool alive = (stats.valid_readings > 0) && IsAnalogSensorAlive(stats.mean_v);

    printf("delay=%lu | mean=%.3fV | std_dev=%.1fmV | range=%.3f-%.3fV | stable=%s | alive=%s\r\n",
           (unsigned long)delay, stats.mean_v, stats.std_dev_mv,
           stats.min_v, stats.max_v,
           stable ? "YES" : "no", alive ? "YES" : "no");

    if (stable && delay < min_stable_delay)
    {
      min_stable_delay = delay;
      found_stable = true;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stable power-up delay: %lu ms\r\n", (unsigned long)min_stable_delay);
  }
  else
  {
    printf("No stable delay found in range\r\n");
  }
}

static bool TestAnalogReadingFrequencySingle(uint32_t powerup_delay_ms, uint32_t read_delay_ms, AnalogStats *stats_out)
{
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    return false;
  }
  FLEX_DelayMs(powerup_delay_ms);

  if (FLEX_AnalogInputInit(FLEX_ANALOG_INPUT_DEFAULT) != 0)
  {
    FLEX_PowerOutDeinit();
    return false;
  }

  float voltages[ANALOG_FREQ_READ_ITERATIONS];
  uint32_t start_tick = FLEX_TickGet();

  for (uint32_t i = 0; i < ANALOG_FREQ_READ_ITERATIONS; i++)
  {
    uint32_t voltage_mv = 0;
    if (FLEX_AnalogInputReadVoltage(&voltage_mv) == 0)
    {
      voltages[i] = voltage_mv / 1000.0f; // Convert mV to V
    }
    else
    {
      voltages[i] = -1.0f; // Invalid reading
    }

    if (read_delay_ms > 0 && i < ANALOG_FREQ_READ_ITERATIONS - 1)
    {
      FLEX_DelayMs(read_delay_ms);
    }
  }

  uint32_t end_tick = FLEX_TickGet();
  uint32_t total_duration_ms = end_tick - start_tick;

  FLEX_AnalogInputDeinit();
  FLEX_PowerOutDeinit();

  CalculateAnalogStats(voltages, ANALOG_FREQ_READ_ITERATIONS, stats_out);
  return true;
}

static void TestAnalogReadingFrequency(void)
{
  const uint32_t POWERUP_DELAY_MS = 100;  // Use minimum from power-up test
  
  printf("\r\n=== Test 2: Analog Sensor Reading Frequency ===\r\n");
  printf("Power-up delay: %lums (fixed)\r\n", (unsigned long)POWERUP_DELAY_MS);
  printf("Taking %d consecutive readings with delay between each\r\n", ANALOG_FREQ_READ_ITERATIONS);
  printf("Sweep read delay: %u-%u ms, step: %u ms\r\n",
         ANALOG_READ_DELAY_MIN_MS, ANALOG_READ_DELAY_MAX_MS, ANALOG_READ_DELAY_STEP_MS);
  printf("Format: read_delay(ms) | mean(V) | std_dev(mV) | range(V) | stable | alive\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_delay = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t delay = ANALOG_READ_DELAY_MIN_MS; delay <= ANALOG_READ_DELAY_MAX_MS; delay += ANALOG_READ_DELAY_STEP_MS)
  {
    AnalogStats stats;
    if (!TestAnalogReadingFrequencySingle(POWERUP_DELAY_MS, delay, &stats))
    {
      printf("delay=%lu | ERROR: Test failed\r\n", (unsigned long)delay);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    bool stable = IsStable(stats.std_dev_mv, stats.min_v, stats.max_v);
    bool alive = (stats.valid_readings > 0) && IsAnalogSensorAlive(stats.mean_v);

    printf("delay=%lu | mean=%.3fV | std_dev=%.1fmV | range=%.3f-%.3fV | stable=%s | alive=%s\r\n",
           (unsigned long)delay, stats.mean_v, stats.std_dev_mv,
           stats.min_v, stats.max_v,
           stable ? "YES" : "no", alive ? "YES" : "no");

    if (stable && delay < min_stable_delay)
    {
      min_stable_delay = delay;
      found_stable = true;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stable read delay: %lu ms\r\n", (unsigned long)min_stable_delay);
  }
  else
  {
    printf("No stable read delay found in range\r\n");
  }
}

#endif // ENABLE_ANALOG_TEST

// ============================================================================
// Main Application
// ============================================================================

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);

#if ENABLE_PULSE_TEST
  TestPulsePowerUpDelay();
  TestPulseReadingFrequency();
#endif

#if ENABLE_ANALOG_TEST
  TestAnalogPowerUpDelay();
  TestAnalogReadingFrequency();
#endif

  printf("\r\n=== All Tests Complete ===\r\n");
}
