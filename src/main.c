#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"

#define APPLICATION_NAME "Analog Input - Minimal Stabilizing Time"

// Test range configuration
#define ANALOG_POWERUP_DELAY_MIN_MS 0
#define ANALOG_POWERUP_DELAY_MAX_MS 5000
#define ANALOG_POWERUP_DELAY_STEP_MS 50  // Smaller step for finer resolution

// Test parameters
#define ANALOG_READ_COUNT 10              // More readings for better statistics
#define ANALOG_SETTLE_AFTER_INIT_MS 100   // Reduced - we're testing power-up delay, not init delay
#define ANALOG_SAMPLE_INTERVAL_MS 50      // Shorter interval but still spaced
#define INTER_CYCLE_DELAY_MS 3000         // Reduced cycle delay for faster testing

// Stability criteria
#define ANALOG_STD_DEV_THRESHOLD_MV 10    // Readings stable if std dev < 10mV
#define ANALOG_TARGET_VOLTAGE_V 1.0f
#define ANALOG_TARGET_TOLERANCE_V 0.05f    // Within 50mV of target (0.95V - 1.05V)

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

  // Calculate mean and min/max
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

  // Calculate standard deviation
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
  // Stable if: std dev < threshold AND mean is close to target
  bool low_noise = (stats->std_dev * 1000.0f) < ANALOG_STD_DEV_THRESHOLD_MV;
  bool on_target = fabsf(stats->mean - ANALOG_TARGET_VOLTAGE_V) < ANALOG_TARGET_TOLERANCE_V;
  return low_noise && on_target;
}

static void TestAnalogPowerUpDelay(void)
{
  printf("\r\n=== Analog Input: Minimal Stabilizing Time Test ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         ANALOG_POWERUP_DELAY_MIN_MS, ANALOG_POWERUP_DELAY_MAX_MS, ANALOG_POWERUP_DELAY_STEP_MS);
  printf("Target: %.2fV ± %.2fV, Stable if std_dev < %umV\r\n",
         ANALOG_TARGET_VOLTAGE_V, ANALOG_TARGET_TOLERANCE_V, ANALOG_STD_DEV_THRESHOLD_MV);
  printf("Format: delay | mean(V) | std_dev(mV) | min-max(V) | stable\r\n");
  printf("------------------------------------------------------------\r\n");

  uint32_t min_stable_delay = UINT32_MAX;
  bool found_stable = false;

  for (uint32_t delay = ANALOG_POWERUP_DELAY_MIN_MS; delay <= ANALOG_POWERUP_DELAY_MAX_MS; delay += ANALOG_POWERUP_DELAY_STEP_MS)
  {
    PowerOn();
    FLEX_DelayMs(delay);

    if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
    {
      printf("ERROR: Failed to init analog input at delay=%lu\r\n", delay);
      PowerOff();
      continue;
    }
    
    FLEX_DelayMs(ANALOG_SETTLE_AFTER_INIT_MS);

    // Take readings
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
      
      if (i < ANALOG_READ_COUNT - 1)
      {
        FLEX_DelayMs(ANALOG_SAMPLE_INTERVAL_MS);
      }
    }

    if (valid_count == 0)
    {
      printf("delay=%lu | ERROR: No valid readings\r\n", delay);
      FLEX_AnalogInputDeinit();
      PowerOff();
      FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
      continue;
    }

    // Calculate stability statistics
    StabilityStats stats;
    CalculateStability(readings, valid_count, &stats);
    bool stable = IsStable(&stats);

    // Print results
    printf("delay=%lu | mean=%.3fV | std_dev=%.1fmV | range=%.3f-%.3fV | stable=%s\r\n",
           delay, stats.mean, stats.std_dev * 1000.0f, stats.min, stats.max,
           stable ? "YES" : "no");

    // Track minimum stable delay
    if (stable && delay < min_stable_delay)
    {
      min_stable_delay = delay;
      found_stable = true;
    }

    FLEX_AnalogInputDeinit();
    PowerOff();

    // Early exit if we found stable reading and want to test a bit more
    if (found_stable && delay > min_stable_delay + 500)
    {
      printf("Stable reading found, continuing to verify...\r\n");
    }

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Results ===\r\n");
  if (found_stable)
  {
    printf("Minimum stabilizing delay: %lu ms\r\n", min_stable_delay);
  }
  else
  {
    printf("No stable readings found in test range\r\n");
  }
  printf("=== Test Complete ===\r\n");
}

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);

  TestAnalogPowerUpDelay();
}
