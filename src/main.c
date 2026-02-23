#include <stdio.h>
#include <stdint.h>
#include "flex.h"

#define APPLICATION_NAME "Analog Input - Raw Readings Debug"

// Test range configuration
#define ANALOG_POWERUP_DELAY_MIN_MS 0
#define ANALOG_POWERUP_DELAY_MAX_MS 5000
#define ANALOG_POWERUP_DELAY_STEP_MS 100

// Test parameters
#define ANALOG_READ_COUNT 5
#define ANALOG_SETTLE_AFTER_INIT_MS 500
#define ANALOG_SAMPLE_INTERVAL_MS 200
#define INTER_CYCLE_DELAY_MS 5000

// Sensor configuration
#define SENSOR_POWER_SUPPLY FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE FLEX_ANALOG_IN_VOLTAGE

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

static void TestAnalogPowerUpDelay(void)
{
  printf("\r\n=== Analog Input: Raw Readings Test ===\r\n");
  printf("Sweep: %u-%u ms, step: %u ms\r\n",
         ANALOG_POWERUP_DELAY_MIN_MS, ANALOG_POWERUP_DELAY_MAX_MS, ANALOG_POWERUP_DELAY_STEP_MS);
  printf("Format: delay(ms) | raw_mv | voltage(V)\r\n");
  printf("----------------------------------------\r\n");

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

    // Take readings and print each one
    for (int i = 0; i < ANALOG_READ_COUNT; i++)
    {
      uint32_t raw_mv = UINT32_MAX;
      int result = FLEX_AnalogInputReadVoltage(&raw_mv);
      
      if (result == 0)
      {
        float voltage = raw_mv / 1000.0f;
        printf("delay=%lu | raw=%lumV | %.3fV\r\n", delay, raw_mv, voltage);
      }
      else
      {
        printf("delay=%lu | ERROR: read failed (result=%d)\r\n", delay, result);
      }
      
      if (i < ANALOG_READ_COUNT - 1)
      {
        FLEX_DelayMs(ANALOG_SAMPLE_INTERVAL_MS);
      }
    }

    FLEX_AnalogInputDeinit();
    PowerOff();

    FLEX_DelayMs(INTER_CYCLE_DELAY_MS);
  }

  printf("\r\n=== Test Complete ===\r\n");
}

void FLEX_AppInit()
{
  printf("\r\n%s\r\n", APPLICATION_NAME);
  printf("Compiled on %s at %s\r\n", __DATE__, __TIME__);

  TestAnalogPowerUpDelay();
}
