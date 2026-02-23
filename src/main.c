#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"

#define APPLICATION_NAME "Analog Input - Minimal Settle Time & Sampling Interval"

// Fixed power-up delay (we know 0-100ms works)
#define ANALOG_POWERUP_DELAY_MS 100

// Test ranges for settle time and sampling interval
#define SETTLE_TIME_MIN_MS 0
#define SETTLE_TIME_MAX_MS 500
#define SETTLE_TIME_STEP_MS 10

#define SAMPLE_INTERVAL_MIN_MS 1
#define SAMPLE_INTERVAL_MAX_MS 200
#define SAMPLE_INTERVAL_STEP_MS 5

// Test parameters
#define ANALOG_READ_COUNT 10
#define INTER_CYCLE_DELAY_MS 3000

// Stability criteria
#define ANALOG_STD_DEV_THRESHOLD_MV 10
#define ANALOG_TARGET_VOLTAGE_V 1.0f
#define ANALOG_TARGET_TOLERANCE_V 0.05f

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE FLEX_ANALOG_IN_VOLTAGE

typedef struct
{
  float mean;
  float std_dev;
  float min;
  float max;
  uint32_t count;
} StabilityStats;

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

static void CalculateStability(float *readings, uint32_t count, StabilityStats *stats)
{
  if (count == 0)
  {
    stats->mean = 0;
    stats->std_dev = 0;
    stats->min = 0;
    stats->max = 0;
    stats->count = 0;
    return;
  }

  float sum = 0;
  stats->min = readings[0];
  stats->max = readings[0];
  for (uint32_t i = 0; i < count; i++)
  {
    sum += readings[i];
    if (readings[i] < stats->min)
      stats->min = readings[i];
    if (readings[i] > stats->max)
      stats->max = readings[i];
  }
  stats->mean = sum / count;
  stats->count = count;

  float variance = 0;
  for (uint32_t i = 0; i < count; i++)
  {
    float diff = readings[i] - stats->mean;
    variance += diff * diff;
  }
  stats->std_dev = sqrtf(variance / count);
}

static bool IsStable(StabilityStats *stats)
{
  bool low_noise = (stats->std_dev * 1000.0f) < ANALOG_STD_DEV_THRESHOLD_MV;
  bool on_target = fabsf(stats->mean - ANALOG_TARGET_VOLTAGE_V) < ANALOG_TARGET_TOLERANCE_V;
  return low_noise && on_target;
}

static bool TestSettleAndInterval(uint32_t settle_ms, uint32_t interval_ms, StabilityStats *stats_out)
{
  PowerOn();
  FLEX_DelayMs(ANALOG_POWERUP_DELAY_MS);

  if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
  {
    PowerOff();
    return false;
  }
  
  // Wait for settle time (instead of discarding samples)
  FLEX_DelayMs(settle_ms);

  // Take readings with specified interval
  float readings[ANALOG_READ_COUNT];
  uint32_t valid_count = 0;
  
  for (int i = 0; i < ANALOG_READ_COUNT; i++)
  {
    uint32_t raw_mv = UINT32_MAX;
    int result = FLEX_AnalogInputReadVoltage(&raw_mv);
    
    if (result == 0 && raw_mv != UINT32_MAX)
    {
      readings[valid_count] = raw_mv / 1000.0f;
      valid_count++;
    }
    
    if (i < ANALOG_READ_COUNT - 1 && interval_ms > 0)
    {
      FLEX_DelayMs(interval_ms);
    }
  }

  if (valid_count == 0)
  {
    FLEX_AnalogInputDeinit();
    PowerOff();
    return false;
  }

  CalculateStability(readings, valid_count, stats_out);
  
  FLEX_AnalogInputDeinit();
  PowerOff();
  
  return true;
}

static void TestMinimalSettleTime(void)
{
  printf("\r\n=== Test 1: Minimal Settle Time (after analog init) ===\r\n");
  printf("Power-up delay: %ums (fixed)\r\n", ANALOG_POWERUP_DELAY_MS);
  printf("Sample interval: 50ms (fixed)\r\n");
  printf("Sweep settle time: %u-%u ms, step: %u ms\r\n",
         SETTLE_TIME_MIN_MS, SETTLE_TIME_MAX_MS, SETTLE_TIME_STEP_MS);
  printf("Format: settle(ms) | mean(V) | std_dev(mV) | range(V) | stable\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_settle = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t settle = SETTLE_TIME_MIN_MS; settle <= SETTLE_TIME_MAX_MS; settle += SETTLE_TIME_STEP_MS)
  {
    StabilityStats stats;
    if (!TestSettleAndInterval(settle, 50, &stats))
    {
      printf("settle=%lu | ERROR: Test failed\r\n", settle);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    bool stable = IsStable(&stats);
    printf("settle=%lu | mean=%.3fV | std_dev=%.1fmV | range=%.3f-%.3fV | stable=%s\r\n",
           settle, stats.mean, stats.std_dev * 1000.0f, stats.min, stats.max,
           stable ? "YES" : "no");

    if (stable && settle < min_stable_settle)
    {
      min_stable_settle = settle;
      found_stable = true;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stable settle time: %lu ms\r\n", min_stable_settle);
  }
  else
  {
    printf("No stable settle time found in range\r\n");
  }
}

static void TestMinimalSampleInterval(void)
{
  // Use minimum stable settle time from Test 1 (110ms) + margin = 120ms
  const uint32_t SETTLE_TIME_FOR_INTERVAL_TEST_MS = 120;
  
  printf("\r\n=== Test 2: Minimal Sample Interval ===\r\n");
  printf("Power-up delay: %ums (fixed)\r\n", ANALOG_POWERUP_DELAY_MS);
  printf("Settle time: %ums (fixed, using minimum stable from Test 1)\r\n", SETTLE_TIME_FOR_INTERVAL_TEST_MS);
  printf("Sweep sample interval: %u-%u ms, step: %u ms\r\n",
         SAMPLE_INTERVAL_MIN_MS, SAMPLE_INTERVAL_MAX_MS, SAMPLE_INTERVAL_STEP_MS);
  printf("Format: interval(ms) | mean(V) | std_dev(mV) | range(V) | stable\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_interval = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t interval = SAMPLE_INTERVAL_MIN_MS; interval <= SAMPLE_INTERVAL_MAX_MS; interval += SAMPLE_INTERVAL_STEP_MS)
  {
    StabilityStats stats;
    if (!TestSettleAndInterval(SETTLE_TIME_FOR_INTERVAL_TEST_MS, interval, &stats))
    {
      printf("interval=%lu | ERROR: Test failed\r\n", interval);
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    bool stable = IsStable(&stats);
    printf("interval=%lu | mean=%.3fV | std_dev=%.1fmV | range=%.3f-%.3fV | stable=%s\r\n",
           interval, stats.mean, stats.std_dev * 1000.0f, stats.min, stats.max,
           stable ? "YES" : "no");

    if (stable && interval < min_stable_interval)
    {
      min_stable_interval = interval;
      found_stable = true;
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stable sample interval: %lu ms\r\n", min_stable_interval);
  }
  else
  {
    printf("No stable sample interval found in range\r\n");
  }
}

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);

  TestMinimalSettleTime();
  TestMinimalSampleInterval();

  printf("\r\n=== All Tests Complete ===\r\n");
}
