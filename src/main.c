#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"

#define APPLICATION_NAME "Sensor Timing Calibration - Analog Input Power-Up Test"

// Test range configuration
#define ANALOG_POWERUP_DELAY_MIN_MS 0
#define ANALOG_POWERUP_DELAY_MAX_MS 5000
#define ANALOG_POWERUP_DELAY_STEP_MS 100

// Test parameters
#define ANALOG_READ_COUNT_POWERUP 10

// Validation thresholds
#define ANALOG_STD_DEV_THRESHOLD_MV 10
#define ANALOG_ALIVE_VOLTAGE_V 1.0
#define ANALOG_ALIVE_TOLERANCE_V 0.2
#define INTER_CYCLE_DELAY_MS 5000

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE FLEX_ANALOG_IN_VOLTAGE

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
  float sorted[ANALOG_READ_COUNT_POWERUP];
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

static void TestAnalogPowerUpDelay(void)
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
  }
}

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);

  TestAnalogPowerUpDelay();

  printf("\r\n=== Calibration Complete ===\r\n");
}
