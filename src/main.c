#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"

#define APPLICATION_NAME "Pulse Interrupt Sensor - Timing Calibration"

// Test ranges for pulse sensor
#define PULSE_POWERUP_DELAY_MIN_MS 0
#define PULSE_POWERUP_DELAY_MAX_MS 1000
#define PULSE_POWERUP_DELAY_STEP_MS 10

#define PULSE_INIT_DELAY_MIN_MS 0
#define PULSE_INIT_DELAY_MAX_MS 200
#define PULSE_INIT_DELAY_STEP_MS 5

// Test parameters
#define PULSE_COUNT_WINDOW_MS 1000  // Count pulses for 1 second
#define PULSE_READ_ITERATIONS 10    // Number of consecutive init/deinit cycles per test
#define INTER_CYCLE_DELAY_MS 1000

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define PULSE_WAKEUP_COUNT 0

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

static bool TestPulsePowerUpDelay(uint32_t delay_ms, PulseReading *reading_out)
{
  PowerOn();
  FLEX_DelayMs(delay_ms);

  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
  {
    PowerOff();
    return false;
  }

  uint32_t start_tick = FLEX_TickGet();
  FLEX_DelayMs(PULSE_COUNT_WINDOW_MS);
  uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
  uint32_t end_tick = FLEX_TickGet();
  uint32_t duration_ms = end_tick - start_tick;

  FLEX_PulseCounterDeinit();
  PowerOff();

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
    if (!TestPulsePowerUpDelay(delay, &reading))
    {
      printf("delay=%lu | ERROR: Test failed\r\n", delay);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    printf("delay=%lu | count=%lu | rate=%.1fppm | alive=%s\r\n",
           delay, (unsigned long)reading.pulse_count, reading.rate_ppm,
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
    printf("Minimum stable power-up delay: %lu ms\r\n", min_stable_delay);
  }
  else
  {
    printf("No stable delay found in range\r\n");
  }
}

static bool TestPulseInitCycleDelay(uint32_t powerup_delay_ms, uint32_t init_delay_ms, PulseStats *stats_out)
{
  PowerOn();
  FLEX_DelayMs(powerup_delay_ms);

  PulseReading readings[PULSE_READ_ITERATIONS];
  float rates[PULSE_READ_ITERATIONS];

  for (int i = 0; i < PULSE_READ_ITERATIONS; i++)
  {
    if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
    {
      PowerOff();
      return false;
    }

    uint32_t start_tick = FLEX_TickGet();
    FLEX_DelayMs(PULSE_COUNT_WINDOW_MS);
    uint32_t pulse_count = (uint32_t)FLEX_PulseCounterGet();
    uint32_t end_tick = FLEX_TickGet();
    uint32_t duration_ms = end_tick - start_tick;

    FLEX_PulseCounterDeinit();

    float rate_ppm = (duration_ms > 0) ? (pulse_count * 60000.0f / duration_ms) : 0;
    rates[i] = rate_ppm;
    readings[i].pulse_count = pulse_count;
    readings[i].rate_ppm = rate_ppm;

    // Delay between init/deinit cycles (not between reads - counter is interrupt-driven)
    if (init_delay_ms > 0 && i < PULSE_READ_ITERATIONS - 1)
    {
      FLEX_DelayMs(init_delay_ms);
    }
  }

  PowerOff();

  // Check if sensor is alive for each reading
  for (int i = 0; i < PULSE_READ_ITERATIONS; i++)
  {
    readings[i].sensor_alive = IsPulseSensorAlive(readings[i].pulse_count, rates, PULSE_READ_ITERATIONS);
  }

  CalculatePulseStats(readings, PULSE_READ_ITERATIONS, stats_out);
  return true;
}

static void TestPulseInitCycleDelay(void)
{
  const uint32_t POWERUP_DELAY_MS = 100;  // Use minimum from power-up test
  
  printf("\r\n=== Test 2: Pulse Sensor Init/Deinit Cycle Delay ===\r\n");
  printf("Power-up delay: %ums (fixed)\r\n", POWERUP_DELAY_MS);
  printf("Sweep delay between init/deinit cycles: %u-%u ms, step: %u ms\r\n",
         PULSE_INIT_DELAY_MIN_MS, PULSE_INIT_DELAY_MAX_MS, PULSE_INIT_DELAY_STEP_MS);
  printf("Count window: %ums per cycle, %d cycles\r\n", PULSE_COUNT_WINDOW_MS, PULSE_READ_ITERATIONS);
  printf("Note: Counter is interrupt-driven, no delay needed between reads\r\n");
  printf("Format: cycle_delay(ms) | avg_rate(ppm) | std_dev(ppm) | count_range | stable\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_delay = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t delay = PULSE_INIT_DELAY_MIN_MS; delay <= PULSE_INIT_DELAY_MAX_MS; delay += PULSE_INIT_DELAY_STEP_MS)
  {
    PulseStats stats;
    if (!TestPulseInitCycleDelay(POWERUP_DELAY_MS, delay, &stats))
    {
      printf("delay=%lu | ERROR: Test failed\r\n", delay);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    // Consider stable if std dev is reasonable and we have valid readings
    bool stable = (stats.valid_readings > 0) && (stats.std_dev_rate < 100.0f); // 100 ppm tolerance

    printf("delay=%lu | avg_rate=%.1fppm | std_dev=%.1fppm | count=%lu-%lu | stable=%s\r\n",
           delay, stats.mean_rate, stats.std_dev_rate,
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
    printf("Minimum stable cycle delay: %lu ms\r\n", min_stable_delay);
  }
  else
  {
    printf("No stable cycle delay found in range\r\n");
  }
}

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);

  TestPulsePowerUpDelay();
  TestPulseInitCycleDelay();

  printf("\r\n=== All Tests Complete ===\r\n");
}
